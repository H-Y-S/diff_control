# Makefile for ./camera/util
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
CFLAGS = -g -Wall -I$(HPATH) -O0 -DDEBUG
CPPFLAGS = -I$(GPATH)
LIBS = -lm -lncurses -lreadline

LOCAL_INC = 

endif

EXTRALIBS = \
	$(TOPDIR)/../../libs/misclib.a \
	$(TOPDIR)/../../libs/iolib.a \
	$(TOPDIR)/../../libs/libcbf.a

SRC =

TESTSRC = \
	test1.c

DIRS = \

INCLUDE = cam_config.h debug.h
TESTINCLUDE = 

ARCHIVE = $(ARCHIVEPATH)/camlib.a

OBJS = $(SRC:.c=.o) 
TESTOBJS = $(TESTSRC:.c=.o)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $*.o

PROGRAM = test1

all: getline $(PROGRAM)

getline: getline.c
	$(CC) $(CFLAGS) $< -o $@ $(LIBS) 

$(PROGRAM): $(OBJS) $(TESTOBJS)
	$(AR) $(ARCHIVE) $(OBJS)
	$(CC) $(CFLAGS) $(TESTOBJS) -o $(PROGRAM) $(ARCHIVE) \
	$(EXTRALIBS) $(LIBS)

$(OBJS): Makefile $(addprefix $(HPATH)/, $(INCLUDE) $(TESTINCLUDE) )

clean:
	$(RMF) core *.o camdbg.out $(PROGRAM) getline

distclean: clean
	$(RMF) $(ARCHIVE)
