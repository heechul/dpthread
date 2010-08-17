TOPDIR  := $(shell if [ "$$PWD" != "" ]; then echo $$PWD; else pwd; fi)/..

include $(TOPDIR)/config.mk
include $(TOPDIR)/rules.mk


ifeq ($(TEST),1)
	CFLAGS += -O0 
endif

CFLAGS += -I../../crest-mt/include  # mini-libc 

CFLAGS += -I. -D_GNU_SOURCE -I$(DPTHREADDIR)/include 
LIBS += -lm -ldpthread -lpthread -lpfm -ldl #-lbz2

LDFLAGS += -L../../crest-mt/mini-libc
LIBS += -lminiClibc 

TARGET=locktest locktest-benign deadlock iotest iotest2 iotest3 fib iotest4 ubench cond_wait simple test-evtcnt

$(TARGET):  %:%.o # $(DPTHREADLIB) 
	$(CC) $(CFLAGS) -o $@ $(LDFLAGS) $^ $(LIBS) 

all: $(TARGET)

clean:
	$(RM) -f *.o $(TARGET) *~ log*.p* a.out
