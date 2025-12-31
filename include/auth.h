#ifndef AUTH_H
#define AUTH_H

#include <stdbool.h>

typedef struct User {
    char username[64];
    char hash[128];
    char role[32];
    struct User *next;
} User;

void load_users(const char *path);
void save_users(const char *path);
bool authenticate(const char *username, const char *password, char out_role[32]);
void add_user(const char *username, const char *password, const char *role);
void free_users();

#endif
