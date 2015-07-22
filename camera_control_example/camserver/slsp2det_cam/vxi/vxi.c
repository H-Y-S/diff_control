/******************************************************************************
 *
 * $RCSfile: vxidemo.c,v $ (This file is best viewed with a tabwidth of 4)
 *
 ******************************************************************************
 *
 * Author: Benjamin Franksen
 *
 *		Demo and evaluation utility for VXI-11 and HP E2050A (core client)
 *
 * $Revision: 1.5 $
 *
 * $Date: 1999/12/03 11:23:49 $
 *
 * $Author: franksen $
 *
 * $Log: vxidemo.c,v $
 * Revision 1.5  1999/12/03 11:23:49  franksen
 * merge between 1.4 and changed 1.3
 *
 * Revision 1.4  1998/06/05 14:50:45  franksen
 *
 * Revision 1.3  1997/04/25 18:10:52  franksen
 * A bisserl aufgemotzt
 *
 * Revision 1.2  1997/04/25 14:20:18  franksen
 * So many changes ...
 *
 * Revision 1.1.1.1  1997/04/25 13:36:05  franksen
 * Imported using tkCVS
 *
 *
 * Copyright (c) 1996  Berliner Elektronenspeicherring-Gesellschaft
 *                           fuer Synchrotronstrahlung m.b.H.,
 *                                   Berlin, Germany
 *
 ******************************************************************************
 *
 * Notes: modified by Bernd Schmitt SLS, Paul Scherrer Institut Scwitzerland
 * Modified by Eric Eikenberrry Sep 2001
 *
 */


/*  Functions in this module:
**		char *vxiError();
**		int rpcopen(void);
**		int gpibsend(char *);
**		int gpibread(char *);
**		Device_Error *rpcclose(void);
*/

#include <ctype.h>
#include <stdio.h>
#include <netinet/in.h>	/* gethostbyname */
#include <netdb.h>		/* struct hostent */
#include "vxi11intr.h"
#include "vxi11core.h"
#include "vxi11.h"
#include "debug.h"


#define IO_TIMEOUT	10000 /* 10 seconds */
#define LOCK_TIMEOUT	0
#define NETWORK_ORDER	1

#define DEVICEIP        "129.129.202.31"	// inst039.psi.ch
//#define DEVICENAME      "hpib,9"			// dvm device
//#define DEVICENAME      "hpib,4"			// oscilloscope device

#define MAXLEN   80
#define MAXSPLIT 10


Device_Error		*derr;
Create_LinkParms	crlp;
Create_LinkResp		*crlr;
Device_WriteParms	dwrp;
Device_WriteResp	*dwrr;
Device_ReadParms	drdp;
Device_ReadResp		*drdr;

struct timeval timeout;

CLIENT *cl;
int  maxRecvSize;

/**************************************************************** 
 * functions
 ****************************************************************/

char *vxiError(err)
	Device_ErrorCode err;
{
	switch(err)
	{
	case VXI_OK:		return("no error"); break;
	case VXI_SYNERR:	return("syntax error"); break;
	case VXI_NOACCESS:	return("device not accessible"); break;
	case VXI_INVLINK:	return("invalid link identifier"); break;
	case VXI_PARAMERR:	return("parameter error"); break;
	case VXI_NOCHAN:	return("channel not established"); break;
	case VXI_NOTSUPP:	return("operation not supported"); break;
	case VXI_NORES:		return("out of resources"); break;
	case VXI_DEVLOCK:	return("device locked by another link"); break;
	case VXI_NOLOCK:	return("no lock held by this link"); break;
	case VXI_IOTIMEOUT:	return("I/O timeout"); break;
	case VXI_IOERR:		return("I/O error"); break;
	case VXI_INVADDR:	return("invalid address"); break;
	case VXI_ABORT:		return("abort"); break;
	case VXI_CHANEXIST:	return("channel already established"); break;
	default:			return("unknown error"); break;
	}
}

int rpcopen(char *devicename)
{
  /************************************
   * open rpc connection              *
   ************************************/
  char *svName;
//  char *dval;
  
  svName = DEVICEIP;
	
  cl = clnt_create(svName, DEVICE_CORE, DEVICE_CORE_VERSION, "tcp");
  if (cl == NULL)
    {
      clnt_pcreateerror(svName);
      return 1;
    }

  /* change timeout to 1 minute for debugging */
  timeout.tv_sec = 60;
  timeout.tv_usec = 0;
  if(!clnt_control(cl, CLSET_TIMEOUT, (char*)&timeout))
    DBG(3, "Warning: Could not set timeout!\n");


  /* fill Create_LinkParms structure */
  crlp.clientId = (long)cl;
  crlp.lockDevice = 0;
  crlp.lock_timeout = 10000;
  crlp.device = devicename;

  /* call create_link on instrument server */
  crlr = create_link_1(&crlp, cl);
  if (crlr == NULL)
    {
      clnt_perror(cl, svName);
      return 1;
    };

  /* show results */
  DBG(7, "create_link result  = %d (%s)\n", (int)crlr->error, vxiError(crlr->error));
  DBG(7, "device link         = %d\n", (int)(crlr->lid));
  DBG(7, "abort port          = %d\n", (int)(crlr->abortPort));
  DBG(7, "max receive size    = %d\n", (int)(crlr->maxRecvSize));

  /* on error exit now */
  if(crlr->error != VXI_OK)
    return 1;

  maxRecvSize = crlr->maxRecvSize;
//  dval = malloc(maxRecvSize);

  return 0;
}


int gpibsend(char *sendstring)
{
  /************************************
   * send string                      *
   ************************************/
  char   *svName;
  svName = DEVICEIP;

  DBG(7, "send string: %s\n",sendstring);
	    
  /* write string to instrument */
  dwrp.lid = crlr->lid;
  dwrp.io_timeout = IO_TIMEOUT;
  dwrp.lock_timeout = LOCK_TIMEOUT;
  dwrp.flags = VXI_ENDW;
  dwrp.data.data_len = strlen(sendstring);
  dwrp.data.data_val = sendstring;
  dwrr = device_write_1(&dwrp, cl);
  if(dwrr == NULL)
    {
      clnt_perror(cl, svName);
      return 1;
    };


  /* show write results */
  DBG(7, "device_write result = %d (%s)\n", (int)dwrr->error, vxiError(dwrr->error));
  DBG(7, "bytes written       = %d\n", (int)dwrr->size);

  return 1;
}


int gpibread(char readstring[MAXLEN])
{
  /************************************
   * receive answer                   *
   ************************************/
  int  reasonCount;
  int  recvSize;
  char   *svName;

  svName = DEVICEIP;

  recvSize = maxRecvSize;


  drdp.lid = crlr->lid;
  drdp.requestSize = recvSize;
  drdp.io_timeout = IO_TIMEOUT;
  drdp.lock_timeout = LOCK_TIMEOUT;
  drdp.flags = VXI_TERMCHRSET;
  drdp.termChar = '\n';
  drdr = device_read_1(&drdp, cl);
  if(drdr == NULL)
    {
      clnt_perror(cl, svName);
      return 1;
    };

  /* show read results */
  DBG(7, "device_read result  = %d (%s)\n", (int)drdr->error, vxiError(drdr->error));
  DBG(7, "bytes read          = %d\n", (int)drdr->data.data_len);
  if(drdr->reason)
    {
      DBG(7, "device_read terminated because ");
      reasonCount = 0;
      if(drdr->reason & VXI_REQCNT)
	{
	  DBG(7, "requestCount (%d) bytes were received", recvSize);
	  reasonCount++;
	}
      if(drdr->reason & VXI_CHR)
	{
	  if(reasonCount++)
	    DBG(7, "\n  and ");
	  DBG(7, "termChar encountered");
	}
      if(drdr->reason & VXI_ENDR)
	{
	  if(reasonCount++)
	    DBG(7, "\n  and ");
	  DBG(7, "an END indicator was read");
	}
      DBG(7, ".\n");
    }
  if(drdr->data.data_len >= 1)
    {
      drdr->data.data_val[drdr->data.data_len-1] = '\0';
      DBG(3, "received string is: \"%s\"\n", drdr->data.data_val);

      strcpy(readstring,drdr->data.data_val);
      /*      printf("received string is: \"%s\"\n", readstring); */
    }

  return 1;
}


Device_Error *rpcclose(void)
{
Device_Error *err;

err = destroy_link_1(&(crlr->lid), cl);
clnt_destroy(cl);
return err;
}
