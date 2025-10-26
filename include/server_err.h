#include <stdio.h>

#define ERR(fmt, ...)   fprintf(stdout, "[ERROR] %s:%d:%s(): " fmt "\n", __FILE__, __LINE__, __func__, ##__VA_ARGS__)
#define WARN(fmt, ...)  fprintf(stdout, "[WARN ] %s:%d:%s(): " fmt "\n", __FILE__, __LINE__, __func__, ##__VA_ARGS__)
#define TRACE(fmt, ...) fprintf(stdout, "[TRACE] %s:%d:%s(): " fmt "\n", __FILE__, __LINE__, __func__, ##__VA_ARGS__)