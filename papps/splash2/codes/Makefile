TOPDIR  := $(shell if [ "$$PWD" != "" ]; then echo $$PWD; else pwd; fi)

all: Makefile.config 

Makefile.config: Makefile.config.in
	sed "s|@SPLASH2_ROOT@|$(TOPDIR)|g" $< > $@     

clean:
	rm -f Makefile.config 

