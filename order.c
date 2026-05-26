#include <stdio.h>
#include <string.h>
#include "common.h"

void getOrder(Order *head, char *username, char* output, size_t outputSize,int checkout);
int updateCart(Order *head, char* username, char* productId, int quantity);
int clearCart(Order *orderHead,Store *storeHead, char* username);
int checkoutCart(Order *head, char* username, char* productId);
int addCart(Order *head, char* username, char* productId, int quantity);
int deleteCart(Order *head, char* username, char* productId);
int saveOrder(Order *head, char *fileName);
int loadOrder(Order *head, char *fileName);
int printOrder(Order *head);

int loadOrder(Order *head, char *fileName) {
    if (!head) return 0;

    FILE *file = fopen(fileName, "r");
    if (!file) return 0;

    memset(head, 0, sizeof(Order));

    char line[256];
    int count = 0;
    while (fgets(line, sizeof(line), file) && count < MAX_CART) {
        char username[USERNAME_SIZE];
        char productId[PRODUCTID_SIZE];
        int quantity, checkout;

        line[strcspn(line, "\r\n")] = '\0';
        if (strlen(line) < 3) continue;

        if (sscanf(line, "%19[^,],%9[^,],%d,%d",
                   username, productId, &quantity, &checkout) == 4) {
            strncpy(head->carts[count].username,  username,  USERNAME_SIZE  - 1);
            strncpy(head->carts[count].productId, productId, PRODUCTID_SIZE - 1);
            head->carts[count].quantity = quantity;
            head->carts[count].checkout = checkout;
            head->carts[count].isUsed   = true;
            count++;
        }
    }

    fclose(file);
    return count;
}

int addCart(Order *head, char *username, char *productId, int quantity) {
    if (!head || !username || !productId || quantity <= 0) return STATUS_INVALID_ARGUMENTS;

    int userPending = 0;
    for (int i = 0; i < MAX_CART; i++) {
        if (head->carts[i].isUsed && head->carts[i].checkout == 0 &&
            strcmp(head->carts[i].username, username) == 0) {
            if (strcmp(head->carts[i].productId, productId) == 0) {
                head->carts[i].quantity += quantity;
                return STATUS_OK;
            }
            userPending++;
        }
    }
    if (userPending >= MAX_CART_PER_USER) return STATUS_UPDATE_CART_FAIL;

    for (int i = 0; i < MAX_CART; i++) {
        if (!head->carts[i].isUsed) {
            strncpy(head->carts[i].username,  username,  USERNAME_SIZE  - 1);
            strncpy(head->carts[i].productId, productId, PRODUCTID_SIZE - 1);
            head->carts[i].quantity = quantity;
            head->carts[i].checkout = 0;
            head->carts[i].isUsed   = true;
            return STATUS_OK;
        }
    }
    return STATUS_FAIL;
}

int clearCart(Order *orderHead, Store *storeHead, char *username) {
    if (!orderHead || !username) return STATUS_INVALID_ARGUMENTS;

    int found = 0;
    for (int i = 0; i < MAX_CART; i++) {
        if (orderHead->carts[i].isUsed &&
            orderHead->carts[i].checkout == 0 &&
            strcmp(orderHead->carts[i].username, username) == 0) {
            memset(&orderHead->carts[i], 0, sizeof(Cart));
            found = 1;
        }
    }
    return found ? STATUS_OK : STATUS_EMPTY_CART;
}

// item format: productId-quantity| (member) or username/productId-quantity| (admin, username==NULL)
void getOrder(Order *head, char *username, char *output, size_t outputSize, int checkout) {
    if (!head || !output) return;

    int all = (username == NULL);
    int count = 0;
    char line[128];

    for (int i = 0; i < MAX_CART; i++) {
        if (head->carts[i].isUsed &&
            (all || strcmp(head->carts[i].username, username) == 0) &&
            (checkout == -1 || head->carts[i].checkout == checkout)) {
            count++;
        }
    }

    if (count == 0) {
        snprintf(line, sizeof(line), ",%d,", STATUS_EMPTY_CART);
        strncat(output, line, outputSize - strlen(output) - 1);
        return;
    }

    snprintf(line, sizeof(line), ",%d,", STATUS_OK);
    strncat(output, line, outputSize - strlen(output) - 1);

    for (int i = 0; i < MAX_CART; i++) {
        if (head->carts[i].isUsed &&
            (all || strcmp(head->carts[i].username, username) == 0) &&
            (checkout == -1 || head->carts[i].checkout == checkout)) {
            if (all)
                snprintf(line, sizeof(line), "%s/%s-%d|",
                         head->carts[i].username,
                         head->carts[i].productId,
                         head->carts[i].quantity);
            else
                snprintf(line, sizeof(line), "%s-%d|",
                         head->carts[i].productId,
                         head->carts[i].quantity);
            if (strlen(output) + strlen(line) < outputSize - 1)
                strncat(output, line, outputSize - strlen(output) - 1);
        }
    }
}

int updateCart(Order *head, char *username, char *productId, int quantity) {
    if (!head || !username || !productId || quantity <= 0) return STATUS_INVALID_ARGUMENTS;
    for (int i = 0; i < MAX_CART; i++) {
        if (head->carts[i].isUsed && head->carts[i].checkout == 0 &&
            strcmp(head->carts[i].username,  username)  == 0 &&
            strcmp(head->carts[i].productId, productId) == 0) {
            head->carts[i].quantity = quantity;
            return STATUS_OK;
        }
    }
    return STATUS_ITEM_NOT_IN_CART;
}

int deleteCart(Order *head, char *username, char *productId) {
    if (!head || !username || !productId) return STATUS_INVALID_ARGUMENTS;
    for (int i = 0; i < MAX_CART; i++) {
        if (head->carts[i].isUsed && head->carts[i].checkout == 0 &&
            strcmp(head->carts[i].username,  username)  == 0 &&
            strcmp(head->carts[i].productId, productId) == 0) {
            memset(&head->carts[i], 0, sizeof(Cart));
            return STATUS_OK;
        }
    }
    return STATUS_ITEM_NOT_IN_CART;
}

int saveOrder(Order *head, char *fileName) {
    if (!head || !fileName) return STATUS_INVALID_ARGUMENTS;
    FILE *file = fopen(fileName, "w");
    if (!file) return STATUS_SAVE_ORDER_FAIL;
    for (int i = 0; i < MAX_CART; i++) {
        if (head->carts[i].isUsed) {
            fprintf(file, "%s,%s,%d,%d\n",
                    head->carts[i].username,
                    head->carts[i].productId,
                    head->carts[i].quantity,
                    head->carts[i].checkout);
        }
    }
    fclose(file);
    return STATUS_OK;
}

int printOrder(Order *head) {
    if (!head) return 0;
    printf("\n%-20s | %-12s | %-5s | %-8s\n", "Username", "Product ID", "Qty", "Status");
    printf("------------------------------------------------------------\n");
    int count = 0;
    for (int i = 0; i < MAX_CART; i++) {
        if (head->carts[i].isUsed) {
            printf("%-20s | %-12s | %5d | %-8s\n",
                   head->carts[i].username,
                   head->carts[i].productId,
                   head->carts[i].quantity,
                   head->carts[i].checkout ? "checkout" : "pending");
            count++;
        }
    }
    if (count == 0) printf("No orders.\n");
    return count;
}

int checkoutCart(Order *head, char *username, char *productId) {
    if (!head || !username) return STATUS_INVALID_ARGUMENTS;

    int found = 0;
    for (int i = 0; i < MAX_CART; i++) {
        if (!head->carts[i].isUsed) continue;
        if (strcmp(head->carts[i].username, username) != 0) continue;
        if (head->carts[i].checkout == 1) continue;

        if (productId && strlen(productId) > 0 &&
            strcmp(head->carts[i].productId, productId) != 0) continue;

        head->carts[i].checkout = 1;
        found = 1;
    }

    return found ? STATUS_OK : STATUS_EMPTY_CART;
}
