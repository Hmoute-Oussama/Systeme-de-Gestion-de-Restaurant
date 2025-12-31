#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "include/statistiques.h"
#include "include/plats.h"

void compute_top5_plats(char out[1024]){

    int ids[1024], counts[1024], n=0;
    FILE *f = fopen("data/statistiques.txt","r");
    if(!f){ strcpy(out,"No sales data\n"); return; }
    int id,c;
    while(fscanf(f,"%d|%d\n",&id,&c)==2){ ids[n]=id; counts[n]=c; n++; }
    fclose(f);

    for(int i=0;i<n;i++){
        for(int j=i+1;j<n;j++) if(counts[j]>counts[i]){
            int t=counts[i]; counts[i]=counts[j]; counts[j]=t;
            int ti=ids[i]; ids[i]=ids[j]; ids[j]=ti;
        }
    }
    char buf[1024]="\0";
    int limit = n<5?n:5;
    for(int i=0;i<limit;i++){
        Plat *p = find_plat_by_id(NULL, ids[i]);
        char line[256];
        if(p) snprintf(line,sizeof(line),"%d. %s (sold %d)\n", i+1, p->name, counts[i]);
        else snprintf(line,sizeof(line),"%d. Plat %d (sold %d)\n",i+1, ids[i], counts[i]);
        strncat(buf,line,sizeof(buf)-strlen(buf)-1);
    }
    strcpy(out,buf);
}

void compute_sales_by_day_csv(const char *outpath){

    FILE *in = fopen("data/statistiques.txt","r");
    if(!in) return;
    FILE *out = fopen(outpath,"w");
    if(!out){ fclose(in); return; }
    fprintf(out,"plat_id,sales\n");
    int id,c;
    while(fscanf(in,"%d|%d\n",&id,&c)==2) fprintf(out,"%d,%d\n",id,c);
    fclose(in); fclose(out);
}

double compute_total_ca(void){
    FILE *f = fopen("data/commandes.txt","r");
    if(!f) return 0.0;
    double total = 0.0;
    int id, client_id, statut;
    long long timestamp;
    double t;
    char rest[1024];
    while(fscanf(f, "%d|%d|%lld|%d|%lf|%1023[^\n]\n", &id, &client_id, &timestamp, &statut, &t, rest) == 6){
        total += t;
    }
    fclose(f);
    return total;
}

int compute_commandes_count(void){
    FILE *f = fopen("data/commandes.txt","r");
    if(!f) return 0;
    int count = 0;
    char line[2048];
    while(fgets(line, sizeof(line), f)){
        if(line[0] && line[0] != '\n') count++;
    }
    fclose(f);
    return count;
}
