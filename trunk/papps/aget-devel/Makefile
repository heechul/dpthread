# EnderUNIX Aget Makefile
# http://www.enderunix.org/aget/

OBJS = main.o Aget.o Misc.o Head.o Signal.o Download.o Ftp.o Resume.o loadrc.o
CFLAGS = -g -Wall 
LDFLAGS = -pthread
CC = gcc
STRIP = strip

USE_DPTHREAD := y

ifeq ($(USE_DPTHREAD),y)
DPTHREAD_DIR=../..
PFM4_DIR=../../../libpfm4
CFLAGS  += -I$(DPTHREAD_DIR)/include -I$(PFM4_DIR)/include \
	-DUSE_DPTHREAD=1 
LDFLAGS += \
	-L$(DPTHREAD_DIR)/lib -L$(PFM4_DIR)/lib \
	-ldpthread -lpfm -ldl -lpthread 
endif # ifeq ($(USE_DPTHREAD),y) 

# use mini libc 
#CFLAGS  += -I../../../crest-mt/include 
#LDFLAGS += -L../../../crest-mt/mini-libc -lminiClibc 

all: aget

aget: $(OBJS)
	$(CC) -o aget -g $(OBJS) $(LDFLAGS)

install: aget
	cp -f aget /usr/local/bin/aget

clean:
	rm -f *.o aget log*.p*
