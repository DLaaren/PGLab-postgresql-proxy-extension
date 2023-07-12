#ifndef PROXY_LOG_H
#define PROXY_LOG_H

typedef enum MessageType {
    INFO,
    WARNING,
    ERROR
} MessageType;

/* 
 * Opening logfile.
 */
extern void log_open();

/* Write message in logfile. 
 * Opens logfile if it closed.
 * Args is the same as in "printf", "message" can be also as a format.
 * Sym "/n" is placed automatically.
 */
extern void log_write(MessageType type, char* message, ...);

/*
 * Close logfile. 
*/
extern void log_close();

#endif /* PROXY_LOG_H */