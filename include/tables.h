#ifndef TABLES_H
#define TABLES_H

#include <time.h>

typedef enum {LIBRE, OCCUPEE, RESERVEE} TableEtat;

typedef struct Table {
    int id;
    int capacity;
    TableEtat etat;
    char reserver_nom[128];
    time_t reserver_when;
    int reserver_duration_min;
    struct Table *next;
} Table;

void load_tables(const char *path);
void save_tables(const char *path);
Table* get_tables();
Table* reserve_best_table(int guests, const char *name, time_t when, int duration_min);
void free_tables();

#endif
