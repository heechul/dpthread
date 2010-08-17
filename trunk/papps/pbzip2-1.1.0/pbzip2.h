/* 
 * File:   pbzip2.h
 * Author: Yavor Nikolov
 *
 * Created on March 6, 2010, 10:18 PM
 */

#ifndef _PBZIP2_H
#define	_PBZIP2_H

#include <string>
#include <cctype>

#ifndef WIN32
#define	FILE_MODE	(S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH)
#define OFF_T		off_t
#else
#define	FILE_MODE	(S_IRUSR | S_IWUSR )
#define OFF_T		__int64
#endif

extern "C"
{
#ifndef WIN32
#include <utime.h>
#include <sys/time.h>
#include <unistd.h>
#else
#include <windows.h>
#include <io.h>
#endif
#ifdef __APPLE__
#include <sys/sysctl.h>
#endif
#ifdef __sun
#include <sys/loadavg.h>
#endif
#ifndef  __BORLANDC__
#define __STDC_FORMAT_MACROS
#include <inttypes.h>
#else
#define PRIu64 "Lu"
#define strncasecmp(x,y,z) strncmpi(x,y,z)
#endif
#ifdef __osf__
#define PRIu64 "llu"
#endif
}

// uncomment for debug output
//#define PBZIP_DEBUG

// uncomment to disable load average code (may be required for some platforms)
//#define PBZIP_NO_LOADAVG

// detect systems that are known not to support load average code
#if defined (WIN32) || defined (__CYGWIN32__) || defined (__MINGW32__) || defined (__BORLANDC__) || defined (__hpux) || defined (__osf__) || defined(__UCLIBC__)
	#define PBZIP_NO_LOADAVG
#endif

#ifdef WIN32
#define PATH_SEP	'\\'
#define usleep(x) Sleep(x/1000)
#define LOW_DWORD(x)  ((*(LARGE_INTEGER *)&x).LowPart)
#define HIGH_DWORD(x) ((*(LARGE_INTEGER *)&x).HighPart)
#ifndef _TIMEVAL_DEFINED /* also in winsock[2].h */
#define _TIMEVAL_DEFINED
struct timeval {
  long tv_sec;
  long tv_usec;
};
#endif
#else
#define PATH_SEP	'/'
#endif

#ifndef O_BINARY
#define O_BINARY 0
#endif

typedef struct
{
	char *buf;
	unsigned int bufSize;
} outBuff;

typedef enum
{
	EF_NOQUIT = 0,
	EF_EXIT = 1,
	EF_ABORT = 2
} ExitFlag;

/*
 *********************************************************
	Print error message and optionally exit or abort
    depending on exitFlag:
     0 - don't quit;
     1 - exit;
     2 - abort.
    On exit - exitCode status is used.
*/
int handle_error(ExitFlag exitFlag, int exitCode, const char *fmt, ...);

/*
 * Delegate to read but keep writing until count bytes are read or
 * error is encountered (on success all count bytes would be read)
 */
ssize_t do_read(int fd, void *buf, size_t count);

/*
 * Delegate to write but keep writing until count bytes are written or
 * error is encountered (on success all count bytes would be written)
 */
ssize_t do_write(int fd, const void *buf, size_t count);


/**
 * Dispose the given buffer memory if not NULL and make it NULL. Provided
 * buffer should be allocated with new[].
 */
template <typename C>
inline void disposeMemory(C *& pBuff)
{
	if (pBuff != NULL)
	{
		delete [] pBuff;
		pBuff = NULL;
	}
}


/**
 * Check if a given string ends with a given suffix ignoring case difference.
 *
 * @return true if str ends with suffix; false - otherwise
 */
template <typename charT>
inline bool ends_with_icase( const std::basic_string<charT> & str, const std::basic_string<charT> & suffix )
{
	int ti = str.size() - suffix.size();

	if ( ti < 0 )
	{
		return false;
	}

	size_t si = 0;
	while ( si < suffix.size() )
	{
		if ( ::tolower( str[ti] ) != ::tolower( suffix[si] ) )
		{
			return false;
		}
		++si;
		++ti;
	}
	
	return true;
}


#endif	/* _PBZIP2_H */

