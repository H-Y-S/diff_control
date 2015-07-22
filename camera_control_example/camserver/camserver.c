// camserver.c -- a tvx camera server

/*
**
** GPL license declared below.
**
** Copyright (C) 2001-2010 Eric F. Eikenberry
** Paul Scherrer Institute, CH-5232 Villigen-PSI, Switzerland
** DECTRIS, AG, CH-5200 Baden, , Switzerland
** All rights reserved.
**
**   This program is free software; you can redistribute it and/or modify
**   it under the terms of the GNU General Public License as published by
**   the Free Software Foundation; either version 2 of the License, or
**   (at your option) any later version.
**
**   This program is distributed in the hope that it will be useful,
**   but WITHOUT ANY WARRANTY; without even the implied warranty of
**   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
**   GNU General Public License for more details.
**
**   You should have received a copy of the GNU General Public License
**   along with this program; if not, write to the Free Software
**   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307, USA.
**
** EF Eikenberry, Jan. 2006
*/

// EFE - socket connections based on code by Ivan Griffin, Linux J., Feb. 1998.

/*
A tvx camera runs as a free-standing server process somewhere on the
internet.  It responds to a set of text commands, e.g. "telemetry"
sent to it through a socket connection, and returns status information
as text through the socket conection.  Images are not transmitted
through this connection, but are written directly as files to a 
specified location, e.g. an nfs mount.

This server processes only text messages, though of unlimited length.
Therefore, each message is assumed to end in '\0'.  Concatenated
messages on receipt are handled.

Commands are executed in the order they are received.  Exposures are 
executed as a background process, so other commands may be executed during
an exposure.  A second exposure during a previous exposure will block the
server until the first is complete.
*/

#define CAMERA_PORT 41234	// overwritten by configuration file                   
#define CAMERA_MAIN			// turn on space allocations in camserver.h

#include <stdio.h>
#include <stdlib.h>			/* exit() */
#include <string.h>			/* memset(), memcpy() */
#include <sys/utsname.h>	/* uname() */
#include <sys/types.h>		/* kill() */
#include <sys/socket.h>		/* socket(), bind(), listen(), accept() */
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>			/* fork(), write(), close() */
#include <signal.h>			/* kill */
#include <sys/poll.h>		/* poll */
#include <ctype.h>
#include <ncurses.h>
#include <sys/wait.h>
#include <sys/vfs.h>		// statfs
#include <sys/types.h>		// open
#include <sys/stat.h>		// open
#include <fcntl.h>			// open
#include <errno.h>
#include <sys/time.h>
#include <time.h>
#include "cam_config.h"
#include "debug.h"

#include "tvxcampkg.h"		// camera-specific header collection

// #define's in camera-specific headers above may be needed here
#include "camserver.h"

#undef CAMERA_MAIN

#include "RELEASE.inc"	// code release date - allocates space

/*
** internal prototypes
*/
static int _GetHostName(char *buffer, int length);
static int rxMsg(int, char *);
static void txMsg(int, char *);
static int command_interpreter(char *, int);
static int command_dispatcher(CAMERA_CMND, char *, int);
static void exposure_service(void);
static void end_exposure(CAMERA_CMND, int);
static void OK_response(int, CAMERA_CMND, char *);
static void ERR_response(int, CAMERA_CMND, char *);
static void menu_print(void);
static int menuPrint_helper(char **, char **);
static __sighandler_t restoreControl(int);
static int check_permission(CAMERA_CMND);

/*
** constants
*/
#define POLL_ms 1		// polling time-out, ms
#define True 1
#define False 0
#define LENMSG 4096		// must be able to accomodate telemetry & status
#define ncols 79		// screen width-1
#define N_MAX_PIPE -1+4096/LENFN	// max N pathnames in pipe

const char MESSAGE[] = "Connected to tvx prototype camera server!\n";
const int BACK_LOG = 5;

/*
** system globals
*/
char txBuffer[LENMSG];
double exposure_time = 1.0;
int shutter_control = 0;
int timerRunning = False;
double endTime;
double lastTime;
double startTime;
int masterProcess = True;
int controllingProcess = True;
int processReentered = False;
int controllingPID;
int camPort;
int txImage;
int nPipeMsgs=0;
char camstat_path[LENFN] = "./demo_cam/cam_data/";
char cam_data_path[LENFN] = "./demo_cam/cam_data/";
char cam_image_path[LENFN] = "/tmp/";
char cam_initial_path[LENFN] = "./";
char previous_data_path[LENFN] = "/tmp/";
char cam_startup_file[LENFN] = "\0";
char header_string[LENMSG] = "\0";	// string to include in image header
CAM_STATE Cam_State = CamIdle;
char image_file[LENFN] = "\0";		// filled by 'exposure' command
// pipes for communication with readline()
int txpipe[2];		// command pipe - 0 is for reading, 1 is for writing
int rxpipe[2];		// data pipe - 0 is for reading, 1 is for writing
struct pollfd ufds[2] = { {0} , {0} };	// sockets and pipes for this process


#ifdef DEBUG
FILE *dbgofp;   // global output file
int dbglvl;     // debugging control level
#endif


static int command_dispatcher(CAMERA_CMND cmnd, char *ptr, int tx)
{
int i, err=0;
char line[2*LENFN], bufr[LENMSG], *p, *q, *qq;
struct timeval tv;
double t;
FILE *fp;
struct statfs statfsbuf;
struct stat statbuf;

#include "tvxcampkg_decl.h"		// camera-specific variable declarations

DBG(7, "User argument: %s\n", ptr)

		// execute the operator's request
		// ptr gives the argument list (coerced to upper case)

// In the case statements, one letter must differ in case from the variable name
// The strange orthography is to avoid collisions with tvx.c
// camera.h is generated from this list.  Changes made here may need to
// be reflected in camclient.c as well

/*****************************************************************************\
**                                                                           **
**      NOTE!   NOTE!   NOTE!   NOTE!   NOTE!   NOTE!   NOTE!   NOTE1        **
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
**   Alternatively, one can fix the case definitions (a C enum) by           **
**  assigning values.  See the README file, or the file                      **
**  demo_cam/include/camserver_casedefs.txt for the method.  This prevents   **
**  misinterpretation of old cases, but new cases will, of course, not be    **
**  recognized until the headers are updated.                                **
**                                                                           **
**   Problems also arise when other clients, such as SPEC, are used to       **
**  interface to camserver, since those clients will generally not use       **
**  the camserver.h header file for symbolic definitions, but rather         **
**  hard-code the integer equivalents of the "tydef enum".                   **
**                                                                           **
**  Please refer to the file "camserver_casedefs.txt" for hard-coding        **
**  the case values.                                                         **
**                                                                           **
\*****************************************************************************/

/*****************************************************************************\
**                                                                           **
**    The following section defines general commands that, in principal,     **
**    are common to any detector.  Each command that has a detector-         **
**    specific realization, such as expose, calls out to detector-specific   **
**    functions for implementation.  These functions are in detector-        **
**    specific directories that are selectively enabled in Makefile.         **
**                                                                           **
\*****************************************************************************/

// camera-specific routine may modify cmnd
cmnd = camera_cmnd_filter(cmnd, ptr, tx);

	/* start of main loop switch - please preserve the format for mkheader */
switch (cmnd)
	{
	case CamCmd:		// client entry to camserver commands - see tvx usage
		DBG(4, "CamCmd line: %s; tx = %d\n", ptr, tx)
		if (tx)								// echo outside command
			printf(" %s\n", ptr);
		if ((err=command_interpreter(ptr, tx)))	// reentrant!
			return err;
		OK_response(tx, CamCmd, "");		// tvx waits for response
		break;

	case CamSetup:			// controllingProcess not required
		print_cam_setup(bufr);
		OK_response(tx, CamSetup, bufr);
		q = bufr;
		qq = strchr(q, '\n');
		while (qq)
			{
			*qq = '\0';
			printf("%s\n", q);
			q=qq+1;
			qq = strchr(q, '\n');
			}					
		break;

	case CamWait:		// wait for an exposure to end, or program a wait state
		if ( check_permission(CamWait) )
			break;
		while (timerRunning)
			{				// wait for exposure in progress
			exposure_service();
			}
		exposure_time = 0;
		sscanf(ptr, "%lf", &exposure_time);
	//	printf("Starting %.6lf second wait\n", exposure_time);
		gettimeofday(&tv, 0);
		startTime = lastTime = (double)tv.tv_sec + (double)tv.tv_usec/1e6;
		endTime = lastTime + exposure_time;
		sprintf(line, "%.6lf", exposure_time);
		set_cam_stat(cam_time, line);
		timerRunning = True;
		set_cam_stat(cam_state, "waiting");
		Cam_State = CamWaiting;
		OK_response(tx, Send, "");		// generic response
		break;

	case DataPath:		// set or show cam_data_path
		if (*ptr == '\0')
			{
			sprintf(line, "cam_data_path:\n  %s", cam_data_path);
			printf("%s\n", line);
			OK_response(tx, Send, line);		// generic response
			break;
			}
		if ( check_permission(DataPath) )
			break;
		for (i=strlen(ptr)-1; i && isspace(*(ptr+i)); i--)
			*(ptr+i) = '\0';			// trim trailing blanks, if any
		if (*(ptr+i) != '/')
			strcat(ptr, "/");			// trailing slash, if not there
		if (*ptr == '~')
			{
			strcpy(cam_data_path, getenv("HOME"));
			strcat(cam_data_path, ptr+1);
			}
		else
			strcpy(cam_data_path, ptr);
		printf("Changing cam_data_path to:\n   %s\n", cam_data_path);
		OK_response(tx, Send, cam_data_path);		// generic response
		break;

	case Df:			// report the space available in the current path
		statfs(cam_image_path, &statfsbuf);	// guaranteed to work
		sprintf(line, "%lu", 
					(unsigned long)statfsbuf.f_bavail*(statfsbuf.f_bsize>>10));
		OK_response(tx, Df, line);
		printf("1K-blocks available: %s\n", line);
		break;

	case ExpEnd:		// end an exposure in response to external signal
		if ( check_permission(ExpEnd) )
			break;
		end_exposure(ExpEnd, 1);
		OK_response(tx, ExpEnd, image_file);		// send to client
		break;

	case Exposure:		// make an exposure
		if ( check_permission(Exposure) )
			break;
		while (timerRunning)
			{				// wait for exposure in progress
			exposure_service();
			}
		if (exposure_time <= 0 )
			{
			sprintf(line, "exposure time = %.6lf", exposure_time);
			printf("Error - %s\n", line);
			ERR_response(tx, Exposure, line);
			break;
			}
		if (*ptr == '\0')
			{
			printf("No image file specified\n");
			break;
			}
		if (*ptr == '/')							// full path given
			image_file[0] = '\0';
		else
			strcpy(image_file, cam_image_path);
		if (2+strlen(ptr)+strlen(image_file) >= LENFN)
			{
			sprintf(line, " *** Full path is too long ***");
			printf(" %s\n", line);
			ERR_response(tx, Send, line);		// send to client
			break;
			}
		strcat(image_file, ptr);
		printf(" Preparing camera for exposure: %s\n", 1+strrchr(image_file, '/'));
		set_cam_stat(cam_target, image_file);
		set_cam_stat(cam_state, "preparing");
		Cam_State = CamPreparing;
		if ( camera_prepare(&exposure_time) )
			{
			set_cam_stat(cam_state, "idle");
			Cam_State = CamIdle;
			ERR_response(tx, Exposure, image_file);
			break;
			}
		// in the case of hardware timing (highly recommended), startTime
		// and endTime may be modified in interface.c
		gettimeofday(&tv, 0);
		startTime = lastTime = (double)tv.tv_sec + (double)tv.tv_usec/1e6;
		endTime = lastTime + exposure_time;
		sprintf(line, "%.6lf", exposure_time);
		set_cam_stat(cam_time, line);
		timerRunning = True;
		if (shutter_control)
			{
			printf("  Opening shutter\n");
			shutter_on_off(1);
			set_cam_stat(cam_shutter, "open");
			sprintf(line ," Starting %.6lf second exposure: %s", exposure_time, timestamp());
			printf(" %s\n", line);
			OK_response(tx, Send, line);		// send to client
			}
		else
			{
			sprintf(line, " Starting %.6lf second background: %s", exposure_time, timestamp());
			shutter_on_off(0);					// not necessary
			printf(" %s\n", line);
			OK_response(tx, Send, line);		// send to client
			}
		set_cam_stat(cam_state, "exposing");
		txImage = tx;							// flag for end_exposure()
		Cam_State = CamExposing;
		if ( camera_start(&exposure_time) )
			{
			camera_stop(2);
			shutter_on_off(0);
			set_cam_stat(cam_shutter, "closed");
			set_cam_stat(cam_state, "idle");
			Cam_State = CamIdle;
			timerRunning = False;
			ERR_response(tx, Exposure, image_file);
			}
		break;

	case ExpTime:		// set the exposure time
		if (*ptr == '\0')
			{
			sprintf(line, "Exposure time set to: %.6lf sec.", exposure_time );
			printf(" %s\n", line);
			OK_response(tx, Send, line);
			break;
			}
		if ( check_permission(ExpTime) )
			break;
		sscanf(ptr, "%lf", &t);
		if (t >= 1.0e-7)
			exposure_time = t;
		else
			{
			sprintf(line, "Illegal exposure time\n");
			printf("  %s\n", line);
			ERR_response(tx, Send, line);
			break;
			}
		sprintf(line, "%.6lf", exposure_time);
		set_cam_stat(cam_exptime, line);
		camera_record_variable(Cam_exposure_time, line);
		sprintf(line, "Exposure time set to: %.6lf sec.", exposure_time );
	//	if (tx)
			printf("  %s\n", line);
		OK_response(tx, Send, line);
		break;

	case HeaderString:		// a line to include in image file header
		if ( check_permission(HeaderString) )
			break;
		strncpy(header_string, ptr, 69);
		header_string[69]='\0';
		if ( (p=strchr(header_string, '\n')) )
			*p='\0';
		if (tx)
			printf ("HeaderString: %s", header_string);
		OK_response(tx, Send, "");		// generic response
		break;

	case ImgPath:	// read or set the camserver image path
		if (*ptr == '\0')		// just read it
			{
			printf(" Image path:\n  %s\n", cam_image_path);
			OK_response(tx, ImgPath, cam_image_path);
			break;
			}
		if ( check_permission(ImgPath) )	// request to change path
			break;

			// resolve the new path from the typed text
		for (err=i=0; i<strlen(ptr); i++)
			if ( isspace(*(ptr+i)) || iscntrl(*(ptr+i)) || 
					(ispunct(*(ptr+i)) && *(ptr+i)!='_' &&
					 *(ptr+i)!='-' && *(ptr+i)!='.' && *(ptr+i)!='/') )
				{
				printf("*** Error: Illegal Path ***\n");
				ERR_response(tx, ImgPath, "*** Illegal Path");
				goto imgpath_break;
				}
		if (*ptr == '~')
			{
			strcpy(line, getenv("HOME"));
			strcat(line, ptr+1);
			}
		else if (*ptr == '/')
			strcpy(line, ptr);
		else if (*ptr == '.')
			{
			strcpy(line, cam_image_path);
			if ( (strstr(ptr, "../")) )
				{
				q=NULL;
				qq=ptr;
				while ( (strstr(qq, "../")) )
					{
					*(line-1+strlen(line)) = '\0';	// remove trailing '/'
					if ( (q=strrchr(line, '/')) )
						*(q+1) = '\0';				// remove 1 dir
					else
						break;
					qq += strlen("../");
					}
				if (!q)							// dir not found
					break;
				strcat(line, qq);
				}
			else
				strcat(line, ptr);				// assume "./" or ".."
			}
		else
			{
			strcpy(line, cam_image_path);
			*(line-1+strlen(line)) = '\0';		// remove trailing '/'
			if ( (q=strstr(line, ptr)) && !strcmp(q, ptr) )
				{
				strcpy(line, cam_image_path);
				goto imgpath_fin;				// path is same as current
				}

			q = line;
			while ( (q = strchr(q, '/')) )
				{
				q++;
				if ( strstr(ptr, q) && (*(ptr+strlen(q))=='\0' || *(ptr+strlen(q))=='/') )
					{
					ptr += strlen(q);			// eliminate common string
					break;
					}
				}
			strcat(line, "/");
			strcat(line, ptr);
			}
		if (*(q = (line-1+strlen(line))) != '/')
			strcat(q, "/");						// ensure trailing '/'

		// line[] contains the desired full path with trailing '/'
		if (strlen(line)>=LENFN)
			{
			printf("*** Error: Path too long ***\n");
			ERR_response(tx, ImgPath, "*** Path too long");
			break;
			}
		qq = line;
		q = strchr(line+1, '/');				// parse path
		while (q && q-line<strlen(line))
			{
			*q = '\0';							// temporary terminator
			if ( stat(line,  &statbuf) )		// path not found
				{
				*q = '/';						// put back '/'
				break;
				}
			else								// found - go on
				{
				*q = '/';						// put back '/'
				qq = q;							// possible new dir
				q++;
				q = strchr(q, '/');				// find next '/'
				}
			}
		q = 1+qq;								// 1st char of new part
		while (*q != '\0')
			{
			q = strchr(q, '/');					// 1st new dir
			*q = '\0';
			if (mkdir(line, 0755))
				{
				perror("mkdir()");
				ERR_response(tx, ImgPath, "mkdir() failed");
				break;
				}
			*q = '/';							// put back '/'
			q++;								// next dir
			}
		if ( stat(line, &statbuf) )				// failed
			break;

		if (chdir(line))
			{
			perror("camserver chdir()");
			printf("%s\n", line);
			ERR_response(tx, ImgPath, "chdir() failed");
			break;
			}
		getcwd(line, sizeof(line));
			// camserver requires write permission
		strcat(line, "/write_test.tst");
		if ( (fp = fopen(line, "w"))==NULL )
			{
			ERR_response(tx, ImgPath, "*** Write permission denied");
			printf("*** Write permission denied\n");
			chdir(previous_data_path);
			break;
			}
		else									// success
			{
			fclose(fp);
			unlink(line);						// delete test
			}
		getcwd(line, sizeof(line));
		strcat(line, "/");						// store with trailing '/'
			// Send the path to the getline process.  If we are setting
			// the path from a client, getline will not see it until after
			// the next 'enter' in the camserver window, although the change
			// is active immediately for, e.g., exposure.
			// But, if there is no keyboard activity, this pipe can fill up
			// (4kB) with excessive, repetitive ImgPath commands from the
			// client; so, do not send duplicates.
			// This proved to be not good enough.  So, count the messages,
			// and limit to ~24.  As soon as there is keyboard activity, then 
			// the most recent path is sent.
		if (nPipeMsgs<N_MAX_PIPE && strcmp(line, previous_data_path))
			{
			if ( write(txpipe[1], line, 1+strlen(line)) < 0)
				{
				perror("chdir()");
				break;
				}
			nPipeMsgs++;
			strcpy(previous_data_path, line);
			}

			// set the new path
		strcpy(cam_image_path, line);

			// Record the path in camstat/variables for any new process
			// that may be created.  But, any non-controlling process
			// that already exists will not see this change.

imgpath_fin:
		printf(" Image path:\n  %s\n", cam_image_path);
		OK_response(tx, ImgPath, cam_image_path);
imgpath_break:
		break;

	case LdCmndFile:		// load a file of camera commands
		if ( check_permission(LdCmndFile) )
			break;
		if ( (q=strchr(ptr, ' ')) )		// remove trailing blanks
			*q = '\0';
		if (*ptr == '\0')
			{
			printf("Usage: LdCommandFile <filename>\n");
			break;
			}
		line[0] = '\0';
		if ( strchr(ptr, '/') == NULL)		// if path not given
			strcpy(line, cam_data_path);
		strcat(line, ptr);		// make up file name
		if ( (fp = fopen(line, "r")) == NULL )
			{
			printf("Cannot find file:\n  %s\n", line);
			ERR_response(tx, LdCmndFile, "Cannot find file");
			err=9;
			break;
			}
		printf("Loading commands from:\n  %s\n", line);
		while ( (fgets(line, sizeof(line), fp) != NULL) && !err)
			{
			if ( (q = strchr(line, '#')) != NULL)
				*q = '\0';		// knock off comments
			err = command_interpreter(line, False);
			}
		fclose(fp);
		if (err>2)
			ERR_response(tx, LdCmndFile, "");
		else
			OK_response(tx, LdCmndFile, "");		// tvx waits for response
		break;

	case Read_setup:		// read a camera setup from a file
		if ( check_permission(Read_setup) )
			break;
		camera_read_setup(ptr, bufr);
		if (*bufr)
			printf("Error: %s\n", bufr);
		if (*bufr)
			ERR_response(tx, Read_setup, bufr);
		else
			OK_response(tx, Read_setup, bufr);
		break;

	case K:			// "kill"
	case ResetCam:
		if ( check_permission(ResetCam) )
			break;
		if (timerRunning)
			{
			printf("\nCancelling exposure in progress\n");
			if (shutter_control)
				{
				printf(" Closing shutter\n");
				shutter_on_off(0);
				shutter_control = 0;
				set_cam_stat(cam_shutter, "closed");
				}
			camera_stop(2);
			set_cam_stat(cam_state, "idle");
			ERR_response(tx, Exposure, image_file);
			timerRunning = False;
			}
		OK_response(tx, Send, "");		// generic response
		break;

	case Send:			// controllingProcess not required - send message to client
		OK_response(tx, Send, ptr);
		printf("Sent message: %s\n", ptr);
		break;

	case ShowPID:		// show PID of this process
		sprintf(bufr, "PID = %d", (int)getpid());
		printf("%s\n", bufr);
		OK_response(tx, ShowPID, bufr);
		break;

	case ShutterEnable:		// enable/disable shutter control
		if ( check_permission(ShutterEnable) )
			break;
		sscanf(ptr, "%d", &shutter_control);
		OK_response(tx, Send, "");
		break;

	case TelemetrY:			// controllingProcess not required
		camera_read_telemetry(bufr, sizeof(bufr));
		printf("%s\n", bufr);
		set_cam_stat(cam_telemetry, bufr);
		OK_response(tx, TelemetrY, bufr);
		break;

	case ExiT:
	case QuiT:
		if ( check_permission(ExiT) )
			break;
		return -1;		// signal for server process to exit
		break;

	case MenU:			// controllingProcess not required
		menu_print();
		break;

	case Status:
	case CamStatus:
		i=0;			// base status
		i = camera_status(i);
		sprintf (bufr, "0x%x -- camera status", i);
		printf("%s\n", bufr);
		OK_response(tx, TelemetrY, bufr);
		break;


/*****************************************************************************\
**                                                                           **
**    Camera-specific packages of commands may be included here.             **
**                                                                           **
**                                                                           **
\*****************************************************************************/

#include "../tvxcampkg.c"		// installation-specific package


/*****************************************************************************\
**                                                                           **
**    End of camera-specific section.                                        **
**                                                                           **
**                                                                           **
\*****************************************************************************/


#ifdef DEBUG
	case CamNoop:
		printf("Argument is: %s\n", ptr);
		break;

	case DbglvL:
		if (*ptr)
			sscanf(ptr, "%d", &dbglvl);
		else
			printf("dbglvl is %d\n", dbglvl);
		break;
#endif

/*****************************************************************************\
**                                                                           **
**  =====   Add new cases here  =====                                        **
**                                                                           **
\*****************************************************************************/
	default:
		printf("Unrecognized command: %s\n", ptr);
		ERR_response(tx, CamCmd, "Unrecognized command");
		break;
	}

return err;
}


/*****************************************************************************\
**                                                                           **
** command_interpreter - execute commands either typed locally or from client**
** If 'tx' is true, appropriate messages are cc'd to client.                 **
**                                                                           **
\*****************************************************************************/
static int command_interpreter(char *ptr, int tx)
{
int i, j, n, idx=0, qc=0, qce=0;
char bufr[LENMSG], *q, *qq;


	// find operator's command in the list
while( (q = strchr(ptr, '\t')) )	// change tabs to spaces
	*q = ' ';
while ( isspace(*ptr) )		// skip whitespace
	ptr++;
q = ptr-1+strlen(ptr);
while ( isspace(*q) )		// remove trailing whitespace
	*q-- = '\0';
DBG(4, "Command (tx=%d): %s\n", tx, ptr)
if (*ptr == '\0')			// empty line?
	return 0;
for (n=0; *(ptr+n); n++)	// count characters in command
	if (!isalnum(*(ptr+n)) && *(ptr+n)!='_')
		break;
if (n==0)		// illegal character in 1st col?
	{
	printf("Command not found: '%s'\n", ptr);
	return 0;
	}
j = 0;			// count matching names
for (i = 0; i < camera_count; i++)
	if ( strncasecmp(ptr, camera_list[i].name, n) == 0 )
		{
		idx = i;		// permit unambiguous abbreviations
		j++;
		if ( n == strlen(camera_list[i].name) )
			{
			j = 1;		// to skip next block
			break;		// exact match is exempt from ambiguity check
			}
		}
if (j != 1)
	{
	for (i=0; *(ptr+i) && !isspace(*(ptr+i)); i++)
		;
	*(ptr+i) = '\0';		// isolate command
	if (j == 0)
		printf("Command not found: '%s'\n", ptr);
	else
		printf("Command '%s' is ambiguous.\n", ptr);
	strcpy(bufr, "*** Unrecognized command: ");
	strcat(bufr, ptr);
	ERR_response(tx, CamCmd, bufr);
	return 0;
	}

	// prepare the arguments for the command
ptr += n;		// skip over command name
while ( *ptr=='=' || isspace(*ptr) )		// skip whitespace & '='
	ptr++;

// As a service to programmers, we remove quotes from strings.  But there are 
// occasions when quotes must be passed to programs.  These must be 'escaped'.
qq = ptr;
while( (q = strchr(qq, '"')) )
	{
	if (*(q-1) == '\\')			// escaped?
		{
		strcpy(q-1, q);			// move string to left preserving quote
		qce++;
		}
	else
		{
		qc++;
		if (isspace( *(q-1)) )
			strcpy(q, q+1);		// move string to left removing quote
		else
			*q = ' ';
		}
	qq = q;
	}
if (qc%2 || qce%2)
	printf("Unmatched quotes in argument\n");

for (i=strlen(ptr)-1; i && isspace(*(ptr+i)); i--)
	*(ptr+i) = '\0';				// trim trailing blanks, if any
for (i=0; *(ptr+i); i++)			// remove extra blanks
	{
	if (*(ptr+i) == ' ' && *(ptr+i+1) == ' ')
		strcpy(ptr+i, ptr+i+1);			// move left 1 space
	}

return command_dispatcher(camera_list[idx].cmnd, ptr, tx);
}


/*****************************************************************************\
**                                                                           **
**              *****-----  camserver main -----*****                        **
**                                                                           **
\*****************************************************************************/

int main(int argc, char *argv[])
{
	int i, n, serverSocket = 0, on = 0, status = 0;
	pid_t childPid = 0, getlinePid = 0;
	struct hostent *hostPtr = NULL;
	char rxBuffer[LENMSG], hostname[80] = "", cmdBuf[150], pipes[2][10], line[150];
	struct sockaddr_in serverName = { 0 };
	extern int errno;
	struct sigaction processSignal = 
			{{(__sighandler_t)restoreControl}, {{0}}, SA_RESTART, NULL};
	FILE *ifps;

	if (argv[1] && *argv[1]=='-')		// respond to queries & exit
		switch (*(1+argv[1]))
		{
			case 'h':
			case 'H':
				printf("   Usage:  camserver [path_to_resource_file]\n");
				exit(0);
			case 'v':
			case 'V':
				printf("   Code release:  %s\n", release);
				exit(0);
			default:
				exit(0);
		}

	serverSocket = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (-1 == serverSocket)
	{
		perror("socket()");
		fprintf(stderr, "Program exiting in 8 seconds\n");
		sleep(8);
		exit(1);
	}

	/*
	* turn off bind address checking, and allow port numbers
	* to be reused - otherwise the TIME_WAIT phenomenon will
	* prevent binding to these address.port combinations for
	* (2 * MSL) seconds.
	*/

	on = 1;

	status = setsockopt(serverSocket, SOL_SOCKET, SO_REUSEADDR,
			(const char *) &on, sizeof(on));

	if (-1 == status)
	{
		perror("setsockopt(...,SO_REUSEADDR,...)");
		fprintf(stderr, "Program exiting in 8 seconds\n");
		sleep(8);
		exit(1);
	}

	/*
	* when connection is closed, there is a need to linger to ensure
	* all data is transmitted, so turn this on also
	*/
	{
		struct linger linger = { 0 };

		linger.l_onoff = 1;
		linger.l_linger = 30;
		status = setsockopt(serverSocket, SOL_SOCKET, SO_LINGER,
		(const char *) &linger, sizeof(linger));

		if (-1 == status)
		{
			perror("setsockopt(...,SO_LINGER,...)");
			fprintf(stderr, "Program exiting in 8 seconds\n");
			sleep(8);
			exit(1);
		}
	}

	/*
	* find out who I am
	*/

	status = _GetHostName(hostname, sizeof(hostname));
	if (-1 == status)
	{
		perror("_GetHostName()");
		fprintf(stderr, "Program exiting in 8 seconds\n");
		sleep(8);
		exit(1);
	}

	hostPtr = gethostbyname(hostname);
	if (NULL == hostPtr)
	{
		perror("gethostbyname()");
		fprintf(stderr, "Program exiting in 8 seconds\n");
		sleep(8);
		exit(1);
	}

	(void) memset(&serverName, 0, sizeof(serverName));
	(void) memcpy(&serverName.sin_addr, hostPtr->h_addr_list[0],
			hostPtr->h_length);

	/*
	* to allow server be contactable on any of its
	* IP addresses, uncomment the following line of code.
	* Without this line, the 'hosts' file must match the ip 
	* address, a moving target when using dhcp.
	*/
	
	serverName.sin_addr.s_addr = htonl(INADDR_ANY);
	
	

	serverName.sin_family = AF_INET;
	serverName.sin_port = htons(CAMERA_PORT); /* network-order */

	status = bind(serverSocket, (struct sockaddr *) &serverName, 
			sizeof(serverName));
	if (-1 == status)
	{
		perror("bind()");
		i=serverName.sin_addr.s_addr;
		fprintf(stderr, "Cannot bind to %s at %d.%d.%d.%d\n", hostPtr->h_name,
				(unsigned char)(i%256), (unsigned char)((i>>8)%256), 
				(unsigned char)((i>>16)%256), (unsigned char)(i>>24));
		fprintf(stderr, "Check inet configuration, /etc/hosts and README\n");
		fprintf(stderr, "Program exiting in 8 seconds\n");
		sleep(8);
		exit(1);
	}

	status = listen(serverSocket, BACK_LOG);
	if (-1 == status)
	{
		perror("listen()");
		fprintf(stderr, "Program exiting in 5 seconds\n");
		sleep(5);
		exit(1);
	}
	
	status = fcntl(serverSocket, F_SETFL, O_NONBLOCK);
	if (-1 == status)
	{
		perror("fcntl()");
		fprintf(stderr, "Program exiting in 8 seconds\n");
		sleep(8);
		exit(1);
	}
			// arrange to catch signals when children exit
	sigaction (SIGCHLD, &processSignal, NULL);
	controllingPID = getpid();

// ---- do the other initializations ----

getcwd(cam_initial_path, LENFN);
strcat(cam_initial_path, "/");

#ifdef DEBUG
	debug_init("camdbg.out", argv[1]);	// Start the debugger
	dbglvl=8;				// pretty full details to debug file
#endif

DBG(3, "camserver IP address = %d.%d.%d.%d\n", 
	(unsigned char)hostPtr->h_addr_list[0][0], (unsigned char)hostPtr->h_addr_list[0][1],
	(unsigned char)hostPtr->h_addr_list[0][2], (unsigned char)hostPtr->h_addr_list[0][3])

/*  Use this to debug initialization
printf("Press any key to begin (start GDB if needed)\n");
fgets(txBuffer, sizeof(txBuffer),  stdin);
*/

/*****************************************************************************\
**                                                                           **
**    General initialization of camserver uses data from file camrc.         **
**    cam_config_initialize() is in camserver/xxx/util/cam_config.c,         **
**    where xxx is the hardware-specific directory, e.g, slsp2det            **
**                                                                           **
**    However, detector-specific initialization can also take place here.    **
**    If a cam_definition_file is specified in camrc, the detector-          **
**    specific camera_read_setup() will be called.  camera_read_setup()      **
**    is in e.g. cmaserver/xxx/util/interface.c.                             **
**                                                                           **
\*****************************************************************************/

	cam_config_initialize(argv[1]);		// read the configuration resource file

	camPort = CAMERA_PORT;				// default port

/*****************************************************************************\
**                                                                           **
**   A detector-specific initialization may be done here.                    **
**   camera_initialize() is in the hardware-specific interface.c             **
**                                                                           **
\*****************************************************************************/

if ( camera_initialize() )
	{
	printf("Camera initialization error -- press <enter> to exit\n");
	getchar();		// wait so operator can see the error message
	exit (1);
	}

/*****************************************************************************\
**                                                                           **
**   End of detector-specific initializations   .                            **
**                                                                           **
\*****************************************************************************/

	printf ("\n");
	printf ("***  TVX Camera server listening on port %d\n", camPort);
	DBG(1, "***  TVX Camera server listening on port %d\n", camPort)

	printf ("***  Camera configured: %s\n\n", camera_name);
	DBG(1, "***  Camera configured: %s\n\n", camera_name)

/*****************************************************************************\
**                                                                           **
**    cam_startup_file, if specified, contains executable commands           **
**                                                                           **
\*****************************************************************************/

	if ( cam_startup_file[0] )
	{
		if ( ((ifps = fopen(cam_startup_file, "r")) !=NULL) )
		{
			printf("... Reading camera startup file:\n");
			printf("   %s\n", cam_startup_file);
			DBG(1, "Reading camera startup file: %s\n", cam_startup_file)
			while (fgets(cmdBuf, sizeof(cmdBuf), ifps) != NULL)
			{
				printf("%s", cmdBuf);
				if (cmdBuf[0] == '#')		// permit comments
					continue;
				command_interpreter(cmdBuf, False);
			}
			fclose(ifps);
		}
		else
		{
			printf("*** Could not find camera startup file: \n");
			printf("    %s\n", cam_startup_file);
		}
	}

	// set up for reading command lines with getline
	if ( (pipe(rxpipe)) )
	{
		perror("pipe(rx)");
		sleep(3);
		exit (1);
	}
	// set up for sending directory info to getline
	if ( (pipe(txpipe)) )
	{
		perror("pipe(tx)");
		sleep(3);
		exit (1);
	}

	// execute getline as a separate process to read command lines
	switch ( (getlinePid = fork()) )
	{
		case -1: /* ERROR */
			perror("fork()");
			exit(1);

		case 0: /* child process */
			// pipe names are from parent's point of view, so backwards here
			close(txpipe[1]);			// child reads from this pipe
			close(rxpipe[0]);			// child writes to this pipe
			sprintf(pipes[0], "%d", txpipe[0]);
			sprintf(pipes[1], "%d", rxpipe[1]);
			execl("./util/getline", "getline", pipes[0], pipes[1], (char *)NULL);

		default:	/* parent process */
			close(txpipe[0]);			// parent process does not read this pipe
			close(rxpipe[1]);			// parent process does not write this pipe
			// set the rxpipe as non-blocking
			status = fcntl(rxpipe[0], F_SETFL, O_NONBLOCK);
			if (-1 == status)
			{
				perror("fcntl()");
				exit(1);
			}

			// set the base directory for readline filename completion
			// use 'cam_image_path' - the only path that really makes sense
			// first, change our own directory
			if (chdir(cam_image_path))
				perror("chdir()");
			else
				write(txpipe[1], cam_image_path, 1+strlen(cam_image_path));

			printf("*");							// next line prompt
			fflush(stdout);

			for (;;)
			{
				struct sockaddr_in clientName = { 0 };
				int go, cnt, slaveSocket=0;
				socklen_t clientLength = sizeof(clientName);

				(void) memset(&clientName, 0, sizeof(clientName));

				// set up for polling of fd's
				ufds[0].fd = rxpipe[0];				// getline
				ufds[0].events = POLLIN;
				ufds[1].fd = serverSocket;			// server contact
				ufds[1].events = POLLIN;

				go = 1;
				cmdBuf[0]='\0';
				while (go)
				{
					exposure_service();
					if (processReentered)
					{
						printf("*");			// next line prompt
						fflush(stdout);
						processReentered = 0;
					}
					if (poll(ufds, 2, POLL_ms) > 0)		// poll for input events
					{
						if (controllingProcess)
						{
							if (ufds[0].revents & POLLERR || ufds[0].revents & POLLHUP)
							{
								printf("Poll reports broken connection from getline\n");
								DBG(1, "Poll reports broken connection from getline\n")
								sleep(3);
								goto clean_up;		// exit
							}
							if (ufds[0].revents & POLLIN)
									// input from getline (keyboard) when there is no client
							{
								n=read(rxpipe[0], cmdBuf, sizeof(cmdBuf)-1);
								if (n<0 && errno == EAGAIN)
									continue;
								if (n<0)
								{
									perror("1:read()");
									sleep(6);
									goto clean_up;		// exit
								}
								DBG(5, "Parent got(n=%d): %s\n", n, cmdBuf)
								if (-1==command_interpreter(cmdBuf, False))		// interpret kybd input
									goto clean_up;		// exit
								printf("*");			// next line prompt
								fflush(stdout);
							}
						}
						if (ufds[1].revents & POLLERR || ufds[1].revents & POLLHUP)
						{
							printf("Poll reports server socket error\n");
							DBG(1, "Poll reports server socket error\n")
							sleep(3);
							goto clean_up;		// exit
						}
						if (ufds[1].revents & POLLIN)	// input from server socket
						{
							slaveSocket = accept(serverSocket, (struct sockaddr *) &clientName, 
										&clientLength);
							if (-1 == slaveSocket)		// when non-blocking, accept returns err
							{
								if (errno != EWOULDBLOCK)
								{
									perror("accept()");
									DBG(1, "*** accept() error - exiting\n")
									exit(1);
								}
							}
							else
								go = 0;		// process a connection request
						}
					}
				}

				switch ( (childPid = fork()) )
				{
				case -1: /* ERROR */
					perror("fork()");
					exit(1);

				case 0: /* child process */

					close(serverSocket);
					masterProcess = 0;

					if (-1 == getpeername(slaveSocket, (struct sockaddr *) &clientName, 
							&clientLength))
					{
						perror("getpeername()");
					}
					else
					{
						printf("\nConnection request from %s\n",
								inet_ntoa(clientName.sin_addr));
						DBG(1, "Connection request from %s @ %s\n", 
								inet_ntoa(clientName.sin_addr), timestamp())
					}

					camera_update_variables();			// read variables from disk, if any

					if (controllingProcess)
					{
						controllingPID = getpid();
						sprintf(line, "%d", controllingPID);
						set_cam_stat(cam_controlling_pid, line);
						printf("Connection has full control; pid = %s\n", line);
						DBG(5, "Is controlling process: pid = %s\n", line)
						txBuffer[0] = '\0';
						rxBuffer[0] = '\0';
						timerRunning = False;
						endTime = time(NULL);
					}
					else
					{
						printf("Read only camera connection: pid = %d\n", getpid());
						DBG(5, "Read only camera connection: pid = %d\n", getpid())
						cmdBuf[0]='\0';
					}

					ufds[1].fd = slaveSocket;
					ufds[1].events = POLLIN;

					printf("*");			// next line prompt
					fflush(stdout);
					cmdBuf[0]='\0';
					while (1)
					{
						if (poll(ufds, 2, POLL_ms) > 0)		// poll for input events
						{
							// terminal input is only to controlling process
							if(controllingProcess)
							{
								if (ufds[0].revents & POLLERR || ufds[0].revents & POLLHUP)
								{
									printf("Poll reports broken connection from getline\n");
									DBG(1, "Poll reports broken connection from getline\n")
									sleep(3);
									goto clean_up;		// exit
								}
								if (ufds[0].revents & POLLIN)
										// input from getline (keyboard) when there is a client
								{
									n=read(rxpipe[0], cmdBuf, sizeof(cmdBuf)-1);
									if (n<0)
										{
										perror("2:read()");
										sleep(3);
										goto clean_up;		// exit
										}
									DBG(5, "Child got(n=%d): %s\n", n, cmdBuf)
									if (-1==command_interpreter(cmdBuf, False))		// interpret kybd input
										goto clean_up;		// exit command received
									if (nPipeMsgs>=N_MAX_PIPE)		// pipe was blocked, so update
									{
										if ( write(txpipe[1], cam_image_path, 1+strlen(cam_image_path)) < 0)
											perror("chdir() - pipe error");		// what should we do here?
										nPipeMsgs=1;
									}
									else
										nPipeMsgs=0;
									if (!timerRunning)
									{
										printf("*");			// next line prompt
										fflush(stdout);
									}
								}
							}
							if (ufds[1].revents & POLLERR || ufds[1].revents & POLLHUP)
							{
								break;			// broken connection
							}
							if (ufds[1].revents & POLLIN)	// input from server socket
							{
								cnt = read(slaveSocket, rxBuffer, sizeof(rxBuffer)-1);
								DBG(3, "*** Socket string: %s\n", rxBuffer)
								if (cnt == 0)
								{
									// cnt == 0 means socket is closing (?)
									break;
								}
								else if (-1==rxMsg(cnt, rxBuffer))	// interpret rx buffer
									break;						// exit command received
								rxBuffer[0] = '\0';
							}
						}
						txMsg(slaveSocket, txBuffer);	// send message, if any exists
						exposure_service();
					}

					printf("\nClosing the socket & exiting child process\n");
					cmdBuf[0]='\0';
					OK_response(1, ExiT, "");
					close(slaveSocket);
					goto clean_up;		// exit

				default: /* parent process */
					close(slaveSocket);
					if (controllingProcess)
						controllingPID = childPid;
					controllingProcess = False;	// all future connections
					cmdBuf[0]='\0';
				}
			}

	clean_up:
		if (masterProcess)
		{
			shutter_on_off(0);		// close down functions for exit.
			set_cam_stat(cam_shutter, "closed");
			set_cam_stat(cam_state, "idle");
			camera_shutdown();
			kill(getlinePid, SIGKILL);
		}
		return 0;
	}
}


/*****************************************************************************\
**                                                                           **
**      Server support routines                                              **
**                                                                           **
\*****************************************************************************/
/*
* Local replacement of gethostname() to aid portability
*/
static int _GetHostName(char *buffer, int length)
{
	struct utsname sysname = {{ 0 }};
	int status = 0;

	status = uname(&sysname);
	if (-1 != status)
	{
		strncpy(buffer, sysname.nodename, length);
	}

	return (status);
}


/*
** restoreControl - invoked by signal system when a process dies
*/
static __sighandler_t restoreControl(int x)
{
FILE* fp;
char line[50]="";
int status = 0;

processReentered = True;

// N.B. - it is bad practice to use printf() in a signal handler

DBG(5, "Entered restoreControl - controllingPID = %d\n", controllingPID)

// Upon entry here, we are always in the parent (master) process.
// If the controlling child has exited, return control token to the parent.
// make up the file name for proc system entry
strcpy(line, "/proc/");
sprintf(line+strlen(line), "%d", controllingPID);
strcat(line, "/status");
if ((fp = fopen(line, "r")) == NULL)
	{
//	printf("Could not open %s\n", line);
	DBG(5, "Could not open %s\n", line)
	return 0;
	}

fgets(line, sizeof(line), fp);
fgets(line, sizeof(line), fp);		// 2nd line has state
if (strchr(line, 'Z') != NULL)		// has the controlling process exited?
	{
	controllingProcess = True;
	controllingPID = getpid();
	sprintf(line, "%d", controllingPID);
	set_cam_stat(cam_controlling_pid, line);
	camera_update_variables();			// read variables from disk, if any
//	printf("Restoring controlling process token to main program\n");
	DBG(5, "Restoring controlling process token to main program, controllingPID = %d\n",
			controllingPID)
	}
fclose(fp);
waitpid(-1, &status, WNOHANG);		// reap zombie processes

return 0;
}


/*
** rxMsg - receive & interpret message(s) from client
**
** The standard message format is:
**   ccc...\nccc...\nccc...\x18ccc...\nccc...\nccc...\x18 etc.
** But, to be generous to the user, we treat all control characters as
** '\n - largely because IDL can send some strange strings.
*/
static int rxMsg(int cnt, char *buffer)
{
	int i, j, k;
	char *p;
	CAMERA_CMND cmnd;
	
	DBG(4, "%s rxMsg: %s\n", timestamp(), buffer)
	p = buffer;
	while (*p)				// change all ctrl-chars into '\n'
		{
		if (iscntrl(*p))
			*p = '\n';
		p++;
		}
	while (buffer[cnt-1] == '\n')
		{
		buffer[cnt-1] = '\0';
		cnt--;
		}
	i = j = 0;
	while (i < cnt-1)	// messages can arrive concatenated - take them apart
	{
		while (isspace( *(buffer+i)) )
			i++;
		if ( (p = strchr(buffer+i, '\n')) )
			*p = '\0';
		j = strlen(buffer+i);
		DBG(4, "Client message: %s\n", buffer+i)
		p = buffer+i;				// first character
		if (isdigit(*p))			// interpret numeric command
			{
			cmnd = (CAMERA_CMND)atoi(p);
			p = strchr(p, ' ');		// point to first space
			if (!p)
				return 1;
			p++;					// point to character after space
			k=0;
			while ( isspace(*(p+k)))
				k++;
			command_dispatcher(cmnd, p+k, 1);
			}
		else if ( (k=command_interpreter(p, 1)) )
			return k;
		i += j+1;
	}
	// if commands from a client are accepted without comment, i.e., without
	// a reply message, the following prompt is doubled, tripled...
	printf("*");			// prompt
	fflush(stdout);
	return 0;
}


/*
** txMsg - if message is ready, send to client
** Earlier versions would transmit the nul at the end of the buffer.
** But, this makes SPEC very unhappy, so final character is ^X.
*/
static void txMsg(int socket, char *buffer)
{
	if (strlen(buffer) > 0)
	{
		DBG(3, "%s txMsg: %s\n", timestamp(), buffer)
		write(socket, buffer, strlen(buffer));
		buffer[0] = '\0';
		DBG(5, "%s txMsg: transmission accepted\n", timestamp())
	}

	return;
}


/*
** print the menu of available commands
*/

static void menu_print()
{
char *list[500];
char line[ncols+1];
int i, j, n, col;

	// construct a list of pointers to the names
n = camera_count;
for (i=0; i<n; i++)
	list[i] = camera_list[i].name;

	// sort the pointers into alphabetical order
qsort (list, (size_t)n, sizeof(char *), (__compar_fn_t)menuPrint_helper);	/* sort the list */

	// print the names with 12 column alignment
for (i = 0; i < ncols; i++)
	line[i] = ' ';		/* blank the output line */
col = 5;					/* start printing in col 5 */
for (j = 0; j < n; j++)
	{if (col > 70 || col + strlen( list[j] ) >= ncols)
		{line[ncols] = '\0';
		printf(line);		/* line is full, so print */
		printf("\n");
		for (i = 0; i < ncols; i++)
			line[i] = ' ';
		col = 5;
		}
	for (i = 0; *(list[j]+i);  )		/* transfer name */
		line[col++] = *(list[j]+i++);
	// some commands end in upper-case letters to distinguish them from commands
	// of the same name and function in the tvx window.  Here I lower case them
	// just to make the printout look better.  But, not single letters, or
	// runs of upper case
	if (i > 1 && !isupper(line[col-2]))
		line[col-1] = tolower( line[col-1] );
	col = 5+12*(1+(++col-6)/12);		/* one blank & tab over */
	}
line[col] = '\0';
printf(line);		/* print partial line */
printf("\n");

return;
}

static int menuPrint_helper(char **s, char **t)
{
return (strcasecmp(*s, *t));	/* compare names */
}


/*****************************************************************************\
**                                                                           **
**      Camera support routines                                              **
**                                                                           **
\*****************************************************************************/
// look after exposure or wait in progress - millisecond resolution
static void exposure_service(void)
{
int i;
struct timeval tv;
double time;
char msgbufr[50];

if(!controllingProcess)
	return;

camera_monitor();				// called whenever camserver is running

if (timerRunning == False)
	return;

gettimeofday(&tv, 0);
time = (double)tv.tv_sec + (double)tv.tv_usec/1e6;

if ((i=camera_check_status(time)))	// possible premature end to expose or wait
	{
	end_exposure(Exposure, i);		// possibly force end
	return;
	}

if (time < endTime)		// update cam_stat entry
	{
	if (time - lastTime > 1.0)
		{
		lastTime = time;
		sprintf(msgbufr, "%.4f", endTime-time);
		set_cam_stat(cam_time, msgbufr);
		}
	return;
	}

end_exposure(Exposure, 0);			// normal end
return;
}


// wrap-up at the end of an exposure and readout
static void end_exposure(CAMERA_CMND cmnd, int prio)
{
struct timeval tv;
char msgbufr[50];
#ifdef DEBUG
int pid=(int)getpid();
#endif

if (timerRunning == False)
	return;

gettimeofday(&tv, 0);
endTime = (double)tv.tv_sec + (double)tv.tv_usec/1e6;
timerRunning = False;
if (Cam_State == CamWaiting)
	{
	Cam_State = CamIdle;
	set_cam_stat(cam_time, "0");
	set_cam_stat(cam_state, "idle");
	OK_response(txImage, CamWait, "");
	return;
	}
camera_stop(prio);		// hardware timing should run out the clock
if (shutter_control)
	printf(" Closing shutter\n");
shutter_on_off(0);				// close shutter, even if not open
printf(" Ending exposure:                  %s\n", timestamp());
DBG(2, " Ending exposure:                  %s\n", timestamp())
set_cam_stat(cam_shutter, "closed");
set_cam_stat(cam_time, "0");
set_cam_stat(cam_state, "reading");

/*
** If camera_readout() forks a process, then writing messages and cam_stat info
** here is invalid - this process doesn't actually know the completion status
*/
if (prio<2)							// 0 & 1 are normal termination
	{
	if ( camera_readout(image_file, msgbufr) )
		ERR_response(txImage, cmnd, msgbufr);
	else
		{
		printf("  Camera image was written:\n  %s\n", image_file);
		DBG(1, "  Camera image was written %s (pid %d):\n  %s\n", 
				timestamp(), pid, image_file)
	sleep(1);
		OK_response(txImage, cmnd, image_file);
		}
	// in the case of hardware timing, endTime may be modified by the
	// readout routine in interface.c; so, delay printing until here
	printf("  Elapsed time: %.4lf seconds\n", endTime-startTime);
	DBG(2, "Elapsed time: %.4lf seconds\n", endTime-startTime)
	}
set_cam_stat(cam_state, "idle");
//set_cam_stat(cam_completed, image_file);
set_cam_stat(cam_target, "(nil)");
Cam_State = CamIdle;
header_string[0]='\0';	// reset header string
printf("*");			// prompt
fflush(stdout);

return;
}


/*****************************************************************************\
**                                                                           **
**      Other support routines                                               **
**                                                                           **
\*****************************************************************************/
// return an OK response to client
static void OK_response(int tx, CAMERA_CMND cmnd, char *txt)
{
if (!tx)					// only transmit to a client
	return;
DBG(5, "%s OK_response: %s\n", timestamp(), txt)
sprintf(txBuffer+strlen(txBuffer), "%d", (int)cmnd);
strcat(txBuffer, " OK ");
strcat(txBuffer, txt);
//if (txBuffer[strlen(txBuffer)-1] != '\n')
//	strcat(txBuffer, "\n");
strcat(txBuffer, "\x18");		// ^X terminates message

return;
}


// return an error response to client
static void ERR_response(int tx, CAMERA_CMND cmnd, char *txt)
{
if (!tx)					// only transmit to a client
	return;
DBG(5, "%s Camera error: %s\n", timestamp(), txt)
sprintf(txBuffer+strlen(txBuffer), "%d", (int)cmnd);
strcat(txBuffer, " ERR ");
strcat(txBuffer, txt);
//if (txBuffer[strlen(txBuffer)-1] != '\n')
//	strcat(txBuffer, "\n");
strcat(txBuffer, "\x18");		// ^X terminates message

return;
}


// check whether current process has permission to control camera
static int check_permission(CAMERA_CMND cmnd)
{
if (controllingProcess || masterProcess)
	return 0;

// only clients can generate permission denied, so we need not check source
printf(" --- access denied\n");
ERR_response(1, CamCmd, "access denied");
return 1;
}
