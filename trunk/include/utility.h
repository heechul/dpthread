// utility.h 
//	Miscellaneous useful type, definitions, functions.
//
//	debugging messages, controllable from the command line arguments (-d)
//	debugging flags are:
//
//	'+' -- turn on all debug messages
//   	'l' -- lock  
//   	'c' -- communication 
//   	'b' -- barrier
//

#ifndef UTILITY_H
#define UTILITY_H

#include <sys/time.h>
#include <time.h>

// Boolean type 

#define TRUE            1
#define FALSE           0

typedef unsigned char  TBool;

// Miscellaneous useful routines
#define STRTOLONG(x)	(*((unsigned long *) (x)))

#define min(a,b)  (((a) < (b)) ? (a) : (b))
#define max(a,b)  (((a) > (b)) ? (a) : (b))
#define abs(a)    (((a) > 0) ? (a) : (-(a)))

#define likely(x)       __builtin_expect((x),1)
#define unlikely(x)     __builtin_expect((x),0)

void    swap(int *a, int *b);

// Divide and either round up or down 
#define divRoundDown(n,s)  ((n) / (s))
#define divRoundUp(n,s)    (((n) / (s)) + ((((n) % (s)) > 0) ? 1 : 0))

void   Assert(int cond, char *msg, ...);
void   DebugInit(char* flags);	// enable printing debug messages
TBool  DebugIsEnabled(char flag); 	// Is this debug flag enabled?
void   DEBUG (char flag, char* format, ...);  	// Print debug message 

#endif // UTILITY_H







