#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "include/plats.h"

static Plat *plats_head = NULL;
static int last_id = 0;
static const char *PLATS_PATH = "data/plats.txt";
static const char *SALES_PATH = "data/statistiques.txt";

void load_plats(const char *path){
	const char *p = path?path:PLATS_PATH;
	FILE *f = fopen(p,"r");
	if(!f) return;
	Plat tmp;


	while(fscanf(f, "%d|%127[^|]|%63[^|]|%lf|%d|%lf|%lf|%511[^\n]\n", &tmp.id, tmp.name, tmp.category, &tmp.price, &tmp.available, &tmp.cost, &tmp.prep_time, tmp.ingredients) == 8) {
		Plat *n = malloc(sizeof(Plat));
		*n = tmp;
		n->next = plats_head;
		plats_head = n;
		if(tmp.id>last_id) last_id = tmp.id;
	}
	fclose(f);
}

void save_plats(const char *path){
	const char *p = path?path:PLATS_PATH;
	char tmp_path[512]; snprintf(tmp_path, sizeof(tmp_path), "%s.tmp", p);
	FILE *f = fopen(tmp_path,"w");
	if(!f) return;
	for(Plat *it=plats_head; it; it=it->next){
		fprintf(f,"%d|%s|%s|%.2f|%d|%.2f|%.2f|%s\n",
				it->id, it->name, it->category, it->price, it->available, it->cost, it->prep_time, it->ingredients);
	}
	fflush(f); fclose(f);
	remove(p); rename(tmp_path, p);
}

Plat* add_plat(Plat *head, Plat p){
	Plat *n = malloc(sizeof(Plat));
	memset(n,0,sizeof(Plat));
	n->id = ++last_id;
	strcpy(n->name,p.name);
	strcpy(n->category,p.category);
	n->price = p.price;
	n->available = p.available;
	n->cost = p.cost;
	n->prep_time = p.prep_time;
	strcpy(n->ingredients,p.ingredients);
	n->next = plats_head;
	plats_head = n;
	save_plats(NULL);
	return plats_head;
}

Plat* find_plat_by_id(Plat *head, int id){
	for(Plat *it=plats_head; it; it=it->next) if(it->id==id) return it;
	return NULL;
}

Plat* remove_plat(Plat *head, int id){
	Plat *cur=plats_head,*prev=NULL;
	while(cur){
		if(cur->id==id){
			if(prev) prev->next = cur->next;
			else plats_head = cur->next;
			free(cur);
			save_plats(NULL);
			return plats_head;
		}
		prev=cur; cur=cur->next;
	}
	return plats_head;
}

void update_plat(Plat *p){
	save_plats(NULL);
}

void free_plats(Plat *head){
	Plat *it=plats_head;
	while(it){
		Plat *n=it->next; free(it); it=n;
	}
	plats_head=NULL;
}

Plat* get_plat_list(){ return plats_head; }
int next_plat_id(){ return last_id+1; }


int count_plat_sales(int plat_id){
	FILE *f = fopen(SALES_PATH,"r");
	if(!f) return 0;
	int id,c; while(fscanf(f,"%d|%d\n",&id,&c)==2){ if(id==plat_id){ fclose(f); return c; } }
	fclose(f); return 0;
}

void increment_plat_sales(int plat_id){

	FILE *f = fopen(SALES_PATH,"r");
	int arr[1024]; int ids[1024]; int n=0;
	if(f){
		int id,c;
		while(fscanf(f,"%d|%d\n",&id,&c)==2){ ids[n]=id; arr[n]=c; n++; }
		fclose(f);
	}
	int i; for(i=0;i<n;i++) if(ids[i]==plat_id){ arr[i]++; break; }
	if(i==n){ ids[n]=plat_id; arr[n]=1; n++; }
	f = fopen(SALES_PATH,"w");
	if(!f) return;
	for(i=0;i<n;i++) fprintf(f,"%d|%d\n",ids[i],arr[i]);
	fclose(f);
}