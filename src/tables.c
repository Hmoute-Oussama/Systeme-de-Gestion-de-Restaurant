#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "include/tables.h"

static Table *tables_head = NULL;
static const char *TABLES_PATH = "data/tables.txt";
static int last_id = 0;

void load_tables(const char *path){
    const char *p = path?path:TABLES_PATH;
    FILE *f = fopen(p,"r");
    if(!f) return;
    Table tmp;
    while(fscanf(f,"%d|%d|%d|%127[^|]|%ld|%d\n",
                 &tmp.id,&tmp.capacity,(int*)&tmp.etat,tmp.reserver_nom,(long*)&tmp.reserver_when,&tmp.reserver_duration_min)==6){
        Table *n = malloc(sizeof(Table));
        *n = tmp;
        n->next = tables_head;
        tables_head = n;
        if(tmp.id>last_id) last_id = tmp.id;
    }

    if(tables_head){
        Table *it = tables_head;
        while(it->next) it = it->next;
        it->next = tables_head;
    }
    fclose(f);
}

void save_tables(const char *path){
    const char *p = path?path:TABLES_PATH;
    char tmp_path[512]; snprintf(tmp_path, sizeof(tmp_path), "%s.tmp", p);
    FILE *f = fopen(tmp_path,"w");
    if(!f) return;
    if(!tables_head){ fclose(f); remove(tmp_path); return; }
    Table *it = tables_head;
    do{
        fprintf(f,"%d|%d|%d|%s|%ld|%d\n", it->id, it->capacity, it->etat, it->reserver_nom, (long)it->reserver_when, it->reserver_duration_min);
        it = it->next;
    } while(it && it!=tables_head);
    fflush(f); fclose(f);
    remove(p); rename(tmp_path, p);
}

Table* get_tables(){ return tables_head; }

Table* reserve_best_table(int guests, const char *name, time_t when, int duration_min){
    if(!tables_head) return NULL;
    Table *best = NULL;
    Table *it = tables_head;
    do{
        if(it->etat==LIBRE && it->capacity>=guests){
            if(!best || it->capacity < best->capacity) best = it;
        }
        it = it->next;
    } while(it && it!=tables_head);
    if(best){
        best->etat = RESERVEE;
        strncpy(best->reserver_nom, name, sizeof(best->reserver_nom)-1);
        best->reserver_when = when;
        best->reserver_duration_min = duration_min;
        save_tables(NULL);
    }
    return best;
}

void free_tables(){
    if(!tables_head) return;

    Table *it = tables_head->next;
    while(it && it!=tables_head){
        Table *n = it->next;
        free(it);
        it = n;
    }
    free(tables_head);
    tables_head = NULL;
}
