# Makefile for camlib for icxdis1 package
#

.EXPORT_ALL_VARIABLES:

ifndef TOPDIR
TOPDIR = $(shell cd ../..; pwd)

PKG = icxdis1cam
CAMERA = ICXIDIS1_CAMERA

CP = cp -a
RM = rm
RMF = rm -f
RMRF = rm -rf
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
CFLAGS = -g -Wall -I$(HPATH) -O0 -DDEBUG
CPPFLAGS = -I$(GPATH)
LIBS = -lm -lncurses

LOCAL_INC = ../$(PKG)/include
CFLAGS += -I$(HPATH)/$(LOCAL_INC)

endif

EXTRALIBS = \
	$(TOPDIR)/../../libs/misclib.a \
	$(TOPDIR)/../../libs/iolib.a \
	$(TOPDIR)/../../libs/libcbf.a \
	$(ARCHIVEPATH)/camlib.a

SRC = \
	interface.c \
	cam_config.c \
	cam_tbl.c

TESTSRC = \
	test1.c

DIRS = \

INCLUDE =
TESTINCLUDE =

ARCHIVE = $(ARCHIVEPATH)/$(PKG)lib.a

OBJS = $(SRC:.c=.o) 
TESTOBJS = $(TESTSRC:.c=.o)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $*.o

PROGRAM = test1

all: $(PROGRAM)

cam_config.o: cam_config.c $(HPATH)/cam_config.h
	../../misc/mkheader cam_config
	$(CC) $(CFLAGS) -c $< -o $*.o

$(PROGRAM): $(OBJS) $(TESTOBJS)
	$(AR) $(ARCHIVE) $(OBJS)
	$(CC) $(CFLAGS) -o $(PROGRAM) $(TESTOBJS) $(OBJS) \
	$(EXTRALIBS) $(LIBS)

$(OBJS): Makefile $(addprefix $(HPATH)/, $(INCLUDE) $(TESTINCLUDE) )

clean:
	$(RMF) core *.o *~ camdbg.out $(PROGRAM)

distclean: clean
	$(RMF) $(ARCHIVE)
