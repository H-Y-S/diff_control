# Makefile for gsd.ko - the gigastar driver module

DEBUG = y

ifndef TOPDIR
	TOPDIR = `shell pwd`
endif
MAKEFLAGS = --stop

TDIR = `cat VERSION`

CP = cp -a

RM = rm
RMF = rm -f
RMRF = rm -rf
RMDIR = rmdir

ifneq ($(KERNELRELEASE),)
obj-m := gsd.o
  ifeq ($(DEBUG),y)
    EXTRA_CFLAGS += -DDEBUG
  endif

else
KDIR := /lib/modules/$(shell uname -r)/build
PWD := $(shell pwd)

default:
	$(MAKE) -C $(KDIR) SUBDIRS=$(PWD) modules
endif
 
clean:
	$(RMF) *.o *.ko *.mod.c
	$(RMRF) .tmp* .gs*

tar: clean
	find * -name "*.o" -exec rm {} \;
	find * -name "*~" -exec rm {} \;
	find * -name "#*#" -exec rm {} \;
	mkdir $(TDIR)
	$(CP) \
	gsd.c \
	gsd.h \
	gsdc.h \
	Makefile \
	GPL-2.0 \
	VERSION \
	ChangeLog \
	gsd_load.sh \
	gsd_unload.sh \
	README \
	$(TDIR)
	tar -zcf ../`cat VERSION`.tgz $(TDIR)
	$(RMRF) $(TDIR)
	@echo ../`cat VERSION`.tgz was written
