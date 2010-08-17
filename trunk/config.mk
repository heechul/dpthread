
SYS  := $(shell uname -s)
ARCH := $(shell uname -m)


DPTHREADDIR=$(TOPDIR)
PFMLIBDIR=$(TOPDIR)/../libpfm4

CC=gcc
LIBS=
INSTALL=install
LN?=ln -sf
DBG?=-g -Wall -Werror
#CFLAGS += $(DBG)
CFLAGS += -O2 -Wall -g -D_GNU_SOURCE
CFLAGS += -march=i686 
LDFLAGS += -L$(PFMLIBDIR)/lib -L$(DPTHREADDIR)/lib
