CC	= gcc

CFLAGS 	= -DSQLITE_OMIT_LOAD_EXTENSION=1 \
	  -DSQLITE_OMIT_AUTOINIT=1 \
	  -DHEECHUL=1 \
	  -g

	 # -DSQLITE_HOMEGROWN_RECURSIVE_MUTEX=1 \
LDFLAGS	= -lpthread 

DPTHREAD = 1

ifeq ($(DPTHREAD),1) 
CFLAGS 	+= -DUSE_DPTHREAD=1 \
	   -I. \
	   -I../../../include \

LDFLAGS	+= -L../../../lib 

LIBS	+= -ldpthread -lpfm 
endif 

LIBS += -lpthread 

all: threadtest1 threadtest2 

orig:
	make clean
	make DPTHREAD=0 

heechul: heechul.o ../sqlite3.o 
	$(CC) $(CFLAGS) -o $@ $(LDFLAGS) $^ $(LIBS)

threadtest1: threadtest1.o ../sqlite3.o 
	$(CC) $(CFLAGS) -o $@ $(LDFLAGS) $^ $(LIBS)

threadtest2: threadtest2.o ../sqlite3.o 
	$(CC) $(CFLAGS) -o $@ $(LDFLAGS) $^ $(LIBS)

clean:
	-rm *.o ../sqlite3.o 
	-rm log*.p* log*.interrupts*

etags:	
	etags *.c ../sqlite3.c 

