# Makefile for ./camera
#

.EXPORT_ALL_VARIABLES:

ifndef TOPDIR

TOPDIR = $(shell cd ..; pwd)

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
XLIBPATH64 = /usr/X11R6/lib64
CFLAGS = -g -Wall -I$(HPATH) -O0 -DDEBUG
CPPFLAGS = -I$(GPATH)
LIBS = -lm -lncurses
XLIBS = -L$(XLIBPATH) -L$(XLIBPATH64) \
	-lXm -lXt -lXmu -lXp -lXext -lX11 -lSM -lICE 
EXTRALIBS = 

endif

EXTRALIBS = \
	$(ARCHIVEPATH)/screenlib.a

SRC = \
	camclient.c

TESTSRC = \
	camclientt.c

DIRS = \
	camserver

INCLUDE = \
	debug.h \
	tvxobject.h \
	decoder.h \
	icf.h \
	tvxsys.h \
	screen.h \
	tools.h \
	pict.h \
	graph.h
TESTINCLUDE =

ARCHIVE = $(ARCHIVEPATH)/utillib.a

OBJS = $(SRC:.c=.o) 
TESTOBJS = $(TESTSRC:.c=.o)

PROGRAM = camclientt

all:  subdirs $(PROGRAM)

subdirs:
	for i in $(DIRS); do $(MAKE) -C $$i; done    

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $*.o

$(PROGRAM): $(OBJS) $(TESTOBJS)
	$(AR) $(ARCHIVE) $(OBJS)
	$(CC) $(CFLAGS) $(TESTOBJS) -o $(PROGRAM) $(ARCHIVE) \
	$(EXTRALIBS) $(LIBS) $(XLIBS)

$(OBJS): Makefile $(addprefix $(HPATH)/, $(INCLUDE)) $(TESTINCLUDE)

clean:
	for i in $(DIRS); do $(MAKE) clean -C $$i; done
	$(RMF) core *.o *~ camdbg.out $(PROGRAM) $(ARCHIVE)

distclean: clean
	for i in $(DIRS); do $(MAKE) distclean -C $$i; done
