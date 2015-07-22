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
 *
 */


#include "../RPCL/vxi11core_clnt.c"
#include "../RPCL/vxi11intr.h"
#include "vxi11.h"

#include <ctype.h>
#include <stdio.h>

#include <netinet/in.h>	/* gethostbyname */
#include <netdb.h>		/* struct hostent */

#define IO_TIMEOUT	10000 /* 10 seconds */
#define LOCK_TIMEOUT	0
#define NETWORK_ORDER	1

#define DEVICEIP        "129.129.11.189"
#define DEVICENAME      "hpib,4"

#define MAXLEN   80
#define MAXSPLIT 10

/*	Device_RemoteFunc	drmf;
	Device_GenericParms	dgnp;
	Device_DocmdParms	ddcp;
	Device_DocmdResp	*ddcr;
	Device_LockParms	dlkp;
	Device_Link		dlnk;
	Device_ReadStbResp	*drsr;
	Device_EnableSrqParms desp;
*/

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
/*char readstring[MAXLEN]; */

static char *vxiError(err)
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

/**************************************************************** 
 * functions
 ****************************************************************/

int rpcopen(void);
int gpibsend(char *sendstring);
int gpibread(char readstring[MAXLEN]);

int splitstring(char *string, char split[][], char fs);

int main(int argc, char **argv)
{

  /*	struct hostent	    *myHostAddr;
	struct sockaddr_in  mySockAddr;
  */
  char *svName;
  char *command, *sendstring, readstring[MAXLEN], *fname, *fmode;
  FILE *fp;
  int i, j, nline, nchannel, nsplit;

  char split[MAXSPLIT][MAXLEN], result[5][MAXLEN];
 
  char sstring[][MAXLEN] = {"", "PAST? CUST,AVG", "PAST? CUST,LOW", "PAST? CUST,HIGH", "PAST? CUST,SIGMA"};


  if(argc < 2)
    {
      printf("possible commands are:\n");
      printf("print nline                      this prints the results from custom line nline\n");
      printf("save nline fname fmode nchannel  this saves the result in file fname with mode fmode and prints nchannel in front\n");
      
      return 1;
    }

  rpcopen();

  /* command is in argv[1] arguments in the other argv's */
  command = argv[1];

  if ( (strcmp(command,"print")!=0) && (strcmp(command,"save")!=0) ){
    printf("command not falid !\n");
    return -1;
  } 

  nline = atoi(argv[2]);

  if ( strcmp(command,"save") == 0 ) { 
    if(argc < 4) {
      printf("please enter filename and mode !\n");
      return -1;	    
    }
    fname = argv[3];
    fmode = argv[4];

    if ( (strcmp(fmode,"a")!=0) && (strcmp(fmode,"w")!=0) ) {
      printf("mode must be a (append data) or w (open new file) !\n");
      return -1;	    
    }
  }

  nchannel = atoi(argv[5]);
      
  /* loop over strings to send and extract answer */
  for (i=1; i<=4;i++) {
    sendstring = sstring[i];
    gpibsend(sendstring);
    gpibread(readstring);

    /*    printf("string read :%s\n",readstring); */
    nsplit = splitstring(readstring, split, ',');
    if (nsplit == 0 ) {
      printf("no splitting done !!\n");
    } else {
      strcpy(result[i],split[nline+1]);
      /*      for (j=0;j<=nsplit;j++) {
	printf("*%s*",split[j]);
	}
	printf("\n\n"); */
    }
  }

  printf("\n\n");

  for (j=0;j<=4;j++) {
    printf(" %s ",result[j]);
  }
  printf("\n\n");


  /* save string to a file */
  if (strcmp(command,"save")==0) {
    fp = fopen(fname,fmode);
    if (fp == NULL) {
      printf("error opening file !!\n");
    }
    fprintf(fp,"%i ", nchannel);
    for (j=0;j<=4;j++) {
      fprintf(fp," %s ", result[j]);
    }
    fprintf(fp,"\n");
    fclose(fp);
  }

  svName = DEVICEIP;
  printf("destroying link...\n");
  derr = destroy_link_1(&(crlr->lid), cl);
  if (derr == NULL)
    {
      clnt_perror(cl, svName);
      return 1;
    };

  /* show results */
  printf("destroy_link result = %d (%s)\n", derr->error, vxiError(derr->error));

  /* ready */
  return 0;
}






int rpcopen(void)
{
  /************************************
   * open rpc connection              *
   ************************************/
  char *svName;
  int  recvSize;
  char *dval;
  
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
    printf("Warning: Could not set timeout!\n");


  /* fill Create_LinkParms structure */
  crlp.clientId = cl;
  crlp.lockDevice = 0;
  crlp.lock_timeout = 10000;
  crlp.device = DEVICENAME;

  /* call create_link on instrument server */
  crlr = create_link_1(&crlp, cl);
  if (crlr == NULL)
    {
      clnt_perror(cl, svName);
      return 1;
    };

  /* show results */
  printf("create_link result  = %d (%s)\n", crlr->error, vxiError(crlr->error));
  printf("device link         = %d\n", crlr->lid);
  printf("abort port          = %d\n", (int)(crlr->abortPort));
  printf("max receive size    = %d\n", (int)(crlr->maxRecvSize));

  /* on error exit now */
  if(crlr->error != VXI_OK)
    return 0;

  maxRecvSize = crlr->maxRecvSize;
  dval = malloc(maxRecvSize);

  return 1;
}

int gpibsend(char *sendstring)
{
  /************************************
   * send string                      *
   ************************************/
  char   *svName;
  svName = DEVICEIP;

  printf("send string: %s",sendstring);
	    
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
  printf("device_write result = %d (%s)\n", dwrr->error, vxiError(dwrr->error));
  printf("bytes written       = %d\n", dwrr->size);

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
  printf("device_read result  = %d (%s)\n", drdr->error, vxiError(drdr->error));
  printf("bytes read          = %d\n", drdr->data.data_len);
  if(drdr->reason)
    {
      printf("device_read terminated because ");
      reasonCount = 0;
      if(drdr->reason & VXI_REQCNT)
	{
	  printf("requestCount (%d) bytes were received", recvSize);
	  reasonCount++;
	}
      if(drdr->reason & VXI_CHR)
	{
	  if(reasonCount++)
	    printf("\n  and ");
	  printf("termChar encountered");
	}
      if(drdr->reason & VXI_ENDR)
	{
	  if(reasonCount++)
	    printf("\n  and ");
	  printf("an END indicator was read");
	}
      printf(".\n");
    }
  if(drdr->data.data_len >= 1)
    {
      drdr->data.data_val[drdr->data.data_len-1] = '\0';
      printf("received string is: \"%s\"\n", drdr->data.data_val);

      strcpy(readstring,drdr->data.data_val);
      /*      printf("received string is: \"%s\"\n", readstring); */
    }

  return 1;
}

int splitstring(char *string, char split[MAXSPLIT][MAXLEN], char fs)
{
/**************************************************
 * split string into fields field seperator is fs *
 **************************************************/
  int i, len, nsplit, nchar;
  char c;

  nsplit = 0;
  nchar = 0;

  len = strlen(string);

  for (i=0; i <= len; i++) {
    c = string[i];
    if ( c == fs ) {        //new field
      split[nsplit][nchar]='\0';
      nchar = 0;
      nsplit ++;
    } else {
      split[nsplit][nchar] = c;
      nchar ++;
    }
    
  }

  return nsplit;
}
