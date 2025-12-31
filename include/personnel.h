#ifndef PERSONNEL_H
#define PERSONNEL_H

typedef struct Employe {
    int id;
    char name[128];
    char role[32];
    double hours_worked;
    struct Employe *prev;
    struct Employe *next;
} Employe;

void load_personnel(const char *path);
void save_personnel(const char *path);
Employe* add_employe(Employe *head, Employe e);
int update_employe(int id, const char *name, const char *role, double hours);
int delete_employe(int id);
int next_employe_id();
Employe* find_employe_by_id(int id);
void free_personnel();
Employe* get_personnel_list();

#endif
