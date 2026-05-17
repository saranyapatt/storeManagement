#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "common.h"
extern char *shared_output;
// Function Prototypes
int updateStore(Store *store, char *productId, char *productTitle, float price, int quantity);
int updateProduct(Store *store, char *productId, char *productTitle, float price, int quantity);
int deleteProduct(Store *store, char *productId);
int addProduct(Store *store, char *productId, char *productTitle, float price, int quantity);
int printStore(Store *store);
int saveStore(Store *store, const char *fileName);
int loadStore(Store *store, const char *fileName);
void getStore(Store *store, char *output, size_t outputSize);
void searchStore(Store *store, char *productId, char *output, size_t outputSize);
/// helper
int findProductIndex(Store *store, char *productId);

// --- Helper: Find product index by ID ---
int findProductIndex(Store *store, char *productId) {
    for (int i = 0; i < MAX_STOCK; i++) {
        if (store->items[i].isUsed && strcmp(store->items[i].productId, productId) == 0) {
            return i;
        }
    }
    return -1;
}

// 1. Add Product
int addProduct(Store *store, char *productId, char *productTitle, float price, int quantity) {
    // Check if ID already exists
    if (findProductIndex(store, productId) != -1) return 0; 

    for (int i = 0; i < MAX_STOCK; i++) {
        if (!store->items[i].isUsed) {
            strncpy(store->items[i].productId, productId, PRODUCTID_SIZE - 1);
            store->items[i].productId[PRODUCTID_SIZE - 1] = '\0';
            
            strncpy(store->items[i].productTitle, productTitle, PRODUCT_TITLE_SIZE - 1);
            store->items[i].productTitle[PRODUCT_TITLE_SIZE - 1] = '\0';
            
            store->items[i].price = price;
            store->items[i].quantity = quantity;
            store->items[i].isUsed = 1;
            return 1;
        }
    }
    return 0; // Store full
}

// 2. Update Product
int updateProduct(Store *store, char *productId, char *productTitle, float price, int quantity) {
    int idx = findProductIndex(store, productId);
    if (idx == -1) return 0;

    strncpy(store->items[idx].productTitle, productTitle, PRODUCT_TITLE_SIZE - 1);
    store->items[idx].productTitle[PRODUCT_TITLE_SIZE - 1] = '\0';
    store->items[idx].price = price;
    store->items[idx].quantity = quantity;
    return 1;
}

// 3. Update Store Wrapper
int updateStore(Store *store, char *productId, char *productTitle, float price, int quantity) {
    return updateProduct(store, productId, productTitle, price, quantity);
}

// 4. Delete Product
int deleteProduct(Store *store, char *productId) {
    int idx = findProductIndex(store, productId);
    if (idx == -1) return 0;

    store->items[idx].isUsed = 0;
    return 1;
}

// 5. Print Store to Console
int printStore(Store *store) {
    if (store == NULL || shared_output == NULL) return 0;

    char line[512];
    shared_output[0] = '\0';

    sprintf(line, "\n%-12s | %-25s | %-8s | %-5s\n", "ID", "Title", "Price", "Qty");
    strcat(shared_output, line);
    strcat(shared_output, "------------------------------------------------------------\n");

    int count = 0;
    for (int i = 0; i < MAX_STOCK; i++) {
        if (store->items[i].isUsed) {
            sprintf(line, "%-12s | %-25s | %8.2f | %5d\n",
                   store->items[i].productId, 
                   store->items[i].productTitle,
                   store->items[i].price, 
                   store->items[i].quantity);
            
            strcat(shared_output, line);
            count++;
        }
    }

    if (count == 0) {
        strcat(shared_output, "Store is currently empty.\n");
    }
    return 1;
}
// OK
// 7. Load Store from Text File (CSV Format)
int loadStore(Store *store, const char *fileName) {
    FILE *file = fopen(fileName, "r");
    if (!file) return 0;

    memset(store, 0, sizeof(Store));
    
    char line[1024];
    int count = 0;
    while (fgets(line, sizeof(line), file) && count < MAX_STOCK) {

        if (sscanf(line, "%[^,],%[^,],%f,%d", 
                   store->items[count].productId, 
                   store->items[count].productTitle, 
                   &store->items[count].price, 
                   &store->items[count].quantity)) {
            
            store->items[count].isUsed = 1;
            count++;
        }
    }
    fclose(file);
    return 1;
}

// OK
// 8. Get Store (Serializes inventory into a string buffer)
void getStore(Store *store, char *output, size_t outputSize) {
    char line[1024];
    snprintf(output + strlen(output), outputSize - strlen(output), ",%d,", STATUS_OK);
    for (int i = 0; i < MAX_STOCK; i++) {
        if (store->items[i].isUsed) {
            snprintf(line, sizeof(line), "%s-%s-%.2f-%d|",
                     store->items[i].productId, store->items[i].productTitle,
                     store->items[i].price, store->items[i].quantity);
            if (strlen(output) + strlen(line) < outputSize - 1) {
                strncat(output, line, outputSize - strlen(output) - 1);
            }
        }
    }
    printf("%s\n", output);
}

// 9. Search Store
void searchStore(Store *store, char *productId, char *output, size_t outputSize) {
    if (store == NULL || output == NULL) return;
    int idx = findProductIndex(store, productId);
    if (idx != -1) {
        char line[256];
        snprintf(line, sizeof(line), ",%d,%s-%s-%.2f-%d|",
                 STATUS_OK,
                 store->items[idx].productId,
                 store->items[idx].productTitle,
                 store->items[idx].price,
                 store->items[idx].quantity);
        strncat(output, line, outputSize - strlen(output) - 1);
    } else {
        char line[64];
        snprintf(line, sizeof(line), "%d,", STATUS_PRODUCT_NOT_FOUND);
        strncat(output, line, outputSize - strlen(output) - 1);
    }
    printf("%s\n", output);
}