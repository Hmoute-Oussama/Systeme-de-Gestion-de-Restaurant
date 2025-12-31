#ifndef CONFIG_H
#define CONFIG_H

typedef struct Config {
    int dark_mode;
    int backup_interval_min;
    char data_dir[256];
} Config;

void load_config(const char *path);
void save_config(const char *path);
Config* get_config();

#endif
