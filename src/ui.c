#include <gtk/gtk.h>
#include <stdio.h>
#include <math.h>
#include <ctype.h>
#include "include/ui.h"
#include "include/auth.h"
#include "include/plats.h"
#include "include/clients.h"
#include "include/commandes.h"
#include "include/stock.h"
#include "include/statistiques.h"
#include "include/tables.h"
#include "include/personnel.h"


static void show_orders_content(void);
static void show_menu_content(void);
static void show_tables_content(void);
static void add_order_line(GtkWidget *list_box, int plat_id, const char *plat_name, int quantity);
static gboolean on_stats_draw(GtkWidget *widget, cairo_t *cr, gpointer user_data);
static void on_delete_plat_clicked(GtkButton *button, gpointer user_data);
static int plat_name_from_file(int id, char *out, size_t sz);


static int plat_name_from_file(int id, char *out, size_t sz) {
    if(!out || sz == 0) return 0;
    const char *path = "data/plats.txt";
    FILE *f = fopen(path, "r");
    if(!f) return 0;

    int fid = 0;
    char name[128];
    char category[64];
    double price = 0.0;
    int available = 0;
    double cost = 0.0, prep = 0.0;
    char ingredients[512];

    while(fscanf(f, "%d|%127[^|]|%63[^|]|%lf|%d|%lf|%lf|%511[^\n]\n", &fid, name, category, &price, &available, &cost, &prep, ingredients) == 8) {
        if(fid == id) {

            strncpy(out, name, sz-1);
            out[sz-1] = '\0';
            fclose(f);
            return 1;
        }
    }

    fclose(f);
    return 0;
}


static int table_filter_mode = 0; 
static int table_capacity_filter = 0; 
static char table_search_text[128] = "";

static void on_table_filter_clicked(GtkButton *button, gpointer user_data) {
    table_filter_mode = GPOINTER_TO_INT(user_data);
    show_tables_content();
}

static void on_table_capacity_changed(GtkComboBoxText *cb, gpointer user_data) {
    const char *txt = gtk_combo_box_text_get_active_text(cb);
    if(!txt) txt = "";
    if(strcmp(txt, "Toutes") == 0) table_capacity_filter = 0;
    else table_capacity_filter = atoi(txt);
    show_tables_content();
}

static void on_table_search_changed(GtkEntry *entry, gpointer user_data) {
    const char *q = gtk_entry_get_text(entry);
    strncpy(table_search_text, q ? q : "", sizeof(table_search_text)-1);
    table_search_text[sizeof(table_search_text)-1] = '\0';
    show_tables_content();
}

static GtkWidget *main_window = NULL;
static GtkWidget *content_area = NULL;
static char current_role[32] = "Employee";


static void on_logout_clicked(GtkButton *button, gpointer user_data);


static void on_add_item_clicked(GtkButton *button, gpointer user_data) {
    struct {
        GtkWidget *combo;
        GtkWidget *spin;
        GtkWidget *list;
    } *data = user_data;
    
    const char *selected_text = gtk_combo_box_text_get_active_text(GTK_COMBO_BOX_TEXT(data->combo));
    if(selected_text) {
        int plat_id;
        char plat_name[128];
        sscanf(selected_text, "%d - %[^(]", &plat_id, plat_name);
        int quantity = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(data->spin));
        add_order_line(data->list, plat_id, g_strchomp(plat_name), quantity);
        g_free((void*)selected_text);
    }
}

static void on_update_order_status(GtkButton *button, gpointer user_data) {
    int *id = (int*)user_data;
    Commande *c = find_commande_by_id(NULL, *id);
    if(c) {
        update_commande_status(*id, c->statut == EN_ATTENTE ? EN_PREPARATION : SERVIE);
        save_commandes("data/commandes.txt");
        show_orders_content();
    }
    g_free(id);
}

static void on_toggle_plat_available(GtkButton *button, gpointer user_data) {
    int *id = (int*)user_data;
    Plat *p = find_plat_by_id(NULL, *id);
    if(p) {
        p->available = !p->available;
        update_plat(p);
        save_plats("data/plats.txt");
        show_menu_content();
    }
    g_free(id);
}

static void on_delete_plat_clicked(GtkButton *button, gpointer user_data) {
    int *id = (int*)user_data;
    Plat *p = find_plat_by_id(NULL, *id);
    if(!p) { g_free(id); return; }


    GtkWidget *dialog = gtk_message_dialog_new(
        GTK_WINDOW(main_window),
        GTK_DIALOG_MODAL,
        GTK_MESSAGE_WARNING,
        GTK_BUTTONS_YES_NO,
        "Supprimer le plat %s?\n\nCette action ne peut pas être annulée.",
        p->name
    );
    gint response = gtk_dialog_run(GTK_DIALOG(dialog));
    gtk_widget_destroy(dialog);

    if(response == GTK_RESPONSE_YES) {
        fprintf(stderr, "[ui] Suppression plat id=%d name=%s\n", *id, p->name);
        remove_plat(get_plat_list(), *id);
        save_plats("data/plats.txt");
        show_menu_content();
    }
    g_free(id);
}


static void on_table_primary_clicked(GtkButton *button, gpointer user_data) {
    int *id = (int*)user_data;
    Table *t = get_tables();
    if(!t) { g_free(id); return; }
    Table *first = t;
    do {
        if(t->id == *id) {

            if(t->etat == LIBRE) t->etat = OCCUPEE;
            else t->etat = LIBRE;
            save_tables(NULL);
            show_tables_content();
            break;
        }
        t = t->next;
    } while(t && t != first);
    g_free(id);
}

static void on_table_details_clicked(GtkButton *button, gpointer user_data) {
    int *id = (int*)user_data;
    Table *t = get_tables();
    if(!t) { return; }
    Table *first = t;
    do {
        if(t->id == *id) {
            char msg[512];
            struct tm *tm = localtime(&t->reserver_when);
            snprintf(msg, sizeof(msg), "Table #%d\nCapacité: %d\nEtat: %s\nRéservé pour: %s\nHeure: %02d:%02d\nDurée: %d min",
                     t->id, t->capacity, t->etat==LIBRE?"Libre":t->etat==OCCUPEE?"Occupée":"Réservée",
                     t->reserver_nom, tm?tm->tm_hour:0, tm?tm->tm_min:0, t->reserver_duration_min);
            GtkWidget *d = gtk_message_dialog_new(GTK_WINDOW(main_window), GTK_DIALOG_MODAL, GTK_MESSAGE_INFO, GTK_BUTTONS_OK, "%s", msg);
            gtk_dialog_run(GTK_DIALOG(d)); gtk_widget_destroy(d);
            break;
        }
        t = t->next;
    } while(t && t != first);
}


static void load_css(void) {
    GtkCssProvider *provider = gtk_css_provider_new();
    gtk_css_provider_load_from_data(provider,
        "button.sidebar-button {"
        "  padding: 10px;"
        "  margin: 2px;"
        "  background: #34495e;"
        "  color: white;"
        "  border: none;"
        "  border-radius: 0;"
        "}"
        "button.sidebar-button:hover {"
        "  background: #3f5f7a;"
        "}"
        "label.header {"
        "  font-size: 24px;"
        "  font-weight: bold;"
        "}"
        "label.welcome {"
        "  font-size: 18px;"
        "  color: #2c3e50;"
        "}"
        ".content-box {"
        "  padding: 20px;"
        "  background: white;"
        "}"
        ".login-window {"
        "  background: #ecf0f1;"
        "}"
        ".login-button {"
        "  background: #2980b9;"
        "  color: white;"
        "  padding: 10px 20px;"
        "  border-radius: 5px;"
        "}"
        ".table-free {"
        "  color: #27ae60;"
        "  font-weight: bold;"
        "}"
        ".table-occupied {"
        "  color: #c0392b;"
        "  font-weight: bold;"
        "}"
        ".table-reserved {"
        "  color: #f39c12;"
        "  font-weight: bold;"
        "}"
        "frame {"
        "  padding: 10px;"
        "  margin: 5px;"
        "  border: 1px solid #bdc3c7;"
        "  border-radius: 5px;"
        "}"
        "frame:hover {"
        "  border-color: #3498db;"
        "}"
        "list box {"
        "  background: white;"
        "}"
        "list row:hover {"
        "  background: #ecf0f1;"
        "}"
        "button {"
        "  padding: 5px 10px;"
        "  border-radius: 3px;"
        "}"
        "button:hover {"
        "  background: #3498db;"
        "  color: white;"
        "}"
        ".white-icon {"
        "  color: white;"
        "}"
        ".dashboard-card {"
        "  border-radius: 10px;"
        "  padding: 20px;"
        "  box-shadow: 0 2px 4px rgba(0,0,0,0.1);"
        "}"
        ".stat-value {"
        "  font-size: 24px;"
        "  font-weight: bold;"
        "}"
        ".stat-label {"
        "  font-size: 14px;"
        "  opacity: 0.9;"
        "}"
        ".orders-list {"
        "  background-color: transparent;"
        "  border: none;"
        "}"
        ".order-row {"
        "  border-bottom: 1px solid #f1f2f6;"
        "  transition: background-color 200ms ease;"
        "}"
        ".order-row:hover {"
        "  background-color: #f8f9fa;"
        "}"
        
    "/* Premium Table card styles */"
    ".table-card {"
    "  background: linear-gradient(135deg, #ffffff 0%, #f8f9fa 100%);"
    "  border-radius: 12px;"
    "  box-shadow: 0 8px 24px rgba(0,0,0,0.12);"
    "  padding: 0;"
    "  border: 1px solid rgba(0,0,0,0.05);"
    "  transition: all 0.3s ease;"
    "  overflow: hidden;"
    "}"
    ".table-card:hover {"
    "  box-shadow: 0 16px 40px rgba(0,0,0,0.18);"
    "  transform: translateY(-6px);"
    "}"
    ".table-title {"
    "  font-weight: 600;"
    "  font-size: 18px;"
    "  color: #2c3e50;"
    "  margin: 0;"
    "  padding: 16px 16px 8px 16px;"
    "}"
    ".table-capacity-label {"
    "  font-size: 14px;"
    "  color: #7f8c8d;"
    "  padding: 0 16px 12px 16px;"
    "  font-weight: normal;"
    "}"
    ".table-badge {"
    "  display: inline-block;"
    "  padding: 8px 16px;"
    "  border-radius: 20px;"
    "  color: white;"
    "  font-weight: bold;"
    "  font-size: 13px;"
    "  margin: 0 16px 12px 16px;"
    "  text-transform: uppercase;"
    "  letter-spacing: 0.5px;"
    "}"
    ".table-free { background: linear-gradient(135deg, #27ae60, #229954); }"
    ".table-occupied { background: linear-gradient(135deg, #e74c3c, #c0392b); }"
    ".table-reserved { background: linear-gradient(135deg, #f39c12, #d68910); }"
        "button.suggested-action {"
        "  background-color: #3498db;"
        "  color: white;"
        "  border: none;"
        "}"
        "/* Sidebar background: improved contrast (lighter than before) */"
        ".sidebar {"
        "  background: #2f455a;"
        "  padding-top: 10px;"
        "}"
        "/* Topbar */"
        ".topbar {"
        "  background: #ffffff;"
        "  border-bottom: 1px solid #e1e5ea;"
        "  padding: 10px 16px;"
        "}"
        "button.suggested-action:hover {"
        "  background-color: #2980b9;"
        "}"
    , -1, NULL);
    
    gtk_style_context_add_provider_for_screen(
        gdk_screen_get_default(),
        GTK_STYLE_PROVIDER(provider),
        GTK_STYLE_PROVIDER_PRIORITY_APPLICATION
    );
    g_object_unref(provider);
}

static void on_add_dish_clicked(GtkButton *button, gpointer user_data) {
    GtkWidget *dialog = gtk_dialog_new_with_buttons("Ajouter un Plat",
                                                   GTK_WINDOW(main_window),
                                                   GTK_DIALOG_MODAL,
                                                   "Annuler", GTK_RESPONSE_CANCEL,
                                                   "Ajouter", GTK_RESPONSE_ACCEPT,
                                                   NULL);
    
    GtkWidget *content_area = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
    gtk_container_set_border_width(GTK_CONTAINER(content_area), 15);
    

    GtkWidget *grid = gtk_grid_new();
    gtk_grid_set_row_spacing(GTK_GRID(grid), 10);
    gtk_grid_set_column_spacing(GTK_GRID(grid), 10);
    

    GtkWidget *name_label = gtk_label_new("Nom du plat:");
    GtkWidget *name_entry = gtk_entry_new();
    gtk_grid_attach(GTK_GRID(grid), name_label, 0, 0, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), name_entry, 1, 0, 1, 1);
    

    GtkWidget *category_label = gtk_label_new("Catégorie:");
    GtkWidget *category_entry = gtk_entry_new();
    gtk_grid_attach(GTK_GRID(grid), category_label, 0, 1, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), category_entry, 1, 1, 1, 1);
    

    GtkWidget *price_label = gtk_label_new("Prix (€):");
    GtkWidget *price_entry = gtk_entry_new();
    gtk_grid_attach(GTK_GRID(grid), price_label, 0, 2, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), price_entry, 1, 2, 1, 1);
    

    GtkWidget *cost_label = gtk_label_new("Coût (€):");
    GtkWidget *cost_entry = gtk_entry_new();
    gtk_grid_attach(GTK_GRID(grid), cost_label, 0, 3, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), cost_entry, 1, 3, 1, 1);
    

    GtkWidget *prep_label = gtk_label_new("Temps de préparation (min):");
    GtkWidget *prep_entry = gtk_entry_new();
    gtk_grid_attach(GTK_GRID(grid), prep_label, 0, 4, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), prep_entry, 1, 4, 1, 1);
    

    GtkWidget *ingredients_label = gtk_label_new("Ingrédients (format: nom:quantité,...):");
    GtkWidget *ingredients_entry = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(ingredients_entry), "ex: Tomate:100,Fromage:80");
    gtk_grid_attach(GTK_GRID(grid), ingredients_label, 0, 5, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), ingredients_entry, 1, 5, 1, 1);
    
    gtk_container_add(GTK_CONTAINER(content_area), grid);
    gtk_widget_show_all(dialog);
    
    gint result = gtk_dialog_run(GTK_DIALOG(dialog));
    if (result == GTK_RESPONSE_ACCEPT) {
        Plat p = {0};
        p.id = next_plat_id();
        
        const char *name = gtk_entry_get_text(GTK_ENTRY(name_entry));
        const char *category = gtk_entry_get_text(GTK_ENTRY(category_entry));
        const char *ingredients = gtk_entry_get_text(GTK_ENTRY(ingredients_entry));
        
        snprintf(p.name, sizeof(p.name), "%s", name);
        snprintf(p.category, sizeof(p.category), "%s", category);
        snprintf(p.ingredients, sizeof(p.ingredients), "%s", ingredients);
        
        p.price = atof(gtk_entry_get_text(GTK_ENTRY(price_entry)));
        p.cost = atof(gtk_entry_get_text(GTK_ENTRY(cost_entry)));
        p.prep_time = atof(gtk_entry_get_text(GTK_ENTRY(prep_entry)));
        p.available = 1;
        
        add_plat(NULL, p);
        save_plats("data/plats.txt");
        
        GtkWidget *success_dialog = gtk_message_dialog_new(GTK_WINDOW(main_window),
                                                         GTK_DIALOG_MODAL,
                                                         GTK_MESSAGE_INFO,
                                                         GTK_BUTTONS_OK,
                                                         "Plat ajouté: %s", p.name);
        gtk_dialog_run(GTK_DIALOG(success_dialog));
        gtk_widget_destroy(success_dialog);
    }
    
    gtk_widget_destroy(dialog);

    show_menu_content();

}

static void add_order_line(GtkWidget *list_box, int plat_id, const char *plat_name, int quantity) {
    GtkWidget *row = gtk_list_box_row_new();
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
    
    char text[256];
    snprintf(text, sizeof(text), "%s x%d", plat_name, quantity);
    GtkWidget *label = gtk_label_new(text);
    gtk_box_pack_start(GTK_BOX(box), label, TRUE, TRUE, 0);
    
    GtkWidget *remove_btn = gtk_button_new_with_label("Supprimer");
    gtk_box_pack_start(GTK_BOX(box), remove_btn, FALSE, FALSE, 0);
    
    gtk_container_add(GTK_CONTAINER(row), box);
    gtk_widget_show_all(row);
    
    gtk_list_box_insert(GTK_LIST_BOX(list_box), row, -1);
    
    g_signal_connect_swapped(remove_btn, "clicked", G_CALLBACK(gtk_widget_destroy), row);
}

static void on_add_order_clicked(GtkButton *button, gpointer user_data) {
    GtkWidget *dialog = gtk_dialog_new_with_buttons("Nouvelle Commande",
                                                   GTK_WINDOW(main_window),
                                                   GTK_DIALOG_MODAL,
                                                   "Annuler", GTK_RESPONSE_CANCEL,
                                                   "Créer", GTK_RESPONSE_ACCEPT,
                                                   NULL);
    
    GtkWidget *content_area = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
    gtk_container_set_border_width(GTK_CONTAINER(content_area), 15);
    gtk_widget_set_size_request(dialog, 400, 500);
    

    GtkWidget *client_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
    GtkWidget *client_label = gtk_label_new("Client ID (0 pour client de passage):");
    GtkWidget *client_entry = gtk_entry_new();
    gtk_entry_set_text(GTK_ENTRY(client_entry), "0");
    gtk_box_pack_start(GTK_BOX(client_box), client_label, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(client_box), client_entry, TRUE, TRUE, 0);
    gtk_container_add(GTK_CONTAINER(content_area), client_box);
    

    GtkWidget *items_frame = gtk_frame_new("Articles");
    GtkWidget *items_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    gtk_container_add(GTK_CONTAINER(items_frame), items_box);
    

    GtkWidget *add_item_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
    GtkWidget *dish_combo = gtk_combo_box_text_new();
    GtkWidget *quantity_spin = gtk_spin_button_new_with_range(1, 100, 1);
    GtkWidget *add_btn = gtk_button_new_with_label("Ajouter");
    

    Plat *plats = get_plat_list();
    while(plats) {
        if(plats->available) {
            char text[256];
            snprintf(text, sizeof(text), "%d - %s (%.2f€)", plats->id, plats->name, plats->price);
            gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(dish_combo), text);
        }
        plats = plats->next;
    }
    
    gtk_box_pack_start(GTK_BOX(add_item_box), dish_combo, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(add_item_box), quantity_spin, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(add_item_box), add_btn, FALSE, FALSE, 0);
    

    GtkWidget *list_box = gtk_list_box_new();
    
    gtk_box_pack_start(GTK_BOX(items_box), add_item_box, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(items_box), list_box, TRUE, TRUE, 0);
    
    gtk_container_add(GTK_CONTAINER(content_area), items_frame);
    

    struct {
        GtkWidget *combo;
        GtkWidget *spin;
        GtkWidget *list;
    } *data = g_new(typeof(*data), 1);
    data->combo = dish_combo;
    data->spin = quantity_spin;
    data->list = list_box;
    
    g_signal_connect(add_btn, "clicked", G_CALLBACK(on_add_item_clicked), data);
    
    gtk_widget_show_all(dialog);
    
    gint result = gtk_dialog_run(GTK_DIALOG(dialog));
    if (result == GTK_RESPONSE_ACCEPT) {
        Commande c = {0};
        c.id = next_commande_id();
        c.client_id = atoi(gtk_entry_get_text(GTK_ENTRY(client_entry)));
        c.timestamp = time(NULL);
        c.statut = EN_ATTENTE;
        c.lignes = NULL;
        c.total = 0.0;
        
        double total = 0.0;
        

        GList *children = gtk_container_get_children(GTK_CONTAINER(list_box));
        for(GList *l = children; l; l = l->next) {
            GtkWidget *row = l->data;
            GtkWidget *box = gtk_bin_get_child(GTK_BIN(row));
            GtkWidget *label = gtk_container_get_children(GTK_CONTAINER(box))->data;
            
            char plat_name[128];
            int quantity;
            sscanf(gtk_label_get_text(GTK_LABEL(label)), "%[^x]x%d", plat_name, &quantity);
            

            Plat *plats = get_plat_list();
            while(plats) {
                if(strcmp(g_strchomp(plat_name), plats->name) == 0) {
                    Ligne *ligne = malloc(sizeof(Ligne));
                    ligne->plat_id = plats->id;
                    ligne->quantity = quantity;
                    ligne->line_total = plats->price * quantity;
                    ligne->next = c.lignes;
                    c.lignes = ligne;
                    

                    total += ligne->line_total;
                    break;
                }
                plats = plats->next;
            }
        }
        g_list_free(children);
        
        c.total = total;
        
        add_commande(NULL, c);
        save_commandes("data/commandes.txt");
        load_commandes("data/commandes.txt");
        
        GtkWidget *success_dialog = gtk_message_dialog_new(GTK_WINDOW(main_window),
                                                         GTK_DIALOG_MODAL,
                                                         GTK_MESSAGE_INFO,
                                                         GTK_BUTTONS_OK,
                                                         "Commande #%d créée\nTotal: %.2f€",
                                                         c.id, c.total);
        gtk_dialog_run(GTK_DIALOG(success_dialog));
        gtk_widget_destroy(success_dialog);
        

        show_orders_content();
    }
    
    gtk_widget_destroy(dialog);

    g_free(data);
}

static void on_order_details_clicked(GtkButton *button, gpointer user_data);

static void on_view_all_clicked(GtkButton *button, gpointer user_data) {
    show_orders_content();
}

static void show_dashboard_content() {
    gtk_container_foreach(GTK_CONTAINER(content_area), (GtkCallback)gtk_widget_destroy, NULL);
    

    GtkWidget *dashboard_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 20);
    gtk_style_context_add_class(gtk_widget_get_style_context(dashboard_box), "content-box");
    gtk_widget_set_margin_start(dashboard_box, 20);
    gtk_widget_set_margin_end(dashboard_box, 20);
    

    GtkWidget *header_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    

    GtkWidget *welcome_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 15);
    GtkWidget *dashboard_icon = gtk_image_new_from_icon_name("view-grid-symbolic", GTK_ICON_SIZE_DIALOG);
    gtk_box_pack_start(GTK_BOX(welcome_box), dashboard_icon, FALSE, FALSE, 0);
    
    GtkWidget *text_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    GtkWidget *header = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(header), "<span size='xx-large' weight='bold'>Tableau de Bord</span>");
    gtk_box_pack_start(GTK_BOX(text_box), header, FALSE, FALSE, 0);
    
    char welcome_text[256];
    char *time_str = NULL;
    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);
    
    if(tm_info->tm_hour < 12)
        time_str = "Bonjour";
    else if(tm_info->tm_hour < 18)
        time_str = "Bon après-midi";
    else
        time_str = "Bonsoir";
    
    snprintf(welcome_text, sizeof(welcome_text), 
             "<span size='large' foreground='#666666'>%s, %s</span>", 
             time_str, current_role);
    GtkWidget *welcome = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(welcome), welcome_text);
    gtk_box_pack_start(GTK_BOX(text_box), welcome, FALSE, FALSE, 0);
    
    gtk_box_pack_start(GTK_BOX(welcome_box), text_box, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(header_box), welcome_box, TRUE, TRUE, 0);
    

    char date_str[128];
    strftime(date_str, sizeof(date_str), 
             "<span size='large'>%A %d %B %Y</span>", 
             tm_info);
    GtkWidget *date_label = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(date_label), date_str);
    gtk_box_pack_end(GTK_BOX(header_box), date_label, FALSE, FALSE, 20);
    
    gtk_box_pack_start(GTK_BOX(dashboard_box), header_box, FALSE, FALSE, 0);
    

    GtkWidget *stats_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 20);
    gtk_widget_set_margin_top(stats_box, 20);
    

    int total_orders = 0, orders_today = 0;
    double total_revenue = 0, revenue_today = 0;
    time_t day_start = now - (now % 86400);
    
    Commande *cmd = get_commande_list();
    while(cmd) {
        total_orders++;
        total_revenue += cmd->total;
        if(cmd->timestamp >= day_start) {
            orders_today++;
            revenue_today += cmd->total;
        }
        cmd = cmd->next;
    }
    
    int total_tables = 0, available_tables = 0;
    Table *table = get_tables();
    Table *first_table = table;
    if(table) do {
        total_tables++;
        if(table->etat == LIBRE)
            available_tables++;
        table = table->next;
    } while(table != first_table);
    

    const char *stat_icons[] = {
        "go-jump", "euro-symbolic", 
        "utilities-system-monitor", "view-refresh"
    };
    
    const char *stat_titles[] = {
        "Commandes du Jour", "Chiffre d'Affaires",
        "Tables Disponibles", "Taux d'Occupation"
    };
    
    char stat_values[4][64];
    snprintf(stat_values[0], 64, "%d", orders_today);
    snprintf(stat_values[1], 64, "%.2f€", total_revenue);
    snprintf(stat_values[2], 64, "%d/%d", available_tables, total_tables);
    snprintf(stat_values[3], 64, "%.1f%%", 
             total_tables ? (100.0 * (total_tables - available_tables) / total_tables) : 0.0);
    
    const char *stat_colors[] = {
        "#3498db", "#2ecc71", "#e74c3c", "#f39c12"
    };
    
    for(int i = 0; i < 4; i++) {
        GtkWidget *stat_card = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 15);
        gtk_widget_set_size_request(stat_card, 250, 120);
        

        char css[512];
        snprintf(css, sizeof(css),
                "box { background-color: %s; border-radius: 10px; padding: 15px; }",
                stat_colors[i]);
        GtkCssProvider *provider = gtk_css_provider_new();
        gtk_css_provider_load_from_data(provider, css, -1, NULL);
        gtk_style_context_add_provider(
            gtk_widget_get_style_context(stat_card),
            GTK_STYLE_PROVIDER(provider),
            GTK_STYLE_PROVIDER_PRIORITY_APPLICATION
        );
        

        GtkWidget *icon = gtk_image_new_from_icon_name(stat_icons[i], GTK_ICON_SIZE_DIALOG);
        gtk_image_set_pixel_size(GTK_IMAGE(icon), 48);
        GtkStyleContext *icon_context = gtk_widget_get_style_context(icon);
        gtk_style_context_add_class(icon_context, "white-icon");
        gtk_box_pack_start(GTK_BOX(stat_card), icon, FALSE, FALSE, 0);
        

        GtkWidget *text_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
        

        GtkWidget *title = gtk_label_new(NULL);
        char *title_markup = g_markup_printf_escaped(
            "<span foreground='white' size='small' weight='bold'>%s</span>",
            stat_titles[i]);
        gtk_label_set_markup(GTK_LABEL(title), title_markup);
        g_free(title_markup);
        gtk_widget_set_halign(title, GTK_ALIGN_START);
        gtk_box_pack_start(GTK_BOX(text_box), title, FALSE, FALSE, 0);
        

        GtkWidget *value = gtk_label_new(NULL);
        char *value_markup = g_markup_printf_escaped(
            "<span foreground='white' size='xx-large' weight='bold'>%s</span>",
            stat_values[i]);
        gtk_label_set_markup(GTK_LABEL(value), value_markup);
        g_free(value_markup);
        gtk_widget_set_halign(value, GTK_ALIGN_START);
        gtk_box_pack_start(GTK_BOX(text_box), value, FALSE, FALSE, 0);
        
        gtk_box_pack_start(GTK_BOX(stat_card), text_box, TRUE, TRUE, 0);
        gtk_box_pack_start(GTK_BOX(stats_box), stat_card, TRUE, TRUE, 0);
        
        g_object_unref(provider);
    }
    
    gtk_box_pack_start(GTK_BOX(dashboard_box), stats_box, FALSE, FALSE, 0);
    

    GtkWidget *recent_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 15);
    gtk_widget_set_margin_top(recent_box, 30);
    
    GtkWidget *recent_header_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    GtkWidget *recent_header = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(recent_header), 
                        "<span size='x-large' weight='bold' foreground='#2c3e50'>🕒 Commandes Récentes</span>");
    gtk_widget_set_halign(recent_header, GTK_ALIGN_START);
    gtk_box_pack_start(GTK_BOX(recent_header_box), recent_header, TRUE, TRUE, 0);
    

    GtkWidget *view_all_btn = gtk_button_new_with_label("Tout Voir");
    GtkStyleContext *btn_ctx = gtk_widget_get_style_context(view_all_btn);
    gtk_style_context_add_class(btn_ctx, "suggested-action"); 
    g_signal_connect(view_all_btn, "clicked", G_CALLBACK(on_view_all_clicked), NULL);
    gtk_box_pack_end(GTK_BOX(recent_header_box), view_all_btn, FALSE, FALSE, 0);
    
    gtk_box_pack_start(GTK_BOX(recent_box), recent_header_box, FALSE, FALSE, 0);
    

    GtkWidget *orders_list = gtk_list_box_new();
    gtk_list_box_set_selection_mode(GTK_LIST_BOX(orders_list), GTK_SELECTION_NONE);
    

    GtkCssProvider *list_provider = gtk_css_provider_new();
    gtk_css_provider_load_from_data(list_provider,
        "list { background: transparent; }"
        "row { "
        "  background: #ffffff; "
        "  margin-bottom: 10px; "
        "  border-radius: 8px; "
        "  box-shadow: 0 2px 5px rgba(0,0,0,0.05); "
        "  padding: 12px; "
        "  border: 1px solid #f0f0f0; "
        "  transition: all 0.2s; "
        "} "
        "row:hover { "
        "  background: #f8faff; "
        "  box-shadow: 0 4px 8px rgba(0,0,0,0.1); "
        "  transform: translateY(-2px); "
        "}", -1, NULL);
    gtk_style_context_add_provider(
        gtk_widget_get_style_context(orders_list),
        GTK_STYLE_PROVIDER(list_provider),
        GTK_STYLE_PROVIDER_PRIORITY_APPLICATION
    );
    g_object_unref(list_provider);
    

    int row_count = 0;
    cmd = get_commande_list();

    
    while(cmd && row_count < 5) {
        GtkWidget *row_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 15);
        

        GtkWidget *status_indicator = gtk_drawing_area_new();
        gtk_widget_set_size_request(status_indicator, 6, 40);
        gtk_widget_set_valign(status_indicator, GTK_ALIGN_CENTER);
        
        const char *status_color_css;
        const char *status_icon_name;
        const char *status_text_nice;
        
        switch(cmd->statut) {
            case EN_ATTENTE:
                status_color_css = "#e74c3c";
                status_icon_name = "alarm-symbolic";
                status_text_nice = "En Attente";
                break;
            case EN_PREPARATION:
                status_color_css = "#f39c12";
                status_icon_name = "media-playlist-shuffle-symbolic";
                status_text_nice = "En Cuisine";
                break;
            case SERVIE:
                status_color_css = "#2ecc71";
                status_icon_name = "emblem-default-symbolic";
                status_text_nice = "Servie";
                break;
            default:
                status_color_css = "#95a5a6";
                status_icon_name = "dialog-question-symbolic";
                status_text_nice = "Inconnu";
        }
        

        char indicator_css[128];
        snprintf(indicator_css, sizeof(indicator_css), "widget { background-color: %s; border-radius: 3px; }", status_color_css);
        GtkCssProvider *ind_provider = gtk_css_provider_new();
        gtk_css_provider_load_from_data(ind_provider, indicator_css, -1, NULL);
        gtk_style_context_add_provider(
            gtk_widget_get_style_context(status_indicator),
            GTK_STYLE_PROVIDER(ind_provider),
            GTK_STYLE_PROVIDER_PRIORITY_APPLICATION
        );
        g_object_unref(ind_provider);
        gtk_box_pack_start(GTK_BOX(row_box), status_indicator, FALSE, FALSE, 0);
        

        GtkWidget *info_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
        
        char id_str[32];
        snprintf(id_str, sizeof(id_str), "Commande #%d", cmd->id);
        GtkWidget *id_label = gtk_label_new(NULL);
        char *id_markup = g_markup_printf_escaped("<span weight='bold' size='large'>%s</span>", id_str);
        gtk_label_set_markup(GTK_LABEL(id_label), id_markup);
        g_free(id_markup);
        gtk_widget_set_halign(id_label, GTK_ALIGN_START);
        
        char client_str[64];
        snprintf(client_str, sizeof(client_str), "Client #%d", cmd->client_id);
        GtkWidget *client_label = gtk_label_new(NULL);
        char *client_markup = g_markup_printf_escaped("<span size='small' foreground='#7f8c8d'>%s</span>", client_str);
        gtk_label_set_markup(GTK_LABEL(client_label), client_markup);
        g_free(client_markup);
        gtk_widget_set_halign(client_label, GTK_ALIGN_START);
        
        gtk_box_pack_start(GTK_BOX(info_box), id_label, FALSE, FALSE, 0);
        gtk_box_pack_start(GTK_BOX(info_box), client_label, FALSE, FALSE, 0);
        gtk_box_pack_start(GTK_BOX(row_box), info_box, TRUE, TRUE, 0);
        

        GtkWidget *status_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
        GtkWidget *s_icon = gtk_image_new_from_icon_name(status_icon_name, GTK_ICON_SIZE_BUTTON);
        GtkWidget *s_label = gtk_label_new(NULL);
        char *s_markup = g_markup_printf_escaped("<span weight='bold' foreground='%s'>%s</span>", status_color_css, status_text_nice);
        gtk_label_set_markup(GTK_LABEL(s_label), s_markup);
        g_free(s_markup);
        
        gtk_box_pack_start(GTK_BOX(status_box), s_icon, FALSE, FALSE, 0);
        gtk_box_pack_start(GTK_BOX(status_box), s_label, FALSE, FALSE, 0);
        gtk_box_pack_start(GTK_BOX(row_box), status_box, FALSE, FALSE, 10);
        

        GtkWidget *meta_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
        
        char total_str[32];
        snprintf(total_str, sizeof(total_str), "%.2f €", cmd->total);
        GtkWidget *total_label = gtk_label_new(NULL);
        char *total_markup = g_markup_printf_escaped("<span weight='bold' size='large' foreground='#2c3e50'>%s</span>", total_str);
        gtk_label_set_markup(GTK_LABEL(total_label), total_markup);
        g_free(total_markup);
        gtk_widget_set_halign(total_label, GTK_ALIGN_END);
        
        struct tm *order_time = localtime(&cmd->timestamp);
        char time_str[32];
        strftime(time_str, sizeof(time_str), "%H:%M", order_time);
        GtkWidget *time_label = gtk_label_new(NULL);
        char *time_markup = g_markup_printf_escaped("<span size='small' foreground='#95a5a6'>%s</span>", time_str);
        gtk_label_set_markup(GTK_LABEL(time_label), time_markup);
        g_free(time_markup);
        gtk_widget_set_halign(time_label, GTK_ALIGN_END);
        
        gtk_box_pack_start(GTK_BOX(meta_box), total_label, FALSE, FALSE, 0);
        gtk_box_pack_start(GTK_BOX(meta_box), time_label, FALSE, FALSE, 0);
        gtk_box_pack_start(GTK_BOX(row_box), meta_box, FALSE, FALSE, 10);
        

        GtkWidget *arrow_btn = gtk_button_new_from_icon_name("go-next-symbolic", GTK_ICON_SIZE_BUTTON);
        gtk_widget_set_opacity(arrow_btn, 0.5);
        int *id_ptr = g_malloc(sizeof(int));
        *id_ptr = cmd->id;
        g_signal_connect(arrow_btn, "clicked", G_CALLBACK(on_order_details_clicked), id_ptr);
        gtk_box_pack_start(GTK_BOX(row_box), arrow_btn, FALSE, FALSE, 0);
        
        gtk_container_add(GTK_CONTAINER(orders_list), row_box);
        
        row_count++;
        cmd = cmd->next;
    }
    

    if (row_count == 0) {
        GtkWidget *empty_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
        gtk_widget_set_margin_top(empty_box, 20);
        gtk_widget_set_margin_bottom(empty_box, 20);
        
        GtkWidget *empty_icon = gtk_image_new_from_icon_name("mail-archive-symbolic", GTK_ICON_SIZE_DIALOG);
        gtk_image_set_pixel_size(GTK_IMAGE(empty_icon), 64);
        gtk_widget_set_opacity(empty_icon, 0.3);
        
        GtkWidget *empty_label = gtk_label_new(NULL);
        gtk_label_set_markup(GTK_LABEL(empty_label), "<span foreground='#bdc3c7' size='large'>Aucune commande récente</span>");
        
        gtk_box_pack_start(GTK_BOX(empty_box), empty_icon, FALSE, FALSE, 0);
        gtk_box_pack_start(GTK_BOX(empty_box), empty_label, FALSE, FALSE, 0);
        gtk_container_add(GTK_CONTAINER(orders_list), empty_box);
    }
    
    gtk_box_pack_start(GTK_BOX(recent_box), orders_list, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(dashboard_box), recent_box, FALSE, FALSE, 0);
    

    gtk_container_add(GTK_CONTAINER(content_area), dashboard_box);
    gtk_widget_show_all(content_area);
}


static void on_order_details_clicked(GtkButton *button, gpointer user_data) {
    int *id_ptr = (int*)user_data;
    int order_id = *id_ptr;
    
    Commande *cmd = find_commande_by_id(NULL, order_id);
    if(!cmd) {
        g_free(id_ptr);
        return;
    }
    

    GtkWidget *dialog = gtk_dialog_new_with_buttons(
        "Détails de la Commande",
        GTK_WINDOW(main_window),
        GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
        "Fermer", GTK_RESPONSE_CLOSE,
        NULL
    );
    
    gtk_window_set_default_size(GTK_WINDOW(dialog), 500, 400);
    
    GtkWidget *content = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
    gtk_container_set_border_width(GTK_CONTAINER(content), 20);
    
    GtkWidget *main_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 15);
    

    char header_text[128];
    snprintf(header_text, sizeof(header_text), "Commande #%d", cmd->id);
    GtkWidget *header_label = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(header_label), 
        g_markup_printf_escaped("<span size='x-large' weight='bold'>%s</span>", header_text));
    gtk_widget_set_halign(header_label, GTK_ALIGN_START);
    gtk_box_pack_start(GTK_BOX(main_box), header_label, FALSE, FALSE, 0);
    

    const char *status_text;
    switch(cmd->statut) {
        case EN_ATTENTE: status_text = "⏰ En Attente"; break;
        case EN_PREPARATION: status_text = "👨‍🍳 En Préparation"; break;
        case SERVIE: status_text = "✅ Servie"; break;
        default: status_text = "❌ Annulée";
    }
    GtkWidget *status_label = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(status_label), 
        g_markup_printf_escaped("<span size='large'>%s</span>", status_text));
    gtk_widget_set_halign(status_label, GTK_ALIGN_START);
    gtk_box_pack_start(GTK_BOX(main_box), status_label, FALSE, FALSE, 0);
    

    char client_text[128];
    snprintf(client_text, sizeof(client_text), "👤 Client #%d", cmd->client_id);
    GtkWidget *client_label = gtk_label_new(client_text);
    gtk_widget_set_halign(client_label, GTK_ALIGN_START);
    gtk_box_pack_start(GTK_BOX(main_box), client_label, FALSE, FALSE, 0);
    

    char date_text[128];
    time_t t = cmd->timestamp;
    struct tm *tm = localtime(&t);
    strftime(date_text, sizeof(date_text), "🕐 %d/%m/%Y à %H:%M", tm);
    GtkWidget *date_label = gtk_label_new(date_text);
    gtk_widget_set_halign(date_label, GTK_ALIGN_START);
    gtk_box_pack_start(GTK_BOX(main_box), date_label, FALSE, FALSE, 0);
    

    GtkWidget *sep1 = gtk_separator_new(GTK_ORIENTATION_HORIZONTAL);
    gtk_box_pack_start(GTK_BOX(main_box), sep1, FALSE, FALSE, 5);
    

    GtkWidget *items_header = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(items_header), 
        "<span weight='bold' size='large'>Articles commandés:</span>");
    gtk_widget_set_halign(items_header, GTK_ALIGN_START);
    gtk_box_pack_start(GTK_BOX(main_box), items_header, FALSE, FALSE, 0);
    

    GtkWidget *items_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    gtk_widget_set_margin_start(items_box, 15);
    
    Ligne *ligne = cmd->lignes;
    while(ligne) {

        Plat *plat = find_plat_by_id(NULL, ligne->plat_id);
        char item_text[256];
        if(plat) {
            snprintf(item_text, sizeof(item_text), 
                "• %s x%d - %.2f€", 
                plat->name, ligne->quantity, ligne->line_total);
        } else {
            snprintf(item_text, sizeof(item_text), 
                "• Plat #%d x%d - %.2f€", 
                ligne->plat_id, ligne->quantity, ligne->line_total);
        }
        
        GtkWidget *item_label = gtk_label_new(item_text);
        gtk_widget_set_halign(item_label, GTK_ALIGN_START);
        gtk_box_pack_start(GTK_BOX(items_box), item_label, FALSE, FALSE, 0);
        
        ligne = ligne->next;
    }
    
    gtk_box_pack_start(GTK_BOX(main_box), items_box, FALSE, FALSE, 0);
    

    GtkWidget *sep2 = gtk_separator_new(GTK_ORIENTATION_HORIZONTAL);
    gtk_box_pack_start(GTK_BOX(main_box), sep2, FALSE, FALSE, 5);
    

    char total_text[64];
    snprintf(total_text, sizeof(total_text), "TOTAL: %.2f €", cmd->total);
    GtkWidget *total_label = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(total_label), 
        g_markup_printf_escaped("<span size='x-large' weight='bold'>%s</span>", total_text));
    gtk_widget_set_halign(total_label, GTK_ALIGN_END);
    gtk_box_pack_start(GTK_BOX(main_box), total_label, FALSE, FALSE, 0);
    
    gtk_container_add(GTK_CONTAINER(content), main_box);
    gtk_widget_show_all(dialog);
    
    gtk_dialog_run(GTK_DIALOG(dialog));
    gtk_widget_destroy(dialog);
    
    g_free(id_ptr);
}

static void show_orders_content() {
    gtk_container_foreach(GTK_CONTAINER(content_area), (GtkCallback)gtk_widget_destroy, NULL);
    

    GtkWidget *orders_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 20);
    gtk_widget_set_margin_start(orders_box, 25);
    gtk_widget_set_margin_end(orders_box, 25);
    gtk_widget_set_margin_top(orders_box, 20);
    gtk_widget_set_margin_bottom(orders_box, 20);
    

    GtkWidget *header_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 15);
    

    GtkWidget *header_icon = gtk_image_new_from_icon_name("emblem-documents", GTK_ICON_SIZE_DIALOG);
    gtk_image_set_pixel_size(GTK_IMAGE(header_icon), 48);
    gtk_box_pack_start(GTK_BOX(header_box), header_icon, FALSE, FALSE, 0);
    

    GtkWidget *header = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(header), 
        "<span size='xx-large' weight='bold' foreground='#2C3E50'>📋 Gestion des Commandes</span>");
    gtk_widget_set_halign(header, GTK_ALIGN_START);
    gtk_box_pack_start(GTK_BOX(header_box), header, FALSE, FALSE, 0);
    
    gtk_box_pack_start(GTK_BOX(orders_box), header_box, FALSE, FALSE, 0);
    

    GtkWidget *stats_container = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 20);
    gtk_widget_set_margin_top(stats_container, 15);
    gtk_widget_set_margin_bottom(stats_container, 15);
    

    int total = 0, waiting = 0, preparing = 0, served = 0;
    Commande *count_cmd = get_commande_list();
    while(count_cmd) {
        total++;
        switch(count_cmd->statut) {
            case EN_ATTENTE: waiting++; break;
            case EN_PREPARATION: preparing++; break;
            case SERVIE: served++; break;
            default: break;
        }
        count_cmd = count_cmd->next;
    }
    

    const char *stat_icons[] = {"📊", "⏰", "👨‍🍳", "✅"};
    const char *stat_labels[] = {"Total Commandes", "En Attente", "En Préparation", "Servies"};
    int stat_values[] = {total, waiting, preparing, served};
    const char *stat_colors[] = {"#667eea", "#f5576c", "#00f2fe", "#38f9d7"};
    
    for(int i = 0; i < 4; i++) {
        GtkWidget *stat_card = gtk_frame_new(NULL);
        gtk_frame_set_shadow_type(GTK_FRAME(stat_card), GTK_SHADOW_NONE);
        
        GtkWidget *card_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
        gtk_widget_set_size_request(card_box, 200, 120);
        

        char card_css[512];
        snprintf(card_css, sizeof(card_css),
            "frame { "
            "  background: %s; "
            "  border-radius: 12px; "
            "  box-shadow: 0 4px 15px rgba(0,0,0,0.1); "
            "  padding: 20px; "
            "} "
            "box { background: transparent; } "
            "label { color: white; font-weight: bold; }",
            stat_colors[i]);
        
        GtkCssProvider *card_provider = gtk_css_provider_new();
        gtk_css_provider_load_from_data(card_provider, card_css, -1, NULL);
        gtk_style_context_add_provider(
            gtk_widget_get_style_context(stat_card),
            GTK_STYLE_PROVIDER(card_provider),
            GTK_STYLE_PROVIDER_PRIORITY_APPLICATION
        );
        

        GtkWidget *icon_label = gtk_label_new(NULL);
        gtk_label_set_markup(GTK_LABEL(icon_label), 
            g_markup_printf_escaped("<span size='x-large'>%s</span>", stat_icons[i]));
        gtk_widget_set_halign(icon_label, GTK_ALIGN_START);
        gtk_box_pack_start(GTK_BOX(card_box), icon_label, FALSE, FALSE, 0);
        

        char value_markup[128];
        snprintf(value_markup, sizeof(value_markup), 
            "<span size='28000' weight='bold'>%d</span>", stat_values[i]);
        GtkWidget *value_label = gtk_label_new(NULL);
        gtk_label_set_markup(GTK_LABEL(value_label), value_markup);
        gtk_widget_set_halign(value_label, GTK_ALIGN_START);
        gtk_box_pack_start(GTK_BOX(card_box), value_label, FALSE, FALSE, 0);
        

        GtkWidget *desc_label = gtk_label_new(NULL);
        gtk_label_set_markup(GTK_LABEL(desc_label), 
            g_markup_printf_escaped("<span size='small'>%s</span>", stat_labels[i]));
        gtk_widget_set_halign(desc_label, GTK_ALIGN_START);
        gtk_box_pack_start(GTK_BOX(card_box), desc_label, FALSE, FALSE, 0);
        
        gtk_container_add(GTK_CONTAINER(stat_card), card_box);
        gtk_box_pack_start(GTK_BOX(stats_container), stat_card, TRUE, TRUE, 0);
        
        g_object_unref(card_provider);
    }
    
    gtk_box_pack_start(GTK_BOX(orders_box), stats_container, FALSE, FALSE, 0);
    

    GtkWidget *action_bar = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    gtk_widget_set_margin_top(action_bar, 10);
    gtk_widget_set_margin_bottom(action_bar, 15);
    
    GtkWidget *new_order_btn = gtk_button_new_with_label("➕ Nouvelle Commande");
    gtk_widget_set_size_request(new_order_btn, 220, 45);
    

    GtkCssProvider *btn_provider = gtk_css_provider_new();
    gtk_css_provider_load_from_data(btn_provider,
        "button { "
        "  background: linear-gradient(135deg, #667eea 0%, #764ba2 100%); "
        "  color: white; "
        "  border-radius: 8px; "
        "  border: none; "
        "  font-size: 14px; "
        "  font-weight: bold; "
        "  box-shadow: 0 4px 12px rgba(102, 126, 234, 0.4); "
        "} "
        "button:hover { "
        "  box-shadow: 0 6px 20px rgba(102, 126, 234, 0.6); "
        "}", -1, NULL);
    gtk_style_context_add_provider(
        gtk_widget_get_style_context(new_order_btn),
        GTK_STYLE_PROVIDER(btn_provider),
        GTK_STYLE_PROVIDER_PRIORITY_APPLICATION
    );
    
    g_signal_connect(new_order_btn, "clicked", G_CALLBACK(on_add_order_clicked), NULL);
    gtk_box_pack_start(GTK_BOX(action_bar), new_order_btn, FALSE, FALSE, 0);
    g_object_unref(btn_provider);
    
    gtk_box_pack_start(GTK_BOX(orders_box), action_bar, FALSE, FALSE, 0);
    

    GtkWidget *scrolled = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled),
                                   GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_widget_set_vexpand(scrolled, TRUE);
    gtk_widget_set_hexpand(scrolled, TRUE);
    

    GtkWidget *grid = gtk_grid_new();
    gtk_grid_set_row_spacing(GTK_GRID(grid), 20);
    gtk_grid_set_column_spacing(GTK_GRID(grid), 20);
    gtk_widget_set_margin_start(grid, 5);
    gtk_widget_set_margin_end(grid, 5);
    

    Commande *cmd = get_commande_list();
    int row = 0, col = 0;
    
    while(cmd) {

        GtkWidget *card = gtk_frame_new(NULL);
        gtk_frame_set_shadow_type(GTK_FRAME(card), GTK_SHADOW_NONE);
        gtk_widget_set_size_request(card, 450, 280);
        

        GtkWidget *card_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
        

        const char *header_color;
        const char *status_icon;
        const char *status_text;
        
        switch(cmd->statut) {
            case EN_ATTENTE:
                header_color = "#FF6B35";
                status_icon = "⏰";
                status_text = "EN ATTENTE";
                break;
            case EN_PREPARATION:
                header_color = "#4A90E2";
                status_icon = "👨‍🍳";
                status_text = "EN PRÉPARATION";
                break;
            case SERVIE:
                header_color = "#27AE60";
                status_icon = "✅";
                status_text = "SERVIE";
                break;
            default:
                header_color = "#95a5a6";
                status_icon = "❌";
                status_text = "ANNULÉE";
        }
        

        GtkWidget *card_header = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
        gtk_widget_set_size_request(card_header, -1, 60);
        
        char header_css[512];
        snprintf(header_css, sizeof(header_css),
            "box { "
            "  background: %s; "
            "  padding: 15px; "
            "  border-radius: 12px 12px 0 0; "
            "} "
            "label { color: white; font-weight: bold; }",
            header_color);
        
        GtkCssProvider *header_provider = gtk_css_provider_new();
        gtk_css_provider_load_from_data(header_provider, header_css, -1, NULL);
        gtk_style_context_add_provider(
            gtk_widget_get_style_context(card_header),
            GTK_STYLE_PROVIDER(header_provider),
            GTK_STYLE_PROVIDER_PRIORITY_APPLICATION
        );
        

        char id_markup[64];
        snprintf(id_markup, sizeof(id_markup), 
            "<span size='xx-large' weight='bold'>#%d</span>", cmd->id);
        GtkWidget *id_label = gtk_label_new(NULL);
        gtk_label_set_markup(GTK_LABEL(id_label), id_markup);
        gtk_box_pack_start(GTK_BOX(card_header), id_label, FALSE, FALSE, 0);
        

        GtkWidget *spacer = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
        gtk_widget_set_hexpand(spacer, TRUE);
        gtk_box_pack_start(GTK_BOX(card_header), spacer, TRUE, TRUE, 0);
        

        char status_markup[128];
        snprintf(status_markup, sizeof(status_markup), 
            "<span size='small'>%s %s</span>", status_icon, status_text);
        GtkWidget *status_label = gtk_label_new(NULL);
        gtk_label_set_markup(GTK_LABEL(status_label), status_markup);
        gtk_box_pack_start(GTK_BOX(card_header), status_label, FALSE, FALSE, 0);
        
        gtk_box_pack_start(GTK_BOX(card_box), card_header, FALSE, FALSE, 0);
        g_object_unref(header_provider);
        

        GtkWidget *card_body = gtk_box_new(GTK_ORIENTATION_VERTICAL, 12);
        gtk_widget_set_margin_start(card_body, 20);
        gtk_widget_set_margin_end(card_body, 20);
        gtk_widget_set_margin_top(card_body, 15);
        gtk_widget_set_margin_bottom(card_body, 15);
        

        char client_text[128];
        snprintf(client_text, sizeof(client_text), "👤 Client #%d", cmd->client_id);
        GtkWidget *client_label = gtk_label_new(NULL);
        gtk_label_set_markup(GTK_LABEL(client_label), 
            g_markup_printf_escaped("<span size='large' weight='bold'>%s</span>", client_text));
        gtk_widget_set_halign(client_label, GTK_ALIGN_START);
        gtk_box_pack_start(GTK_BOX(card_body), client_label, FALSE, FALSE, 0);
        

        char date_str[128];
        time_t t = cmd->timestamp;
        struct tm *tm = localtime(&t);
        strftime(date_str, sizeof(date_str), "🕐 %d/%m/%Y à %H:%M", tm);
        GtkWidget *date_label = gtk_label_new(NULL);
        gtk_label_set_markup(GTK_LABEL(date_label), 
            g_markup_printf_escaped("<span foreground='#7f8c8d'>%s</span>", date_str));
        gtk_widget_set_halign(date_label, GTK_ALIGN_START);
        gtk_box_pack_start(GTK_BOX(card_body), date_label, FALSE, FALSE, 0);
        

        int item_count = 0;
        Ligne *ligne = cmd->lignes;
        while(ligne) {
            item_count += ligne->quantity;
            ligne = ligne->next;
        }
        
        char items_text[64];
        snprintf(items_text, sizeof(items_text), "🍽️ %d article%s", item_count, item_count > 1 ? "s" : "");
        GtkWidget *items_label = gtk_label_new(NULL);
        gtk_label_set_markup(GTK_LABEL(items_label), 
            g_markup_printf_escaped("<span foreground='#7f8c8d'>%s</span>", items_text));
        gtk_widget_set_halign(items_label, GTK_ALIGN_START);
        gtk_box_pack_start(GTK_BOX(card_body), items_label, FALSE, FALSE, 0);
        

        GtkWidget *separator = gtk_separator_new(GTK_ORIENTATION_HORIZONTAL);
        gtk_box_pack_start(GTK_BOX(card_body), separator, FALSE, FALSE, 5);
        

        char total_markup[128];
        snprintf(total_markup, sizeof(total_markup), 
            "<span size='x-large' weight='bold' foreground='#2C3E50'>%.2f €</span>", cmd->total);
        GtkWidget *total_label = gtk_label_new(NULL);
        gtk_label_set_markup(GTK_LABEL(total_label), total_markup);
        gtk_widget_set_halign(total_label, GTK_ALIGN_START);
        gtk_box_pack_start(GTK_BOX(card_body), total_label, FALSE, FALSE, 0);
        

        GtkWidget *actions = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
        gtk_widget_set_margin_top(actions, 10);
        

        GtkWidget *details_btn = gtk_button_new_with_label("📄 Détails");
        gtk_widget_set_size_request(details_btn, 100, 35);
        GtkCssProvider *details_provider = gtk_css_provider_new();
        gtk_css_provider_load_from_data(details_provider,
            "button { "
            "  background: #ecf0f1; "
            "  color: #2c3e50; "
            "  border-radius: 6px; "
            "  border: 1px solid #bdc3c7; "
            "  font-weight: bold; "
            "} "
            "button:hover { background: #bdc3c7; }", -1, NULL);
        gtk_style_context_add_provider(
            gtk_widget_get_style_context(details_btn),
            GTK_STYLE_PROVIDER(details_provider),
            GTK_STYLE_PROVIDER_PRIORITY_APPLICATION
        );
        gtk_box_pack_start(GTK_BOX(actions), details_btn, FALSE, FALSE, 0);
        

        int *details_id = malloc(sizeof(int));
        *details_id = cmd->id;
        g_signal_connect(details_btn, "clicked", G_CALLBACK(on_order_details_clicked), details_id);
        
        g_object_unref(details_provider);
        

        if(cmd->statut == EN_ATTENTE || cmd->statut == EN_PREPARATION) {
            const char *btn_text = cmd->statut == EN_ATTENTE ? "▶️ Préparer" : "✅ Servir";
            const char *btn_color = cmd->statut == EN_ATTENTE ? "#4A90E2" : "#27AE60";
            
            GtkWidget *update_btn = gtk_button_new_with_label(btn_text);
            gtk_widget_set_size_request(update_btn, 120, 35);
            
            char btn_css[256];
            snprintf(btn_css, sizeof(btn_css),
                "button { "
                "  background: %s; "
                "  color: white; "
                "  border-radius: 6px; "
                "  border: none; "
                "  font-weight: bold; "
                "} "
                "button:hover { opacity: 0.9; }", btn_color);
            
            GtkCssProvider *update_provider = gtk_css_provider_new();
            gtk_css_provider_load_from_data(update_provider, btn_css, -1, NULL);
            gtk_style_context_add_provider(
                gtk_widget_get_style_context(update_btn),
                GTK_STYLE_PROVIDER(update_provider),
                GTK_STYLE_PROVIDER_PRIORITY_APPLICATION
            );
            
            int *id_ptr = malloc(sizeof(int));
            *id_ptr = cmd->id;
            g_signal_connect(update_btn, "clicked", G_CALLBACK(on_update_order_status), id_ptr);
            
            gtk_box_pack_start(GTK_BOX(actions), update_btn, FALSE, FALSE, 0);
            g_object_unref(update_provider);
        }
        
        gtk_box_pack_start(GTK_BOX(card_body), actions, FALSE, FALSE, 0);
        gtk_box_pack_start(GTK_BOX(card_box), card_body, TRUE, TRUE, 0);
        

        GtkCssProvider *card_provider = gtk_css_provider_new();
        gtk_css_provider_load_from_data(card_provider,
            "frame { "
            "  background: white; "
            "  border-radius: 12px; "
            "  box-shadow: 0 2px 10px rgba(0,0,0,0.08); "
            "  border: 1px solid #e0e0e0; "
            "} "
            "frame:hover { "
            "  box-shadow: 0 4px 20px rgba(0,0,0,0.12); "
            "  transform: translateY(-2px); "
            "}", -1, NULL);
        gtk_style_context_add_provider(
            gtk_widget_get_style_context(card),
            GTK_STYLE_PROVIDER(card_provider),
            GTK_STYLE_PROVIDER_PRIORITY_APPLICATION
        );
        
        gtk_container_add(GTK_CONTAINER(card), card_box);
        gtk_grid_attach(GTK_GRID(grid), card, col, row, 1, 1);
        
        g_object_unref(card_provider);
        

        col++;
        if(col >= 2) {
            col = 0;
            row++;
        }
        
        cmd = cmd->next;
    }
    
    gtk_container_add(GTK_CONTAINER(scrolled), grid);
    gtk_box_pack_start(GTK_BOX(orders_box), scrolled, TRUE, TRUE, 0);
    
    gtk_container_add(GTK_CONTAINER(content_area), orders_box);
    gtk_widget_show_all(content_area);
}

static void show_menu_content() {
    gtk_container_foreach(GTK_CONTAINER(content_area), (GtkCallback)gtk_widget_destroy, NULL);


    GtkWidget *menu_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 20);
    gtk_widget_set_margin_start(menu_box, 25);
    gtk_widget_set_margin_end(menu_box, 25);
    gtk_widget_set_margin_top(menu_box, 20);
    gtk_widget_set_margin_bottom(menu_box, 20);
    

    GtkWidget *header_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 15);
    

    GtkWidget *header_icon = gtk_image_new_from_icon_name("emblem-photos", GTK_ICON_SIZE_DIALOG);
    gtk_image_set_pixel_size(GTK_IMAGE(header_icon), 48);
    gtk_box_pack_start(GTK_BOX(header_box), header_icon, FALSE, FALSE, 0);
    

    GtkWidget *header = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(header), 
        "<span size='xx-large' weight='bold' foreground='#2C3E50'>🍽️ Gestion du Menu</span>");
    gtk_widget_set_halign(header, GTK_ALIGN_START);
    gtk_box_pack_start(GTK_BOX(header_box), header, FALSE, FALSE, 0);
    
    gtk_box_pack_start(GTK_BOX(menu_box), header_box, FALSE, FALSE, 0);
    

    GtkWidget *stats_container = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 20);
    gtk_widget_set_margin_top(stats_container, 15);
    gtk_widget_set_margin_bottom(stats_container, 15);
    

    int total = 0, available = 0, unavailable = 0;
    Plat *count_plat = get_plat_list();
    while(count_plat) {
        total++;
        if(count_plat->available) available++;
        else unavailable++;
        count_plat = count_plat->next;
    }
    

    const char *stat_icons[] = {"📊", "✅", "❌"};
    const char *stat_labels[] = {"Total Plats", "Disponibles", "Indisponibles"};
    int stat_values[] = {total, available, unavailable};
    const char *stat_colors[] = {"#9b59b6", "#27ae60", "#e74c3c"};
    
    for(int i = 0; i < 3; i++) {
        GtkWidget *stat_card = gtk_frame_new(NULL);
        gtk_frame_set_shadow_type(GTK_FRAME(stat_card), GTK_SHADOW_NONE);
        
        GtkWidget *card_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
        gtk_widget_set_size_request(card_box, 220, 120);
        

        char card_css[512];
        snprintf(card_css, sizeof(card_css),
            "frame { "
            "  background: %s; "
            "  border-radius: 12px; "
            "  box-shadow: 0 4px 15px rgba(0,0,0,0.1); "
            "  padding: 20px; "
            "} "
            "box { background: transparent; } "
            "label { color: white; font-weight: bold; }",
            stat_colors[i]);
        
        GtkCssProvider *card_provider = gtk_css_provider_new();
        gtk_css_provider_load_from_data(card_provider, card_css, -1, NULL);
        gtk_style_context_add_provider(
            gtk_widget_get_style_context(stat_card),
            GTK_STYLE_PROVIDER(card_provider),
            GTK_STYLE_PROVIDER_PRIORITY_APPLICATION
        );
        

        GtkWidget *icon_label = gtk_label_new(NULL);
        gtk_label_set_markup(GTK_LABEL(icon_label), 
            g_markup_printf_escaped("<span size='x-large'>%s</span>", stat_icons[i]));
        gtk_widget_set_halign(icon_label, GTK_ALIGN_START);
        gtk_box_pack_start(GTK_BOX(card_box), icon_label, FALSE, FALSE, 0);
        

        char value_markup[128];
        snprintf(value_markup, sizeof(value_markup), 
            "<span size='28000' weight='bold'>%d</span>", stat_values[i]);
        GtkWidget *value_label = gtk_label_new(NULL);
        gtk_label_set_markup(GTK_LABEL(value_label), value_markup);
        gtk_widget_set_halign(value_label, GTK_ALIGN_START);
        gtk_box_pack_start(GTK_BOX(card_box), value_label, FALSE, FALSE, 0);
        

        GtkWidget *desc_label = gtk_label_new(NULL);
        gtk_label_set_markup(GTK_LABEL(desc_label), 
            g_markup_printf_escaped("<span size='small'>%s</span>", stat_labels[i]));
        gtk_widget_set_halign(desc_label, GTK_ALIGN_START);
        gtk_box_pack_start(GTK_BOX(card_box), desc_label, FALSE, FALSE, 0);
        
        gtk_container_add(GTK_CONTAINER(stat_card), card_box);
        gtk_box_pack_start(GTK_BOX(stats_container), stat_card, TRUE, TRUE, 0);
        
        g_object_unref(card_provider);
    }
    
    gtk_box_pack_start(GTK_BOX(menu_box), stats_container, FALSE, FALSE, 0);
    

    GtkWidget *action_bar = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    gtk_widget_set_margin_top(action_bar, 10);
    gtk_widget_set_margin_bottom(action_bar, 15);
    
    GtkWidget *add_dish_btn = gtk_button_new_with_label("➕ Ajouter un Plat");
    gtk_widget_set_size_request(add_dish_btn, 220, 45);
    

    GtkCssProvider *btn_provider = gtk_css_provider_new();
    gtk_css_provider_load_from_data(btn_provider,
        "button { "
        "  background: linear-gradient(135deg, #27ae60 0%, #229954 100%); "
        "  color: white; "
        "  border-radius: 8px; "
        "  border: none; "
        "  font-size: 14px; "
        "  font-weight: bold; "
        "  box-shadow: 0 4px 12px rgba(39, 174, 96, 0.4); "
        "} "
        "button:hover { "
        "  box-shadow: 0 6px 20px rgba(39, 174, 96, 0.6); "
        "}", -1, NULL);
    gtk_style_context_add_provider(
        gtk_widget_get_style_context(add_dish_btn),
        GTK_STYLE_PROVIDER(btn_provider),
        GTK_STYLE_PROVIDER_PRIORITY_APPLICATION
    );
    
    g_signal_connect(add_dish_btn, "clicked", G_CALLBACK(on_add_dish_clicked), NULL);
    gtk_box_pack_start(GTK_BOX(action_bar), add_dish_btn, FALSE, FALSE, 0);
    g_object_unref(btn_provider);
    
    gtk_box_pack_start(GTK_BOX(menu_box), action_bar, FALSE, FALSE, 0);
    

    GtkWidget *scrolled = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled),
                                   GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_widget_set_vexpand(scrolled, TRUE);
    gtk_widget_set_hexpand(scrolled, TRUE);
    

    GtkWidget *grid = gtk_grid_new();
    gtk_grid_set_row_spacing(GTK_GRID(grid), 20);
    gtk_grid_set_column_spacing(GTK_GRID(grid), 20);
    gtk_widget_set_margin_start(grid, 5);
    gtk_widget_set_margin_end(grid, 5);
    

    Plat *plat = get_plat_list();
    int row = 0, col = 0;
    
    while(plat) {

        GtkWidget *card = gtk_frame_new(NULL);
        gtk_frame_set_shadow_type(GTK_FRAME(card), GTK_SHADOW_NONE);
        gtk_widget_set_size_request(card, 320, 240);
        

        GtkWidget *card_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
        

        const char *category_color;
        const char *category_emoji;
        
        if(strstr(plat->category, "Entrée") || strstr(plat->category, "Entree")) {
            category_color = "#3498db";
            category_emoji = "🥗";
        } else if(strstr(plat->category, "Plat") || strstr(plat->category, "Principal")) {
            category_color = "#e67e22";
            category_emoji = "🍖";
        } else if(strstr(plat->category, "Dessert")) {
            category_color = "#e91e63";
            category_emoji = "🍰";
        } else if(strstr(plat->category, "Boisson")) {
            category_color = "#9c27b0";
            category_emoji = "🥤";
        } else {
            category_color = "#95a5a6";
            category_emoji = "🍽️";
        }
        

        GtkWidget *card_header = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
        gtk_widget_set_size_request(card_header, -1, 50);
        
        char header_css[512];
        snprintf(header_css, sizeof(header_css),
            "box { "
            "  background: %s; "
            "  padding: 12px; "
            "  border-radius: 12px 12px 0 0; "
            "} "
            "label { color: white; font-weight: bold; }",
            category_color);
        
        GtkCssProvider *header_provider = gtk_css_provider_new();
        gtk_css_provider_load_from_data(header_provider, header_css, -1, NULL);
        gtk_style_context_add_provider(
            gtk_widget_get_style_context(card_header),
            GTK_STYLE_PROVIDER(header_provider),
            GTK_STYLE_PROVIDER_PRIORITY_APPLICATION
        );
        

        char category_markup[128];
        snprintf(category_markup, sizeof(category_markup), 
            "<span>%s %s</span>", category_emoji, plat->category);
        GtkWidget *category_label = gtk_label_new(NULL);
        gtk_label_set_markup(GTK_LABEL(category_label), category_markup);
        gtk_box_pack_start(GTK_BOX(card_header), category_label, FALSE, FALSE, 0);
        

        GtkWidget *spacer = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
        gtk_widget_set_hexpand(spacer, TRUE);
        gtk_box_pack_start(GTK_BOX(card_header), spacer, TRUE, TRUE, 0);
        

        const char *avail_text = plat->available ? "✅ Dispo" : "❌ Indispo";
        GtkWidget *avail_label = gtk_label_new(NULL);
        gtk_label_set_markup(GTK_LABEL(avail_label), 
            g_markup_printf_escaped("<span size='small'>%s</span>", avail_text));
        gtk_box_pack_start(GTK_BOX(card_header), avail_label, FALSE, FALSE, 0);
        
        gtk_box_pack_start(GTK_BOX(card_box), card_header, FALSE, FALSE, 0);
        g_object_unref(header_provider);
        

        GtkWidget *card_body = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
        gtk_widget_set_margin_start(card_body, 15);
        gtk_widget_set_margin_end(card_body, 15);
        gtk_widget_set_margin_top(card_body, 12);
        gtk_widget_set_margin_bottom(card_body, 12);
        

        GtkWidget *name_label = gtk_label_new(NULL);
        gtk_label_set_markup(GTK_LABEL(name_label), 
            g_markup_printf_escaped("<span size='large' weight='bold'>%s</span>", plat->name));
        gtk_widget_set_halign(name_label, GTK_ALIGN_START);
        gtk_label_set_line_wrap(GTK_LABEL(name_label), TRUE);
        gtk_label_set_max_width_chars(GTK_LABEL(name_label), 30);
        gtk_box_pack_start(GTK_BOX(card_body), name_label, FALSE, FALSE, 0);
        

        char id_text[32];
        snprintf(id_text, sizeof(id_text), "ID: #%d", plat->id);
        GtkWidget *id_label = gtk_label_new(NULL);
        gtk_label_set_markup(GTK_LABEL(id_label), 
            g_markup_printf_escaped("<span foreground='#7f8c8d' size='small'>%s</span>", id_text));
        gtk_widget_set_halign(id_label, GTK_ALIGN_START);
        gtk_box_pack_start(GTK_BOX(card_body), id_label, FALSE, FALSE, 0);
        

        GtkWidget *separator = gtk_separator_new(GTK_ORIENTATION_HORIZONTAL);
        gtk_box_pack_start(GTK_BOX(card_body), separator, FALSE, FALSE, 3);
        

        char price_markup[64];
        snprintf(price_markup, sizeof(price_markup), 
            "<span size='x-large' weight='bold' foreground='#27ae60'>%.2f €</span>", plat->price);
        GtkWidget *price_label = gtk_label_new(NULL);
        gtk_label_set_markup(GTK_LABEL(price_label), price_markup);
        gtk_widget_set_halign(price_label, GTK_ALIGN_START);
        gtk_box_pack_start(GTK_BOX(card_body), price_label, FALSE, FALSE, 0);
        

        GtkWidget *actions = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
        gtk_widget_set_margin_top(actions, 8);
        

        const char *toggle_text = plat->available ? "❌ Désactiver" : "✅ Activer";
        const char *toggle_color = plat->available ? "#e74c3c" : "#27ae60";
        
        GtkWidget *toggle_btn = gtk_button_new_with_label(toggle_text);
        gtk_widget_set_size_request(toggle_btn, 120, 32);
        
        char toggle_css[256];
        snprintf(toggle_css, sizeof(toggle_css),
            "button { "
            "  background: %s; "
            "  color: white; "
            "  border-radius: 6px; "
            "  border: none; "
            "  font-weight: bold; "
            "  font-size: 11px; "
            "} "
            "button:hover { opacity: 0.9; }", toggle_color);
        
        GtkCssProvider *toggle_provider = gtk_css_provider_new();
        gtk_css_provider_load_from_data(toggle_provider, toggle_css, -1, NULL);
        gtk_style_context_add_provider(
            gtk_widget_get_style_context(toggle_btn),
            GTK_STYLE_PROVIDER(toggle_provider),
            GTK_STYLE_PROVIDER_PRIORITY_APPLICATION
        );
        
        int *toggle_id = malloc(sizeof(int));
        *toggle_id = plat->id;
        g_signal_connect(toggle_btn, "clicked", G_CALLBACK(on_toggle_plat_available), toggle_id);
        
        gtk_box_pack_start(GTK_BOX(actions), toggle_btn, FALSE, FALSE, 0);
        g_object_unref(toggle_provider);
        

        GtkWidget *delete_btn = gtk_button_new_with_label("🗑️");
        gtk_widget_set_size_request(delete_btn, 40, 32);
        
        GtkCssProvider *delete_provider = gtk_css_provider_new();
        gtk_css_provider_load_from_data(delete_provider,
            "button { "
            "  background: #ecf0f1; "
            "  color: #e74c3c; "
            "  border-radius: 6px; "
            "  border: 1px solid #bdc3c7; "
            "  font-size: 14px; "
            "} "
            "button:hover { background: #e74c3c; color: white; }", -1, NULL);
        gtk_style_context_add_provider(
            gtk_widget_get_style_context(delete_btn),
            GTK_STYLE_PROVIDER(delete_provider),
            GTK_STYLE_PROVIDER_PRIORITY_APPLICATION
        );
        
        int *delete_id = malloc(sizeof(int));
        *delete_id = plat->id;
        g_signal_connect(delete_btn, "clicked", G_CALLBACK(on_delete_plat_clicked), delete_id);
        
        gtk_box_pack_start(GTK_BOX(actions), delete_btn, FALSE, FALSE, 0);
        g_object_unref(delete_provider);
        
        gtk_box_pack_start(GTK_BOX(card_body), actions, FALSE, FALSE, 0);
        gtk_box_pack_start(GTK_BOX(card_box), card_body, TRUE, TRUE, 0);
        
        // Card styling with shadow
        GtkCssProvider *card_provider = gtk_css_provider_new();
        gtk_css_provider_load_from_data(card_provider,
            "frame { "
            "  background: white; "
            "  border-radius: 12px; "
            "  box-shadow: 0 2px 10px rgba(0,0,0,0.08); "
            "  border: 1px solid #e0e0e0; "
            "} "
            "frame:hover { "
            "  box-shadow: 0 4px 20px rgba(0,0,0,0.12); "
            "  transform: translateY(-2px); "
            "}", -1, NULL);
        gtk_style_context_add_provider(
            gtk_widget_get_style_context(card),
            GTK_STYLE_PROVIDER(card_provider),
            GTK_STYLE_PROVIDER_PRIORITY_APPLICATION
        );
        
        gtk_container_add(GTK_CONTAINER(card), card_box);
        gtk_grid_attach(GTK_GRID(grid), card, col, row, 1, 1);
        
        g_object_unref(card_provider);
        

        col++;
        if(col >= 3) {
            col = 0;
            row++;
        }
        
        plat = plat->next;
    }
    
    gtk_container_add(GTK_CONTAINER(scrolled), grid);
    gtk_box_pack_start(GTK_BOX(menu_box), scrolled, TRUE, TRUE, 0);
    
    gtk_container_add(GTK_CONTAINER(content_area), menu_box);
    gtk_widget_show_all(content_area);
}
static void show_clients_content(void);
static void show_stock_content(void);
static void show_personnel_content(void);
static void show_stats_content(void);

static void on_sidebar_button_clicked(GtkButton *button, gpointer user_data) {
    const char *button_label = gtk_button_get_label(button);
    
    if(strcmp(button_label, "Tableau de Bord") == 0) {
        show_dashboard_content();
    }
    else if(strcmp(button_label, "Commandes") == 0) {
        show_orders_content();
    }
    else if(strcmp(button_label, "Menu") == 0) {
        show_menu_content();
    }
    else if(strcmp(button_label, "Tables") == 0) {
        show_tables_content();
    }
    else if(strcmp(button_label, "Clients") == 0) {
        show_clients_content();
    }
    else if(strcmp(button_label, "Stock") == 0) {
        show_stock_content();
    }
    else if(strcmp(button_label, "Personnel") == 0) {
        show_personnel_content();
    }
    else if(strcmp(button_label, "Statistiques") == 0) {
        show_stats_content();
    }
}

void show_main_window(const char *role) {
    strcpy(current_role, role?role:"Employee");
    if(main_window) {
        gtk_widget_show_all(main_window);
        return;
    }
    
    // Load custom CSS
    load_css();
    
    main_window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_default_size(GTK_WINDOW(main_window), 1400, 900);
    gtk_window_maximize(GTK_WINDOW(main_window));
    gtk_window_set_title(GTK_WINDOW(main_window), "Système de Gestion de Restaurant");
    g_signal_connect(main_window, "destroy", G_CALLBACK(gtk_main_quit), NULL);
    

    GtkWidget *outer_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_container_add(GTK_CONTAINER(main_window), outer_box);


    GtkWidget *topbar = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    gtk_style_context_add_class(gtk_widget_get_style_context(topbar), "topbar");
    gtk_widget_set_hexpand(topbar, TRUE);


    GtkWidget *logo = gtk_image_new_from_icon_name("emblem-photos", GTK_ICON_SIZE_DIALOG);
    gtk_box_pack_start(GTK_BOX(topbar), logo, FALSE, FALSE, 8);


    GtkWidget *title = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(title), "<span size='xx-large' weight='bold'>Système de Gestion de Restaurant</span>");
    gtk_box_set_center_widget(GTK_BOX(topbar), title);


    GtkWidget *spacer = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_widget_set_hexpand(spacer, TRUE);
    gtk_box_pack_start(GTK_BOX(topbar), spacer, TRUE, TRUE, 0);


    GtkWidget *profile_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    GtkWidget *avatar = gtk_image_new_from_icon_name("system-users", GTK_ICON_SIZE_BUTTON);
    gtk_box_pack_start(GTK_BOX(profile_box), avatar, FALSE, FALSE, 0);
    GtkWidget *user_label = gtk_label_new(NULL);
    char user_markup[128];
    snprintf(user_markup, sizeof(user_markup), "<span weight='bold'>%s</span>", current_role);
    gtk_label_set_markup(GTK_LABEL(user_label), user_markup);
    gtk_box_pack_start(GTK_BOX(profile_box), user_label, FALSE, FALSE, 0);


    GtkWidget *logout_btn = gtk_button_new_with_label("Se déconnecter");
    gtk_button_set_image(GTK_BUTTON(logout_btn), gtk_image_new_from_icon_name("system-log-out", GTK_ICON_SIZE_BUTTON));
    gtk_button_set_always_show_image(GTK_BUTTON(logout_btn), TRUE);
    gtk_box_pack_start(GTK_BOX(profile_box), logout_btn, FALSE, FALSE, 0);

    gtk_box_pack_end(GTK_BOX(topbar), profile_box, FALSE, FALSE, 8);

    gtk_box_pack_start(GTK_BOX(outer_box), topbar, FALSE, FALSE, 0);


    GtkWidget *main_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_box_pack_start(GTK_BOX(outer_box), main_box, TRUE, TRUE, 0);
    

    GtkWidget *sidebar = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_set_size_request(sidebar, 200, -1);
    gtk_style_context_add_class(gtk_widget_get_style_context(sidebar), "sidebar");
    

    const char *sidebar_items[] = {
        "Tableau de Bord", "Commandes", "Menu", "Tables",
        "Clients", "Stock", "Personnel", "Statistiques"
    };
    
    for(int i = 0; i < 8; i++) {
        GtkWidget *button = gtk_button_new_with_label(sidebar_items[i]);
        gtk_style_context_add_class(gtk_widget_get_style_context(button), "sidebar-button");
        g_signal_connect(button, "clicked", G_CALLBACK(on_sidebar_button_clicked), NULL);
        gtk_box_pack_start(GTK_BOX(sidebar), button, FALSE, FALSE, 0);
    }
    
    gtk_box_pack_start(GTK_BOX(main_box), sidebar, FALSE, FALSE, 0);
    

    content_area = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_box_pack_start(GTK_BOX(main_box), content_area, TRUE, TRUE, 0);
    

    show_dashboard_content();
    
    gtk_widget_show_all(main_window);


    g_signal_connect(logout_btn, "clicked", G_CALLBACK(on_logout_clicked), NULL);
}


static void on_logout_clicked(GtkButton *button, gpointer user_data) {

    g_signal_handlers_disconnect_by_func(main_window, G_CALLBACK(gtk_main_quit), NULL);
    

    gtk_widget_destroy(main_window);
    main_window = NULL;
    

    show_login_window();
}

static GtkWidget *login_window = NULL;
static GtkWidget *user_entry = NULL;
static GtkWidget *pass_entry = NULL;


//
void on_login_clicked(GtkButton *b, gpointer ud){
	const char *user = gtk_entry_get_text(GTK_ENTRY(user_entry));
	const char *pass = gtk_entry_get_text(GTK_ENTRY(pass_entry));
	char role[32];
	if(authenticate(user, pass, role)){
        

		g_signal_handlers_disconnect_by_func(login_window, G_CALLBACK(gtk_main_quit), NULL);

		gtk_widget_destroy(login_window);
		login_window = NULL;
		show_main_window(role);
	} else {
		GtkWidget *d = gtk_message_dialog_new(NULL, GTK_DIALOG_MODAL, GTK_MESSAGE_ERROR, GTK_BUTTONS_OK, "Authentification échouée");
		gtk_dialog_run(GTK_DIALOG(d)); gtk_widget_destroy(d);
	}
}

//

void show_login_window() {
    if(login_window) {
        // Clear the entry fields when showing the login window again
        gtk_entry_set_text(GTK_ENTRY(user_entry), "");
        gtk_entry_set_text(GTK_ENTRY(pass_entry), "");
        gtk_widget_show_all(login_window);
        return;
    }
    
    // Load custom CSS if not already loaded
    load_css();
    
    login_window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(login_window), "Système de Gestion de Restaurant - Connexion");
    gtk_window_set_default_size(GTK_WINDOW(login_window), 400, 300);
    gtk_window_set_position(GTK_WINDOW(login_window), GTK_WIN_POS_CENTER);
    g_signal_connect(login_window, "destroy", G_CALLBACK(gtk_main_quit), NULL);
    
    // Main container with padding
    GtkWidget *main_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 20);
    gtk_container_set_border_width(GTK_CONTAINER(main_box), 30);
    gtk_container_add(GTK_CONTAINER(login_window), main_box);
    gtk_style_context_add_class(gtk_widget_get_style_context(main_box), "login-window");
    
    // Header
    GtkWidget *header = gtk_label_new("Système de Gestion de Restaurant");
    gtk_style_context_add_class(gtk_widget_get_style_context(header), "header");
    gtk_widget_set_halign(header, GTK_ALIGN_CENTER);
    gtk_box_pack_start(GTK_BOX(main_box), header, FALSE, FALSE, 10);
    
    // Form container
    GtkWidget *form_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_box_pack_start(GTK_BOX(main_box), form_box, TRUE, FALSE, 0);
    
    // Username field
    GtkWidget *user_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    GtkWidget *user_label = gtk_label_new("Nom d'utilisateur");
    gtk_widget_set_halign(user_label, GTK_ALIGN_START);
    gtk_box_pack_start(GTK_BOX(user_box), user_label, FALSE, FALSE, 0);
    
    user_entry = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(user_entry), "Entrez votre nom d'utilisateur");
    gtk_box_pack_start(GTK_BOX(user_box), user_entry, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(form_box), user_box, FALSE, FALSE, 0);
    
    // Password field
    GtkWidget *pass_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    GtkWidget *pass_label = gtk_label_new("Mot de passe");
    gtk_widget_set_halign(pass_label, GTK_ALIGN_START);
    gtk_box_pack_start(GTK_BOX(pass_box), pass_label, FALSE, FALSE, 0);
    
    pass_entry = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(pass_entry), "Entrez votre mot de passe");
    gtk_entry_set_visibility(GTK_ENTRY(pass_entry), FALSE);
    gtk_entry_set_input_purpose(GTK_ENTRY(pass_entry), GTK_INPUT_PURPOSE_PASSWORD);
    gtk_box_pack_start(GTK_BOX(pass_box), pass_entry, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(form_box), pass_box, FALSE, FALSE, 0);
    
    // Login button
    GtkWidget *btn_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    GtkWidget *btn = gtk_button_new_with_label("Se connecter");
    gtk_style_context_add_class(gtk_widget_get_style_context(btn), "login-button");
    gtk_widget_set_size_request(btn, 200, -1);
    gtk_box_pack_start(GTK_BOX(btn_box), btn, TRUE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(form_box), btn_box, FALSE, FALSE, 10);
    
    g_signal_connect(btn, "clicked", G_CALLBACK(on_login_clicked), NULL);
    g_signal_connect(pass_entry, "activate", G_CALLBACK(on_login_clicked), NULL);
    
    gtk_widget_show_all(login_window);
}

static void show_tables_content(void) {
    gtk_container_foreach(GTK_CONTAINER(content_area), (GtkCallback)gtk_widget_destroy, NULL);

    // Main container with modern styling
    GtkWidget *tables_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 20);
    gtk_widget_set_margin_start(tables_box, 25);
    gtk_widget_set_margin_end(tables_box, 25);
    gtk_widget_set_margin_top(tables_box, 20);
    gtk_widget_set_margin_bottom(tables_box, 20);
    
    // ========== MODERN HEADER WITH ICON ==========
    GtkWidget *header_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 15);
    
    // Icon
    GtkWidget *header_icon = gtk_image_new_from_icon_name("view-list", GTK_ICON_SIZE_DIALOG);
    gtk_image_set_pixel_size(GTK_IMAGE(header_icon), 48);
    gtk_box_pack_start(GTK_BOX(header_box), header_icon, FALSE, FALSE, 0);
    
    // Title
    GtkWidget *header = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(header), 
        "<span size='xx-large' weight='bold' foreground='#2C3E50'>🪑 Gestion des Tables</span>");
    gtk_widget_set_halign(header, GTK_ALIGN_START);
    gtk_box_pack_start(GTK_BOX(header_box), header, FALSE, FALSE, 0);
    
    gtk_box_pack_start(GTK_BOX(tables_box), header_box, FALSE, FALSE, 0);
    
    // ========== PREMIUM STATISTICS CARDS ==========
    GtkWidget *stats_container = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 20);
    gtk_widget_set_margin_top(stats_container, 15);
    gtk_widget_set_margin_bottom(stats_container, 15);
    
    // Count tables by status
    int total = 0, libre = 0, occupee = 0, reservee = 0;
    Table *count_table = get_tables();
    if(count_table) {
        Table *first = count_table;
        do {
            total++;
            switch(count_table->etat) {
                case LIBRE: libre++; break;
                case OCCUPEE: occupee++; break;
                case RESERVEE: reservee++; break;
            }
            count_table = count_table->next;
        } while(count_table && count_table != first);
    }
    
    // Modern stat cards
    const char *stat_icons[] = {"📊", "✅", "🔴", "📅"};
    const char *stat_labels[] = {"Total Tables", "Libres", "Occupées", "Réservées"};
    int stat_values[] = {total, libre, occupee, reservee};
    const char *stat_colors[] = {"#3498db", "#27ae60", "#e74c3c", "#f39c12"};
    
    for(int i = 0; i < 4; i++) {
        GtkWidget *stat_card = gtk_frame_new(NULL);
        gtk_frame_set_shadow_type(GTK_FRAME(stat_card), GTK_SHADOW_NONE);
        
        GtkWidget *card_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
        gtk_widget_set_size_request(card_box, 200, 120);
        
        // Card styling
        char card_css[512];
        snprintf(card_css, sizeof(card_css),
            "frame { "
            "  background: %s; "
            "  border-radius: 12px; "
            "  box-shadow: 0 4px 15px rgba(0,0,0,0.1); "
            "  padding: 20px; "
            "} "
            "box { background: transparent; } "
            "label { color: white; font-weight: bold; }",
            stat_colors[i]);
        
        GtkCssProvider *card_provider = gtk_css_provider_new();
        gtk_css_provider_load_from_data(card_provider, card_css, -1, NULL);
        gtk_style_context_add_provider(
            gtk_widget_get_style_context(stat_card),
            GTK_STYLE_PROVIDER(card_provider),
            GTK_STYLE_PROVIDER_PRIORITY_APPLICATION
        );
        
        // Icon
        GtkWidget *icon_label = gtk_label_new(NULL);
        gtk_label_set_markup(GTK_LABEL(icon_label), 
            g_markup_printf_escaped("<span size='x-large'>%s</span>", stat_icons[i]));
        gtk_widget_set_halign(icon_label, GTK_ALIGN_START);
        gtk_box_pack_start(GTK_BOX(card_box), icon_label, FALSE, FALSE, 0);
        
        // Value
        char value_markup[128];
        snprintf(value_markup, sizeof(value_markup), 
            "<span size='28000' weight='bold'>%d</span>", stat_values[i]);
        GtkWidget *value_label = gtk_label_new(NULL);
        gtk_label_set_markup(GTK_LABEL(value_label), value_markup);
        gtk_widget_set_halign(value_label, GTK_ALIGN_START);
        gtk_box_pack_start(GTK_BOX(card_box), value_label, FALSE, FALSE, 0);
        
        // Description
        GtkWidget *desc_label = gtk_label_new(NULL);
        gtk_label_set_markup(GTK_LABEL(desc_label), 
            g_markup_printf_escaped("<span size='small'>%s</span>", stat_labels[i]));
        gtk_widget_set_halign(desc_label, GTK_ALIGN_START);
        gtk_box_pack_start(GTK_BOX(card_box), desc_label, FALSE, FALSE, 0);
        
        gtk_container_add(GTK_CONTAINER(stat_card), card_box);
        gtk_box_pack_start(GTK_BOX(stats_container), stat_card, TRUE, TRUE, 0);
        
        g_object_unref(card_provider);
    }
    
    gtk_box_pack_start(GTK_BOX(tables_box), stats_container, FALSE, FALSE, 0);
    
    // ========== FILTER BUTTONS ==========
    GtkWidget *filter_bar = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    gtk_widget_set_margin_top(filter_bar, 10);
    gtk_widget_set_margin_bottom(filter_bar, 15);
    
    const char *filter_labels[] = {"Toutes", "Libres", "Occupées", "Réservées"};
    const char *filter_colors[] = {"#95a5a6", "#27ae60", "#e74c3c", "#f39c12"};
    
    for(int i = 0; i < 4; i++) {
        GtkWidget *filter_btn = gtk_button_new_with_label(filter_labels[i]);
        gtk_widget_set_size_request(filter_btn, 120, 35);
        
        char btn_css[256];
        snprintf(btn_css, sizeof(btn_css),
            "button { "
            "  background: %s; "
            "  color: white; "
            "  border-radius: 6px; "
            "  border: none; "
            "  font-weight: bold; "
            "} "
            "button:hover { opacity: 0.8; }", filter_colors[i]);
        
        GtkCssProvider *btn_provider = gtk_css_provider_new();
        gtk_css_provider_load_from_data(btn_provider, btn_css, -1, NULL);
        gtk_style_context_add_provider(
            gtk_widget_get_style_context(filter_btn),
            GTK_STYLE_PROVIDER(btn_provider),
            GTK_STYLE_PROVIDER_PRIORITY_APPLICATION
        );
        
        g_signal_connect(filter_btn, "clicked", G_CALLBACK(on_table_filter_clicked), GINT_TO_POINTER(i));
        gtk_box_pack_start(GTK_BOX(filter_bar), filter_btn, FALSE, FALSE, 0);
        g_object_unref(btn_provider);
    }
    
    gtk_box_pack_start(GTK_BOX(tables_box), filter_bar, FALSE, FALSE, 0);
    
    // ========== TABLES GRID (CARD LAYOUT) ==========
    GtkWidget *scrolled = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled),
                                   GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_widget_set_vexpand(scrolled, TRUE);
    gtk_widget_set_hexpand(scrolled, TRUE);
    
    // Grid for table cards (4 columns)
    GtkWidget *grid = gtk_grid_new();
    gtk_grid_set_row_spacing(GTK_GRID(grid), 20);
    gtk_grid_set_column_spacing(GTK_GRID(grid), 20);
    gtk_widget_set_margin_start(grid, 5);
    gtk_widget_set_margin_end(grid, 5);
    
    // Create table cards
    Table *table = get_tables();
    int row = 0, col = 0;
    
    if(table) {
        Table *first = table;
        do {
            // Apply filters
            int show_table = 1;
            
            // Status filter
            if(table_filter_mode == 1 && table->etat != LIBRE) show_table = 0;
            if(table_filter_mode == 2 && table->etat != OCCUPEE) show_table = 0;
            if(table_filter_mode == 3 && table->etat != RESERVEE) show_table = 0;
            
            // Capacity filter
            if(table_capacity_filter > 0 && table->capacity != table_capacity_filter) show_table = 0;
            
            // Search filter
            if(strlen(table_search_text) > 0) {
                char id_str[32];
                snprintf(id_str, sizeof(id_str), "%d", table->id);
                if(strstr(id_str, table_search_text) == NULL &&
                   strstr(table->reserver_nom, table_search_text) == NULL) {
                    show_table = 0;
                }
            }
            
            if(show_table) {
                // ===== TABLE CARD =====
                GtkWidget *card = gtk_frame_new(NULL);
                gtk_frame_set_shadow_type(GTK_FRAME(card), GTK_SHADOW_NONE);
                gtk_widget_set_size_request(card, 280, 220);
                
                // Card container
                GtkWidget *card_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
                
                // Status color and text
                const char *status_color;
                const char *status_text;
                const char *status_emoji;
                
                switch(table->etat) {
                    case LIBRE:
                        status_color = "#27ae60";
                        status_text = "Libre";
                        status_emoji = "✅";
                        break;
                    case OCCUPEE:
                        status_color = "#e74c3c";
                        status_text = "Occupée";
                        status_emoji = "🔴";
                        break;
                    case RESERVEE:
                        status_color = "#f39c12";
                        status_text = "Réservée";
                        status_emoji = "📅";
                        break;
                    default:
                        status_color = "#95a5a6";
                        status_text = "Inconnu";
                        status_emoji = "❓";
                }
                
                // Card Header with status
                GtkWidget *card_header = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
                gtk_widget_set_size_request(card_header, -1, 50);
                
                char header_css[512];
                snprintf(header_css, sizeof(header_css),
                    "box { "
                    "  background: %s; "
                    "  padding: 12px; "
                    "  border-radius: 12px 12px 0 0; "
                    "} "
                    "label { color: white; font-weight: bold; }",
                    status_color);
                
                GtkCssProvider *header_provider = gtk_css_provider_new();
                gtk_css_provider_load_from_data(header_provider, header_css, -1, NULL);
                gtk_style_context_add_provider(
                    gtk_widget_get_style_context(card_header),
                    GTK_STYLE_PROVIDER(header_provider),
                    GTK_STYLE_PROVIDER_PRIORITY_APPLICATION
                );
                
                // Status with emoji
                char status_markup[128];
                snprintf(status_markup, sizeof(status_markup), 
                    "<span>%s %s</span>", status_emoji, status_text);
                GtkWidget *status_label = gtk_label_new(NULL);
                gtk_label_set_markup(GTK_LABEL(status_label), status_markup);
                gtk_box_pack_start(GTK_BOX(card_header), status_label, FALSE, FALSE, 0);
                
                // Spacer
                GtkWidget *spacer = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
                gtk_widget_set_hexpand(spacer, TRUE);
                gtk_box_pack_start(GTK_BOX(card_header), spacer, TRUE, TRUE, 0);
                
                // Table number
                char table_num[32];
                snprintf(table_num, sizeof(table_num), "Table #%d", table->id);
                GtkWidget *num_label = gtk_label_new(NULL);
                gtk_label_set_markup(GTK_LABEL(num_label), 
                    g_markup_printf_escaped("<span size='small'>%s</span>", table_num));
                gtk_box_pack_start(GTK_BOX(card_header), num_label, FALSE, FALSE, 0);
                
                gtk_box_pack_start(GTK_BOX(card_box), card_header, FALSE, FALSE, 0);
                g_object_unref(header_provider);
                
                // Card Body
                GtkWidget *card_body = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
                gtk_widget_set_margin_start(card_body, 15);
                gtk_widget_set_margin_end(card_body, 15);
                gtk_widget_set_margin_top(card_body, 12);
                gtk_widget_set_margin_bottom(card_body, 12);
                
                // Capacity
                char capacity_text[64];
                snprintf(capacity_text, sizeof(capacity_text), "👥 Capacité: %d personnes", table->capacity);
                GtkWidget *capacity_label = gtk_label_new(NULL);
                gtk_label_set_markup(GTK_LABEL(capacity_label), 
                    g_markup_printf_escaped("<span size='medium'>%s</span>", capacity_text));
                gtk_widget_set_halign(capacity_label, GTK_ALIGN_START);
                gtk_box_pack_start(GTK_BOX(card_body), capacity_label, FALSE, FALSE, 0);
                
                // Reservation info (if reserved)
                if(table->etat == RESERVEE && strlen(table->reserver_nom) > 0) {
                    GtkWidget *separator = gtk_separator_new(GTK_ORIENTATION_HORIZONTAL);
                    gtk_box_pack_start(GTK_BOX(card_body), separator, FALSE, FALSE, 3);
                    
                    char res_text[128];
                    snprintf(res_text, sizeof(res_text), "📝 %s", table->reserver_nom);
                    GtkWidget *res_label = gtk_label_new(NULL);
                    gtk_label_set_markup(GTK_LABEL(res_label), 
                        g_markup_printf_escaped("<span size='small'>%s</span>", res_text));
                    gtk_widget_set_halign(res_label, GTK_ALIGN_START);
                    gtk_label_set_line_wrap(GTK_LABEL(res_label), TRUE);
                    gtk_label_set_max_width_chars(GTK_LABEL(res_label), 25);
                    gtk_box_pack_start(GTK_BOX(card_body), res_label, FALSE, FALSE, 0);
                    
                    // Time
                    struct tm *tm = localtime(&table->reserver_when);
                    char time_text[64];
                    snprintf(time_text, sizeof(time_text), "🕐 %02d:%02d (%d min)", 
                        tm->tm_hour, tm->tm_min, table->reserver_duration_min);
                    GtkWidget *time_label = gtk_label_new(NULL);
                    gtk_label_set_markup(GTK_LABEL(time_label), 
                        g_markup_printf_escaped("<span size='small' foreground='#7f8c8d'>%s</span>", time_text));
                    gtk_widget_set_halign(time_label, GTK_ALIGN_START);
                    gtk_box_pack_start(GTK_BOX(card_body), time_label, FALSE, FALSE, 0);
                }
                
                // Action buttons
                GtkWidget *actions = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
                gtk_widget_set_margin_top(actions, 8);
                
                // Toggle status button
                const char *toggle_text = (table->etat == LIBRE) ? "🔴 Occuper" : "✅ Libérer";
                const char *toggle_color = (table->etat == LIBRE) ? "#e74c3c" : "#27ae60";
                
                GtkWidget *toggle_btn = gtk_button_new_with_label(toggle_text);
                gtk_widget_set_size_request(toggle_btn, 110, 32);
                
                char toggle_css[256];
                snprintf(toggle_css, sizeof(toggle_css),
                    "button { "
                    "  background: %s; "
                    "  color: white; "
                    "  border-radius: 6px; "
                    "  border: none; "
                    "  font-weight: bold; "
                    "  font-size: 11px; "
                    "} "
                    "button:hover { opacity: 0.9; }", toggle_color);
                
                GtkCssProvider *toggle_provider = gtk_css_provider_new();
                gtk_css_provider_load_from_data(toggle_provider, toggle_css, -1, NULL);
                gtk_style_context_add_provider(
                    gtk_widget_get_style_context(toggle_btn),
                    GTK_STYLE_PROVIDER(toggle_provider),
                    GTK_STYLE_PROVIDER_PRIORITY_APPLICATION
                );
                
                int *toggle_id = malloc(sizeof(int));
                *toggle_id = table->id;
                g_signal_connect(toggle_btn, "clicked", G_CALLBACK(on_table_primary_clicked), toggle_id);
                
                gtk_box_pack_start(GTK_BOX(actions), toggle_btn, FALSE, FALSE, 0);
                g_object_unref(toggle_provider);
                
                // Details button
                GtkWidget *details_btn = gtk_button_new_with_label("ℹ️");
                gtk_widget_set_size_request(details_btn, 40, 32);
                
                GtkCssProvider *details_provider = gtk_css_provider_new();
                gtk_css_provider_load_from_data(details_provider,
                    "button { "
                    "  background: #3498db; "
                    "  color: white; "
                    "  border-radius: 6px; "
                    "  border: none; "
                    "  font-size: 14px; "
                    "} "
                    "button:hover { background: #2980b9; }", -1, NULL);
                gtk_style_context_add_provider(
                    gtk_widget_get_style_context(details_btn),
                    GTK_STYLE_PROVIDER(details_provider),
                    GTK_STYLE_PROVIDER_PRIORITY_APPLICATION
                );
                
                int *details_id = malloc(sizeof(int));
                *details_id = table->id;
                g_signal_connect_data(details_btn, "clicked", G_CALLBACK(on_table_details_clicked), details_id, (GClosureNotify)g_free, 0);
                
                gtk_box_pack_start(GTK_BOX(actions), details_btn, FALSE, FALSE, 0);
                g_object_unref(details_provider);
                
                gtk_box_pack_start(GTK_BOX(card_body), actions, FALSE, FALSE, 0);
                gtk_box_pack_start(GTK_BOX(card_box), card_body, TRUE, TRUE, 0);
                
                // Card styling with shadow
                GtkCssProvider *card_provider = gtk_css_provider_new();
                gtk_css_provider_load_from_data(card_provider,
                    "frame { "
                    "  background: white; "
                    "  border-radius: 12px; "
                    "  box-shadow: 0 2px 10px rgba(0,0,0,0.08); "
                    "  border: 1px solid #e0e0e0; "
                    "} "
                    "frame:hover { "
                    "  box-shadow: 0 4px 20px rgba(0,0,0,0.12); "
                    "}", -1, NULL);
                gtk_style_context_add_provider(
                    gtk_widget_get_style_context(card),
                    GTK_STYLE_PROVIDER(card_provider),
                    GTK_STYLE_PROVIDER_PRIORITY_APPLICATION
                );
                
                gtk_container_add(GTK_CONTAINER(card), card_box);
                gtk_grid_attach(GTK_GRID(grid), card, col, row, 1, 1);
                
                g_object_unref(card_provider);
                
                // Move to next position (4 columns)
                col++;
                if(col >= 4) {
                    col = 0;
                    row++;
                }
            }
            
            table = table->next;
        } while(table && table != first);
    }
    
    gtk_container_add(GTK_CONTAINER(scrolled), grid);
    gtk_box_pack_start(GTK_BOX(tables_box), scrolled, TRUE, TRUE, 0);
    
    gtk_container_add(GTK_CONTAINER(content_area), tables_box);
    gtk_widget_show_all(content_area);
}

static void on_add_client_clicked(GtkButton *button, gpointer user_data) {
    GtkWidget *dialog = gtk_dialog_new_with_buttons("Ajouter un Client",
                                                   GTK_WINDOW(main_window),
                                                   GTK_DIALOG_MODAL,
                                                   "Annuler", GTK_RESPONSE_CANCEL,
                                                   "Ajouter", GTK_RESPONSE_ACCEPT,
                                                   NULL);
    
    GtkWidget *content = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
    gtk_container_set_border_width(GTK_CONTAINER(content), 15);
    
    GtkWidget *grid = gtk_grid_new();
    gtk_grid_set_row_spacing(GTK_GRID(grid), 10);
    gtk_grid_set_column_spacing(GTK_GRID(grid), 10);
    
    // Name field
    GtkWidget *name_label = gtk_label_new("Nom:");
    GtkWidget *name_entry = gtk_entry_new();
    gtk_grid_attach(GTK_GRID(grid), name_label, 0, 0, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), name_entry, 1, 0, 1, 1);
    
    // Phone field
    GtkWidget *phone_label = gtk_label_new("Téléphone:");
    GtkWidget *phone_entry = gtk_entry_new();
    gtk_grid_attach(GTK_GRID(grid), phone_label, 0, 1, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), phone_entry, 1, 1, 1, 1);
    
    // Email field
    GtkWidget *email_label = gtk_label_new("Email:");
    GtkWidget *email_entry = gtk_entry_new();
    gtk_grid_attach(GTK_GRID(grid), email_label, 0, 2, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), email_entry, 1, 2, 1, 1);
    
    gtk_container_add(GTK_CONTAINER(content), grid);
    gtk_widget_show_all(dialog);
    
    if(gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {
        Client c = {0};
        c.id = next_client_id();
        
        snprintf(c.name, sizeof(c.name), "%s", gtk_entry_get_text(GTK_ENTRY(name_entry)));
        snprintf(c.phone, sizeof(c.phone), "%s", gtk_entry_get_text(GTK_ENTRY(phone_entry)));
        snprintf(c.email, sizeof(c.email), "%s", gtk_entry_get_text(GTK_ENTRY(email_entry)));
        
        add_client(NULL, c);
    save_clients("data/clients.txt");

    GtkWidget *confirm = gtk_message_dialog_new(GTK_WINDOW(main_window),
                            GTK_DIALOG_MODAL,
                            GTK_MESSAGE_INFO,
                            GTK_BUTTONS_OK,
                            "Client ajouté : %s", c.name);
    gtk_dialog_run(GTK_DIALOG(confirm));
    gtk_widget_destroy(confirm);
    show_clients_content();
    }
    
    gtk_widget_destroy(dialog);
}

static void on_edit_client_clicked(GtkButton *button, gpointer user_data) {
    int id = *((int*)user_data);
    // free(user_data); // Handled by GClosureNotify
    Client *c = find_client_by_id(NULL, id);
    if(!c) return;

    GtkWidget *dialog = gtk_dialog_new_with_buttons("Modifier Client",
                                                   GTK_WINDOW(main_window),
                                                   GTK_DIALOG_MODAL,
                                                   "Annuler", GTK_RESPONSE_CANCEL,
                                                   "Enregistrer", GTK_RESPONSE_ACCEPT,
                                                   NULL);
    GtkWidget *content = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
    gtk_container_set_border_width(GTK_CONTAINER(content), 15);

    GtkWidget *grid = gtk_grid_new();
    gtk_grid_set_row_spacing(GTK_GRID(grid), 10);
    gtk_grid_set_column_spacing(GTK_GRID(grid), 10);

    GtkWidget *name_label = gtk_label_new("Nom:");
    GtkWidget *name_entry = gtk_entry_new();
    gtk_entry_set_text(GTK_ENTRY(name_entry), c->name);
    gtk_grid_attach(GTK_GRID(grid), name_label, 0, 0, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), name_entry, 1, 0, 1, 1);

    GtkWidget *phone_label = gtk_label_new("Téléphone:");
    GtkWidget *phone_entry = gtk_entry_new();
    gtk_entry_set_text(GTK_ENTRY(phone_entry), c->phone);
    gtk_grid_attach(GTK_GRID(grid), phone_label, 0, 1, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), phone_entry, 1, 1, 1, 1);

    GtkWidget *email_label = gtk_label_new("Email:");
    GtkWidget *email_entry = gtk_entry_new();
    gtk_entry_set_text(GTK_ENTRY(email_entry), c->email);
    gtk_grid_attach(GTK_GRID(grid), email_label, 0, 2, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), email_entry, 1, 2, 1, 1);

    gtk_container_add(GTK_CONTAINER(content), grid);
    gtk_widget_show_all(dialog);

    if(gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {
        const char *new_name = gtk_entry_get_text(GTK_ENTRY(name_entry));
        const char *new_phone = gtk_entry_get_text(GTK_ENTRY(phone_entry));
        const char *new_email = gtk_entry_get_text(GTK_ENTRY(email_entry));
        if(update_client(id, new_name, new_phone, new_email)){
            GtkWidget *conf = gtk_message_dialog_new(GTK_WINDOW(main_window), GTK_DIALOG_MODAL,
                                                     GTK_MESSAGE_INFO, GTK_BUTTONS_OK,
                                                     "Client mis à jour : %s", new_name);
            gtk_dialog_run(GTK_DIALOG(conf)); gtk_widget_destroy(conf);
        } else {
            GtkWidget *err = gtk_message_dialog_new(GTK_WINDOW(main_window), GTK_DIALOG_MODAL,
                                                   GTK_MESSAGE_ERROR, GTK_BUTTONS_OK,
                                                   "Échec de la mise à jour du client.");
            gtk_dialog_run(GTK_DIALOG(err)); gtk_widget_destroy(err);
        }
        show_clients_content();
    }
    gtk_widget_destroy(dialog);
}

static void on_delete_client_clicked(GtkButton *button, gpointer user_data) {
    int id = *((int*)user_data);
    // free(user_data); // Handled by GClosureNotify

    GtkWidget *confirm = gtk_message_dialog_new(GTK_WINDOW(main_window), GTK_DIALOG_MODAL,
                                                GTK_MESSAGE_WARNING, GTK_BUTTONS_YES_NO,
                                                "Supprimer le client #%d ?", id);
    int resp = gtk_dialog_run(GTK_DIALOG(confirm));
    gtk_widget_destroy(confirm);
    if(resp == GTK_RESPONSE_YES){
        if(delete_client(id)){
            GtkWidget *info = gtk_message_dialog_new(GTK_WINDOW(main_window), GTK_DIALOG_MODAL,
                                                     GTK_MESSAGE_INFO, GTK_BUTTONS_OK,
                                                     "Client supprimé.");
            gtk_dialog_run(GTK_DIALOG(info)); gtk_widget_destroy(info);
            show_clients_content();
        } else {
            GtkWidget *err = gtk_message_dialog_new(GTK_WINDOW(main_window), GTK_DIALOG_MODAL,
                                                   GTK_MESSAGE_ERROR, GTK_BUTTONS_OK,
                                                   "Impossible de supprimer le client.");
            gtk_dialog_run(GTK_DIALOG(err)); gtk_widget_destroy(err);
        }
    }
}

static void show_clients_content(void) {
    gtk_container_foreach(GTK_CONTAINER(content_area), (GtkCallback)gtk_widget_destroy, NULL);
    
    GtkWidget *clients_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 15);
    gtk_style_context_add_class(gtk_widget_get_style_context(clients_box), "content-box");
    gtk_widget_set_margin_start(clients_box, 20);
    gtk_widget_set_margin_end(clients_box, 20);
    

    GtkWidget *header_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    GtkWidget *header = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(header), "<span size='xx-large' weight='bold'>👥 Gestion des Clients</span>");
    gtk_box_pack_start(GTK_BOX(header_box), header, TRUE, TRUE, 0);
    

    int client_count = 0;
    Client *c = get_client_list();
    Client *first_c = c;
    if(c) do { client_count++; c = c->next; } while(c && c != first_c);
    
    char stats_text[128];
    snprintf(stats_text, sizeof(stats_text), "Total: %d clients", client_count);
    GtkWidget *stats = gtk_label_new(stats_text);
    gtk_widget_set_halign(stats, GTK_ALIGN_END);
    gtk_box_pack_end(GTK_BOX(header_box), stats, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(clients_box), header_box, FALSE, FALSE, 5);
    

    GtkWidget *toolbar = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    GtkWidget *add_btn = gtk_button_new_with_label("➕ Nouveau Client");
    gtk_widget_set_size_request(add_btn, 150, 36);

    GtkCssProvider *add_btn_provider = gtk_css_provider_new();
    gtk_css_provider_load_from_data(add_btn_provider,
        "button { background: linear-gradient(135deg, #27ae60, #229954); color: white; padding: 8px 16px; border-radius: 4px; border: none; font-weight: bold; } button:hover { background: linear-gradient(135deg, #229954, #1e8449); }", -1, NULL);
    gtk_style_context_add_provider(gtk_widget_get_style_context(add_btn),
        GTK_STYLE_PROVIDER(add_btn_provider), GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
    g_signal_connect(add_btn, "clicked", G_CALLBACK(on_add_client_clicked), NULL);
    gtk_box_pack_start(GTK_BOX(toolbar), add_btn, FALSE, FALSE, 0);
    g_object_unref(add_btn_provider);
    gtk_box_pack_start(GTK_BOX(clients_box), toolbar, FALSE, FALSE, 5);
    

    GtkWidget *scrolled_window = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled_window), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    
    GtkWidget *flowbox = gtk_flow_box_new();
    gtk_widget_set_valign(flowbox, GTK_ALIGN_START);
    gtk_flow_box_set_max_children_per_line(GTK_FLOW_BOX(flowbox), 3);
    gtk_flow_box_set_selection_mode(GTK_FLOW_BOX(flowbox), GTK_SELECTION_NONE);
    gtk_flow_box_set_min_children_per_line(GTK_FLOW_BOX(flowbox), 1);
    gtk_flow_box_set_row_spacing(GTK_FLOW_BOX(flowbox), 20);
    gtk_flow_box_set_column_spacing(GTK_FLOW_BOX(flowbox), 20);
    
    gtk_container_add(GTK_CONTAINER(scrolled_window), flowbox);
    gtk_box_pack_start(GTK_BOX(clients_box), scrolled_window, TRUE, TRUE, 0);
    

    c = get_client_list();
    if(c) do {
        GtkWidget *card = gtk_frame_new(NULL);
        gtk_frame_set_shadow_type(GTK_FRAME(card), GTK_SHADOW_NONE);
        gtk_widget_set_size_request(card, 280, 160);
        
        GtkCssProvider *card_provider = gtk_css_provider_new();
        gtk_css_provider_load_from_data(card_provider,
            "frame { background: white; border-radius: 12px; box-shadow: 0 4px 12px rgba(0,0,0,0.08); padding: 0; } "
            "frame:hover { box-shadow: 0 8px 24px rgba(0,0,0,0.12); transform: translateY(-2px); transition: all 0.2s; }", -1, NULL);
        gtk_style_context_add_provider(gtk_widget_get_style_context(card),
            GTK_STYLE_PROVIDER(card_provider), GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
        g_object_unref(card_provider);
        
        GtkWidget *card_vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
        gtk_container_add(GTK_CONTAINER(card), card_vbox);
        

        GtkWidget *header = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 15);
        gtk_widget_set_size_request(header, -1, 60);
        
        int name_hash = 0;
        for(int k=0; c->name[k]; k++) name_hash += c->name[k];
        const char *bg_colors[] = {"#6c5ce7", "#00b894", "#e17055", "#0984e3", "#d63031", "#e84393"};
        
        char header_css[512];
        snprintf(header_css, sizeof(header_css),
            "box { background: linear-gradient(135deg, %s, #2d3436); border-radius: 12px 12px 0 0; padding: 10px; }", bg_colors[name_hash % 6]);
        GtkCssProvider *header_provider = gtk_css_provider_new();
        gtk_css_provider_load_from_data(header_provider, header_css, -1, NULL);
        gtk_style_context_add_provider(gtk_widget_get_style_context(header),
            GTK_STYLE_PROVIDER(header_provider), GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
        g_object_unref(header_provider);
        

        char initials[3] = "??";
        if(strlen(c->name) > 0) {
            initials[0] = toupper(c->name[0]);
            char *space = strchr(c->name, ' ');
            if(space && *(space+1)) initials[1] = toupper(*(space+1));
            else initials[1] = toupper(c->name[1] ? c->name[1] : c->name[0]);
            initials[2] = '\0';
        }
        GtkWidget *avatar = gtk_label_new(initials);
        gtk_widget_set_size_request(avatar, 40, 40);
        GtkCssProvider *p_avatar = gtk_css_provider_new();
        gtk_css_provider_load_from_data(p_avatar, "label { background: rgba(255,255,255,0.25); color: white; border-radius: 20px; font-weight: bold; font-size: 16px; }", -1, NULL);
        gtk_style_context_add_provider(gtk_widget_get_style_context(avatar), GTK_STYLE_PROVIDER(p_avatar), GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
        g_object_unref(p_avatar);
        gtk_box_pack_start(GTK_BOX(header), avatar, FALSE, FALSE, 0);
        

        GtkWidget *titles_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
        gtk_widget_set_valign(titles_box, GTK_ALIGN_CENTER);
        char *name_markup = g_markup_printf_escaped("<span weight='bold' size='large' foreground='white'>%s</span>", c->name);
        GtkWidget *name_label = gtk_label_new(NULL);
        gtk_label_set_markup(GTK_LABEL(name_label), name_markup);
        g_free(name_markup);
        gtk_widget_set_halign(name_label, GTK_ALIGN_START);
        
        char *id_markup = g_markup_printf_escaped("<span size='small' foreground='#dfe6e9'>ID: #%d</span>", c->id);
        GtkWidget *id_label = gtk_label_new(NULL);
        gtk_label_set_markup(GTK_LABEL(id_label), id_markup);
        g_free(id_markup);
        gtk_widget_set_halign(id_label, GTK_ALIGN_START);
        
        gtk_box_pack_start(GTK_BOX(titles_box), name_label, FALSE, FALSE, 0);
        gtk_box_pack_start(GTK_BOX(titles_box), id_label, FALSE, FALSE, 0);
        gtk_box_pack_start(GTK_BOX(header), titles_box, TRUE, TRUE, 0);
        gtk_box_pack_start(GTK_BOX(card_vbox), header, FALSE, FALSE, 0);
        

        GtkWidget *body = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
        gtk_widget_set_margin_top(body, 10);
        gtk_widget_set_margin_bottom(body, 10);
        gtk_widget_set_margin_start(body, 10);
        gtk_widget_set_margin_end(body, 10);
        

        GtkWidget *phone_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
        GtkWidget *p_icon = gtk_image_new_from_icon_name("call-start-symbolic", GTK_ICON_SIZE_MENU);
        gtk_widget_set_opacity(p_icon, 0.6);
        GtkWidget *p_label = gtk_label_new(c->phone);
        gtk_box_pack_start(GTK_BOX(phone_box), p_icon, FALSE, FALSE, 0);
        gtk_box_pack_start(GTK_BOX(phone_box), p_label, FALSE, FALSE, 0);
        gtk_box_pack_start(GTK_BOX(body), phone_box, FALSE, FALSE, 0);
        

        GtkWidget *email_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
        GtkWidget *e_icon = gtk_image_new_from_icon_name("mail-message-new-symbolic", GTK_ICON_SIZE_MENU);
        gtk_widget_set_opacity(e_icon, 0.6);
        
        GtkWidget *e_label = gtk_label_new(c->email);
        gtk_label_set_ellipsize(GTK_LABEL(e_label), PANGO_ELLIPSIZE_END);
        gtk_widget_set_halign(e_label, GTK_ALIGN_START);
        
        gtk_box_pack_start(GTK_BOX(email_box), e_icon, FALSE, FALSE, 0);
        gtk_box_pack_start(GTK_BOX(email_box), e_label, FALSE, FALSE, 0);
        gtk_box_pack_start(GTK_BOX(body), email_box, FALSE, FALSE, 0);
        
        gtk_box_pack_start(GTK_BOX(card_vbox), body, TRUE, TRUE, 0);
        

        GtkWidget *footer = gtk_button_box_new(GTK_ORIENTATION_HORIZONTAL);
        gtk_button_box_set_layout(GTK_BUTTON_BOX(footer), GTK_BUTTONBOX_END);
        gtk_box_set_spacing(GTK_BOX(footer), 5);
        gtk_widget_set_margin_end(footer, 10);
        gtk_widget_set_margin_bottom(footer, 10);
        
        GtkWidget *edit_btn = gtk_button_new_from_icon_name("document-edit-symbolic", GTK_ICON_SIZE_BUTTON);
        gtk_widget_set_tooltip_text(edit_btn, "Modifier");
        int *edit_id = malloc(sizeof(int)); *edit_id = c->id;
        g_signal_connect_data(edit_btn, "clicked", G_CALLBACK(on_edit_client_clicked), edit_id, (GClosureNotify)free, 0);
        
        GtkWidget *del_btn = gtk_button_new_from_icon_name("user-trash-symbolic", GTK_ICON_SIZE_BUTTON);
        gtk_widget_set_tooltip_text(del_btn, "Supprimer");
        GtkStyleContext *del_ctx = gtk_widget_get_style_context(del_btn);
        gtk_style_context_add_class(del_ctx, "destructive-action");
        int *del_id = malloc(sizeof(int)); *del_id = c->id;
        g_signal_connect_data(del_btn, "clicked", G_CALLBACK(on_delete_client_clicked), del_id, (GClosureNotify)free, 0);
        
        gtk_container_add(GTK_CONTAINER(footer), edit_btn);
        gtk_container_add(GTK_CONTAINER(footer), del_btn);
        gtk_box_pack_start(GTK_BOX(card_vbox), footer, FALSE, FALSE, 0);
        
        gtk_widget_show_all(card);
        gtk_flow_box_insert(GTK_FLOW_BOX(flowbox), card, -1);
        
        c = c->next;
    } while(c && c != first_c);
    
    gtk_box_pack_start(GTK_BOX(content_area), clients_box, TRUE, TRUE, 0);
    gtk_widget_show_all(content_area);
}

static void on_add_stock_clicked(GtkButton *button, gpointer user_data){
    GtkWidget *dialog = gtk_dialog_new_with_buttons("Ajouter un Ingrédient",
                                                   GTK_WINDOW(main_window),
                                                   GTK_DIALOG_MODAL,
                                                   "Annuler", GTK_RESPONSE_CANCEL,
                                                   "Ajouter", GTK_RESPONSE_ACCEPT,
                                                   NULL);
    GtkWidget *content = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
    gtk_container_set_border_width(GTK_CONTAINER(content), 12);

    GtkWidget *grid = gtk_grid_new();
    gtk_grid_set_row_spacing(GTK_GRID(grid), 8);
    gtk_grid_set_column_spacing(GTK_GRID(grid), 8);

    GtkWidget *name_label = gtk_label_new("Nom:");
    GtkWidget *name_entry = gtk_entry_new();
    gtk_grid_attach(GTK_GRID(grid), name_label, 0, 0, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), name_entry, 1, 0, 1, 1);

    GtkWidget *qty_label = gtk_label_new("Quantité:");
    GtkWidget *qty_entry = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(qty_entry), "ex: 100.0");
    gtk_grid_attach(GTK_GRID(grid), qty_label, 0, 1, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), qty_entry, 1, 1, 1, 1);

    GtkWidget *unit_label = gtk_label_new("Unité:");
    GtkWidget *unit_entry = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(unit_entry), "g / pcs / L");
    gtk_grid_attach(GTK_GRID(grid), unit_label, 0, 2, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), unit_entry, 1, 2, 1, 1);

    gtk_container_add(GTK_CONTAINER(content), grid);
    gtk_widget_show_all(dialog);

    if(gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT){
        const char *name = gtk_entry_get_text(GTK_ENTRY(name_entry));
        const char *qtys = gtk_entry_get_text(GTK_ENTRY(qty_entry));
        const char *unit = gtk_entry_get_text(GTK_ENTRY(unit_entry));
        double q = atof(qtys);
        if(name && *name){
            add_ingredient(name, q, unit);
            GtkWidget *conf = gtk_message_dialog_new(GTK_WINDOW(main_window), GTK_DIALOG_MODAL,
                                                    GTK_MESSAGE_INFO, GTK_BUTTONS_OK,
                                                    "Ingrédient ajouté : %s", name);
            gtk_dialog_run(GTK_DIALOG(conf)); gtk_widget_destroy(conf);
            show_stock_content();
        }
    }
    gtk_widget_destroy(dialog);
}

static void on_edit_stock_clicked(GtkButton *button, gpointer user_data){
    char *name = (char*)user_data;
    Ingredient *it = find_ingredient(name);
    if(!it){ free(name); return; }

    GtkWidget *dialog = gtk_dialog_new_with_buttons("Modifier Ingrédient",
                                                   GTK_WINDOW(main_window),
                                                   GTK_DIALOG_MODAL,
                                                   "Annuler", GTK_RESPONSE_CANCEL,
                                                   "Enregistrer", GTK_RESPONSE_ACCEPT,
                                                   NULL);
    GtkWidget *content = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
    gtk_container_set_border_width(GTK_CONTAINER(content), 12);

    GtkWidget *grid = gtk_grid_new();
    gtk_grid_set_row_spacing(GTK_GRID(grid), 8);
    gtk_grid_set_column_spacing(GTK_GRID(grid), 8);

    GtkWidget *name_label = gtk_label_new("Nom:");
    GtkWidget *name_entry = gtk_entry_new();
    gtk_entry_set_text(GTK_ENTRY(name_entry), it->name);
    gtk_grid_attach(GTK_GRID(grid), name_label, 0, 0, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), name_entry, 1, 0, 1, 1);

    GtkWidget *qty_label = gtk_label_new("Quantité:");
    GtkWidget *qty_entry = gtk_entry_new();
    char qbuf[64]; snprintf(qbuf, sizeof(qbuf), "%.2f", it->quantity);
    gtk_entry_set_text(GTK_ENTRY(qty_entry), qbuf);
    gtk_grid_attach(GTK_GRID(grid), qty_label, 0, 1, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), qty_entry, 1, 1, 1, 1);

    GtkWidget *unit_label = gtk_label_new("Unité:");
    GtkWidget *unit_entry = gtk_entry_new();
    gtk_entry_set_text(GTK_ENTRY(unit_entry), it->unit);
    gtk_grid_attach(GTK_GRID(grid), unit_label, 0, 2, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), unit_entry, 1, 2, 1, 1);

    gtk_container_add(GTK_CONTAINER(content), grid);
    gtk_widget_show_all(dialog);

    if(gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT){
        const char *new_name = gtk_entry_get_text(GTK_ENTRY(name_entry));
        const char *qtys = gtk_entry_get_text(GTK_ENTRY(qty_entry));
        const char *unit = gtk_entry_get_text(GTK_ENTRY(unit_entry));
        double q = atof(qtys);
        if(new_name && *new_name){

            if(strcmp(new_name, name) != 0){
                delete_ingredient(name);
                add_ingredient(new_name, q, unit);
            } else {
                update_ingredient(name, q, unit);
            }
            GtkWidget *conf = gtk_message_dialog_new(GTK_WINDOW(main_window), GTK_DIALOG_MODAL,
                                                    GTK_MESSAGE_INFO, GTK_BUTTONS_OK,
                                                    "Ingrédient mis à jour : %s", new_name);
            gtk_dialog_run(GTK_DIALOG(conf)); gtk_widget_destroy(conf);
            show_stock_content();
        }
    }
    gtk_widget_destroy(dialog);
}

static void on_delete_stock_clicked(GtkButton *button, gpointer user_data){
    char *name = (char*)user_data;
    GtkWidget *confirm = gtk_message_dialog_new(GTK_WINDOW(main_window), GTK_DIALOG_MODAL,
                                                GTK_MESSAGE_WARNING, GTK_BUTTONS_YES_NO,
                                                "Supprimer l'ingrédient '%s' ?", name);
    int resp = gtk_dialog_run(GTK_DIALOG(confirm)); gtk_widget_destroy(confirm);
    if(resp == GTK_RESPONSE_YES){
        delete_ingredient(name);
        GtkWidget *info = gtk_message_dialog_new(GTK_WINDOW(main_window), GTK_DIALOG_MODAL,
                                                 GTK_MESSAGE_INFO, GTK_BUTTONS_OK,
                                                 "Ingrédient supprimé.");
        gtk_dialog_run(GTK_DIALOG(info)); gtk_widget_destroy(info);
        show_stock_content();
    }
}

static void show_stock_content(void) {
    gtk_container_foreach(GTK_CONTAINER(content_area), (GtkCallback)gtk_widget_destroy, NULL);

    GtkWidget *stock_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 15);
    gtk_style_context_add_class(gtk_widget_get_style_context(stock_box), "content-box");
    gtk_widget_set_margin_start(stock_box, 20);
    gtk_widget_set_margin_end(stock_box, 20);

    GtkWidget *header_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    GtkWidget *header = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(header), "<span size='xx-large' weight='bold'>📦 Gestion du Stock</span>");
    gtk_box_pack_start(GTK_BOX(header_box), header, TRUE, TRUE, 0);

    // Count items
    int count = 0;
    Ingredient *it = get_stock_list();
    for(Ingredient *dbg = it; dbg; dbg = dbg->next) fprintf(stderr, "[ui] stock item: %s %.2f %s\n", dbg->name, dbg->quantity, dbg->unit);
    for(Ingredient *tmp = it; tmp; tmp = tmp->next) count++;
    char stats_text[64]; snprintf(stats_text, sizeof(stats_text), "Total: %d ingrédients", count);
    GtkWidget *stats = gtk_label_new(stats_text);
    gtk_widget_set_halign(stats, GTK_ALIGN_END);
    gtk_box_pack_end(GTK_BOX(header_box), stats, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(stock_box), header_box, FALSE, FALSE, 5);

    // Toolbar
    GtkWidget *toolbar = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    GtkWidget *add_btn = gtk_button_new_with_label("➕ Ajouter Ingrédient");
    gtk_widget_set_size_request(add_btn, 180, 36);
    GtkCssProvider *p = gtk_css_provider_new();
    gtk_css_provider_load_from_data(p, "button { background: linear-gradient(135deg,#27ae60,#229954); color:white; border-radius:4px; } button:hover{opacity:0.95}", -1, NULL);
    gtk_style_context_add_provider(gtk_widget_get_style_context(add_btn), GTK_STYLE_PROVIDER(p), GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
    g_signal_connect(add_btn, "clicked", G_CALLBACK(on_add_stock_clicked), NULL);
    gtk_box_pack_start(GTK_BOX(toolbar), add_btn, FALSE, FALSE, 0);
    g_object_unref(p);
    gtk_box_pack_start(GTK_BOX(stock_box), toolbar, FALSE, FALSE, 5);


    GtkWidget *scrolled_window = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled_window), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    
    GtkWidget *flowbox = gtk_flow_box_new();
    gtk_widget_set_valign(flowbox, GTK_ALIGN_START);
    gtk_flow_box_set_max_children_per_line(GTK_FLOW_BOX(flowbox), 4);
    gtk_flow_box_set_selection_mode(GTK_FLOW_BOX(flowbox), GTK_SELECTION_NONE);
    gtk_flow_box_set_min_children_per_line(GTK_FLOW_BOX(flowbox), 1);
    gtk_flow_box_set_row_spacing(GTK_FLOW_BOX(flowbox), 20);
    gtk_flow_box_set_column_spacing(GTK_FLOW_BOX(flowbox), 20);
    
    gtk_container_add(GTK_CONTAINER(scrolled_window), flowbox);
    gtk_box_pack_start(GTK_BOX(stock_box), scrolled_window, TRUE, TRUE, 0);


    it = get_stock_list();
    for(Ingredient *cur = it; cur; cur = cur->next){
        GtkWidget *card = gtk_frame_new(NULL);
        gtk_frame_set_shadow_type(GTK_FRAME(card), GTK_SHADOW_NONE);
        gtk_widget_set_size_request(card, 220, 160);
        
        GtkCssProvider *card_provider = gtk_css_provider_new();
        gtk_css_provider_load_from_data(card_provider,
            "frame { background: white; border-radius: 12px; box-shadow: 0 4px 12px rgba(0,0,0,0.08); padding: 0; } "
            "frame:hover { box-shadow: 0 8px 24px rgba(0,0,0,0.12); transform: translateY(-2px); transition: all 0.2s; }", -1, NULL);
        gtk_style_context_add_provider(gtk_widget_get_style_context(card),
            GTK_STYLE_PROVIDER(card_provider), GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
        g_object_unref(card_provider);
        
        GtkWidget *card_vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
        gtk_container_add(GTK_CONTAINER(card), card_vbox);
        

        GtkWidget *header = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
        gtk_widget_set_size_request(header, -1, 50);
        
        char header_css[512];
        const char *bg_color = cur->quantity < 10.0 ? "#e74c3c" : "#2ecc71";
        snprintf(header_css, sizeof(header_css),
            "box { background: linear-gradient(135deg, %s, #2d3436); border-radius: 12px 12px 0 0; padding: 10px; }", bg_color);
        GtkCssProvider *header_provider = gtk_css_provider_new();
        gtk_css_provider_load_from_data(header_provider, header_css, -1, NULL);
        gtk_style_context_add_provider(gtk_widget_get_style_context(header),
            GTK_STYLE_PROVIDER(header_provider), GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
        g_object_unref(header_provider);
        
        GtkWidget *icon = gtk_label_new("📦");
        GtkWidget *name_label = gtk_label_new(NULL);
        char *markup = g_markup_printf_escaped("<span weight='bold' foreground='white'>%s</span>", cur->name);
        gtk_label_set_markup(GTK_LABEL(name_label), markup);
        g_free(markup);
        
        gtk_box_pack_start(GTK_BOX(header), icon, FALSE, FALSE, 0);
        gtk_box_pack_start(GTK_BOX(header), name_label, FALSE, FALSE, 0);
        gtk_box_pack_start(GTK_BOX(card_vbox), header, FALSE, FALSE, 0);
        

        GtkWidget *body = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
        gtk_widget_set_valign(body, GTK_ALIGN_CENTER);
        gtk_widget_set_vexpand(body, TRUE);
        
        char q_str[64];
        snprintf(q_str, sizeof(q_str), "%.2f", cur->quantity);
        GtkWidget *q_label = gtk_label_new(NULL);
        char *q_markup = g_markup_printf_escaped("<span weight='bold' size='xx-large' foreground='#2c3e50'>%s</span>", q_str);
        gtk_label_set_markup(GTK_LABEL(q_label), q_markup);
        g_free(q_markup);
        
        GtkWidget *u_label = gtk_label_new(cur->unit);
        GtkCssProvider *u_prov = gtk_css_provider_new();
        gtk_css_provider_load_from_data(u_prov, "label { color: #95a5a6; font-size: 12px; font-weight: bold; text-transform: uppercase; }", -1, NULL);
        gtk_style_context_add_provider(gtk_widget_get_style_context(u_label), GTK_STYLE_PROVIDER(u_prov), GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
        g_object_unref(u_prov);
        
        gtk_box_pack_start(GTK_BOX(body), q_label, FALSE, FALSE, 0);
        gtk_box_pack_start(GTK_BOX(body), u_label, FALSE, FALSE, 0);
        gtk_box_pack_start(GTK_BOX(card_vbox), body, TRUE, TRUE, 0);
        

        GtkWidget *footer = gtk_button_box_new(GTK_ORIENTATION_HORIZONTAL);
        gtk_button_box_set_layout(GTK_BUTTON_BOX(footer), GTK_BUTTONBOX_CENTER);
        gtk_box_set_spacing(GTK_BOX(footer), 10);
        gtk_widget_set_margin_bottom(footer, 10);
        
        GtkWidget *edit_btn = gtk_button_new_from_icon_name("document-edit-symbolic", GTK_ICON_SIZE_BUTTON);
        gtk_widget_set_tooltip_text(edit_btn, "Modifier");
        char *name_copy = strdup(cur->name);
        g_signal_connect_data(edit_btn, "clicked", G_CALLBACK(on_edit_stock_clicked), name_copy, (GClosureNotify)free, 0);
        
        GtkWidget *del_btn = gtk_button_new_from_icon_name("user-trash-symbolic", GTK_ICON_SIZE_BUTTON);
        gtk_widget_set_tooltip_text(del_btn, "Supprimer");
        char *name_copy2 = strdup(cur->name);
        GtkStyleContext *del_ctx = gtk_widget_get_style_context(del_btn);
        gtk_style_context_add_class(del_ctx, "destructive-action");
        g_signal_connect_data(del_btn, "clicked", G_CALLBACK(on_delete_stock_clicked), name_copy2, (GClosureNotify)free, 0);
        
        gtk_container_add(GTK_CONTAINER(footer), edit_btn);
        gtk_container_add(GTK_CONTAINER(footer), del_btn);
        
        gtk_box_pack_start(GTK_BOX(card_vbox), footer, FALSE, FALSE, 0);
        
        gtk_widget_show_all(card);
        gtk_flow_box_insert(GTK_FLOW_BOX(flowbox), card, -1);
    }
    
    gtk_box_pack_start(GTK_BOX(content_area), stock_box, TRUE, TRUE, 0);
    gtk_widget_show_all(content_area);
}

static void on_add_personnel_clicked(GtkButton *button, gpointer user_data){
    GtkWidget *dialog = gtk_dialog_new_with_buttons("Ajouter un Employé",
                                                   GTK_WINDOW(main_window),
                                                   GTK_DIALOG_MODAL,
                                                   "Annuler", GTK_RESPONSE_CANCEL,
                                                   "Ajouter", GTK_RESPONSE_ACCEPT,
                                                   NULL);
    GtkWidget *content = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
    gtk_container_set_border_width(GTK_CONTAINER(content), 12);

    GtkWidget *grid = gtk_grid_new();
    gtk_grid_set_row_spacing(GTK_GRID(grid), 8);
    gtk_grid_set_column_spacing(GTK_GRID(grid), 8);

    GtkWidget *name_label = gtk_label_new("Nom:");
    GtkWidget *name_entry = gtk_entry_new();
    gtk_grid_attach(GTK_GRID(grid), name_label, 0, 0, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), name_entry, 1, 0, 1, 1);

    GtkWidget *role_label = gtk_label_new("Rôle:");
    GtkWidget *role_entry = gtk_combo_box_text_new();
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(role_entry), "Serveur");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(role_entry), "Cuisinier");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(role_entry), "Caissier");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(role_entry), "Admin");
    gtk_grid_attach(GTK_GRID(grid), role_label, 0, 1, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), role_entry, 1, 1, 1, 1);

    GtkWidget *hours_label = gtk_label_new("Heures travaillées:");
    GtkWidget *hours_entry = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(hours_entry), "ex: 40.0");
    gtk_grid_attach(GTK_GRID(grid), hours_label, 0, 2, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), hours_entry, 1, 2, 1, 1);

    gtk_container_add(GTK_CONTAINER(content), grid);
    gtk_widget_show_all(dialog);

    if(gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT){
        Employe e = {0};
        const char *name = gtk_entry_get_text(GTK_ENTRY(name_entry));
        const char *role = gtk_combo_box_text_get_active_text(GTK_COMBO_BOX_TEXT(role_entry));
        const char *hours_s = gtk_entry_get_text(GTK_ENTRY(hours_entry));
        e.hours_worked = atof(hours_s);
        if(name && *name){ strncpy(e.name, name, sizeof(e.name)-1); }
        if(role) strncpy(e.role, role, sizeof(e.role)-1);
        add_employe(NULL, e);
        GtkWidget *conf = gtk_message_dialog_new(GTK_WINDOW(main_window), GTK_DIALOG_MODAL,
                                                GTK_MESSAGE_INFO, GTK_BUTTONS_OK,
                                                "Employé ajouté : %s", e.name);
        gtk_dialog_run(GTK_DIALOG(conf)); gtk_widget_destroy(conf);
        show_personnel_content();
    }
    gtk_widget_destroy(dialog);
}

static void on_edit_personnel_clicked(GtkButton *button, gpointer user_data){
    int id = *((int*)user_data); free(user_data);
    Employe *emp = find_employe_by_id(id);
    if(!emp) return;

    GtkWidget *dialog = gtk_dialog_new_with_buttons("Modifier Employé",
                                                   GTK_WINDOW(main_window),
                                                   GTK_DIALOG_MODAL,
                                                   "Annuler", GTK_RESPONSE_CANCEL,
                                                   "Enregistrer", GTK_RESPONSE_ACCEPT,
                                                   NULL);
    GtkWidget *content = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
    gtk_container_set_border_width(GTK_CONTAINER(content), 12);

    GtkWidget *grid = gtk_grid_new();
    gtk_grid_set_row_spacing(GTK_GRID(grid), 8);
    gtk_grid_set_column_spacing(GTK_GRID(grid), 8);

    GtkWidget *name_label = gtk_label_new("Nom:");
    GtkWidget *name_entry = gtk_entry_new(); gtk_entry_set_text(GTK_ENTRY(name_entry), emp->name);
    gtk_grid_attach(GTK_GRID(grid), name_label, 0, 0, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), name_entry, 1, 0, 1, 1);

    GtkWidget *role_label = gtk_label_new("Rôle:");
    GtkWidget *role_entry = gtk_combo_box_text_new();
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(role_entry), "Serveur");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(role_entry), "Cuisinier");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(role_entry), "Caissier");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(role_entry), "Admin");
    gtk_combo_box_set_active(GTK_COMBO_BOX(role_entry), 0);
    gtk_grid_attach(GTK_GRID(grid), role_label, 0, 1, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), role_entry, 1, 1, 1, 1);

    GtkWidget *hours_label = gtk_label_new("Heures travaillées:");
    GtkWidget *hours_entry = gtk_entry_new();
    char hbuf[32]; snprintf(hbuf, sizeof(hbuf), "%.2f", emp->hours_worked);
    gtk_entry_set_text(GTK_ENTRY(hours_entry), hbuf);
    gtk_grid_attach(GTK_GRID(grid), hours_label, 0, 2, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), hours_entry, 1, 2, 1, 1);

    gtk_container_add(GTK_CONTAINER(content), grid);
    gtk_widget_show_all(dialog);

    if(gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT){
        const char *new_name = gtk_entry_get_text(GTK_ENTRY(name_entry));
        const char *new_role = gtk_combo_box_text_get_active_text(GTK_COMBO_BOX_TEXT(role_entry));
        const char *hours_s = gtk_entry_get_text(GTK_ENTRY(hours_entry));
        double h = atof(hours_s);
        if(update_employe(id, new_name, new_role, h)){
            GtkWidget *conf = gtk_message_dialog_new(GTK_WINDOW(main_window), GTK_DIALOG_MODAL,
                                                    GTK_MESSAGE_INFO, GTK_BUTTONS_OK,
                                                    "Employé mis à jour : %s", new_name);
            gtk_dialog_run(GTK_DIALOG(conf)); gtk_widget_destroy(conf);
        } else {
            GtkWidget *err = gtk_message_dialog_new(GTK_WINDOW(main_window), GTK_DIALOG_MODAL,
                                                   GTK_MESSAGE_ERROR, GTK_BUTTONS_OK,
                                                   "Échec de la mise à jour.");
            gtk_dialog_run(GTK_DIALOG(err)); gtk_widget_destroy(err);
        }
        show_personnel_content();
    }
    gtk_widget_destroy(dialog);
}

static void on_delete_personnel_clicked(GtkButton *button, gpointer user_data){
    int id = *((int*)user_data); free(user_data);
    GtkWidget *confirm = gtk_message_dialog_new(GTK_WINDOW(main_window), GTK_DIALOG_MODAL,
                                                GTK_MESSAGE_WARNING, GTK_BUTTONS_YES_NO,
                                                "Supprimer l'employé #%d ?", id);
    int resp = gtk_dialog_run(GTK_DIALOG(confirm)); gtk_widget_destroy(confirm);
    if(resp == GTK_RESPONSE_YES){
        if(delete_employe(id)){
            GtkWidget *info = gtk_message_dialog_new(GTK_WINDOW(main_window), GTK_DIALOG_MODAL,
                                                     GTK_MESSAGE_INFO, GTK_BUTTONS_OK,
                                                     "Employé supprimé.");
            gtk_dialog_run(GTK_DIALOG(info)); gtk_widget_destroy(info);
            show_personnel_content();
        } else {
            GtkWidget *err = gtk_message_dialog_new(GTK_WINDOW(main_window), GTK_DIALOG_MODAL,
                                                   GTK_MESSAGE_ERROR, GTK_BUTTONS_OK,
                                                   "Impossible de supprimer l'employé.");
            gtk_dialog_run(GTK_DIALOG(err)); gtk_widget_destroy(err);
        }
    }
}

static void show_personnel_content(void) {
    gtk_container_foreach(GTK_CONTAINER(content_area), (GtkCallback)gtk_widget_destroy, NULL);
    
    // Main container with padding
    GtkWidget *personnel_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 20);
    // gtk_style_context_add_class(gtk_widget_get_style_context(personnel_box), "content-box"); // Removed for cleaner look
    gtk_widget_set_margin_start(personnel_box, 25);
    gtk_widget_set_margin_end(personnel_box, 25);
    gtk_widget_set_margin_top(personnel_box, 20);
    gtk_widget_set_margin_bottom(personnel_box, 20);

    // ========== MODERN HEADER ==========
    GtkWidget *header_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 15);
    
    GtkWidget *header_icon = gtk_image_new_from_icon_name("system-users", GTK_ICON_SIZE_DIALOG);
    gtk_image_set_pixel_size(GTK_IMAGE(header_icon), 48);
    gtk_box_pack_start(GTK_BOX(header_box), header_icon, FALSE, FALSE, 0);

    GtkWidget *header = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(header), "<span size='xx-large' weight='bold' foreground='#2C3E50'>👥 Gestion du Personnel</span>");
    gtk_box_pack_start(GTK_BOX(header_box), header, FALSE, FALSE, 0);
    
    // Right side stats
    GtkWidget *spacer = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_widget_set_hexpand(spacer, TRUE);
    gtk_box_pack_start(GTK_BOX(header_box), spacer, TRUE, TRUE, 0);

    int count = 0;
    Employe *e = get_personnel_list(); 
    for(Employe *tmp=e; tmp; tmp=tmp->next) count++;
    
    char stats_text[128]; 
    snprintf(stats_text, sizeof(stats_text), "<span size='large' weight='bold' foreground='#7f8c8d'>Total: %d employés</span>", count);
    GtkWidget *stats = gtk_label_new(NULL); 
    gtk_label_set_markup(GTK_LABEL(stats), stats_text);
    gtk_box_pack_end(GTK_BOX(header_box), stats, FALSE, FALSE, 0);
    
    gtk_box_pack_start(GTK_BOX(personnel_box), header_box, FALSE, FALSE, 5);

    // ========== ACTION BAR ==========
    GtkWidget *action_bar = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    gtk_widget_set_margin_top(action_bar, 10);
    gtk_widget_set_margin_bottom(action_bar, 15);

    GtkWidget *add_btn = gtk_button_new_with_label("➕ Nouveau Employé"); 
    gtk_widget_set_size_request(add_btn, 200, 45);
    
    GtkCssProvider *btn_provider = gtk_css_provider_new();
    gtk_css_provider_load_from_data(btn_provider, 
        "button { "
        "  background: linear-gradient(135deg, #8e44ad 0%, #9b59b6 100%); "
        "  color: white; "
        "  border-radius: 8px; "
        "  border: none; "
        "  font-weight: bold; "
        "  box-shadow: 0 4px 12px rgba(142, 68, 173, 0.4); "
        "} "
        "button:hover { "
        "  box-shadow: 0 6px 16px rgba(142, 68, 173, 0.6); "
        "}", -1, NULL);
    gtk_style_context_add_provider(gtk_widget_get_style_context(add_btn), GTK_STYLE_PROVIDER(btn_provider), GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
    g_object_unref(btn_provider);
    
    g_signal_connect(add_btn, "clicked", G_CALLBACK(on_add_personnel_clicked), NULL);
    gtk_box_pack_start(GTK_BOX(action_bar), add_btn, FALSE, FALSE, 0);
    
    gtk_box_pack_start(GTK_BOX(personnel_box), action_bar, FALSE, FALSE, 5);

    // ========== SCROLLED CONTAINER ==========
    GtkWidget *scrolled = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_widget_set_vexpand(scrolled, TRUE);
    gtk_widget_set_hexpand(scrolled, TRUE);

    // ========== GRID / FLOWBOX LAYOUT ==========
    // Using a GtkFlowBox for responsive card layout
    GtkWidget *flowbox = gtk_flow_box_new();
    gtk_widget_set_valign(flowbox, GTK_ALIGN_START);
    gtk_flow_box_set_max_children_per_line(GTK_FLOW_BOX(flowbox), 4);
    gtk_flow_box_set_min_children_per_line(GTK_FLOW_BOX(flowbox), 1);
    gtk_flow_box_set_selection_mode(GTK_FLOW_BOX(flowbox), GTK_SELECTION_NONE);
    gtk_flow_box_set_row_spacing(GTK_FLOW_BOX(flowbox), 20);
    gtk_flow_box_set_column_spacing(GTK_FLOW_BOX(flowbox), 20);
    gtk_widget_set_margin_start(flowbox, 5);
    gtk_widget_set_margin_end(flowbox, 5);

    for(Employe *cur = e; cur; cur = cur->next){
        GtkWidget *card_frame = gtk_frame_new(NULL);
        gtk_frame_set_shadow_type(GTK_FRAME(card_frame), GTK_SHADOW_NONE);
        gtk_widget_set_size_request(card_frame, 280, 220); // Fixed card size

        // Card Container
        GtkWidget *card_vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);

        // --- Determine Style based on Role ---
        const char *role_graident;
        const char *role_icon_name;
        if(strcmp(cur->role, "Serveur") == 0) {
            role_graident = "linear-gradient(135deg, #3498db, #2980b9)";
            role_icon_name = "user-info";
        } else if (strcmp(cur->role, "Cuisinier") == 0) {
            role_graident = "linear-gradient(135deg, #e67e22, #d35400)";
            role_icon_name = "starred"; // best approximation
        } else if (strcmp(cur->role, "Caissier") == 0) {
             role_graident = "linear-gradient(135deg, #27ae60, #229954)";
             role_icon_name = "emblem-money"; // best approximation
        } else if (strcmp(cur->role, "Admin") == 0) {
             role_graident = "linear-gradient(135deg, #e74c3c, #c0392b)";
             role_icon_name = "security-high"; 
        } else {
             role_graident = "linear-gradient(135deg, #95a5a6, #7f8c8d)";
             role_icon_name = "avatar-default";
        }

        // --- Card Header ---
        GtkWidget *card_header = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
        gtk_widget_set_size_request(card_header, -1, 60);
        
        char header_css[512];
        snprintf(header_css, sizeof(header_css),
            "box { background: %s; border-radius: 12px 12px 0 0; padding: 15px; } "
            "label { color: white; font-weight: bold; }", role_graident);
        
        GtkCssProvider *h_prov = gtk_css_provider_new();
        gtk_css_provider_load_from_data(h_prov, header_css, -1, NULL);
        gtk_style_context_add_provider(gtk_widget_get_style_context(card_header), GTK_STYLE_PROVIDER(h_prov), GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
        g_object_unref(h_prov);

        // Icon/Avatar
        GtkWidget *avatar = gtk_image_new_from_icon_name(role_icon_name, GTK_ICON_SIZE_DND); // Large icon
        gtk_image_set_pixel_size(GTK_IMAGE(avatar), 40);
        // Make icon white
        // This is tricky in pure GTK3 CSS without SVG tampering, but let's try a filter or just minimal style
        // Actually gtk icons follow text color usually if symbolic, but these are standard.
        // We'll leave them as is, they look okay on colored backgrounds. 
        gtk_box_pack_start(GTK_BOX(card_header), avatar, FALSE, FALSE, 0);

        // Name & ID container
        GtkWidget *name_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
        GtkWidget *name_lbl = gtk_label_new(NULL);
        char name_markup[128];
        snprintf(name_markup, sizeof(name_markup), "<span size='large'>%s</span>", cur->name);
        gtk_label_set_markup(GTK_LABEL(name_lbl), name_markup);
        gtk_widget_set_halign(name_lbl, GTK_ALIGN_START);
        
        char id_str[32]; snprintf(id_str, sizeof(id_str), "ID: #%d", cur->id);
        GtkWidget *id_lbl = gtk_label_new(NULL);
        char id_markup[64]; snprintf(id_markup, sizeof(id_markup), "<span size='small' alpha='80%%'>%s</span>", id_str);
        gtk_label_set_markup(GTK_LABEL(id_lbl), id_markup);
        gtk_widget_set_halign(id_lbl, GTK_ALIGN_START);

        gtk_box_pack_start(GTK_BOX(name_box), name_lbl, FALSE, FALSE, 0);
        gtk_box_pack_start(GTK_BOX(name_box), id_lbl, FALSE, FALSE, 0);
        gtk_box_pack_start(GTK_BOX(card_header), name_box, TRUE, TRUE, 0);

        gtk_box_pack_start(GTK_BOX(card_vbox), card_header, FALSE, FALSE, 0);

        // --- Card Body ---
        GtkWidget *card_body = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
        gtk_widget_set_margin_start(card_body, 20);
        gtk_widget_set_margin_end(card_body, 20);
        gtk_widget_set_margin_top(card_body, 20);
        gtk_widget_set_margin_bottom(card_body, 20);

        // Role Line
        GtkWidget *role_row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
        GtkWidget *role_lbl_title = gtk_label_new("Rôle:");
        gtk_style_context_add_class(gtk_widget_get_style_context(role_lbl_title), "dim-label"); // Assuming we might add this class or just inline style
        GtkWidget *role_val = gtk_label_new(NULL);
        char role_markup[128]; snprintf(role_markup, sizeof(role_markup), "<span weight='bold' foreground='#2c3e50'>%s</span>", cur->role);
        gtk_label_set_markup(GTK_LABEL(role_val), role_markup);
        
        gtk_box_pack_start(GTK_BOX(role_row), role_lbl_title, FALSE, FALSE, 0);
        gtk_box_pack_end(GTK_BOX(role_row), role_val, FALSE, FALSE, 0);
        gtk_box_pack_start(GTK_BOX(card_body), role_row, FALSE, FALSE, 0);

        // Hours Line
        GtkWidget *hours_row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
        GtkWidget *hours_title = gtk_label_new("Heures:");
        GtkWidget *hours_badge = gtk_label_new(NULL);
        char h_str[64]; snprintf(h_str, sizeof(h_str), "%.2f h", cur->hours_worked);
        char h_markup[128]; snprintf(h_markup, sizeof(h_markup), "<span background='#ecf0f1' foreground='#2c3e50' size='small'><b> %s </b></span>", h_str);
        gtk_label_set_markup(GTK_LABEL(hours_badge), h_markup); // GTK native markup doesn't support background on span well in 3.0 everywhere, but text works.
        // Let's use a simpler badge approach if needed, or just bold text.
        // Simple bold text for now to be safe.
        gtk_label_set_markup(GTK_LABEL(hours_badge), g_markup_printf_escaped("<span weight='bold'>%s</span>", h_str));

        gtk_box_pack_start(GTK_BOX(hours_row), hours_title, FALSE, FALSE, 0);
        gtk_box_pack_end(GTK_BOX(hours_row), hours_badge, FALSE, FALSE, 0);
        gtk_box_pack_start(GTK_BOX(card_body), hours_row, FALSE, FALSE, 0);

        // Separator
        GtkWidget *sep = gtk_separator_new(GTK_ORIENTATION_HORIZONTAL);
        gtk_box_pack_start(GTK_BOX(card_body), sep, FALSE, FALSE, 5);

        // Actions
        GtkWidget *actions_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
        gtk_box_set_homogeneous(GTK_BOX(actions_box), TRUE);
        
        // Edit
        GtkWidget *edit_btn = gtk_button_new_with_label("Modifier");
        // Style edit button
        GtkCssProvider *edit_prov = gtk_css_provider_new();
        gtk_css_provider_load_from_data(edit_prov, "button { background: transparent; color: #3498db; border: 1px solid #3498db; } button:hover { background: #3498db; color: white; }", -1, NULL);
        gtk_style_context_add_provider(gtk_widget_get_style_context(edit_btn), GTK_STYLE_PROVIDER(edit_prov), GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
        g_object_unref(edit_prov);
        
        // Delete
        GtkWidget *del_btn = gtk_button_new_with_label("Supprimer");
         GtkCssProvider *del_prov = gtk_css_provider_new();
        gtk_css_provider_load_from_data(del_prov, "button { background: transparent; color: #e74c3c; border: 1px solid #e74c3c; } button:hover { background: #e74c3c; color: white; }", -1, NULL);
        gtk_style_context_add_provider(gtk_widget_get_style_context(del_btn), GTK_STYLE_PROVIDER(del_prov), GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
        g_object_unref(del_prov);


        // Connect signals (need to alloc IDs)
        int *idp = malloc(sizeof(int)); *idp = cur->id; 
        int *idp2 = malloc(sizeof(int)); *idp2 = cur->id;
        g_signal_connect(edit_btn, "clicked", G_CALLBACK(on_edit_personnel_clicked), idp);
        g_signal_connect(del_btn, "clicked", G_CALLBACK(on_delete_personnel_clicked), idp2);

        gtk_box_pack_start(GTK_BOX(actions_box), edit_btn, TRUE, TRUE, 0);
        gtk_box_pack_start(GTK_BOX(actions_box), del_btn, TRUE, TRUE, 0);
        
        gtk_box_pack_start(GTK_BOX(card_body), actions_box, FALSE, FALSE, 0);
        
        gtk_box_pack_start(GTK_BOX(card_vbox), card_body, TRUE, TRUE, 0);

        // --- Frame Styling ---
        GtkCssProvider *frame_prov = gtk_css_provider_new();
        gtk_css_provider_load_from_data(frame_prov, 
            "frame { background: white; border-radius: 12px; border: 1px solid #dfe6e9; box-shadow: 0 2px 5px rgba(0,0,0,0.05); } "
            "frame:hover { box-shadow: 0 5px 15px rgba(0,0,0,0.1); transform: translateY(-2px); }", -1, NULL);
        gtk_style_context_add_provider(gtk_widget_get_style_context(card_frame), GTK_STYLE_PROVIDER(frame_prov), GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
        g_object_unref(frame_prov);

        gtk_container_add(GTK_CONTAINER(card_frame), card_vbox);
        gtk_flow_box_insert(GTK_FLOW_BOX(flowbox), card_frame, -1);
    }

    gtk_container_add(GTK_CONTAINER(scrolled), flowbox);
    gtk_box_pack_start(GTK_BOX(personnel_box), scrolled, TRUE, TRUE, 0);

    gtk_container_add(GTK_CONTAINER(content_area), personnel_box);
    gtk_widget_show_all(content_area);
}

static gboolean on_stats_draw(GtkWidget *widget, cairo_t *cr, gpointer user_data){
    (void)user_data;
    int ids[256]; int counts[256]; int n=0;
    FILE *f = fopen("data/statistiques.txt","r");
    if(f){
        int id,c;
        while(n<256 && fscanf(f, "%d|%d\n", &id, &c)==2){ 
            if(find_plat_by_id(NULL, id) != NULL) {
                ids[n]=id; counts[n]=c; n++; 
            }
        }
        fclose(f);
    }

    // sort
    for(int i=0;i<n;i++){
        for(int j=i+1;j<n;j++) if(counts[j]>counts[i]){
            int t=counts[i]; counts[i]=counts[j]; counts[j]=t;
            int ti=ids[i]; ids[i]=ids[j]; ids[j]=ti;
        }
    }

    int width = gtk_widget_get_allocated_width(widget);
    int height = gtk_widget_get_allocated_height(widget);
    
    // Background
    cairo_set_source_rgb(cr, 1.0, 1.0, 1.0);
    cairo_paint(cr);

    if(n==0){
        cairo_set_source_rgb(cr, 0.6, 0.6, 0.6);
        cairo_select_font_face(cr, "Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
        cairo_set_font_size(cr, 16.0);
        cairo_move_to(cr, width/2 - 100, height/2);
        cairo_show_text(cr, "Aucune donnée de ventes disponible");
        return FALSE;
    }

    // Chart Layout
    int margin = 50;
    int chart_w = width - 2*margin;
    int chart_h = height - 2*margin;
    int top = n < 8 ? n : 8; // Top 8 items
    int maxc = 1; for(int i=0;i<top;i++) if(counts[i]>maxc) maxc = counts[i];

    // --- Grid Lines ---
    cairo_set_line_width(cr, 1.0);
    int ticks = 5;
    for(int t=0; t<=ticks; t++){
        double y = margin + chart_h - (t * (double)chart_h/ticks);
        if(t==0) cairo_set_source_rgba(cr, 0.2, 0.2, 0.2, 0.5); // Baseline
        else cairo_set_source_rgba(cr, 0.9, 0.9, 0.9, 1.0); // Grid
        
        cairo_move_to(cr, margin, y);
        cairo_line_to(cr, margin + chart_w, y);
        cairo_stroke(cr);
        
        // Tick labels
        if(t>0){
            char buf[32]; snprintf(buf, sizeof(buf), "%d", (int)(t * (double)maxc/ticks));
            cairo_set_source_rgb(cr, 0.6, 0.6, 0.6);
            cairo_set_font_size(cr, 10.0);
            cairo_move_to(cr, margin - 30, y + 4);
            cairo_show_text(cr, buf);
        }
    }

    // --- Bars ---
    double bar_width = (double)chart_w / top * 0.6;
    double spacing = (double)chart_w / top * 0.4;
    
    for(int i=0; i<top; i++){
        double h = (double)counts[i] / maxc * chart_h;
        double x = margin + i * ((double)chart_w / top) + spacing/2;
        double y = margin + chart_h - h;
        
        // Bar Gradient
        cairo_pattern_t *pat = cairo_pattern_create_linear(x, y, x, y+h);
        double hue = (double)i/top; 
        // Simple colorful palette logic
        if(i==0) { cairo_pattern_add_color_stop_rgb(pat, 0, 0.9, 0.3, 0.3); cairo_pattern_add_color_stop_rgb(pat, 1, 0.7, 0.2, 0.2); } // Red
        else if(i==1) { cairo_pattern_add_color_stop_rgb(pat, 0, 0.9, 0.6, 0.2); cairo_pattern_add_color_stop_rgb(pat, 1, 0.8, 0.5, 0.1); } // Orange
        else if(i==2) { cairo_pattern_add_color_stop_rgb(pat, 0, 0.2, 0.8, 0.4); cairo_pattern_add_color_stop_rgb(pat, 1, 0.1, 0.6, 0.3); } // Green
        else { 
            cairo_pattern_add_color_stop_rgb(pat, 0, 0.2, 0.6, 0.9); 
            cairo_pattern_add_color_stop_rgb(pat, 1, 0.1, 0.4, 0.7); 
        } // Blueish for others

        // Rounded top bars
        double r = 6.0;
        if(h < r) r = h/2; // safety
        
        cairo_new_path(cr);
        cairo_move_to(cr, x, y+h);
        cairo_line_to(cr, x, y+r);
        cairo_arc(cr, x+r, y+r, r, -M_PI, -M_PI/2);
        cairo_line_to(cr, x+bar_width-r, y);
        cairo_arc(cr, x+bar_width-r, y+r, r, -M_PI/2, 0);
        cairo_line_to(cr, x+bar_width, y+h);
        cairo_close_path(cr);
        
        cairo_set_source(cr, pat);
        cairo_fill(cr);
        cairo_pattern_destroy(pat);

        // Value Label on top
        char vbuf[32]; snprintf(vbuf, sizeof(vbuf), "%d", counts[i]);
        cairo_set_source_rgb(cr, 0.3, 0.3, 0.3);
        cairo_select_font_face(cr, "Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
        cairo_set_font_size(cr, 11.0);
        cairo_move_to(cr, x + bar_width/2 - 4, y - 5);
        cairo_show_text(cr, vbuf);
        
        // Name Label below
        Plat *p = find_plat_by_id(NULL, ids[i]);
        char name[64];
        if(p) strncpy(name, p->name, sizeof(name)-1); 
        else snprintf(name, sizeof(name), "Ancien (#%d)", ids[i]);
        name[sizeof(name)-1] = '\0';
        
        cairo_save(cr);
        cairo_translate(cr, x + bar_width/2, margin + chart_h + 15);
        cairo_rotate(cr, -0.3); // Slight tilt
        cairo_set_source_rgb(cr, 0.4, 0.4, 0.4);
        cairo_set_font_size(cr, 10.0);
        cairo_move_to(cr, 0, 0); // centered pivot
        cairo_text_extents_t ext;
        cairo_text_extents(cr, name, &ext);
        cairo_move_to(cr, -ext.width/2, 0);
        cairo_show_text(cr, name);
        cairo_restore(cr);
    }
    
    // Title
    cairo_set_source_rgb(cr, 0.2, 0.2, 0.2);
    cairo_select_font_face(cr, "Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
    cairo_set_font_size(cr, 14.0);
    cairo_move_to(cr, margin, margin - 20);
    cairo_show_text(cr, "Ventes par Plat (Top 8)");

    return FALSE;
}

static void show_stats_content(void) {
    gtk_container_foreach(GTK_CONTAINER(content_area), (GtkCallback)gtk_widget_destroy, NULL);
    
    GtkWidget *stats_container = gtk_box_new(GTK_ORIENTATION_VERTICAL, 20);
    gtk_widget_set_margin_start(stats_container, 25);
    gtk_widget_set_margin_end(stats_container, 25);
    gtk_widget_set_margin_top(stats_container, 20);
    gtk_widget_set_margin_bottom(stats_container, 20);
    
    // Header
    GtkWidget *header = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(header), "<span size='xx-large' weight='bold' foreground='#2C3E50'>📈 Tableau de Bord Statistiques</span>");
    gtk_widget_set_halign(header, GTK_ALIGN_START);
    gtk_box_pack_start(GTK_BOX(stats_container), header, FALSE, FALSE, 10);

    // ========== KPI CARDS ==========
    GtkWidget *kpi_grid = gtk_grid_new();
    gtk_grid_set_column_spacing(GTK_GRID(kpi_grid), 20);
    gtk_grid_set_column_homogeneous(GTK_GRID(kpi_grid), TRUE);
    
    // Calculate Stats
    double ca = compute_total_ca();
    int nb_cmd = compute_commandes_count();
    
    double avg_cart = nb_cmd > 0 ? ca / nb_cmd : 0.0;
    
    struct KPIData {
        const char *title;
        const char *value;
        const char *icon;
        const char *color1;
        const char *color2;
    };
    
    char ca_str[64]; snprintf(ca_str, sizeof(ca_str), "%.2f €", ca);
    char cmd_str[64]; snprintf(cmd_str, sizeof(cmd_str), "%d", nb_cmd);
    char avg_str[64]; snprintf(avg_str, sizeof(avg_str), "%.2f €", avg_cart);
    
    struct KPIData kpis[] = {
        {"Chiffre d'Affaires", ca_str, "emblem-money", "#27ae60", "#2ecc71"},
        {"Total Commandes", cmd_str, "emblem-documents", "#2980b9", "#3498db"},
        {"Panier Moyen", avg_str, "emblem-symbolic-link", "#8e44ad", "#9b59b6"}, 
    };

    for(int i=0; i<3; i++){
        GtkWidget *card = gtk_frame_new(NULL);
        gtk_frame_set_shadow_type(GTK_FRAME(card), GTK_SHADOW_NONE);
        
        GtkWidget *card_vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
        
        // CSS for Gradient Background
        char css[512];
        snprintf(css, sizeof(css), 
            "frame { background: linear-gradient(135deg, %s, %s); border-radius: 12px; box-shadow: 0 4px 10px rgba(0,0,0,0.1); padding: 20px; }", 
            kpis[i].color1, kpis[i].color2);
        
        GtkCssProvider *p = gtk_css_provider_new();
        gtk_css_provider_load_from_data(p, css, -1, NULL);
        gtk_style_context_add_provider(gtk_widget_get_style_context(card), GTK_STYLE_PROVIDER(p), GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
        g_object_unref(p);

        // Content
        GtkWidget *icon = gtk_image_new_from_icon_name(kpis[i].icon, GTK_ICON_SIZE_DIALOG);
        gtk_image_set_pixel_size(GTK_IMAGE(icon), 48);
        gtk_widget_set_halign(icon, GTK_ALIGN_END); // Icon top right? Or maybe left. Let's do a hbox.
        
        GtkWidget *hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
        
        GtkWidget *texts = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
        GtkWidget *lbl_title = gtk_label_new(NULL);
        char t_mk[128]; snprintf(t_mk, sizeof(t_mk), "<span foreground='white' alpha='80%%' weight='bold'>%s</span>", kpis[i].title);
        gtk_label_set_markup(GTK_LABEL(lbl_title), t_mk);
        gtk_widget_set_halign(lbl_title, GTK_ALIGN_START);
        
        GtkWidget *lbl_val = gtk_label_new(NULL);
        char v_mk[128]; snprintf(v_mk, sizeof(v_mk), "<span foreground='white' size='xx-large' weight='bold'>%s</span>", kpis[i].value);
        gtk_label_set_markup(GTK_LABEL(lbl_val), v_mk);
        gtk_widget_set_halign(lbl_val, GTK_ALIGN_START);
        
        gtk_box_pack_start(GTK_BOX(texts), lbl_title, FALSE, FALSE, 0);
        gtk_box_pack_start(GTK_BOX(texts), lbl_val, FALSE, FALSE, 0);
        
        gtk_box_pack_start(GTK_BOX(hbox), texts, TRUE, TRUE, 0);
        gtk_box_pack_end(GTK_BOX(hbox), icon, FALSE, FALSE, 0);
        
        gtk_container_add(GTK_CONTAINER(card_vbox), hbox);
        gtk_container_add(GTK_CONTAINER(card), card_vbox);
        
        gtk_grid_attach(GTK_GRID(kpi_grid), card, i, 0, 1, 1);
    }
    
    gtk_box_pack_start(GTK_BOX(stats_container), kpi_grid, FALSE, FALSE, 15);

    // ========== CHART & LIST SECTION ==========
    GtkWidget *content_grid = gtk_grid_new();
    gtk_grid_set_column_spacing(GTK_GRID(content_grid), 20);
    gtk_grid_set_row_spacing(GTK_GRID(content_grid), 20);
    gtk_widget_set_vexpand(content_grid, TRUE);
    
    // 1. Chart (Takes 2/3 width)
    GtkWidget *chart_frame = gtk_frame_new(NULL);
    gtk_widget_set_hexpand(chart_frame, TRUE);
    gtk_widget_set_vexpand(chart_frame, TRUE);
    
    GtkCssProvider *chart_prov = gtk_css_provider_new();
    gtk_css_provider_load_from_data(chart_prov, 
        "frame { background: white; border-radius: 12px; border: 1px solid #dfe6e9; box-shadow: 0 2px 5px rgba(0,0,0,0.05); }", -1, NULL);
    gtk_style_context_add_provider(gtk_widget_get_style_context(chart_frame), GTK_STYLE_PROVIDER(chart_prov), GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
    g_object_unref(chart_prov);
    
    GtkWidget *drawing = gtk_drawing_area_new();
    gtk_container_add(GTK_CONTAINER(chart_frame), drawing);
    g_signal_connect(drawing, "draw", G_CALLBACK(on_stats_draw), NULL);
    
    gtk_grid_attach(GTK_GRID(content_grid), chart_frame, 0, 0, 2, 1); // 2 cols wide

    // 2. Info Side Panel (Takes 1/3 width, if we had more info, skipping for now to let chart breathe or maybe add a "Top Actions" list)
    // Actually, let's keep it simple and just use the chart big for "Wow" effect.
    
    gtk_box_pack_start(GTK_BOX(stats_container), content_grid, TRUE, TRUE, 0);

    gtk_container_add(GTK_CONTAINER(content_area), stats_container);
    gtk_widget_show_all(content_area);
}

void init_ui(int *argc, char ***argv) {
    gtk_init(argc, argv);
    show_login_window();
    gtk_main();
}