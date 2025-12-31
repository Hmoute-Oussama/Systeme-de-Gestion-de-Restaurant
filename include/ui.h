#ifndef UI_H
#define UI_H

#include <gtk/gtk.h>

void init_ui(int *argc, char ***argv);
void show_login_window();
void show_main_window(const char *role);

#endif
