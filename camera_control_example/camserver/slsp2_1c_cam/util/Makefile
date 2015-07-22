# Makefile for (part of) camlib for SLSP2_1C_CAMERA package
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
	$(TOPDIR)/../../libs/iolib.a \
	$(TOPDIR)/../../libs/libcbf.a \
	$(ARCHIVEPATH)/camlib.a

SRC = \
	xor_tab.c \
	cam_tbl.c \
	interface.c \
	cam_config.c \
	cutils.c \
	bl38_util.c

TESTSRC = \
	test5.c

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

PROGRAM = test5

all: $(PROGRAM) mktrimpat
	@echo -n $(addprefix $(shell pwd)/, $(OBJS))' ' >> $(TOPDIR)/$(PKG)/objlist

cam_config.o: cam_config.c $(HPATH)/cam_config.h
	../../misc/mkheader cam_config
	$(CC) $(CFLAGS) -c $< -o $*.o

cam_tbl.o: cam_tbl.c $(HPATH)/$(LOCAL_INC)/cam_tbl.h
	../../misc/mkheader cam_tbl
	$(CC) $(CFLAGS) -c $< -o $*.o

mktrimpat: mktrimpat.c
	$(CC) -o mktrimpat $<

#$(ARCHIVE): $(OBJS)
#	$(AR) -cr $(ARCHIVE) $^
#	ranlib $(ARCHIVE)

$(PROGRAM): $(OBJS) $(TESTOBJS)
	$(CC) $(CFLAGS) $(TESTOBJS) -o $(PROGRAM) $(OBJS) \
	$(EXTRALIBS) $(LIBS)

$(OBJS): Makefile $(addprefix $(HPATH)/, $(INCLUDE) $(TESTINCLUDE) )

clean:
	$(RMF) core *.o *~ debug.out camdbg.out $(PROGRAM) mktrimpat

distclean: clean
	$(RMF) $(ARCHIVE)
