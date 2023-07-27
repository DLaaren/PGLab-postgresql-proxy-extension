/* contrib/proxy/proxy_log.h */

#ifndef PROXY_LOG_H
#define PROXY_LOG_H

typedef enum MessageType {
    LOG_INFO,
    LOG_WARNING,
    LOG_ERROR
} MessageType;

/* 
 * Opens logfile.
 */
extern void
log_open();

/* 
 * Writes message in logfile. 
 * Opens logfile if it closed.
 * Args is the same as in "printf", "message" can be also as a format.
 * Sym "/n" is placed automatically.
 */
extern void
log_write(MessageType type, char* message, ...);

/*
 * Closes logfile. 
*/
extern void
log_close();

#endif /* PROXY_LOG_H */