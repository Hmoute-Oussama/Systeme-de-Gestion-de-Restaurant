#ifndef PLATS_H
#define PLATS_H

#include <stdbool.h>

typedef struct Plat {
    int id;
    char name[128];
    char category[64];
    double price;
    int available;
    double cost;
    double prep_time;
    char ingredients[512];
    struct Plat *next;
} Plat;

void load_plats(const char *path);
void save_plats(const char *path);
Plat* add_plat(Plat *head, Plat p);
Plat* find_plat_by_id(Plat *head, int id);
Plat* remove_plat(Plat *head, int id);
void update_plat(Plat *p);
void free_plats(Plat *head);
Plat* get_plat_list();
int next_plat_id();
int count_plat_sales(int plat_id);
void increment_plat_sales(int plat_id);

#endif
