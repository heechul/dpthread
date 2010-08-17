/*
 *	File  : pbzip2.cpp
 *
 *	Title : Parallel BZIP2 (pbzip2)
 *
 *	Author: Jeff Gilchrist (http://gilchrist.ca/jeff/)
 *           - Modified producer/consumer threading code from
 *             Andrae Muys <andrae@humbug.org.au.au>
 *           - uses libbzip2 by Julian Seward (http://sources.redhat.com/bzip2/)
 *           - Major contributions by Yavor Nikolov <nikolov.javor+pbzip2@gmail.com>
 *
 *	Date  : March 12, 2010
 *
 *  Contributions
 *  -------------
 *  Bryan Stillwell <bryan@bokeoa.com> - code cleanup, RPM spec, prep work
 *			                             for inclusion in Fedora Extras
 *  Dru Lemley [http://lemley.net/smp.html] - help with large file support
 *  Kir Kolyshkin <kir@sacred.ru> - autodetection for # of CPUs
 *  Joergen Ramskov <joergen@ramskov.org> - initial version of man page
 *  Peter Cordes <peter@cordes.ca> - code cleanup
 *  Kurt Fitzner <kfitzner@excelcia.org> - port to Windows compilers and
 *                                         decompression throttling
 *  Oliver Falk <oliver@linux-kernel.at> - RPM spec update
 *  Jindrich Novy <jnovy@redhat.com> - code cleanup and bug fixes
 *  Benjamin Reed <ranger@befunk.com> - autodetection for # of CPUs in OSX
 *  Chris Dearman <chris@mips.com> - fixed pthreads race condition
 *  Richard Russon <ntfs@flatcap.org> - help fix decompression bug
 *  Paul Pluzhnikov <paul@parasoft.com> - fixed minor memory leak
 *  Aníbal Monsalve Salazar <anibal@debian.org> - creates and maintains Debian packages
 *  Steve Christensen - creates and maintains Solaris packages (sunfreeware.com)
 *  Alessio Cervellin - creates and maintains Solaris packages (blastwave.org)
 *  Ying-Chieh Liao - created the FreeBSD port
 *  Andrew Pantyukhin <sat@FreeBSD.org> - maintains the FreeBSD ports and willing to
 *                                        resolve any FreeBSD-related problems
 *  Roland Illig <rillig@NetBSD.org> - creates and maintains NetBSD packages
 *  Matt Turner <mattst88@gmail.com> - code cleanup
 *  Álvaro Reguly <alvaro@reguly.com> - RPM spec update to support SUSE Linux
 *  Ivan Voras <ivoras@freebsd.org> - support for stdin and pipes during compression and
 *                                    CPU detect changes
 *  John Dalton <john@johndalton.info> - code cleanup and bug fixes for stdin support
 *  Rene Georgi <rene.georgi@online.de> - code and Makefile cleanup, support for direct
 *                                        decompress and bzcat
 *  René Rhéaume & Jeroen Roovers <jer@xs4all.nl> - patch to support uclibc's lack of
 *                                                  a getloadavg function
 *  Reinhard Schiedermeier <rs@cs.hm.edu> - support for tar --use-compress-prog=pbzip2
 *  Elbert Pol - creates and maintains OS/2 packages
 *  Nico Vrouwe <nico@gojelly.com> - support for CPU detection on Win32
 *  Eduardo Terol <EduardoTerol@gmx.net> - creates and maintains Windows 32bit package
 *  Nikita Zhuk <nikita@zhuk.fi> - creates and maintains Mac OS X Automator action and
 *                                 workflow/service
 *  Jari Aalto <jari.aalto@cante.net> - Add long options to -h output.
 *                                      Add --loadavg, --read long options.
 *  Scott Emery <emery@sgi.com> - ignore fwrite return and pass chown errors in
 *                                writeFileMetaData if effective uid root
 *  Steven Chamberlain <steven@pyro.eu.org> - code to support throttling compression to
 *                                            prevent memory exhaustion with slow output
 *                                            pipe
 *  Yavor Nikolov <nikolov.javor+pbzip2@gmail.com> - code to support throttling compression to
 *                      prevent memory exhaustion with slow output, cleanup of debug output
 *                    - fixed infinite loop on when fileWriter fails to create output file
 *                      at start
 *                    - allDone renamed to producerDone and added mutex synchronized-access
 *                    - Changed fileWriter loop exit condition: now protected from
 *                      simultaneous access
 *                    - Mutex initialization/disposal refactored
 *                    - Throttling loops using thread condition wait
 *                    - Fatal error handling refactored
 *                    - Removed allDone checks used to signal error (now handled by
 *                      handle_error function)
 *                    - Prevented dangling threads on switch from Multi to Single threaded
 *                    - Inline hint added on a few functions
 *                    - Some additional error_handlers placed instead of returns (kill any
 *                      dangling threads)
 *                    - Cleanup and termination changed in attempt to prevent
 *                      signal-handling issues in mulit-threaded environment (still some
 *                      problems are observed on signalling e.g. with Ctrl+C)
 *                    - Signal-handling in child threads disabled. The goal is to have
 *                      single thread only which accepts signals
 *                    - Using abort instead of exit on error termination
 *                    - Fixed command-line parsing problem (e.g. -m100 -p12 -> 120 CPUs)
 *                      (Problem was unterminated strings afer strncpy).
 *                    - Signal handlers setup refactored to separate function and
 *                      switched from signal to sigaction as per POSIX recommendations
 *                    - Added mutexes unlocking before error-termination.
 *                    - Termination flag introduced (terminateFlag) to indicate abrupt
 *                      termination and facilitate thread finishing in error conditon.
 *                    - fileWriter: error_handler instead of exit on write error.
 *                    - percentComplete progress printed only if changed.
 *                    - signal handling redesigned: using sigwait in separate thread.
 *                    - Makefile: -D_POSIX_PTHREAD_SEMANTICS (used in Solaris).
 *                    - CHAR_BIT instead of 8 used in a warning message.
 *                    - SIGUSR1 signal handling added and used to terminate signal handling
 *                      thread. (Resolved issue with pthread_cancel on Windows-Cygwin)
 *                    - Fixed wrongly issued exit code 1 instead of 0.
 *                    - Corrected some error messages and added a few new ones at signal and
 *                      terminator threads join.
 *                    - Added support for thread stack size customization (-S# option)
 *                      Needs USE_STACKSIZE_CUSTOMIZATION to be defined to enable that option
 *                    - Added define of PTHREAD_STACK_MIN if such is not available in
 *                      standard headers.
 *                    - OutputBuffer usage redesigned as fixed-size circular buffer. Adding
 *                      new elements to it refactored as separate function.
 *                    - OutputBuffer resizing removed from producer_decompress since now
 *                      buffer should be with fixed size.
 *                    - Fixed debug print of OutputBuffer now referencing OutputBuffer in
 *                      old-style absolute index (in fileWriter and others).
 *                    - memstr function implementation simplified (delegated to standard
 *                      library function which is doing the same more efficiently).
 *                    - Changed some variables from int to size_t to get rid of compiler
 *                      warnings (signed + unsigned expressions).
 *                    - Sequential processing of input file/pipe/redirect implemented (capsulated
 *                      as separate class: BZ2StreamScanner)
 *                    - Parallel decompression enabled (now possible with the sequential in)
 *                    - Refactored declarations moved to separate header file (pbzip2.h) to
 *                      make global definitions available to other source modules
 *                    - Progress reporting modified since we don't have number of
 *                      blocks up-front with sequential input read (now based on bytes). fileSize
 *                      moved as InFileSize global variable for that purpose
 *                    - Progress computation in fileWriter moved to QuietMode != 0
 *                      (not needed to do it if we won't print it)
 *                    - disposeMemory helper function implemented to ease memory disposal
 *                    - Processing functions of threads declared as extern "C" since pthread_t
 *                      requires plain "C" calling convention instead of the default "C++"
 *                    - pthread_mutex_{lock|unlock} replaced with safe_mutex_{lock|unlock}
 *                      where appropriate (to prevent from issues like out of sys mutexes)
 *                    - Makefile modified to include the new source files for BZ2StreamScanner
 *                    - Makefile refined (library flags specified in LDFLAGS variable)
 *                    - Makefile.solaris.sunstudio included as example makefile for Solaris
 *                      and SunStudio 12 C++ compiler
 *                    - bz2HeaderZero in main initialized to value 0x90 > 127 which is in general
 *                      out of char type range. Changed to unsigned along with tmpBuff to avoid
 *                      some compiler(e.g. c++0x)/runtime warnings/errors.
 *                    - Some thread conditions signalling added on termination requested to ease
 *                      termination of blocked on conditions threads
 *                    - Other pthread_* calls (signal, wait) migrated to safe_* wrappers to
 *                      handle error return codes (and simplify code where already handled)
 *                    - Timed pthread cond waits refactored to separate function and moved to
 *                      debug sections only; non-timed wait used in non-debug mode. Signalling
 *                      consitions to wake threads waiting on these conditions guaranteed.
 *                    - memstr function templetized to allow working with other data types but
 *                      not only char * (e.g. unsigned char *)
 *                    - safe_cond_broadcast implemented and additional signalling added at
 *                      fileWriter end to prevent consumers blocking at end.
 *                    - Signal error when the input file doesn't contain any bzip2 headers.
 *                    - Fixed problems with not-handling zero-file length special header on compression
 *                      and decompression.
 *                    - Signalling error on stdin decompression when file doesn't start with
 *                      correct bzip2 magic header.
 *                    - Implemented outputBufferInit(size_t size) utility function for output
 *                      buffer initialization/resetting.
 *                    - Plain C headers moved to extern "C" section.
 *                    - Modified file-names handling to avoid issues with file-sizes > 2040
 *                    - Fixed out of array pointer for OutFilename in strncasecmp calls
 *                    - A few other minor modifications
 *
 *  Specials thanks for suggestions and testing:  Phillippe Welsh,
 *  James Terhune, Dru Lemley, Bryan Stillwell, George Chalissery,
 *  Kir Kolyshkin, Madhu Kangara, Mike Furr, Joergen Ramskov, Kurt Fitzner,
 *  Peter Cordes, Oliver Falk, Jindrich Novy, Benjamin Reed, Chris Dearman,
 *  Richard Russon, Aníbal Monsalve Salazar, Jim Leonard, Paul Pluzhnikov,
 *  Coran Fisher, Ken Takusagawa, David Pyke, Matt Turner, Damien Ancelin,
 *  Álvaro Reguly, Ivan Voras, John Dalton, Sami Liedes, Rene Georgi,
 *  René Rhéaume, Jeroen Roovers, Reinhard Schiedermeier, Kari Pahula,
 *  Elbert Pol, Nico Vrouwe, Eduardo Terol, Samuel Thibault, Michael Fuereder,
 *  Jari Aalto, Scott Emery, Steven Chamberlain, Yavor Nikolov, Nikita Zhuk,
 *  Joao Seabra, Conn Clark, Mark A. Haun, Tim Bielawa, Michal Gorny,
 *  Mikolaj Habdank, Christian Kujau, Marc-Christian Petersen, Piero Ottuzzi,
 *  Ephraim Ofir.
 *
 *
 * This program, "pbzip2" is copyright (C) 2003-2010 Jeff Gilchrist.
 * All rights reserved.
 *
 * The library "libbzip2" which pbzip2 uses, is copyright
 * (C) 1996-2008 Julian R Seward.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. The origin of this software must not be misrepresented; you must
 *    not claim that you wrote the original software.  If you use this
 *    software in a product, an acknowledgment in the product
 *    documentation would be appreciated but is not required.
 *
 * 3. Altered source versions must be plainly marked as such, and must
 *    not be misrepresented as being the original software.
 *
 * 4. The name of the author may not be used to endorse or promote
 *    products derived from this software without specific prior written
 *    permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
 * GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * Jeff Gilchrist, Ottawa, Canada.
 * pbzip2@compression.ca
 * pbzip2 version 1.1.0 of March 12, 2010
 *
 */
#include "pbzip2.h"
#include "BZ2StreamScanner.h"

#include <vector>
#include <algorithm>
#include <string>
#include <new>

extern "C"
{
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <bzlib.h>
#include <limits.h>
#if USE_DPTHREAD
#include <dpthread-wrapper.h> 
  extern void det_print_stat(); 
#endif 
#include <sys/mman.h> // heechul 
}

typedef struct
{
	char **buf;
	unsigned int *bufSize;
	int *blockNum;
	long head, tail;
	int full, empty;
	pthread_mutex_t *mut;
	pthread_cond_t *notFull, *notEmpty;
	pthread_t *consumers;
} queue;

typedef struct
{
	OFF_T dataStart;
	OFF_T dataSize;
} bz2BlockListing;

//
// GLOBALS
//
static int producerDone = 0;
static int terminateFlag = 0; // Abnormal premature termination
static int finishedFlag = 0; // Main thread work finished (about to exit)
static int unfinishedWorkCleaned = 0;
static int numCPU = 2;
static int QUEUESIZE = 2;
static int SIG_HANDLER_QUIT_SIGNAL = SIGUSR1; // signal used to stop SignalHandlerThread
#ifdef USE_STACKSIZE_CUSTOMIZATION
static int ChildThreadStackSize = 0; // -1 - don't modify stacksize; 0 - use minimum; > 0 - use specified
#ifndef PTHREAD_STACK_MIN
	#define PTHREAD_STACK_MIN 4096
#endif
#endif // USE_STACKSIZE_CUSTOMIZATION
static unsigned char Bz2HeaderZero[] = {
	0x42, 0x5A, 0x68, 0x39, 0x17, 0x72, 0x45, 0x38, 0x50, 0x90, 0x00, 0x00, 0x00, 0x00 };
static OFF_T InFileSize;
static OFF_T InBytesProduced = 0;
static int NumBlocks = 0;
static int NumBufferedBlocks = 0;
static size_t NumBufferedBlocksMax = 0;
static int NextBlockToWrite;
static size_t OutBufferPosToWrite; // = 0; // position in output buffer
static int Verbosity = 0;
static int QuietMode = 1;
static int OutputStdOut = 0;
static int ForceOverwrite = 0;
static int BWTblockSize = 9;
static int FileListCount = 0;
static std::vector <outBuff> OutputBuffer;
static queue *FifoQueue; // fifo queue (global var used on termination cleanup)
static pthread_mutex_t *OutMutex = NULL;
static pthread_mutex_t *ProducerDoneMutex = NULL;
static pthread_mutex_t ErrorHandlerMutex /* KENDO  = PTHREAD_MUTEX_INITIALIZER */;
static pthread_mutex_t TerminateFlagMutex /* KENDO = PTHREAD_MUTEX_INITIALIZER */;
static pthread_mutex_t ProgressIndicatorsMutex /* = PTHREAD_MUTEX_INITIALIZER */; 
static pthread_cond_t *notTooMuchNumBuffered;
static pthread_cond_t TerminateCond /* KENDO: = PTHREAD_COND_INITIALIZER */ ;
static pthread_attr_t ChildThreadAttributes;
static struct stat fileMetaData;
static const char *sigInFilename = NULL;
static const char *sigOutFilename = NULL;
static char BWTblockSizeChar = '9';
static sigset_t SignalMask;
static pthread_t SignalHandlerThread;
static pthread_t TerminatorThread;

inline int syncGetProducerDone();
inline void syncSetProducerDone(int newValue);
inline int syncGetTerminateFlag();
void syncSetTerminateFlag(int newValue);
void syncSetFinishedFlag(int newValue);
void cleanupUnfinishedWork();
void cleanupAndQuit(int exitCode);
int initSignalMask();
int setupSignalHandling();
int setupTerminator();

inline void safe_mutex_lock(pthread_mutex_t *mutex);
inline void safe_mutex_unlock(pthread_mutex_t *mutex);
inline void safe_cond_wait(pthread_cond_t *cond, pthread_mutex_t *mutex);
inline void safe_cond_signal(pthread_cond_t *cond);
int safe_cond_timed_wait(pthread_cond_t *cond, pthread_mutex_t *mutex, int seconds, const char *caller = "safe_cond_timed_wait");

template <typename FI1, typename FI2>
FI1 memstr(FI1 searchBuf, int searchBufSize, FI2 searchString, int searchStringSize);
int producer_decompress(int, OFF_T, queue *);
int directcompress(int, OFF_T, int, const char *);
int directdecompress(const char *, const char *);
int producer(int, int, int, queue *);
int mutexesInit();
void mutexesDelete();
queue *queueInit(int);
void queueDelete(queue *);
inline void queueAdd(queue *, char *, unsigned int, int);
inline char *queueDel(queue *, unsigned int *, int *);
void outputBufferInit(size_t size);
int outputBufferAdd(char *in, unsigned int bufSize, int blockNum, const char *caller);
int getFileMetaData(const char *);
int writeFileMetaData(const char *);
int testBZ2ErrorHandling(int, BZFILE *, int);
int testCompressedData(char *);
ssize_t bufread(int hf, char *buf, size_t bsize);
int detectCPUs(void);

/*
 * Pointers to functions used by plain C pthreads API require C calling
 * conventions.
 */
extern "C"
{
void* signalHandlerProc(void* arg);
void* terminatorThreadProc(void* arg);
void *consumer_decompress(void *);
void *fileWriter(void *);
void *consumer(void *);
}


#define HEECHUL 1

#if HEECHUL 
int g_thread_cnt = 0; 


void *operator new (size_t bytes)
{
	printf("mynew(%d)\n", bytes);
	return malloc(bytes);
}

void *operator new[] (size_t bytes)
{
	printf("mynew[](%d)\n", bytes);
	return malloc(bytes);
}

void operator delete (void *ptr )
{
	printf("mydelete\n");
	free(ptr);
}

void operator delete[] (void *ptr )
{
	printf("mydelete[]\n");
	free(ptr);
}

#endif 
/*
 * Lock mutex or exit application immediately on error.
 */
inline void safe_mutex_lock(pthread_mutex_t *mutex)
{
	int ret = pthread_mutex_lock(mutex);
	if (ret != 0)
	{
		fprintf(stderr, "pthread_mutex_lock error [%d]! Aborting immediately!\n", ret);
		cleanupAndQuit(-5);
	}
}

/*
 * Unlock mutex or exit application immediately on error.
 */
inline void safe_mutex_unlock(pthread_mutex_t *mutex)
{
	int ret = pthread_mutex_unlock(mutex);
	if (ret != 0)
	{
		fprintf(stderr, "pthread_mutex_unlock error [%d]! Aborting immediately!\n", ret);
		cleanupAndQuit(-6);
	}
}

/*
 * Call pthread_cond_signal - check return code and exit application immediately
 * on error.
 */
inline void safe_cond_signal(pthread_cond_t *cond)
{
	int ret = pthread_cond_signal(cond);
	if (ret != 0)
	{
		fprintf(stderr, "pthread_cond_signal error [%d]! Aborting immediately!\n", ret);
		cleanupAndQuit(-7);
	}
}

/*
 * Call pthread_cond_signal - check return code and exit application immediately
 * on error.
 */
inline void safe_cond_broadcast(pthread_cond_t *cond)
{
	int ret = pthread_cond_broadcast(cond);
	if (ret != 0)
	{
		fprintf(stderr, "pthread_cond_broadcast error [%d]! Aborting immediately!\n", ret);
		cleanupAndQuit(-7);
	}
}

/*
 * Unlock mutex or exit application immediately on error.
 */
inline void safe_cond_wait(pthread_cond_t *cond, pthread_mutex_t *mutex)
{
	int ret = pthread_cond_wait(cond, mutex);
	if (ret != 0)
	{
		fprintf(stderr, "pthread_cond_wait error [%d]! Aborting immediately!\n", ret);
		pthread_mutex_unlock(mutex);
		cleanupAndQuit(-8);
	}
}

/*
 * Delegate to pthread_cond_timedwait. Check for errors and abort if
 * any encountered. Return 0 on success and non-zero code on error
 */
int safe_cond_timed_wait(pthread_cond_t *cond, pthread_mutex_t *mutex, int seconds, const char *caller)
{
	struct timespec waitTimer;
	#ifndef WIN32
	struct timeval tv;
	struct timezone tz;
	#else
	SYSTEMTIME systemtime;
	LARGE_INTEGER filetime;
	#endif

	#ifndef WIN32
	gettimeofday(&tv, &tz);
	waitTimer.tv_sec = tv.tv_sec + seconds;
	waitTimer.tv_nsec = tv.tv_usec * 1000;
	#else
	GetSystemTime(&systemtime);
	SystemTimeToFileTime(&systemtime, (FILETIME *)&filetime);
	waitTimer.tv_sec = filetime.QuadPart / 10000000;
	waitTimer.tv_nsec = filetime.QuadPart - ((LONGLONG)waitTimer.tv_sec * 10000000) * 10;
	waitTimer.tv_sec += seconds;
	#endif
	#ifdef PBZIP_DEBUG
	fprintf(stderr, "%s:  waitTimer.tv_sec: %d  waitTimer.tv_nsec: %d\n", caller, waitTimer.tv_sec, waitTimer.tv_nsec);
	#endif
	int pret = pthread_cond_timedwait(cond, mutex, &waitTimer);
	// we are not using a compatible pthreads library so abort
	if ((pret != 0) && (pret != EINTR) && (pret != EBUSY))
	{
		pthread_mutex_unlock(mutex);
		handle_error(EF_EXIT, 1,
				"pbzip2: *ERROR: %s:  pthread_cond_timedwait() call invalid [pret=%d].  This machine\n"
				"         does not have compatible pthreads library.  Aborting.\n", caller, pret);

		cleanupAndQuit(-9);
	}
	#ifdef PBZIP_DEBUG
	else if (pret != 0)
	{
		fprintf(stderr, "%s: pthread_cond_timedwait returned with non-fatal error [%d]\n", caller, pret);
	}
	#endif // PBZIP_DEBUG

	return 0;
}

/*
 * Delegate to write but keep writing until count bytes are written or
 * error is encountered (on success all count bytes would be written)
 */
ssize_t do_write(int fd, const void *buf, size_t count)
{
	ssize_t bytesRemaining = count;
	ssize_t nbytes = 0;
	const char *pbuf = (const char *)buf;
	while ((bytesRemaining > 0) && ((nbytes = write(fd, pbuf, bytesRemaining)) > 0))
	{
		bytesRemaining -= nbytes;
		pbuf += nbytes;
	}

	if (nbytes < 0)
	{
		return nbytes;
	}

	return (count - bytesRemaining);
}

/*
 * Delegate to read but keep writing until count bytes are read or
 * error is encountered (on success all count bytes would be read)
 */
ssize_t do_read(int fd, void *buf, size_t count)
{
	ssize_t bytesRemaining = count;
	ssize_t nbytes = 0;
	char *pbuf = (char *)buf;
	while ((bytesRemaining > 0) && (nbytes = read(fd, pbuf, bytesRemaining)) > 0)
	{
		bytesRemaining -= nbytes;
		pbuf += nbytes;
	}

	if (nbytes < 0)
	{
		return nbytes;
	}

	return (count - bytesRemaining);
}


/*
 *********************************************************
	Atomically get producerDone value.
*/
inline int syncGetProducerDone()
{
	int ret;
	safe_mutex_lock(ProducerDoneMutex);
	ret = producerDone;
	safe_mutex_unlock(ProducerDoneMutex);

	return ret;
}

/*
 *********************************************************
	Atomically set producerDone value.
*/
inline void syncSetProducerDone(int newValue)
{
	safe_mutex_lock(ProducerDoneMutex);
	producerDone = newValue;
	safe_mutex_unlock(ProducerDoneMutex);
}

/*
 * Atomic get terminateFlag
 */
inline int syncGetTerminateFlag()
{
	int ret;
	safe_mutex_lock(&TerminateFlagMutex);
	ret = terminateFlag;
	safe_mutex_unlock(&TerminateFlagMutex);

	return ret;
}

/*
 * Atomically set termination flag and signal the related
 * condition.
 */
void syncSetTerminateFlag(int newValue)
{
	safe_mutex_lock(&TerminateFlagMutex);

	terminateFlag = newValue;
	if (terminateFlag != 0)
	{
		// wake up terminator thread
		pthread_cond_signal(&TerminateCond);

		// wake up all other possibly blocked on cond threads
		pthread_cond_broadcast(notTooMuchNumBuffered);
		if (FifoQueue != NULL)
		{
			pthread_cond_broadcast(FifoQueue->notFull);
			pthread_cond_broadcast(FifoQueue->notEmpty);
		}
	}

	safe_mutex_unlock(&TerminateFlagMutex);
}

/*
 *  Set finishedSucessFlag and signal the related condition.
 */
void syncSetFinishedFlag(int newValue)
{
	safe_mutex_lock(&TerminateFlagMutex);

	finishedFlag = newValue;
	if (finishedFlag != 0)
	{
		pthread_cond_signal(&TerminateCond);
	}

	fprintf(stderr, "%s: %d\n", __FUNCTION__, newValue); 

	safe_mutex_unlock(&TerminateFlagMutex);
}

/*
 *********************************************************
	Print error message and optionally exit or abort
    depending on exitFlag:
     0 - don't quit;
     1 - exit;
     2 - abort.
    On exit - exitCode status is used.
*/
int handle_error(ExitFlag exitFlag, int exitCode, const char *fmt, ...)
{
	va_list args;

	va_start(args, fmt);
	vfprintf(stderr, fmt, args);
	fflush(stderr);
	va_end(args);

	if (exitFlag == EF_ABORT)
	{
		syncSetTerminateFlag(1);
		abort();
	}
	if (exitFlag == EF_EXIT)
	{
		syncSetTerminateFlag(1);
	}

	return exitCode;
}

/*
 * Initialize and set thread signal mask
 */
int initSignalMask()
{
	int ret = 0;
	ret = sigemptyset(&SignalMask);

//	ret = sigaddset(&SignalMask, SIGINT) | ret;
	ret = sigaddset(&SignalMask, SIGTERM) | ret;
	ret = sigaddset(&SignalMask, SIGABRT) | ret;
	ret = sigaddset(&SignalMask, SIG_HANDLER_QUIT_SIGNAL) | ret;
	#ifndef WIN32
	ret = sigaddset(&SignalMask, SIGHUP) | ret;
	#endif

	if (ret == 0)
	{
		ret = pthread_sigmask(SIG_BLOCK, &SignalMask, NULL);
	}

	return ret;
}

/*
 * Initialize attributes for child threads.
 * 
 */
int initChildThreadAttributes()
{
	int ret = pthread_attr_init(&ChildThreadAttributes);

	if (ret < 0)
	{
		fprintf(stderr, "Can't initialize thread atrributes [err=%d]! Aborting...\n", ret);
		exit(-1);
	}

	#ifdef USE_STACKSIZE_CUSTOMIZATION
	if (ChildThreadStackSize > 0)
	{
		ret = pthread_attr_setstacksize(&ChildThreadAttributes, ChildThreadStackSize);

		if (ret != 0)
		{
			fprintf(stderr, "Can't set thread stacksize [err=%d]! Countinue with default one.\n", ret);
		}
	}
	#endif // USE_STACKSIZE_CUSTOMIZATION

	return ret;
}

/*
 * Setup and start signal handling.
 */
int setupSignalHandling()
{
	int ret = initSignalMask();

	if (ret == 0)
	{
#if HEECHUL 
		fprintf(stderr, "Thread %d : SignalHandlerThread\n", ++g_thread_cnt); 
#endif 
		ret = pthread_create(&SignalHandlerThread, &ChildThreadAttributes, signalHandlerProc, NULL);
	}

	return ret;
}

/*
 * Setup and start signal handling.
 */
int setupTerminator()
{
#if HEECHUL 
	fprintf(stderr, "Thread %d : TerminatorThread\n", ++g_thread_cnt); 
#endif 
	return pthread_create(&TerminatorThread, &ChildThreadAttributes, terminatorThreadProc, NULL );
}

/*
 *********************************************************
 * Clean unfinished work (after error).
 * Deletes output file if such exists and if not using pipes.
 */
void cleanupUnfinishedWork()
{
	if (unfinishedWorkCleaned != 0)
	{
		return;
	}

	struct stat statBuf;
	int ret = 0;

	#ifdef PBZIP_DEBUG
	fprintf(stderr, " Infile: %s   Outfile: %s\n", sigInFilename, sigOutFilename);
	#endif

	// only cleanup files if we did something with them
	if ((sigInFilename == NULL) || (sigOutFilename == NULL) || (OutputStdOut == 1))
	{
		unfinishedWorkCleaned = 1;
		return;
	}

	if (QuietMode != 1)
	{
		fprintf(stderr, "Cleanup unfinished work [Outfile: %s]...\n", sigOutFilename);
	}

	// check to see if input file still exists
	ret = stat(sigInFilename, &statBuf);
	if (ret == 0)
	{
		// only want to remove output file if input still exists
		if (QuietMode != 1)
			fprintf(stderr, "Deleting output file: %s, if it exists...\n", sigOutFilename);
		ret = remove(sigOutFilename);
		if (ret != 0)
		{
			fprintf(stderr, "pbzip2:  *WARNING: Deletion of output file (apparently) failed.\n");
		}
		else
		{
			fprintf(stderr, "pbzip2:  *INFO: Deletion of output file succeeded.\n");
			sigOutFilename = NULL;
		}
	}
	else
	{
		fprintf(stderr, "pbzip2:  *WARNING: Output file was not deleted since input file no longer exists.\n");
		fprintf(stderr, "pbzip2:  *WARNING: Output file: %s, may be incomplete!\n", sigOutFilename);
	}

	unfinishedWorkCleaned = 1;
}

/*
 *********************************************************
 */

/*
 *********************************************************
 * Terminator thread procedure: looking at terminateFlag
 * and exit application when it's set.
 */
void* terminatorThreadProc(void* arg)
{
	int ret = pthread_mutex_lock(&TerminateFlagMutex);

	if (ret != 0)
	{
		fprintf(stderr, "Terminator thread: pthread_mutex_lock error [%d]! Aborting...\n", ret);
		syncSetTerminateFlag(1);
		cleanupAndQuit(1);
	}
	
	while ((finishedFlag == 0) && (terminateFlag == 0))
	{
		ret = pthread_cond_wait(&TerminateCond, &TerminateFlagMutex);
	}

	// Successfull end
	if (finishedFlag != 0)
	{
		ret = pthread_mutex_unlock(&TerminateFlagMutex);
		return NULL;
	}
	
	// Being here implies (terminateFlag != 0)
	ret = pthread_mutex_unlock(&TerminateFlagMutex);

	fprintf(stderr, "Terminator thread: premature exit requested - quitting...\n");
	cleanupAndQuit(1);

	return NULL; // never reachable
}

/*
 *********************************************************
 * Signal handler thread function to hook cleanup on
 * certain signals.
 */
void* signalHandlerProc(void* arg)
{
	int signalCaught;

	// wait for specified in mask signal
	int ret = sigwait(&SignalMask, &signalCaught);

	if (ret != 0)
	{
		fprintf(stderr, "\n *signalHandlerProc - sigwait error: %d\n", ret);
	}
	else if (signalCaught == SIG_HANDLER_QUIT_SIGNAL)
	{
		return NULL;
	}
	else // ret == 0
	{
		fprintf(stderr, "\n *Control-C or similar caught [sig=%d], quitting...\n", signalCaught);
		// Delegating cleanup and termination to Terminator Thread
		syncSetTerminateFlag(1);
	}

	return NULL;
}

/*
 * Cleanup unfinished work (output file) and exit with the given exit code.
 * To be used to quite on error with non-zero exitCode.
 */
void cleanupAndQuit(int exitCode)
{
	// syncSetTerminateFlag(1);
	
	int ret = pthread_mutex_lock(&ErrorHandlerMutex);
	if (ret != 0)
	{
		fprintf(stderr, "Cleanup Handler: Failed to lock ErrorHandlerMutex! May double cleanup...\n");
	}
	cleanupUnfinishedWork();
	pthread_mutex_unlock(&ErrorHandlerMutex);

	exit(exitCode);
}

/*
 *********************************************************
    This function will search the array pointed to by
    searchBuf[] for the string searchString[] and return
    a pointer to the start of the searchString[] if found
    otherwise return NULL if not found.
*/
template <typename FI1, typename FI2>
FI1 memstr(FI1 searchBuf, int searchBufSize, FI2 searchString, int searchStringSize)
{
	FI1 searchBufEnd = searchBuf + searchBufSize;
	FI1 s = std::search(searchBuf, searchBufEnd,
						searchString, searchString + searchStringSize);

	return (s != searchBufEnd) ? s : NULL;
}

/*
 *********************************************************
    This function works in two passes of the input file.
    The first pass will look for BZIP2 headers in the file
    and note their location and size of the sections.
    The second pass will read in those BZIP2 sections and
    pass them off the the selected CPU(s) for decompression.
 */
int producer_decompress(int hInfile, OFF_T fileSize, queue *fifo)
{
	safe_mutex_lock(&ProgressIndicatorsMutex);
	InBytesProduced = 0;
	NumBlocks = 0;
	safe_mutex_unlock(&ProgressIndicatorsMutex);
	
	pbzip2::BZ2StreamScanner bz2StreamScanner(hInfile);
	
	// keep going until all the blocks are processed
	outBuff fileData = bz2StreamScanner.getNextStream();
	while ((fileData.bufSize > 0) && !bz2StreamScanner.failed())
	{
		#ifdef PBZIP_DEBUG
		fprintf(stderr, " -> Bytes Read: %u bytes...\n", fileData.bufSize);
		#endif

		if (QuietMode != 1)
		{
			// give warning to user if block is larger than 250 million bytes
			if (fileData.bufSize > 250000000)
			{
				fprintf(stderr, "pbzip2:  *WARNING: Compressed block size is large [%"PRIu64" bytes].\n",
						(unsigned long long) fileData.bufSize);
				fprintf(stderr, "          If program aborts, use regular BZIP2 to decompress.\n");
			}
		}

		// add data to the decompression queue
		safe_mutex_lock(fifo->mut);
		while (fifo->full) 
		{
			#ifdef PBZIP_DEBUG
			fprintf (stderr, "producer: queue FULL.\n");
			#endif
			safe_cond_wait (fifo->notFull, fifo->mut);
		}
		#ifdef PBZIP_DEBUG
		fprintf(stderr, "producer:  Buffer: %x  Size: %"PRIu64"   Block: %d\n", fileData.buf,
			(unsigned long long)fileData.bufSize, NumBlocks);
		#endif

		queueAdd(fifo, fileData.buf, fileData.bufSize, NumBlocks);
		safe_cond_signal (fifo->notEmpty);

		safe_mutex_lock(&ProgressIndicatorsMutex);
		InBytesProduced += fileData.bufSize;
		++NumBlocks;
		safe_mutex_unlock(&ProgressIndicatorsMutex);

		safe_mutex_unlock(fifo->mut);

		fileData = bz2StreamScanner.getNextStream();
	} // for

	close(hInfile);

	if (bz2StreamScanner.failed() || !bz2StreamScanner.eof())
	{
		handle_error(EF_EXIT, 1, "pbzip2: producer_decompress: *ERROR: when reading bzip2 input stream\n");
		return -1;
	}
	else if (!bz2StreamScanner.isBz2HeaderFound() || !bz2StreamScanner.eof())
	{
		handle_error(EF_EXIT, 1, "pbzip2: producer_decompress: *ERROR: input file is not a valid bzip2 stream\n");
		return -1;
	}

	syncSetProducerDone(1);
	safe_cond_broadcast(fifo->notEmpty); // just in case

	#ifdef PBZIP_DEBUG
		fprintf(stderr, "producer:  Done - exiting. Last Block: %d\n", NumBlocks);
	#endif
	
	return 0;
}


/*
 *********************************************************
 */
void *consumer_decompress(void *q)
{
	queue *fifo;
	char *FileData = NULL;
	char *DecompressedData = NULL;
	unsigned int inSize = 0;
	unsigned int outSize = 0;
	int blockNum = -1;
	int ret = -1;
	int pret = -1;

	fifo = (queue *)q;

	for (;;)
	{
		if (syncGetTerminateFlag() != 0)
		{
			#ifdef PBZIP_DEBUG
			fprintf (stderr, "consumer: terminating1 - terminateFlag set\n");
			#endif
			return (NULL);
		}

		safe_mutex_lock(fifo->mut);
		while (fifo->empty)
		{
			#ifdef PBZIP_DEBUG
			fprintf (stderr, "consumer: queue EMPTY.\n");
			#endif
			
			if ((syncGetProducerDone() == 1) || (syncGetTerminateFlag() != 0))
			{
				pthread_mutex_unlock(fifo->mut);
				#ifdef PBZIP_DEBUG
				fprintf (stderr, "consumer: exiting2\n");
				#endif
				return (NULL);
			}

			#ifdef PBZIP_DEBUG
			safe_cond_timed_wait(fifo->notEmpty, fifo->mut, 1, "consumer");
			#else
			safe_cond_wait(fifo->notEmpty, fifo->mut);
			#endif
		}
		FileData = queueDel(fifo, &inSize, &blockNum);
		#ifdef PBZIP_DEBUG
		fprintf(stderr, "consumer:  Buffer: %x  Size: %u   Block: %d\n", FileData, inSize, blockNum);
		#endif

		pret = pthread_cond_signal(fifo->notFull);
		safe_mutex_unlock(fifo->mut);
		
		if (pret != 0)
		{
			handle_error(EF_EXIT, -1,
				"pbzip2: consumer_decompress: *ERROR: pthread_cond_signal error = %d\n", pret);
			return (NULL);
		}
		#ifdef PBZIP_DEBUG
		fprintf (stderr, "consumer: recieved %d.\n", blockNum);
		#endif

		outSize = 900000;
		// allocate memory for decompressed data (start with default 900k block size)
		DecompressedData = new char[outSize];
		// make sure memory was allocated properly
		if (DecompressedData == NULL)
		{
			handle_error(EF_EXIT, -1,
					" *ERROR: Could not allocate memory (DecompressedData)!  Aborting...\n");
			return (NULL);
		}

		// decompress the memory buffer (verbose=0)
		ret = BZ2_bzBuffToBuffDecompress(DecompressedData, &outSize, FileData, inSize, 0, Verbosity);
		while (ret == BZ_OUTBUFF_FULL)
		{
			#ifdef PBZIP_DEBUG
			fprintf(stderr, "Increasing DecompressedData buffer size: %d -> %d\n", outSize, outSize*4);
			#endif

			if (DecompressedData != NULL)
				delete [] DecompressedData;
			DecompressedData = NULL;
			// increase buffer space
			outSize = outSize * 4;
			// allocate memory for decompressed data (start with default 900k block size)
			DecompressedData = new char[outSize];
			// make sure memory was allocated properly
			if (DecompressedData == NULL)
			{
				handle_error(EF_EXIT, -1,
						"pbzip2: *ERROR: Could not allocate memory (DecompressedData)!  Aborting...\n");
				return (NULL);
			}

			// decompress the memory buffer (verbose=0)
			ret = BZ2_bzBuffToBuffDecompress(DecompressedData, &outSize, FileData, inSize, 0, Verbosity);
		} // while

		if ((ret != BZ_OK) && (ret != BZ_OUTBUFF_FULL))
		{
			handle_error(EF_EXIT, -1, "pbzip2: *ERROR during decompression: %d\n", ret);
			return (NULL);
		}

		#ifdef PBZIP_DEBUG
		fprintf(stderr, "\n Compressed Block Size: %u\n", inSize);
		fprintf(stderr, "   Original Block Size: %u\n", outSize);
		#endif

		if (FileData != NULL)
		{
			delete [] FileData;
			FileData = NULL;
		}

		// store data to be written in output bin
		if (outputBufferAdd(DecompressedData, outSize, blockNum, "consumer_decompress") != 0)
		{
			return (NULL);
		}

		#ifdef PBZIP_DEBUG
		fprintf(stderr, " OutputBuffer[%d].buf = %x\n", blockNum, DecompressedData);
		fprintf(stderr, " OutputBuffer[%d].bufSize = %u\n", blockNum, outSize);
		#endif

	} // for
	
	#ifdef PBZIP_DEBUG
	fprintf (stderr, "consumer: exiting\n");
	#endif
	return (NULL);
}

/*
 *********************************************************
 */
void *fileWriter(void *outname)
{
	char *OutFilename;
	OFF_T CompressedSize = 0;
	int percentComplete = 0;
	int hOutfile = 1;  // default to stdout
	int currBlock = 0;
	size_t outBufferPos = 0;
	int ret = -1;
	OFF_T bytesProcessed = 0;

	OutFilename = (char *) outname;

	#ifdef PBZIP_DEBUG
	fprintf(stderr, "fileWriter function started\n");
	#endif

	// write to file instead of stdout
	if (OutputStdOut == 0)
	{
#if !USE_DPTHREAD
		hOutfile = open(OutFilename, O_RDWR | O_CREAT | O_TRUNC | O_BINARY, FILE_MODE);
#else 
		hOutfile = open(OutFilename, O_RDWR | O_CREAT | O_TRUNC | O_BINARY); 
#endif 
		// check to see if file creation was successful
		if (hOutfile == -1)
		{
			handle_error(EF_EXIT, -1,
				"pbzip2: *ERROR: Could not create output file [%s]!\n", OutFilename);
			return (NULL);
		}
	}

	while (true)
	{
		// Order is important. We don't need sync on NumBlocks when producer
		// is done.
		if ((syncGetProducerDone() == 1) && (currBlock >= NumBlocks))
		{
			#ifdef PBZIP_DEBUG
			fprintf(stderr, "fileWriter [b:%d]: done - quit loop.\n", currBlock);
			#endif
			// We're done
			break;
		}

		if (syncGetTerminateFlag() != 0)
		{
			#ifdef PBZIP_DEBUG
			fprintf (stderr, "fileWriter [b:%d]: terminating1 - terminateFlag set\n", currBlock);
			#endif
			break;
		}

		#ifdef PBZIP_DEBUG
		fprintf(stderr, "fileWriter:  Block: %d Size: %u Next File Block: %d"
				", outBufferPos: %u, NumBlocks: %d, producerDone: %d\n",
				currBlock, NumBufferedBlocksMax, NextBlockToWrite,
				outBufferPos, NumBlocks, syncGetProducerDone());
		#endif

		safe_mutex_lock(OutMutex);
		if (OutputBuffer[outBufferPos].buf == NULL)
		{
			safe_mutex_unlock(OutMutex);
			// sleep a little so we don't go into a tight loop using up all the CPU

			usleep(50000); // <-- HEECHUL 
			// fprintf(stderr, "."); 
			continue;
		}
		else
		{
			safe_mutex_unlock(OutMutex);
		}

		#ifdef PBZIP_DEBUG
		fprintf(stderr, "fileWriter:  Buffer: %x  Size: %u   Block: %d\n", OutputBuffer[outBufferPos].buf, OutputBuffer[outBufferPos].bufSize, currBlock);
		#endif

		// write data to the output file
		ret = do_write(hOutfile, OutputBuffer[outBufferPos].buf, OutputBuffer[outBufferPos].bufSize);

		//		#ifdef PBZIP_DEBUG
		fprintf(stderr, "\n -> Total Bytes Written[%d]: %d bytes...\n", currBlock, ret);
		//		#endif

		if (ret < 0)
		{
			if (OutputStdOut == 0)
				close(hOutfile);

			handle_error(EF_EXIT, -1,
				"pbzip2: *ERROR: Could not write %d bytes to file [ret=%d]!  Aborting...\n",
				OutputBuffer[outBufferPos].bufSize, ret);
			return (NULL);
		}
		CompressedSize += ret;
		
		bytesProcessed += OutputBuffer[outBufferPos].bufSize;
		delete [] OutputBuffer[outBufferPos].buf;
		OutputBuffer[outBufferPos].buf = NULL;
		OutputBuffer[outBufferPos].bufSize = 0;

		if (++outBufferPos == NumBufferedBlocksMax)
		{
			outBufferPos = 0;
		}
		currBlock++;

		safe_mutex_lock(OutMutex);
		++NextBlockToWrite;
		OutBufferPosToWrite = outBufferPos;
		// --NumBufferedBlocks; // to be removed
		safe_cond_broadcast(notTooMuchNumBuffered);
		safe_mutex_unlock(OutMutex);

		if (QuietMode != 1)
		{
			// print current completion status
			int percentCompleteOld = percentComplete;
			if (InFileSize > 0)
			{
				// we can't be here if NumBlocks is == 0 (on decompress)
				safe_mutex_lock(&ProgressIndicatorsMutex);
				percentComplete = (100 * InBytesProduced * currBlock)/(NumBlocks * InFileSize);
				safe_mutex_unlock(&ProgressIndicatorsMutex);
			}
			
			#ifdef PBZIP_DEBUG
			fprintf(stderr, "Completed: %d%%  NextBlockToWrite: %d/%u        \r", percentComplete, NextBlockToWrite, NumBufferedBlocksMax);
			fflush(stderr);
			#else
			if (percentComplete != percentCompleteOld)
			{
				fprintf(stderr, "Completed: %d%%             \r", percentComplete);
				fflush(stderr);
			}
			#endif
		}
	} // while

	if (currBlock == 0)
	{
		// zero-size file needs special handling
		ret = do_write(hOutfile, Bz2HeaderZero, sizeof(Bz2HeaderZero) );

		if (ret < 0)
		{
			handle_error(EF_EXIT, -1, "pbzip2: *ERROR: Could not write to file!  Aborting...\n");
			return (NULL);
		}
	}

	if (OutputStdOut == 0)
		close(hOutfile);
	
	if (QuietMode != 1)
	{
		fprintf(stderr, "    Output Size: %"PRIu64" bytes\n", (unsigned long long)CompressedSize);
	}

	#ifdef PBZIP_DEBUG
	fprintf(stderr, "fileWriter exit\n");
	fflush(stderr);
	#endif

	// wake up all other possibly blocked on cond threads
	if (FifoQueue != NULL)
	{
		safe_cond_broadcast(FifoQueue->notEmpty); // important
		safe_cond_broadcast(FifoQueue->notFull); // not really needed
	}
	safe_cond_broadcast(notTooMuchNumBuffered); // not really needed

	return (NULL);
}

/*
 *********************************************************
 */
int directcompress(int hInfile, OFF_T fileSize, int blockSize, const char *OutFilename)
{
	char *FileData = NULL;
	char *CompressedData = NULL;
	OFF_T CompressedSize = 0;
	OFF_T bytesLeft = 0;
	OFF_T inSize = 0;
	unsigned int outSize = 0;
	int percentComplete = 0;
	int hOutfile = 1;  // default to stdout
	int currBlock = 0;
	int rret = 0;
	int ret = 0;

	bytesLeft = fileSize;

	// write to file instead of stdout
	if (OutputStdOut == 0)
	{
#if !USE_DPTHREAD
		hOutfile = open(OutFilename, O_RDWR | O_CREAT | O_TRUNC | O_BINARY, FILE_MODE);
#else 
		hOutfile = open(OutFilename, O_RDWR | O_CREAT | O_TRUNC | O_BINARY); 
#endif 
		// check to see if file creation was successful
		if (hOutfile == -1)
		{
			fprintf(stderr, "pbzip2: *ERROR: Could not create output file [%s]!\n", OutFilename);
			return -1;
		}
	}
    #ifdef WIN32        
	else
	{
        setmode(fileno(stdout), O_BINARY);
    }
    #endif

	// keep going until all the file is processed
	while (bytesLeft > 0)
	{
		if (syncGetTerminateFlag() != 0)
		{
			close(hInfile);
			if (OutputStdOut == 0)
				close(hOutfile);

			fprintf (stderr, "directcompress: terminating - terminateFlag set\n");

			return -1;
		}

		//
		// READ DATA
		//
		
		// set buffer size
		if (bytesLeft > blockSize)
			inSize = blockSize;
		else
			inSize = bytesLeft;

		#ifdef PBZIP_DEBUG
		fprintf(stderr, " -> Bytes To Read: %"PRIu64" bytes...\n", inSize);
		#endif

		// allocate memory to read in file
		FileData = NULL;
		FileData = new char[inSize];
		// make sure memory was allocated properly
		if (FileData == NULL)
		{
			close(hInfile);
			if (OutputStdOut == 0)
				close(hOutfile);

			handle_error(EF_EXIT, -1,
					 "pbzip2: *ERROR: Could not allocate memory (FileData)!  Aborting...\n");
			return -1;
		}

		// read file data
		rret = do_read(hInfile, (char *) FileData, inSize);
		#ifdef PBZIP_DEBUG
		fprintf(stderr, " -> Total Bytes Read: %d bytes...\n\n", rret);
		#endif
		if (rret == 0)
		{
			if (FileData != NULL)
				delete [] FileData;
			break;
		}
		else if (rret < 0)
		{
			close(hInfile);
			if (FileData != NULL)
				delete [] FileData;
			if (OutputStdOut == 0)
				close(hOutfile);

			handle_error(EF_EXIT, -1,
					"pbzip2: *ERROR: Could not read from file!  Aborting...\n");
			return -1;
		}

		// set bytes left after read
		bytesLeft -= rret;

		//
		// COMPRESS DATA
		//
			
		outSize = (int) ((inSize*1.01)+600);
		// allocate memory for compressed data
		CompressedData = new char[outSize];
		// make sure memory was allocated properly
		if (CompressedData == NULL)
		{
			close(hInfile);
			if (FileData != NULL)
				delete [] FileData;

			handle_error(EF_EXIT, -1,
					"pbzip2: *ERROR: Could not allocate memory (CompressedData)!  Aborting...\n");
			return -1;
		}

		// compress the memory buffer (blocksize=9*100k, verbose=0, worklevel=30)
		ret = BZ2_bzBuffToBuffCompress(CompressedData, &outSize, FileData, inSize, BWTblockSize, Verbosity, 30);
		if (ret != BZ_OK)
		{
			close(hInfile);
			if (FileData != NULL)
				delete [] FileData;

			handle_error(EF_EXIT, -1, "pbzip2: *ERROR during compression: %d!  Aborting...\n", ret);
			return -1;
		}

		#ifdef PBZIP_DEBUG
		fprintf(stderr, "\n   Original Block Size: %u\n", inSize);
		fprintf(stderr, " Compressed Block Size: %u\n", outSize);
		#endif

		//
		// WRITE DATA
		//

		// write data to the output file
		ret = do_write(hOutfile, CompressedData, outSize);

		#ifdef PBZIP_DEBUG
		fprintf(stderr, "\n -> Total Bytes Written[%d]: %d bytes...\n", currBlock, ret);
		#endif
		if (ret <= 0)
		{
			close(hInfile);
			if (FileData != NULL)
				delete [] FileData;
			if (CompressedData != NULL)
				delete [] CompressedData;
			if (OutputStdOut == 0)
				close(hOutfile);

			handle_error(EF_EXIT, -1, "pbzip2: *ERROR: Could not write to file!  Aborting...\n");
			return -1;
		}

		CompressedSize += ret;

		currBlock++;
		// print current completion status
		int percentCompleteOld = percentComplete;
		percentComplete = 100 * currBlock / NumBlocks;
		if (QuietMode != 1)
		{
			if (percentComplete != percentCompleteOld)
			{
				fprintf(stderr, "Completed: %d%%             \r", percentComplete);
				fflush(stderr);
			}
		}
		
		// clean up memory
		if (FileData != NULL)
		{
			delete [] FileData;
			FileData = NULL;
		}
		if (CompressedData != NULL)
		{
			delete [] CompressedData;
			CompressedData = NULL;
		}
		
		// check to make sure all the data we expected was read in
		if (rret != inSize)
			inSize = rret;
	} // while

	close(hInfile);
	
	if (OutputStdOut == 0)
		close(hOutfile);
	if (QuietMode != 1)
	{
		fprintf(stderr, "    Output Size: %"PRIu64" bytes\n", (unsigned long long)CompressedSize);
	}

	syncSetProducerDone(1); // Not really needed for direct version
	return 0;
}

/*
 *********************************************************
 */
int directdecompress(const char *InFilename, const char *OutFilename)
{
	FILE *stream = NULL;
	FILE *zStream = NULL;
	BZFILE* bzf = NULL;
	unsigned char obuf[5000];
	unsigned char unused[BZ_MAX_UNUSED];
	unsigned char *unusedTmp;
	int bzerr, nread, streamNo;
	int nUnused;
	int ret = 0;
	int i;

	nUnused = 0;
	streamNo = 0;

	// see if we are using stdin or not
	if (strcmp(InFilename, "-") != 0) 
	{
		// open the file for reading
		zStream = fopen(InFilename, "rb");
		if (zStream == NULL)
		{
			handle_error(EF_NOQUIT, -1,
					"pbzip2: *ERROR: Could not open input file [%s]!  Aborting...\n", InFilename);
			return -1;
		}
	}
	else
	{
		#ifdef WIN32        
        setmode(fileno(stdin), O_BINARY);
		#endif
		zStream = stdin;
    }

	// check file stream for errors
	if (ferror(zStream))
	{
		if (zStream != stdin)
			fclose(zStream);

		handle_error(EF_NOQUIT, -1,
				"pbzip2: *ERROR: Problem with input stream of file [%s]!  Aborting...\n", InFilename);
		return -1;
	}

	// see if we are outputting to stdout
	if (OutputStdOut == 0)
	{
        stream = fopen(OutFilename, "wb");
	}
	else
	{
        #ifdef WIN32        
        setmode(fileno(stdout), O_BINARY);
        #endif
		stream = stdout;
    }

	// check file stream for errors
	if (ferror(stream))
	{
		if (stream != stdout)
			fclose(stream);
		
		handle_error(EF_NOQUIT, -1,
				"pbzip2: *ERROR: Problem with output stream of file [%s]!  Aborting...\n", InFilename);
		return -1;
	}

	// loop until end of file
	while(true)
	{
		if (syncGetTerminateFlag() != 0)
		{
			fprintf (stderr, "directdecompress: terminating1 - terminateFlag set\n");
			if (zStream != stdin)
				fclose(zStream);
			if (stream != stdout)
				fclose(stream);
			return -1;
		}

		bzf = BZ2_bzReadOpen(&bzerr, zStream, Verbosity, 0, unused, nUnused);
		if (bzf == NULL || bzerr != BZ_OK)
		{
			ret = testBZ2ErrorHandling(bzerr, bzf, streamNo);
			if (zStream != stdin)
				fclose(zStream);
			if (stream != stdout)
				fclose(stream);

			return ret;
		}

		streamNo++;
		
		while (bzerr == BZ_OK)
		{
			if (syncGetTerminateFlag() != 0)
			{
				fprintf (stderr, "directdecompress: terminating2 - terminateFlag set\n");
				if (zStream != stdin)
					fclose(zStream);
				if (stream != stdout)
					fclose(stream);
				return -1;
			}

			nread = BZ2_bzRead(&bzerr, bzf, obuf, sizeof(obuf));
			if (bzerr == BZ_DATA_ERROR_MAGIC)
			{
				// try alternate way of reading data
				if (ForceOverwrite == 1)
				{
					rewind(zStream);
					while (true)
					{
						int c = fgetc(zStream);
						if (c == EOF)
							break;
						ungetc(c,zStream);
					 
						nread = fread(obuf, sizeof(unsigned char), sizeof(obuf), zStream );
				      	if (ferror(zStream))
						{
							ret = testBZ2ErrorHandling(bzerr, bzf, streamNo);
							if (zStream != stdin)
								fclose(zStream);
							if (stream != stdout)
								fclose(stream);
							return ret;
						}
						if (nread > 0)
							(void) fwrite (obuf, sizeof(unsigned char), nread, stream);
						if (ferror(stream))
						{
							ret = testBZ2ErrorHandling(bzerr, bzf, streamNo);
							if (zStream != stdin)
								fclose(zStream);
							if (stream != stdout)
								fclose(stream);
							return ret;
						}
					}
					goto closeok;
				}
			}
			if ((bzerr == BZ_OK || bzerr == BZ_STREAM_END) && nread > 0)
				(void) fwrite(obuf, sizeof(unsigned char), nread, stream );
			if (ferror(stream))
			{
				ret = testBZ2ErrorHandling(bzerr, bzf, streamNo);
				if (zStream != stdin)
					fclose(zStream);
				if (stream != stdout)
					fclose(stream);
				return ret;
			}
		}
		if (bzerr != BZ_STREAM_END)
		{
			ret = testBZ2ErrorHandling(bzerr, bzf, streamNo);
			if (zStream != stdin)
				fclose(zStream);
			if (stream != stdout)
				fclose(stream);
			return ret;
		}

		BZ2_bzReadGetUnused(&bzerr, bzf, (void**)(&unusedTmp), &nUnused);
		if (bzerr != BZ_OK)
		{
			fprintf(stderr, "pbzip2: *ERROR: Unexpected error. Aborting!\n");
			exit(3);
		}

		for (i = 0; i < nUnused; i++)
			unused[i] = unusedTmp[i];

		BZ2_bzReadClose(&bzerr, bzf);
		if (bzerr != BZ_OK)
		{
			fprintf(stderr, "pbzip2: *ERROR: Unexpected error. Aborting!\n");
			exit(3);
		}

		// check to see if we are at the end of the file
		if (nUnused == 0)
		{
			int c = fgetc(zStream);
			if (c == EOF)
				break;
			ungetc(c, zStream);
		}
	}
	
closeok:
	// check file stream for errors
	if (ferror(zStream))
	{
		fprintf(stderr, "pbzip2: *ERROR: Problem with intput stream of file [%s]!  Skipping...\n", InFilename);
		if (zStream != stdin)
			fclose(zStream);
		if (stream != stdout)
			fclose(stream);
		return -1;
	}
	// close file
	ret = fclose(zStream);
	if (ret == EOF)
	{
		fprintf(stderr, "pbzip2: *ERROR: Problem closing file [%s]!  Skipping...\n", InFilename);
		return -1;
	}

	// check file stream for errors
	if (ferror(stream))
	{
		fprintf(stderr, "pbzip2: *ERROR: Problem with output stream of file [%s]!  Skipping...\n", InFilename);
		if (stream != stdout)
			fclose(stream);
		return -1;
	}
	ret = fflush(stream);
	if (ret != 0)
	{
		fprintf(stderr, "pbzip2: *ERROR: Problem with output stream of file [%s]!  Skipping...\n", InFilename);
		if (stream != stdout)
			fclose(stream);
		return -1;
	}
	if (stream != stdout)
	{
		ret = fclose(stream);
		if (ret == EOF)
		{
			fprintf(stderr, "pbzip2: *ERROR: Problem closing file [%s]!  Skipping...\n", OutFilename);
			return -1;
		}
	}

	syncSetProducerDone(1); // Not really needed for direct version.
	return 0;
}

/*
 * Simulate an unconditional read(), reading in data to fill the
 * bsize-sized buffer if it can, even if it means calling read() multiple
 * times. This is needed since pipes and other "special" streams
 * sometimes don't allow reading of arbitrary sized buffers.
 */
ssize_t bufread(int hf, char *buf, size_t bsize)
{
	size_t bufr = 0;
	int ret;
	int rsize = bsize;

	while (1)
	{
		ret = read(hf, buf, rsize);

		if (ret < 0)
			return ret;
		if (ret == 0)
			return bufr;

		bufr += ret;
		if (bufr == bsize)
			return bsize;
		rsize -= ret;
		buf += ret;
	}
}

/*
 *********************************************************
 */
int producer(int hInfile, int blockSize, queue *fifo)
{
	char *FileData = NULL;
	OFF_T inSize = 0;
	// int blockNum = 0;
	int ret = 0;
	int pret = -1;

	// We will now totally ignore the fileSize and read the data as it
	// comes in. Aside from allowing us to process arbitrary streams, it's
	// also the *right thing to do* in unix environments where data may
	// be appended to the file as it's processed (e.g. log files).

	safe_mutex_lock(&ProgressIndicatorsMutex);
	NumBlocks = 0;
	InBytesProduced = 0;
	safe_mutex_unlock(&ProgressIndicatorsMutex);

	// keep going until all the file is processed
	while (1)
	{
		if (syncGetTerminateFlag() != 0)
		{
			close(hInfile);
			return -1;
		}

		// set buffer size
		inSize = blockSize;

		#ifdef PBZIP_DEBUG
		fprintf(stderr, " -> Bytes To Read: %"PRIu64" bytes...\n", inSize);
		#endif

		// allocate memory to read in file
		FileData = NULL;
		FileData = new char[inSize];
		// make sure memory was allocated properly
		if (FileData == NULL)
		{
			close(hInfile);
			handle_error(EF_EXIT, -1, "pbzip2: *ERROR: Could not allocate memory (FileData)!  Aborting...\n");
			return -1;
		}

		// read file data
		ret = bufread(hInfile, (char *) FileData, inSize);
		#ifdef PBZIP_DEBUG
		fprintf(stderr, " -> Total Bytes Read: %d bytes...\n\n", ret);
		#endif
		if (ret == 0)
		{
			// finished reading.
			if (FileData != NULL)
				delete [] FileData;
			break;
		}
		else if (ret < 0)
		{
			close(hInfile);
			if (FileData != NULL)
				delete [] FileData;

			handle_error(EF_EXIT, -1, "pbzip2: *ERROR: Could not read from file!  Aborting...\n");
			return -1;
		}

		// check to make sure all the data we expected was read in
		if (ret != inSize)
			inSize = ret;

		#ifdef PBZIP_DEBUG
		fprintf(stderr, "producer:  Going into fifo-mut lock (NumBlocks: %d)\n", NumBlocks);
		#endif

		// add data to the compression queue
		safe_mutex_lock(fifo->mut);
		while (fifo->full)
		{
			#ifdef PBZIP_DEBUG
			fprintf (stderr, "producer: queue FULL.\n");
			#endif
			safe_cond_wait(fifo->notFull, fifo->mut);

			if (syncGetTerminateFlag() != 0)
			{
				pthread_mutex_unlock(fifo->mut);
				close(hInfile);
				return -1;
			}
		}
		#ifdef PBZIP_DEBUG
		fprintf(stderr, "producer:  Buffer: %x  Size: %"PRIu64"   Block: %d\n", FileData, inSize, NumBlocks);
		#endif

		queueAdd(fifo, FileData, inSize, NumBlocks);
		pret = pthread_cond_signal(fifo->notEmpty);

		safe_mutex_lock(&ProgressIndicatorsMutex);
		++NumBlocks;
		InBytesProduced += inSize;
		safe_mutex_unlock(&ProgressIndicatorsMutex);
		
		safe_mutex_unlock(fifo->mut);

		if (pret != 0)
		{
			handle_error(EF_EXIT, -1, "pbzip2: producer: *ERROR: pthread_cond_signal error = %d\n", pret);
			return -1;
		}

	} // while

	close(hInfile);

	syncSetProducerDone(1);
	safe_cond_broadcast(fifo->notEmpty); // just in case

	// #ifdef PBZIP_DEBUG
		fprintf(stderr, "producer:  Done - exiting. Num Blocks: %d\n", NumBlocks);
		// 	#endif

	return 0;
}

/*
 *********************************************************
 */
void *consumer (void *q)
{
	queue *fifo;
	char *FileData = NULL;
	char *CompressedData = NULL;
	unsigned int inSize = 0;
	unsigned int outSize = 0;
	int blockNum = -1;
	int ret = -1;
	int pret = -1;

	fifo = (queue *)q;

	for (;;)
	{
		if (syncGetTerminateFlag() != 0)
		{
			#ifdef PBZIP_DEBUG
			fprintf (stderr, "consumer: terminating1 - terminateFlag set\n");
			#endif
			return (NULL);
		}

		safe_mutex_lock(fifo->mut);
		while (fifo->empty)
		{
			#ifdef PBZIP_DEBUG
			fprintf (stderr, "consumer: queue EMPTY.\n");
			#endif
			
			if ((syncGetProducerDone() == 1) || (syncGetTerminateFlag() != 0))
			{
				safe_mutex_unlock(fifo->mut);
				#ifdef PBZIP_DEBUG
				fprintf (stderr, "consumer: exiting2\n");
				#endif
				return (NULL);
			}

			#ifdef PBZIP_DEBUG
			safe_cond_timed_wait(fifo->notEmpty, fifo->mut, 1, "consumer");
			#else
			safe_cond_wait(fifo->notEmpty, fifo->mut);
			#endif
		}
		FileData = queueDel(fifo, &inSize, &blockNum);
		#ifdef PBZIP_DEBUG
		fprintf(stderr, "consumer:  Buffer: %x  Size: %u   Block: %d\n", FileData, inSize, blockNum);
		#endif

		pret = pthread_cond_signal(fifo->notFull);
		if (pret != 0)
		{
			pthread_mutex_unlock(fifo->mut);
			handle_error(EF_EXIT, -1, "pbzip2: consumer: *ERROR: pthread_cond_signal error = %d\n", pret);
			return (NULL);
		}
		safe_mutex_unlock(fifo->mut);
		#ifdef PBZIP_DEBUG
		fprintf(stderr, "consumer: received %d.\n", blockNum);
		#endif

		outSize = (int) ((inSize*1.01)+600);
		// allocate memory for compressed data
		CompressedData = new char[outSize];
		// make sure memory was allocated properly
		if (CompressedData == NULL)
		{
			handle_error(EF_EXIT, -1, "pbzip2: *ERROR: Could not allocate memory (CompressedData)!  Aborting...\n");
			return (NULL);
		}

		// compress the memory buffer (blocksize=9*100k, verbose=0, worklevel=30)
		ret = BZ2_bzBuffToBuffCompress(CompressedData, &outSize, FileData, inSize, BWTblockSize, Verbosity, 30);
		if (ret != BZ_OK)
		{
			handle_error(EF_EXIT, -1, "pbzip2: *ERROR during compression: %d!  Aborting...\n", ret);
			return (NULL);
		}

		#ifdef PBZIP_DEBUG
		fprintf(stderr, "\n   Original Block Size: %u\n", inSize);
		fprintf(stderr, " Compressed Block Size: %u\n", outSize);
		#endif

		if (FileData != NULL)
		{
			delete [] FileData;
			FileData = NULL;
		}

		// store data to be written in output bin
		if (outputBufferAdd(CompressedData, outSize, blockNum, "consumer") != 0)
		{
			return (NULL);
		}
		
	} // for
	
	#ifdef PBZIP_DEBUG
	fprintf (stderr, "consumer: exiting\n");
	#endif
	return (NULL);
}

/*
 *********************************************************
 */
int mutexesInit()
{
	// initialize mutexes
	OutMutex = new pthread_mutex_t;
	// make sure memory was allocated properly
	if (OutMutex == NULL)
	{
		fprintf(stderr, "pbzip2: *ERROR: Could not allocate memory (OutMutex)!  Aborting...\n");
		return 1;
	}
	pthread_mutex_init(OutMutex, NULL);

	ProducerDoneMutex = new pthread_mutex_t;
	// make sure memory was allocated properly
	if (ProducerDoneMutex == NULL)
	{
		fprintf(stderr, "pbzip2: *ERROR: Could not allocate memory (ProducerDoneMutex)!  Aborting...\n");
		return 1;
	}
	pthread_mutex_init(ProducerDoneMutex, NULL);

	return 0;
}

/*
 *********************************************************
 */
void mutexesDelete()
{
	if (OutMutex != NULL)
	{
		pthread_mutex_destroy(OutMutex);
		delete OutMutex;
		OutMutex = NULL;
	}

	if (ProducerDoneMutex != NULL)
	{
		pthread_mutex_destroy(ProducerDoneMutex);
		delete ProducerDoneMutex;
		ProducerDoneMutex = NULL;
	}
}

/*
 *********************************************************
 */
queue *queueInit(int queueSize)
{
	queue *q;
	QUEUESIZE = queueSize;

	q = new queue;
	if (q == NULL)
		return NULL;

	q->buf = NULL;
	q->buf = new char*[queueSize];
	if (q->buf == NULL)
		return NULL;

	q->bufSize = NULL;
	q->bufSize = new unsigned int[queueSize];
	if (q->bufSize == NULL)
		return NULL;

	q->blockNum = NULL;
	q->blockNum = new int[queueSize];
	if (q->blockNum == NULL)
		return NULL;

	q->empty = 1;
	q->full = 0;
	q->head = 0;
	q->tail = 0;

	q->mut = NULL;
	q->mut = new pthread_mutex_t;
	if (q->mut == NULL)
		return NULL;
	pthread_mutex_init(q->mut, NULL);

	q->notFull = NULL;
	q->notFull = new pthread_cond_t;
	if (q->notFull == NULL)
		return NULL;
	pthread_cond_init(q->notFull, NULL);

	q->notEmpty = NULL;
	q->notEmpty = new pthread_cond_t;
	if (q->notEmpty == NULL)
		return NULL;
	pthread_cond_init(q->notEmpty, NULL);
	
	q->consumers = NULL;
	q->consumers = new pthread_t[queueSize];
	if (q->consumers == NULL)
		return NULL;

	notTooMuchNumBuffered = NULL;
	notTooMuchNumBuffered = new pthread_cond_t;
	if (notTooMuchNumBuffered == NULL)
		return NULL;
	pthread_cond_init(notTooMuchNumBuffered, NULL);

	return (q);
}


/*
 *********************************************************
 */
void queueDelete (queue *q)
{
	if (q == NULL)
		return;

	if (q->mut != NULL)
	{
		pthread_mutex_destroy(q->mut);
		delete q->mut;
		q->mut = NULL;
	}

	if (q->notFull != NULL)
	{
		pthread_cond_destroy(q->notFull);
		delete q->notFull;
		q->notFull = NULL;
	}

	if (q->notEmpty != NULL)
	{
		pthread_cond_destroy(q->notEmpty);
		delete q->notEmpty;
		q->notEmpty = NULL;
	}

    delete [] q->consumers;
    delete [] q->blockNum;
    delete [] q->bufSize;
    delete [] q->buf;
	delete q;
	q = NULL;

	if (notTooMuchNumBuffered != NULL)
	{
		pthread_cond_destroy(notTooMuchNumBuffered);
		delete notTooMuchNumBuffered;
		notTooMuchNumBuffered = NULL;
	}

	return;
}

/*
 *********************************************************
 */
inline void queueAdd (queue *q, char *in, unsigned int bufSize, int blockNum)
{
	q->buf[q->tail] = in;
	q->bufSize[q->tail] = bufSize;
	q->blockNum[q->tail] = blockNum;
	q->tail++;
	if (q->tail == QUEUESIZE)
		q->tail = 0;
	if (q->tail == q->head)
		q->full = 1;
	q->empty = 0;

	return;
}

/*
 *********************************************************
 */
inline char *queueDel (queue *q, unsigned int *bufSize, int *blockNum)
{
	char *out;

	out = q->buf[q->head];
	*bufSize = q->bufSize[q->head];
	*blockNum = q->blockNum[q->head];

	q->head++;
	if (q->head == QUEUESIZE)
		q->head = 0;
	if (q->head == q->tail)
		q->empty = 1;
	q->full = 0;

	return out;
}

/**
 * Initialize output buffer contents with empty (NULL, 0) blocks
 *
 * @param size new size of buffer
 *
 */
void outputBufferInit(size_t size)
{
	safe_mutex_lock(OutMutex);

	NextBlockToWrite = 0;
	OutBufferPosToWrite = 0;
	NumBufferedBlocks = 0;

	outBuff emptyElement;
	emptyElement.buf = NULL;
	emptyElement.bufSize = 0;

	// Resize and fill-in with empty elements
	OutputBuffer.assign(size, emptyElement);

	// unlikely to get here since more likely exception will be thrown
	if (OutputBuffer.size() != size)
	{
		fprintf(stderr, "pbzip2: *ERROR: Could not initialize (OutputBuffer); size=%u!  Aborting...\n", size);
		safe_mutex_unlock(OutMutex);
		exit(1);
	}

	safe_mutex_unlock(OutMutex);
}

/**
 * Store an item in OutputBuffer out bin. Synchronization is embedded to protect
 * from simultaneous access.
 *
 * @param in - item buffer
 * @param bufSize - item buffer size
 * @param blockNum - block number in the whole stream (not the position in buffer)
 * @param caller - calling function (used for logging and debug purposes)
 *
 * @return 0 - on success; other value - error.
 */
int outputBufferAdd(char *in, unsigned int bufSize, int blockNum, const char *caller)
{
	safe_mutex_lock(OutMutex);
	
	// wait while blockNum is out of range
	// [NextBlockToWrite, NextBlockToWrite + NumBufferedBlocksMax)
	int dist = blockNum - NumBufferedBlocksMax;
	while (dist >= NextBlockToWrite)
	{
		if (syncGetTerminateFlag() != 0)
		{
			#ifdef PBZIP_DEBUG
			fprintf (stderr, "%s: terminating2 - terminateFlag set\n", caller);
			#endif
			pthread_mutex_unlock(OutMutex);
			return -1;
		}

		#ifdef PBZIP_DEBUG
		fprintf (stderr, "%s: Throttling from FileWriter backlog: %d\n", caller, NumBufferedBlocks);
		#endif
		int pret = pthread_cond_wait(notTooMuchNumBuffered, OutMutex);
		if (pret != 0)
		{
			pthread_mutex_unlock(OutMutex);
			handle_error(EF_EXIT, -1, "pbzip2: %s: *ERROR: pthread_cond_wait error = %d\n", caller, pret);
			return -1;
		}
	}

	// calculate output buffer position (used in circular mode)
	size_t outBuffPos = OutBufferPosToWrite + blockNum - NextBlockToWrite;
	if (outBuffPos >= NumBufferedBlocksMax)
	{
		outBuffPos -= NumBufferedBlocksMax;
	}

	OutputBuffer[outBuffPos].bufSize = bufSize;
	OutputBuffer[outBuffPos].buf = in;
	++NumBufferedBlocks;

	safe_mutex_unlock(OutMutex);

	return 0;
}

/*
 *********************************************************
 Much of the code in this function is taken from bzip2.c
 */
int testBZ2ErrorHandling(int bzerr, BZFILE* bzf, int streamNo)
{
	int bzerr_dummy;

	BZ2_bzReadClose(&bzerr_dummy, bzf);
	switch (bzerr)
	{
		case BZ_CONFIG_ERROR:
			fprintf(stderr, "pbzip2: *ERROR: Integers are not the right size for libbzip2. Aborting!\n");
			exit(3);
			break;
		case BZ_IO_ERROR:
			fprintf(stderr, "pbzip2: *ERROR: Integers are not the right size for libbzip2. Aborting!\n");
			return 1;
			break;
		case BZ_DATA_ERROR:
			fprintf(stderr,	"pbzip2: *ERROR: Data integrity (CRC) error in data!  Skipping...\n");
			return -1;
			break;
		case BZ_MEM_ERROR:
			fprintf(stderr, "pbzip2: *ERROR: Could NOT allocate enough memory. Aborting!\n");
			return 1;
			break;
		case BZ_UNEXPECTED_EOF:
			fprintf(stderr,	"pbzip2: *ERROR: File ends unexpectedly!  Skipping...\n");
			return -1;
			break;
		case BZ_DATA_ERROR_MAGIC:
			if (streamNo == 1)
			{
				fprintf(stderr, "pbzip2: *ERROR: Bad magic number (file not created by bzip2)!  Skipping...\n");
				return -1;
			}
			else
			{
				if (QuietMode != 1)
					fprintf(stderr, "pbzip2: *WARNING: Trailing garbage after EOF ignored!\n");
				return 0;
			}
		default:
			fprintf(stderr, "pbzip2: *ERROR: Unexpected error. Aborting!\n");
			exit(3);
	}

	return 0;
}

/*
 *********************************************************
 Much of the code in this function is taken from bzip2.c
 */
int testCompressedData(char *fileName)
{
	FILE *zStream = NULL;
	int ret = 0;

	BZFILE* bzf = NULL;
	unsigned char obuf[5000];
	unsigned char unused[BZ_MAX_UNUSED];
	unsigned char *unusedTmp;
	int bzerr, nread, streamNo;
	int nUnused;
	int i;

	nUnused = 0;
	streamNo = 0;

	// see if we are using stdin or not
	if (strcmp(fileName, "-") != 0) 
	{
		// open the file for reading
		zStream = fopen(fileName, "rb");
		if (zStream == NULL)
		{
			fprintf(stderr, "pbzip2: *ERROR: Could not open input file [%s]!  Skipping...\n", fileName);
			return -1;
		}
	}
	else
		zStream = stdin;

	// check file stream for errors
	if (ferror(zStream))
	{
		fprintf(stderr, "pbzip2: *ERROR: Problem with stream of file [%s]!  Skipping...\n", fileName);
		if (zStream != stdin)
			fclose(zStream);
		return -1;
	}

	// loop until end of file
	while(true)
	{
		bzf = BZ2_bzReadOpen(&bzerr, zStream, Verbosity, 0, unused, nUnused);
		if (bzf == NULL || bzerr != BZ_OK)
		{
			ret = testBZ2ErrorHandling(bzerr, bzf, streamNo);
			if (zStream != stdin)
				fclose(zStream);
			return ret;
		}

		streamNo++;

		while (bzerr == BZ_OK)
		{
			nread = BZ2_bzRead(&bzerr, bzf, obuf, sizeof(obuf));
			if (bzerr == BZ_DATA_ERROR_MAGIC)
			{
				ret = testBZ2ErrorHandling(bzerr, bzf, streamNo);
				if (zStream != stdin)
					fclose(zStream);
				return ret;
			}
		}
		if (bzerr != BZ_STREAM_END)
		{
			ret = testBZ2ErrorHandling(bzerr, bzf, streamNo);
			if (zStream != stdin)
				fclose(zStream);
			return ret;
		}

		BZ2_bzReadGetUnused(&bzerr, bzf, (void**)(&unusedTmp), &nUnused);
		if (bzerr != BZ_OK)
		{
			fprintf(stderr, "pbzip2: *ERROR: Unexpected error. Aborting!\n");
			exit(3);
		}

		for (i = 0; i < nUnused; i++)
			unused[i] = unusedTmp[i];

		BZ2_bzReadClose(&bzerr, bzf);
		if (bzerr != BZ_OK)
		{
			fprintf(stderr, "pbzip2: *ERROR: Unexpected error. Aborting!\n");
			exit(3);
		}

		// check to see if we are at the end of the file
		if (nUnused == 0)
		{
			int c = fgetc(zStream);
			if (c == EOF)
				break;
			else
				ungetc(c, zStream);
		}
	}

	// check file stream for errors
	if (ferror(zStream))
	{
		fprintf(stderr, "pbzip2: *ERROR: Problem with stream of file [%s]!  Skipping...\n", fileName);
		if (zStream != stdin)
			fclose(zStream);
		return -1;
	}

	// close file
	ret = fclose(zStream);
	if (ret == EOF)
	{
		fprintf(stderr, "pbzip2: *ERROR: Problem closing file [%s]!  Skipping...\n", fileName);
		return -1;
	}

	return 0;
}

/*
 *********************************************************
 */
int getFileMetaData(const char *fileName)
{
	// get the file meta data and store it in the global structure
	return stat(fileName, &fileMetaData);
}

/*
 *********************************************************
 */
int writeFileMetaData(const char *fileName)
{
	int ret = 0;
	#ifndef WIN32
	struct utimbuf uTimBuf;
    #else
	_utimbuf uTimBuf;
    #endif

	// store file times in structure
	uTimBuf.actime = fileMetaData.st_atime;
	uTimBuf.modtime = fileMetaData.st_mtime;

	// update file with stored file permissions
	ret = chmod(fileName, fileMetaData.st_mode);
	if (ret != 0)
		return ret;

	// update file with stored file access and modification times
	ret = utime(fileName, &uTimBuf);
	if (ret != 0)
		return ret;

	// update file with stored file ownership (if access allows)
	#ifndef WIN32
	ret = chown(fileName, fileMetaData.st_uid, fileMetaData.st_gid);
	// following may happen on some Linux filesystems (i.e. NTFS)
	// extra error messages do no harm
	if ((geteuid() == 0) && (ret != 0))
		return ret;
	#endif

	return 0;
}

/*
 *********************************************************
 */
int detectCPUs()
{
	int ncpu;
	
	// Set default to 1 in case there is no auto-detect
	ncpu = 1;

	// Autodetect the number of CPUs on a box, if available
	#if defined(__APPLE__)
		size_t len = sizeof(ncpu);
		int mib[2];
		mib[0] = CTL_HW;
		mib[1] = HW_NCPU;
		if (sysctl(mib, 2, &ncpu, &len, 0, 0) < 0 || len != sizeof(ncpu))
			ncpu = 1;
	#elif defined(_SC_NPROCESSORS_ONLN)
		ncpu = sysconf(_SC_NPROCESSORS_ONLN);
	#elif defined(WIN32)
		SYSTEM_INFO si;
		GetSystemInfo(&si);
		ncpu = si.dwNumberOfProcessors;
	#endif

	// Ensure we have at least one processor to use
	if (ncpu < 1)
		ncpu = 1;

	return ncpu;
}


/*
 *********************************************************
 */
void banner()
{
	fprintf(stderr, "Parallel BZIP2 v1.1.0 - by: Jeff Gilchrist [http://compression.ca]\n");
	fprintf(stderr, "[Mar. 12, 2010]             (uses libbzip2 by Julian Seward)\n");
	fprintf(stderr, "Major contributions: Yavor Nikolov <nikolov.javor+pbzip2@gmail.com>\n");

	return;
}

/*
 *********************************************************
 */
void usage(char* progname, const char *reason)
{
	banner();
	
	if (strncmp(reason, "HELP", 4) == 0)
		fprintf(stderr, "\n");
	else
		fprintf(stderr, "\nInvalid command line: %s.  Aborting...\n\n", reason);

#ifndef PBZIP_NO_LOADAVG
	fprintf(stderr, "Usage: %s [-1 .. -9] [-b#cdfhklm#p#qrS#tVz] <filename> <filename2> <filenameN>\n", progname);
#else
	fprintf(stderr, "Usage: %s [-1 .. -9] [-b#cdfhkm#p#qrS#tVz] <filename> <filename2> <filenameN>\n", progname);
#endif // PBZIP_NO_LOADAVG
	fprintf(stderr, " -1 .. -9        set BWT block size to 100k .. 900k (default 900k)\n");
	fprintf(stderr, " -b#             Block size in 100k steps (default 9 = 900k)\n");
	fprintf(stderr, " -c,--stdout     Output to standard out (stdout)\n");
	fprintf(stderr, " -d,--decompress Decompress file\n");
	fprintf(stderr, " -f,--force      Overwrite existing output file\n");
	fprintf(stderr, " -h,--help       Print this help message\n");
	fprintf(stderr, " -k,--keep       Keep input file, don't delete\n");
#ifndef PBZIP_NO_LOADAVG
	fprintf(stderr, " -l,--loadavg    Load average determines max number processors to use\n");
#endif // PBZIP_NO_LOADAVG
	fprintf(stderr, " -m#             Maximum memory usage in 1MB steps (default 100 = 100MB)\n");
	fprintf(stderr, " -p#             Number of processors to use (default");
#if defined(_SC_NPROCESSORS_ONLN) || defined(__APPLE__)
	fprintf(stderr, ": autodetect [%d])\n", detectCPUs());
#else
	fprintf(stderr, " 2)\n");
#endif // _SC_NPROCESSORS_ONLN || __APPLE__
	fprintf(stderr, " -q,--quiet      Quiet mode (default)\n");
	fprintf(stderr, " -r,--read       Read entire input file into RAM and split between processors\n");
#ifdef USE_STACKSIZE_CUSTOMIZATION
	fprintf(stderr, " -S#             Child thread stack size in 1KB steps (default stack size if unspecified)\n");
#endif // USE_STACKSIZE_CUSTOMIZATION
	fprintf(stderr, " -t,--test       Test compressed file integrity\n");
	fprintf(stderr, " -v,--verbose    Verbose mode\n");
	fprintf(stderr, " -V,--version    Display version info for pbzip2 then exit\n");
	fprintf(stderr, " -z,--compress   Compress file (default)\n\n");
	fprintf(stderr, "Example: pbzip2 -b15vk myfile.tar\n");
	fprintf(stderr, "Example: pbzip2 -p4 -r -5 myfile.tar second*.txt\n");
	fprintf(stderr, "Example: tar cf myfile.tar.bz2 --use-compress-prog=pbzip2 dir_to_compress/\n");
	fprintf(stderr, "Example: pbzip2 -d -m500 myfile.tar.bz2\n\n");
	exit(-1);
}

/*
 *********************************************************
 */
int main(int argc, char* argv[])
{
	queue *fifo;
	pthread_t output;
	char **FileList = NULL;
	char *InFilename = NULL;
	char *progName = NULL;
	char *progNamePos = NULL;
	char bz2Header[] = {"BZh91AY&SY"};  // using 900k block size
	// unsigned char bz2HeaderZero[] = { 0x42, 0x5A, 0x68, 0x39, 0x17, 0x72, 0x45, 0x38, 0x50, 0x90, 0x00, 0x00, 0x00, 0x00 };
	std::string outFilename; // [2048];
	char cmdLineTemp[2048];
	unsigned char tmpBuff[50];
	char stdinFile[2] = {"-"};
	struct timeval tvStartTime;
	struct timeval tvStopTime;
	#ifndef WIN32
	struct timezone tz;
	double loadAverage = 0.0;
	double loadAvgArray[3];
	int useLoadAverage = 0;
	int numCPUtotal = 0;
	int numCPUidle = 0;
	#else
	SYSTEMTIME systemtime;
	LARGE_INTEGER filetime;
	LARGE_INTEGER fileSize_temp;
	HANDLE hInfile_temp;
	#endif
	double timeCalc = 0.0;
	double timeStart = 0.0;
	double timeStop = 0.0;
	// OFF_T fileSize = 0;
	// size_t size;
	int cmdLineTempCount = 0;
	int readEntireFile = 0;
	int zeroByteFile = 0;
	int hInfile = -1;
	int hOutfile = -1;
	int numBlocks = 0;
	int blockSize = 9*100000;
	int maxMemory = 100000000;
	int decompress = 0;
	int compress = 0;
	int testFile = 0;
	int errLevel = 0;
	int noThreads = 0;
	int keep = 0;
	int force = 0;
	int ret = 0;
	int fileLoop;
	size_t i, j, k;
	bool switchedMtToSt = false; // switched from multi- to single-thread

	// heechul 
	struct sched_param param; 
	param.sched_priority = 49;
        if(sched_setscheduler(0, SCHED_RR, &param) == -1) {
                perror("sched_setscheduler failed");
            //     exit(-1);
        }

        if(mlockall(MCL_CURRENT|MCL_FUTURE) == -1) {
                perror("mlockall failed");
                // exit(-2);
        }
	
	// get current time for benchmark reference
	#ifndef WIN32
	gettimeofday(&tvStartTime, &tz);
	#else
	GetSystemTime(&systemtime);
	SystemTimeToFileTime(&systemtime, (FILETIME *)&filetime);
	tvStartTime.tv_sec = filetime.QuadPart / 10000000;
	tvStartTime.tv_usec = (filetime.QuadPart - (LONGLONG)tvStartTime.tv_sec * 10000000) / 10;
	#endif

	// check to see if we are likely being called from TAR
	if (argc < 2)
	{
		OutputStdOut = 1;
		keep = 1;
	}

	// get program name to determine if decompress mode should be used
	progName = argv[0];
	for (progNamePos = argv[0]; progNamePos[0] != '\0'; progNamePos++)
	{
		if (progNamePos[0] == PATH_SEP)
			progName = progNamePos + 1;
	}
	if ((strstr(progName, "unzip") != 0) || (strstr(progName, "UNZIP") != 0))
	{
		decompress = 1;
	}
	if ((strstr(progName, "zcat") != 0) || (strstr(progName, "ZCAT") != 0))
	{
		decompress = OutputStdOut = keep = 1; 
	}
	
	FileListCount = 0;
	FileList = new char *[argc];
	if (FileList == NULL)
	{
		fprintf(stderr, "pbzip2: *ERROR: Not enough memory!  Aborting...\n");
		return 1;
	}
	// set default max memory usage to 100MB
	maxMemory = 100000000;
	NumBufferedBlocksMax = 0;

	numCPU = detectCPUs();

	#ifndef WIN32
	numCPUtotal = numCPU;
	#endif

	// parse command line switches
	for (i=1; (int)i < argc; i++)
	{
		if (argv[i][0] == '-')
		{
			if (argv[i][1] == '\0') 
			{
				// support "-" as a filename
				FileList[FileListCount] = argv[i];
				FileListCount++;
				continue;
			}
			else if (argv[i][1] == '-')
			{
				// get command line options with "--"
				if (strcmp(argv[i], "--best") == 0)
				{
					BWTblockSize = 9;
				}
				else if (strcmp(argv[i], "--decompress") == 0)
				{
					decompress = 1;
				}
				else if (strcmp(argv[i], "--compress") == 0)
				{
					compress = 1;
				}
				else if (strcmp(argv[i], "--fast") == 0)
				{
					BWTblockSize = 1;
				}
				else if (strcmp(argv[i], "--force") == 0)
				{
					force = 1; ForceOverwrite = 1;
				}
				else if (strcmp(argv[i], "--help") == 0)
				{
					usage(argv[0], "HELP");
				}
				else if (strcmp(argv[i], "--keep") == 0)
				{
					keep = 1;
				}
				else if (strcmp(argv[i], "--license") == 0)
				{
					usage(argv[0], "HELP");
				}
				#ifndef PBZIP_NO_LOADAVG
				else if (strcmp(argv[i], "--loadavg") == 0)
				{
					useLoadAverage = 1;
				}
				#endif
				else if (strcmp(argv[i], "--quiet") == 0)
				{
					QuietMode = 1;
				}
				else if (strcmp(argv[i], "--read") == 0)
				{
					readEntireFile = 1;
				}
				else if (strcmp(argv[i], "--stdout") == 0)
				{
					OutputStdOut = 1; keep = 1;
				}
				else if (strcmp(argv[i], "--test") == 0)
				{
					testFile = 1;
				}
				else if (strcmp(argv[i], "--verbose") == 0)
				{
					QuietMode = 0;
				}
				else if (strcmp(argv[i], "--version") == 0)
				{
					banner(); exit(0);
				}
				
				continue;
			}
			#ifdef PBZIP_DEBUG
			fprintf(stderr, "argv[%u]: %s   Len: %d\n", i, argv[i], strlen(argv[i]));
			#endif
			// get command line options with single "-"
			// check for multiple switches grouped together
			for (j=1; argv[i][j] != '\0'; j++)
			{
				switch (argv[i][j])
				{
				case 'p': k = j+1; cmdLineTempCount = 0; strcpy(cmdLineTemp, "2");
					while (argv[i][k] != '\0' && k < sizeof(cmdLineTemp))
					{
						// no more numbers, finish
						if ((argv[i][k] < '0') || (argv[i][k] > '9'))
							break;
						k++;
						cmdLineTempCount++;
					}
					if (cmdLineTempCount == 0)
						usage(argv[0], "Cannot parse -p argument");
					strncpy(cmdLineTemp, argv[i]+j+1, cmdLineTempCount);
					cmdLineTemp[cmdLineTempCount] = '\0';
					numCPU = atoi(cmdLineTemp);
					if (numCPU > 4096)
					{
						fprintf(stderr,"pbzip2: *ERROR: Maximal number of supported processors is 4096!  Aborting...\n");
						return 1;
					}
					else if (numCPU < 1)
					{
						fprintf(stderr,"pbzip2: *ERROR: Minimum number of supported processors is 1!  Aborting...\n");
						return 1;
					}
					j += cmdLineTempCount;
					#ifdef PBZIP_DEBUG
					fprintf(stderr, "-p%d\n", numCPU);
					#endif
					break;
				case 'b': k = j+1; cmdLineTempCount = 0; strcpy(cmdLineTemp, "9"); blockSize = 900000;
					while (argv[i][k] != '\0' && k < sizeof(cmdLineTemp))
					{
						// no more numbers, finish
						if ((argv[i][k] < '0') || (argv[i][k] > '9'))
							break;
						k++;
						cmdLineTempCount++;
					}
					if (cmdLineTempCount == 0)
						usage(argv[0], "Cannot parse file block size");
					strncpy(cmdLineTemp, argv[i]+j+1, cmdLineTempCount);
					cmdLineTemp[cmdLineTempCount] = '\0';
					blockSize = atoi(cmdLineTemp)*100000;
					if ((blockSize < 100000) || (blockSize > 1000000000))
					{
						fprintf(stderr,"pbzip2: *ERROR: File block size Min: 100k and Max: 10000k!  Aborting...\n");
						return 1;
					}
					j += cmdLineTempCount;
					#ifdef PBZIP_DEBUG
					fprintf(stderr, "-b%d\n", blockSize);
					#endif
					break;
				case 'm': k = j+1; cmdLineTempCount = 0; strcpy(cmdLineTemp, "1"); maxMemory = 1000000;
					while (argv[i][k] != '\0' && k < sizeof(cmdLineTemp))
					{
						// no more numbers, finish
						if ((argv[i][k] < '0') || (argv[i][k] > '9'))
							break;
						k++;
						cmdLineTempCount++;
					}
					if (cmdLineTempCount == 0)
						usage(argv[0], "Cannot parse -m argument");
					strncpy(cmdLineTemp, argv[i]+j+1, cmdLineTempCount);
					cmdLineTemp[cmdLineTempCount] = '\0';
					maxMemory = atoi(cmdLineTemp)*1000000;
					if ((maxMemory < 1000000) || (maxMemory > 1000000000))
					{
						fprintf(stderr,"pbzip2: *ERROR: Memory usage size Min: 1MB and Max: 1000MB!  Aborting...\n");
						return 1;
					}
					j += cmdLineTempCount;
					#ifdef PBZIP_DEBUG
					fprintf(stderr, "-m%d\n", maxMemory);
					#endif
					break;
				#ifdef USE_STACKSIZE_CUSTOMIZATION
				case 'S': k = j+1; cmdLineTempCount = 0; strcpy(cmdLineTemp, "0"); ChildThreadStackSize = -1;
					while (argv[i][k] != '\0' && k < sizeof(cmdLineTemp))
					{
						// no more numbers, finish
						if ((argv[i][k] < '0') || (argv[i][k] > '9'))
							break;
						k++;
						cmdLineTempCount++;
					}
					if (cmdLineTempCount == 0)
						usage(argv[0], "Cannot parse -S argument");
					strncpy(cmdLineTemp, argv[i]+j+1, cmdLineTempCount);
					cmdLineTemp[cmdLineTempCount] = '\0';
					ChildThreadStackSize = atoi(cmdLineTemp)*1024;
					if (ChildThreadStackSize < 0)
					{
						fprintf(stderr,"pbzip2: *ERROR: Parsing -S: invalid stack size specified [%d]!  Ignoring...\n",
							 ChildThreadStackSize);
					}
					else if (ChildThreadStackSize < PTHREAD_STACK_MIN)
					{
						fprintf(stderr,"pbzip2: *WARNING: Stack size %d bytes less than minumum - adjusting to %d bytes.\n",
							 ChildThreadStackSize, PTHREAD_STACK_MIN);
						ChildThreadStackSize = PTHREAD_STACK_MIN;
					}
					j += cmdLineTempCount;
					#ifdef PBZIP_DEBUG
					fprintf(stderr, "-S%d\n", ChildThreadStackSize);
					#endif
					break;
				#endif // USE_STACKSIZE_CUSTOMIZATION
				case 'd': decompress = 1; break;
				case 'c': OutputStdOut = 1; keep = 1; break;
				case 'f': force = 1; ForceOverwrite = 1; break;
				case 'h': usage(argv[0], "HELP"); break;
				case 'k': keep = 1; break;
				#ifndef PBZIP_NO_LOADAVG
				case 'l': useLoadAverage = 1; break;
				#endif
				case 'L': banner(); exit(0); break;
				case 'q': QuietMode = 1; break;
				case 'r': readEntireFile = 1; break;
				case 't': testFile = 1; break;
				case 'v': QuietMode = 0; break;
				case 'V': banner(); exit(0); break;
				case 'z': compress = 1; break;
				case '1': BWTblockSize = 1; break;
				case '2': BWTblockSize = 2; break;
				case '3': BWTblockSize = 3; break;
				case '4': BWTblockSize = 4; break;
				case '5': BWTblockSize = 5; break;
				case '6': BWTblockSize = 6; break;
				case '7': BWTblockSize = 7; break;
				case '8': BWTblockSize = 8; break;
				case '9': BWTblockSize = 9; break;
				}
			}
		}
		else
		{
			// add filename to list for processing FileListCount
			FileList[FileListCount] = argv[i];
			FileListCount++;
		}
	} /* for */

	Bz2HeaderZero[3] = '0' + BWTblockSize;
	bz2Header[3] = Bz2HeaderZero[3];

	// check to make sure we are not trying to compress and decompress at same time
	if ((compress == 1) && (decompress == 1))
	{
		fprintf(stderr,"pbzip2: *ERROR: Can't compress and uncompress data at same time.  Aborting!\n");
		fprintf(stderr,"pbzip2: For help type: %s -h\n", argv[0]);
		return 1;
	}

	if (FileListCount == 0)
	{
		if (testFile == 1)
		{
			#ifndef WIN32
			if (isatty(fileno(stdin)))
			#else
			if (_isatty(_fileno(stdin)))
			#endif
			{
					fprintf(stderr,"pbzip2: *ERROR: Won't read compressed data from terminal.  Aborting!\n");
					fprintf(stderr,"pbzip2: For help type: %s -h\n", argv[0]);
					return 1;
			}
			// expecting data from stdin
			FileList[FileListCount] = stdinFile;
			FileListCount++;
		}
		else if (OutputStdOut == 1)
		{
			#ifndef WIN32
			if (isatty(fileno(stdout)))
			#else
			if (_isatty(_fileno(stdout)))
			#endif
			{
					fprintf(stderr,"pbzip2: *ERROR: Won't write compressed data to terminal.  Aborting!\n");
					fprintf(stderr,"pbzip2: For help type: %s -h\n", argv[0]);
					return 1;
			}
			// expecting data from stdin
			FileList[FileListCount] = stdinFile;
			FileListCount++;
		}
		else if ((decompress == 1) && (argc == 2))
		{
			#ifndef WIN32
			if (isatty(fileno(stdin)))
			#else
			if (_isatty(_fileno(stdin)))
			#endif
			{
					fprintf(stderr,"pbzip2: *ERROR: Won't read compressed data from terminal.  Aborting!\n");
					fprintf(stderr,"pbzip2: For help type: %s -h\n", argv[0]);
					return 1;
			}
			// expecting data from stdin via TAR
			OutputStdOut = 1;
			keep = 1;
			FileList[FileListCount] = stdinFile;
			FileListCount++;
		}
		else
		{
			// probably trying to input data from stdin
			if (QuietMode != 1)
				fprintf(stderr,"pbzip2: Assuming input data coming from stdin...\n\n");
			
			OutputStdOut = 1;
			keep = 1;
#ifndef WIN32
			if (isatty(fileno(stdout)))
#else
				if (_isatty(_fileno(stdout)))
#endif
				{
					fprintf(stderr,"pbzip2: *ERROR: Won't write compressed data to terminal.  Aborting!\n");
					fprintf(stderr,"pbzip2: For help type: %s -h\n", argv[0]);
					return 1;
				}
			// expecting data from stdin
			FileList[FileListCount] = stdinFile;
			FileListCount++;
		}
	}

	if (QuietMode != 1)
	{
		// display program banner
		banner();

		// do sanity check to make sure integers are the size we expect
		#ifdef PBZIP_DEBUG
		fprintf(stderr, "off_t size: %u    uint size: %u\n", sizeof(OFF_T), sizeof(unsigned int));
		#endif
		if (sizeof(OFF_T) <= 4)
		{
			fprintf(stderr, "\npbzip2: *WARNING: off_t variable size only %u bits!\n", sizeof(OFF_T)*CHAR_BIT);
			if (decompress == 1)
				fprintf(stderr, " You will only able to uncompress files smaller than 2GB in size.\n\n");
			else
				fprintf(stderr, " You will only able to compress files smaller than 2GB in size.\n\n");
		}
	}
	
	// Calculate number of processors to use based on load average if requested
	#ifndef PBZIP_NO_LOADAVG
	if (useLoadAverage == 1)
	{
		// get current load average
		ret = getloadavg(loadAvgArray, 3);
		if (ret != 3)
		{
			loadAverage = 0.0;
			useLoadAverage = 0;
			if (QuietMode != 1)
				fprintf(stderr, "pbzip2:  *WARNING: Could not get load average!  Using requested processors...\n");
		}
		else
		{
			#ifdef PBZIP_DEBUG
			fprintf(stderr, "Load Avg1: %f  Avg5: %f  Avg15: %f\n", loadAvgArray[0], loadAvgArray[1], loadAvgArray[2]);
			#endif
			// use 1 min load average to adjust number of processors used
			loadAverage = loadAvgArray[0];	// use [1] for 5 min average and [2] for 15 min average
			// total number processors minus load average rounded up
			numCPUidle = numCPUtotal - (int)(loadAverage + 0.5);
			// if user asked for a specific # processors and they are idle, use all requested
			// otherwise give them whatever idle processors are available
			if (numCPUidle < numCPU)
				numCPU = numCPUidle;
			if (numCPU < 1)
				numCPU = 1;
		}
	}
	#endif

#if USE_DPTHREAD
	pthread_mutex_init(&ErrorHandlerMutex, NULL); 
	pthread_mutex_init(&TerminateFlagMutex, NULL);
	pthread_mutex_init(&ProgressIndicatorsMutex, NULL); 
	pthread_cond_init(&TerminateCond, NULL); /* KENDO: = PTHREAD_COND_INITIALIZER */ ;
#endif 

	// Initialize child threads attributes
	initChildThreadAttributes();

	// setup signal handling (should be before creating any child thread)
	sigInFilename = NULL;
	sigOutFilename = NULL;

	ret = setupSignalHandling();
	if (ret != 0)
	{
		fprintf(stderr, "pbzip2: *ERROR: Can't setup signal handling [%d]. Aborting!\n", ret);
		return 1;
	}

	// Create and start terminator thread.
	ret = setupTerminator();
	if (ret != 0)
	{
		fprintf(stderr, "pbzip2: *ERROR: Can't setup terminator thread [%d]. Aborting!\n", ret);
		return 1;
	}

	if (numCPU < 1)
		numCPU = 1;

	// display global settings
	if (QuietMode != 1)
	{
		if (testFile != 1)
		{
			fprintf(stderr, "\n         # CPUs: %d\n", numCPU);
			#ifndef PBZIP_NO_LOADAVG
			if (useLoadAverage == 1)
				fprintf(stderr, "   Load Average: %.2f\n", loadAverage);
			#endif
			if (decompress != 1)
			{
				fprintf(stderr, " BWT Block Size: %d00 KB\n", BWTblockSize);
				if (blockSize < 100000)
					fprintf(stderr, "File Block Size: %d bytes\n", blockSize);
				else
					fprintf(stderr, "File Block Size: %d KB\n", blockSize/1000);
			}
			fprintf(stderr, " Maximum Memory: %d MB\n", maxMemory/1000000);
			#ifdef USE_STACKSIZE_CUSTOMIZATION
				if (ChildThreadStackSize > 0)
					fprintf(stderr, "     Stack Size: %d KB\n", ChildThreadStackSize/1024);
			#endif
		}
		fprintf(stderr, "-------------------------------------------\n");
	}

	int mutexesInitRet = mutexesInit();
	if ( mutexesInitRet != 0 )
	{
		return mutexesInitRet;
	}

	// create queue
	fifo = FifoQueue = queueInit(numCPU);
	if (fifo == NULL)
	{
		fprintf (stderr, "pbzip2: *ERROR: Queue Init failed.  Aborting...\n");
		return 1;
	}

	// process all files
	for (fileLoop=0; fileLoop < FileListCount; fileLoop++)
	{
		producerDone = 0;
		InFileSize = 0;
		NumBlocks = 0;

		// set input filename
		InFilename = FileList[fileLoop];

		// test file for errors if requested
		if (testFile != 0)
		{
			if (QuietMode != 1)
			{
				fprintf(stderr, "      File #: %d of %d\n", fileLoop+1, FileListCount);
				if (strcmp(InFilename, "-") != 0) 
					fprintf(stderr, "     Testing: %s\n", InFilename);
				else
					fprintf(stderr, "     Testing: <stdin>\n");
			}
			ret = testCompressedData(InFilename);
			if (ret > 0)
				return ret;
			else if (ret == 0)
			{
				if (QuietMode != 1)
					fprintf(stderr, "        Test: OK\n");
			}
			else
				errLevel = 2;

			if (QuietMode != 1)
				fprintf(stderr, "-------------------------------------------\n");
			continue;
		}

		// set ouput filename
		outFilename = std::string(FileList[fileLoop]);
		if ((decompress == 1) && (strcmp(InFilename, "-") != 0))
		{
			// check if input file is a valid .bz2 compressed file
			hInfile = open(InFilename, O_RDONLY | O_BINARY);
			// check to see if file exists before processing
			if (hInfile == -1)
			{
				fprintf(stderr, "pbzip2: *ERROR: File [%s] NOT found!  Skipping...\n", InFilename);
				fprintf(stderr, "-------------------------------------------\n");
				errLevel = 1;
				continue;
			}
			memset(tmpBuff, 0, sizeof(tmpBuff));
			size_t size = do_read(hInfile, tmpBuff, strlen(bz2Header)+1);
			close(hInfile);
			if ((size == (size_t)(-1)) || (size < strlen(bz2Header)+1))
			{
				fprintf(stderr, "pbzip2: *ERROR: File [%s] is NOT a valid bzip2!  Skipping...\n", InFilename);
				fprintf(stderr, "-------------------------------------------\n");
				errLevel = 1;
				continue;
			}
			else
			{
				// make sure start of file has valid bzip2 header
				if (memstr(tmpBuff, 4, bz2Header, 3) == NULL)
				{
					fprintf(stderr, "pbzip2: *ERROR: File [%s] is NOT a valid bzip2!  Skipping...\n", InFilename);
					fprintf(stderr, "-------------------------------------------\n");
					errLevel = 1;
					continue;
				}
				// skip 4th char which differs depending on BWT block size used
				if (memstr(tmpBuff+4, size-4, bz2Header+4, strlen(bz2Header)-4) == NULL)
				{
					// check to see if this is a special 0 byte file
					if (memstr(tmpBuff+4, size-4, Bz2HeaderZero+4, strlen(bz2Header)-4) == NULL)
					{
						fprintf(stderr, "pbzip2: *ERROR: File [%s] is NOT a valid bzip2!  Skipping...\n", InFilename);
						fprintf(stderr, "-------------------------------------------\n");
						errLevel = 1;
						continue;
					}
					#ifdef PBZIP_DEBUG
					fprintf(stderr, "** ZERO byte compressed file detected\n");
					#endif
				}
				// set block size for decompression
				if ((tmpBuff[3] >= '1') && (tmpBuff[3] <= '9'))
					BWTblockSizeChar = tmpBuff[3];
				else
				{
					fprintf(stderr, "pbzip2: *ERROR: File [%s] is NOT a valid bzip2!  Skipping...\n", InFilename);
					fprintf(stderr, "-------------------------------------------\n");
					errLevel = 1;
					continue;
				}
			}

			// check if filename ends with .bz2
			std::string bz2Tail(".bz2");
			if ( ends_with_icase(outFilename, bz2Tail) )
			{
				// remove .bz2 extension
				outFilename.resize( outFilename.size() - bz2Tail.size() );
			}
			else
			{
				// add .out extension so we don't overwrite original file
				outFilename += ".out";
			}
		} // decompress == 1
		else
		{
			// check input file to make sure its not already a .bz2 file
			std::string bz2Tail(".bz2");
			if ( ends_with_icase(std::string(InFilename), bz2Tail) )
			{
				fprintf(stderr, "pbzip2: *ERROR: Input file [%s] already has a .bz2 extension!  Skipping...\n", InFilename);
				fprintf(stderr, "-------------------------------------------\n");
				errLevel = 1;
				continue;
			}
			outFilename += bz2Tail;
		}

		// setup signal handling filenames
		sigInFilename = InFilename;
		sigOutFilename = outFilename.c_str();

		if (strcmp(InFilename, "-") != 0) 
		{
			struct stat statbuf;
			// read file for compression
			hInfile = open(InFilename, O_RDONLY | O_BINARY);
			// check to see if file exists before processing
			if (hInfile == -1)
			{
				fprintf(stderr, "pbzip2: *ERROR: File [%s] NOT found!  Skipping...\n", InFilename);
				fprintf(stderr, "-------------------------------------------\n");
				errLevel = 1;
				continue;
			}

			// get some information about the file
			fstat(hInfile, &statbuf);
			// check to make input is not a directory
			if (S_ISDIR(statbuf.st_mode))
			{
				fprintf(stderr, "pbzip2: *ERROR: File [%s] is a directory!  Skipping...\n", InFilename);
				fprintf(stderr, "-------------------------------------------\n");
				errLevel = 1;
				continue;
			}
			// check to make sure input is a regular file
			if (!S_ISREG(statbuf.st_mode))
			{
				fprintf(stderr, "pbzip2: *ERROR: File [%s] is not a regular file!  Skipping...\n", InFilename);
				fprintf(stderr, "-------------------------------------------\n");
				errLevel = 1;
				continue;
			}
			// get size of file
			#ifndef WIN32
			InFileSize = statbuf.st_size;
			#else
			fileSize_temp.LowPart = GetFileSize((HANDLE)_get_osfhandle(hInfile), (unsigned long *)&fileSize_temp.HighPart);
			InFileSize = fileSize_temp.QuadPart;
			#endif
			// don't process a 0 byte file
			if (InFileSize == 0)
			{
				if (decompress == 1)
				{
					fprintf(stderr, "pbzip2: *ERROR: File is of size 0 [%s]!  Skipping...\n", InFilename);
					fprintf(stderr, "-------------------------------------------\n");
					errLevel = 1;
					continue;
				}
				
				// make sure we handle zero byte files specially
				zeroByteFile = 1;
			}
			else
				zeroByteFile = 0;

			// get file meta data to write to output file
			if (getFileMetaData(InFilename) != 0)
			{
				fprintf(stderr, "pbzip2: *ERROR: Could not get file meta data from [%s]!  Skipping...\n", InFilename);
				fprintf(stderr, "-------------------------------------------\n");
				errLevel = 1;
				continue;
			}
		}
		else
		{
			hInfile = 0;	// stdin
			InFileSize = -1;	// fake it
		}

		// check to see if output file exists
		if ((force != 1) && (OutputStdOut == 0))
		{
			hOutfile = open(outFilename.c_str(), O_RDONLY | O_BINARY);
			// check to see if file exists before processing
			if (hOutfile != -1)
			{
				fprintf(stderr, "pbzip2: *ERROR: Output file [%s] already exists!  Use -f to overwrite...\n", outFilename.c_str());
				fprintf(stderr, "-------------------------------------------\n");
				errLevel = 1;
				close(hOutfile);
				errLevel = 1;
				continue;
			}
		}

		if (readEntireFile == 1)
		{
			if (hInfile == 0) 
			{
				if (QuietMode != 1)
					fprintf(stderr, " *Warning: Ignoring -r switch since input is stdin.\n");
			}
			else
			{
				// determine block size to try and spread data equally over # CPUs
				blockSize = InFileSize / numCPU;
			}
		}

		// display per file settings
		if (QuietMode != 1)
		{
			fprintf(stderr, "         File #: %d of %d\n", fileLoop+1, FileListCount);
			fprintf(stderr, "     Input Name: %s\n", hInfile != 0 ? InFilename : "<stdin>");

			if (OutputStdOut == 0)
				fprintf(stderr, "    Output Name: %s\n\n", outFilename.c_str());
			else
				fprintf(stderr, "    Output Name: <stdout>\n\n");

			if (decompress == 1)
				fprintf(stderr, " BWT Block Size: %c00k\n", BWTblockSizeChar);
			if (strcmp(InFilename, "-") != 0) 
				fprintf(stderr, "     Input Size: %"PRIu64" bytes\n", (unsigned long long)InFileSize);
		}

		if (decompress == 1)
		{
			numBlocks = 0;
			// Do not use threads if we only have 1 CPU or small files
			if ((numCPU == 1) || (InFileSize < 1000000))
				noThreads = 1;
			else
				noThreads = 0;
			
			// Enable threads method for uncompressing from stdin
			if ((numCPU > 1) && (strcmp(InFilename, "-") == 0))
				noThreads = 0;
		}
		else
		{
			if (InFileSize > 0)
			{
				// calculate the # of blocks of data
				numBlocks = (InFileSize + blockSize - 1) / blockSize;
				// Do not use threads for small files where we only have 1 block to process
				// or if we only have 1 CPU
				if ((numBlocks == 1) || (numCPU == 1))
					noThreads = 1;
				else
					noThreads = 0;
			} 
			else 
			{
				// Simulate a "big" number of buffers. Will need to resize it later
				numBlocks = 10000;
			}
			
			// write special compressed data for special 0 byte input file case
			if (zeroByteFile == 1)
			{
				hOutfile = 1;
				// write to file instead of stdout
				if (OutputStdOut == 0)
				{
#if !USE_DPTHREAD
					hOutfile = open(outFilename.c_str(), O_RDWR | O_CREAT | O_TRUNC | O_BINARY, FILE_MODE);
#else 
					hOutfile = open(outFilename.c_str(), O_RDWR | O_CREAT | O_TRUNC | O_BINARY); 
#endif 
					// check to see if file creation was successful
					if (hOutfile == -1)
					{
						fprintf(stderr, "pbzip2: *ERROR: Could not create output file [%s]!\n", outFilename.c_str());
						close(hOutfile);
						errLevel = 1;
						continue;
					}
				}
				// write data to the output file
				ret = do_write(hOutfile, Bz2HeaderZero, sizeof(Bz2HeaderZero));
				if (OutputStdOut == 0)
					close(hOutfile);
				if (ret != sizeof(Bz2HeaderZero))
				{
					fprintf(stderr, "pbzip2: *ERROR: Could not write to file!  Skipping...\n");
					fprintf(stderr, "-------------------------------------------\n");
					errLevel = 1;
					continue;
				}
				if (QuietMode != 1)
				{
					fprintf(stderr, "    Output Size: %"PRIu64" bytes\n", (unsigned long long)sizeof(Bz2HeaderZero));
					fprintf(stderr, "-------------------------------------------\n");
				}
				// remove input file unless requested not to by user
				if (keep != 1)
				{
					struct stat statbuf;
					if (OutputStdOut == 0)
					{
						// only remove input file if output file exists
						if (stat(outFilename.c_str(), &statbuf) == 0)
							remove(InFilename);
					}
					else
						remove(InFilename);
				}
				continue;
			}
		}
		#ifdef PBZIP_DEBUG
		fprintf(stderr, "# Blocks: %d\n", numBlocks);
		#endif
		// set global variable
		NumBlocks = numBlocks;
		// Calculate maximum number of buffered blocks to use
		NumBufferedBlocksMax = maxMemory / blockSize;
		// Subtract blocks for number of extra buffers in producer and fileWriter (~ numCPU for each)
		if ((int)NumBufferedBlocksMax - (numCPU * 2) < 1)
			NumBufferedBlocksMax = 1;
		else
			NumBufferedBlocksMax = NumBufferedBlocksMax - (numCPU * 2);
		#ifdef PBZIP_DEBUG
		fprintf(stderr, "pbzip2: maxMemory: %d    blockSize: %d\n", maxMemory, blockSize);
		fprintf(stderr, "pbzip2: NumBufferedBlocksMax: %u\n", NumBufferedBlocksMax);
		#endif
		
		// create output buffer
		outputBufferInit(NumBufferedBlocksMax);

		if (decompress == 1)
		{
			// use multi-threaded code
			if (noThreads == 0)
			{
				// do decompression
				if (QuietMode != 1)
					fprintf(stderr, "Decompressing data...\n");
				for (i=0; (int)i < numCPU; i++)
				{
					ret = pthread_create(&fifo->consumers[i], &ChildThreadAttributes, consumer_decompress, fifo);
					if (ret != 0)
					{
						fprintf(stderr, "pbzip2: *ERROR: Not enough resources to create consumer thread #%u (code = %d)  Aborting...\n", i, ret);
						return 1;
					}
				}

				ret = pthread_create(&output, &ChildThreadAttributes, fileWriter, (void*)outFilename.c_str());
				if (ret != 0)
				{
					handle_error(EF_EXIT, 1,
							"pbzip2: *ERROR: Not enough resources to create fileWriter thread (code = %d)  Aborting...\n", ret);

					ret = pthread_join(TerminatorThread, NULL);
					return 1;
				}

				// start reading in data for decompression
				ret = producer_decompress(hInfile, InFileSize, fifo);
				if (ret == -99)
				{
					// only 1 block detected, use single threaded code to decompress
					noThreads = 1;

					switchedMtToSt = true;

					// wait for fileWriter thread to exit
					if (pthread_join(output, NULL) != 0)
					{
						errLevel = 1;
					}
				}
				else if (ret != 0)
					errLevel = 1;
			}
			
			// use single threaded code
			if ((noThreads == 1) && (errLevel == 0))
			{
				if (QuietMode != 1)
					fprintf(stderr, "Decompressing data (no threads)...\n");

				if (hInfile > 0)
					close(hInfile);
				ret = directdecompress(InFilename, outFilename.c_str());
				if (ret != 0)
					errLevel = 1;
			}
		}
		else
		{
			// do compression code
				
			// use multi-threaded code
			if (noThreads == 0)
			{
				if (QuietMode != 1)
					fprintf(stderr, "Compressing data...\n");
					
				for (i=0; (int)i < numCPU; i++)
				{
#if HEECHUL 
					fprintf(stderr, "Thread %d : fifo->consumer[%d]\n", ++g_thread_cnt, i); 
#endif 
					ret = pthread_create(&fifo->consumers[i], &ChildThreadAttributes, consumer, fifo);
					if (ret != 0)
					{
						fprintf(stderr, "pbzip2: *ERROR: Not enough resources to create consumer thread #%u (code = %d)  Aborting...\n", i, ret);
						return 1;
					}
				}

#if HEECHUL 
				fprintf(stderr, "Thread %d : fileWriter\n", ++g_thread_cnt); 
#endif 
	
				ret = pthread_create(&output, &ChildThreadAttributes, fileWriter, (void*)outFilename.c_str());
				if (ret != 0)
				{
					handle_error(EF_EXIT, 1,
							"pbzip2: *ERROR: Not enough resources to create fileWriter thread (code = %d)  Aborting...\n", ret);
					pthread_join(TerminatorThread, NULL);
					return 1;

				}
	
				// start reading in data for compression
				ret = producer(hInfile, blockSize, fifo);
				if (ret != 0)
					errLevel = 1;
			}
			else
			{
				// do not use threads for compression
				if (QuietMode != 1)
					fprintf(stderr, "Compressing data (no threads)...\n");

				ret = directcompress(hInfile, InFileSize, blockSize, outFilename.c_str());
				if (ret != 0)
					errLevel = 1;
			}
		} // else

		if (noThreads == 0)
		{
			// wait for fileWriter thread to exit
#if HEECHUL
			fprintf(stderr, "wait for output\n"); 
#endif 
			ret = pthread_join(output, NULL);
			if (ret != 0)
				errLevel = 1;
		}

		if ((noThreads == 0) || switchedMtToSt )
		{
			// wait for consumer threads to exit
			for (i = 0; (int)i < numCPU; i++)
			{
#if HEECHUL
				fprintf(stderr, "wait for consumer[%d]\n", i); 
#endif 
				ret = pthread_join(fifo->consumers[i], NULL);
				if (ret != 0)
					errLevel = 1;
			}
	  	}

		if (OutputStdOut == 0)
		{
			// write store file meta data to output file
			if (writeFileMetaData(outFilename.c_str()) != 0)
				fprintf(stderr, "pbzip2: *ERROR: Could not write file meta data to [%s]!\n", InFilename);
		}

		// finished processing file (mutex since accessed by cleanup procedure)
		safe_mutex_lock(&ErrorHandlerMutex);
		sigInFilename = NULL;
		sigOutFilename = NULL;
		safe_mutex_unlock(&ErrorHandlerMutex);

		// remove input file unless requested not to by user
		if (keep != 1)
		{
			struct stat statbuf;
			if (OutputStdOut == 0)
			{
				// only remove input file if output file exists
				if (stat(outFilename.c_str(), &statbuf) == 0)
					remove(InFilename);
			}
			else
				remove(InFilename);
		}

		// reclaim memory
		OutputBuffer.clear();
		fifo->empty = 1;
		fifo->full = 0;
		fifo->head = 0;
		fifo->tail = 0;

		if (QuietMode != 1)
			fprintf(stderr, "-------------------------------------------\n");
	} /* for */

	// Terminate signal handler thread sending SIGQUIT signal
	// ret = pthread_cancel(SignalHandlerThread); // problem on Cygwin

	ret = pthread_kill(SignalHandlerThread, SIG_HANDLER_QUIT_SIGNAL);
	if (ret != 0)
	{
		fprintf(stderr, "Couldn't signal signal QUIT to SignalHandlerThread [%d]. Quitting prematurely!\n", ret);
		exit(errLevel);
	}
	else
	{
#if HEECHUL
 		fprintf(stderr, "wait for SignalHander thread\n"); 
#endif 
		ret = pthread_join(SignalHandlerThread, NULL);
		if (ret != 0)
		{
			fprintf(stderr, "Error on join of SignalHandlerThread [%d]\n", ret);
		}
	}

	syncSetFinishedFlag(1);

#if HEECHUL
	fprintf(stderr, "wait for terminator thread\n"); 
#endif 
	ret = pthread_join(TerminatorThread, NULL);
	if (ret != 0)
	{
		fprintf(stderr, "Error on join of TerminatorThread [%d]\n", ret);
	}

	// reclaim memory
	queueDelete(fifo);
	mutexesDelete();
	disposeMemory(FileList);

	// get current time for end of benchmark
	#ifndef WIN32
	gettimeofday(&tvStopTime, &tz);
	#else
	GetSystemTime(&systemtime);
	SystemTimeToFileTime(&systemtime, (FILETIME *)&filetime);
	tvStopTime.tv_sec = filetime.QuadPart / 10000000;
	tvStopTime.tv_usec = (filetime.QuadPart - (LONGLONG)tvStopTime.tv_sec * 10000000) / 10;
	#endif

	#ifdef PBZIP_DEBUG
	fprintf(stderr, "\n Start Time: %ld + %ld\n", tvStartTime.tv_sec, tvStartTime.tv_usec);
	fprintf(stderr, " Stop Time : %ld + %ld\n", tvStopTime.tv_sec, tvStopTime.tv_usec);
	#endif

	// convert time structure to real numbers
	timeStart = (double)tvStartTime.tv_sec + ((double)tvStartTime.tv_usec / 1000000);
	timeStop = (double)tvStopTime.tv_sec + ((double)tvStopTime.tv_usec / 1000000);
	timeCalc = timeStop - timeStart;
	if (QuietMode != 1)
		fprintf(stderr, "\n     Wall Clock: %f seconds\n", timeCalc);

#if USE_DPTHREAD
	det_print_stat(); 
#endif 
	exit(errLevel);
}
