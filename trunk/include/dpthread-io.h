#ifndef DPTHREAD_IO_H
#define DPTHREAD_IO_H

#include <stdint.h> 
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <unistd.h>
#include <signal.h>
#include <stdio.h>
#include <malloc.h>
#include <string.h>
#include <time.h>


///////////////////////////////////////////////////////////////////////////////////
// Extennal library APIs  
///////////////////////////////////////////////////////////////////////////////////

// unistd.h 
unsigned int  detio_sleep(unsigned int seconds);
int detio_usleep(unsigned int usecs); 
// int detio_usleep(useconds_t usec);
long detio_sysconf(int name);
int detio_getopt(int argc, char * const argv[], const char *optstring);

// signal.h 
int detio_sigwait(const sigset_t *set, int *sig); 

// stdio.h 
int detio_fprintf(FILE *fp, const char *format, ...); 
char *detio_fgets(char *s, int size, FILE *stream);
int detio_fgetc(FILE *stream);
int detio_fscanf(FILE *stream, const char *format, ...);
int detio_sscanf(const char *str, const char *format, ...);
int detio_fclose(FILE *fp);
FILE *detio_fopen(const char *path, const char *mode);
int detio_fflush(FILE *stream);
int detio_snprintf(char *str, size_t size, const char *format, ...); 
int detio_putchar(int c); 

// malloc.h
void *detio_valloc(size_t size); 
void detio_free(void *ptr); 

// sys/time.h 
int detio_gettimeofday(struct timeval *tv, void *tz); 

///////////////////////////////////////////////////////////////////////////////////
// System call APIs  
///////////////////////////////////////////////////////////////////////////////////
void det_FD_ZERO(fd_set *set); 

ssize_t detio_pread(int fd, void *buf, size_t count, off_t offset);
ssize_t detio_pwrite(int fd, const void *buf, size_t count, off_t offset);

ssize_t detio_read(int fd, void *buf, size_t count);
ssize_t detio_write(int fd, const void *buf, size_t count);

int detio_stat(const char *path, struct stat *buf);
int detio_fstat(int fd, struct stat *buf);

int detio_open(const char *pathname, int flags);

ssize_t detio_recv(int sockfd, void *buf, size_t len, int flags);
ssize_t detio_send(int sockfd, const void *buf, size_t len, int flags);

int detio_select(int nfds, fd_set *readfds, fd_set *writefds,
	   fd_set *exceptfds, struct timeval *timeout);

#endif /* DPTHREAD_IO_H */ 
