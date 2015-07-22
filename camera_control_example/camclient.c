// camclient.c -- the tvx client routine to connect to the camera server

#define CAMERA_MAIN			// turn on allocations in camserver.h

#include <stdio.h>                /* perror() */
#include <stdlib.h>               /* atoi() */
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>               /* read() */
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/poll.h>
#include <string.h>
#include "darwin.h"		// __compar_fn_t override for MAC OS X
#include "tvxsys.h"
#include "screen.h"
#include "tvxobject.h"
#include "camserver.h"
#include "debug.h"

#define POLL_ms 1

// functions in this module
//int camclient_initialize(char *, int);
//int read_camera_string(char*, int);
//int write_camera_string(char *);
//void close_camera_connection(void);

static int clientSocket;
static struct pollfd ufds;

// connect to the server
int camclient_initialize(char *remoteHost, int remotePort)
{
	int status = 0;
	struct hostent *hostPtr = NULL;
	struct sockaddr_in serverName = { 0 };

	DBG(3, "(camclient.c) Initializing the camera client\n");
	DBG(3, "remoteHost = %s, remotePort = %d\n", remoteHost, remotePort);
	clientSocket = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (-1 == clientSocket)
	{
		perror("socket()");
		return -1;
	}

	/*
	* need to resolve the remote server name or IP address
	*/
	hostPtr = gethostbyname(remoteHost);
	if (NULL == hostPtr)
	{
		hostPtr = gethostbyaddr(remoteHost, strlen(remoteHost), AF_INET);
		if (NULL == hostPtr)
		{
			DBG(3, "Error resolving server address %s\n", remoteHost);
			perror("Error resolving server address ");
			return 1;
		}
	}

	serverName.sin_family = AF_INET;
	serverName.sin_port = htons(remotePort);
	(void) memcpy(&serverName.sin_addr, hostPtr->h_addr, hostPtr->h_length);

	// this blocks - can hang seemingly forever - eventually times out
	DBG(3, "(camclient.c) Connecting to camera server %s:%d\n", 
			remoteHost, remotePort);
	status = connect(clientSocket, (struct sockaddr*) &serverName,
			sizeof(serverName));
	if (-1 == status)
	{
		DBG(3, "(camclient.c) - connection failed\n");
#ifndef _TVX_
		perror("connect()");
#endif
		return -1;
	}

	DBG(3, "(camclient.c) Successful connection to the camera\n");
	ufds.fd = clientSocket;		// fill in structure for polling
	ufds.events = POLLIN;

	return 0;
}


// get a string from the server
int read_camera_string(char *buffer, int size)	// non-blocking read
{	
int lng, n;

if ( (n=poll(&ufds, 1, POLL_ms)) > 0)
	{
	if (ufds.revents & POLLHUP || ufds.revents & POLLERR)
		{
		write_line("Camera connection has closed\n");
		WaitingForCamera = False;
		camera_is_initialized = False;
		return -1;		// broken connection
		}
	if (ufds.revents & POLLIN)
		{
		lng = read(clientSocket, buffer, size);
		if (lng > 0)
			{
			buffer[lng] = '\0';
			DBG(6, "%s RX camera_string: %s\n", timestamp(), buffer);
			return lng;
			}
		else if (lng < 0)
			{
			DBG(5, "(camclient.c) read returned an error in read_camera_string\n");
//			printf("***read returned an error in read_camera_string\n");
			return 0;
			}
		/* a zero-length string suggests a dead socket - test it
		** this is a work-around for the fact that kernel 2.4.9 does not
		** return POLLHUP on reading
		*/
		write(clientSocket, "", 1);
		if ( (poll(&ufds, 1, POLL_ms)) > 0)
			{
			if (ufds.revents & POLLHUP || ufds.revents & POLLERR)
				{
				write_line("Camera connection has closed\n");
				WaitingForCamera = False;
				camera_is_initialized = False;
				return -1;		// broken connection
				}
			}
		return 0;
		}
	}
else if ( n<0 )
	{
	DBG(5, "(camclient.c) poll returned an error in read_camera_string\n");
//	printf("***poll returned an error in read_camera_string\n");
	}

return 0;
}


// put a string to the server
// do not set 'WaitingForCamera' here since not all commands have a response
int write_camera_string(char *buffer)
{
	write(clientSocket, buffer, strlen(buffer)+1);
	DBG(6,"%s TX camera string: %s\n", timestamp(), buffer);
	return 0;
}


// close connection
void close_camera_connection(void)
{
	DBG(6, "(camclient.c) Closing camera connection\n");
	close(clientSocket);
	WaitingForCamera = False;
	camera_is_initialized = False;
	return;
}


// if there is a message from the camera, interpret it, and activate callbacks
void check_camera_response(void)
{
CAMERA_CMND cmnd;
char *p, *q;
int status;

if (!camera_is_initialized)
	return;

//status = read_camera_string(camMsgBuf, MSG_BUF_LENGTH);
//DBG(1,"Camclient status = %d\n", status);
//if (status==0)return;

if ((status = read_camera_string(camMsgBuf, MSG_BUF_LENGTH)) == 0)	// get a new message
	return;		// nothing to report
if (status < 0)	// error, printed above
	return;

// signal the system that camera has responded
WaitingForCamera = False;

p = camMsgBuf;
if (*p == '\0')
	return;
while (*p && *p==' ')		// skip possible white space
	p++;
DBG(6, "(camclient.c) msg: %s\n", p);
while ( (q = strchr(p, '\x18')) )		// ^X ends each message
	{
	*q = '\0';
	if (*(q-1) == '\n')
		*(q-1) = '\0';
	cmnd = (CAMERA_CMND)atoi(p);
	p = strchr(p, ' ');		// point to first space
	if (p)
		p++;
	if (p && strstr(p, "OK") != NULL)
		{
		status = CAM_OK;
		p += 3;
		}
	else if (p && strstr(p, "ERR") != 0)
		{
		status = CAM_ERR;
		p += 4;
		}
	else
		{
		write_line ("Error interpreting camera message\n");
		status = False;
		camMsgBuf[0] = '\0';
		return;
		}

	// send callbacks to processes for individual commands
	// Not every possible command has an entry, since they won't occur in
	// this direction - for example, ShutterEnable doesn't generate a callback.
	// Its status would be reported by telemetry.

	// Keep the cases parallel to the ones in camserver.c, as needed.  Note that
	// the strange typography is to prevent collisions with tvx.c
	// the cases are defined in camera.h, and are generated automatically

/*****************************************************************************\
**                                                                           **
**    NOTE!   NOTE!   NOTE!   NOTE!   NOTE!   NOTE!   NOTE!   NOTE1          **
**                                                                           **
**    If development on camserver becomes separated from work on tvx, as say **
**  on another machine, the switch/case enums could get out of synchroniz-   **
**  ation, with the consequence that messages will be misinterpreted.        **
**                                                                           **
**   The compiler cannot prevent this, but careful procedure can.            **
**                                                                           **
**     ADD NEW CASES **ONLY** TO THE BOTTOM OF THE LIST (before default)!    **
**                                                                           **
**   With this precaution, all the old cases will be interpreted correctly,  **
**  and the only problem will be that newer cases are not recognized.        **
**                                                                           **
**                                                                           **
\*****************************************************************************/
	switch(cmnd)
		{
		case CamCmd:
			WaitingForCamera = False;
			if (status == CAM_ERR)
				print_camera_message(p);
			else if (strlen(p) > 1)
				{
				if ( *(p+strlen(p)-1) == '\n')
					*(p+strlen(p)-1) = '\0';
				write_line("%s\n", p);	// data from camera for tvx screen
				}
			break;

		case CamSetup:
			print_camera_message(p);
			break;

		case CamWait:
			WaitingForCamera = False;
			if (status == CAM_ERR)
				print_camera_message(p);
			break;

		case Df:
			q = camMsgBuf + MSG_BUF_LENGTH/2;	// borrow a buffer
			sprintf(q, "1K-blocks available: %s\n", p);
			print_camera_message(q);
			break;

		case ExpEnd:
		case Exposure:
			exposureCB(status, p);	// process the end of an exposure
			break;

		case ExpTime:
			if (status)
				print_camera_message(p);		// error messaage
			break;

		case ImgPath:	// read or set the cmaserver image path
			print_camera_message(p);
			break;

		case LdCmndFile:		// load a file of camera commands
			if (status)
				print_camera_message(p);		// error messaage
			break;

		case Read_setup:
			if (status)
				print_camera_message(p);		// error messaage
			break;

		case K:
		case ResetCam:
			if (status)
				print_camera_message(p);		// error messaage
			break;

		case Send:
			sendCB(status, p);					// the generic response
			break;

		case ShutterEnable:
			if (status)
				print_camera_message(p);		// error messaage
			break;

		case TelemetrY:
			telemetryCB(status, p);
			break;

		case ExiT:
		case QuiT:
			close_camera_connection();
			camera_is_initialized = False;
			break;

		case MenU:
			break;

		case THread:
			print_camera_message(p);		// error messaage
			if (status == CAM_ERR)
				tvx_buffer[0] = '\0';
			else
				strcpy(tvx_buffer,p);
			break;

		default:
			write_line("Unrecognized camera message: %s\n", p);
			sys_error=1;
			break;
		}
	p = q+1;		// next message
	}
camMsgBuf[0] = '\0';	// zero out the buffer
return;
}



/*
**  print_camera_message -- put multiline message on screen
*/
void print_camera_message(char *buf)
{
int l;
char *p = buf, *q;

l = strlen(buf);
if (l==0)			// don't print 0 length messages
	return;
reset_cursor();

// remove '\r', which occurs when working with CBF
while ( (q=strchr(buf, '\r')) )
	*q=' ';

// make sure there are no really long lines in multiline message
if ( (q = strchr(buf, '\n')) )
	*q = '\0';
write_line("Camera message:\n %s\n", buf);
if (strstr(buf, "***"))
	sys_error=1;
if (!q)
	return;
p = q+1;
while ((p-buf < l) && q)
	{
	if ( (q = strchr(p, '\n')) )
		*q = '\0';
	write_line(" %s\n", p);
	p = q+1;
	}

return;
}


/*
**  camMenuPrint - print the camera menu as known to camclient (this program)
**  The menu known to the server could differ, if it is on a different machine
*/
static int camMenuPrint_helper(char **s, char **t)
{
return (strcasecmp(*s, *t));	/* compare names */
}

void camMenuPrint()
{
char *list[500];
char line[250];
int i, j, n, col, ncols, lines=0;

ncols = -1+display_width();
	// construct a list of pointers to the names
n = camera_count;
for (i=0; i<n; i++)
	list[i] = camera_list[i].name;

	// sort the pointers into alphabetical order
qsort (list, (size_t)n, sizeof(char *), (__compar_fn_t)camMenuPrint_helper);	/* sort the list */

	// print the names with 12 column alignment
for (i = 0; i < ncols; i++)
	line[i] = ' ';		/* blank the output line */
col = 5;					/* start printing in col 5 */
for (j = 0; j < n; j++)
	{if (col > 70 || col + strlen( list[j] ) >= ncols)
		{line[col] = '\0';
		write_line(line);		/* line is full, so print */
		write_line("\n");
		check_lines(&lines);
		for (i = 0; i < ncols; i++)
			line[i] = ' ';
		col = 5;
		}
	for (i = 0; *(list[j]+i);  )		/* transfer name */
		line[col++] = *(list[j]+i++);
	col = 5+12*(1+(++col-6)/12);		/* one blank & tab over */
	}
line[col] = '\0';
write_line(line);		/* print partial line */
write_line("\n");

return;
}
