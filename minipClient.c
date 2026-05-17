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
char *getMenu(unsigned long);
SOCKET serverConnect(struct addrinfo *);
int serverDisconnect(SOCKET);
int testConnect(struct addrinfo *);
int doExit(struct addrinfo *peer_address);
int doViewProducts(struct addrinfo *peer_address);
int doSearchProduct(struct addrinfo *peer_address);
int doLogin(struct addrinfo *peer_address);
int doRegister(struct addrinfo *peer_address);
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
    // Anonymous menu
    switch (clientChoice) {
    case 0: doExit(peer_address); return;
    case 1: doViewProducts(peer_address); break;
    case 2: doSearchProduct(peer_address); break;
    case 3: doLogin(peer_address); break;
    case 4: doRegister(peer_address); break;
    default: printf("Wrong menu. Please select 0-4.\n"); break;
    }
  } else {
    // Member menu
    switch (clientChoice) {
    case 0: doExit(peer_address); return;
    case 1: doViewProducts(peer_address); break;
    case 2: doSearchProduct(peer_address); break;
    case 7: session = 0; printf("Logged out.\n"); break;
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
                 "   ยินดีต้อนรับสู่ร้านค้า\n"
                 "======================\n"
                 "[1] ดูรายการสินค้าทั้งหมด\n"
                 "[2] ค้นหาสินค้า\n"
                 "[3] Login\n"
                 "[4] สมัครสมาชิก\n"
                 "[0] ออก\n"
                 "กรุณาเลือก [0-4]: ");
    return menu;
  } else {
    strcpy(menu, "\n======================\n"
                 "     เมนูผู้ใช้งาน\n"
                 "======================\n"
                 "[1] ดูรายการสินค้า\n"
                 "[2] เพิ่มสินค้าลงตะกร้า\n"
                 "[3] ดูตะกร้าสินค้า\n"
                 "[4] ลบสินค้าออกจากตะกร้า\n"
                 "[5] สั่งซื้อสินค้า\n"
                 "[6] ดูประวัติการสั่งซื้อ\n"
                 "[7] ออกจากระบบ\n"
                 "[0] ออก\n"
                 "กรุณาเลือก [0-7]: ");
    return menu;
  }
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
    if (p)
      session = strtoul(p + 1, NULL, 10);
    printf("Login successful! Welcome, %s.\n", username);
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