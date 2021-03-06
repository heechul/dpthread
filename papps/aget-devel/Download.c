
/*
 * Aget, Multithreaded Download Accelerator
 *
 * (c) 2002 Murat Balaban <murat at enderunix dot org>
 * See COPYING for copyright information
 *
 *
 */

#define _XOPEN_SOURCE 500

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <netdb.h>
#include <time.h>
#include <signal.h>
#include <pthread.h>
#include <errno.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <sys/select.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#include "Head.h"
#include "Data.h"
#include "Defs.h"
#include "Misc.h"
#include "Download.h"

extern sigset_t signal_set;

#define DBG(x) x 

unsigned int bwritten = 0;
pthread_mutex_t bwritten_mutex;

void *http_get(void *arg) {
	struct thread_data *td;
	int sd;
	char *rbuf, *s;
	int dr, dw, i; 
	long foffset;
	pthread_t tid;
	sigset_t set;

	tid = pthread_self();
	/* Block out all signals */
	sigfillset(&set);
	pthread_sigmask(SIG_BLOCK, &set, NULL);
	/* Set Cancellation Type to Asynchronous */
	pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, NULL);

	td = (struct thread_data *)arg;
	foffset = td->foffset;
	
	if(td->soffset < 0 || td->soffset >= td->foffset) { /* If download complete */
		td->status = STAT_OK;		/* Tell that download is OK. */
		pthread_exit((void *)1);
		return NULL;
	}

	rbuf = (char *)calloc(MAXBUFSIZ, sizeof(char));
	if ((sd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
		Log("<THREAD #%ld> socket creation failed: %s", tid, strerror(errno));
		pthread_exit((void *)1);
	}

	if ((connect(sd, (const struct sockaddr *)&td->sin, sizeof(struct sockaddr))) == -1) {
		Log("<THREAD #%ld> connection failed: %s", tid, strerror(errno));
		pthread_exit((void *)1);
	}

	if ((send(sd, td->getstr, strlen(td->getstr), 0)) == -1) {
		Log("<THREAD #%ld> send failed: %s", tid, strerror(errno));
		pthread_exit((void *)1);
	}

        if ((dr = recv(sd, rbuf, MAXBUFSIZ-1, 0)) == -1) {
		Log("<THREAD #%ld> recv failed: %s", tid, strerror(errno));
		pthread_exit((void *)1);
        }
	rbuf[dr] = '\0';

	handleHttpRetcode(rbuf);
        if ((strstr(rbuf, "HTTP/1.1 206")) == NULL) {
		fprintf(stderr, "Something unhandled happened, shutting down...\n");
		exit(1);
	}
	s = rbuf;
	i = 0;
	while(1 && (s - rbuf) < dr) {
		if (*s == '\n' && *(s - 1) == '\r' && *(s - 2) == '\n' && *(s - 3) == '\r' && (s - rbuf) < dr) {
			s++;
			i++;
			break;
		}
		s++;
		i++;
	}

	if ((dr - i ) > foffset)
		dw = pwrite(td->fd, s, (foffset - i), td->soffset);
	else
		dw = pwrite(td->fd, s, (dr - i), td->soffset);
	td->offset = td->soffset + dw;
	pthread_mutex_lock(&bwritten_mutex);
	bwritten += dw;
	pthread_mutex_unlock(&bwritten_mutex);

	DBG(det_dbg("call testcancel\n")); 

	pthread_testcancel();	/* Check for pending cancel requests */

	DBG(det_dbg("Now begin\n")); 

	while (td->offset < foffset) {
		fd_set set;
		/* Set Cancellation Type to Asynchronous
		 * so that a blocking recv() doesn't cause problems
		 */

		pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, NULL);
		
		/* patch by Gurer Ozen	
		 * in slow downloads, avoids excessive CPU consuming...
		 */

		/* 
		 * FD_ZERO is defined in usr/include/bits/select.h as 
		 * inline assembly. for some reason it's non-determinsitic 
		 * maybe because of stosl ??? 
		 * 
		 * 08/29/2010 now it's handled by det-libc.c <heechul@illinois.edu> 
		 */ 

		FD_ZERO(&set);
		FD_SET(sd,&set);

		select(FD_SETSIZE,&set,NULL,NULL,NULL);
		/* --end of patch	*/
		memset(rbuf, 0, MAXBUFSIZ);
		dr = recv(sd, rbuf, MAXBUFSIZ, 0);

#if USE_DPTHREAD
		if ( dr != MAXBUFSIZ) det_dbg("recv: %d\n", dr); 
#endif 
		/* Set Cancellation Type back to Deferred */
		pthread_setcanceltype(PTHREAD_CANCEL_DEFERRED, NULL);

		if ((td->offset + dr) > foffset) {
			dw = pwrite(td->fd, rbuf, foffset - td->offset, td->offset);
#if USE_DPTHREAD
			if ( dw != foffset - td->offset ) DBG(det_dbg("write: %d\n", dw)); 
#endif 
		} else {
			dw = pwrite(td->fd, rbuf, dr, td->offset);
#if USE_DPTHREAD
			if ( dw != dr ) DBG(det_dbg("write: %d\n", dw)); 
#endif 
		}

		td->offset += dw;
		pthread_mutex_lock(&bwritten_mutex);
		bwritten += dw;
		pthread_mutex_unlock(&bwritten_mutex);
		pthread_testcancel();	/* Check for pending cancel requests */
	}

	if (td->offset == td->foffset)
		td->status = STAT_OK;		/* Tell thet download is OK. */
	close(sd);
	pthread_exit(NULL);
	return NULL;
}
