// interface.c - execute basic camera instructions 

// hardware specific for: icxdis1 camera

/*
** The following routines are here:
**
** camera_readout
** shutter_on_off
** camera_initialize
** camera_prepare
** camera_start
** camera_stop
** camera_read_telemetry
** camera_check_status
** camera_reset
** camera_shutdown
** camera_read_setup
** camera_monitor
** camera_cmnd_filter
*/


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/vfs.h>
#include <fcntl.h>
#include <unistd.h> 
#include "cam_config.h"
#include "camserver.h"
#include "debug.h"
#include "cam_tbl.h"
#include "tvxobject.h"
#include "imageio.h"

#include "edt.h"
  

/*****a.heron added 02/2001**************************
***
***	keep initialisations for added camera specific commands to external programs below
***	These include path and start of command line
***	oarg is added to path and command in CASE section and sent to edt
***	Actual command line being run is the "CASE" in the commands section
***
*****************************************************/

char edttake[100] = "/home/ajh/EDT/take ";		//EDT take command - set the correct path
char edtinitcam[100] = "/home/ajh/EDT/initcam ";	//EDT initcam command - set the correct path
char edtinfo[100] = "/home/ajh/EDT/edtinfo ";


/*****************************************************/



/*****************************************************************************\
**                                                                           **
**    camera_readout:  trigger a camera readout                              **
**                                                                           **
**    Data are placed in the fully prepared object descriptor argument.      **
**                                                                           **
**    Returns an error flag: 0 means OK, 1 means an error                    **
**    This routine should print the error message                            **
**                                                                           **
\*****************************************************************************/

int camera_readout(char *image_file, char *msg)
{
int inh, n;
char *imgP, line[LENFN];
struct object_descriptor Obj;
struct statfs disk_data;
struct image_info *info=NULL;

msg[0] = '\0';		// default

/*
**  We build an object descriptor to transport our image
**  Start filling object descriptor.  First find out if we can write.
*/

memset(&Obj, 0, sizeof(struct object_descriptor));
Obj.Kind = Image;
Obj.Type = Memory;
Obj.Role = Destination;
Obj.Wide = camera_wide;
Obj.High = camera_high;
Obj.Bpp = camera_bpp;
Obj.Path = image_file;

if ((Obj.Handle = open(Obj.Path, O_WRONLY | O_CREAT | O_TRUNC,
					S_IRUSR | S_IWUSR |S_IRGRP | S_IWGRP | S_IROTH)) == -1)
	{
	printf("Could not open %s for writing\n", image_file);
	sprintf(msg, "Could not open %s for writing\n", image_file);
	return 1;
	}
Obj.OStatus = OpenWrite;

// check disk blocks available vs. image size in blocks
statfs(Obj.Path, &disk_data);	// returns nonsense if the path does not exist
if ( disk_data.f_bavail <
		(Obj.Wide * Obj.High * Obj.Bpp)/ (8 * disk_data.f_bsize) )
	{
	printf("***** Disk Is Full *****\n");
	if (Obj.Handle) close(Obj.Handle);
	Obj.Handle = 0;
	return 1;
	}

fill_basic_header(&Obj, NULL);					// allocate a header
info = (struct image_info *)Obj.Header;		// header, ready to use

/*****************************************************************************\
**                                                                           **
**    Fill in header here - telemetry values, etc.                           **
**    Values go into the info structure, defined in tvxobjec.h               **
**    The info is then automatically written to the file.                    **
**                                                                           **
\*****************************************************************************/


/*****************************************************************************\
**                                                                           **
**     The code to read an image from the camera goes here.                  **
**     The result is that the image data are in memory at 'imgP'             **
**                                                                           **
\*****************************************************************************/


/*
**  But, here we allocate & fill imgP from canned data.
*/

Obj.High=1024;		// known parameters of fake image
Obj.Wide=1024;
Obj.Bpp=16;
// CameraWide*CameraHigh*CameraBpp/8 = 1024*1024*16/8
// 4096 is for the header
if ( (imgP = malloc(4096+1024*1024*16/8)) == NULL)
	{
	printf("Camserver could not allocate memory\n");
	sprintf(msg, "Camserver could not allocate memory\n");
	free(Obj.Header);
	return 1;
	}

strcpy(line, cam_initial_path);
strcat(line, "test/p.tif");
if ((inh = open(line, O_RDONLY)) == -1)		// fake an image
	{
	printf("Could not open fake data file test/p.tif\n");
	strcpy(msg, image_file);
	free(Obj.Header);
	return 1;
	}

printf("Reading the camera image\n");
n=read(inh, imgP, 4096+1024*1024*16/8);
close(inh);
Obj.Data=imgP+4096;		// attach the data to the object, skipping header

/*
**  Now we have our fake image
*/


/*
**  Write image file to disk
*/
write_diskimage(&Obj, 0, Obj.High*Obj.Wide*Obj.Bpp/8, Obj.Data);

/*
** clean up
*/
if (Obj.Handle) close(Obj.Handle);
Obj.Handle = 0;
free(Obj.Header);
free(imgP);

return 0;
}



/*****************************************************************************\
**                                                                           **
**   shutter_on_off:  operate the shutter                                    **
**                                                                           **
**   state = 0  => close the shutter                                         **
**   state = 1  => open the shutter                                          **
**                                                                           **
**   appropriate messages are given in main program, not here                **
**                                                                           **
\*****************************************************************************/

void shutter_on_off(int state)
{
return;
}



/*****************************************************************************\
**                                                                           **
**    camera_prepare:  prepare camera for exposure                           **
**                                                                           **
**    A pointer to the exposure time (in seconds) is the argument.           **
**                                                                           **
**    If the camera does hardware exposure timing, it might be useful to     **
**    slightly increase camserver's exposure time so that camserver times    **
**    out after the actual exposure is complete.  0.01 should do it.         **
**                                                                           **
**    Returns an error flag: 0 means OK, 1 means an error                    **
**    This routine should print the error message                            **
**                                                                           **
\*****************************************************************************/

int camera_prepare(double *exp_time)
{
return 0;
}



/*****************************************************************************\
**                                                                           **
**    camera_initialize:  hardware specific initialization of camera         **
**                                                                           **
**    Returns an error flag: 0 means OK, 1 means an error                    **
**    This routine should print the error message                            **
**                                                                           **
\*****************************************************************************/

int camera_initialize(void)
{
return 0;
}



/*****************************************************************************\
**                                                                           **
**    camera_start:  start integrating or counting                           **
**                                                                           **
**    The exposure time is passed as the agument, should it be needed        **
**                                                                           **
**    Returns an error flag: 0 means OK, 1 means an error                    **
**    This routine should print the error message                            **
**                                                                           **
\*****************************************************************************/

int camera_start(double *exp_time)
{
return 0;
}



/*****************************************************************************\
**                                                                           **
**    camera_stop:  stop integrating or counting                             **
**                                                                           **
**    Some cameras will control exposure time in hardware, others will let   **
**    let camserver do the timing.  In the case of hardware control, this    **
**    routine should 'hang up' waiting for the hardware to finish, because   **
**    camserver will proceed immediately to the readout state.               **
**                                                                           **
**    Returns an error flag: 0 means OK, 1 means an error                    **
**    This routine should print the error message                            **
**                                                                           **
\*****************************************************************************/

int camera_stop(void)
{
return 0;
}



/*****************************************************************************\
**                                                                           **
**    camera_telemetry:  read camera telemetry into buffer provided          **
**                                                                           **
\*****************************************************************************/

void camera_read_telemetry(char *bufr)
{
char line[50];
int n;
time_t t = time(NULL);

strftime(line, 50, "%X %x", localtime(&t) );
n = sprintf(bufr, "- * Fantastic telemetry at %s * -", line);

if (size < 2048)
	return;

return;
}



/*****************************************************************************\
**                                                                           **
**    camera_check_status:  check status of camera                           **
**                                                                           **
**    camera can call a premature end to an exposure or wait, if needed      **
**                                                                           **
\*****************************************************************************/

int camera_check_status(double time)
{
return 0;
}



/*****************************************************************************\
**                                                                           **
**    camera_reset:  reset the camera                                        **
**                                                                           **
\*****************************************************************************/

void camera_reset(void)
{
return;
}



/*****************************************************************************\
**                                                                           **
**    camera_shutdown:  program is exiting - close down camera as needed     **
**                                                                           **
\*****************************************************************************/

void camera_shutdown(void)
{

DBG(3, "(interface.c) camera_shutdown: pid = %d\n", getpid());

return;
}



/*****************************************************************************\
**                                                                           **
**    camera_read_setup:  (re)read the camera setup                          **
**                                                                           **
**    The name of the setup table is suppled as the argument                 **
**                                                                           **
**    Returns an error flag: 0 means OK, 1 means an error                    **
**    This routine should print the error message                            **
**                                                                           **
\*****************************************************************************/

int camera_read_setup(char *name, char *msg)
{
read_camera_setup(name, msg);	// error message returned in 'name'

return 0;
}



/*****************************************************************************\
**                                                                           **
**    camera_monitor - monitor status of camera                              **
**                                                                           **
**    Called ~once per millisecond irrespective of exposure status, if .     **
**       exposures are controlled by camserver, not by the camera            **
**    Can be used, e.g., to monitor temperature.                             **
**                                                                           **
\*****************************************************************************/

void camera_monitor(void)
{
return;
}



/*****************************************************************************\
**                                                                           **
**    camera_cmnd_filter - potentially modify a command before execution     **
**                                                                           **
**    This can be used, e.g., to communicate with secondary computers        **
**    in a multi-computer detector                                           **
**                                                                           **
\*****************************************************************************/

CAMERA_CMND camera_cmnd_filter(CAMERA_CMND cmnd, char *ptr, int tx)
{
CAMERA_CMND tcmnd=cmnd;


return tcmnd;
}




/*****************************************************************************\
*******************************************************************************
**                                                                           **
**    End of camserver built-in customizable functions                       **
**                                                                           **
*******************************************************************************
\*****************************************************************************/

