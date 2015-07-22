# Makefile for gst - the test program for the gigastar driver

.EXPORT_ALL_VARIABLES:

ifndef TOPDIR

# N.B: this is correct only when coming thru the link
TOPDIR = $(shell cd ../../..; pwd)
MAKEFLAGS = --stop

PKG = slsp2det_cam

CP = cp -a
RM = rm
RMF = rm -f
RMRF = rm -rf
RMDIR = rmdir
AR = ar -crs
LNF = ln -snf

TDIR = `cat Version`

INSTALL = /bin/install -c
INSTALL_DATA = ${INSTALL} -m 644
INSTALL_PROGRAM = ${INSTALL}

CC = gcc
CPP = g++
GPATH =
HPATH = $(TOPDIR)/include
ARCHIVEPATH = $(TOPDIR)/libs
XLIBPATH = /usr/X11/lib
CFLAGS = -g -Wall -D_LINUX_ -I$(HPATH) -O0 -DDEBUG
CPPFLAGS = -I$(GPATH)
LIBS = -lm -lncurses -lreadline

LOCAL_INC = ../$(PKG)/include
CFLAGS += -I$(HPATH)/$(LOCAL_INC)

endif

CFLAGS += -Iinclude -D_SLS_CAMERA_

EXTRALIBS =  \
	$(ARCHIVEPATH)/$(PKG)lib.a \
	$(ARCHIVEPATH)/camlib.a \
	$(TOPDIR)/../../libs/misclib.a \
	$(TOPDIR)/../../libs/iolib.a

SRC = \
	gst.c
	

TESTSRC = \

DIRS = \
	util

INCLUDE = $(addprefix ../$(PKG)/gstar/gst/include/, gst.h gslib.h gsdc.h)
TESTINCLUDE = debug.h

ARCHIVE =

OBJS = $(SRC:.c=.o)
TESTOBJS = $(TESTSRC:.c=.o)
EXTRAOBJS = \
	$(TOPDIR)/$(PKG)/util/xor_tab.o

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $*.o

PROGRAM = gst

all: .depend subdirs $(PROGRAM)

$(PROGRAM).o: $(PROGRAM).c
	$(TOPDIR)/misc/mkheader $(PROGRAM)
	$(CC) $(CFLAGS) -c $< -o $*.o

subdirs:
	for i in $(DIRS); do $(MAKE) -C $$i; done

$(PROGRAM): $(OBJS) $(TESTOBJS) $(EXTRALIBS)
	$(CC) $(OPTS) -o $(PROGRAM) $(OBJS) $(TESTOBJS) $(EXTRAOBJS) \
	$(EXTRALIBS) $(LIBS) 

$(OBJS): Makefile $(addprefix $(HPATH)/, $(INCLUDE) $(TESTINCLUDE) )

clean:
	for i in $(DIRS); do $(MAKE) clean -C $$i; done
	$(RMF) core *.o debug.out  .depend $(PROGRAM)

depend .depend dep:
	$(CC) $(CFLAGS) -MM $(SRC) > $@

ifeq (.depend, $(wildcard .depend))
include .depend
endif
