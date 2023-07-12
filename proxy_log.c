#include <stdio.h>
#include <string.h>
#include <stdarg.h>

#include "proxy_log.h"
#include "time.h"
#include "sys/time.h"
#include "unistd.h"

typedef struct ProxyLog
{
    FILE* file;
} ProxyLog;


ProxyLog proxy_log;

char *log_type_names[] = {
    "INFO",
    "WARNING",
    "ERROR"
};

void write_format();

void 
log_open()
{
    proxy_log.file = fopen("logfile.log", "w");
    if (NULL == proxy_log.file)
    {
        perror("ERROR: logfile could not be opened.");
    }
}

void
log_write(MessageType type, char *message, ...)
{
    if (NULL == proxy_log.file)
    {
        log_open();
    }
    if (NULL == proxy_log.file)
    {
        perror("ERROR: you can't write in logfile.");
        return;
    }

    write_format();

    fprintf(proxy_log.file, "%s: ", log_type_names[type]);
    va_list vl;
    va_start(vl, message);
    vfprintf(proxy_log.file, message, vl);
    va_end(vl);
    fprintf(proxy_log.file, "\n");
}

void
log_close()
{
    if (NULL != proxy_log.file)
    {
        fclose(proxy_log.file);
    }
}

void write_format() {
    time_t currentTime;
    struct tm *localTime;
    char timeString[100];
    currentTime = time(NULL);
    localTime = localtime(&currentTime);
    struct timeval currentAcTime;
    gettimeofday(&currentAcTime, NULL);

    long milliseconds = currentAcTime.tv_usec / 1000;

    /*Date format.*/
    strftime(timeString, sizeof(timeString), "%Y:%m:%d", localTime);
    fprintf(proxy_log.file, "%s ", timeString);  

    /*Time format.*/
    strftime(timeString, sizeof(timeString), "%H:%M:%S", localTime);
    fprintf(proxy_log.file, "%s.%.3ld ", timeString, milliseconds);

    /*Time zone format.*/
    strftime(timeString, sizeof(timeString), "%z", localTime);
    fprintf(proxy_log.file, "%.3s ", timeString);

    /*PID*/
    fprintf(proxy_log.file, "[%d] LOG: ", getpid());
} 