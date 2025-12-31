#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "include/config.h"

static Config cfg = {0, 10, "data"};
static const char *CONFIG_PATH = "data/config.txt";

void load_config(const char *path){
    const char *p = path?path:CONFIG_PATH;
    FILE *f = fopen(p,"r");
    if(!f) return;
    int dm,bi; char dir[256];
    if(fscanf(f,"%d|%d|%255[^\n]\n", &dm, &bi, dir)==3){
        cfg.dark_mode = dm; cfg.backup_interval_min = bi; strncpy(cfg.data_dir, dir,255);
    }
    fclose(f);
}

void save_config(const char *path){
    const char *p = path?path:CONFIG_PATH;
    FILE *f = fopen(p,"w");
    if(!f) return;
    fprintf(f,"%d|%d|%s\n", cfg.dark_mode, cfg.backup_interval_min, cfg.data_dir);
    fclose(f);
}

Config* get_config(){ return &cfg; }
