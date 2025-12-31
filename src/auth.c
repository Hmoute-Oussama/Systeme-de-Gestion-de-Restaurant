#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <openssl/sha.h>
#include "include/auth.h"

static User *users_head = NULL;
static const char *USERS_PATH = "data/users.txt";

static void hash_password(const char *password, const char *salt, char out[128]){
    unsigned char hash[SHA256_DIGEST_LENGTH];
    char buf[512];
    snprintf(buf,sizeof(buf),"%s%s", salt?salt:"", password);
    SHA256((unsigned char*)buf, strlen(buf), hash);
    char hex[128]="\0";
    for(int i=0;i<SHA256_DIGEST_LENGTH;i++) sprintf(hex+strlen(hex),"%02x", hash[i]);
    strcpy(out, hex);
}

void load_users(const char *path){
    const char *p = path?path:USERS_PATH;
    FILE *f = fopen(p,"r");
    if(!f) return;
    char username[64], hash[128], role[32];
    while(fscanf(f,"%63[^|]|%127[^|]|%31[^\n]\n", username, hash, role)==3){
        User *u = malloc(sizeof(User));
        strcpy(u->username, username); strcpy(u->hash, hash); strcpy(u->role, role);
        u->next = users_head; users_head = u;
    }
    fclose(f);
}

void save_users(const char *path){
    const char *p = path?path:USERS_PATH;
    FILE *f = fopen(p,"w");
    if(!f) return;
    for(User *it=users_head; it; it=it->next) fprintf(f,"%s|%s|%s\n", it->username, it->hash, it->role);
    fclose(f);
}

bool authenticate(const char *username, const char *password, char out_role[32]){
    for(User *it=users_head; it; it=it->next){
        if(strcmp(it->username, username)==0){
            char h[128];

            hash_password(password, username, h);
            if(strcmp(h, it->hash)==0){ strcpy(out_role, it->role); return true; }
            return false;
        }
    }
    return false;
}

void add_user(const char *username, const char *password, const char *role){
    User *u = malloc(sizeof(User));
    strcpy(u->username, username); strcpy(u->role, role);
    hash_password(password, username, u->hash);
    u->next = users_head; users_head = u;
    save_users(NULL);
}

void free_users(){
    User *it=users_head;
    while(it){ User *n=it->next; free(it); it=n; }
    users_head=NULL;
}
