#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "include/stock.h"
#include "include/plats.h"

static Ingredient *stock_head = NULL;
static const char *STOCK_PATH = "data/stock.txt";

void load_stock(const char *path){
    const char *p = path?path:STOCK_PATH;
    FILE *f = fopen(p,"r");
    if(!f) return;
    Ingredient tmp;
    while(fscanf(f,"%127[^|]|%lf|%15[^\n]\n",tmp.name,&tmp.quantity,tmp.unit)==3){
        Ingredient *n = malloc(sizeof(Ingredient));
        *n = tmp;
        n->next = stock_head;
        stock_head = n;
    }
    fclose(f);
}

void save_stock(const char *path){
    const char *p = path?path:STOCK_PATH;
    FILE *f = fopen(p,"w");
    if(!f) return;
    for(Ingredient *it=stock_head; it; it=it->next){
        fprintf(f,"%s|%.2f|%s\n", it->name, it->quantity, it->unit);
    }
    fclose(f);
}

Ingredient* find_ingredient(const char *name){
    for(Ingredient *it=stock_head; it; it=it->next) if(strcmp(it->name,name)==0) return it;
    return NULL;
}


void decrement_ingredients_from_plate(int plat_id, int portions){
    Plat *p = find_plat_by_id(NULL, plat_id);
    if(!p) return;
    char copy[512]; strcpy(copy,p->ingredients);
    char *tok = strtok(copy,",");
    while(tok){
        char ing[128]; double per=0;
        if(sscanf(tok,"%127[^:]:%lf",ing,&per)==2){
            Ingredient *I = find_ingredient(ing);
            if(I){
                I->quantity -= per * portions;
                if(I->quantity < 0) I->quantity = 0;
            }
        }
        tok = strtok(NULL,",");
    }
    save_stock(NULL);
}

void free_stock(){
    Ingredient *it=stock_head;
    while(it){ Ingredient *n=it->next; free(it); it=n; }
    stock_head=NULL;
}

void alert_low_stock(double threshold){
    for(Ingredient *it=stock_head; it; it=it->next){
        if(it->quantity <= threshold){
            fprintf(stderr,"[ALERT] Low stock: %s -> %.2f %s\n", it->name, it->quantity, it->unit);
        }
    }
}


Ingredient* get_stock_list(){ return stock_head; }

int add_ingredient(const char *name, double quantity, const char *unit){
    if(!name || !*name) return 0;
    Ingredient *existing = find_ingredient(name);
    if(existing){
        existing->quantity += quantity;
        if(unit && unit[0]) strncpy(existing->unit, unit, sizeof(existing->unit)-1);
        save_stock(NULL);
        fprintf(stderr, "[stock] increased %s by %.2f %s -> now %.2f\n", name, quantity, unit?unit:"", existing->quantity);
        return 1;
    }
    Ingredient *n = malloc(sizeof(Ingredient));
    if(!n) return 0;
    strncpy(n->name, name, sizeof(n->name)-1); n->name[sizeof(n->name)-1]='\0';
    n->quantity = quantity;
    if(unit) strncpy(n->unit, unit, sizeof(n->unit)-1); else n->unit[0]='\0';
    n->next = stock_head;
    stock_head = n;
    save_stock(NULL);
    fprintf(stderr, "[stock] added %s %.2f %s\n", name, quantity, unit?unit:"");
    return 1;
}

int update_ingredient(const char *name, double quantity, const char *unit){
    Ingredient *it = find_ingredient(name);
    if(!it) return 0;
    it->quantity = quantity;
    if(unit) strncpy(it->unit, unit, sizeof(it->unit)-1);
    save_stock(NULL);
    fprintf(stderr, "[stock] updated %s -> %.2f %s\n", name, quantity, unit?unit:"");
    return 1;
}

int delete_ingredient(const char *name){
    Ingredient *prev = NULL;
    Ingredient *it = stock_head;
    while(it){
        if(strcmp(it->name, name)==0){
            if(prev) prev->next = it->next; else stock_head = it->next;
            free(it);
            save_stock(NULL);
            fprintf(stderr, "[stock] deleted %s\n", name);
            return 1;
        }
        prev = it; it = it->next;
    }
    return 0;
}
