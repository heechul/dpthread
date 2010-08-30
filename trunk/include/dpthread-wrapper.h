/**
 * Deterministic threading runtime 
 *
 * Author: Heechul Yun <heechul@illinois.edu> 
 *
 * This file is distributed under the University of Illinois Open Source
 * License. See LICENSE.TXT for details.
 *
 */ 

#ifndef DPTHREAD_WRAPPER_H
#define DPTHREAD_WRAPPER_H

#include <dpthread.h>
#include <dpthread-io.h>

#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <sys/time.h>
#include <sys/select.h>

///////////////////////////////////////////////////////////////////
// pthread apis 
///////////////////////////////////////////////////////////////////

#define pthread_mutex_t  det_mutex_t 
#define pthread_barrier_t det_barrier_t 
#define pthread_cond_t det_cond_t

// for 32bit machien see 

#undef PTHREAD_MUTEX_INITIALIZER
// #undef PTHREAD_RECURSIVE_MUTEX_INITIALIZER_NP
// #undef PTHREAD_ERRORCHECK_MUTEX_INITIALIZER_NP
// #undef PTHREAD_ADAPTIVE_MUTEX_INITIALIZER_NP

#if __WORDSIZE == 64
#    error "not tested on 64 bit machine" 
#else 
#    define PTHREAD_MUTEX_INITIALIZER				\
  {-1, { { 0, 0, 0, 0, 0, { 0 } } }, 0, 0, 0, { 0, 0, 0, 0} }
#endif 

// not support recursive lock and so force. 
#define pthread_mutexattr_init(a)       
#define pthread_mutexattr_settype(a,v)
#define pthread_mutexattr_destroy(a)

#define pthread_create(t,attr,func,arg) det_create(t,attr,func,arg)
#define pthread_cancel(t) det_cancel(t)
#define pthread_exit(ptr) det_exit(ptr)
#define pthread_join(t, r) det_join(t, r)
#define pthread_mutex_init(m, x) det_lock_init(m)
#define pthread_mutex_lock(m) det_lock(m)
#define pthread_mutex_trylock(m) det_trylock(m)
#define pthread_mutex_unlock(m) det_unlock(m)
#define pthread_barrier_init(b, x, c ) det_barrier_init(b, c)
#define pthread_barrier_wait(b) det_barrier_wait(b)
#define pthread_cond_init(c,a) det_cond_init(c)
#define pthread_cond_wait(c,m) det_cond_wait(c,m)
#define pthread_cond_signal(c) det_cond_signal(c)
#define pthread_cond_broadcast(c) det_cond_broadcast(c)

#define pthread_mutex_destroy(m) 0
#define pthread_cond_destroy(c) 0 
#define pthread_cond_timedwait(c,m,t) det_cond_wait(c,m)

#define pthread_testcancel() 0
#define pthread_setcancelstate(s,os) 0
#define pthread_setcanceltype(t,ot) 0 

///////////////////////////////////////////////////////////////////
// external libraries 
///////////////////////////////////////////////////////////////////

// unistd.h 
#define sleep(sec) detio_sleep(sec)
#define usleep(usec) detio_usleep(usec)
#define sysconf(name) detio_sysconf(name)
#define getopt(argc, argv, optstr) detio_getopt(argc, argv, optstr)

// signal.h 
#define sigwait(set,sig) detio_sigwait(set,sig)

// stdio.h 
#define putchar(c) detio_putchar(c)
#define snprintf(str,size,fmt, args...) detio_snprintf(str,size,fmt, ## args)
#define fprintf(output, fmt, args...) detio_fprintf(output, fmt, ## args)
#define printf(fmt, args...) detio_fprintf(stdout, fmt, ## args)
#define fgets(s, size, stream) detio_fgets(s, size, stream)
#define fgetc(stream) detio_fgetc(stream)
#define fscanf(stream, fmt, args...) detio_fscanf(stream, fmt, ## args)
#define sscanf(str, fmt, args...) detio_sscanf(str, fmt, ## args)
#define fclose(fp) detio_fclose(fp)
#define fopen(path, mode) detio_fopen(path, mode)
#define fflush(stream) detio_fflush(stream)

// stdlib.h 
#define valloc(s) detio_valloc(s)
#define malloc(s) detio_valloc(s)
#define calloc(n,s) detio_valloc((n)*(s))
#define free(x) detio_free(x)

// sys/time.h 
#define gettimeofday(tv, tz) detio_gettimeofday(tv, tz)

///////////////////////////////////////////////////////////////////
// system calls
///////////////////////////////////////////////////////////////////

// #define open(path, flags) detio_open(path, flags)
// #define open(path, flags, mode) detio_open(path, flags)

#ifdef FD_ZERO
  #undef FD_ZERO 
  #define FD_ZERO(fdsp) det_FD_ZERO(fdsp)
#endif 
#define pwrite(fd, buf, count, off) detio_pwrite(fd, buf, count, off)
#define pread(fd, buf, count, off) detio_pread(fd, buf, count, off)
#define write(fd, buf, count) detio_write(fd, buf, count)
#define read(fd, buf, count) detio_read(fd, buf, count)
#define gettimeofday(tv, tz) detio_gettimeofday(tv, tz)
#define recv(fd, buf, len, flag) detio_recv(fd, buf, len, flag)
#define send(fd, buf, len, flag) detio_send(fd, buf, len, flag)
#define select(nfd, rfds, wfds, efds, to) detio_select(nfd, rfds, wfds, efds, to)
// #define fcntl(fd, cmd, args... ) detio_fcntl(fd, cmd, ## args) 
#define fdatasync(fd) detio_fsync(fd)
#define fsync(fd) detio_fsync(fd)

#endif /* DPTHREAD_WRAPPER_H */ 


