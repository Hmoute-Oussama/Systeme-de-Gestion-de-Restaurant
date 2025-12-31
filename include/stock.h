#ifndef STOCK_H
#define STOCK_H

typedef struct Ingredient {
    char name[128];
    double quantity;
    char unit[16];
    struct Ingredient *next;
} Ingredient;

void load_stock(const char *path);
void save_stock(const char *path);
Ingredient* find_ingredient(const char *name);
void decrement_ingredients_from_plate(int plat_id, int portions);
void free_stock();
void alert_low_stock(double threshold);

Ingredient* get_stock_list();
int add_ingredient(const char *name, double quantity, const char *unit);
int update_ingredient(const char *name, double quantity, const char *unit);
int delete_ingredient(const char *name);

#endif
