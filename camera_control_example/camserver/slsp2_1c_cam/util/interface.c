// interface.c - execute basic camera instructions 

//changed CB 17.11.03
// hardware specific for: sls8x2 camera

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
#include "debug.h"
#include "tvxcampkg.h"
#include "camserver.h"
#include "tvxobject.h"
#include "imageio.h"
//
#include "ppg_util.h"


#define REAL_HARDWARE

static int using_vpg_timer=False;
static double exposureTime=1.0;


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
#ifdef REAL_HARDWARE
char *p;
int n;
char line[LENFN];
struct object_descriptor Obj;
struct image_info *info=NULL;

msg[0] = '\0';		// default

p = img_file;
if (strstr(p, cam_image_path))
	{
	p += strlen(cam_image_path);		// remove local path write_image_nfs()
	strcpy(line, img_file);				// full path for write_image_pci()
	}
else
	{
	strcpy(line, cam_image_path);
	strcat(line, img_file);				// full path for write_image_pci()
	}
DBG(5, "%s Reading out camera\n", timestamp());

if (n_images)		// multi-image (fine-slice) mode - not done here
	;
else				// single image mode
	{
/*inserted by CB should correspond to tvx readout sequence:
fwrite_control 0x18;fwrite_control 0x1a5;stop;start [baser];cam pwait;
fwrite_control 0x18 */
/* PILATUS2 chip - bottom half read here by pettern; top half read
** by pattern in write_image_nfs() - an ugly kludge
*/
	fifo_write_control(0x18);				// enable timeout
	fifo_write_control(0x1a5);				// clear fifo, no timeout, LVDS->fifo
	vpg_stop_clk(1);					    // sets the default unit
	vpg_set_clk_divider(1, vpg_default_unit); //-- BHe be sure to have the right clock divider
	vpg_set_jump(base_read_chip_pattern1, vpg_default_unit);		// readchip10m 
	vpg_start_clock(vpg_default_unit);
	pgRunning[vpg_default_unit]=True;
	vpg_wait_pattern(1, vpg_default_unit);
	fifo_write_control(0x18);				// enable timeout

	write_image_nfs(p, img_mode);           // readout FIFO
	}

if (strstr(img_file, ".tif"))				// was a tif file requested?
	{
	/*
	**  We build an object descriptor to transport our image.
	**  Do this by rereading the image (it's still in CPU cache),
	**  adding the needed header info, and rewriting.
	**  Start by filling the object descriptor.
	*/

	memset(&Obj, 0, sizeof(struct object_descriptor));
	Obj.Kind = Image;
	Obj.Type = Memory;
	Obj.Role = Destination;
	Obj.Wide = camera_wide;
	Obj.High = camera_high;
	Obj.Bpp = camera_bpp;
	Obj.Path = line;

	if ( (Obj.Data = malloc(camera_wide*camera_high*camera_bpp/8)) == NULL)
		{
		printf("camera_readout could not allocate memory\n");
		return 1;
		}
	if ((Obj.Handle = open(Obj.Path, O_RDONLY)) == -1)
		{
		printf("Could not open %s for reading\n", Obj.Path);
		sprintf(msg, "Could not open %s for reading\n", Obj.Path);
		return 1;
		}
	n = read(Obj.Handle, Obj.Data, camera_wide*camera_high*camera_bpp/8);
	if (Obj.Handle) close(Obj.Handle);
	Obj.Handle = 0;

	if ((Obj.Handle = open(Obj.Path, O_WRONLY | O_CREAT | O_TRUNC,
						S_IRUSR | S_IWUSR |S_IRGRP | S_IWGRP | S_IROTH)) == -1)
		{
		printf("Could not open %s for writing\n", Obj.Path);
		sprintf(msg, "Could not open %s for writing\n", Obj.Path);
		return 1;
		}
	Obj.OStatus = OpenWrite;

	fill_basic_header(&Obj, NULL);				// allocate a header
	info = (struct image_info *)Obj.Header;		// header, ready to use

/*****************************************************************************\
**                                                                           **
**    Fill in header here - telemetry values, etc.                           **
**    Values go into the info structure, defined in tvxobjec.h               **
**    The info is then automatically written to the file.                    **
**                                                                           **
\*****************************************************************************/

	info->exposure_time=exposure_time;

	/*
	**  Write tiff file to disk
	*/
	write_diskimage(&Obj, 0, Obj.High*Obj.Wide*Obj.Bpp/8, Obj.Data);

	/*
	** clean up
	*/
	if (Obj.Handle) close(Obj.Handle);
	Obj.Handle = 0;
	free(Obj.Header);
	free(Obj.Data);
	}
else
	{
	printf("*** Error - only TIFF files supported here\n");
	return -1;
	}


strcpy(img_file, line);		// send full specifier back to caller

return 0;

#else		// not REAL_HARDWARE - fake the image

int inh, n;
char *imgP, line[LENFN];
char *head=NULL;
long head_size;
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
	write_line("***** Disk Is Full *****\n");
	close(Obj.Handle);
	return 1;
	}

fill_basic_header(&Obj, NULL);				// allocate a header
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
**  Write tiff file to disk
*/
head = make_im_head(&Obj, &head_size);
lseek(Obj.Handle , 0 , SEEK_SET);
write(Obj.Handle , head , head_size);
free (head);
write_diskimage(&Obj, 0, Obj.High*Obj.Wide*Obj.Bpp/8, Obj.Data);

/*
** clean up
*/
close(Obj.Handle);
free(Obj.Header);
free(imgP);

#endif		// #ifdef REAL_HARDWARE

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
#ifdef SP8_BL38B1
cm_shutter(state);	// 0 = close; else open
#endif

return;
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
char bufr[4096];
int err = 0;

init_trim_array(-1);

		// this is done after cam_config_initialize(), which calls 
		// camera_read_setup, and then sets up camstat.  So, this 
		// must be placed here.

camera_read_telemetry(bufr, sizeof(bufr));
set_cam_stat(cam_telemetry, bufr);

#ifdef SP8_BL38B1
err = connect_ra4();
#endif

return err;
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
#ifdef REAL_HARDWARE

exposureTime=*exp_time;		// make a local record

#ifdef SLS8x2_CAMERA

if ( ((*exp_time*20.0)-((int)(*exp_time*20.0))) > 0.01)
	{
	*exp_time = ((int)(0.99+*exp_time*20.0))/20.0;
	printf("Exposure time adjusted to %.3f s (a multiple of 0.05 is required)\n",
			*exp_time);
	}

DBG(5, "%s Preparing SLS8x2 camera\n", timestamp());

/*inserted CB 18.11.03 */
strcpy(line, "CHSEL 0xffff"); //sel all chips
prog_i2c(line);
vpg_execute_pattern(thePattern, patternP-thePattern, i2c_vpg_unit);

//clear module
vpg_set_clk_divider(1, vpg_default_unit);
fifo_write_control(0x18);				// enable timeout
fifo_write_control(0x1a5);				// clear fifo, no timeout, ECL->fifo
vpg_stop_clk(1);						// sets the default unit
vpg_set_jump(base_read_chip_pattern, vpg_default_unit);		// readchip10m - clear chips
vpg_start_clock(vpg_default_unit);
pgRunning[vpg_default_unit]=True;
vpg_wait_pattern(1, vpg_default_unit);
fifo_write_control(0x18);				// enable timeout

img_mode = 1;							// set for x-rays, not pulses

#endif		// SLS8x2_CAMERA

#else		// REAL_HARDWARE
exposureTime=*exp_time;		// make a local record
#endif		// REAL_HARDWARE

return 0;
}



/*****************************************************************************\
**                                                                           **
**    camera_start:  start integrating or counting                           **
**                                                                           **
**    A ponter to the exposure time is passed as the agument, should         **
**    it be needed.                                                          **
**                                                                           **
**    Returns an error flag: 0 means OK, 1 means an error                    **
**    This routine should print the error message                            **
**                                                                           **
\*****************************************************************************/

int camera_start(double *exp_time)
{
// a global copy or the exposure time was made in camera_prepare()
#ifdef REAL_HARDWARE

char img_file[150], *p;
// from prepare (above), we have vpg 1 and clock running
printf("  %s Starting camera\n", timestamp());
DBG(5, "%s Starting camera\n", timestamp());

if (n_images)		// multi-image (fine-slice) mode
	{
	using_vpg_timer=False;					// just to be sure
	if (image_file[0] == '/')				// full path given
		strcpy(img_file, image_file);
	else									// add a path
		{
		strcpy(img_file, cam_image_path);
		strcat(img_file, image_file);
		}
	if ( (p=strrchr(img_file, '_')) )
		*(p+1) = '\0';						// remove file number from GUI
	else
		strcat(img_file, "_");

	write_n_images_nfs(p, n_images, 0);		// exp. time not used

	}
else		// single exposure mode
	{
	if (*exp_time <= 12.5)	// use vpg timer
		{
		//-- BHe 08-10-04- derived from camera_start function, using vpg timer
		unsigned int data; 
		unsigned int addr = base_vpg_ena_pattern+0x0001;
		long long dataL = 0x79f10004;

		data = (unsigned int)(*exp_time*20.0);
		if(data<1)
			{
  			printf("Using 0.05 sec minimum exposure time\n");
  			data = 1;
			}
		data = (unsigned int)dataL+0x100*data;
		//-- poke new data to addr
		//-- vpg clock should be stopped for this operation
		addr = addr<<2;
		vpg_stop_clk(1);
		vpg_write(addr, data, vpg_default_unit);
		vpg_set_clk_divider(15, vpg_default_unit); //set slowest possible clock 
		vpg_set_jump(base_vpg_ena_pattern, vpg_default_unit);		// enable 
		vpg_start_clock(vpg_default_unit);//--restart vpg clock and jump to start address
		using_vpg_timer=True;
		}
	else		// use system software timer
		{
		vpg_set_jump(base_enable_pattern, vpg_default_unit);		// enable
		using_vpg_timer=False;
		}
	}

n_images = 0;

#endif

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
#ifdef REAL_HARDWARE
// from prepare (above), we have vpg 1 and clock running
if (using_vpg_timer)		// look for 'wait' instruction in vpg
	{
	vpg_wait_pattern(1, vpg_default_unit);
	DBG(5, "%s Stopping camera\n", timestamp());
	using_vpg_timer=False;
	}
else				// software timer
	{
	DBG(5, "%s Stopping camera\n", timestamp());
	vpg_set_jump(base_disable_pattern, vpg_default_unit);		// disable
	}
#endif

return 0;
}



/*****************************************************************************\
**                                                                           **
**   camera_read_telemetry:  read camera telemetry into buffer provided      **
**                                                                           **
\*****************************************************************************/

void camera_read_telemetry(char *bufr, int size)
{
char line[50];
int n;
time_t t = time(NULL);

strftime(line, 50, "%X %x", localtime(&t) );
n = sprintf(bufr, " - * Telemetry at %s * -\n", line);

if (size < 2048)
	return;

#ifdef SLSDET_CAMERA
n+= sprintf(bufr+n, "image format: %d(w) x %d(h) pixels\n",
	IMAGE_NCOL, IMAGE_NROW);
n+= sprintf(bufr+n, "selected bank: %d\n", selected_bank);
n+= sprintf(bufr+n, "selected module: %d\n", selected_mod);
n+= sprintf(bufr+n, "selected chip: %d\n", selected_chip);
#endif
#ifdef SLS8x2_CAMERA
n+= sprintf(bufr+n, "selected chip: %d\n", selected_chip);
#endif


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
if (0x80 | vpg_read(0x1ffe8, vpg_default_unit))		// is #end-of-pattern set?
	return 0;		// not set
else
	return 1;		// set
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
#ifdef SP8_BL38B1
disconnect_ra4();
#endif

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
read_camera_setup(name, msg);	// error message returned in 'msg'
xor_tab_initialize(-1);			// reinitialize - data polarity may have changed

if (*msg)
	return -1;
else
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

CAMERA_CMND camera_cmnd_filter(CAMERA_CMND cmnd, char *ptr)
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

