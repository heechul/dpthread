TOPDIR  := $(shell if [ "$$PWD" != "" ]; then echo $$PWD; else pwd; fi)

include config.mk

DIRS=src 

all: 
	@echo Compiling for \'$(ARCH)\' target
	@set -e ; for d in $(DIRS) ; do $(MAKE) -C $$d $@ ; done

src:
	$(MAKE) -C src

app:
	$(MAKE) -C test 

check:
	./check.sh 

bench:
	./bench.sh 


etags:
	etags `find src include -name "*.[ch]"` 

clean: 
	@set -e ; for d in $(DIRS) ; do $(MAKE) -C $$d $@ ; done
	rm *~

distclean:  clean

depend: 
	@set -e ; for d in $(DIRS) ; do $(MAKE) -C $$d $@ ; done

tar: clean
	a=`basename $$PWD`; cd ..; tar zcf $$a.tar.gz $$a; echo generated ../$$a.tar.gz; cp -v $$a.tar.gz ~/Dropbox;

tarcvs: clean
	a=`basename $$PWD`; cd ..; tar --exclude=CVS -zcf $$a.tar.gz $$a; echo generated ../$$a.tar.gz;
install: 
	@set -e ; for d in $(DIRS) ; do $(MAKE) -C $$d $@ ; done

.PHONY: all clean distclean depend tar tarcvs install lib

# DO NOT DELETE
