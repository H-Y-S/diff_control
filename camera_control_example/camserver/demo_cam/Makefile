# Makefile for the camlib for demo_cam package
#

.EXPORT_ALL_VARIABLES:

ifndef TOPDIR
TOPDIR = $(shell cd ..; pwd)

PKG = demo_cam
CAMERA = DEMO_CAMERA

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
CFLAGS = -g -Wall -I$(HPATH) -O0 -DDEBUG -D$(CAMERA)
CPPFLAGS = -I$(GPATH)
LIBS = -lm -lncurses

LOCAL_INC = ../$(PKG)/include
CFLAGS += -I$(HPATH)/$(LOCAL_INC)

endif

EXTRALIBS =

SRC = \

TESTSRC = \

DIRS = \
	util

INCLUDE =
TESTINCLUDE =

ARCHIVE = $(ARCHIVEPATH)/$(PKG)lib.a

OBJS = $(SRC:.c=.o) 
TESTOBJS = $(TESTSRC:.c=.o)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $*.o

PROGRAM =

all: subdirs $(ARCHIVE)

$(ARCHIVE): $(OBJS)
	$(AR) $(ARCHIVE) $^

subdirs:
	for i in $(DIRS); do $(MAKE) -C $$i; done

clean:
	for i in $(DIRS); do $(MAKE) clean -C $$i; done
	$(RMF) core *.o *~ camdbg.out $(PROGRAM)

distclean:
	for i in $(DIRS); do $(MAKE) distclean -C $$i; done
	$(RMF) $(ARCHIVE)
