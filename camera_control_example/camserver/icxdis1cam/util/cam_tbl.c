/* cam_tbl.c - table of camera-specific parameters for use by camserver
** and supporting routines.  Every variable has a default, which optionally 
** may be overwritten by the camera definition file. 
**
** EFE, May 00
*/

#define CAM_TBL_MAIN	// turn on storage allocations in cam_tbl.h

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include "cam_config.h"
#include "camserver.h"
#include "cam_tbl.h"
#include "debug.h"

/*****************************************************************************\
**                                                                           **
**    Global variables - allocated here, overwritten from camera.def below   **
**                                                                           **
\*****************************************************************************/

char camera_name[50] = "default_camera";
char cam_definition[LENFN];
int camera_wide = 512;
int camera_high = 512;
int camera_bpp = 8;

/*****************************************************************************\
**                                                                           **
**      End of lobal camera variables                                        **
**                                                                           **
\*****************************************************************************/


/* ----- Functions defined in this module ---- */
static void set_configuration(char *);
static void isolate_string(char **);


void read_camera_setup(char *filename, char *msg)
{
char line[120], *p=NULL, *q;
FILE *ifp;

*line = '\0';
if (*filename == '~')
	{
	strcpy(line, getenv("HOME"));	// make up absolute path
	strcat(line, filename+1);
	}
else if (*filename == '.')
	{
	getcwd(line, sizeof(line));
	strcat(line, filename+1);
	}
else
	strcpy(line, filename);

if ((ifp = fopen(line, "r")) == NULL)	// error message returned in name
	{
	sprintf(msg, "Cannot find camera setup file: %s\n", line);
	return;
	}
else
	*msg = '\0';		// no error
strcpy(cam_definition, line);
DBG(2, "(cam_tbl.c) config file is: %s\n", line);

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
	set_configuration(p);
	}

set_cam_stat(cam_def_file, cam_definition);
set_cam_stat(cam_name, camera_name);
return;
}



// p points to a non-blank line - try to set a variable from it
static void set_configuration(char *p)
{
int i;

for (i = 0; i < cam_tbl_count; i++)
	{
	
	if ( strncasecmp(p, cam_tbl_list[i].name, strlen(cam_tbl_list[i].name)) == 0)
		{
		DBG(6, "Configuring: %s\n", p);
		p += strlen(cam_tbl_list[i].name);	// skip to value field
		while (isspace(*p) || *p == '=')		// strip blanks & '='
			p++;

	// In the case statements, one letter must differ in case from the variable name

	/* start of main loop switch - please preserve the format for mkheader */
		switch (cam_tbl_list[i].cmnd)
			{
			case Camera_name:
				isolate_string(&p);
				strcpy(camera_name, p);
				break;

			case Camera_wide:
				sscanf(p, "%d", &camera_wide);
				break;

			case Camera_high:
				sscanf(p, "%d", &camera_high);
				break;

			case Camera_bpp:
				sscanf(p, "%d", &camera_bpp);
				break;

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

return;
}
