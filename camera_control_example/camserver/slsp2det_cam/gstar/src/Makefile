# makefile for gsd and gst as a standalone pair
# launch the target 'standalone' only from **src** directory
# do 'make' only in the standalone environment

TOPDIR = $(shell pwd)

DIRNAME = gigastar

TEMPDIR = /tmp/gsdgst/$(DIRNAME)

LIBS = -lm -lncurses -lreadline

CFLAGS = -g -Wall -I$(TOPDIR)/gst/include -o0 -DDEBUG

# note that dependents 'gsd' and 'gst' do not exist in the tvx tree;
# therefore 'make' issued in the tvx environment throws an error
all: gsd gst
	cd gsd; make
	cd gst/misc; gcc $(CFLAGS) -o mkheader mkheader.c
	cd gst/misc; gcc $(CFLAGS) -c -o0 -o debug.o debug.c
	cd gst/util; gcc $(CFLAGS) -c -o gslib.o gslib.c
	cd gst; misc/mkheader gst
	cd gst;gcc $(CFLAGS) -c -o gst.o gst.c
	cd gst;gcc $(CFLAGS) -o gst $(LIBS) gst.o misc/debug.o util/gslib.o
	cd gst;gcc $(CFLAGS) -o bong util/gslib.o bong.c

clean:
	cd gs_drv*; make clean
	cd gst*; rm -rf gst bong misc/mkheader
	rm -rf core* *.o debug.out

standalone: clean
	mkdir -p $(TEMPDIR)
	cd ..; cp -aL src/* $(TEMPDIR)
	cd $(TEMPDIR); ln -s gs_drv* gsd; ln -s gst* gst; \
	cd gst/include; ln -snf ../../gsd/gsdc.h
	cd $(TEMPDIR)/..; tar -zcf $(HOME)/gsd_gst.tgz $(DIRNAME)
	rm -rf $(dir $(TEMPDIR))
	cd ..; make
