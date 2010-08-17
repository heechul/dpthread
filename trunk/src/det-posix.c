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
// deterministic system calls 
////////////////////////////////////////////////////////////////////////////

static ssize_t (*__read)(int fd, void *buf, size_t count) = NULL; 
static ssize_t (*__write)(int fd, const void *buf, size_t count) = NULL; 
static int (*__stat)(const char *path, struct stat *buf) = NULL; 
static int (*__fstat)(int fd, struct stat *buf) = NULL; 

ssize_t detio_read(int fd, void *buf, size_t count)
{
	det_increase_logical_clock(EVENTS_read);
	return read(fd, buf, count); 
}

ssize_t detio_write(int fd, const void *buf, size_t count)
{
	det_increase_logical_clock(EVENTS_write);
	return write(fd, buf, count); 
}


int detio_stat(const char *path, struct stat *buf)
{
	det_increase_logical_clock(EVENTS_stat);
	return stat(path, buf); 
}

int detio_fstat(int fd, struct stat *buf)
{
	det_increase_logical_clock(EVENTS_fstat);	
	return fstat(fd, buf); 
}


// based on program count. estimation. deterministic function. 
int detio_gettimeofday(struct timeval *tv, struct timezone *tz)
{
	uint64_t clock = det_get_clock(); 
        uint64_t usecs = (clock / EVENTS_PER_USEC); 
	tv->tv_sec  = usecs / 1000000; 
	tv->tv_usec = usecs % 1000000; 
	return 0; 
}

// socket.h 
static ssize_t (*__recv)(int sockfd, void *buf, size_t len, int flags) = NULL; 
static ssize_t (*__send)(int sockfd, const void *buf, size_t len, int flags) = NULL; 
static int (*__select)(int nfds, fd_set *readfds, fd_set *writefds,
		       fd_set *exceptfds, struct timeval *timeout) = NULL; 

/* non-deterministic network packet reception. */ 
ssize_t detio_recv(int sockfd, void *buf, size_t len, int flags)
{
	int ret; 
	size_t requested = len; 
        int lret = det_disable_logical_clock();

	while ( len > 0 ) { 
		ret = recv(sockfd, buf, len, flags); 
		if ( ret == 0 ) {
			requested -= len; 
			break; 
		}
		len -= ret; 
	}
	// det_adjust_logical_clock(); // minimize non-determinism. 
	if ( lret == 0 ) det_enable_logical_clock(0); 
	return requested; 
}

ssize_t detio_send(int sockfd, const void *buf, size_t len, int flags)
{
	int ret; 
	size_t requested = len; 
        int lret = det_disable_logical_clock();

	while ( len > 0 ) { 
		ret = send(sockfd, buf, len, flags); 
		if ( ret == 0 ) { 
			requested -= len; 
			break; 
		}
		len -= ret; 
	}
	// det_adjust_logical_clock(); // minimize non-determinism. 
	if ( lret == 0 ) det_enable_logical_clock(0); 
	return requested; 
}

// #include <sys/select.h>
int detio_select(int nfds, fd_set *readfds, fd_set *writefds,
		 fd_set *exceptfds, struct timeval *timeout)
{
	int ret; 
	ret = select(nfds, readfds, writefds, exceptfds, timeout); 
	// det_adjust_logical_clock(); // minimize non-determinism. 
	return ret; 
}


void  det_posix_init()
{
	int lret = det_disable_logical_clock(); 
	if ( !__read ) __read = dlsym(RTLD_NEXT, "read"); 	
	if ( !__write ) __write = dlsym(RTLD_NEXT, "write"); 	
	if ( !__stat ) __stat = dlsym(RTLD_NEXT, "stat"); 	
	if ( !__fstat ) __fstat = dlsym(RTLD_NEXT, "fstat"); 	
	if ( !__recv) __recv = dlsym(RTLD_NEXT, "recv"); 	
	if ( !__send) __send = dlsym(RTLD_NEXT, "send"); 	
	if ( !__select) __select = dlsym(RTLD_NEXT, "select"); 	

	if ( lret == 0 ) det_enable_logical_clock(0); 
}
