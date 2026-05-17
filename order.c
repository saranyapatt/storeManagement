#include <stdio.h>
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