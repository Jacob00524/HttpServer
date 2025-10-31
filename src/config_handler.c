#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <errno.h>
#include "config_handler.h"
#include "cJSON.h"
#include "server_err.h"

int read_config(char *path, Server_Settings *settings)
{
    FILE *json_file;
    cJSON *json_root, *json_obj;
    char *json_string;
    size_t json_length;
    
    json_file = fopen(path, "r");
    if (!json_file)
        return 0;

    fseek(json_file, 0, SEEK_END);
    json_length = ftell(json_file);
    fseek(json_file, 0, SEEK_SET);
    json_string = malloc((json_length * sizeof(char)) + 1);
    fread(json_string, sizeof(char), json_length, json_file);
    fclose(json_file);
    json_string[json_length] = 0;

    json_root = cJSON_Parse(json_string);
    free(json_string);
    if (!json_root)
        return -1;

    json_obj = cJSON_GetObjectItem(json_root, "content_folder");
    settings->content_folder = cJSON_GetStringValue(json_obj);
    
    json_obj = cJSON_GetObjectItem(json_root, "error_folder");
    settings->error_folder = cJSON_GetStringValue(json_obj);

    json_obj = cJSON_GetObjectItem(json_root, "index_name");
    settings->index_name = cJSON_GetStringValue(json_obj);

    json_obj = cJSON_GetObjectItem(json_root, "port");
    settings->port = cJSON_GetNumberValue(json_obj);

    json_obj = cJSON_GetObjectItem(json_root, "address");
    settings->address = cJSON_GetStringValue(json_obj);

    json_obj = cJSON_GetObjectItem(json_root, "max_queue");
    settings->max_queue = cJSON_GetNumberValue(json_obj);

    cJSON_free(json_obj);
    cJSON_free(json_root);

    return 1;
}

int create_config(char *config_path, Server_Settings *settings)
{
    FILE *json_file;
    cJSON *json_root = cJSON_CreateObject();
    char *cjson_string;

    settings->content_folder = "content";
    settings->error_folder = "error_content";
    settings->index_name = "index.html";
    settings->port = 80;
    settings->address = "127.0.0.1";
    settings->max_queue = 5;

    cJSON_AddStringToObject(json_root, "content_folder", settings->content_folder);
    cJSON_AddStringToObject(json_root, "error_folder", settings->error_folder);
    cJSON_AddStringToObject(json_root, "index_name", settings->index_name);
    cJSON_AddNumberToObject(json_root, "port", settings->port);
    cJSON_AddStringToObject(json_root, "address", settings->address);
    cJSON_AddNumberToObject(json_root, "max_queue", settings->max_queue);

    json_file = fopen(config_path, "w");
    if (!json_file)
        return 0;

    cjson_string = cJSON_Print(json_root);
    fwrite(cjson_string, sizeof(char), strlen(cjson_string), json_file);
    fclose(json_file);
    free(cjson_string);
    cJSON_free(json_root);

    return 1;
}

void config_free(Server_Settings *settings)
{
    free(settings->content_folder);
    free(settings->error_folder);
    free(settings->index_name);
    free(settings->address);
    memset(settings, 0, sizeof(*settings));
}

int create_folder(char *name)
{
    if (mkdir(name, 0755) == 0)
    {
        TRACE("Created folder (%s).\n", name);
        return 1;
    } else
    {
        if (errno == EEXIST)
        {
            TRACE("Found folder (%s).\n", name);
            return 1;
        }
        else
        {
            ERR("Could not create folder (%s): errno=%d\n", name, errno);
            return 0;
        }
        return 1;
    }
}
