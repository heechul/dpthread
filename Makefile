TOPDIR  := $(shell if [ "$$PWD" != "" ]; then echo $$PWD; else pwd; fi)

DIRS=src test papps/splash2/codes

export $(TOPDIR) 

include config.mk 

all: config.mk 
	@echo Compiling for \'$(ARCH)\' target
	@set -e ; for d in $(DIRS) ; do $(MAKE) -C $$d $@ ; done

config.mk: config.mk.in 
	sed "s|@DPTHREAD_ROOT@|$(TOPDIR)|g" $< > $@ 	

check:
	./check.sh 

bench:
	./bench.sh 

etags:
	etags `find src include -name "*.[ch]"` 

clean: 
	@set -e ; for d in $(DIRS) ; do $(MAKE) -C $$d $@ ; done
	rm -f *~ config.mk 

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
