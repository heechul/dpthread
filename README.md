# Introduction #


Writing a correct multithreaded program is hard because non-deterministic interleaving of threads can introduce hard to reproduce bugs. DPTRHEAD is a deterministic thread library, which you can use instead of pthread library,  aim to solve (or reduce) the difficulty.

DPTHREAD eliminates non-deterministic thread interleaving. Therefore, a program behavior depends only on its input but not on thread interleaving; we call this program is deterministic.  In comparison, typical pthread programs are non-deterministic in a sense that their outcome not only depends on its input but also depend on the order of thread interleavings.

For example, let’s see the following bank account example ( the full source code is in test/bankacct.c) .

```
int g_account;  // global variable 

void Deposit(int amount)
{
	int balance; // ERROR: balance can be stale
	pthread_mutex_lock(&lock); 
	balance = g_account;  
	pthread_mutex_unlock(&lock); 

	pthread_mutex_lock(&lock);
	g_account = balance + amount; 
	pthread_mutex_unlock(&lock); 

	return NULL; 
}

void Withdraw(int amount)
{
	int balance; // ERROR: balance can be stale
	pthread_mutex_lock(&lock);
	balance = g_account; 
	pthread_mutex_unlock(&lock); 

	pthread_mutex_lock(&lock);
	g_account = balance - amount; 
	pthread_mutex_unlock(&lock); 
	return NULL; 
}

main()
{
	g_account = 10; 
	ret = pthread_create(..., Deposit, 1); // Deposit(1) 
	ret = pthread_create(..., Withdraw, 2); //Withdraw(2) 
	... 
	assert( g_account == 9 ); 
}
```

This program have a bug because ‘balance’ can be stale. Therefore, the final value of g\_account is non-deterministic as it can be 8 or 11, depending on the order of thread interleavings. However, the most of the time, it will result in correct value -- 9. When I ran this program on my desktop, I got the first assertion violation at 25390th execution; which means that it is very difficult to reproduce the problem when it depends on thread interleaving.  DHPTHREAD make this program deterministic; if it is correct at first run, it will always be correct; if it get assertion violation, it always will be the same no matter how many times you run the program.  This makes programmer’s life much easier.

DPTHREAD use performance counters (inst\_retired:stores event counter) to determine the order of shared memory accesses between multiple threads. The idea of using performance counter for deterministic thread scheduling is first explored in [Kendo (ASPLOS’09)](http://www.google.com/url?sa=t&source=web&cd=1&ved=0CBYQFjAA&url=http%3A%2F%2Fpeople.csail.mit.edu%2Fmareko%2Fasplos073-olszewski.pdf&ei=nPB-TIySJ46onQegiOXvAQ&usg=AFQjCNFw7KpteuBfvkXl5edKG1FNzsOFew&sig2=QNohy7eWv3aZvo97fFVIqQ), primarily for scientific parallel applications. In fact, DPTHREAD use the same deterministic locking algorithm which is described in the Kendo paper. The difference between the two is more on implementation side.

DPTHREAD uses libpfm4 and perf\_event based kernel to access performance counter while Kendo used custom kernel patch (based on the paper description). Largely thanks to the power and flexibility of perf\_event kernel interface and libpfm4, DPTHREAD is less intrusive (no custom kernel patch) yet more flexible: you can run  arbitrary number of threads in arbitrary number of cores ( do not need to be 1 thread per core); you can run multiple DPTHREAD enabled programs (just like you can run multiple pthread programs in a computer).

DPTHREAD is currently tested for aget (mutithread file downloader), and sqlite (embedded database which supports concurrent accesses by multiple threads) and SPLASH2 benchmark suites (representing scientific parallel applications).  Please also check the examples in test/ directory ( deadlock, bankacct, order, and multivar. see test/README.txt for more details) as well to see different type of multithreaded bugs which are difficult to reproduce in pthread but easy to diagnose in DPTHREAD.


# Installation #

DPTHREAD use libpfm4 library to access performance counters. You can download it from http://perfmon2.sourceforge.net.

Once you install the libpfm4, type 'make' on the top directory of DPTHREAD. If the build succeed, you will see libdpthread.a in lib/ directory.

```
$ make 
Compiling for 'i686' target
make[1]: Entering directory `/home/heechul/Projects/dpthread/src'
sh -c ./detect_cpus.sh 
...
cp -a libdpthread.a ../lib
make[1]: Leaving directory `/home/heechul/Projects/dpthread/src'
```

Then, you can check whether it works correctly by doing the following.

```
$ make check 
...
ITERATION 9
>> compare log8.p0 vs log9.p0
>> compare log8.p1 vs log9.p1
 -> OK
PASS 
```


# Run #

Running the bankacct example with DPTHREAD will show the following output. No matter how many times you run, the result always will be the same.

```
$ cd test/
$ make bankacct  
$ DPTHREAD_DEBUG=0 ./bankacct 
[LT:20000000000698][ 1]EXIT: (hw_evt:523, sw_evt:47) ndet_evt:0, 3 locks and 0 barriers.
bankacct: bankacct.c:108: main: Assertion `g_account == 9' failed.
Aborted
```

If you want to disable deterministic execution, you can just comment out the following line in the source code and recompile it.

```
// #include <dpthread-wrapper.h>
```

And try to run as many times by doing the following.

```
cnt=0 
while true; do 
	cnt=`expr $cnt + 1` 
	if ! ./bankacct; then 
		echo "failed at $cnt th iteration"
		exit 1 
	fi 
done 
```
# Limitation #

**Supported processors.**  Currently, It only deterministically works on Intel Core2 based processors (Core2Duo and Core2Quad). It will run on other processors (i.e., i3, i5 and  i7) but it does not guarantee determinism because I can not find any deterministic counter (like inst\_retired:stores counter in Core2) in those processors.


**Data race.** It provides deterministic guarantee only when the program is data race free. Identifying and removing data race is, however, relatively easy because many tools, such as valgrind, are available; first run the interested program with “valgrind --tool=drd <your pgm>” command and remove reported data race; then recompile the program with DPTHREAD.


**Non-determinism in external library functions and system calls.** Library functions or system calls can be non-deterministic (internal data race, dependency to OS internal state, and so on). In such case we provide wrappers or deterministic alternative implementation (see include/dpthread-wrapper.h). While it was good enough for the tested programs, if you use unsupported functions or other external libraries which are non-deterministic, DPTHREAD may not guarantee deterministic execution.