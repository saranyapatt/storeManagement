#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <crypt.h>
#include <time.h>
#include <stdbool.h>
#include "common.h"


char* hash_password(const char *password);
bool verify_password(const char *password, const char *hash);
unsigned long generate_session_id(char *username);
User* getUserBySession(UserSessions *head, unsigned long sid);
int registerUser(UserSessions *head,const char *username, const char *password, bool isAdmin);
User* loginUser(UserSessions *head, const char *username, const char *password);
int logoutUser(UserSessions *head, const char *username);
int saveUser(UserSessions *head, char *fileName);
int loadUser(UserSessions *user, char *fileName);
char* getUser(UserSessions *head,char *output, size_t outputSize);

// OK
// 1. Hash Password
char* hash_password(const char *password) {
    // Using a static salt for this example
    const char *salt = "$2b$12$R9h/cIPz0gi.URQHue91zu";
    return crypt(password, salt);
    }

// OK
// 2. Verify
bool verify_password(const char *password, const char *hash) {
    if (!password || !hash) return false;
    char *new_hash = crypt(password, hash);
    if (!new_hash) return false;
    return strcmp(new_hash, hash) == 0;
}

// OK
// 3. Generate Session ID
unsigned long generate_session_id(char *username) {
    char seed[128];
    snprintf(seed, sizeof(seed), "%s%ld", username, (long)time(NULL));
    unsigned long hash = 5381;
    int c;
    char *ptr = seed;
    while ((c = *ptr++)) {
    hash = ((hash << 5) + hash) + c;
    }
    return hash;
    }

// OK
// 4. Get User By Session
User* getUserBySession(UserSessions *sessions, unsigned long sid) {
    if (!sessions || sid == 0) return NULL;
    for (int i = 0; i < MAX_USERS; i++) {
        if (sessions->users[i].isActive && sessions->users[i].session_id == sid) {
            return &sessions->users[i];
        }
    }
    return NULL;
}

// OK
// 5. Register User
int registerUser(UserSessions *sessions, const char *username, const char *password, bool isAdmin) {
    if (!sessions || !username || !password) return STATUS_INVALID_ARGUMENTS;
    for (int i = 0; i < MAX_USERS; i++) {
        if (sessions->users[i].isActive && strcmp(sessions->users[i].username, username) == 0) {
            return STATUS_DUPLICATE_USER;
        }
    }

    for (int i = 0; i < MAX_USERS; i++) {
        if (!sessions->users[i].isActive) {
            memset(&sessions->users[i], 0, sizeof(User));
            
            strncpy(sessions->users[i].username, username, USERNAME_SIZE - 1);
            
            char *hashed = hash_password(password);
            strncpy(sessions->users[i].password_hash, hashed, PASSWORD_SIZE - 1);
            
            sessions->users[i].isAdmin = isAdmin;
            sessions->users[i].isActive = true;

            FILE *file = fopen(USER_FILENAME, "a");
            if (file != NULL) {
                fprintf(file, "%s,%s,%d\n", 
                        sessions->users[i].username, 
                        sessions->users[i].password_hash,
                        sessions->users[i].isAdmin ? 1 : 0);
                fclose(file);
            } else {
                printf("[Error] Cannot open user.txt for writing.\n");
                return STATUS_FAIL;
            }
            return STATUS_OK;
        }
    }
    return STATUS_FAIL;
}

// OK
// 6. Login User
User* loginUser(UserSessions *sessions, const char *username, const char *password) {
    if (!sessions || !username || !password) return NULL;

    for (int i = 0; i < MAX_USERS; i++) {
        if (sessions->users[i].isActive && strcmp(sessions->users[i].username, username) == 0) {
            if (verify_password(password, sessions->users[i].password_hash)) {
                sessions->users[i].session_id = generate_session_id((char*)username);
                return &sessions->users[i];
            }
        }
    }
    return NULL;
}

// OK
// 7. Logout User
int logoutUser(UserSessions *sessions, const char *username) {
    for (int i = 0; i < MAX_USERS; i++) {
        if (sessions->users[i].isActive && strcmp(sessions->users[i].username, username) == 0) {
            sessions->users[i].session_id = 0;
            return STATUS_OK;
        }
    }
    return STATUS_FAIL;
}

// OK
// 8. Save User
int saveUser(UserSessions *sessions, char *fileName) {
    FILE *file = fopen(fileName, "w");
    if (!file) return 0;
    for (int i = 0; i < MAX_USERS; i++) {
        if (sessions->users[i].isActive) {
            fprintf(file, "%s,%s,%d\n", 
                    sessions->users[i].username, 
                    sessions->users[i].password_hash, 
                    sessions->users[i].isAdmin ? 1 : 0);
        }
    }
    fclose(file);
    return 1;
}

// OK
// 9. Load User
int loadUser(UserSessions *sessions, char *fileName) {
    FILE *file = fopen(fileName, "r");
    if (!file) return 0;

    memset(sessions, 0, sizeof(UserSessions));
    char line[256];
    int i = 0;
    while (fgets(line, sizeof(line), file) && i < MAX_USERS) {
        int adminInt = 0;
        line[strcspn(line, "\r\n")] = '\0';
        
        if (strlen(line) < 5) continue;

        if (sscanf(line, "%[^,],%[^,],%d", 
                   sessions->users[i].username, 
                   sessions->users[i].password_hash, 
                   &adminInt) == 3) {
            sessions->users[i].isAdmin = (adminInt == 1);
            sessions->users[i].isActive = true;
            sessions->users[i].session_id = 0;
            i++;
        }
    }
    fclose(file);
    return 1;
}

// OK
// 10. Get User
char* getUser(UserSessions *sessions, char *output, size_t outputSize) {
    if (!sessions || !output) return NULL;
    char line[128];
    snprintf(output, outputSize, "\n%-15s | %-10s\n", "Username", "Role");
    strncat(output, "---------------------------\n", outputSize - strlen(output) - 1);

    for (int i = 0; i < MAX_USERS; i++) {
        if (sessions->users[i].isActive) {
            snprintf(line, sizeof(line), "%-15s | %-10s\n", 
                     sessions->users[i].username, 
                     sessions->users[i].isAdmin ? "Admin" : "User");
            if (strlen(output) + strlen(line) < outputSize - 1) {
                strcat(output, line);
            }
        }
    }
    return output;
}