// interface_dummy.c - execute basic camera instructions 

// hardware specific for: dummy slsp2det camera

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
** camera_status
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
#include <math.h>
#include "cam_config.h"
#include "tvxcampkg.h"
#include "camserver.h"
#include "debug.h"
#include "cam_tbl.h"
#include "tvxobject.h"
#include "imageio.h"


/*
**  Global variables
*/
int acknowledge_interval=0;
static int imtype=0;



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

int camera_readout(char *img_file, char *msg)
{
int N, len, x, y;
TYPEOF_PIX *imgP;
struct object_descriptor Obj;
struct statfs disk_data;
struct image_info *info=NULL;
char *q, nil[]="(nil)";
char user_string[80];
int multiplicity=0;
double exposure_period=0.005+exposure_time;
double rad, radmax;

msg[0] = '\0';		// default

/*****************************************************************************\
**                                                                           **
**     The code to read an image from the camera goes here.                  **
**     The result is that the image data are in memory in 'theImage'         **
**                                                                           **
\*****************************************************************************/



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
Obj.SignedData = -1;		// data may contain -1, thus signed
Obj.Path = img_file;
Obj.Text = strrchr(img_file, '/');		// for printing below
Obj.Data = (char *)theImage;

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
// these parameters have their own entry in struct info
sprintf(info->detector, "%.*s", DETECT_LENGTH, camera_name);
strncpy(info->date, timestamp(), DATE_LENGTH);
info->exposure_time=exposure_time;

// header string - format as comments
N=2048;
len = strlen(header_string);
if (len<N-30)
	len+=sprintf(header_string+len, "# %s\r\n", camera_pixel_xy);
if (len<N-30)
	len+=sprintf(header_string+len, "# %s\r\n", camera_sensor);
if (multiplicity>nfrmpim && len<N-30)
	len+=sprintf(header_string+len, "### multiplicity = %d\r\n", multiplicity);
if (nfrmpim>1 && len<N-30)
	len+=sprintf(header_string+len, "# N_frames_per_image = %d\r\n", nfrmpim);
if (len<N-30)
	len+=sprintf(header_string+len, "# Exposure_time %.6lf s\r\n", nfrmpim*exposure_time);
if (n_images>1 && len<N-30)
	len+=sprintf(header_string+len, "# Exposure_period %.6lf s\r\n", exposure_period);
else if (len<N-30)
	len+=sprintf(header_string+len, "# Exposure_period %.6lf s\r\n", exposure_time);
if (len<N-30 && tau>0.0)
	len+=sprintf(header_string+len, "# Tau = %.1fe-09 s\r\n", 1.0e9*tau);
else if (len<N-30)
	len+=sprintf(header_string+len, "# Tau = 0 s\r\n");
if (len<N-30)
	len+=sprintf(header_string+len, "# Count_cutoff %d counts\r\n", count_cutoff);
if (len<N-30)
	len+=sprintf(header_string+len, "# Threshold_setting %.0lf eV\r\n", 
			get_current_threshold());
if (len<N-30)
	len+=sprintf(header_string+len, "# Gain_setting %s (vrf = %.3lf)\r\n", 
			get_current_gain(),  get_current_vrf());
if (len<N-30)
	len+=sprintf(header_string+len, "# N_excluded_pixels = %d\r\n", n_bad_pix);

if ( (q=strrchr(bad_pix_file_name, '/')) )
	q++;
else
	q = nil;
if (len<N-LENFN)
	len+=sprintf(header_string+len, "# Excluded_pixels: %s\r\n", q);

if ( (q=strrchr(flat_field_file_name, '/')) )
	q++;
else
	q = nil;
if (len<N-LENFN)
	len+=sprintf(header_string+len, "# Flat_field: %s\r\n", q);

if ( (q=strrchr(trim_directory, '/')) )
	q++;
else
	q = nil;
if (len<N-LENFN)
	len+=sprintf(header_string+len, "# Trim_directory: %s\r\n", q);

if (len<N-80 && user_string[0])
	len+=sprintf(header_string+len, "# Comment: %s\r\n", user_string);

if ( (info->descriptor = malloc(N)) == NULL)
	{
	printf("camera_readout could not allocate memory\n");
	return 1;
	}

info->descriptor_length = strlen(header_string);
if (info->descriptor_length > N-3)
	{
	strncpy(info->descriptor, header_string, N-3);
	strcpy(info->descriptor+N-3, "\r\n");
	info->descriptor_length = N;
	}
else
	strcpy(info->descriptor, header_string);

info->descriptor_length += 
		format_mx_params(info->descriptor+info->descriptor_length,
		N-info->descriptor_length-1);

header_string[0]='\0';

// make up a fake image
Obj.High = IMAGE_NROW;		// known parameters of fake image
Obj.Wide = IMAGE_NCOL;
Obj.Bpp = 8*sizeof(TYPEOF_PIX);
imgP = (TYPEOF_PIX *)Obj.Data;
radmax=sqrt((double)(Obj.Wide/2)*(Obj.Wide/2)+(Obj.High/2)*(Obj.High/2));
switch (imtype)
	{
	case 0:
		for(y=0; y<Obj.High; y++)
			for(x=0; x<Obj.Wide; x++)
				{
				rad=sqrt((double)(x-Obj.Wide/2)*(x-Obj.Wide/2)+(y-Obj.High/2)*(y-Obj.High/2));
				*imgP++ = (TYPEOF_PIX)20000.0*(1.0-rad/radmax);
				}
		break;
	case 1:
		for(y=0; y<Obj.High; y++)
			for(x=0; x<Obj.Wide; x++)
				{
				rad=sqrt((double)(x-Obj.Wide/2)*(x-Obj.Wide/2)+(y-Obj.High/2)*(y-Obj.High/2));
				*imgP++ = (TYPEOF_PIX)20000.0*(rad/radmax);
				}
		break;
	case 2:
		for(y=0; y<Obj.High; y++)
			for(x=0; x<Obj.Wide; x++)
				*imgP++ = (TYPEOF_PIX)20000.0*((double)x/(Obj.Wide-1));
		break;
	case 3:
		for(y=0; y<Obj.High; y++)
			for(x=0; x<Obj.Wide; x++)
				*imgP++ = (TYPEOF_PIX)20000.0*((double)y/(Obj.High-1));
		break;
	case 4:
		for(y=0; y<Obj.High; y++)
			for(x=0; x<Obj.Wide; x++)
				*imgP++ = (TYPEOF_PIX)20000.0*((double)(Obj.Wide-x)/(Obj.Wide-1));
		break;
	case 5:
		for(y=0; y<Obj.High; y++)
			for(x=0; x<Obj.Wide; x++)
				*imgP++ = (TYPEOF_PIX)20000.0*((double)(Obj.High-y)/(Obj.High-1));
		break;
	default:
		for(y=0; y<Obj.High; y++)
			for(x=0; x<Obj.Wide; x++)
				*imgP++ = (TYPEOF_PIX)20000.0;
		break;
	}
imtype++;
imtype %= 6;

/*
**  Now we have our fake image
*/

Obj.OStatus = Writing;

/*
**  Write image file to disk
*/
write_diskimage_det(&Obj, 0, Obj.High*Obj.Wide*Obj.Bpp/8, Obj.Data);
if (Obj.Handle)
	{
	close(Obj.Handle);
	Obj.Handle=0;
	}

/*
** clean up
*/
if (Obj.Handle) close(Obj.Handle);
Obj.Handle = 0;
free(Obj.Header);
set_cam_stat(cam_completed, img_file);

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

int camera_stop(int prio)
{
return 0;
}



/*****************************************************************************\
**                                                                           **
**    camera_telemetry:  read camera telemetry into buffer provided          **
**                                                                           **
\*****************************************************************************/

void camera_read_telemetry(char *bufr, int size)
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
**    camera_status - return the status as a bit-encoded int                 **
**                                                                           **
\*****************************************************************************/

int camera_status(int status)
{
// OR bits as needed

return status;
}



/*****************************************************************************\
**                                                                           **
**    camera_monitor - monitor status of camera                              **
**                                                                           **
**    Called ~once per millisecond irrespective of exposure status.          **
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

