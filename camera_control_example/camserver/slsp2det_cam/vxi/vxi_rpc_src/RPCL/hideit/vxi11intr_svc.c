/*
 * Please do not edit this file.
 * It was generated using rpcgen.
 */

#include "vxi11intr.h"
#include <stdio.h>
#include <stdlib.h>/* getenv, exit */
#include <rpc/pmap_clnt.h> /* for pmap_unset */
#include <string.h> /* strcmp */ 
#include <memory.h>
#include <sys/socket.h>
#include <netinet/in.h>

#ifdef __STDC__
#define SIG_PF void(*)(int)
#endif

static void
device_intr_1(struct svc_req *rqstp, register SVCXPRT *transp)
{
	union {
		Device_SrqParms device_intr_srq_1_arg;
	} argument;
	char *result;
	xdrproc_t xdr_argument, xdr_result;
	char *(*local)(char *, struct svc_req *);

	switch (rqstp->rq_proc) {
	case NULLPROC:
		(void) svc_sendreply(transp, (xdrproc_t) xdr_void, (char *)NULL);
		return;

	case device_intr_srq:
		xdr_argument = (xdrproc_t) xdr_Device_SrqParms;
		xdr_result = (xdrproc_t) xdr_void;
		local = (char *(*)(char *, struct svc_req *)) device_intr_srq_1_svc;
		break;

	default:
		svcerr_noproc(transp);
		return;
	}
	(void) memset((char *)&argument, 0, sizeof (argument));
	if (!svc_getargs(transp, xdr_argument, (caddr_t) &argument)) {
		svcerr_decode(transp);
		return;
	}
	result = (*local)((char *)&argument, rqstp);
	if (result != NULL && !svc_sendreply(transp, xdr_result, result)) {
		svcerr_systemerr(transp);
	}
	if (!svc_freeargs(transp, xdr_argument, (caddr_t) &argument)) {
		fprintf(stderr, "unable to free arguments");
		exit(1);
	}
	return;
}

int
main(int argc, char **argv)
{
	register SVCXPRT *transp;

	(void) pmap_unset(DEVICE_INTR, DEVICE_INTR_VERSION);

	transp = svcudp_create(RPC_ANYSOCK);
	if (transp == NULL) {
		fprintf(stderr, "cannot create udp service.");
		exit(1);
	}
	if (!svc_register(transp, DEVICE_INTR, DEVICE_INTR_VERSION, device_intr_1, IPPROTO_UDP)) {
		fprintf(stderr, "unable to register (DEVICE_INTR, DEVICE_INTR_VERSION, udp).");
		exit(1);
	}

	transp = svctcp_create(RPC_ANYSOCK, 0, 0);
	if (transp == NULL) {
		fprintf(stderr, "cannot create tcp service.");
		exit(1);
	}
	if (!svc_register(transp, DEVICE_INTR, DEVICE_INTR_VERSION, device_intr_1, IPPROTO_TCP)) {
		fprintf(stderr, "unable to register (DEVICE_INTR, DEVICE_INTR_VERSION, tcp).");
		exit(1);
	}

	svc_run();
	fprintf(stderr, "svc_run returned");
	exit(1);
	/* NOTREACHED */
}
