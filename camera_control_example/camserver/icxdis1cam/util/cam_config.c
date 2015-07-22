// cam_config.c -- Read tvx camera configuration files at startup

/******************************************************************************\
** Method - first read a system-wide configuration file from /usr/local/etc,
** if it exists.  This path is settable in cam_config.h.  Then read the user's
** personal resource file from his home directory, if it exists.  Finally,
** look for the resource file in the directory provided in the command line,
** if present.  Failure to find these files is not an error.
** The debug file shows the results.
**
** We use the same method for generating the required header as in tvx.c, namely
** that the program misc/mkheader examines the cases below and writes the
** needed enums and structures to include/cam_config.h.  Thus, to enter a new 
** parameter, it is only reuired to enter the case name in the switch statement
** below.  The first letter of the case statement is capitalized to avoid
** collision with the identically-spelled variable name.
**
** However, it is not required to have an associated variable with the same
** name - see logfile_name as an example.
\******************************************************************************/

#define CAM_CONFIG_MAIN		// turn on space allocations in cam_configure.h

#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include <time.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>
#include <math.h>
#include "cam_config.h"
#include "camserver.h"
#include "cam_tbl.h"
#include "debug.h"

#undef CAM_CONFIG_MAIN


/* ----- Local functions defined in this module ---- */
static void read_configuration(FILE *, char *);
static void set_configuration(char *, char *);
static void isolate_string(char **);
static void print_cam_setup_helper(char *, char *, char *);
static void resolve_path(char *, char *);
static void resolve_name(char *, char *);

static int camstat_is_configured = False;
static char *resource_path;
static double previous_exposure_time;
static char last_camstat_line[LENFN];


void cam_config_initialize(char *rcpath)
{
char line2[120], cmsg[120]="", dmsg[120]="", *p;
FILE *ifp;
struct stat buf;

if ((ifp = fopen(SYSTEM_CAM_CONFIG_FILE, "r")) != NULL)	// e.g. /usr/local/etc/camrc
	{
	sprintf(cmsg, "...Configured from %s\n", SYSTEM_CAM_CONFIG_FILE);
	DBG(1, "...Configuring from %s\n", SYSTEM_CAM_CONFIG_FILE);
	resource_path = SYSTEM_CAM_CONFIG_FILE;
	read_configuration(ifp, dmsg);
	fclose(ifp);
	}

if (strstr(USER_CAM_CONFIG_FILE, "$HOME") != NULL)		// e.g. $HOME/.camrc
	{
	strcpy(line2, getenv("HOME"));		// make up absolute path
	strcat(line2, strlen("$HOME")+USER_CAM_CONFIG_FILE);
	}
else
	strcpy(line2, USER_CAM_CONFIG_FILE);
if ((ifp = fopen(line2, "r")) != NULL)
	{
	sprintf(cmsg, "...Configured from %s\n", line2);
	DBG(1, "...Configuring from %s\n", line2);
	resource_path = line2;
	read_configuration(ifp, dmsg);
	fclose(ifp);
	}

if ( rcpath && rcpath[0] )
	strcpy(line2, rcpath);
else
	line2[0] = '\0';
if ( LOCAL_CAM_CONFIG_FILE[0] == '.' )		// e.g. ./camrc
	strcat(line2, 1+LOCAL_CAM_CONFIG_FILE);
else
	strcat(line2, LOCAL_CAM_CONFIG_FILE);
if ((ifp = fopen(line2, "r")) != NULL)
	{
	sprintf(cmsg, "...Configured from %s\n", line2);
	DBG(1, "...Configuring from %s\n", line2);
	resource_path = line2;
	read_configuration(ifp, dmsg);
	fclose(ifp);
	}

printf("%s", cmsg);		// shows last one
if (dmsg[0])			// shows last one
	{
	p = strchr(dmsg, '\n');		// print as 2 lines
	*p = '\0';
	printf("%s\n", dmsg);
	printf("%s", p+1);
	}

// set up cam_stat, a proc-like file system to communicate camera status
if (-1 == stat(camstat_path, &buf))
	if (-1 == mkdir(camstat_path, 0755))
		{
		printf("*** Could not make directory %s\n", camstat_path);
		return;
		}

camstat_is_configured = True;
set_cam_stat(cam_def_file, cam_definition);
set_cam_stat(cam_name, camera_name);
set_cam_stat(cam_state, "idle");
set_cam_stat(cam_target, "(nil)");
set_cam_stat(cam_completed, "(nil)");
set_cam_stat(cam_time, "0");
set_cam_stat(cam_exptime, "1.0");				// default set in camserver.c
set_cam_stat(cam_shutter, "closed");
set_cam_stat(cam_telemetry, "(nil)");
set_cam_stat(cam_variables, "# variables");		// empty the file on initialization
sprintf(line2, "%d", getpid());
set_cam_stat(cam_controlling_pid, line2);

previous_exposure_time = exposure_time;

return;
}


// find potentially valid lines in the configuration file
static void read_configuration(FILE *ifp, char *dmsg)
{
char line[120], *p, *q;

while ( (fgets(line, sizeof(line), ifp) != NULL) )
	{
	if ( (p = strchr(line, '#')) )
		*p = '\0';						// cut off comments
	p = line;
	while (isspace(*p))					// skip leading whitespace
		p++;
	q = p-1+strlen(p);
	while ( isspace(*q) )				// remove trailing whitespace
		*q-- = '\0';
	if (!*p)
		continue;
	set_configuration(p, dmsg);
	}
}


// p points to a non-blank line - try to set a variable from it
static void set_configuration(char *p, char *dmsg)
{
int i;
char rmsg[120], line[120];

for (i = 0; i < cam_config_count; i++)
	{
	
	if ( strncasecmp(p, cam_config_list[i].name, strlen(cam_config_list[i].name)) == 0)
		{
		DBG(6, "Configuring: %s\n", p);
		p += strlen(cam_config_list[i].name);	// skip to value field
		while (isspace(*p) || *p == '=')		// strip blanks & '='
			p++;

	// In the case statements, one letter must differ in case from the variable name

	/* start of main loop switch - please preserve the format for mkheader */
		switch (cam_config_list[i].cmnd)
			{
/*****************************************************************************\
**  Add camera-specific configuration commands here.                         **
\*****************************************************************************/

/*****************************************************************************\
** Camera config commands below here are part of the base distribution       **
\*****************************************************************************/

			case Camstat_path:
				resolve_path(p, camstat_path);
				break;

			case Cam_definition_file:
				resolve_name(p, line);
				sprintf(dmsg, "...Read camera setup data from:\n    %s\n", line);
				DBG(1, "(cam_config.c) Reading camera setup data from %s\n", line);
				camera_read_setup(line, rmsg);
				if (*rmsg)
					{
					DBG(1, "(cam_config.c) Returned message: %s\n", rmsg);
					strcpy(dmsg, rmsg);
					}
				break;

			case Cam_data_path:
				resolve_path(p, cam_data_path);
				break;

			case Cam_exposure_time:
				sscanf(p, "%lf", &exposure_time);
				break;

			case Cam_image_path:
				resolve_path(p, cam_image_path);
				break;

			case Cam_startup_file:
				resolve_name(p, cam_startup_file);
				break;

			case Camera_port_number:
				sscanf(p, "%d", &camPort);
				break;

#ifdef DEBUG
			case Dbglvl:
				sscanf(p, "%d", &dbglvl);
				break;
#endif

			default:
				break;
			}
			/* end of main loop switch */
		return;
		}
	}

return;
}


// if string has quotes, take them off
static void isolate_string(char **pp)
{
char *q;

if ( (q = strchr(*pp, '"')) != NULL)
	{
	*pp = q+1;		// move pointer to 1st char after quote
	if ( (q = strchr(*pp, '"')) != NULL)		// there should be another one
		*q = '\0';								// new string end
	}

// remove newline, if present
if ( (q = strchr(*pp, '\n')) != NULL)
	*q = '\0';

return;
}

/*****************************************************************************\
**                                                                           **
**     Routines to support the proc-like camera status file-system           **
**                                                                           **
\*****************************************************************************/
/*
**  set a status message in camstat
*/
void set_cam_stat(CAM_STATUS event, char *txt)
{
char line[120];
FILE *ofp;

if (!camstat_is_configured)
	return;


// text is stored with newline to make reading by 'cat' more pleasant
strcpy(line, camstat_path);
switch(event)
	{
	case cam_def_file:
		strcat(line, "configfile");
		break;

	case cam_name:
		strcat(line, "camname");
		break;

	case cam_state:
		strcat(line, "state");
		break;

	case cam_target:
		strcat(line, "target");
		break;

	case cam_completed:
		strcat(line, "completed");
		break;

	case cam_time:
		strcat(line, "time");
		break;

	case cam_exptime:
		strcat(line, "exptime");
		break;

	case cam_shutter:
		strcat(line, "shutter");
		break;

	case cam_telemetry:
		strcat(line, "telemetry");
		break;

	case cam_variables:
		strcat(line, "variables");
		break;

	case cam_controlling_pid:
		strcat(line, "controlpid");
		break;

	default:
		return;
		break;
	}
if ((ofp = fopen(line, "w")) == NULL)
	{
	printf("Could not open %s for writing\n", line);
	return;
	}
fprintf(ofp, "%s\n", txt);
fclose(ofp);

return;
}


/*
** read the camera status from camstat and print it in buffer provided
** length of buffer is LENMSG, and is assumed to be adequate
*/
int print_cam_setup(char *p)
{
char *o=p;			// origin

if (!camstat_is_configured)
	return 0;

strcpy(p, "\n");
p += strlen(p);

print_cam_setup_helper(p, "/configfile", "    Camera definition:\n\t  ");
print_cam_setup_helper(p, "/camname", "    Camera name: ");
print_cam_setup_helper(p, "/state", "    Camera state: ");
print_cam_setup_helper(p, "/target", "    Target file: ");
print_cam_setup_helper(p, "/time", "    Time left: ");
print_cam_setup_helper(p, "/completed", "    Last image: ");
print_cam_setup_helper(p, "/masterpid", "    Master PID is: ");
print_cam_setup_helper(p, "/controlpid", "    Controlling PID is: ");
print_cam_setup_helper(p, "/exptime", "    Exposure time: ");
print_cam_setup_helper(p, "/completed", "    Last completed image:\n\t ");
print_cam_setup_helper(p, "/shutter", "    Shutter is: ");

// N.B. We could put 'cam_telemtry' here as well, but it has the potential
// to be very long.  Since 'telemetry' is already a separate command, perhaps
// it is not needed.

return p-o;			// length
}


static void print_cam_setup_helper(char *p, char *fn, char *title)
{
char line[120];
FILE *ifp;

strcpy(line, camstat_path);
strcat(line, fn);
if ((ifp = fopen(line, "r")) == NULL)
	{
	printf("Could not open %s for reading\n", line);
	return;
	}
strcat(p, title);
p += strlen(p);
fgets(p, 70, ifp);
p += strlen(p);
fclose(ifp);
return;
}


/*****************************************************************************\
**                                                                           **
**     Filename and Pathname routines                                        **
**                                                                           **
\*****************************************************************************/
// make up path if $HOME specified
static void resolve_path(char *p, char *sysvar)
{
char *q;

isolate_string(&p);
if ( (q = strstr(p, "$HOME")) )
	{
	strcpy(sysvar, getenv("HOME"));
	strcat(sysvar, q+strlen("$HOME"));
	}
else if ( (q = strchr(p, '~')) )
	{
	strcpy(sysvar, getenv("HOME"));
	strcat(sysvar, q+1);
	}
else if ( (q = strstr(p, "../")) )
	{
	strcpy(sysvar, resource_path);
	*strrchr(sysvar, '/') = '\0';		// cut off filename
	*strrchr(sysvar, '/') = '\0';		// back up 1 directory level
	strcat(sysvar, q+2);
	}
else if ( (q = strstr(p, "./")) )
	{
	strcpy(sysvar, resource_path);
	*strrchr(sysvar, '/') = '\0';		// cut off filename
	strcat(sysvar, q+1);
	}
else
	strcpy(sysvar, p);
if ( *(sysvar + strlen(sysvar) -1) != '/')
	strcat(sysvar, "/");		// fix ending

q = sysvar;
while (*sysvar)							// remove possible doubled '/'
	if (*sysvar=='/' && *(sysvar+1)=='/')
		sysvar++;
	else
		*q++ = *sysvar++;

return;
}


// make up a filename taking $HOME into account
static void resolve_name(char *p, char *sysvar)
{
char *q;

isolate_string(&p);
if ( (q = strrchr(p, '/')) )
	{
	*q++ = '\0';		// chop off name, but remember it
	resolve_path(p, sysvar);
	strcat(sysvar, q);	// put the name back
	}
else
	strcpy(sysvar, p);		// just a plain name

return;
}


/*****************************************************************************\
**                                                                           **
**     Routines to update and read variables across instantiations           **
**                                                                           **
\*****************************************************************************/
// record the value of a variable as text to be read when a new instance
// of camserver is started
void camera_record_variable(CAM_CONFIG_CMND cmd, char *txt)
{
char line[120];
FILE *ofp;

if (!camstat_is_configured)
	return;

if (!strcmp(txt, last_camstat_line))		// don't duplicate lines
	return;
strcpy(last_camstat_line, txt);

switch(cmd)			// if special processing is required
	{
/*****************************************************************************\
**  Add camera-specific processing here.                                     **
\*****************************************************************************/

/*****************************************************************************\
** Below here is part of the base distribution                               **
\*****************************************************************************/

	case Cam_exposure_time:
		if ( fabs(1.0 - previous_exposure_time/exposure_time) < 0.0001)
			return;				// do not record tiny changes
		previous_exposure_time = exposure_time;
		break;
	default:
		break;
	}

strcpy(line, camstat_path);
strcat(line, "variables");
if ((ofp = fopen(line, "a")) == NULL)
	{
	printf("Could not open %s for writing\n", line);
	return;
	}
fprintf(ofp, "%s %s\n", cam_config_list[-1+(int)cmd].name, txt);	// enum offset by 1
fclose(ofp);
return;
}


// read the stored values of variables from camstat/variables
void camera_update_variables(void)
{
char line[120], line2[120];
FILE *ifp;

if (!camstat_is_configured)
	return;
strcpy(line, camstat_path);
strcat(line, "variables");
if ((ifp = fopen(line, "r")) == NULL)
	{
	printf("Could not open %s for reading\n", line);
	return;
	}
line2[0]='\0';
while (fgets(line, sizeof(line), ifp))
	set_configuration(line, line2);
fclose(ifp);

// if this is the master process, variables can be emptied
if(masterProcess)
	set_cam_stat(cam_variables, "# variables");		// empty the file

return;
}
