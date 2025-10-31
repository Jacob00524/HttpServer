#include "http.h"

int read_config(char *path, Server_Settings *settings);
int create_config(char *config_path, Server_Settings *settings);
void config_free(Server_Settings *settings);
int create_folder(char *name);
int create_file(char *text, char *folder, char *name);