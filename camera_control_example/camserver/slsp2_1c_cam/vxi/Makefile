# Makefile for vxi
#

.EXPORT_ALL_VARIABLES:

ifndef TOPDIR
TOPDIR = $(shell cd ../..; pwd)

PKG = slsp2_1c_cam
CAMERA = SLSP2_1C_CAMERA

RM = rm
RMF = rm -f
RMDIR = rmdir
AR = ar

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
XLIBS = -L$(XLIBPATH) -lXm -lXp -lSM -lICE -lXt -lXext -lX11

LOCAL_INC = ../$(PKG)/include
CFLAGS += -I$(HPATH)/$(LOCAL_INC)

endif

EXTRALIBS = \
	$(TOPDIR)/../../libs/misclib.a \
	$(ARCHIVEPATH)/camlib.a

SRC = \
	vxi11core_clnt.c \
	vxi11core_xdr.c \
	vxi.c

TESTSRC = \
	test6.c

DIRS = \

INCLUDE = \
	$(LOCAL_INC)/pix_detector.h \
	$(LOCAL_INC)/ppg_util.h \
	$(LOCAL_INC)/detsys.h \
	$(LOCAL_INC)/tvxcampkg.h \
	$(LOCAL_INC)/cam_tbl.h \
	debug.h \
	cam_config.h
TESTINCLUDE = 

ARCHIVE =

OBJS = $(SRC:.c=.o) 
TESTOBJS = $(TESTSRC:.c=.o)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $*.o

PROGRAM = test6

all: $(ARCHIVE) $(PROGRAM)
	@echo -n $(addprefix $(shell pwd)/, $(OBJS))' ' >> $(TOPDIR)/$(PKG)/objlist

#suppress warnings for this one
vxi11core_clnt.o:  vxi11core_clnt.c
	$(CC) -g -w -c -I$(HPATH) -I$(HPATH)/$(LOCAL_INC) -O0 -DDEBUG vxi11core_clnt.c

#suppress warnings for this one
vxi11core_xdr.o:  vxi11core_xdr.c
	$(CC) -g -w -c -I$(HPATH) -I$(HPATH)/$(LOCAL_INC) -O0 -DDEBUG vxi11core_xdr.c

#$(ARCHIVE): $(OBJS)
#	$(AR) -cr $(ARCHIVE) $^
#	ranlib $(ARCHIVE)

$(PROGRAM): $(OBJS) $(TESTOBJS)
	$(CC) $(CFLAGS) $(TESTOBJS) -o $(PROGRAM) $(OBJS) \
	$(EXTRALIBS) $(LIBS)

$(OBJS): Makefile $(addprefix $(HPATH)/, $(INCLUDE) $(TESTINCLUDE) )

clean:
	$(RMF) core *.o *~ debug.out $(PROGRAM)

distclean: clean
	$(RMF) $(ARCHIVE)
