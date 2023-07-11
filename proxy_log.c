#include "stdio.h"
#include "proxy_log.h"
#include "string.h"
#include "stdarg.h"

typedef struct ProxyLog
{
    FILE* file;
} ProxyLog;

ProxyLog proxy_log;

void log_open() {
    proxy_log.file = fopen("logfile.log", "w");
}

void log_write(char *message, ...) {
    if (NULL == proxy_log.file) {
        log_open();
    }
    if (NULL == proxy_log.file) {
        perror("ERROR: logfile could not be opened.");
        return;
    }
    va_list vl;
    va_start(vl, message);
    vfprintf(proxy_log.file, message, vl);
    va_end(vl);
    fprintf(proxy_log.file, "\n");
}

void log_close() {
    if (NULL != proxy_log.file) {
        fclose(proxy_log.file);
    }
}

 