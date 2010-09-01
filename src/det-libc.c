/**
 * Deterministic threading runtime 
 *
 * Author: Heechul Yun <heechul@illinois.edu> 
 *
 * This file is distributed under the University of Illinois Open Source
 * License. See LICENSE.TXT for details.
 *
 */ 

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif 

// internal use 
#include <sys/syscall.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdio.h>
#include <inttypes.h>
#include <dlfcn.h>

// dpthread apis
extern int det_disable_logical_clock(); 
extern int det_enable_logical_clock(uint64_t incr); 
extern int det_increase_logical_clock(uint64_t incr);
extern int det_exit_logical_clock();
extern int det_adjust_logical_clock();
extern uint64_t det_get_clock(); 

// external library calls
#include <unistd.h>
#include <signal.h>
#include <stdio.h>
#include <malloc.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/time.h>
#include <stdarg.h> // va_

// syscalls 
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <sys/socket.h>

#define USE_DET_TIME_OPT 0
#define EVENTS_PER_USEC 122 // average store events per 1 usec. 

#if USE_DET_TIME_OPT
  #define EVENTS_read 106272 // (761*EVENTS_PER_USEC)
  #define EVENTS_write 6060  // (48*EVENTS_PER_USEC)
  #define EVENTS_printf 10274 // (83*EVENTS_PER_USEC)
  #define EVENTS_valloc 2500
  #define EVENTS_free 2500
  #define EVENTS_stat 5000
  #define EVENTS_strtol 3797
  #define EVENTS_getopt 3882
  #define EVENTS_fstat 5000
  #define EVENTS_fprintf 7774
#else 
  #define EVENTS_read  1
  #define EVENTS_write 1
  #define EVENTS_printf 1 
  #define EVENTS_valloc 1
  #define EVENTS_free   1
  #define EVENTS_stat 1 
  #define EVENTS_fstat 1 
  #define EVENTS_fprintf 1
#endif 
  #define EVENTS_strtol 1
  #define EVENTS_getopt 1
  #define EVENTS_fgets 1 
  #define EVENTS_fgetc 1
  #define EVENTS_fscanf 1 
  #define EVENTS_sscanf 1 
  #define EVENTS_fclose 1 
  #define EVENTS_fopen 1 
  #define EVENTS_fflush 1 
  #define EVENTS_snprintf 1 
  #define EVENTS_fgets 1 
  #define EVENTS_fgetc 1
  #define EVENTS_fscanf 1 
  #define EVENTS_sscanf 1 
  #define EVENTS_fclose 1 
  #define EVENTS_fopen 1 
  #define EVENTS_fflush 1 

////////////////////////////////////////////////////////////////////////////
// deterministic library calls 
////////////////////////////////////////////////////////////////////////////

char *detio_strncpy(char *dst, const char *src, size_t n)
{
	if (n != 0) {
		char *d = dst;
		const char *s = src;

		do {
			if ((*d++ = *s++) == 0) {
				/* NUL pad the remaining n-1 bytes */
				while (--n != 0)
					*d++ = 0;
				break;
			}
		} while (--n != 0);
	}
	return (dst);
}

void *detio_memset(void * dst, int s, size_t count) 
{
    char * a = dst;
    while (count-- > 0)
      *a++ = s;
    return dst;
}

void *detio_memmove(void *dst, const void *src, size_t count) 
{
  char *a = dst;
  const char *b = src;

  if (src == dst)
    return dst;

  if (src>dst) {
    while (count--) *a++ = *b++;
  } else {
    a+=count-1;
    b+=count-1;
    while (count--) *a-- = *b--;
  }

  return dst;
}

void *detio_memcpy(void *destaddr, void const *srcaddr, size_t len) 
{
  char *dest = destaddr;
  char const *src = srcaddr;

  while (len-- > 0)
    *dest++ = *src++;
  return destaddr;
}

////////////////////////////////////////////////////////////////////////////

int detio_usleep(unsigned int usecs)
{
	int ret; 
	int lret = det_disable_logical_clock(); 
	ret = usleep(usecs); 
	if ( lret == 0 ) det_enable_logical_clock((uint64_t)usecs * EVENTS_PER_USEC);
	return ret; 
}

unsigned int  detio_sleep(unsigned int seconds)
{
	unsigned int ret; 
	// disable count       
	int lret = det_disable_logical_clock();
	// actual sleep 
	ret = sleep(seconds); 
	// resume logical clock 
	if ( lret == 0 ) det_enable_logical_clock((uint64_t)seconds * 1000000 * EVENTS_PER_USEC);
	return ret; 
}

long detio_sysconf(int name)
{
	long ret; 
	int  lret = det_disable_logical_clock();
	ret = sysconf(name); 
	if ( lret == 0 ) det_enable_logical_clock(0); 
	return ret; 
}

int detio_getopt(int argc, char * const argv[], const char *optstring)
{
	int ret; 
	int lret = det_disable_logical_clock(); 
	ret = getopt(argc, argv, optstring); 
	if ( lret == 0 ) det_enable_logical_clock(0);
	return ret; 
}

// signal.h 
/* non-deterministic I/O, external signal from user or other program */ 

int detio_sigwait(const sigset_t *set, int *sig)
{
	int ret;

	det_exit_logical_clock(); 
	ret = sigwait(set, sig); 
	det_adjust_logical_clock(); // minimize non-determinism. 

	return ret; 
}

int detio_sigfillset(sigset_t *set)
{
	int ret; 
	int lret = det_disable_logical_clock(); 
	ret = sigfillset(set); 
	if ( lret == 0 ) det_enable_logical_clock(0); 
}

// malloc.h 

void *detio_valloc(size_t size) 
{
	void *ptr;
	int lret = det_disable_logical_clock(); 
	ptr = valloc(size); 
	if ( lret == 0 ) det_enable_logical_clock(0); 
	return ptr; 
}

void detio_free(void *ptr) 
{
	int lret = det_disable_logical_clock(); 
	free(ptr); 
	if ( lret == 0 ) det_enable_logical_clock(EVENTS_free);
}

void *detio_realloc(void *ptr, size_t size)
{
	void *ret; 
	int lret = det_disable_logical_clock(); 
	ret = realloc(ptr, size); 
	if ( lret == 0 ) det_enable_logical_clock(0); 
	return ret; 
}

// stdlib.h 
char *detio_getenv(const char *name)
{
	char *ret; 
	int lret = det_disable_logical_clock(); 
	ret = getenv(name); 
	if ( lret == 0 ) det_enable_logical_clock(0); 
	return ret; 
}


// stdio.h 
int detio_putchar(int c)
{
	int ret; 
	int lret = det_disable_logical_clock(); 
	ret = putchar(c); 
	if ( lret == 0 ) det_enable_logical_clock(0); 
	return ret; 
}

int detio_snprintf(char *str, size_t size, const char *format, ...)
{
	int ret; 
	va_list ap; 
	int lret = det_disable_logical_clock(); 
	va_start(ap, format); 
	ret = vsnprintf(str, size, format, ap); 
	va_end(ap); 
	if ( lret == 0 ) det_enable_logical_clock(0); 
	return ret; 
}

int detio_fflush(FILE *stream)
{
	int ret; 
	int lret = det_disable_logical_clock(); 
	ret = fflush(stream); 
	if ( lret == 0 ) det_enable_logical_clock(0); 
	return ret; 
}

int detio_fprintf(FILE *fp, const char *format, ...) 
{
	int ret; 
	va_list ap; 
	int lret = det_disable_logical_clock(); 

	va_start(ap, format); 
	ret = vfprintf(fp, format, ap); 
	va_end(ap); 
	fflush(fp);

	if ( lret == 0 ) det_enable_logical_clock(EVENTS_printf); 
	return ret; 
}

// stdio.h 
char *detio_fgets(char *s, int size, FILE *stream)
{
	char *ret; 
	int lret = det_disable_logical_clock(); 
	ret = fgets(s, size, stream); 
	if ( lret == 0 ) det_enable_logical_clock(EVENTS_fgets);
	return ret; 
}

int detio_fgetc(FILE *stream)
{
	int ret; 
	int lret = det_disable_logical_clock(); 
	ret = fgetc(stream); 
	if ( lret == 0 ) det_enable_logical_clock(EVENTS_fgetc);
	return ret; 
}

int detio_fscanf(FILE *stream, const char *format, ...)
{
	int ret; 
	va_list ap; 
	int lret = det_disable_logical_clock(); 
	va_start(ap, format); 
	ret = vfscanf(stream, format, ap); 
	va_end(ap); 
	if ( lret == 0 ) det_enable_logical_clock(EVENTS_fscanf);
	return ret; 
}

int detio_sscanf(const char *str, const char *format, ...)
{
	int ret; 
	va_list ap; 
	int lret = det_disable_logical_clock(); 

	va_start(ap, format); 
	ret = vsscanf(str, format, ap); 
	va_end(ap); 

	if ( lret == 0 ) det_enable_logical_clock(EVENTS_sscanf);
	return ret; 
}

int detio_fclose(FILE *fp)
{
	int ret; 
	int lret = det_disable_logical_clock(); 
	ret = fclose(fp); 
	if ( lret == 0 ) det_enable_logical_clock(EVENTS_fclose);
	return ret; 
}

FILE *detio_fopen(const char *path, const char *mode)
{
	FILE *fp; 
	int lret = det_disable_logical_clock();
	fp = fopen(path, mode); 
	if ( lret == 0 ) det_enable_logical_clock(EVENTS_fopen);
	return fp; 
}

int detio_fcntl(int fd, int cmd, ... /* arg */ )
{
	int ret; 

	long arg; 
	va_list ap; 

	int lret = det_disable_logical_clock(); 

	va_start(ap, cmd); 
	arg = va_arg(ap, long); 
	va_end(ap); 

	ret = fcntl(fd, cmd, arg); 

	if ( lret == 0 ) det_enable_logical_clock(0); 
	return ret; 
}

int detio_fsync(int fd) // may not need. 
{
	int ret; 
	int lret = det_disable_logical_clock();
	ret = fsync(fd); 
	if ( lret == 0 ) det_enable_logical_clock(0);	
	return ret; 
}

char *detio_getcwd(char *buf, size_t size) // may not need. 
{
	char *ret; 
	int lret = det_disable_logical_clock();
	ret = getcwd(buf, size); 
	if ( lret == 0 ) det_enable_logical_clock(0);	
	return ret; 
}

