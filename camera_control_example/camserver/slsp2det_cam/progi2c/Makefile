# Makefile for progi2c
# Note that spaces instead of tabs give a "missing separator" error

.EXPORT_ALL_VARIABLES:
ifndef TOPDIR

TOPDIR = $(shell cd ../..; pwd)
MAKEFLAGS = --stop

PKG = slsp2det_cam
CAMERA = SLSP2DET_CAMERA

RM = rm
RMF = rm -f
RMDIR = rmdir
AR = ar -crs
LNF = ln -snf

INSTALL = /bin/install -c
INSTALL_DATA = ${INSTALL} -m 644
INSTALL_PROGRAM = ${INSTALL}

CC = gcc
CPP = g++
GPATH =
HPATH = $(TOPDIR)/include
ARCHIVEPATH = $(TOPDIR)/libs
XLIBPATH = /usr/X11/lib
CFLAGS = -g -Wall -I$(HPATH) -O0 -DDEBUG -D$(CAMERA)
CPPFLAGS = -I$(GPATH)
LIBS =-lm -lncurses

LOCAL_INC = ../$(PKG)/include
CFLAGS += -I$(HPATH)/$(LOCAL_INC)

endif

EXTRALIBS = \
	$(TOPDIR)/../../libs/misclib.a \
	$(TOPDIR)/../../libs/iolib.a \
	$(TOPDIR)/../../libs/libcbf.a \
	$(ARCHIVEPATH)/camlib.a
	
SRC = \
	signals.c \
	progi2c.c \
	i2c_util.c

TESTSRC = \
	progi2ct.c

DIRS = \

INCLUDE = \
	$(LOCAL_INC)/pix_detector.h \
	$(LOCAL_INC)/i2c_util.h \
	$(LOCAL_INC)/detsys.h \
	$(LOCAL_INC)/tvxcampkg.h \
	$(LOCAL_INC)/cam_tbl.h \
	debug.h \
	cam_config.h
TESTINCLUDE = 

ARCHIVE = $(ARCHIVEPATH)/$(PKG)lib.a

OBJS = $(SRC:.c=.o)
TESTOBJS = $(TESTSRC:.c=.o)

EXTRAOBJS = \
	../util/xor_tab.o \
	../util/cutils.o

.c.o:
	$(CC) -c $(CFLAGS) $<

PROGRAM = progi2ct

all: $(PROGRAM)

subdirs:
	for i in $(DIRS); do $(MAKE) -C $$i; done

$(OBJS): Makefile $(addprefix $(HPATH)/, $(INCLUDE) $(TESTINCLUDE) )

$(PROGRAM): $(OBJS) $(TESTOBJS) $(EXTRALIBS) $(EXTRAOBJS)
	$(AR) $(ARCHIVE) $(OBJS)
	$(CC) $(OPTS) $(LIBS) -o $(PROGRAM) $(OBJS) $(TESTOBJS) $(EXTRAOBJS) \
	$(EXTRALIBS) $(ARCHIVE)
	
clean:
#	for i in $(DIRS); do $(MAKE) clean -C $$i; done
	$(RMF) core *.o *~ *.dat camdbg.out $(PROGRAM)

distclean: clean
	$(RMF) $(ARCHIVE)

