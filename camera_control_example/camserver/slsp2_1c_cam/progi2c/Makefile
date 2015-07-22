# Makefile for progi2c
# Note that spaces instead of tabs give a "missing separator" error

.EXPORT_ALL_VARIABLES:
ifndef TOPDIR

TOPDIR = $(shell cd ../..; pwd)
MAKEFLAGS = --stop

PKG = slsp2_1c_cam
CAMERA = SLSP2_1C_CAMERA

RM = rm
RMF = rm -f
RMDIR = rmdir

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
XLIBS = -L$(XLIBPATH) -lXm -lXp -lSM -lICE -lXt -lXext -lX11

LOCAL_INC = ../$(PKG)/include
CFLAGS += -I$(HPATH)/$(LOCAL_INC)

endif

EXTRALIBS = \
	$(TOPDIR)/../../libs/misclib.a \
	$(ARCHIVEPATH)/camlib.a
	
SRC = \
	signals.c \
	progi2c.c \
	ppg_util.c

TESTSRC = \
	progi2ct.c

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

EXTRAOBJS = \
	../util/xor_tab.o \
	../util/cutils.o \
	../rpc/rpc_client.o \
	../rpc/rpc_clnt.o \
	../rpc/ioc_rpc_xdr.o \
	../vxi/vxi.o \
	../vxi/vxi11core_clnt.o \
	../vxi/vxi11core_xdr.o

.c.o:
	$(CC) -c $(CFLAGS) $<

PROGRAM = progi2ct

all: $(ARCHIVE) $(PROGRAM)
	@echo -n $(addprefix $(shell pwd)/, $(OBJS))' ' >> $(TOPDIR)/$(PKG)/objlist

subdirs:
#	for i in $(DIRS); do $(MAKE) -C $$i; done

#$(ARCHIVE): $(OBJS)
#	$(AR) -cr $(ARCHIVE) $^
#	ranlib $(ARCHIVE)

$(OBJS): Makefile $(addprefix $(HPATH)/, $(INCLUDE) $(TESTINCLUDE) )

$(PROGRAM): $(OBJS) $(TESTOBJS) $(EXTRALIBS) $(EXTRAOBJS)
	$(CC) $(OPTS) $(LIBS) -o $(PROGRAM) $(OBJS) $(TESTOBJS) $(EXTRAOBJS) \
	$(EXTRALIBS)
	
clean:
#	for i in $(DIRS); do $(MAKE) clean -C $$i; done
	$(RMF) core *.o *~ *.dat camdbg.out $(PROGRAM)

distclean: clean
	$(RMF) $(ARCHIVE)

#tar: distclean
#	tar -zcf progi2c.tgz *
