#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "include/commandes.h"
#include "include/plats.h"
#include "include/statistiques.h"

static Commande *commandes_head = NULL;
static int last_id = 0;
static const char *COMMANDES_PATH = "data/commandes.txt";

static Ligne* parse_lignes(const char *s){

	Ligne *head=NULL;
	if(!s || !*s) return NULL;
	char *cp = strdup(s);
	char *tok = strtok(cp,",");
	while(tok){
		int id,qty;
		if(sscanf(tok,"%d:%d",&id,&qty)==2){
			Ligne *l = malloc(sizeof(Ligne));
			l->plat_id = id; l->quantity = qty; l->line_total = 0; l->next = head; head = l;
		}
		tok = strtok(NULL,",");
	}
	free(cp);
	return head;
}


void load_commandes(const char *path){
	const char *p = path?path:COMMANDES_PATH;
	FILE *f = fopen(p,"r");
	if(!f) return;


	free_commandes(commandes_head); 
	commandes_head = NULL;
	last_id = 0;

	int id, client_id, statut_int;
	long long timestamp_ll;
	double total;
	char lignestr[1024];
	
	Commande *tail = NULL;


	while(fscanf(f, "%d|%d|%lld|%d|%lf|%1023[^\n]\n", 
				 &id, &client_id, &timestamp_ll, &statut_int, &total, lignestr) == 6)
	{
		Commande *c = malloc(sizeof(Commande));
		if(!c) break;
		
		c->id = id;
		c->client_id = client_id;
		c->timestamp = (time_t)timestamp_ll;
		c->statut = (Statut)statut_int;
		c->total = total;
		c->lignes = parse_lignes(lignestr);
		c->next = NULL;

		if (commandes_head == NULL) {
			commandes_head = c;
			tail = c;
		} else {
			tail->next = c;
			tail = c;
		}
		
		if (id > last_id) {
			last_id = id;
		}
	}
	fclose(f);
}

void save_commandes(const char *path){
	const char *p = path?path:COMMANDES_PATH;
	char tmp_path[512];
	snprintf(tmp_path, sizeof(tmp_path), "%s.tmp", p);

	FILE *f = fopen(tmp_path, "w");
	if(!f) return;

	int count = 0;
	for(Commande *it=commandes_head; it; it=it->next) count++;
	fprintf(stderr, "save_commandes: writing %d commandes to %s\n", count, tmp_path);

	for(Commande *it=commandes_head; it; it=it->next){
		// basic validation: skip obviously-bad nodes
		if(it->id <= 0) continue;

		// serialize lignes as id:qty,....
		char buf[1024]="";
		for(Ligne *l=it->lignes; l; l=l->next){
			char tmp[64]; snprintf(tmp,sizeof(tmp),"%d:%d,", l->plat_id, l->quantity);
			strncat(buf,tmp,sizeof(buf)-strlen(buf)-1);
		}

		// write to temp file
		fprintf(f, "%d|%d|%lld|%d|%.2f|%s\n",
				it->id, it->client_id, (long long)it->timestamp, (int)it->statut, it->total, buf);
	}

	fflush(f);
	fclose(f);

	/* Replace the original file atomically where possible */
	if(remove(p) != 0) {
		fprintf(stderr, "save_commandes: remove(%s) returned non-zero (ignored)\n", p);
	}
	if(rename(tmp_path, p) != 0) {
		fprintf(stderr, "save_commandes: rename(%s,%s) failed\n", tmp_path, p);
	} else {
		fprintf(stderr, "save_commandes: successfully wrote %s\n", p);
	}
}

Commande* add_commande(Commande *head, Commande c){
	Commande *n = malloc(sizeof(Commande));
	memset(n,0,sizeof(Commande));
	n->id = ++last_id;
	n->client_id = c.client_id;
	n->lignes = c.lignes;
	// compute total and increment sales
	double tot = 0;
	for(Ligne *l=n->lignes; l; l=l->next){
		Plat *p = find_plat_by_id(NULL, l->plat_id);
		if(p){ l->line_total = p->price * l->quantity; tot += l->line_total; increment_plat_sales(p->id); }
	}
	n->total = tot;
	n->statut = c.statut;
	n->timestamp = time(NULL);
	n->next = commandes_head;
	commandes_head = n;
	save_commandes(NULL);
	return commandes_head;
}

Commande* find_commande_by_id(Commande *head, int id){
	for(Commande *it=commandes_head; it; it=it->next) if(it->id==id) return it;
	return NULL;
}

void update_commande_status(int id, Statut s){
	Commande *c = find_commande_by_id(NULL,id);
	if(!c) return;
	c->statut = s;
	save_commandes(NULL);
}

void free_commandes(Commande *head){
	Commande *it=commandes_head;
	while(it){
		Ligne *l = it->lignes;
		while(l){ Ligne *ln=l->next; free(l); l=ln; }
		Commande *n=it->next; free(it); it=n;
	}
	commandes_head=NULL;
}

Commande* get_commande_list(){ return commandes_head; }
int next_commande_id(){ return last_id+1; }

void generate_invoice(int commande_id, const char *outdir){
	Commande *c = find_commande_by_id(NULL, commande_id);
	if(!c) return;
	char path[512];
	snprintf(path,sizeof(path),"%s/invoice_%d.txt", outdir?outdir:"pdf", commande_id);
	FILE *f = fopen(path,"w");
	if(!f) return;
	fprintf(f,"Invoice #%d\n", c->id);
    //
	// CORRECTION 3: %ld est devenu %lld pour le timestamp
    //
	// cast timestamp to long long to match %%lld
	fprintf(f,"Timestamp: %lld\n", (long long)c->timestamp);
	fprintf(f,"Client ID: %d\n", c->client_id);
	fprintf(f,"Items:\n");
	for(Ligne *l=c->lignes; l; l=l->next){
		Plat *p = find_plat_by_id(NULL, l->plat_id);
		if(p) fprintf(f,"- %s x%d = %.2f\n", p->name, l->quantity, l->line_total);
	}
	fprintf(f,"Total: %.2f\n", c->total);
	fclose(f);
}