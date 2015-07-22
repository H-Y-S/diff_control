# Makefile for dcbt - the dcb test program

.EXPORT_ALL_VARIABLES:

ifndef TOPDIR

TOPDIR = $(shell cd ../..; pwd)
MAKEFLAGS = --stop

PKG = slsp2det_cam
CAMERA = SLSP2DET_CAMERA

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
CFLAGS = -g -Wall -D_LINUX_ -I$(HPATH) -O0 -DDEBUG -D$(CAMERA)
CPPFLAGS = -I$(GPATH)
LIBS = -lm -lncurses -lreadline

LOCAL_INC = ../$(PKG)/include
CFLAGS += -I$(HPATH)/$(LOCAL_INC)

endif

CFLAGS += -Iinclude

EXTRALIBS = \
	$(ARCHIVEPATH)/camlib.a \
	$(TOPDIR)/../../libs/misclib.a \
	$(TOPDIR)/../../libs/iolib.a \
	$(TOPDIR)/../../libs/libcbf.a

SRC = \
	dcblib.c \
	time_log.c
	

TESTSRC = \
	dcbt.c

DIRS = \
	gsd \
	gst

INCLUDE = $(addprefix ../$(PKG)/gstar/include/, dcb.h gslib.h time_log.h)
TESTINCLUDE = $(addprefix ../$(PKG)/gstar/include/, dcbt.h gsdc.h)

ARCHIVE = $(ARCHIVEPATH)/$(PKG)lib.a

OBJS = $(SRC:.c=.o)
TESTOBJS = $(TESTSRC:.c=.o)
EXTRAOBJS = \
	$(TOPDIR)/$(PKG)/util/xor_tab.o \
	$(TOPDIR)/$(PKG)/util/cutils.o 
LIBOBJS = \
	gst/util/gslib.o

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $*.o

PROGRAM = dcbt

all: subdirs .depend $(PROGRAM)

$(PROGRAM).o: $(PROGRAM).c
	$(TOPDIR)/misc/mkheader $(PROGRAM)
	$(CC) $(CFLAGS) -c $< -o $*.o

subdirs: gsd gst
	for i in $(DIRS); do $(MAKE) -C $$i; done

gsd:
	ln -s src/gs_drv* gsd

gst:
	ln -s src/gst* gst

$(PROGRAM): $(OBJS) $(TESTOBJS) $(EXTRALIBS)
	$(AR) $(ARCHIVE) $(OBJS)
	$(CC) -o $(PROGRAM) $(OBJS) $(TESTOBJS) $(EXTRAOBJS) \
	$(LIBOBJS) $(EXTRALIBS) $(LIBS) $(ARCHIVE)

$(OBJS): Makefile $(addprefix $(HPATH)/, $(INCLUDE) $(TESTINCLUDE) )

clean:
	for i in $(DIRS); do $(MAKE) clean -C $$i; done
	$(RMF) core *.o debug.out .depend $(PROGRAM)

distclean: clean
	rm -f $(ARCHIVEPATH)/*

depend .depend dep:
	$(CC) $(CFLAGS) -MM $(SRC) > $@

ifeq (.depend, $(wildcard .depend))
include .depend
endif
