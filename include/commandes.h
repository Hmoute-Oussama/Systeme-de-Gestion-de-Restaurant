#ifndef COMMANDES_H
#define COMMANDES_H

#include "plats.h"
#include "clients.h"
#include <time.h>

typedef enum {EN_ATTENTE, EN_PREPARATION, SERVIE, ANNULEE} Statut;

typedef struct Ligne {
    int plat_id;
    int quantity;
    double line_total;
    struct Ligne *next;
} Ligne;

typedef struct Commande {
    int id;
    int client_id;
    Ligne *lignes;
    double total;
    Statut statut;
    time_t timestamp;
    struct Commande *next;
} Commande;

void load_commandes(const char *path);
void save_commandes(const char *path);
Commande* add_commande(Commande *head, Commande c);
Commande* find_commande_by_id(Commande *head, int id);
void update_commande_status(int id, Statut s);
void free_commandes(Commande *head);
Commande* get_commande_list();
int next_commande_id();
void generate_invoice(int commande_id, const char *outdir);

#endif
