#include "auth.c"
#include "common.h"
#include "store.c"
#include <arpa/inet.h>
#include "order.c"
#include <errno.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>
#include <semaphore.h>
#include <signal.h>

char *shared_output = NULL;
sem_t *sem = NULL;
Store *g_store = NULL;
Order *g_order = NULL;
UserSessions *g_sessions = NULL;
SOCKET g_socket_listen = -1;
typedef void (*CommandHandler)(CommandContext *ctx);
typedef struct {
  char *commandName;
  CommandHandler handler;
} CommandEntry;

// Anonymous Handlers
void handleLogin(CommandContext *ctx);
void handleLogout(CommandContext *ctx);
void handleViewProduct(CommandContext *ctx);
void handleSearchProduct(CommandContext *ctx);
void handleRegisterMember(CommandContext *ctx);

// Member Handlers
void handleUpdateCart(CommandContext *ctx);
void handleDeleteFromCart(CommandContext *ctx);
void handleClearCart(CommandContext *ctx);
void handleCheckoutCart(CommandContext *ctx);
void handleViewCart(CommandContext *ctx);
void handleViewOrder(CommandContext *ctx);

// Admin Handlers
void handleAddProduct(CommandContext *ctx);
void handleUpdateProduct(CommandContext *ctx);
void handleRemoveProduct(CommandContext *ctx);
void handleViewMember(CommandContext *ctx);

// Common
int handleClient(SOCKET socket_client, char *read);
int dispatchCommand(CommandContext *ctx, char *commandName);
void replace_char(char *str, char old_char, char new_char);
void handle_shutdown(int sig);
int setup(char *port);

// string->*func
CommandEntry commandTable[] = {
    {COMMAND_LOGIN, handleLogin},
    {COMMAND_LOGOUT, handleLogout},
    {COMMAND_VIEW_PRODUCT, handleViewProduct},
    {COMMAND_SEARCH_PRODUCT, handleSearchProduct},
    {COMMAND_REGISTER_MEMBER, handleRegisterMember},
    {COMMAND_UPDATE_CART, handleUpdateCart},
    {COMMAND_DELETE_FROM_CART, handleDeleteFromCart},
    {COMMAND_CLEAR_CART, handleClearCart},
    {COMMAND_CHECKOUT_CART, handleCheckoutCart},
    {COMMAND_VIEW_CART, handleViewCart},
    {COMMAND_VIEW_ORDER, handleViewOrder},
    {COMMAND_ADD_PRODUCT, handleAddProduct},
    {COMMAND_UPDATE_PRODUCT, handleUpdateProduct},
    {COMMAND_REMOVE_PRODUCT, handleRemoveProduct},
    {COMMAND_VIEW_MEMBER, handleViewMember},
    {NULL, NULL} // Sentinel marks the end of table
};

int main() {
  // SET SHARED MEMORY
  g_store = mmap(NULL, sizeof(Store), PROT_READ | PROT_WRITE,
                 MAP_SHARED | MAP_ANONYMOUS, -1, 0);
  loadStore(g_store, STORE_FILENAME);

  g_sessions = mmap(NULL, sizeof(UserSessions), PROT_READ | PROT_WRITE,
                    MAP_SHARED | MAP_ANONYMOUS, -1, 0);
  loadUser(g_sessions, USER_FILENAME);

  g_order = mmap(NULL, sizeof(Order), PROT_READ | PROT_WRITE,
                 MAP_SHARED | MAP_ANONYMOUS, -1, 0);
  loadOrder(g_order, ORDER_FILENAME);

  shared_output = mmap(NULL, 8192, PROT_READ | PROT_WRITE,
                       MAP_SHARED | MAP_ANONYMOUS, -1, 0);

  // sem
  sem = mmap(NULL, sizeof(sem_t), PROT_READ | PROT_WRITE,
             MAP_SHARED | MAP_ANONYMOUS, -1, 0);
  sem_init(sem, 1, 1);

  signal(SIGINT, handle_shutdown);
  signal(SIGTERM, handle_shutdown);
  signal(SIGCHLD, SIG_IGN);

  g_socket_listen = setup("8082");
  if (g_socket_listen < 0) return 1;

  printf("Waiting for connections...\n");
  while (1) {
    struct sockaddr_storage client_address;
    socklen_t client_len = sizeof(client_address);
    SOCKET socket_client =
        accept(g_socket_listen, (struct sockaddr *)&client_address, &client_len);
    if (!ISVALIDSOCKET(socket_client)) {
      fprintf(stderr, "accept() failed. (%d)\n", GETSOCKETERRNO());
      return 1;
    }

    char address_buffer[100];
    getnameinfo((struct sockaddr *)&client_address, client_len, address_buffer,
                sizeof(address_buffer), 0, 0, NI_NUMERICHOST);
    printf("New connection from %s\n", address_buffer);
    printf("Forking..\n");

    int pid = fork();
    printf("pid: %i\n", pid);
    // child
    if (pid == 0) {
      CLOSESOCKET(g_socket_listen);
      while (1) {
        char read[1024];
        memset(read, 0, sizeof(read));
        int bytes_received = recv(socket_client, read, sizeof(read), 0);
        printf("read: %s\n", read);

        if (bytes_received < 1) {
          printf("Client disconnected.\n");
          CLOSESOCKET(socket_client);
          exit(0);
        }

        CommandContext ctx;
        ctx.socket_client = socket_client;
        ctx.store = g_store;
        ctx.order = g_order;
        ctx.sessions = g_sessions;
        ctx.rawInput = read;

        if (dispatchCommand(&ctx, read) == -1) {
          printf("Client logged out.\n");
          CLOSESOCKET(socket_client);
          exit(0);
        }
      }
    }

    CLOSESOCKET(socket_client);
  }

  CLOSESOCKET(g_socket_listen);
  return 0;
}


void replace_char(char *str, char old_char, char new_char) {
  for (int i = 0; str[i] != '\0'; i++) {
    if (str[i] == old_char) {
      str[i] = new_char;
    }
  }
}

int handleClient(SOCKET socket_client, char *read) {
  read[strcspn(read, "\r\n")] = '\0';
  int choice = atoi(read);
  return choice;
}

// ==========================================
// Anonymous Handlers
// ==========================================
// OK
void handleLogin(CommandContext *ctx) {
  sem_wait(sem);
  char *token = strtok(ctx->rawInput, ",");
  char *username = token ? token : "";
  token = strtok(NULL, ",");
  char *password = token ? token : "";

  User *user = loginUser(ctx->sessions, username, password);
  char msg[64];
  if (user != NULL) {
    snprintf(msg, sizeof(msg), "LOGIN,%d,%lu,%d", STATUS_OK, user->session_id, user->isAdmin ? 1 : 0);
  } else {
    snprintf(msg, sizeof(msg), "LOGIN,%d", STATUS_FAIL);
  }
  send(ctx->socket_client, msg, strlen(msg), 0);
  sem_post(sem);
}

void handleLogout(CommandContext *ctx) {
  sem_wait(sem);
  unsigned long sid = strtoul(ctx->rawInput, NULL, 10);
  char msg[32];

  User *user = getUserBySession(ctx->sessions, sid);
  if (user) user->session_id = 0;

  snprintf(msg, sizeof(msg), "LOGOUT,%d", STATUS_OK);
  send(ctx->socket_client, msg, strlen(msg), 0);
  sem_post(sem);
}

// OK
void handleViewProduct(CommandContext *ctx) {
  sem_wait(sem);
  char msg[2048] = COMMAND_VIEW_PRODUCT;
  shared_output[0] = '\0';
  getStore(ctx->store, msg, sizeof(msg));
  send(ctx->socket_client, msg, sizeof(msg), 0);
  sem_post(sem);
}

void handleSearchProduct(CommandContext *ctx) {
  sem_wait(sem);
  char *productid = ctx->rawInput;
  productid[strcspn(productid, "\r\n")] = '\0';
  char msg[2048] = COMMAND_SEARCH_PRODUCT;
  searchStore(ctx->store, productid, msg, sizeof(msg));
  send(ctx->socket_client, msg, strlen(msg), 0);
  sem_post(sem);
}

void handleRegisterMember(CommandContext *ctx) {
  sem_wait(sem);
  char *token = strtok(ctx->rawInput, ",");
  char *username = token ? token : "";
  token = strtok(NULL, ",");
  char *password = token ? token : "";

  int status = registerUser(ctx->sessions, username, password, false);
  char msg[32];
  snprintf(msg, sizeof(msg), "REGISTER_MEMBER,%d", status);
  send(ctx->socket_client, msg, strlen(msg), 0);
  sem_post(sem);
}

// ==========================================
// Member Handlers
// ==========================================

void handleUpdateCart(CommandContext *ctx) {
  sem_wait(sem);
  char *token = strtok(ctx->rawInput, ",");
  unsigned long sid = token ? strtoul(token, NULL, 10) : 0;
  char *productId = strtok(NULL, ",");
  char *qtyStr    = strtok(NULL, ",");
  int quantity    = qtyStr ? atoi(qtyStr) : 0;

  char msg[64];
  User *user = getUserBySession(ctx->sessions, sid);
  if (!user) {
    snprintf(msg, sizeof(msg), "UPDATE_CART,%d - not login", STATUS_INVALID_SESSION);
    send(ctx->socket_client, msg, strlen(msg), 0);
    sem_post(sem);
    return;
  }

  int status = addCart(ctx->order, user->username, productId, quantity);
  if (status == STATUS_OK) saveOrder(ctx->order, ORDER_FILENAME);
  snprintf(msg, sizeof(msg), "UPDATE_CART,%d", status);
  send(ctx->socket_client, msg, strlen(msg), 0);
  sem_post(sem);
}

void handleDeleteFromCart(CommandContext *ctx) {
  sem_wait(sem);
  char *token = strtok(ctx->rawInput, ",");
  unsigned long sid = token ? strtoul(token, NULL, 10) : 0;
  char *productId = strtok(NULL, ",");

  char msg[64];
  User *user = getUserBySession(ctx->sessions, sid);
  if (!user) {
    snprintf(msg, sizeof(msg), "DELETE_FROM_CART,%d", STATUS_INVALID_SESSION);
    send(ctx->socket_client, msg, strlen(msg), 0);
    sem_post(sem);
    return;
  }
  if (!productId) {
    snprintf(msg, sizeof(msg), "DELETE_FROM_CART,%d", STATUS_INVALID_ARGUMENTS);
    send(ctx->socket_client, msg, strlen(msg), 0);
    sem_post(sem);
    return;
  }
  int status = deleteCart(ctx->order, user->username, productId);
  if (status == STATUS_OK) saveOrder(ctx->order, ORDER_FILENAME);
  snprintf(msg, sizeof(msg), "DELETE_FROM_CART,%d", status);
  send(ctx->socket_client, msg, strlen(msg), 0);
  sem_post(sem);
}

void handleClearCart(CommandContext *ctx) {
  sem_wait(sem);
  unsigned long sid = strtoul(ctx->rawInput, NULL, 10);
  char msg[64];

  User *user = getUserBySession(ctx->sessions, sid);
  if (!user) {
    snprintf(msg, sizeof(msg), "CLEAR_CART,%d", STATUS_INVALID_SESSION);
    send(ctx->socket_client, msg, strlen(msg), 0);
    sem_post(sem);
    return;
  }
  int status = clearCart(ctx->order, ctx->store, user->username);
  if (status == STATUS_OK) saveOrder(ctx->order, ORDER_FILENAME);
  snprintf(msg, sizeof(msg), "CLEAR_CART,%d", status);
  send(ctx->socket_client, msg, strlen(msg), 0);
  sem_post(sem);
}

void handleCheckoutCart(CommandContext *ctx) {
  sem_wait(sem);
  unsigned long sid = strtoul(ctx->rawInput, NULL, 10);
  char msg[64];

  User *user = getUserBySession(ctx->sessions, sid);
  if (!user) {
    snprintf(msg, sizeof(msg), "CHECKOUT_CART,%d", STATUS_INVALID_SESSION);
    send(ctx->socket_client, msg, strlen(msg), 0);
    sem_post(sem);
    return;
  }

  for (int i = 0; i < MAX_CART; i++) {
    Cart *c = &ctx->order->carts[i];
    if (!c->isUsed || c->checkout != 0) continue;
    if (strcmp(c->username, user->username) != 0) continue;
    int idx = findProductIndex(ctx->store, c->productId);
    if (idx >= 0)
      ctx->store->items[idx].quantity -= c->quantity;
  }

  int status = checkoutCart(ctx->order, user->username, NULL);
  if (status == STATUS_OK) {
    saveOrder(ctx->order, ORDER_FILENAME);
    saveStore(ctx->store, STORE_FILENAME);
  }
  snprintf(msg, sizeof(msg), "CHECKOUT_CART,%d", status);
  send(ctx->socket_client, msg, strlen(msg), 0);
  sem_post(sem);
}

void handleViewCart(CommandContext *ctx) {
  sem_wait(sem);
  unsigned long sid = strtoul(ctx->rawInput, NULL, 10);
  char msg[2048] = COMMAND_VIEW_CART;

  User *user = getUserBySession(ctx->sessions, sid);
  if (!user) {
    snprintf(msg, sizeof(msg), "VIEW_CART,%d", STATUS_INVALID_SESSION);
    send(ctx->socket_client, msg, strlen(msg), 0);
    sem_post(sem);
    return;
  }
  getOrder(ctx->order, user->username, msg, sizeof(msg), 0);
  send(ctx->socket_client, msg, strlen(msg), 0);
  sem_post(sem);
}

void handleViewOrder(CommandContext *ctx) {
  sem_wait(sem);
  unsigned long sid = strtoul(ctx->rawInput, NULL, 10);
  char msg[2048] = COMMAND_VIEW_ORDER;

  User *user = getUserBySession(ctx->sessions, sid);
  if (!user) {
    snprintf(msg, sizeof(msg), "VIEW_ORDER,%d", STATUS_INVALID_SESSION);
    send(ctx->socket_client, msg, strlen(msg), 0);
    sem_post(sem);
    return;
  }
  if (user->isAdmin)
    getOrder(ctx->order, NULL, msg, sizeof(msg), -1);
  else
    getOrder(ctx->order, user->username, msg, sizeof(msg), 1);
  send(ctx->socket_client, msg, strlen(msg), 0);
  sem_post(sem);
}

// ==========================================
// Admin Handlers (Requires Admin Role)
// ==========================================

void handleAddProduct(CommandContext *ctx) {
  sem_wait(sem);
  char *sidStr    = strtok(ctx->rawInput, ",");
  unsigned long sid = sidStr ? strtoul(sidStr, NULL, 10) : 0;
  char *productId = strtok(NULL, ",");
  char *title     = strtok(NULL, ",");
  char *priceStr  = strtok(NULL, ",");
  char *qtyStr    = strtok(NULL, ",");

  char msg[64];
  User *user = getUserBySession(ctx->sessions, sid);
  if (!user || !user->isAdmin) {
    snprintf(msg, sizeof(msg), "ADD_PRODUCT,%d", STATUS_PERMISSION_DENIED);
    send(ctx->socket_client, msg, strlen(msg), 0);
    sem_post(sem);
    return;
  }
  if (!productId || !title || !priceStr || !qtyStr) {
    snprintf(msg, sizeof(msg), "ADD_PRODUCT,%d", STATUS_INVALID_ARGUMENTS);
    send(ctx->socket_client, msg, strlen(msg), 0);
    sem_post(sem);
    return;
  }

  float price = atof(priceStr);
  int qty     = atoi(qtyStr);
  int status  = addProduct(ctx->store, productId, title, price, qty);
  if (status) saveStore(ctx->store, STORE_FILENAME);
  snprintf(msg, sizeof(msg), "ADD_PRODUCT,%d", status ? STATUS_OK : STATUS_ADD_PRODUCT_FAIL);
  send(ctx->socket_client, msg, strlen(msg), 0);
  sem_post(sem);
}

void handleUpdateProduct(CommandContext *ctx) {
  sem_wait(sem);
  char *sidStr    = strtok(ctx->rawInput, ",");
  unsigned long sid = sidStr ? strtoul(sidStr, NULL, 10) : 0;
  char *productId = strtok(NULL, ",");
  char *title     = strtok(NULL, ",");
  char *priceStr  = strtok(NULL, ",");
  char *qtyStr    = strtok(NULL, ",");

  char msg[64];
  User *user = getUserBySession(ctx->sessions, sid);
  if (!user || !user->isAdmin) {
    snprintf(msg, sizeof(msg), "UPDATE_PRODUCT,%d", STATUS_PERMISSION_DENIED);
    send(ctx->socket_client, msg, strlen(msg), 0);
    sem_post(sem);
    return;
  }
  if (!productId || !title || !priceStr || !qtyStr) {
    snprintf(msg, sizeof(msg), "UPDATE_PRODUCT,%d", STATUS_INVALID_ARGUMENTS);
    send(ctx->socket_client, msg, strlen(msg), 0);
    sem_post(sem);
    return;
  }

  float price = atof(priceStr);
  int qty     = atoi(qtyStr);
  int status  = updateProduct(ctx->store, productId, title, price, qty);
  if (status) saveStore(ctx->store, STORE_FILENAME);
  snprintf(msg, sizeof(msg), "UPDATE_PRODUCT,%d", status ? STATUS_OK : STATUS_UPDATE_PRODUCT_FAIL);
  send(ctx->socket_client, msg, strlen(msg), 0);
  sem_post(sem);
}

void handleRemoveProduct(CommandContext *ctx) {
  sem_wait(sem);
  char *sidStr    = strtok(ctx->rawInput, ",");
  unsigned long sid = sidStr ? strtoul(sidStr, NULL, 10) : 0;
  char *productId = strtok(NULL, ",");

  char msg[64];
  User *user = getUserBySession(ctx->sessions, sid);
  if (!user || !user->isAdmin) {
    snprintf(msg, sizeof(msg), "REMOVE_PRODUCT,%d", STATUS_PERMISSION_DENIED);
    send(ctx->socket_client, msg, strlen(msg), 0);
    sem_post(sem);
    return;
  }
  if (!productId) {
    snprintf(msg, sizeof(msg), "REMOVE_PRODUCT,%d", STATUS_INVALID_ARGUMENTS);
    send(ctx->socket_client, msg, strlen(msg), 0);
    sem_post(sem);
    return;
  }
  productId[strcspn(productId, "\r\n")] = '\0';

  int status = deleteProduct(ctx->store, productId);
  if (status) saveStore(ctx->store, STORE_FILENAME);
  snprintf(msg, sizeof(msg), "REMOVE_PRODUCT,%d", status ? STATUS_OK : STATUS_DELETE_PRODUCT_FAIL);
  send(ctx->socket_client, msg, strlen(msg), 0);
  sem_post(sem);
}

void handleViewMember(CommandContext *ctx) {
  sem_wait(sem);
  unsigned long sid = strtoul(ctx->rawInput, NULL, 10);
  char msg[4096] = COMMAND_VIEW_MEMBER;

  User *user = getUserBySession(ctx->sessions, sid);
  if (!user || !user->isAdmin) {
    snprintf(msg, sizeof(msg), "VIEW_MEMBER,%d", STATUS_PERMISSION_DENIED);
    send(ctx->socket_client, msg, strlen(msg), 0);
    sem_post(sem);
    return;
  }

  snprintf(msg + strlen(msg), sizeof(msg) - strlen(msg), ",%d,", STATUS_OK);
  char line[USERNAME_SIZE + 16];
  int count = 0;
  for (int i = 0; i < MAX_USERS; i++) {
    if (ctx->sessions->users[i].isActive) {
      snprintf(line, sizeof(line), "%s-%s|",
               ctx->sessions->users[i].username,
               ctx->sessions->users[i].isAdmin ? "Admin" : "Member");
      if (strlen(msg) + strlen(line) < sizeof(msg) - 1)
        strncat(msg, line, sizeof(msg) - strlen(msg) - 1);
      count++;
    }
  }
  if (count == 0)
    snprintf(msg, sizeof(msg), "VIEW_MEMBER,%d,", STATUS_EMPTY_USER);

  send(ctx->socket_client, msg, strlen(msg), 0);
  sem_post(sem);
}

void handle_shutdown(int sig) {
  printf("\n[System] Shutdown signal received (%d). Cleaning up...\n", sig);
  if (ISVALIDSOCKET(g_socket_listen)) CLOSESOCKET(g_socket_listen);
  if (sem) {
    sem_destroy(sem);
    munmap(sem, sizeof(sem_t));
  }
  if (g_store)    munmap(g_store, sizeof(Store));
  if (g_order)    munmap(g_order, sizeof(Order));
  if (g_sessions) munmap(g_sessions, sizeof(UserSessions));
  if (shared_output) munmap(shared_output, 8192);
  exit(0);
}

int setup(char *port) {
  printf("[System] Setting up server on port %s...\n", port);
  struct addrinfo hints;
  memset(&hints, 0, sizeof(hints));
  hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_flags = AI_PASSIVE;

  struct addrinfo *bind_address;
  if (getaddrinfo(0, port, &hints, &bind_address) != 0) {
    fprintf(stderr, "getaddrinfo() failed. (%d)\n", GETSOCKETERRNO());
    return -1;
  }

  SOCKET s = socket(bind_address->ai_family, bind_address->ai_socktype,
                    bind_address->ai_protocol);
  if (!ISVALIDSOCKET(s)) {
    fprintf(stderr, "socket() failed. (%d)\n", GETSOCKETERRNO());
    freeaddrinfo(bind_address);
    return -1;
  }

  int yes = 1;
  if (setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes)) < 0) {
    fprintf(stderr, "setsockopt() failed. (%d)\n", GETSOCKETERRNO());
    CLOSESOCKET(s);
    freeaddrinfo(bind_address);
    return -1;
  }

  if (bind(s, bind_address->ai_addr, bind_address->ai_addrlen)) {
    fprintf(stderr, "bind() failed. (%d)\n", GETSOCKETERRNO());
    CLOSESOCKET(s);
    freeaddrinfo(bind_address);
    return -1;
  }
  freeaddrinfo(bind_address);

  if (listen(s, 10) < 0) {
    fprintf(stderr, "listen() failed. (%d)\n", GETSOCKETERRNO());
    CLOSESOCKET(s);
    return -1;
  }

  printf("[System] Listening on port %s.\n", port);
  return s;
}

int dispatchCommand(CommandContext *ctx, char *commandName) {
  commandName[strcspn(commandName, "\r\n")] = '\0';
  char *comma = strchr(commandName, ',');
  if (comma) {
    *comma = '\0';
    ctx->rawInput = comma + 1;
  } else {
    ctx->rawInput = commandName + strlen(commandName);
  }
  for (int i = 0; commandTable[i].commandName != NULL; i++) {
    if (strcmp(commandTable[i].commandName, commandName) == 0) {
      commandTable[i].handler(ctx);
      if (strcmp(commandName, COMMAND_LOGOUT) == 0) return -1;
      return 0;
    }
  }
  char *err = "[Server] Unknown command.\n";
  send(ctx->socket_client, err, strlen(err), 0);
  return 0;
}