#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../include/commandes.h"
#include "../include/plats.h"

int main(void){

    load_plats(NULL);
    load_commandes(NULL);

    int before = 0; for(Commande *c = get_commande_list(); c; c = c->next) before++;
    printf("commandes before: %d\n", before);


    Commande c = {0};
    c.client_id = 0;
    c.statut = EN_ATTENTE;
    c.lignes = NULL;

    Ligne *l1 = malloc(sizeof(Ligne)); l1->plat_id = next_plat_id()-1; l1->quantity = 1; l1->next = c.lignes; c.lignes = l1;
    Ligne *l2 = malloc(sizeof(Ligne)); l2->plat_id = next_plat_id()-1; l2->quantity = 2; l2->next = c.lignes; c.lignes = l2;

    add_commande(NULL, c);
    save_commandes(NULL);

    load_commandes(NULL);
    int after = 0; for(Commande *cc = get_commande_list(); cc; cc = cc->next) after++;
    printf("commandes after: %d\n", after);

    if(after <= before) return 1;
    printf("Test PASSED: commandes incremented\n");
    return 0;
}
