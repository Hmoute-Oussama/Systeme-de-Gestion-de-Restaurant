#ifndef CLIENTS_H
#define CLIENTS_H

#include <time.h>

typedef struct Client {
    int id;
    char name[128];
    char phone[32];
    char email[128];
    struct Client *next;
} Client;

void load_clients(const char *path);
void save_clients(const char *path);
Client* add_client(Client *head, Client c);
int update_client(int id, const char *name, const char *phone, const char *email);
int delete_client(int id);
Client* find_client_by_id(Client *head, int id);
void free_clients(Client *head);
Client* get_client_list();
int next_client_id();

#endif
