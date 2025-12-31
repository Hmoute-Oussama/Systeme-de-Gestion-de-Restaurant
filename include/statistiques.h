#ifndef STATISTIQUES_H
#define STATISTIQUES_H

void compute_top5_plats(char out[1024]);
void compute_sales_by_day_csv(const char *outpath);
double compute_total_ca(void);
int compute_commandes_count(void);

#endif
