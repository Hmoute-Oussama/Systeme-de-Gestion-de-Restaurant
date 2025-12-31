#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "include/personnel.h"

static Employe *head = NULL;
static int last_id = 0;
static const char *PERSONNEL_PATH = "data/personnel.txt";

void load_personnel(const char *path){
    const char *p = path?path:PERSONNEL_PATH;
    FILE *f = fopen(p,"r");
    if(!f) return;
    int id; char name[128]; char role[32]; double hours;
    while(fscanf(f,"%d|%127[^|]|%31[^|]|%lf\n",&id,name,role,&hours)==4){
        Employe *e = malloc(sizeof(Employe));
        e->id = id; strncpy(e->name,name,127); strncpy(e->role,role,31); e->hours_worked = hours;
        e->prev = NULL; e->next = head;
        if(head) head->prev = e;
        head = e;
        if(id>last_id) last_id = id;
    }
    fclose(f);
}

void save_personnel(const char *path){
    const char *p = path?path:PERSONNEL_PATH;
    FILE *f = fopen(p,"w");
    if(!f) return;
    for(Employe *it=head; it; it=it->next){
        fprintf(f,"%d|%s|%s|%.2f\n", it->id, it->name, it->role, it->hours_worked);
    }
    fclose(f);
}

Employe* add_employe(Employe *h, Employe e){
    Employe *n = malloc(sizeof(Employe));
    memset(n,0,sizeof(Employe));
    n->id = ++last_id;
    strcpy(n->name,e.name);
    strcpy(n->role,e.role);
    n->hours_worked = e.hours_worked;
    n->prev = NULL; n->next = head;
    if(head) head->prev = n;
    head = n;
    save_personnel(NULL);
    return head;
}

int update_employe(int id, const char *name, const char *role, double hours){
    for(Employe *it = head; it; it = it->next){
        if(it->id == id){
            if(name) strncpy(it->name, name, sizeof(it->name)-1);
            if(role) strncpy(it->role, role, sizeof(it->role)-1);
            it->hours_worked = hours;
            save_personnel(NULL);
            fprintf(stderr, "[personnel] updated id=%d name=%s role=%s hours=%.2f\n", it->id, it->name, it->role, it->hours_worked);
            return 1;
        }
    }
    return 0;
}

int delete_employe(int id){
    Employe *it = head;
    while(it){
        if(it->id == id){
            if(it->prev) it->prev->next = it->next; else head = it->next;
            if(it->next) it->next->prev = it->prev;
            free(it);
            save_personnel(NULL);
            fprintf(stderr, "[personnel] deleted id=%d\n", id);
            return 1;
        }
        it = it->next;
    }
    return 0;
}

int next_employe_id(){ return last_id+1; }

void free_personnel(){
    Employe *it = head;
    while(it){ Employe *n=it->next; free(it); it=n; }
    head = NULL;
}

Employe* get_personnel_list(){ return head; }

Employe* find_employe_by_id(int id){
    for(Employe *it = head; it; it = it->next) if(it->id == id) return it;
    return NULL;
}
