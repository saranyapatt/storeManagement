#include "common.h"
#include <arpa/inet.h>
#include <errno.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#define ISVALIDSOCKET(s) ((s) >= 0)
#define CLOSESOCKET(s) close(s)
#define SOCKET int
#define GETSOCKETERRNO() (errno)

unsigned long session = 0;
int isAdmin = 0;
char *getMenu(unsigned long);
SOCKET serverConnect(struct addrinfo *);
int serverDisconnect(SOCKET);
int testConnect(struct addrinfo *);
int doExit(struct addrinfo *peer_address);
int doViewProducts(struct addrinfo *peer_address);
int doSearchProduct(struct addrinfo *peer_address);
int doLogin(struct addrinfo *peer_address);
int doRegister(struct addrinfo *peer_address);
int doAddToCart(struct addrinfo *peer_address);
int doViewCart(struct addrinfo *peer_address);
int doDeleteFromCart(struct addrinfo *peer_address);
int doOrder(struct addrinfo *peer_address);
int doViewHistory(struct addrinfo *peer_address);
int doLogout(struct addrinfo *peer_address);
int doAddProduct(struct addrinfo *peer_address);
int doUpdateProduct(struct addrinfo *peer_address);
int doRemoveProduct(struct addrinfo *peer_address);
int doViewMembers(struct addrinfo *peer_address);
void runMenu(struct addrinfo *peer_address);

int main() {
  printf("Connecting to server...\n");
  struct addrinfo hints;

  memset(&hints, 0, sizeof(hints));
  hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_STREAM;

  struct addrinfo *peer_address;
  if (getaddrinfo("127.0.0.1", "8082", &hints, &peer_address)) {
    fprintf(stderr, "getaddrinfo() failed.\n");
    return 0;
  }

  if (!testConnect(peer_address))
    return 0;

  runMenu(peer_address);
  freeaddrinfo(peer_address);
  return 0;
}

void runMenu(struct addrinfo *peer_address) {
  printf("%s", getMenu(session));
  int clientChoice = 0;
  if (!scanf("%d", &clientChoice))
    return;
  getchar();

  if (session == 0) {
    switch (clientChoice) {
    case 0: doExit(peer_address); return;
    case 1: doViewProducts(peer_address); break;
    case 2: doSearchProduct(peer_address); break;
    case 3: doLogin(peer_address); break;
    case 4: doRegister(peer_address); break;
    default: printf("Wrong menu. Please select 0-4.\n"); break;
    }
  } else if (isAdmin) {
    switch (clientChoice) {
    case 0: doExit(peer_address); return;
    case 1: {
      printf("\n======================\n"
             "   Manage Products\n"
             "======================\n"
             "[4] Add Product\n"
             "[5] Update Product\n"
             "[6] Remove Product\n"
             "[0] Back\n"
             "Please select: ");
      int sub = 0;
      if (!scanf("%d", &sub)) break;
      getchar();
      switch (sub) {
        case 4: doAddProduct(peer_address); break;
        case 5: doUpdateProduct(peer_address); break;
        case 6: doRemoveProduct(peer_address); break;
        case 0: break;
        default: printf("Wrong menu.\n"); break;
      }
      break;
    }
    case 2: doViewHistory(peer_address); break;
    case 3: doViewMembers(peer_address); break;
    case 7: doLogout(peer_address); break;
    default: printf("Wrong menu. Please select 0-3 or 7.\n"); break;
    }
  } else {
    switch (clientChoice) {
    case 0: doExit(peer_address); return;
    case 1: doViewProducts(peer_address); break;
    case 2: doSearchProduct(peer_address); break;
    case 3: doAddToCart(peer_address); break;
    case 4: doViewCart(peer_address); break;
    case 5: doOrder(peer_address); break;
    case 6: doViewHistory(peer_address); break;
    case 7: doLogout(peer_address); break;
    default: printf("Wrong menu. Please select 0-7.\n"); break;
    }
  }
  runMenu(peer_address);
}

char *getMenu(unsigned long sessionId) {
  char *menu = malloc(1024);
  if (menu == NULL)
    return NULL;
  if (sessionId == 0) {
    strcpy(menu, "\n======================\n"
                 " Welcome to our Shop\n"
                 "======================\n"
                 "[1] View Products\n"
                 "[2] Search Products\n"
                 "[3] Login\n"
                 "[4] Register\n"
                 "[0] Exit\n"
                 "Please select [0-4]: ");
  } else if (isAdmin) {
    strcpy(menu, "\n======================\n"
                 "     Admin Menu\n"
                 "======================\n"
                 "[1] Manage Products\n"
                 "[2] View All Orders\n"
                 "[3] View Members\n"
                 "[7] Logout\n"
                 "[0] Exit\n"
                 "Please select: ");
  } else {
    strcpy(menu, "\n======================\n"
                 "    Member Menu\n"
                 "======================\n"
                 "[1] View Products\n"
                 "[2] Search Products\n"
                 "[3] Add to Cart\n"
                 "[4] View Cart\n"
                 "[5] Checkout\n"
                 "[6] View Orders\n"
                 "[7] Logout\n"
                 "[0] Exit\n"
                 "Please select [0-7]: ");
  }
  return menu;
}

SOCKET serverConnect(struct addrinfo *peer_address) {
  SOCKET socket_peer =
      socket(peer_address->ai_family, peer_address->ai_socktype,
             peer_address->ai_protocol);
  if (!ISVALIDSOCKET(socket_peer)) {
    fprintf(stderr, "socket() failed. (%d)\n", GETSOCKETERRNO());
    return -1;
  }
  if (connect(socket_peer, peer_address->ai_addr, peer_address->ai_addrlen)) {
    fprintf(stderr, "connect() failed. (%d)\n", GETSOCKETERRNO());
    CLOSESOCKET(socket_peer);
    return -1;
  }
  return socket_peer;
}

int serverDisconnect(SOCKET socket_peer) {
  CLOSESOCKET(socket_peer);
  return 1;
}

int testConnect(struct addrinfo *peer_address) {
  SOCKET s = serverConnect(peer_address);
  if (ISVALIDSOCKET(s)) {
    serverDisconnect(s);
    return 1;
  }
  printf("Server is not working right now. Please try again later.\n");
  return 0;
}

int doExit(struct addrinfo *peer_address) {
  printf("Closing connection...\nSuccessfully closed connection\n");
  return 1;
}

int doViewProducts(struct addrinfo *peer_address) {
  char response[2048];
  memset(response, 0, sizeof(response));
  SOCKET s = serverConnect(peer_address);
  if (!ISVALIDSOCKET(s))
    return 0;
  send(s, COMMAND_VIEW_PRODUCT, strlen(COMMAND_VIEW_PRODUCT), 0);
  int bytes = recv(s, response, sizeof(response) - 1, 0);
  if (bytes < 1) {
    serverDisconnect(s);
    return 0;
  }
  response[bytes] = '\0';
  serverDisconnect(s);

  char *p = strchr(response, ',');
  if (!p) {
    printf("Invalid response.\n");
    return 0;
  }
  p = strchr(p + 1, ',');
  if (!p) {
    printf("Invalid response.\n");
    return 0;
  }
  p++;

  printf("\n%-8s | %-33s | %-7s | %-5s\n", "ID", "Title", "Price", "Qty");
  printf(
      "--------------------------------------------------------------------\n");
  int count = 0;
  char *item = strtok(p, "|");
  while (item != NULL) {
    char productId[PRODUCTID_SIZE];
    char productTitle[PRODUCT_TITLE_SIZE];
    float price;
    int quantity;
    if (sscanf(item, "%9[^-]-%127[^-]-%f-%d", productId, productTitle, &price,
               &quantity) == 4) {
      printf("%-8s | %-33s | %7.2f | %5d\n", productId, productTitle, price,
             quantity);
      count++;
    }
    item = strtok(NULL, "|");
  }
  if (count == 0)
    printf("Store is currently empty.\n");
  return 1;
}

int doSearchProduct(struct addrinfo *peer_address) {
  char product[24];
  char command[256];
  char response[2048];
  memset(response, 0, sizeof(response));
  printf("Enter Product ID: ");
  fgets(product, sizeof(product), stdin);
  product[strcspn(product, "\r\n")] = '\0';
  snprintf(command, sizeof(command), "%s,%s", COMMAND_SEARCH_PRODUCT, product);
  SOCKET s = serverConnect(peer_address);
  if (!ISVALIDSOCKET(s))
    return 0;
  send(s, command, strlen(command), 0);
  int bytes = recv(s, response, sizeof(response) - 1, 0);
  if (bytes < 1) {
    serverDisconnect(s);
    return 0;
  }
  response[bytes] = '\0';
  serverDisconnect(s);

  char *p = strchr(response, ',');
  if (!p) {
    printf("Invalid response.\n");
    return 0;
  }
  int status = atoi(p + 1);
  p = strchr(p + 1, ',');
  if (!p) {
    printf("Invalid response.\n");
    return 0;
  }
  p++;

  if (status != STATUS_OK) {
    printf("Product '%s' not found.\n", product);
    return 0;
  }

  char productId[PRODUCTID_SIZE];
  char productTitle[PRODUCT_TITLE_SIZE];
  float price;
  int quantity;
  if (sscanf(p, "%9[^-]-%127[^-]-%f-%d", productId, productTitle, &price,
             &quantity) == 4) {
    printf("\n-----------------------------\n");
    printf("ID    : %s\n", productId);
    printf("Title : %s\n", productTitle);
    printf("Price : %.2f\n", price);
    printf("Qty   : %d\n", quantity);
    printf("-----------------------------\n");
  } else {
    printf("Failed to parse product data.\n");
  }
  return 1;
}

int doLogin(struct addrinfo *peer_address) {
  char username[USERNAME_SIZE];
  char password[PASSWORD_SIZE];
  char command[256];
  char response[2048];
  memset(response, 0, sizeof(response));
  printf("Enter username: ");
  fgets(username, sizeof(username), stdin);
  username[strcspn(username, "\r\n")] = '\0';
  printf("Enter password: ");
  fgets(password, sizeof(password), stdin);
  password[strcspn(password, "\r\n")] = '\0';
  snprintf(command, sizeof(command), "%s,%s,%s", COMMAND_LOGIN, username,
           password);
  SOCKET s = serverConnect(peer_address);
  if (!ISVALIDSOCKET(s))
    return 0;
  send(s, command, strlen(command), 0);
  int bytes = recv(s, response, sizeof(response) - 1, 0);
  if (bytes < 1) {
    serverDisconnect(s);
    return 0;
  }
  response[bytes] = '\0';
  serverDisconnect(s);

  char *p = strchr(response, ',');
  if (!p) {
    printf("Invalid response.\n");
    return 0;
  }
  int status = atoi(p + 1);

  switch (status) {
  case STATUS_OK:
    p = strchr(p + 1, ',');
    if (p) session = strtoul(p + 1, NULL, 10);
    p = strchr(p + 1, ',');
    if (p) isAdmin = atoi(p + 1);
    printf("Login successful! Welcome, %s.%s\n", username, isAdmin ? " [Admin]" : "");
    break;
  case STATUS_FAIL:
    printf("Login failed. Invalid username or password.\n");
    break;
  case STATUS_INVALID_ARGUMENTS:
    printf("Login failed. Username or password missing.\n");
    break;
  default:
    printf("Login failed. Error code: %d\n", status);
    break;
  }
  return status == STATUS_OK ? 1 : 0;
}

int doRegister(struct addrinfo *peer_address) {
  char username[USERNAME_SIZE];
  char password[PASSWORD_SIZE];
  char conf_password[PASSWORD_SIZE];
  char command[256];
  char response[2048];

  printf("Registration\nEnter username: ");
  fgets(username, sizeof(username), stdin);
  username[strcspn(username, "\r\n")] = '\0';

  printf("Enter password: ");
  fgets(password, sizeof(password), stdin);
  password[strcspn(password, "\r\n")] = '\0';
  printf("Confirm password: ");
  fgets(conf_password, sizeof(conf_password), stdin);
  conf_password[strcspn(conf_password, "\r\n")] = '\0';
  while (strcmp(password, conf_password) != 0) {
    printf("Passwords do not match. Enter password again: ");
    fgets(password, sizeof(password), stdin);
    password[strcspn(password, "\r\n")] = '\0';
    printf("Confirm password: ");
    fgets(conf_password, sizeof(conf_password), stdin);
    conf_password[strcspn(conf_password, "\r\n")] = '\0';
  }

  snprintf(command, sizeof(command), "%s,%s,%s", COMMAND_REGISTER_MEMBER,
           username, password);
  SOCKET s = serverConnect(peer_address);
  if (!ISVALIDSOCKET(s))
    return 0;
  send(s, command, strlen(command), 0);
  int bytes = recv(s, response, sizeof(response) - 1, 0);
  if (bytes < 1) {
    serverDisconnect(s);
    return 0;
  }
  response[bytes] = '\0';
  serverDisconnect(s);

  char *p = strchr(response, ',');
  if (!p) {
    printf("Invalid response.\n");
    return 0;
  }
  int status = atoi(p + 1);

  switch (status) {
  case STATUS_OK:
    printf("Registration successful! You can now login.\n");
    break;
  case STATUS_DUPLICATE_USER:
    printf("Registration failed. Username '%s' already exists.\n", username);
    break;
  case STATUS_INVALID_ARGUMENTS:
    printf("Registration failed. Invalid input.\n");
    break;
  default:
    printf("Registration failed. Error code: %d\n", status);
    break;
  }
  return status == STATUS_OK ? 1 : 0;
}

int doAddToCart(struct addrinfo *peer_address) {
  char productId[PRODUCTID_SIZE];
  int quantity;
  char command[256];
  char response[256];
  memset(response, 0, sizeof(response));

  printf("Enter Product ID: ");
  fgets(productId, sizeof(productId), stdin);
  productId[strcspn(productId, "\r\n")] = '\0';

  printf("Enter Quantity: ");
  scanf("%d", &quantity);
  getchar();

  snprintf(command, sizeof(command), "%s,%lu,%s,%d",
           COMMAND_UPDATE_CART, session, productId, quantity);

  SOCKET s = serverConnect(peer_address);
  if (!ISVALIDSOCKET(s)) return 0;
  send(s, command, strlen(command), 0);
  int bytes = recv(s, response, sizeof(response) - 1, 0);
  if (bytes < 1) { serverDisconnect(s); return 0; }
  response[bytes] = '\0';
  serverDisconnect(s);

  char *p = strchr(response, ',');
  if (!p) { printf("Invalid response.\n"); return 0; }
  int status = atoi(p + 1);

  switch (status) {
    case STATUS_OK:   printf("Product added to cart.\n"); break;
    default:          printf("Failed to add to cart. Error: %d\n", status); break;
  }
  return status == STATUS_OK ? 1 : 0;
}

int doViewCart(struct addrinfo *peer_address) {
  char command[64];
  char response[2048];
  memset(response, 0, sizeof(response));

  snprintf(command, sizeof(command), "%s,%lu", COMMAND_VIEW_CART, session);
  SOCKET s = serverConnect(peer_address);
  if (!ISVALIDSOCKET(s)) return 0;
  send(s, command, strlen(command), 0);
  int bytes = recv(s, response, sizeof(response) - 1, 0);
  if (bytes < 1) { serverDisconnect(s); return 0; }
  response[bytes] = '\0';
  serverDisconnect(s);

  char *p = strchr(response, ',');
  if (!p) { printf("Invalid response.\n"); return 0; }
  int status = atoi(p + 1);

  if (status == STATUS_EMPTY_CART) { printf("Your cart is empty.\n"); return 1; }
  if (status != STATUS_OK)         { printf("Error: %d\n", status); return 0; }

  p = strchr(p + 1, ',');
  if (!p) { printf("Invalid response.\n"); return 0; }
  p++;

  printf("\n%-12s | %-5s\n", "Product ID", "Qty");
  printf("--------------------\n");

  int count = 0;
  char *item = strtok(p, "|");
  while (item != NULL) {
    char productId[PRODUCTID_SIZE];
    int quantity;
    if (sscanf(item, "%9[^-]-%d", productId, &quantity) == 2) {
      printf("%-12s | %5d\n", productId, quantity);
      count++;
    }
    item = strtok(NULL, "|");
  }
  if (count == 0) printf("Your cart is empty.\n");
  return 1;
}

int doDeleteFromCart(struct addrinfo *peer_address) {
  char productId[PRODUCTID_SIZE];
  char command[128];
  char response[256];
  memset(response, 0, sizeof(response));

  printf("Enter Product ID to remove: ");
  fgets(productId, sizeof(productId), stdin);
  productId[strcspn(productId, "\r\n")] = '\0';

  snprintf(command, sizeof(command), "%s,%lu,%s",
           COMMAND_DELETE_FROM_CART, session, productId);

  SOCKET s = serverConnect(peer_address);
  if (!ISVALIDSOCKET(s)) return 0;
  send(s, command, strlen(command), 0);
  int bytes = recv(s, response, sizeof(response) - 1, 0);
  if (bytes < 1) { serverDisconnect(s); return 0; }
  response[bytes] = '\0';
  serverDisconnect(s);

  char *p = strchr(response, ',');
  if (!p) { printf("Invalid response.\n"); return 0; }
  int status = atoi(p + 1);

  switch (status) {
    case STATUS_OK:               printf("Item removed from cart.\n"); break;
    case STATUS_ITEM_NOT_IN_CART: printf("Item is not in your cart.\n"); break;
    case STATUS_INVALID_SESSION:  printf("Session invalid. Please log in again.\n"); break;
    default:                      printf("Failed to remove item. Error: %d\n", status); break;
  }
  return status == STATUS_OK ? 1 : 0;
}

int doOrder(struct addrinfo *peer_address) {
  char command[64];
  char response[256];
  memset(response, 0, sizeof(response));

  snprintf(command, sizeof(command), "%s,%lu", COMMAND_CHECKOUT_CART, session);
  SOCKET s = serverConnect(peer_address);
  if (!ISVALIDSOCKET(s)) return 0;
  send(s, command, strlen(command), 0);
  int bytes = recv(s, response, sizeof(response) - 1, 0);
  if (bytes < 1) { serverDisconnect(s); return 0; }
  response[bytes] = '\0';
  serverDisconnect(s);

  char *p = strchr(response, ',');
  if (!p) { printf("Invalid response.\n"); return 0; }
  int status = atoi(p + 1);

  switch (status) {
    case STATUS_OK:               printf("Checkout successful! Your order has been placed.\n"); break;
    case STATUS_EMPTY_CART:       printf("Your cart is empty. Nothing to checkout.\n"); break;
    case STATUS_INVALID_SESSION:  printf("Not logged in.\n"); break;
    default:                      printf("Checkout failed. Error: %d\n", status); break;
  }
  return status == STATUS_OK ? 1 : 0;
}

int doViewHistory(struct addrinfo *peer_address) {
  char command[64];
  char response[2048];
  memset(response, 0, sizeof(response));

  snprintf(command, sizeof(command), "%s,%lu", COMMAND_VIEW_ORDER, session);
  SOCKET s = serverConnect(peer_address);
  if (!ISVALIDSOCKET(s)) return 0;
  send(s, command, strlen(command), 0);
  int bytes = recv(s, response, sizeof(response) - 1, 0);
  if (bytes < 1) { serverDisconnect(s); return 0; }
  response[bytes] = '\0';
  serverDisconnect(s);

  char *p = strchr(response, ',');
  if (!p) { printf("Invalid response.\n"); return 0; }
  int status = atoi(p + 1);

  if (status == STATUS_EMPTY_CART)      { printf("No order history found.\n"); return 1; }
  if (status == STATUS_INVALID_SESSION) { printf("Not logged in.\n"); return 0; }
  if (status != STATUS_OK)              { printf("Error: %d\n", status); return 0; }

  p = strchr(p + 1, ',');
  if (!p) { printf("Invalid response.\n"); return 0; }
  p++;

  int count = 0;
  char *item = strtok(p, "|");
  if (isAdmin) {
    printf("\n%-15s | %-12s | %-5s\n", "Username", "Product ID", "Qty");
    printf("----------------------------------------\n");
    while (item != NULL) {
      char username[USERNAME_SIZE];
      char productId[PRODUCTID_SIZE];
      int quantity;
      if (sscanf(item, "%19[^/]/%9[^-]-%d", username, productId, &quantity) == 3) {
        printf("%-15s | %-12s | %5d\n", username, productId, quantity);
        count++;
      }
      item = strtok(NULL, "|");
    }
  } else {
    printf("\n%-12s | %-5s\n", "Product ID", "Qty");
    printf("--------------------\n");
    while (item != NULL) {
      char productId[PRODUCTID_SIZE];
      int quantity;
      if (sscanf(item, "%9[^-]-%d", productId, &quantity) == 2) {
        printf("%-12s | %5d\n", productId, quantity);
        count++;
      }
      item = strtok(NULL, "|");
    }
  }
  if (count == 0) printf("No order history found.\n");
  return 1;
}

int doLogout(struct addrinfo *peer_address) {
  char command[64];
  char response[256];
  memset(response, 0, sizeof(response));

  snprintf(command, sizeof(command), "%s,%lu", COMMAND_LOGOUT, session);
  SOCKET s = serverConnect(peer_address);
  if (!ISVALIDSOCKET(s)) return 0;
  send(s, command, strlen(command), 0);
  int bytes = recv(s, response, sizeof(response) - 1, 0);
  if (bytes > 0) response[bytes] = '\0';
  serverDisconnect(s);

  session = 0;
  isAdmin = 0;
  printf("Logged out successfully.\n");
  return 1;
}

int doAddProduct(struct addrinfo *peer_address) {
  char productId[PRODUCTID_SIZE];
  char title[PRODUCT_TITLE_SIZE];
  float price;
  int qty;
  char command[512];
  char response[256];
  memset(response, 0, sizeof(response));

  printf("Product ID: ");
  fgets(productId, sizeof(productId), stdin);
  productId[strcspn(productId, "\r\n")] = '\0';

  printf("Title: ");
  fgets(title, sizeof(title), stdin);
  title[strcspn(title, "\r\n")] = '\0';

  printf("Price: ");
  scanf("%f", &price);
  printf("Quantity: ");
  scanf("%d", &qty);
  getchar();

  snprintf(command, sizeof(command), "%s,%lu,%s,%s,%.2f,%d",
           COMMAND_ADD_PRODUCT, session, productId, title, price, qty);

  SOCKET s = serverConnect(peer_address);
  if (!ISVALIDSOCKET(s)) return 0;
  send(s, command, strlen(command), 0);
  int bytes = recv(s, response, sizeof(response) - 1, 0);
  if (bytes < 1) { serverDisconnect(s); return 0; }
  response[bytes] = '\0';
  serverDisconnect(s);

  char *p = strchr(response, ',');
  if (!p) { printf("Invalid response.\n"); return 0; }
  int status = atoi(p + 1);

  switch (status) {
    case STATUS_OK:               printf("Product '%s' added successfully.\n", productId); break;
    case STATUS_DUPLICATE_PRODUCT: printf("Product ID '%s' already exists.\n", productId); break;
    case STATUS_INVALID_ARGUMENTS: printf("Invalid input.\n"); break;
    default:                       printf("Failed to add product. Error: %d\n", status); break;
  }
  return status == STATUS_OK ? 1 : 0;
}

int doUpdateProduct(struct addrinfo *peer_address) {
  char productId[PRODUCTID_SIZE];
  char title[PRODUCT_TITLE_SIZE];
  float price;
  int qty;
  char command[512];
  char response[256];
  memset(response, 0, sizeof(response));

  printf("Product ID to update: ");
  fgets(productId, sizeof(productId), stdin);
  productId[strcspn(productId, "\r\n")] = '\0';

  printf("New Title: ");
  fgets(title, sizeof(title), stdin);
  title[strcspn(title, "\r\n")] = '\0';

  printf("New Price: ");
  scanf("%f", &price);
  printf("New Quantity: ");
  scanf("%d", &qty);
  getchar();

  snprintf(command, sizeof(command), "%s,%lu,%s,%s,%.2f,%d",
           COMMAND_UPDATE_PRODUCT, session, productId, title, price, qty);

  SOCKET s = serverConnect(peer_address);
  if (!ISVALIDSOCKET(s)) return 0;
  send(s, command, strlen(command), 0);
  int bytes = recv(s, response, sizeof(response) - 1, 0);
  if (bytes < 1) { serverDisconnect(s); return 0; }
  response[bytes] = '\0';
  serverDisconnect(s);

  char *p = strchr(response, ',');
  if (!p) { printf("Invalid response.\n"); return 0; }
  int status = atoi(p + 1);

  switch (status) {
    case STATUS_OK:                  printf("Product '%s' updated successfully.\n", productId); break;
    case STATUS_PRODUCT_NOT_FOUND:   printf("Product '%s' not found.\n", productId); break;
    case STATUS_INVALID_ARGUMENTS:   printf("Invalid input.\n"); break;
    default:                         printf("Failed to update product. Error: %d\n", status); break;
  }
  return status == STATUS_OK ? 1 : 0;
}

int doRemoveProduct(struct addrinfo *peer_address) {
  char productId[PRODUCTID_SIZE];
  char command[64];
  char response[256];
  memset(response, 0, sizeof(response));

  printf("Product ID to remove: ");
  fgets(productId, sizeof(productId), stdin);
  productId[strcspn(productId, "\r\n")] = '\0';

  snprintf(command, sizeof(command), "%s,%lu,%s", COMMAND_REMOVE_PRODUCT, session, productId);

  SOCKET s = serverConnect(peer_address);
  if (!ISVALIDSOCKET(s)) return 0;
  send(s, command, strlen(command), 0);
  int bytes = recv(s, response, sizeof(response) - 1, 0);
  if (bytes < 1) { serverDisconnect(s); return 0; }
  response[bytes] = '\0';
  serverDisconnect(s);

  char *p = strchr(response, ',');
  if (!p) { printf("Invalid response.\n"); return 0; }
  int status = atoi(p + 1);

  switch (status) {
    case STATUS_OK:                printf("Product '%s' removed.\n", productId); break;
    case STATUS_PRODUCT_NOT_FOUND: printf("Product '%s' not found.\n", productId); break;
    default:                       printf("Failed to remove product. Error: %d\n", status); break;
  }
  return status == STATUS_OK ? 1 : 0;
}

int doViewMembers(struct addrinfo *peer_address) {
  char command[64];
  char response[4096];
  memset(response, 0, sizeof(response));

  snprintf(command, sizeof(command), "%s,%lu", COMMAND_VIEW_MEMBER, session);
  SOCKET s = serverConnect(peer_address);
  if (!ISVALIDSOCKET(s)) return 0;
  send(s, command, strlen(command), 0);
  int bytes = recv(s, response, sizeof(response) - 1, 0);
  if (bytes < 1) { serverDisconnect(s); return 0; }
  response[bytes] = '\0';
  serverDisconnect(s);

  char *p = strchr(response, ',');
  if (!p) { printf("Invalid response.\n"); return 0; }
  int status = atoi(p + 1);

  if (status == STATUS_EMPTY_USER)        { printf("No members registered.\n"); return 1; }
  if (status == STATUS_PERMISSION_DENIED) { printf("Permission denied.\n"); return 0; }
  if (status != STATUS_OK)                { printf("Error: %d\n", status); return 0; }

  p = strchr(p + 1, ',');
  if (!p) { printf("Invalid response.\n"); return 0; }
  p++;

  printf("\n%-15s | %-10s\n", "Username", "Role");
  printf("---------------------------\n");

  int count = 0;
  char *item = strtok(p, "|");
  while (item != NULL) {
    char username[USERNAME_SIZE];
    char role[16];
    if (sscanf(item, "%19[^-]-%15s", username, role) == 2) {
      printf("%-15s | %-10s\n", username, role);
      count++;
    }
    item = strtok(NULL, "|");
  }
  if (count == 0) printf("No members found.\n");
  return 1;
}