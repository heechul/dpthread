CC	= gcc

CFLAGS 	= -DSQLITE_OMIT_LOAD_EXTENSION=1 \
	  -DTHREADSAFE=1 \
	  -DHEECHUL=1 \
	  -g

#	  -DSQLITE_HOMEGROWN_RECURSIVE_MUTEX=1 \

LDFLAGS	= -lpthread 

DPTHREAD = 1

ifeq ($(DPTHREAD),1) 
CFLAGS 	+= -DUSE_DPTHREAD=1 \
	   -I. \
	   -I$(HOME)/Papers/cs523/dpthread/include \

LDFLAGS	+= -L$(HOME)/Papers/cs523/libpfm4/lib \
	   -L$(HOME)/Papers/cs523/dpthread/lib

LIBS	+= -ldl -ldpthread -lpfm 
endif 

all: threadtest1 threadtest2 sqlitetest_thrd35

orig:
	make clean
	make DPTHREAD=0 

heechul: heechul.o ../sqlite3.o 
	$(CC) $(CFLAGS) -o $@ $(LDFLAGS) $^ $(LIBS)

threadtest1: threadtest1.o ../sqlite3.o 
	$(CC) $(CFLAGS) -o $@ $(LDFLAGS) $^ $(LIBS)

threadtest2: threadtest2.o ../sqlite3.o 
	$(CC) $(CFLAGS) -o $@ $(LDFLAGS) $^ $(LIBS)

sqlitetest_thrd35: sqlitetest_thrd35.o ../sqlite3.o 
	$(CC) $(CFLAGS) -o $@ $(LDFLAGS) $^ $(LIBS)

clean:
	-rm *.o ../sqlite3.o 
	-rm log*.p*

etags:	
	etags *.c ../sqlite3.c 

