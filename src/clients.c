#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "include/clients.h"

static Client *clients_head = NULL;
static int last_id = 0;
static const char *CLIENTS_PATH = "data/clients.txt";

void load_clients(const char *path){
    const char *p = path?path:CLIENTS_PATH;
    FILE *f = fopen(p,"r");
    if(!f) return;
    Client tmp;
    while(fscanf(f,"%d|%127[^|]|%31[^|]|%127[^\n]\n",
                 &tmp.id,tmp.name,tmp.phone,tmp.email)==4){
        Client *n = malloc(sizeof(Client));
        *n = tmp;
        n->next = clients_head;
        clients_head = n;
        if(tmp.id>last_id) last_id = tmp.id;
    }
    fclose(f);
}

void save_clients(const char *path){
    const char *p = path?path:CLIENTS_PATH;
    char tmp_path[512]; snprintf(tmp_path, sizeof(tmp_path), "%s.tmp", p);
    FILE *f = fopen(tmp_path,"w");
    if(!f) return;
    for(Client *it=clients_head; it; it=it->next){
        fprintf(f,"%d|%s|%s|%s\n", it->id,it->name,it->phone,it->email);
    }
    fflush(f); fclose(f);
    remove(p); rename(tmp_path, p);
}

Client* add_client(Client *head, Client c){
    Client *n = malloc(sizeof(Client));
    memset(n,0,sizeof(Client));
    n->id = ++last_id;
    strcpy(n->name,c.name);
    strcpy(n->phone,c.phone);
    strcpy(n->email,c.email);
    n->next = clients_head;
    clients_head = n;

    fprintf(stderr, "[clients] added id=%d name=%s phone=%s email=%s\n", n->id, n->name, n->phone, n->email);
    save_clients(NULL);
    return clients_head;
}

Client* find_client_by_id(Client *head, int id){
    for(Client *it=clients_head; it; it=it->next) if(it->id==id) return it;
    return NULL;
}

int update_client(int id, const char *name, const char *phone, const char *email){
    Client *c = find_client_by_id(NULL, id);
    if(!c) return 0;
    strncpy(c->name, name, sizeof(c->name)-1); c->name[sizeof(c->name)-1] = '\0';
    strncpy(c->phone, phone, sizeof(c->phone)-1); c->phone[sizeof(c->phone)-1] = '\0';
    strncpy(c->email, email, sizeof(c->email)-1); c->email[sizeof(c->email)-1] = '\0';

    fprintf(stderr, "[clients] updated id=%d name=%s phone=%s email=%s\n", c->id, c->name, c->phone, c->email);
    save_clients(NULL);
    return 1;
}

int delete_client(int id){
    Client *prev = NULL;
    Client *it = clients_head;
    while(it){
        if(it->id == id){
            if(prev) prev->next = it->next; else clients_head = it->next;
            free(it);
            fprintf(stderr, "[clients] deleted id=%d\n", id);
            save_clients(NULL);
            return 1;
        }
        prev = it;
        it = it->next;
    }
    return 0;
}

void free_clients(Client *head){
    Client *it = clients_head;
    while(it){ Client *n=it->next; free(it); it=n; }
    clients_head = NULL;
}

Client* get_client_list(){ return clients_head; }
int next_client_id(){ return last_id+1; }
