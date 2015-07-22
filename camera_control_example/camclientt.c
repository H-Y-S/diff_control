// camclientt.c - the test program for the camera client.

#define CAMERA_IP "127.0.0.1"
#define CAMERA_PORT 41234

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/poll.h>
#include "tvxsys.h"
#include "screen.h"
#include "tvxobject.h"
#include "debug.h"


#ifdef DEBUG
FILE *dbgofp;   // global output file
int dbglvl;     // debugging control level
#endif

char camMsgBuf[1000];
int camera_is_initialized = 0;
int WaitingForCamera = 0;
int sys_error;
char tvx_buffer[1000];


int main (int argc, char *argv[])
{
char buffer[256] = "", rxBuffer[250] = "";
char *res;
int status;
struct pollfd ufds;
int i, j;
		

printf ("*** Camera client test\n");
status = camclient_initialize(CAMERA_IP, CAMERA_PORT);
printf("Connecting to camera: host i.p. = %s, port no. = %d\n",
			CAMERA_IP, CAMERA_PORT);


if (status < 0)
	exit (1);

ufds.fd = 0;		// set up polling on stdin
ufds.events = POLLIN;

// send a message to the camera
printf("Send message #1 to camera\n");
strcpy(buffer, "Send ...Message to the camera #1");
write_camera_string(buffer);

/* This may be needed to study concatenated messages
** // send a message to the camera - bomb the server
** printf("Send message #2 to camera\n");
** strcpy(buffer, "Message to the camera #2");
** write_camera_string(buffer);
** printf("Send message #3 to camera\n");
** strcpy(buffer, "Message to the camera #3");
** write_camera_string(buffer);
** printf("Send message #4 to camera\n");
** strcpy(buffer, "Message to the camera #4");
** write_camera_string(buffer);
** printf("Send message #5 to camera\n");
** strcpy(buffer, "Message to the camera #5");
** write_camera_string(buffer);
** printf("Client now polling for messages\n");
*/

// get some messages from the operator & transmit them
while(1)
	{
		rxBuffer[0]='\0';
		if ((status = read_camera_string(rxBuffer, sizeof(rxBuffer))) > 0)
		{	
			if (status < 50)
				printf("Raw message from server: %d: %s\n", status, rxBuffer);
			else
				printf("Raw message from server: %d:\n  %s\n", status, rxBuffer);
			if (rxBuffer[status-1] == '\n')
				rxBuffer[status-1] = '\0';
			if (strcmp("exit", rxBuffer) == 0)
				break;
			if (strcmp("quit", rxBuffer) == 0)
				break;
			if (status > strlen(rxBuffer)+1)
			{			// messages can arrive concatenated - take them apart
				i = 0;
				while ( i < status-1)
				{
					j = strlen(rxBuffer+i);
					printf("Details of message - status: %d: %s\n", j, rxBuffer+i);
					i += j+1;
				}
			}
		}
		if (poll(&ufds, 1, 2) > 0)
		{
			res = fgets(buffer, sizeof(buffer), stdin);
			if (buffer[strlen(buffer)-1] == '\n')
				buffer[strlen(buffer)-1] = '\0';
			printf("Sending message to server: %s\n", buffer);
			write_camera_string(buffer);
			if (strcmp("exit", buffer) == 0)
				break;
			if (strcmp("quit", buffer) == 0)
				break;
			buffer[strlen(buffer)-1] = '\0';
		}
	}


// close the client socket
close_camera_connection();
printf("Camclientt: closing connection to camera server\n\n");

return 0;
}


// dummy routines to satisfy linking
int timerRunning;
char expose_path[100];
float expt=0;
struct object_descriptor *DefaultIMPtr;
struct object_descriptor *exposeObject;
int exposeOverhead;

int reset_cursor(void)
{return 0;}

void setProcessTime(double q)
{return;}

int clkslp(void)
{return 0;}

int write_line(const char *x, ...)
{return 0;}

void exposureCB(int s, char *p)
{return;}

void telemetryCB(int s, char *p)
{return;}

void sendCB(int s, char *p)
{return;}

void check_lines(int *a)
{return;}

int display_width(void)
{return 80;}

char *timestamp(void)
{return NULL;}
