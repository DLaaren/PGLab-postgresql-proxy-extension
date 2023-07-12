#ifndef PROXY_LOG_H
#define PROXY_LOG_H

/* 
 * Opening logfile.
 */
extern void log_open();

/* Write message in logfile. 
 * Opens logfile if it closed.
 * Args is the same as in "printf", "message" can be also as a format.
 * Sym "/n" is placed automatically.
 */
extern void log_write(char* message, ...);

/*
 * Close logfile. 
*/
extern void log_close();

#endif /* PROXY_LOG_H */