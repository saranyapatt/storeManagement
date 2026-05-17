#include "auth.c"
#include "common.h"
#include "store.c"
#include <arpa/inet.h>
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

char *shared_output = NULL;
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
void handle_shutdown(int sig);
int setup(char *port);
int dispatchCommand(CommandContext *ctx, char *commandName);

// string->*func
CommandEntry commandTable[] = {
    {COMMAND_LOGIN, handleLogin},
    {COMMAND_LOGOUT, handleLogout},
    {COMMAND_VIEW_PRODUCT, handleViewProduct},
    {COMMAND_SEARCH_PRODUCT, handleSearchProduct},
    {COMMAND_REGISTER_MEMBER, handleRegisterMember},
    {COMMAND_UPDATE_CART, handleUpdateCart},
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

#define STORE_FILENAME "./txt/store.txt"
void replace_char(char *str, char old_char, char new_char);

int main() {
  // SET SHARED MEMORY
  // store
  Store *store = mmap(NULL, sizeof(Store), PROT_READ | PROT_WRITE,
                      MAP_SHARED | MAP_ANONYMOUS, -1, 0);
  loadStore(store, STORE_FILENAME);

  // user
  UserSessions *userSession = mmap(NULL, sizeof(UserSessions), PROT_READ | PROT_WRITE,
                    MAP_SHARED | MAP_ANONYMOUS, -1, 0);
  loadUser(userSession, USER_FILENAME);

  shared_output = mmap(NULL, 8192, PROT_READ | PROT_WRITE,
                       MAP_SHARED | MAP_ANONYMOUS, -1, 0);

  // sem
  sem_t sem;
  sem_init(&sem, 1, 1);

  // init base Address
  printf("Configuring local address...\n");
  struct addrinfo hints;
  memset(&hints, 0, sizeof(hints));
  hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_flags = AI_PASSIVE;
  struct addrinfo *bind_address;
  int addressinfo = getaddrinfo(0, "8082", &hints, &bind_address);

  printf("Creating socket...\n");
  SOCKET socket_listen;
  socket_listen = socket(bind_address->ai_family, bind_address->ai_socktype,
                         bind_address->ai_protocol);

  if (!ISVALIDSOCKET(socket_listen)) {
    fprintf(stderr, "socket() failed. (%d)\n", GETSOCKETERRNO());
    return 1;
  }

  int yes = 1;
  if (setsockopt(socket_listen, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes)) <
      0) {
    fprintf(stderr, "setsockopt() failed. (%d)\n", GETSOCKETERRNO());
    return 1;
  }

  printf("Binding socket to local address...\n");
  if (bind(socket_listen, bind_address->ai_addr, bind_address->ai_addrlen)) {
    fprintf(stderr, "bind() failed. (%d)\n", GETSOCKETERRNO());
    return 1;
  }
  freeaddrinfo(bind_address);

  printf("Listening...\n");
  if (listen(socket_listen, 10) < 0) {
    fprintf(stderr, "listen() failed. (%d)\n", GETSOCKETERRNO());
    return 1;
  }

  printf("Waiting for connections...\n");
  while (1) {
    struct sockaddr_storage client_address;
    socklen_t client_len = sizeof(client_address);
    SOCKET socket_client =
        accept(socket_listen, (struct sockaddr *)&client_address, &client_len);
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
      CLOSESOCKET(socket_listen);
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
        ctx.store = store;
        ctx.order = NULL;
        ctx.sessions = userSession;
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

  CLOSESOCKET(socket_listen);
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
// Anonymous Handlers (General Access)
// ==========================================
// OK
void handleLogin(CommandContext *ctx) {
  char *token = strtok(ctx->rawInput, ",");
  char *username = token ? token : "";
  token = strtok(NULL, ",");
  char *password = token ? token : "";

  User *user = loginUser(ctx->sessions, username, password);
  char msg[64];
  if (user != NULL) {
    snprintf(msg, sizeof(msg), "LOGIN,%d,%lu", STATUS_OK, user->session_id);
  } else {
    snprintf(msg, sizeof(msg), "LOGIN,%d", STATUS_FAIL);
  }
  send(ctx->socket_client, msg, strlen(msg), 0);
}

void handleLogout(CommandContext *ctx) {
  char msg[] = "\n[Server] Executing: handleLogout\nStatus: Session Cleared\n";
  send(ctx->socket_client, msg, strlen(msg), 0);
  ctx->sessions->users->isActive = false;
}

// OK
void handleViewProduct(CommandContext *ctx) {
  char msg[2048] = COMMAND_VIEW_PRODUCT;
  shared_output[0] = '\0';
  getStore(ctx->store, msg, sizeof(msg));
  send(ctx->socket_client, msg, sizeof(msg), 0);
}

void handleSearchProduct(CommandContext *ctx) {
  char *productid = ctx->rawInput;
  productid[strcspn(productid, "\r\n")] = '\0';
  char msg[2048] = COMMAND_SEARCH_PRODUCT;
  searchStore(ctx->store, productid, msg, sizeof(msg));
  send(ctx->socket_client, msg, strlen(msg), 0);
}

void handleRegisterMember(CommandContext *ctx) {
  char *token = strtok(ctx->rawInput, ",");
  char *username = token ? token : "";
  token = strtok(NULL, ",");
  char *password = token ? token : "";

  int status = registerUser(ctx->sessions, username, password, false);
  char msg[32];
  snprintf(msg, sizeof(msg), "REGISTER_MEMBER,%d", status);
  send(ctx->socket_client, msg, strlen(msg), 0);
}

// ==========================================
// Member Handlers (Requires Login)
// ==========================================

void handleUpdateCart(CommandContext *ctx) {
  char msg[] =
      "\n[Server] Executing: handleUpdateCart\nAdding item to your cart...\n";
  send(ctx->socket_client, msg, strlen(msg), 0);
}

void handleClearCart(CommandContext *ctx) {
  char msg[] =
      "\n[Server] Executing: handleClearCart\nYour cart is now empty.\n";
  send(ctx->socket_client, msg, strlen(msg), 0);
}

void handleCheckoutCart(CommandContext *ctx) {
  char msg[] = "\n[Server] Executing: handleCheckoutCart\nProcessing order and "
               "payment...\n";
  send(ctx->socket_client, msg, strlen(msg), 0);
}

void handleViewCart(CommandContext *ctx) {
  char msg[] =
      "\n[Server] Executing: handleViewCart\nShowing items in your cart.\n";
  send(ctx->socket_client, msg, strlen(msg), 0);
}

void handleViewOrder(CommandContext *ctx) {
  char msg[] =
      "\n[Server] Executing: handleViewOrder\nRetrieving order history...\n";
  send(ctx->socket_client, msg, strlen(msg), 0);
}

// ==========================================
// Admin Handlers (Requires Admin Role)
// ==========================================

void handleAddProduct(CommandContext *ctx) {
  char msg[] = "\n[Server: ADMIN] Executing: handleAddProduct\nAdding new "
               "stock to database...\n";
  send(ctx->socket_client, msg, strlen(msg), 0);
}

void handleUpdateProduct(CommandContext *ctx) {
  char msg[] = "\n[Server: ADMIN] Executing: handleUpdateProduct\nUpdating "
               "item details...\n";
  send(ctx->socket_client, msg, strlen(msg), 0);
}

void handleRemoveProduct(CommandContext *ctx) {
  char msg[] = "\n[Server: ADMIN] Executing: handleRemoveProduct\nItem removed "
               "from store.\n";
  send(ctx->socket_client, msg, strlen(msg), 0);
}

void handleViewMember(CommandContext *ctx) {
  char msg[] = "\n[Server: ADMIN] Executing: handleViewMember\nListing all "
               "registered members.\n";
  send(ctx->socket_client, msg, strlen(msg), 0);
}

// ==========================================
// Common / System Functions
// ==========================================

void handle_shutdown(int sig) {
  printf("\n[System] Shutdown signal received (%d). Cleaning up...\n", sig);
  // TODO: munmap and close sockets if necessary
  exit(0);
}

int setup(char *port) {
  printf("[System] Setting up server on port %s...\n", port);
  // Placeholder for socket initialization logic
  return 0;
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