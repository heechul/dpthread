#include <stdio.h>
#include <stdlib.h>
#include <dpthread.h> 
#include <mini-libc.h>

#include <string.h>

#define NUM_THREADS 4

static det_mutex_t m; 
static const int iteration = 1000000;

volatile int sum = 0; 

static struct _tbl {
	int input; 
	int count; 
} tbl[NUM_THREADS] = { 
	{16385, 0},
	{100, 0}, 
	{2031, 0}, 
	{8193, 0},
}; 


void test_malloc_free(int input)
{ // non-det 
	char *buf; 
	buf = malloc(input); 
	free(buf); 
}

void test_putchar(int input)
{ // non-det 
	char src[] = "abcdefghijklmnopqrstuvwxyz"; 

	// putchar(src[input % 26]); 
	crest_putchar(src[input % 26]); 
}

int  test_tolower(int input)
{ // non-det at fist call 
	char src[] = "abcDEFghiJKLmnoPQRstuvWXYz"; 
	int i;
	for ( i = 0; i < input % 26; i++ ) {
		src[i] = tolower(src[i]); 
	}
	return 0; 
}
int  test_strtol(int input)
{ // det 
	char number[]  = "102300"; 
	int base = 0; 
	switch ( input % 3) {
	case 0: base = 10; break; 
	case 1: base = 16; break;
	case 2: base = 8; break; 
	default: base = 0; break; 
	}
	return strtol(number, NULL, base); 
}

int  test_strncmp(int input)
{ // non-det at first call 
	int ret; 
	char s1[] = "abcdasljkdflak;jsdfa"; 	
	char slarge[] = "ABCDalsjkdflajsdf";
	char sless[] = "0000al;sjdflaksdf"; 
	char seq[] = "abcdalsjkdfasdf";
	char *s2 = slarge; 
	switch ( input % 3 ) { 
	case 0: s2 = slarge; break; 
	case 1: s2 = sless; break; 
	case 2: s2 = seq; break; 
	}
	
	ret = crest_strncmp(s1, s2, 10); 
	return ret; 
}

void test_memcpy(int input)
{ // non-det at first call 
	char src[] = "abcdefghijklmnopqrstuvwxyz"; 
	char dst[26]; 
	int i; 

	int repeat = input / 26; 
	int nchars = input % 26; 

	for ( i = 0; i < repeat; i++ ) { 
		// strncpy(src, dst, nchars); 
		crest_memcpy((void *)src, (void *)dst, nchars); 
	}
}

int test_memcmp(int input)
{ // det 
	int ret; 
	char s1[] = "abcd"; 	
	char slarge[] = "ABCD";
	char sless[] = "0000"; 
	char seq[] = "abcd";
	char *s2; 
	switch ( input % 3 ) { 
	case 0: s2 = slarge; break; 
	case 1: s2 = sless; break; 
	case 2: s2 = seq; break; 
	}
	ret = memcmp(s1, s2, 4); 
	return ret; 
}


void test_memchr(int input)
{ // non-det at first call 
	char src[] = "abcdefghijklmnopqrstuvwxyz"; 	
	crest_memchr(src, input % 26, 26); 
	// memchr(src, input % 26, 26);  // non-det 
}

void test_strcpy(int input)
{ // det 
	char src[] = "abcdefghijklmnopqrstuvwxyz"; 
	char dst[26]; 
	int i; 

	int repeat = input / 26; 
	int nchars = input % 26; 

	src[nchars] = 0; 

	for ( i = 0; i < repeat; i++ ) { 
		// crest_strcpy(src, dst); 
		strcpy(src, dst); 
	}
}

void test_strncpy(int input)
{ // non-det at first call 
	char src[] = "abcdefghijklmnopqrstuvwxyz"; 
	char dst[26]; 
	int i; 

	int repeat = input / 26; 
	int nchars = input % 26; 

	for ( i = 0; i < repeat; i++ ) { 
		strncpy(src, dst, nchars); 
		// crest_strncpy(src, dst, nchars); 
	}
}

#define TEST(x) test_strncpy(x)

static int __thread nondet_count = 0; 

void *my_worker(void *v)
{
	int i; 
	int64_t start, end; 
	int idx = (int)v;

	/* initialize */ 
	start = det_get_clock(); 
	TEST(tbl[idx].input);
	end   = det_get_clock(); 

	det_disable_logical_clock();
	tbl[idx].count = (end - start); 

	
	fprintf(stdout, "idx:%d, input:%d, count:%d\n", idx, tbl[idx].input, tbl[idx].count ); 

	// library calls 
	for ( i = 0; i < iteration; i++) { 
		det_enable_logical_clock(0);

		// if ( i % 1000 == 0 ) det_dbg("%d th call to TEST function\n", i); 
		start = det_get_clock(); 
		TEST(tbl[idx].input); 
		end   = det_get_clock(); 

		det_disable_logical_clock();
		if ( end - start != tbl[idx].count ) {
			// mismatch 
			fprintf(stderr, "%d: expected=%d, actual=%lld\n", 
				idx, tbl[idx].count, end - start); 
			if ( nondet_count > 0) {
				// det_enable_logical_clock(0);
				det_exit(NULL); 
			} else 
				nondet_count++; 
		}
	}
	// det_enable_logical_clock(0);
	return NULL; 
}

int main(int argc, char *argv[])
{
	int i; 
	pthread_t allthr[NUM_THREADS];
	int max_thr = NUM_THREADS; 

	det_init(argc, argv); 
	det_lock_init(&m); 

	for ( i = 0; i < max_thr; i++ ) { 
		det_create(allthr+i, NULL, my_worker, (void *)i);
	}

	for (i = 0; i < max_thr; i++) {
		det_join(allthr[i], NULL);
	}

	return 0; 
}
