.PHONY: all clean

all: vxi11core.h vxi11core_clnt.c vxi11core_svc.c vxi11core_xdr.c \
	vxi11intr.h vxi11intr_clnt.c vxi11intr_svc.c vxi11intr_xdr.c

vxi11core.h vxi11core_clnt.c vxi11core_svc.c vxi11core_xdr.c: vxi11core.rpcl
	rpcgen vxi11core.rpcl

vxi11intr.h vxi11intr_clnt.c vxi11intr_svc.c vxi11intr_xdr.c: vxi11intr.rpcl
	rpcgen vxi11intr.rpcl

clean:
	rm -rf *.h *.c
