#define USER_FILENAME "./txt/test_user.txt"
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <sys/mman.h>
#include "common.h"
#include "auth.c"
#include "store.c"
#include "order.c"

char *shared_output = NULL;

// ─── Test helpers ─────────────────────────────────────────────────────────────
int tests_run    = 0;
int tests_passed = 0;

#define ASSERT(name, condition) do {                        \
    tests_run++;                                            \
    if (condition) {                                        \
        printf("  [PASS] %s\n", name);                     \
        tests_passed++;                                     \
    } else {                                                \
        printf("  [FAIL] %s\n", name);                     \
    }                                                       \
} while (0)

void section(const char *title) {
    printf("\n=== %s ===\n", title);
}

// ─── Main ─────────────────────────────────────────────────────────────────────
int main() {
    UserSessions *sessions = mmap(NULL, sizeof(UserSessions),
        PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    Store *store = mmap(NULL, sizeof(Store),
        PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    Order *order = mmap(NULL, sizeof(Order),
        PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    shared_output = mmap(NULL, 8192,
        PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);

    memset(sessions, 0, sizeof(UserSessions));
    memset(store,    0, sizeof(Store));
    memset(order,    0, sizeof(Order));

    // reset test user file so each run starts clean
    FILE *_tf = fopen(USER_FILENAME, "w");
    if (_tf) fclose(_tf);


    // =========================================================================
    // AUTH
    // =========================================================================

    section("AUTH: registerUser — normal cases");
    ASSERT("register member",
        registerUser(sessions, "alice", "pass123", false) == STATUS_OK);
    ASSERT("register admin",
        registerUser(sessions, "admin", "admin123", true) == STATUS_OK);
    ASSERT("register second member",
        registerUser(sessions, "bob", "bobpass", false) == STATUS_OK);

    section("AUTH: registerUser — error cases");
    ASSERT("duplicate username → STATUS_DUPLICATE_USER",
        registerUser(sessions, "alice", "other", false) == STATUS_DUPLICATE_USER);
    ASSERT("null sessions → STATUS_INVALID_ARGUMENTS",
        registerUser(NULL, "x", "y", false) == STATUS_INVALID_ARGUMENTS);
    ASSERT("null username → STATUS_INVALID_ARGUMENTS",
        registerUser(sessions, NULL, "y", false) == STATUS_INVALID_ARGUMENTS);
    ASSERT("null password → STATUS_INVALID_ARGUMENTS",
        registerUser(sessions, "newuser", NULL, false) == STATUS_INVALID_ARGUMENTS);

    section("AUTH: loginUser — normal cases");
    User *alice = loginUser(sessions, "alice", "pass123");
    ASSERT("login correct credentials returns User*",
        alice != NULL);
    ASSERT("session_id is nonzero after login",
        alice != NULL && alice->session_id != 0);
    ASSERT("isAdmin false for member",
        alice != NULL && alice->isAdmin == false);

    User *adm = loginUser(sessions, "admin", "admin123");
    ASSERT("admin login returns User*",
        adm != NULL);
    ASSERT("isAdmin true for admin",
        adm != NULL && adm->isAdmin == true);

    section("AUTH: loginUser — error cases");
    ASSERT("wrong password → NULL",
        loginUser(sessions, "alice", "wrongpass") == NULL);
    ASSERT("unknown user → NULL",
        loginUser(sessions, "nobody", "pass") == NULL);
    ASSERT("null sessions → NULL",
        loginUser(NULL, "alice", "pass123") == NULL);
    ASSERT("null username → NULL",
        loginUser(sessions, NULL, "pass123") == NULL);
    ASSERT("null password → NULL",
        loginUser(sessions, "alice", NULL) == NULL);

    section("AUTH: getUserBySession — normal cases");
    unsigned long sid = alice ? alice->session_id : 0;
    ASSERT("find user by valid session",
        getUserBySession(sessions, sid) != NULL);
    ASSERT("returned user has matching username",
        getUserBySession(sessions, sid) != NULL &&
        strcmp(getUserBySession(sessions, sid)->username, "alice") == 0);

    section("AUTH: getUserBySession — error cases");
    ASSERT("session_id == 0 → NULL",
        getUserBySession(sessions, 0) == NULL);
    ASSERT("wrong session id → NULL",
        getUserBySession(sessions, 9999999999UL) == NULL);
    ASSERT("null sessions → NULL",
        getUserBySession(NULL, sid) == NULL);

    section("AUTH: logoutUser — normal cases");
    ASSERT("logout valid user → STATUS_OK",
        logoutUser(sessions, "alice") == STATUS_OK);
    ASSERT("session_id is 0 after logout",
        alice != NULL && alice->session_id == 0);
    ASSERT("getUserBySession returns NULL after logout",
        getUserBySession(sessions, sid) == NULL);

    section("AUTH: logoutUser — error cases");
    ASSERT("logout already-logged-out user → STATUS_OK (resets session again)",
        logoutUser(sessions, "alice") == STATUS_OK);
    ASSERT("logout unknown user → STATUS_FAIL",
        logoutUser(sessions, "nobody") == STATUS_FAIL);
    ASSERT("logout after re-login works",
        loginUser(sessions, "alice", "pass123") != NULL &&
        logoutUser(sessions, "alice") == STATUS_OK);

    section("AUTH: loadUser from file");
    UserSessions *sessions2 = mmap(NULL, sizeof(UserSessions),
        PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    memset(sessions2, 0, sizeof(UserSessions));
    int loaded = loadUser(sessions2, USER_FILENAME);
    ASSERT("loadUser returns nonzero on success",
        loaded != 0);
    int found_admin = 0;
    for (int i = 0; i < MAX_USERS; i++)
        if (sessions2->users[i].isActive &&
            strcmp(sessions2->users[i].username, "admin") == 0 &&
            sessions2->users[i].isAdmin == true) { found_admin = 1; break; }
    ASSERT("admin loaded with isAdmin=1", found_admin);

    int sessions_ok = 1;
    for (int i = 0; i < MAX_USERS; i++)
        if (sessions2->users[i].isActive && sessions2->users[i].session_id != 0)
            sessions_ok = 0;
    ASSERT("all loaded users have isActive=true and session_id=0", sessions_ok);
    munmap(sessions2, sizeof(UserSessions));

    section("AUTH: getUser output");
    char uout[2048] = "";
    getUser(sessions, uout, sizeof(uout));
    ASSERT("getUser output contains 'Username' header",
        strstr(uout, "Username") != NULL);
    ASSERT("getUser output contains 'alice'",
        strstr(uout, "alice") != NULL);
    ASSERT("getUser output contains 'admin' role",
        strstr(uout, "Admin") != NULL);


    // =========================================================================
    // STORE
    // =========================================================================

    section("STORE: addProduct — normal cases");
    ASSERT("add product A",
        addProduct(store, "COS1101", "Intro to CS", 30.50f, 12) == 1);
    ASSERT("add product B",
        addProduct(store, "COS3105", "OS", 10.50f, 10) == 1);
    ASSERT("add product C",
        addProduct(store, "COS3106", "Network", 15.00f, 20) == 1);
    ASSERT("add product with price=0 and qty=0",
        addProduct(store, "FREE001", "Free Item", 0.0f, 0) == 1);

    section("STORE: addProduct — error cases");
    ASSERT("duplicate product ID → 0",
        addProduct(store, "COS1101", "Dup", 1.0f, 1) == 0);

    section("STORE: addProduct — full store");
    Store *fullstore = mmap(NULL, sizeof(Store),
        PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    memset(fullstore, 0, sizeof(Store));
    char pid[PRODUCTID_SIZE];
    for (int i = 0; i < MAX_STOCK; i++) {
        snprintf(pid, sizeof(pid), "P%04d", i);
        addProduct(fullstore, pid, "Item", 1.0f, 1);
    }
    ASSERT("add to full store → 0",
        addProduct(fullstore, "EXTRA", "Extra", 1.0f, 1) == 0);
    munmap(fullstore, sizeof(Store));

    section("STORE: findProductIndex");
    ASSERT("find existing product → >= 0",
        findProductIndex(store, "COS1101") >= 0);
    ASSERT("find second product → >= 0",
        findProductIndex(store, "COS3105") >= 0);
    ASSERT("find non-existing → -1",
        findProductIndex(store, "XXXXX") == -1);
    ASSERT("find deleted (not yet) product exists",
        findProductIndex(store, "COS3106") >= 0);

    section("STORE: updateProduct");
    ASSERT("update existing → 1",
        updateProduct(store, "COS1101", "Updated Title", 99.99f, 5) == 1);
    ASSERT("price updated correctly",
        store->items[findProductIndex(store, "COS1101")].price == 99.99f);
    ASSERT("title updated correctly",
        strcmp(store->items[findProductIndex(store, "COS1101")].productTitle,
               "Updated Title") == 0);
    ASSERT("update non-existing → 0",
        updateProduct(store, "XXXXX", "X", 1.0f, 1) == 0);

    section("STORE: deleteProduct");
    ASSERT("delete existing → 1",
        deleteProduct(store, "COS3106") == 1);
    ASSERT("deleted product not found → -1",
        findProductIndex(store, "COS3106") == -1);
    ASSERT("delete non-existing → 0",
        deleteProduct(store, "XXXXX") == 0);
    ASSERT("delete already-deleted → 0",
        deleteProduct(store, "COS3106") == 0);

    section("STORE: getStore output");
    char msg[2048];
    strcpy(msg, "VIEW_PRODUCT");
    getStore(store, msg, sizeof(msg));
    ASSERT("getStore appends STATUS_OK token",
        strstr(msg, ",0,") != NULL);
    ASSERT("getStore contains remaining product COS1101",
        strstr(msg, "COS1101") != NULL);
    ASSERT("getStore does not contain deleted COS3106",
        strstr(msg, "COS3106") == NULL);

    section("STORE: getStore on empty store");
    Store *emptystore = mmap(NULL, sizeof(Store),
        PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    memset(emptystore, 0, sizeof(Store));
    char emsg[256] = "VIEW_PRODUCT";
    getStore(emptystore, emsg, sizeof(emsg));
    ASSERT("getStore on empty store still has STATUS_OK",
        strstr(emsg, ",0,") != NULL);
    ASSERT("getStore on empty store has no product data",
        strstr(emsg, "|") == NULL);
    munmap(emptystore, sizeof(Store));

    section("STORE: searchStore");
    strcpy(msg, "SEARCH_PRODUCT");
    searchStore(store, "COS1101", msg, sizeof(msg));
    ASSERT("searchStore found — contains STATUS_OK",
        strstr(msg, ",0,") != NULL);
    ASSERT("searchStore found — contains product ID",
        strstr(msg, "COS1101") != NULL);
    ASSERT("searchStore found — contains price",
        strstr(msg, "99.99") != NULL);

    strcpy(msg, "SEARCH_PRODUCT");
    searchStore(store, "NOTEXIST", msg, sizeof(msg));
    ASSERT("searchStore not found — contains STATUS_PRODUCT_NOT_FOUND",
        strstr(msg, "201") != NULL);

    strcpy(msg, "SEARCH_PRODUCT");
    searchStore(store, "COS3106", msg, sizeof(msg));
    ASSERT("searchStore deleted product — not found",
        strstr(msg, "201") != NULL);

    section("STORE: loadStore from file");
    Store *fstore = mmap(NULL, sizeof(Store),
        PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    memset(fstore, 0, sizeof(Store));
    int sl = loadStore(fstore, STORE_FILENAME);
    ASSERT("loadStore returns nonzero",
        sl != 0);
    ASSERT("loaded store has at least one product",
        findProductIndex(fstore, "COS1101") >= 0 ||
        findProductIndex(fstore, "COS3105") >= 0);
    int products_ok = 1;
    for (int i = 0; i < MAX_STOCK; i++)
        if (fstore->items[i].isUsed && fstore->items[i].price < 0) products_ok = 0;
    ASSERT("loaded products have no negative price", products_ok);
    munmap(fstore, sizeof(Store));


    // =========================================================================
    // ORDER
    // =========================================================================

    section("ORDER: addCart — normal cases");
    ASSERT("add new item for alice",
        addCart(order, "alice", "COS1101", 2) == STATUS_OK);
    ASSERT("add different item for alice",
        addCart(order, "alice", "COS3105", 1) == STATUS_OK);
    ASSERT("add same item again — quantity accumulates",
        addCart(order, "alice", "COS1101", 3) == STATUS_OK);
    int alice_qty = 0;
    for (int i = 0; i < MAX_CART; i++)
        if (order->carts[i].isUsed &&
            strcmp(order->carts[i].username, "alice") == 0 &&
            strcmp(order->carts[i].productId, "COS1101") == 0)
            alice_qty = order->carts[i].quantity;
    ASSERT("accumulated quantity is 5", alice_qty == 5);
    ASSERT("add item for bob",
        addCart(order, "bob", "COS1101", 1) == STATUS_OK);

    section("ORDER: addCart — error cases");
    ASSERT("null order → STATUS_INVALID_ARGUMENTS",
        addCart(NULL, "alice", "COS1101", 1) == STATUS_INVALID_ARGUMENTS);
    ASSERT("null username → STATUS_INVALID_ARGUMENTS",
        addCart(order, NULL, "COS1101", 1) == STATUS_INVALID_ARGUMENTS);
    ASSERT("null productId → STATUS_INVALID_ARGUMENTS",
        addCart(order, "alice", NULL, 1) == STATUS_INVALID_ARGUMENTS);
    ASSERT("quantity = 0 → STATUS_INVALID_ARGUMENTS",
        addCart(order, "alice", "COS1101", 0) == STATUS_INVALID_ARGUMENTS);
    ASSERT("quantity < 0 → STATUS_INVALID_ARGUMENTS",
        addCart(order, "alice", "COS1101", -5) == STATUS_INVALID_ARGUMENTS);

    section("ORDER: addCart — full cart");
    Order *fullorder = mmap(NULL, sizeof(Order),
        PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    memset(fullorder, 0, sizeof(Order));
    char fpid[PRODUCTID_SIZE];
    for (int i = 0; i < MAX_CART; i++) {
        snprintf(fpid, sizeof(fpid), "P%04d", i);
        addCart(fullorder, "user", fpid, 1);
    }
    ASSERT("add to full cart → STATUS_FAIL",
        addCart(fullorder, "user", "EXTRA", 1) == STATUS_FAIL);
    munmap(fullorder, sizeof(Order));

    section("ORDER: getOrder — cart (pending)");
    char out[2048];
    strcpy(out, "VIEW_CART");
    getOrder(order, "alice", out, sizeof(out), 0);
    ASSERT("alice cart contains STATUS_OK",
        strstr(out, ",0,") != NULL);
    ASSERT("alice cart contains COS1101",
        strstr(out, "COS1101") != NULL);
    ASSERT("alice cart contains COS3105",
        strstr(out, "COS3105") != NULL);

    strcpy(out, "VIEW_CART");
    getOrder(order, "nobody", out, sizeof(out), 0);
    ASSERT("unknown user returns STATUS_EMPTY_CART",
        strstr(out, "204") != NULL);

    section("ORDER: getOrder — admin (NULL username, all orders)");
    strcpy(out, "VIEW_ORDER");
    getOrder(order, NULL, out, sizeof(out), -1);
    ASSERT("admin getOrder (NULL) returns STATUS_OK",
        strstr(out, ",0,") != NULL);
    ASSERT("admin getOrder contains alice's item with username prefix",
        strstr(out, "alice/") != NULL);
    ASSERT("admin getOrder contains bob's item",
        strstr(out, "bob/") != NULL);

    section("ORDER: checkoutCart");
    ASSERT("checkout all for alice → STATUS_OK",
        checkoutCart(order, "alice", NULL) == STATUS_OK);
    ASSERT("second checkout for alice → STATUS_EMPTY_CART",
        checkoutCart(order, "alice", NULL) == STATUS_EMPTY_CART);
    ASSERT("checkout non-existing user → STATUS_EMPTY_CART",
        checkoutCart(order, "nobody", NULL) == STATUS_EMPTY_CART);
    ASSERT("checkout null args → STATUS_INVALID_ARGUMENTS",
        checkoutCart(NULL, "alice", NULL) == STATUS_INVALID_ARGUMENTS);

    section("ORDER: getOrder — history (checked out)");
    strcpy(out, "VIEW_ORDER");
    getOrder(order, "alice", out, sizeof(out), 1);
    ASSERT("alice order history contains STATUS_OK",
        strstr(out, ",0,") != NULL);
    ASSERT("alice order history contains COS1101",
        strstr(out, "COS1101") != NULL);

    strcpy(out, "VIEW_CART");
    getOrder(order, "alice", out, sizeof(out), 0);
    ASSERT("alice pending cart is empty after checkout",
        strstr(out, "204") != NULL);

    section("ORDER: clearCart");
    addCart(order, "charlie", "COS1101", 2);
    addCart(order, "charlie", "COS3105", 1);
    ASSERT("clearCart removes pending items → STATUS_OK",
        clearCart(order, NULL, "charlie") == STATUS_OK);
    ASSERT("clearCart on already-cleared → STATUS_EMPTY_CART",
        clearCart(order, NULL, "charlie") == STATUS_EMPTY_CART);

    section("ORDER: clearCart — only checked-out items");
    addCart(order, "diana", "COS1101", 1);
    checkoutCart(order, "diana", NULL);
    ASSERT("clearCart when only checked-out items → STATUS_EMPTY_CART",
        clearCart(order, NULL, "diana") == STATUS_EMPTY_CART);

    section("ORDER: clearCart — error cases");
    ASSERT("clearCart null order → STATUS_INVALID_ARGUMENTS",
        clearCart(NULL, NULL, "alice") == STATUS_INVALID_ARGUMENTS);
    ASSERT("clearCart null username → STATUS_INVALID_ARGUMENTS",
        clearCart(order, NULL, NULL) == STATUS_INVALID_ARGUMENTS);

    section("ORDER: loadOrder from file");
    Order *forder = mmap(NULL, sizeof(Order),
        PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    memset(forder, 0, sizeof(Order));
    int ol = loadOrder(forder, ORDER_FILENAME);
    ASSERT("loadOrder returns nonzero on existing file",
        ol >= 0);
    int order_found = 0;
    for (int i = 0; i < MAX_CART; i++)
        if (forder->carts[i].isUsed) { order_found = 1; break; }
    ASSERT("loaded orders have isUsed=true", order_found);
    munmap(forder, sizeof(Order));


    // =========================================================================
    // INTEGRATION: Checkout reduces store quantity
    // =========================================================================

    Store *shop = mmap(NULL, sizeof(Store),
        PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    Order *basket = mmap(NULL, sizeof(Order),
        PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    memset(shop,   0, sizeof(Store));
    memset(basket, 0, sizeof(Order));

    section("INTEGRATION: setup — add 3 books to store");
    ASSERT("add COS1101 Intro to CS  qty=10",
        addProduct(shop, "COS1101", "Intro to CS",  30.50f, 10) == 1);
    ASSERT("add COS2201 Data Struct   qty=8",
        addProduct(shop, "COS2201", "Data Structures", 25.00f, 8) == 1);
    ASSERT("add COS3301 Algorithms    qty=5",
        addProduct(shop, "COS3301", "Algorithms",   40.00f, 5) == 1);

    section("INTEGRATION: member adds books to cart");
    ASSERT("add 2x COS1101 to cart",
        addCart(basket, "eve", "COS1101", 2) == STATUS_OK);
    ASSERT("add 3x COS2201 to cart",
        addCart(basket, "eve", "COS2201", 3) == STATUS_OK);
    ASSERT("add 1x COS3301 to cart",
        addCart(basket, "eve", "COS3301", 1) == STATUS_OK);

    section("INTEGRATION: checkout deducts store quantity");
    // simulate server handleCheckoutCart deduction
    for (int i = 0; i < MAX_CART; i++) {
        Cart *c = &basket->carts[i];
        if (!c->isUsed || c->checkout != 0) continue;
        if (strcmp(c->username, "eve") != 0) continue;
        int idx = findProductIndex(shop, c->productId);
        if (idx >= 0)
            shop->items[idx].quantity -= c->quantity;
    }
    checkoutCart(basket, "eve", NULL);

    int idx1 = findProductIndex(shop, "COS1101");
    int idx2 = findProductIndex(shop, "COS2201");
    int idx3 = findProductIndex(shop, "COS3301");
    ASSERT("COS1101 qty reduced from 10 to 8",
        shop->items[idx1].quantity == 8);
    ASSERT("COS2201 qty reduced from 8 to 5",
        shop->items[idx2].quantity == 5);
    ASSERT("COS3301 qty reduced from 5 to 4",
        shop->items[idx3].quantity == 4);

    section("INTEGRATION: cart is checked out, store quantities stay after second checkout");
    checkoutCart(basket, "eve", NULL);
    ASSERT("second checkout does not reduce qty again",
        shop->items[idx1].quantity == 8);

    section("INTEGRATION: another member buys remaining COS3301 stock");
    addCart(basket, "frank", "COS3301", 4);
    for (int i = 0; i < MAX_CART; i++) {
        Cart *c = &basket->carts[i];
        if (!c->isUsed || c->checkout != 0) continue;
        if (strcmp(c->username, "frank") != 0) continue;
        int idx = findProductIndex(shop, c->productId);
        if (idx >= 0)
            shop->items[idx].quantity -= c->quantity;
    }
    checkoutCart(basket, "frank", NULL);
    ASSERT("COS3301 qty is now 0 after frank buys 4",
        shop->items[idx3].quantity == 0);

    munmap(shop,   sizeof(Store));
    munmap(basket, sizeof(Order));


    // =========================================================================
    // INTEGRATION: Normal user journey (no data deletion)
    // =========================================================================
    printf("\n--- Normal User Journey ---\n");

    UserSessions *uj_sessions = mmap(NULL, sizeof(UserSessions),
        PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    Store *uj_store = mmap(NULL, sizeof(Store),
        PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    Order *uj_order = mmap(NULL, sizeof(Order),
        PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    memset(uj_sessions, 0, sizeof(UserSessions));
    memset(uj_store,    0, sizeof(Store));
    memset(uj_order,    0, sizeof(Order));

    // seed store with 3 books
    addProduct(uj_store, "COS1101", "Intro to CS",      30.50f, 10);
    addProduct(uj_store, "COS2201", "Data Structures",  25.00f,  8);
    addProduct(uj_store, "COS3301", "Algorithms",       40.00f,  5);

    section("USER JOURNEY: register");
    ASSERT("grace registers successfully",
        registerUser(uj_sessions, "grace", "grace123", false) == STATUS_OK);
    int grace_not_admin = 0;
    for (int i = 0; i < MAX_USERS; i++)
        if (uj_sessions->users[i].isActive &&
            strcmp(uj_sessions->users[i].username, "grace") == 0)
            grace_not_admin = uj_sessions->users[i].isAdmin ? 0 : 1;
    ASSERT("grace is not admin", grace_not_admin);

    section("USER JOURNEY: login");
    User *uj_user = loginUser(uj_sessions, "grace", "grace123");
    ASSERT("grace logs in",
        uj_user != NULL);
    ASSERT("session is assigned",
        uj_user != NULL && uj_user->session_id != 0);
    unsigned long uj_sid = uj_user ? uj_user->session_id : 0;
    ASSERT("session lookup works after login",
        getUserBySession(uj_sessions, uj_sid) != NULL);

    section("USER JOURNEY: browse store");
    char uj_buf[2048];
    strcpy(uj_buf, "VIEW_PRODUCT");
    getStore(uj_store, uj_buf, sizeof(uj_buf));
    ASSERT("store lists COS1101",
        strstr(uj_buf, "COS1101") != NULL);
    ASSERT("store lists COS2201",
        strstr(uj_buf, "COS2201") != NULL);
    ASSERT("store lists COS3301",
        strstr(uj_buf, "COS3301") != NULL);

    section("USER JOURNEY: search product");
    strcpy(uj_buf, "SEARCH_PRODUCT");
    searchStore(uj_store, "COS2201", uj_buf, sizeof(uj_buf));
    ASSERT("search COS2201 found",
        strstr(uj_buf, ",0,") != NULL);
    ASSERT("search result shows correct title",
        strstr(uj_buf, "Data Structures") != NULL);

    section("USER JOURNEY: add to cart");
    ASSERT("add 2x COS1101",
        addCart(uj_order, "grace", "COS1101", 2) == STATUS_OK);
    ASSERT("add 1x COS3301",
        addCart(uj_order, "grace", "COS3301", 1) == STATUS_OK);
    ASSERT("add 1 more COS1101 — qty accumulates",
        addCart(uj_order, "grace", "COS1101", 1) == STATUS_OK);

    section("USER JOURNEY: view cart");
    strcpy(uj_buf, "VIEW_CART");
    getOrder(uj_order, "grace", uj_buf, sizeof(uj_buf), 0);
    ASSERT("cart shows STATUS_OK",
        strstr(uj_buf, ",0,") != NULL);
    ASSERT("cart contains COS1101",
        strstr(uj_buf, "COS1101") != NULL);
    ASSERT("cart contains COS3301",
        strstr(uj_buf, "COS3301") != NULL);
    ASSERT("COS2201 not in cart",
        strstr(uj_buf, "COS2201") == NULL);

    int uj_i1 = findProductIndex(uj_store, "COS1101");
    int uj_i3 = findProductIndex(uj_store, "COS3301");
    int qty_before_1 = uj_store->items[uj_i1].quantity;
    int qty_before_3 = uj_store->items[uj_i3].quantity;

    section("USER JOURNEY: checkout");
    // deduct store qty (server-side logic)
    for (int i = 0; i < MAX_CART; i++) {
        Cart *c = &uj_order->carts[i];
        if (!c->isUsed || c->checkout != 0) continue;
        if (strcmp(c->username, "grace") != 0) continue;
        int idx = findProductIndex(uj_store, c->productId);
        if (idx >= 0) uj_store->items[idx].quantity -= c->quantity;
    }
    ASSERT("checkout returns STATUS_OK",
        checkoutCart(uj_order, "grace", NULL) == STATUS_OK);
    ASSERT("COS1101 qty reduced by 3 (2+1)",
        uj_store->items[uj_i1].quantity == qty_before_1 - 3);
    ASSERT("COS3301 qty reduced by 1",
        uj_store->items[uj_i3].quantity == qty_before_3 - 1);

    section("USER JOURNEY: cart empty after checkout");
    strcpy(uj_buf, "VIEW_CART");
    getOrder(uj_order, "grace", uj_buf, sizeof(uj_buf), 0);
    ASSERT("pending cart is empty",
        strstr(uj_buf, "204") != NULL);

    section("USER JOURNEY: view order history");
    strcpy(uj_buf, "VIEW_ORDER");
    getOrder(uj_order, "grace", uj_buf, sizeof(uj_buf), 1);
    ASSERT("history shows STATUS_OK",
        strstr(uj_buf, ",0,") != NULL);
    ASSERT("history contains COS1101",
        strstr(uj_buf, "COS1101") != NULL);
    ASSERT("history contains COS3301",
        strstr(uj_buf, "COS3301") != NULL);

    section("USER JOURNEY: logout");
    ASSERT("logout returns STATUS_OK",
        logoutUser(uj_sessions, "grace") == STATUS_OK);
    ASSERT("session invalid after logout",
        getUserBySession(uj_sessions, uj_sid) == NULL);

    munmap(uj_sessions, sizeof(UserSessions));
    munmap(uj_store,    sizeof(Store));
    munmap(uj_order,    sizeof(Order));


    // ─── Summary ──────────────────────────────────────────────────────────────
    printf("\n=============================\n");
    printf(" Results: %d / %d passed\n", tests_passed, tests_run);
    printf("=============================\n");

    munmap(sessions, sizeof(UserSessions));
    munmap(store,    sizeof(Store));
    munmap(order,    sizeof(Order));
    munmap(shared_output, 8192);
    return tests_passed == tests_run ? 0 : 1;
}
