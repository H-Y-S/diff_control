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

#define DEVICEIP        "129.129.11.172"
#define DEVICENAME      "hpib,9"

#define MAXLEN   300
#define MAXSPLIT 10


extern Device_Error		*derr;
extern Create_LinkParms	crlp;
extern Create_LinkResp		*crlr;
extern Device_WriteParms	dwrp;
extern Device_WriteResp	*dwrr;
extern Device_ReadParms	drdp;
extern Device_ReadResp		*drdr;

extern struct timeval timeout;

extern CLIENT *cl;
extern int  maxRecvSize;

#ifdef DEBUG
FILE *dbgofp;   // global output file
int dbglvl;     // debugging control level
#endif


/**************************************************************** 
 * function declarations
 ****************************************************************/

int splitstring(char *string, char split[][], char fs);
int checkstring(char string[MAXLEN]);


/*****************************************************************/

int main(int argc, char **argv)
{

  /*	struct hostent	    *myHostAddr;
	struct sockaddr_in  mySockAddr;
  */
  char *svName;
  char command[MAXLEN], sendstring[MAXLEN], readstring[MAXLEN];
  char fname[MAXLEN], fmode[MAXLEN];
  FILE *fp;
  int j, nsplit;
//  double voltages[7];

  char split[MAXSPLIT][MAXLEN];
 
#ifdef DEBUG
// Start debugger
	debug_init("debug.out", NULL);
	dbglvl=9;               // full details
#endif

  if(argc < 2)
    {
      printf("possible commands are:\n");
      printf("print string            this prints the results \n");
      printf("save string fname fmode this saves the result in file fname with mode fmode\n");
      
      return 1;
    }

  rpcopen("hpib,9");

  /* command is in argv[1] arguments in the other argv's */
  strcpy(command,argv[1]);

  if ( (strcmp(command,"print")!=0) && (strcmp(command,"save")!=0) ){
    printf("command not falid !\n");
    return -1;
  } 

  strcpy(sendstring,argv[2]);

  if ( strcmp(command,"save") == 0 ) { 
    if(argc < 4) {
      printf("please enter filename and mode !\n");
      return -1;	    
    }
    strcpy(fname,argv[3]);
    strcpy(fmode,argv[4]);

    if ( (strcmp(fmode,"a")!=0) && (strcmp(fmode,"w")!=0) ) {
      printf("mode must be a (append data) or w (open new file) !\n");
      return -1;	    
    }
  }

  /* send  string and extract answer */

  checkstring(sendstring);
  printf("sending : %s\n",sendstring);

  gpibsend(sendstring);
  gpibread(readstring);

  printf("string read :%s\n",readstring); 

//sscanf(readstring, "%lf,%lf,%lf,%lf,%lf,%lf,%lf", voltages+0,voltages+1,voltages+2,voltages+3,voltages+4,voltages+5,voltages+6);
//for(j=0;j<7;j++)
//	printf("%.4g\n", voltages[j]);

  nsplit = splitstring(readstring, split, ',');
  if (nsplit == 0 ) {
    printf("no splitting done !!\n");
  } else {

   for (j=0;j<=nsplit;j++) {
     printf(" %s ",split[j]);
   }
   printf("\n\n"); 
  }
  

  printf("command %s\n",command);

  printf("label1\n");

  /* save string to a file */
  if (strcmp(command,"save") == 0) {
    printf("label3\n");

    fp = fopen(fname,fmode);
    if (fp == NULL) {
      printf("error opening file !!\n");
    }
    for (j=0;j<=nsplit;j++) {
      fprintf(fp," %s ",split[j]);
    }
    fprintf(fp,"\n");
    fclose(fp);
  }

  printf("label2\n");

  svName = DEVICEIP;
  printf("destroying link...\n");
//  derr = destroy_link_1(&(crlr->lid), cl);
  derr = rpcclose();
  if (derr == NULL)
    {
      clnt_perror(cl, svName);
      return 1;
    };

  /* show results */
  printf("destroy_link result = %d (%s)\n", (int)derr->error, vxiError(derr->error));

  /* ready */
  return 0;
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
      nsplit++;
    } else {
      split[nsplit][nchar] = c;
      nchar++;
    }
    
  }

  return nsplit;
}

int checkstring(char sendstring[MAXLEN])
{
/**************************************************
 * remove " at beginning and end string           *
 **************************************************/
  int i, len;
  char c;
  char temp[MAXLEN];

  len = strlen(sendstring);
  c = sendstring[0];

  if ( c =='"' ) {               /* remove " */
    for (i=1; i < len-1; i++) {
      temp[i-1] =  sendstring[i];
    }
    temp[len-2] = '\0';

    strcpy(sendstring,temp);
  }


  return 1;
}





