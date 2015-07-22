# Makefile for misc
#

.EXPORT_ALL_VARIABLES:

ifndef TOPDIR
TOPDIR = ..
endif

RM = rm
RMF = rm -f
RMDIR = rmdir
AR = ar -crs

INSTALL = /bin/install -c
INSTALL_DATA = ${INSTALL} -m 644
INSTALL_PROGRAM = ${INSTALL}

CC = gcc
CPP = g++
GPATH =
HPATH = 
ARCHIVEPATH = 
XLIBPATH = /usr/X11/lib
CFLAGS = -g -Wall -I$(HPATH) -O0 -DDEBUG
CPPFLAGS = -I$(GPATH)
LIBS = -lm -lncurses
EXTRALIBS =

PROGRAMS = mkheader

all: mkheader $(PROGRAMS)

mkheader: mkheader.c
	$(CC) $(CFLAGS) $< -o $@ $(LIBS) 

$(PROGRAMS): Makefile

clean:
	$(RMF) core *.o *~ $(PROGRAMS)

distclean: clean
