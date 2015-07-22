# Makefile for ./camserver
#

.EXPORT_ALL_VARIABLES:

# enable the camera-specific code - a define in CFLAGS - ONE and ONLY ONE!!
#CAMERA = SLSP2_1C_CAMERA
CAMERA = SLSP2DET_CAMERA
#CAMERA = SLSP2PS_CAMERA
#CAMERA = ICXIDIS1_CAMERA
#CAMERA = DEMO_CAMERA

# select the camera-specific directory
ifeq ($(CAMERA), SLSP2_1C_CAMERA)
PKG = slsp2_1c_cam
endif

ifeq ($(CAMERA), SLSP2DET_CAMERA)
PKG = slsp2det_cam
endif

ifeq ($(CAMERA), SLSP2PS_CAMERA)
PKG = slsp2ps_cam
endif

ifeq ($(CAMERA), ICXIDIS1_CAMERA)
PKG = icxdis1cam
endif

ifeq ($(CAMERA), DEMO_CAMERA)
PKG = demo_cam
endif

TOPDIR = $(shell pwd)

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
#CFLAGS = -g -Wall -I$(HPATH) -O0 -DDEBUG -D$(CAMERA)
CFLAGS = -g -Wall -I$(HPATH) -O2 -DDEBUG -D$(CAMERA)
#CFLAGS = -Wall -I$(HPATH) -O2 -D$(CAMERA)
CPPFLAGS = -I$(GPATH)
LIBS = -lm -lncurses -lreadline

EXTRALIBS = \
	$(TOPDIR)/../../libs/misclib.a \
	$(ARCHIVEPATH)/camlib.a

SRC = \
	camserver.c

TESTSRC = \

DIRS = \
	misc \
	util

INCLUDE = cam_config.h \
	imageio.h
TESTINCLUDE =

ARCHIVE =

# incorporate package-specific information
# this is not optional, as it is in the tvx Makefile, because camserver
# usually needs some supplemental package (not really for the demo).

LOCAL_INC = ../$(PKG)/include
CFLAGS += -I$(HPATH)/$(LOCAL_INC)

DEPEND = $(PKG)/tvxcampkg.c

DIRS += \
	$(PKG)

# link order is important - libcbf last
EXTRALIBS += \
	$(ARCHIVEPATH)/$(PKG)lib.a \
	$(TOPDIR)/../../libs/iolib.a \
	$(TOPDIR)/../../libs/libcbf.a

OBJS = $(SRC:.c=.o) 
TESTOBJS = $(TESTSRC:.c=.o)

PROGRAM = camserver

all: .depend subdirs $(PROGRAM)

subdirs:
	for i in $(DIRS); do $(MAKE) -C $$i; done

camserver.o: camserver.c $(DEPEND)
	misc/mkheader $(PROGRAM) $(HPATH)/$(LOCAL_INC)
	$(CC) $(CFLAGS) -c $< -o $*.o

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $*.o

$(PROGRAM): $(EXTRALIBS) $(OBJS)
	$(CC) $(CFLAGS) $(OBJS) -o $(PROGRAM) \
	$(EXTRALIBS) $(ARCHIVE) $(LIBS)

$(OBJS): Makefile $(addprefix $(HPATH)/, $(INCLUDE)) $(TESTINCLUDE)

depend .depend dep:
	$(CC) $(CFLAGS) -MM $(SRC) > $@

ifeq (.depend,$(wildcard .depend))
include .depend
endif

clean:
	for i in $(DIRS); do $(MAKE) clean -C $$i; done
	$(RMF) core* *.o camdbg.out $(PROGRAM) $(ARCHIVE) .depend ./images/*

distclean: clean
	for i in $(DIRS); do $(MAKE) distclean -C $$i; done
	$(RMF) $(ARCHIVEPATH)/*
