/* cam_tbl.c - table of camera-specific parameters for use by camserver
** and supporting routines.  Every variable has a default, which optionally 
** may be overwritten by the camera definition file. 
**
** EFE, May 00
*/

#define CAM_TBL_MAIN	// turn on storage allocations in cam_tbl.h

#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <stdlib.h>
#include "tvxcampkg.h"
#include "camserver.h"
#include "pix_detector.h"		// read_detector_info()
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
**      End of global camera variables                                       **
**                                                                           **
\*****************************************************************************/


/* ----- Functions defined in this module ---- */
static int set_configuration(char *);
static void isolate_string(char **);
static void resolve_path(char *, char *);
static void resolve_name(char *, char *, char *);

static int n_vars_found;

// possible error message returned in msg
void read_camera_setup(char *filename, char *msg)
{
char line[120], *p=NULL, *q;
int i, idx, nselbits=0, err=0;
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

if ((ifp = fopen(line, "r")) == NULL)
	{
	sprintf(msg, "Cannot find camera setup file:\n\t%s\n", line);
	return;
	}
else
	*msg = '\0';		// no error
strcpy(cam_definition, line);
DBG(2, "(cam_tbl.c) config file is: %s\n", line);
n_vars_found = 0;

while (!err && (fgets(line, sizeof(line), ifp) != NULL) )
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
	if ( strstr(p, "asserted_level_of_DATA") == p )
		{
		if (! (p = strstr(p, "lvl_")) )
			{
			printf("*** Error: lvl not found: %s", p);
			continue;
			}
		if ( *(p+strlen("lvl_")) == '0')
			fifo_data_true_level = 0;
		else
			fifo_data_true_level = 1;
		fifo_set_true_level(fifo_data_true_level);	// transmit to fifo
		n_vars_found++;
		}
	else if ( strstr(p, "data_selector_image_") == p )
		{
		p += strlen("data_selector_image_");
		idx = atoi((p));
		selectors[idx] = 0;
		p += 2;		// skip to white space
		// older files had 16 selector bits, now 32
		if (!nselbits)
			{
			q = p;
			while (*q && *q != '#')
				{
				if (isdigit(*q))
					nselbits++;
				q++;
				}
			}
		for (i=0; *p && i<nselbits; i++)		// pick up 16 or 32 bits
			{
			while(isspace(*p))
				p++;
			selectors[idx] <<= 1;
			if (*p == '1')
				selectors[idx] |= 0x1;
			p++;
			}
		DBG(7,"(cam_tbl.c) selectors[%d] = 0x%.4x\n", idx, selectors[idx]);
		n_vars_found++;
		}
	else 
		err = set_configuration(p);
	}

if (n_vars_found == 0)
	sprintf(msg, "No definitions recognized in:\n\t%s\n", line);
if (err)
	{
	*(p+50) = '\0';		// truncate possibly long line
	sprintf(msg, "***Error in line: %s\n", p);
	}

set_cam_stat(cam_def_file, cam_definition);
set_cam_stat(cam_name, camera_name);
return;
}



// p points to a non-blank line - try to set a variable from it
static int set_configuration(char *p)
{
int i, err=0;
char fname[120];

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

			case Ppg_configuration_file:
				resolve_name(p, fname, cam_definition);
				err = read_detector_info(fname);
				break;

			default:
				break;
			}
			/* end of main loop switch */
		n_vars_found++;
		return err;
		}
	}
*(p+30) = '\0';
DBG(1, "(cam_tbl.c) Unrecognized configuration variable: %s\n", p);
return 0;		// not really an error
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
else
	strcpy(sysvar, p);
if ( *(sysvar + strlen(sysvar) -1) != '/')
	strcat(sysvar, "/");		// fix ending

return;
}


// make up a filename taking $HOME into account
static void resolve_name(char *p, char *sysvar, char *base)
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
	{			// just a plain name - use path from 'base'
	*sysvar = '\0';
	if (base)
		{
		strcpy(sysvar, base);
		*(1+strrchr(sysvar, '/')) = '\0';
		}
	strcat(sysvar, p);
	}

return;
}
