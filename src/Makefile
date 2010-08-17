#
# Deterministic threading runtime 
#
# This file is distributed under the University of Illinois Open Source
# License. See LICENSE.TXT for details.
#

TOPDIR  := $(shell if [ "$$PWD" != "" ]; then echo $$PWD; else pwd; fi)/..

include $(TOPDIR)/config.mk
include $(TOPDIR)/rules.mk

ENV_SRCS=det-posix.c det-libc.c 
DET_SRCS=dpthread.c queue.c perf_util.c 

CFLAGS += -D_REENTRANT -g -D__USE_GNU -I/usr/local/include -I../include 
CFLAGS += -I$(PFMLIBDIR)/include 

ifeq ($(VALGRIND),1)
CFLAGS += -DUSE_VALGRIND=1 -DUSE_RECURSIVE_MUTEX=1
endif 

TARGETS=libdpthread.a # libdpthread.so.1  # shared library make app behave non-deterministically.

OBJS=$(DET_SRCS:.c=.o) $(ENV_SRCS:.c=.o) 

INCDEP=$(DPTHREADDIR)/include/dpthread.h

all: config.h $(TARGETS)

$(OBJS): $(TOPDIR)/config.mk $(TOPDIR)/rules.mk Makefile $(INCDEP)

config.h: 
	sh -c ./detect_cpus.sh 

libdpthread.so.1: $(OBJS)
	gcc -fPIC -shared -Wl,-soname,libdpthread.so.1 -ldl -lpthread -L$(PFMLIBDIR) -lpfm -o libdpthread.so.1  $(OBJS) $(CFLAGS)
	[ ! -f "libdpthread.so" ] && ln -s libdpthread.so.1 libdpthread.so || echo "exist" 
	cp -a libdpthread.so* ../lib

libdpthread.a:  $(OBJS)
	$(RM) $@
	$(AR) cru $@ $(OBJS)
	cp -a libdpthread.a ../lib

clean:
	$(RM) -f *.o *.lo *.a *.so* *~ *.$(SOLIBEXT) config.h 

distclean: clean

depend:
	$(MKDEP) $(CFLAGS) $(SRCS)

valgrind:
	make VALGRIND=1
