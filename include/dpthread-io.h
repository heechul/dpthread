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

// string.h 
void *detio_memset(void * dst, int s, size_t count); 

// stdlib.h 
char *detio_getenv(const char *name); 

// unistd.h 
unsigned int  detio_sleep(unsigned int seconds);
int detio_usleep(unsigned int usecs); 
// int detio_usleep(useconds_t usec);
long detio_sysconf(int name);
int detio_getopt(int argc, char * const argv[], const char *optstring);
char *detio_getcwd(char *buf, size_t size);

// signal.h 
int detio_sigwait(const sigset_t *set, int *sig); 
int detio_sigfillset(sigset_t *set); 

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
void *detio_realloc(void *ptr, size_t size); 

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

ssize_t detio_recv(int sockfd, void *buf, size_t len, int flags);
ssize_t detio_send(int sockfd, const void *buf, size_t len, int flags);

int detio_select(int nfds, fd_set *readfds, fd_set *writefds,
	   fd_set *exceptfds, struct timeval *timeout);

// ugly 
int detio_fcntl(int fd, int cmd, ... /* arg */ );
int det_ftruncate(int fd, off_t length);
void *detio_memmove(void *dst, const void *src, size_t count); 
void *detio_memcpy(void *destaddr, void const *srcaddr, size_t len); 
off_t detio_lseek(int fd, off_t offset, int whence);
int detio_open(const char *pathname, int flags, mode_t mode); 

#endif /* DPTHREAD_IO_H */ 
