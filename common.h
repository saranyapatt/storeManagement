#ifndef COMMON_H
#define COMMON_H
#include <stdbool.h>

#define COMMAND_VIEW_PRODUCT "VIEW_PRODUCT" // ANONYMOUS(2.1.1), MEMBER(2.2.1), ADMIN(2.3.1)
#define COMMAND_SEARCH_PRODUCT "SEARCH_PRODUCT" // ANONYMOUS(2.1.2), MEMBER(2.2.1), ADMIN(2.3.1)
#define COMMAND_UPDATE_PRODUCT "UPDATE_PRODUCT" // ADMIN(2.3.1)
#define COMMAND_ADD_PRODUCT "ADD_PRODUCT" // ADMIN(2.3.1)
#define COMMAND_REMOVE_PRODUCT "REMOVE_PRODUCT" // ADMIN(2.3.1)
#define COMMAND_UPDATE_CART "UPDATE_CART" // MEMBER(2.2.2,2.2.4)
#define COMMAND_CLEAR_CART "CLEAR_CART" // MEMBER(2.2.4) - ล้ํางตะกร้ํา
#define COMMAND_VIEW_CART "VIEW_CART" // MEMBER(2.2.3)
#define COMMAND_CHECKOUT_CART "CHECKOUT_CART" // MEMBER(2.2.5)
#define COMMAND_VIEW_ORDER "VIEW_ORDER" // MEMBER(2.2.6), ADMIN (2.3.2)
#define COMMAND_VIEW_MEMBER "VIEW_MEMBER" // ADMIN (2.3.3)
#define COMMAND_REGISTER_MEMBER "REGISTER_MEMBER" // ANONYMOUS (2.1.4)
#define COMMAND_LOGOUT "LOGOUT" // MEMBER(2.2.7), ADMIN (2.3.7)
#define COMMAND_LOGIN "LOGIN" // ANONYMOUS(2.1.3)
#define COMMAND_SEPARATOR ","
#define COMMAND_UNKNOWN "UNKNOWN"

#define ISVALIDSOCKET(s) ((s) >= 0)
#define CLOSESOCKET(s) close(s)
#define SOCKET int
#define GETSOCKETERRNO() (errno)

typedef struct UserSessions UserSessions;
typedef struct Store Store;
typedef struct Order Order;
typedef struct Cart Cart;
typedef struct User User;
typedef struct Stock Stock;
typedef struct CommandContext CommandContext;

struct CommandContext {
    UserSessions *sessions;
    Store *store;
    Order *order;
    SOCKET socket_client;
    char *rawInput;
};

#define COMMAND_VIEW_PRODUCT    "VIEW_PRODUCT"
#define COMMAND_SEARCH_PRODUCT  "SEARCH_PRODUCT"
#define COMMAND_UPDATE_PRODUCT  "UPDATE_PRODUCT"
#define COMMAND_ADD_PRODUCT     "ADD_PRODUCT"
#define COMMAND_REMOVE_PRODUCT  "REMOVE_PRODUCT"
#define COMMAND_UPDATE_CART     "UPDATE_CART"
#define COMMAND_CLEAR_CART      "CLEAR_CART"
#define COMMAND_VIEW_CART       "VIEW_CART"
#define COMMAND_CHECKOUT_CART   "CHECKOUT_CART"
#define COMMAND_VIEW_ORDER      "VIEW_ORDER"
#define COMMAND_VIEW_MEMBER     "VIEW_MEMBER"
#define COMMAND_REGISTER_MEMBER "REGISTER_MEMBER"
#define COMMAND_LOGOUT          "LOGOUT"
#define COMMAND_LOGIN           "LOGIN"
#define COMMAND_SEPARATOR       ","
#define COMMAND_UNKNOWN         "UNKNOWN"

/// stock 
#define PRODUCTID_SIZE 10
#define PRODUCT_TITLE_SIZE 128
#define MAX_STOCK 50

/// store user
#define USERNAME_SIZE 20
#define PASSWORD_SIZE 128
#define MAX_CART 10
#define MAX_USERS 512


#define ISVALIDSOCKET(s) ((s) >= 0)
#define CLOSESOCKET(s) close(s)
#define GETSOCKETERRNO() (errno)

typedef enum {
    STATUS_OK = 0,
    STATUS_FAIL = 1,
    STATUS_UNKNOWN = 3,
    STATUS_INVALID_SESSION = 100,
    STATUS_PERMISSION_DENIED = 101,
    STATUS_DUPLICATE_USER = 102,
    STATUS_INVALID_ARGUMENTS = 200,
    STATUS_PRODUCT_NOT_FOUND = 201,
    STATUS_DUPLICATE_PRODUCT = 202,
    STATUS_EMPTY_PRODUCT = 203,
    STATUS_EMPTY_CART = 204,
    STATUS_EMPTY_USER = 205,
    STATUS_SAVE_STORE_FAIL = 500,
    STATUS_ADD_PRODUCT_FAIL = 501,
    STATUS_UPDATE_PRODUCT_FAIL = 502,
    STATUS_DELETE_PRODUCT_FAIL = 503,
    STATUS_UPDATE_CART_FAIL = 504,
    STATUS_SAVE_ORDER_FAIL = 505,
    STATUS_UPDATE_STORE_FAIL = 506,
    STATUS_ITEM_NOT_IN_CART = 507,
    STATUS_CLEAR_CART_FAIL = 508,
} ResponseStatus;

///stock
#define PRODUCTID_SIZE 10
#define PRODUCT_TITLE_SIZE 128
#define MAX_STOCK 50
struct Stock {
    char productId[PRODUCTID_SIZE];
    char productTitle[PRODUCT_TITLE_SIZE];
    int quantity;
    float price;
    int isUsed;
};
struct Store {
    Stock items[MAX_STOCK];
};


///order
struct Cart {
    char username[USERNAME_SIZE];
    char productId[PRODUCTID_SIZE];
    int quantity;
    int checkout;
    bool isUsed;
};
struct Order {
    Cart carts[MAX_CART];
};

struct User {
    char username[USERNAME_SIZE];
    char password_hash[PASSWORD_SIZE];
    bool isAdmin;
    unsigned long session_id;
    bool isActive;
};

struct UserSessions {
    User users[MAX_USERS];
};

#endif