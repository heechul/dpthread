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
#include <sys/select.h> 
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

int detio_open(const char *pathname, int flags, mode_t mode)
{
	int ret; 
	int lret = det_disable_logical_clock(); 	
	ret = open(pathname, flags, mode); 
	if ( lret == 0 ) det_enable_logical_clock(0); 
	return ret; 
}

off_t detio_lseek(int fd, off_t offset, int whence)
{
	off_t ret; 
	int lret = det_disable_logical_clock(); 	
	ret = lseek(fd, offset, whence);
	if ( lret == 0 ) det_enable_logical_clock(0); 
	return ret; 
}

int detio_ftruncate(int fd, off_t length)
{
	int ret; 
	int lret = det_disable_logical_clock(); 	
	ret = ftruncate(fd, length); 
	if ( lret == 0 ) det_enable_logical_clock(0); 
	return ret; 
}

// <sys/select.h> -- POSIX.1-2001 
void det_FD_ZERO(fd_set *set)
{
	int lret = det_disable_logical_clock(); 	
	FD_ZERO(set); 
	if ( lret == 0 ) det_enable_logical_clock(0); 
}

ssize_t detio_pread(int fd, void *buf, size_t count, off_t offset)
{
	ssize_t ret; 
	int lret = det_disable_logical_clock(); 
	ret = pread(fd, buf, count, offset); 
	if ( lret == 0 ) det_enable_logical_clock(0); 
	return ret; 
}

ssize_t detio_pwrite(int fd, const void *buf, size_t count, off_t offset)
{
	ssize_t ret; 
	int lret = det_disable_logical_clock(); 
	ret = pwrite(fd, buf, count, offset); 
	if ( lret == 0 ) det_enable_logical_clock(0); 
	return ret; 
}

ssize_t detio_read(int fd, void *buf, size_t count)
{
	ssize_t ret; 
	int lret = det_disable_logical_clock(); 
	ret = read(fd, buf, count); 
	if ( lret == 0 ) det_enable_logical_clock(0); 
	return ret; 
}

ssize_t detio_write(int fd, const void *buf, size_t count)
{
	ssize_t ret; 
	int lret = det_disable_logical_clock(); 
	ret = write(fd, buf, count); 
	if ( lret == 0 ) det_enable_logical_clock(0); 
	return ret; 
}


int detio_stat(const char *path, struct stat *buf)
{
	int ret; 
	int lret = det_disable_logical_clock(); 
	ret = stat(path, buf); 
	if ( lret == 0 ) det_enable_logical_clock(0); 
	return ret; 
}

int detio_fstat(int fd, struct stat *buf)
{
	int ret; 
	int lret = det_disable_logical_clock(); 
	ret = fstat(fd, buf); 
	if ( lret == 0 ) det_enable_logical_clock(0); 
	return ret; 
}


// based on program count. estimation. deterministic function. 
int detio_gettimeofday(struct timeval *tv, void *tz)
{
	uint64_t clock = det_get_clock(); 
        uint64_t usecs = (clock / EVENTS_PER_USEC); 
	tv->tv_sec  = usecs / 1000000; 
	tv->tv_usec = usecs % 1000000; 
	return 0; 
}

// socket.h 

/* non-deterministic network packet reception. */ 
ssize_t detio_recv(int sockfd, void *buf, size_t len, int flags)
{
	int ret; 
	size_t requested = len; 
	size_t remain = len; 
	int lret = det_disable_logical_clock();
	int retry_cnt = 0; 

	while ( remain > 0 ) { 
	retry:
		ret = recv(sockfd, buf, remain, flags); 
		if ( ret == 0 ) { // orderly shutdown 
			requested -= remain; 
			break; 
		} else if ( ret < 0 && retry_cnt < 3) { 
			retry_cnt++; 
			goto retry; 
		}
		remain -= ret; 
	} 

	// det_adjust_logical_clock(); // minimize non-determinism. 
	if ( lret == 0 ) det_enable_logical_clock(0); 
	return requested; 
}

ssize_t detio_send(int sockfd, const void *buf, size_t len, int flags)
{
	int ret; 
	int lret = det_disable_logical_clock();
	ret = send(sockfd, buf, len, flags); 
	if ( lret == 0 ) det_enable_logical_clock(0); 
	return ret; 
}

// #include <sys/select.h>
int detio_select(int nfds, fd_set *readfds, fd_set *writefds,
		 fd_set *exceptfds, struct timeval *timeout)
{
	int ret; 
	int lret = det_disable_logical_clock();
	ret = select(nfds, readfds, writefds, exceptfds, timeout); 
	if ( lret == 0 ) det_enable_logical_clock(0); 
	// det_adjust_logical_clock(); // minimize non-determinism. 
	return ret; 
}


void  det_posix_init()
{
	// nothing 
}

