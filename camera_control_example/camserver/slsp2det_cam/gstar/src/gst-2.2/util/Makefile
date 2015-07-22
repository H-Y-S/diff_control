# Makefile for gst utillib
#

.EXPORT_ALL_VARIABLES:

ifndef TOPDIR

TOPDIR = $(shell cd ../../../..; pwd)
MAKEFLAGS = --stop

PKG = slsp2det_cam

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
CFLAGS = -g -Wall -D_LINUX_ -I$(HPATH) -O0 -DDEBUG
CPPFLAGS = -I$(GPATH)
LIBS = -lm -lncurses

LOCAL_INC = ../$(PKG)/include
CFLAGS += -I$(HPATH)/$(LOCAL_INC)

endif

CFLAGS += -I../include

EXTRALIBS = \
	$(ARCHIVEPATH)/camlib.a \
	$(TOPDIR)/../../libs/misclib.a \
	$(TOPDIR)/../../libs/iolib.a

SRC = \
	gslib.c

TESTSRC = \
	test9.c

DIRS = \

INCLUDE = $(addprefix ../$(PKG)/gstar/gst/include/, gslib.h gsdc.h)
TESTINCLUDE = debug.h

ARCHIVE = $(ARCHIVEPATH)/$(PKG)lib.a

OBJS = $(SRC:.c=.o) 
TESTOBJS = $(TESTSRC:.c=.o)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $*.o

PROGRAM = test9

all: $(PROGRAM)

$(PROGRAM): $(OBJS) $(TESTOBJS)
	$(AR) $(ARCHIVE) $(OBJS)
	$(CC) $(CFLAGS) $(TESTOBJS) -o $(PROGRAM) $(ARCHIVE) \
	$(EXTRALIBS) $(LIBS)

$(OBJS): Makefile $(addprefix $(HPATH)/, $(INCLUDE) $(TESTINCLUDE) )

clean:
	$(RMF) core *.o debug.out $(PROGRAM) $(ARCHIVE)
