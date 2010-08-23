TOPDIR  := $(shell if [ "$$PWD" != "" ]; then echo $$PWD; else pwd; fi)/..

include $(TOPDIR)/config.mk
include $(TOPDIR)/rules.mk


ifeq ($(TEST),1)
	CFLAGS += -O0 
endif

# CFLAGS += -I../../crest-mt/include  # mini-libc 

CFLAGS += -I. -D_GNU_SOURCE -I$(DPTHREADDIR)/include 
LIBS += -lm -ldpthread -lpthread -lpfm -ldl #-lbz2
LDFLAGS += $(LIBS) 

# LDFLAGS += -L../../crest-mt/mini-libc
# LIBS += -lminiClibc 

TARGETS=deadlock multivar locktest cond_wait 

all: $(TARGETS)

$(TARGETS):  %:%.o # $(DPTHREADLIB) 
	$(CC) $(CFLAGS) -o $@ $(LDFLAGS) $^ $(LIBS) 

clean:
	$(RM) -f *.o $(TARGETS) *~ log*.p* a.out
