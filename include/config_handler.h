typedef struct Server_Settings
{
    char *content_folder;
    char *error_folder;
    char *index_name;

    int port;
    char *address;
    int max_queue;
}Server_Settings;

int read_config(char *path, Server_Settings *settings);
int create_config(char *config_path, Server_Settings *settings);
void config_free(Server_Settings *settings);
