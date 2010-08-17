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

// unistd.h 
static int (*__usleep)(useconds_t usec) = NULL; 
static unsigned int (*__sleep)(unsigned int seconds) = NULL; 
static long (*__sysconf)(int name) = NULL;
static int (*__getopt)(int argc, char * const argv[], const char *optstring) = NULL; 

int detio_usleep(unsigned int usecs)
{
	int ret; 
	int lret = det_disable_logical_clock(); 
	if ( !__usleep ) __usleep = dlsym(RTLD_NEXT, "usleep"); 
	ret = __usleep(usecs); 
	if ( lret == 0 ) det_enable_logical_clock((uint64_t)usecs * EVENTS_PER_USEC);
	return ret; 
}

unsigned int  detio_sleep(unsigned int seconds)
{
	unsigned int ret; 
	// disable count       
	int lret = det_disable_logical_clock();
	if ( !__sleep ) __sleep = dlsym(RTLD_NEXT, "sleep"); 
	// actual sleep 
	ret = __sleep(seconds); 
	// resume logical clock 
	if ( lret == 0 ) det_enable_logical_clock((uint64_t)seconds * 1000000 * EVENTS_PER_USEC);
	return ret; 
}

long detio_sysconf(int name)
{
	long ret; 
	int  lret = det_disable_logical_clock();
	if ( !__sysconf ) __sysconf = dlsym(RTLD_NEXT, "sysconf"); 
	ret = __sysconf(name); 
	if ( lret == 0 ) det_enable_logical_clock(0); 
	return ret; 
}

int detio_getopt(int argc, char * const argv[], const char *optstring)
{
	int ret; 
	int lret = det_disable_logical_clock(); 
	if ( !__getopt ) __getopt = dlsym(RTLD_NEXT, "getopt"); 
	ret = __getopt(argc, argv, optstring); 
	if ( lret == 0 ) det_enable_logical_clock(0);
	return ret; 
}

// signal.h 
/* non-deterministic I/O, external signal from user or other program */ 
static int (*__sigwait)(const sigset_t *set, int *sig) = NULL; 

int detio_sigwait(const sigset_t *set, int *sig)
{
	int ret;

	det_exit_logical_clock(); 
	if ( !__sigwait ) __sigwait = dlsym(RTLD_NEXT, "sigwait"); 
	ret = __sigwait(set, sig); 
	det_adjust_logical_clock(); // minimize non-determinism. 
	return ret; 
}

// malloc.h 
static void *(*__valloc)(size_t size) = NULL; 
static void (*__free)(void *ptr) = NULL;

void *detio_valloc(size_t size) 
{
	void *ptr;
	int lret = det_disable_logical_clock(); 
	if ( !__valloc ) __valloc = dlsym(RTLD_NEXT, "valloc"); 
	ptr = __valloc(size); 
	if ( lret == 0 ) det_enable_logical_clock(0); 
	return ptr; 
}

void detio_free(void *ptr) 
{
	int lret = det_disable_logical_clock(); 
	if ( !__free ) __free = dlsym(RTLD_NEXT, "free"); 
	__free(ptr); 
	if ( lret == 0 ) det_enable_logical_clock(EVENTS_free);
}


// stdio.h 
static int (*__putchar)(int c) = NULL; 
static char* (*__fgets)(char *s, int size, FILE *stream) = NULL;
static int (*__fgetc)(FILE *stream) = NULL; 
static int (*__fflush)(FILE *stream) = NULL; 
static int (*__fclose)(FILE *fp) = NULL; 
static FILE* (*__fopen)(const char *path, const char *mode) = NULL;

int detio_putchar(int c)
{
	int ret; 
	int lret = det_disable_logical_clock(); 
	if ( !__putchar ) __putchar = dlsym(RTLD_NEXT, "putchar"); 
	ret = __putchar(c); 
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
	if ( !__fflush ) __fflush = dlsym(RTLD_NEXT, "fflush"); 	
	ret = __fflush(stream); 
	if ( lret == 0 ) det_enable_logical_clock(0); 
	return ret; 
}

int detio_fprintf(FILE *fp, const char *format, ...) 
{
	int ret; 
	va_list ap; 
	int lret = det_disable_logical_clock(); 

	if ( !__fflush ) __fflush = dlsym(RTLD_NEXT, "fflush"); 	
	va_start(ap, format); 
	ret = vfprintf(fp, format, ap); 
	va_end(ap); 
	__fflush(fp);

	if ( lret == 0 ) det_enable_logical_clock(EVENTS_printf); 
	return ret; 
}

// stdio.h 
char *detio_fgets(char *s, int size, FILE *stream)
{
	char *ret; 
	int lret = det_disable_logical_clock(); 
	if ( !__fgets ) __fgets = dlsym(RTLD_NEXT, "fgets"); 
	ret = __fgets(s, size, stream); 
	if ( lret == 0 ) det_enable_logical_clock(EVENTS_fgets);
	return ret; 
}

int detio_fgetc(FILE *stream)
{
	int ret; 
	int lret = det_disable_logical_clock(); 
	if ( !__fgetc ) __fgetc = dlsym(RTLD_NEXT, "fgetc"); 
	ret = __fgetc(stream); 
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
	if ( !__fclose ) __fclose = dlsym(RTLD_NEXT, "fclose"); 
	ret = __fclose(fp); 
	if ( lret == 0 ) det_enable_logical_clock(EVENTS_fclose);
	return ret; 
}

FILE *detio_fopen(const char *path, const char *mode)
{
	FILE *fp; 
	int lret = det_disable_logical_clock();
	if ( !__fopen ) __fopen = dlsym(RTLD_NEXT, "fopen"); 
	fp = __fopen(path, mode); 
	if ( lret == 0 ) det_enable_logical_clock(EVENTS_fopen);
	return fp; 
}

