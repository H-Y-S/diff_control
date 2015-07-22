RD = ../RPCL
TD = $(HOME)/bin/$(shell uname)
CFLAGS = -I../RPCL

vxidvm: vxidvm.c
	gcc -w vxidvm.c $(RD)/vxi11core_xdr.c  -o vxidvm

clean:
	rm *.o
