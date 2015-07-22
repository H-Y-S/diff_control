# Makefile for (part of) camlib for SLSP2DET_CAMERA package
#

.EXPORT_ALL_VARIABLES:

ifndef TOPDIR
TOPDIR = $(shell cd ../..; pwd)

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
LIBS = -lm -lncurses -lreadline

LOCAL_INC = ../$(PKG)/include
CFLAGS += -I$(HPATH)/$(LOCAL_INC)

endif

EXTRALIBS = \
	$(TOPDIR)/../../libs/misclib.a \
	$(TOPDIR)/../../libs/iolib.a \
	$(TOPDIR)/../../libs/libcbf.a \
	$(ARCHIVEPATH)/camlib.a

SRC = \
	xor_tab.c \
	cam_tbl.c \
	cam_config.c \
	cutils.c \
	mx_parms.c \
	calibs.c \
	settrims.c \
	interface.c

TESTSRC = \
	test5.c

DIRS = \

INCLUDE = \
	$(LOCAL_INC)/pix_detector.h \
	$(LOCAL_INC)/i2c_util.h \
	$(LOCAL_INC)/detsys.h \
	$(LOCAL_INC)/tvxcampkg.h \
	$(LOCAL_INC)/cam_tbl.h \
	$(LOCAL_INC)/mx_parms.h \
	debug.h \
	cam_config.h
TESTINCLUDE =

ARCHIVE = $(ARCHIVEPATH)/$(PKG)lib.a

OBJS = $(SRC:.c=.o) 
TESTOBJS = $(TESTSRC:.c=.o)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $*.o

PROGRAM = test5

all: $(PROGRAM)

cam_config.o: cam_config.c $(HPATH)/cam_config.h
	../../misc/mkheader cam_config
	$(CC) $(CFLAGS) -c $< -o $*.o

cam_tbl.o: cam_tbl.c $(HPATH)/$(LOCAL_INC)/cam_tbl.h
	../../misc/mkheader cam_tbl
	$(CC) $(CFLAGS) -c $< -o $*.o

mx_parms.o: mx_parms.c $(HPATH)/$(LOCAL_INC)/mx_parms.h
	../../misc/mkheader mx_parms
	$(CC) $(CFLAGS) -c $< -o $*.o

calibs.o: calibs.c $(HPATH)/$(LOCAL_INC)/calibs.h
	../../misc/mkheader calibs
	$(CC) $(CFLAGS) -c $< -o $*.o

$(PROGRAM): $(OBJS) $(TESTOBJS)
	$(AR) $(ARCHIVE) $(OBJS)
	$(CC) $(CFLAGS) $(TESTOBJS) -o $(PROGRAM) $(OBJS) \
	$(EXTRALIBS) $(LIBS)

$(OBJS): Makefile $(addprefix $(HPATH)/, $(INCLUDE) $(TESTINCLUDE) )

clean:
	$(RMF) core *.o *~ debug.out camdbg.out $(PROGRAM) mktrimpat

distclean: clean
	$(RMF) $(ARCHIVE)
