#include <stdio.h>
#include <stdlib.h>
#include "include/config.h"
#include "include/plats.h"
#include "include/clients.h"
#include "include/commandes.h"
#include "include/stock.h"
#include "include/tables.h"
#include "include/personnel.h"
#include "include/statistiques.h"
#include "include/auth.h"
#include "include/ui.h"

int main(int argc, char **argv){

    load_config(NULL);
    load_users(NULL);
    load_plats(NULL);
    load_clients(NULL);
    load_stock(NULL);
    load_tables(NULL);
    load_personnel(NULL);
    load_commandes(NULL);


    if(!authenticate("admin","admin",(char[32]){0})){
        add_user("admin","admin","Admin");
    }

    init_ui(&argc, &argv);


    save_plats(NULL);
    save_clients(NULL);
    save_stock(NULL);
    save_tables(NULL);
    save_personnel(NULL);
    save_commandes(NULL);
    save_users(NULL);
    save_config(NULL);
    return 0;
}
