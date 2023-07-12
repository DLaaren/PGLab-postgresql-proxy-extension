#ifndef PROXY_LOG_H
#define PROXY_LOG_H

/* 
 * Opening logfile.
 */
void log_open();

/* Write message in logfile. 
 * Opens logfile if it closed.
 * Args is the same as in "print" but "message" can be also as a format.
 * Sym "/n" is placed automatically.
 */
void log_write(char* message, ...);

/*
 * Close logfile. 
*/
void log_close();

#endif /* PROXY_LOG_H */