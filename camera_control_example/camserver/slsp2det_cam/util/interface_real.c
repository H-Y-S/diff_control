// interface.c - execute basic camera instructions 

/*
**  ***  For SLSP2DET_CAMERA systems with PMC gigastar dcb  ***
*/

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



#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <sys/vfs.h>
#include <sys/time.h>
#include <sys/poll.h>
#include <sys/socket.h>
#include <netdb.h>
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>
#include <math.h>
#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <sched.h>
#include "detsys.h"

#if defined MASTER_COMPUTER || defined SLAVE_COMPUTER
 #define CAMERA_MAIN			// access camera_list[]
 #include "camserver.h"
 #undef CAMERA_MAIN
#else
 #include "camserver.h"
#endif

#include "debug.h"
#include "tvxcampkg.h"
#include "cam_config.h"
#include "tvxobject.h"
#include "imageio.h"
#include "gslib.h"
#include "dcblib.h"
#include "dcb.h"
#include "time_log.h"




/*
**   MAXPROC biases against unlimited process count by throwing in wait
** time in proportion to the number of process beyond the specified maximum.
**
** Suggested value: (total memory) / (3 * image size) for big images;
** ~1500 for small images.  
**
** If the system is going to miss readouts (as it will when it is heavily
** overloaded), an appropriate value of MAXPROC makes the missed readouts
** be relatively evenly distributed, rather than all bunched up in the 
** later images (e.g., many images with a multiplicity of 2, rather than
** a few images with multiplicity 50).
*/

#define REAL_HARDWARE
#define USE_FORK
//#define LOG_IMAGES


/*
** prototypes of local functions
*/
#ifdef USE_FORK
static __sighandler_t endWriteProcess(int, siginfo_t *, void *);
#endif

static void monitor_temp_humid(void);
static int setup_other_computers(void);
static int socket_service(void);
#ifdef MASTER_COMPUTER
static int tell_other_computer(int, CAMERA_CMND, char *);
static int tell_all_other_computers(CAMERA_CMND, char *);
#endif

// rotating directory system
static int setupDirs(char *, char *);
static void nextFileName(int, int);
static int cleanupDirs(void);
static char *name2w, name2l[LENFN];			// name to write, name to log


/*
**  Global variables
*/
int external_enable_mode=0;
int external_trigger_mode=0;
int external_multi_trigger_mode=0;
int kill_mutiple_images=0;
int acknowledge_interval=0;
double trigger_offset[DET_NDCB];
static double min_delay=0.0, max_delay=0.0;
static double exp_start_time;
static double exposure_period;

static int n_cpus=0;
static int next_cpu=0;
static char *theData;
static unsigned int monitor_count=0;
static int last_hour=-1;
static int last_min=-1;
static int last_exposure=0;
static unsigned int multiplicity;
static unsigned int nexppf=1;
static int longsleep=0;
static int camera_killed;

#ifdef MASTER_COMPUTER
 static int sockets[N_COMPUTERS]={0};	// socket connections to other computers
 static struct pollfd sfds[N_COMPUTERS-1] = { {0} };
 static char sfdBufr[500];
#endif

#ifdef USE_FORK
 static int pcnt;					// process count
 static int ns;						// number of sleeps
 static int pcmax;					// maximum number of processes
 static int process_wait_flag;		// wait for processes to finish
 static struct sigaction receiveSignal = 
			 {{(__sighandler_t)endWriteProcess}, {{0}}, SA_SIGINFO, NULL};
 static struct sigaction prevAction;
#endif


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
int N, nerr=0, len;
struct object_descriptor Obj;
struct image_info *info=NULL;
double est_end_time, met;
char *q, nil[]="(nil)";
char user_string[80];
struct sched_param sp;
#ifdef USE_TMPFN
char tmpfn[4+LENFN];
#endif

#if defined LOG_IMAGES || defined USE_FORK || defined MASTER_COMPUTER
char line[LENFN];
#endif

#if defined MASTER_COMPUTER || defined USE_FORK
struct stat buf;

#endif

#if defined MASTER_COMPUTER
double ts = gs_time();
int res, bank, mod;
char *p;
 #ifdef WIDE_FAST
  int y, offset=0;
 #endif
#endif

#ifdef LOG_IMAGES
FILE *lfp;
#endif

#ifdef USE_FORK
char line2[LENFN];
int i, j, k, fd;
pid_t pid;
DIR *dir;
struct dirent *ent;
cpu_set_t cpuset;
#endif

msg[0] = '\0';				// default

if (img_mode && (multiplicity<-1+nfrmpim))		// if multi-frame image
	{
	dcb_wait_read_image(&theData);
	multiplicity += dcb_get_exposures_in_frame()/nexppf;	// number of exps in frame
	convert_image_add(theData);		// convert multi-frame image
	return 0;
	}

// wait for exposure to finish, then read
est_end_time = dcb_wait_read_image(&theData);
if (last_exposure)
	dcb_reset_exposure_mode();
if (est_end_time < 0.0)
	{
	printf("*** Error in reading image\n");
	DBG(1, "*** Error in reading image\n");
	multiplicity = 0;
	return -1;
	}
// multiplicity is zero in a readout without exposure
multiplicity += dcb_get_exposures_in_frame()/nexppf;	// number of exps in frame
/*
** best approximation of exposure end time
** N.B. camserver timings (startTime, endTime) are on a different scale
** from dcblib.c timings, so we compute endTime from the difference.
*/
endTime = startTime +(est_end_time - exp_start_time) + max_delay;
if (endTime<startTime)
	endTime=startTime;

if ( (nfrmpim==1) && ((met=dcb_get_measured_exposure_time()) > 0.0)
		&& n_images<=20 && nexppf==1)
	{		// backspaces to overwrite a possible countdown in progress
	printf("\b\b\b\b\b\b\b  Measured exposure time: %.8lf s\n", met);
	DBG(3, "  Measured exposure time: %.8lf s\n", met);
	}

if (n_images<=1)				// single image mode
	{
	DBG(2, "%s (interface.c) Reading out camera: %s\n",
			gs_timestamp(), 1+strrchr(img_file, '/'))
#ifdef LOG_IMAGES
	strcpy(line, img_file);
	*(1+strrchr(line, '/')) = '\0';
	strcat(line, "images.log");
	if ( (lfp=fopen(line,"a")) )
		{
		fprintf(lfp, "%.4lf  %.4lf  %s  %s\n", 
				exposure_time, endTime-startTime, gs_timestamp(), img_file);
		if (endTime-startTime > 1.01*exposure_time)
			fprintf(lfp, "### - possible long exposure\n");
		fclose(lfp);
		}
#endif
	}

#ifdef USE_FORK
// limit the number of simultaneous forks
if (pcnt>MAXPROC)	// we hope this never happens - but it does
	{
	j=10000*(pcnt-MAXPROC);
	usleep(j);		// give away tens of milliseconds
	ns+=j;
	}
pcnt++;
pcmax = pcnt>pcmax ? pcnt : pcmax;

// set up for reassigning processor affinity
next_cpu = (++next_cpu>=n_cpus) ? 1 : next_cpu;		// do not use 0

time_log(0, "before fork");
// measured 3.3 ms (max 5.0) to set up fork
pid = fork();	// fork a process to do the writing
switch(pid)
	{
	case -1:
		printf("Error in write_image_to_disk: fork could not allocate memory\n");
		return -1;
	case 0:
		break; 	// child process - go do the write
	default:					// parent process - update status
		longsleep=0;			// flag used by calibrate
		if (process_wait_flag)	// last exposure of series
			{
			// wait for all the processes to exit - 3 sec max
			k = pcnt;
			DBG(5, "Start of sleeps - pcnt = %d, %s\n", pcnt, gs_timestamp())
			for(i=0; i<3 && pcnt>1; i++)
				sleep(1);
			DBG(5, "End of sleeps - pcnt = %d, %s\n", pcnt, gs_timestamp())
			for(i=0; i<pcnt; i++)
				waitpid(-1, &j, WNOHANG);	// reap all zombie processes
				// restore signal
			if ( (sigaction (SIGCHLD, &prevAction, NULL)) == -1)
				{
				perror("sigaction restore");
				return -1;
				}
			if (ns)
				printf("  Used %d milliseconds of usleep(3) to control process count\n", ns/1000);
			if (pcmax>1)
				printf("  We got up to %d processes\n", pcmax);
			if (k>1)
				printf("  Sequence ended with %d processes still outstanding\n", k);
			process_wait_flag=0;

			// clean up rotating directories, if applicable
			cleanupDirs();

			// restore normal scheduling policy
			sp.sched_priority = 0;		// cf. note in camera_start()
			sched_setscheduler(0, SCHED_OTHER, &sp);
			}
		return 0;					// parent process - back to caller
	}

// child process - release unneeded file descriptors
close(ufds[0].fd);
close(ufds[1].fd);
#ifdef MASTER_COMPUTER
close(sfds[0].fd);
#endif

// reset processor affinity for this new process
if (n_cpus > 2)			// e.g., 6M system
	{
	CPU_ZERO(&cpuset);
	CPU_SET(next_cpu, &cpuset);
	sched_setaffinity(0, sizeof(cpu_set_t), &cpuset);
	DBG(5, "Set process %d affinity to CPU %d\n", (int)getpid(), next_cpu);
	}
else if (n_cpus > 1)	// for 100K systems, use both cpus
	{
	CPU_ZERO(&cpuset);
	CPU_SET(0, &cpuset);
	CPU_SET(1, &cpuset);
	sched_setaffinity(0, sizeof(cpu_set_t), &cpuset);
	DBG(5, "Set process %d affinity to both CPUs\n", (int)getpid());
	}
#endif

/*
** Decrease the priority of this process to favor the main process.
** This slows the cleanup at the end of a series.
** Do not do nice(2) in single-image mode - it makes the image late
*/
sp.sched_priority = 0;		// cf. note in camera_start()
sched_setscheduler(0, SCHED_OTHER, &sp);
sched_yield();
if (n_images>1)
	nice(2);

/*
** Convert the data to an image.
**
** On a single-processor (single core) system, this conversion should be
** done in dcblib.c to save some time.  On a multi-processor system, this
** algorithm moves the time-consuming image conversion onto a CPU different
** from the detector-control process.  See note in dcblib.c
*/
if (kill_mutiple_images && multiplicity>1)
	initialize_theImage(gap_fill_byte);
else if (nfrmpim > 1)
	{
	convert_image_add(theData);
	gap_fill_image();
	}
else
	convert_image(theData);		// convert with immediate gap-fill

#ifdef MASTER_COMPUTER
// gather sub-image data from secondary computers
// look in /compB_images - only 1 slave for now

strcpy(line, "/compB_images");
p = 1+strrchr(img_file, '/');
q = strchr(p, '.');
strcat(line, "/B_");
strncat(line, p, q-p);
strcat(line, ".cbf");

// it appears that we must sleep() before stat() is called the first time
// otherwise stat() does not see the file for up to 15 s.(?!)  But, the 
// sched_yield() seems to help (usually)
//sleep(1);
//if (process_wait_flag)		// on last image we need extra time?
//	sleep(1);
//else if (longsleep)
if (longsleep)
	{
	sleep(2);				// needed for calibrate & vdelta
	longsleep=0;			// needed if no fork
	}

if (trigger_offset[0] >= 0.0)
	{
	ts=gs_time();
	while (gs_time() < ts+max_delay-min_delay)
		usleep(10000);
	}

usleep(20000);
sched_yield();
ts=gs_time();
res = stat(line, &buf);
DBG(5, "%s: waiting for %s to appear\n", gs_timestamp(), line)
// This loop is used only for 0.1% of images, but then the delay is ~1 sec.
// If the loop is not entered, time delay is ~100 us.
// wait up to 15 s for image - longer if period is huge
while (res && (gs_time() < ts+5.0+(10.0>exposure_period ? 10.0 : exposure_period)))
	{
	usleep(10000);
	sched_yield();
	system("ls /compB_images >/dev/null");	// clear stat() cache?
	res = stat(line, &buf);
	}
if (res)
	{
	printf("*** Did not find %s\n", line);
	DBG(1, "*** Did not find %s:\n  %s\n", timestamp(), line)
	DBG(5, "%s\n", gs_timestamp())
	return -1;
	}

// pick up image data
memset(&Obj, 0, sizeof(struct object_descriptor));
Obj.Kind = Image;
Obj.Type = Memory;
Obj.Role = Source;
Obj.Path = line;
if ((Obj.Handle = open(Obj.Path, O_RDONLY)) < 2)
	{
	printf("*** Could not open %s\n", Obj.Path);
	DBG(1, "*** Could not open %s\n", Obj.Path)
	return -1;
	}
if (get_im_head(&Obj))
	{
	printf("*** Error in header: %s\n", Obj.Path);
	DBG(1, "*** Error in header: %s\n", Obj.Path)
	return -1;
	}
if (read_diskimage_cbf_raw(&Obj, 0, 0, NULL))
	{
	printf("*** Error reading data: %s\n", Obj.Path);
	DBG(1, "*** Error reading data: %s\n", Obj.Path)
	return -1;
	}

// transfer data to image under construction
#ifdef WIDE_FAST
 bank=1;
 mod=2;
 p=Obj.Data;
 px=pix_add(-1,-1,-1,bank,-1,-1,mod,0,0,-1,-1,-1);
 for (y=0; y<NROW_DET; y++, offset+=NCOL_DET, p+=Obj.Wide*Obj.Bpp/8)
	 memcpy(theImage+px->pix+offset, p, Obj.Wide*Obj.Bpp/8);
#else
 bank=2;
 mod=1;
 px=pix_add(-1,-1,-1,bank,-1,-1,mod,0,0,-1,-1,-1);
 memcpy(theImage+px->pix, Obj.Data, Obj.Wide*Obj.High*Obj.Bpp/8);
#endif		// WIDE_FAST

free(Obj.Data);
close(Obj.Handle);
unlink(Obj.Path);		// delete temporary sub-image

#endif		// MASTER_COMPUTER

/*
**  Build an object descriptor to transport our image.
*/
memset(&Obj, 0, sizeof(struct object_descriptor));
Obj.Kind = Image;
Obj.Type = Memory;
Obj.Role = Destination;
Obj.Wide = camera_wide;
Obj.High = camera_high;
Obj.Bpp = camera_bpp;
Obj.SignedData = -1;		// data may contain -1, thus signed
Obj.Text = strrchr(img_file, '/');		// for printing below
Obj.Data = (char *)theImage;
#ifdef USE_TMPFN
strcpy(tmpfn, img_file);		// write a temporary file, then rename
strcat(tmpfn, ".tmp");
Obj.Path = tmpfn;
#else
Obj.Path = img_file;
#endif

time_log(0, "before open");		// if not active, same as gstime()
if ((Obj.Handle = open(Obj.Path, O_WRONLY | O_CREAT | O_TRUNC,
					S_IRUSR | S_IWUSR |S_IRGRP | S_IWGRP | S_IROTH)) == -1)
	{
	printf("*** Could not open %s for writing\n", Obj.Path);
#ifdef USE_FORK
	exit(1);
#else
	sprintf(msg, "*** Could not open %s for writing\n", Obj.Path);
	return 1;
#endif
	}
Obj.OStatus = OpenWrite;

fill_basic_header(&Obj, NULL);					// allocate a header
info = (struct image_info *)Obj.Header;			// header, ready to use

/*****************************************************************************\
**                                                                           **
**    Fill in header here - telemetry values, etc.                           **
**    Values go into the info structure, defined in tvxobjec.h               **
**    The info is then automatically written to the file.                    **
**                                                                           **
\*****************************************************************************/
// these parameters have their own entry in struct info
sprintf(info->detector, "%.*s", DETECT_LENGTH, camera_name);
strncpy(info->date, gs_timestamp(), DATE_LENGTH);
info->exposure_time=exposure_time;


user_string[0]='\0';
header_string[80-13]='\0';		// 13 chars used by system
q = header_string;
if (*q=='#')
	q++;
if (*q==' ')
	q++;
strcpy(user_string, q);
header_string[0]='\0';
// header string - format as comments
// lines should be <80 columns, or change the header reader
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

Obj.OStatus = Writing;

/*
** Write the image file of desired type
*/
nerr=write_diskimage_det(&Obj, 0, Obj.High*Obj.Wide*Obj.Bpp/8, Obj.Data);

#ifdef USE_TMPFN
rename(tmpfn, img_file);
Obj.Path = img_file;
DBG(5, "rename %s to %s at %s\n", 1+strrchr(tmpfn, '/'), 
		strrchr(img_file, '/'), gs_timestamp());
#endif

if (Obj.Handle)
	{
	close(Obj.Handle);
	Obj.Handle=0;
	}
DBG(3, "image %s completed: %s\n", 1+strrchr(Obj.Path, '/'), gs_timestamp())
set_cam_stat(cam_completed, img_file);

#ifdef USE_FORK
// see documentation in im_moves.c for this section

//sleep(1);		// pause before closing all??
usleep(5000);	// helps performance at 17 Hz

strcpy(line, "/proc/");
sprintf(line+strlen(line), "%d", (int)getpid());
strcat(line, "/fd/");
dir = opendir(line);
while( (ent=readdir(dir)) )
	{
	if (*ent->d_name == '.')		// skip . & ..
		continue;
	strcat(line, ent->d_name);
	stat(line, &buf);
	if ( S_ISREG(buf.st_mode) )
		{
		lstat(line, &buf);
		if((buf.st_mode & S_IRUSR) && !(buf.st_mode & S_IWUSR) )	// read & not write?
			{
			sscanf(ent->d_name, "%d", &fd);
			line2[readlink(line, line2, sizeof(line2))] = '\0';		// readlink does not terminate string
			DBG(7, "(interface.c) child process closing fd %d = %s\n", fd, line2)
			close(fd);
			}
		}
	*(1+strrchr(line, '/')) = '\0';
	}
closedir(dir);
DBG(5,"(interface.c) write_image_to_disk (...%s) - done: nerr = %d\n", Obj.Text, nerr)
exit(0);

#else			// not using fork
if (Obj.Handle) close(Obj.Handle);
Obj.Handle = 0;
Obj.Role = UnknownRole;	// prevent accidental reutilization as a destination
Obj.OStatus = Ready;	// signal that data are on disk
Obj.Data = NULL;		// points to theImage - do not use free()
delete_header(&Obj);
return 0;

#endif


// ----------------------------------------------------------------------------------

#if 0		// not REAL_HARDWARE - fake the image

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
statfs(Obj.Path, &disk_data);		// we already know it is there
if ( disk_data.f_bavail <
		(Obj.Wide * Obj.High * Obj.Bpp)/ (8 * disk_data.f_bsize) )
	{
	printf("***** Disk Is Full *****\n");
	if (Obj.Handle) close(Obj.Handle);
	Obj.Handle = 0;
	return 1;
	}

fill_basic_header(&Obj);					// allocate a header
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
write_diskimage_det(&Obj, 0, Obj.High*Obj.Wide*Obj.Bpp/8, Obj.Data);

/*
** clean up
*/
free (head);
if (Obj.Handle) close(Obj.Handle);
Obj.Handle = 0;
delete_header(&Obj);
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
int err = 0, i;
cpu_set_t cpuset;

// set cpu affinity for SMP systems - count the number of CPUs
sched_getaffinity(0, sizeof(cpu_set_t), &cpuset);
if (n_cpus==0)
	for(i=0; i<CPU_SETSIZE; i++)
		if ( CPU_ISSET(i, &cpuset) )
			n_cpus++;

// this process must run on a single CPU because of the timers - use 0
CPU_ZERO(&cpuset);
CPU_SET(0, &cpuset);
sched_setaffinity(0, sizeof(cpu_set_t), &cpuset);
DBG(3, "Found %d CPUs\n", n_cpus);

xor_tab_initialize(-1);
init_trim_array(-1);
initialize_theImage(0);

	// this (camera_initialize()) is done after cam_config_initialize(),
	// which calls camera_read_setup(), and sets up camstat.  So, this 
	// (init_trim_array())must be placed here.

DBG(3, "(interface.c) camera_initialize: pid = %d\n", getpid())

printf("%s\n", camera_name);
if ( (err = dcb_initialize(camera_name)) )
	{
	printf("Bad return from dcb_initialize()\n");
	return err;
	}

if ( (err = setup_other_computers()) )		// a no-op if only 1 computer
	{
	printf("Bad return from setup_computers()\n");
	return err;
	}

camera_read_telemetry(bufr, sizeof(bufr));
set_cam_stat(cam_telemetry, bufr);

sprintf(bufr, "B01_M01");

#ifdef SLAVE_COMPUTER
 #ifdef WIDE_FAST
  sprintf(bufr, "B01_M02");			// SLAVE_COMPUTER - hard coded bank for now
#else
  sprintf(bufr, "B02_M01");
 #endif
#endif
set_i2c(bufr);						// set up defaults
dcb_set_exposure_time(exposure_time);
dcb_set_exposure_period(0.005+exposure_time);
dcb_set_tx_frame_limit(1);
dcb_set_exposure_count_limit(1);
nfrmpim = 1;						// N frames per image
for (i=0;i<NDCB; i++)
 	dcb_set_exp_trigger_delay(0.0, i);
for (i=0;i<DET_NDCB; i++)
	trigger_offset[i] = -1.0;		// turn off dcb trigger offset mode.

#ifdef MASTER_COMPUTER
 #ifdef WIDE_FAST
  printf("Master computer supporting 1 bank fast detector\n");
  DBG(1, "Master computer supporting 1 bank fast detector\n")
 #else
  printf("Master computer supporting %d bank detector\n", NBANK);
  DBG(1, "Master computer supporting %d bank detector\n", NBANK)
 #endif
#endif
#ifdef SLAVE_COMPUTER
system("rm -f /home/det/p2_det/images/*");	// start with a clean slate
 #ifdef WIDE_FAST
  printf("Slave computer supporting modules %d through %d\n",
			1+SLAVE_COMPUTER, NMOD_BANK+SLAVE_COMPUTER);
  DBG(1, "Slave computer supporting modules %d through %d\n",
			1+SLAVE_COMPUTER, NMOD_BANK+SLAVE_COMPUTER)
 #else
  printf("Slave computer supporting banks %d through %d\n",
			1+SLAVE_COMPUTER, NDCB+SLAVE_COMPUTER);
  DBG(1, "Slave computer supporting banks %d through %d\n",
			1+SLAVE_COMPUTER, NDCB+SLAVE_COMPUTER)
 #endif
#endif

printf("\n... Camera image format: %d(w) x %d(h) pixels\n", IMAGE_NCOL, IMAGE_NROW);
DBG(1, "Camera image format: %d(w) x %d(h) pixels\n", IMAGE_NCOL, IMAGE_NROW)

// sanity check
if (strlen(DETECTOR) > DETECT_LENGTH)
	{
	printf("DETECTOR name string (detsys.h) is too long\n");
	err=-1;
	}

// set up to put DECTRIS logo in each module
#ifdef ENABLE_DECTRIS_LOGO
memset(dectris_logo, 0, MOD_SIZE*sizeof(TYPEOF_PIX));
if ( (i=open("../../test/images/dectris1.tif", O_RDONLY)) > 2)
	{
	lseek(i, 4096, SEEK_SET);
	read(i, dectris_logo, MOD_SIZE*sizeof(TYPEOF_PIX));
	close(i);
	}
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
char line[50];

strcpy(imgfile, image_file);	// make sure these agree

#ifdef REAL_HARDWARE

img_mode = 1;					// set for x-rays, not pulses
camera_killed = 0;

DBG(4, "%s Preparing SLSP2DET camera\n", timestamp())
//printf("interface.c: camera_prepare\n");


// select all chips ??
// using calls to prog_i2c() allows the pattern to be recorded.
// do not call dcb_prog_i2c() directly here
strcpy(line, "B15_M7_CHSEL 0xffff");
prog_i2c(line);
initialize_theImage(gap_fill_byte);

// make sure dcb fifo & gs dma are clear
dcb_stop();						// kills dma
dcb_reset_exposure_mode();
dcb_set_multi_trigger(0);

#else		// REAL_HARDWARE

printf("Preparing NULL camera\n");

#endif		// REAL_HARDWARE

return 0;
}



/*****************************************************************************\
**                                                                           **
**    camera_start:  start integrating or counting                           **
**                                                                           **
**    A pointer to the exposure time is passed as the agument, should        **
**    it be needed.                                                          **
**                                                                           **
**    Returns an error flag: 0 means OK, 1 means an error                    **
**    This routine should print the error message                            **
**                                                                           **
\*****************************************************************************/

int camera_start(double *exp_time)
{
char *p, *q, msg[80+LENFN], *logb, *lbP, ext[10], line[2*LENFN];
char mode[4], base_name[80];
int i, nmissed=0, ret, sn, fmt, go, flag;
double dt=0.0;
double delay_time = dcb_get_exp_trigger_delay(0), frame_time, tdiff;
FILE *lfp;
struct sched_param sp;
struct timeval tv;
struct timezone tz;
struct statfs disk_data;

if (!external_enable_mode)
	xor_tab_initialize(-1);		// if rate correction parameters have changed

//printf("  %s Starting camera\n", gs_timestamp());
DBG(3, "##### --- %s Starting camera ------------------\n", gs_timestamp())

exposure_period=dcb_get_exposure_period();
if (n_images>1 && delay_time>=exposure_period)
	{
	printf("*** ERROR - delay time (%.6lf) is too long\n", delay_time);
	DBG(1, "*** ERROR - delay time (%.6lf) is too long\n", delay_time);
	return -1;
	}
p = image_file;
while(*p)
	{
	if ( !(isalnum(*p) || *p=='/' || *p=='_' || *p=='-' || *p=='.') )
		{
		printf("*** ERROR - illegal file name\n  %s\n", image_file);
		DBG(1, "*** ERROR - illegal file name\n  %s\n", image_file)
		return -1;
		}
	p++;
	}

// camera_prepare() has reset the DCB

nexppf = dcb_get_exposure_count_limit();		// unsigned int

if (nexppf <= 0)
	{
	printf("*** ERROR - nexppf == 0 from DCB\n");
	DBG(1, "*** ERROR - nexppf == 0 from DCB\n")
	return -1;
	}

if (external_enable_mode && (external_trigger_mode || 
		external_multi_trigger_mode))		// sanity check
	{
	printf("*** ERROR - cannot have both ExtE and ExtT or ExtMT\n");
	DBG(1, "*** ERROR - cannot have both ExtE and ExtT or ExtMT\n")
	dcb_reset_exposure_mode();
	return -1;
	}

/*
** Camserver timings (startTime, endTime) are on a scale derived from
** gettimeofday() whereas dcblib uses timings from gs_time().  These
** scales are offset by the number of seconds from the 'epoch'.  Since
** calls to gettimeofday() are slow, we calculate a compensation.
*/
gettimeofday(&tv, &tz);
gs_set_t0(0.0);			// synchronize clocks
tdiff = gs_time() - ((double)tv.tv_sec + (double)tv.tv_usec/1e6);
DBG(5, "Timer setup: time = %s, tn = %.6lf\n", gs_timestamp(), gs_time())

/*
** Evaluate disk space for the proposed operation
*/
strcpy(line, image_file);
if ( (p=strrchr(line, '/')) )
	*p='\0';
if (statfs(line, &disk_data))	// returns nonsense if the path does not exist
	{
	printf("Cannot stat %s\n", image_file);
	DBG(1, "Cannot stat %s\n", image_file);
	return -1;
	}
// If "disk" is tmpfs, assume the user is using a ramdisk as a buffer
// and the size is irrelevant.  The test below assumes uncompressed images.
#define TMPFS_MAGIC 0x01021994		// from statfs(2)
if ( (disk_data.f_type != TMPFS_MAGIC) && ((disk_data.f_bavail - 150*n_images) <
		n_images * ((camera_wide * camera_high * camera_bpp) / (8 * disk_data.f_bsize))) )
	{
	printf("***** Insufficient Disk Space *****\n");
	DBG(1, "***** Insufficient Disk Space *****\n");
	return -1;
	}

// calculate the spread of individual DCB delays & bail if too high
min_delay = max_delay = 0.0;
if (trigger_offset[0] >= 0.0)
	{
	min_delay = trigger_offset[0];
	for(i=0; i<DET_NDCB; i++)
		max_delay += trigger_offset[i];
	}
if (max_delay-min_delay > 4.0)
	{
	printf("*** Spread in delays too great: %lf sec.\n",
			max_delay-min_delay);
	DBG(3, "*** Spread in delays too great: %lf sec.\n",
			max_delay-min_delay);
	return -1;
	}


DBG(1, "Images: %s\n", image_file);

if (n_images>1)		// multi-image ("fine-slice") mode
	{
/*
** to get the ultimate performance, we attempt to change the scheduling
** policy to SCHED_FIFO.  This requires either (a) running as root 
** (possibly invoking sudo from a script); or (b) a hacked kernel in which
** sched_setscheduler() in kernel/sched.c is modified.  For this, I changed
** 'if (user && !capable(CAP_SYS_NICE)) {' to 'if (0) {'  (near line 5087 in 
** linux-2.6.27.39).  If the requested change in scheduling policy is 
** denied, the process simply runs slower & less reliably.
**
** The newer machines are so fast that this may no longer be necessary,
** but it is hard to predict behavior in a real, heavily loaded situation.
**
** But, the process priority does not carry through to DMA priority.  DMA
** is first come, first served; so as the load increases, the bus eventually
** chokes.
*/
	sp.sched_priority = 99;		// for RT kernel
	ret = sched_setscheduler(0, SCHED_FIFO, &sp);
	ret = sched_getscheduler(0);
	if (ret == SCHED_FIFO)
		{
		DBG(3, "Set SCHED_FIFO scheduling policy, priority %d\n",
				sp.sched_priority);
		}
	else if (exposure_period < 0.2)		// priority not really needed
		{
		printf("*** Unable to set Real-Time scheduling policy\n");
		DBG(1, "*** Unable to set SCHED_FIFO scheduling policy\n")
		}

    // clear the detector - why does this spoil the first image?
//	dcb_start_read_image();
//	dcb_wait_read_image(&theData);

	// set up for external enable or external trigger , if requested
	if (external_enable_mode)
		dcb_set_external_enb(0);
	else if (external_trigger_mode)
		dcb_set_external_trigger(0);
	else if (external_multi_trigger_mode)
		dcb_set_multi_trigger(1);

	// calculate maximal delay if DCBs have differing delays
	if (trigger_offset[0] >= 0.0)
		for (i=0; i<DET_NDCB; i++)
			delay_time = max(delay_time, trigger_offset[i]);

	// set up dcb parameters
	frame_time = nexppf*exposure_period;
	dcb_set_tx_frame_limit(n_images*nfrmpim);
	if (dcb_external_enb())
		{
		dcb_set_exp_trigger_delay(0, -1);
		printf("  N enb/frame = %d, N frame/image = %d, N img = %d\n",
				nexppf, nfrmpim, n_images);
		DBG(1, "  N enb/frame = %d, N frame/image = %d, N img = %d\n",
				nexppf, nfrmpim, n_images)
		}
	else if (dcb_external_trig())
		{
		// ensure timing margins
		if (exposure_time+dcb_overhead_time()+max_delay>=nexppf*exposure_period)
			{
			printf("*** Exposure period is too short\n");
			dcb_reset_exposure_mode();
 			external_enable_mode = external_trigger_mode =
 					external_multi_trigger_mode = 0;
#ifdef MASTER_COMPUTER
				strcpy(line, "K");
				tell_all_other_computers(ResetCam, line);
#endif
			return 1;
			}
		printf("  Delay = %.4lf, Exp time = %.4lf, Exp period = %.4lf\n", 
				delay_time, exposure_time, exposure_period);
		printf("  N exp/frame = %d, N frame/image = %d, N images = %d\n",
				nexppf, nfrmpim, n_images);
		DBG(1, "  Delay = %.4lf, Exp time = %.4lf, Exp period = %.4lf\n", 
				delay_time, exposure_time, exposure_period)
		DBG(1, "  N exp/frame = %d, N frame/image = %d, N images = %d\n",
				nexppf, nfrmpim, n_images)
		}
	else if (dcb_external_multi_trig())
		{
		printf("  Delay = %.4lf, Exp time = %.4lf\n", 
				delay_time, exposure_time);
		printf("  N exp/frame = %d, N frame/image = %d, N images = %d\n",
				nexppf, nfrmpim, n_images);
		DBG(1, "  Delay = %.4lf, Exp time = %.4lf\n", 
				delay_time, exposure_time)
		DBG(1, "  N exp/frame = %d, N frame/image = %d, N images = %d\n",
				nexppf, nfrmpim, n_images)
		}
	else			// normal exposures
		{
		// ensure timing margins
		if (exposure_time+dcb_overhead_time()+max_delay>=nexppf*exposure_period)
			{
			printf("*** Exposure period is too short\n");
			dcb_reset_exposure_mode();
 			external_enable_mode = external_trigger_mode =
 					external_multi_trigger_mode =0;
#ifdef MASTER_COMPUTER
				strcpy(line, "K");
				tell_all_other_computers(ResetCam, line);
#endif
			return 1;
			}
		printf("  Exp time = %.6lf, Exp period = %.6lf\n", exposure_time, exposure_period);
		printf("  N exp/frame = %d, N frame/image = %d, N img = %d\n",
				nexppf, nfrmpim, n_images);
		DBG(1, "  Exp time = %.6lf, Exp period = %.6lf\n", exposure_time, exposure_period);
		DBG(1, "  N exp/frame = %d, N frame/image = %d, N img = %d\n",
				nexppf, nfrmpim, n_images);
		}
	dcb_set_exposure_time(exposure_time);
	if ((exposure_time+(nexppf-1)*exposure_period)*nfrmpim > 3700.0)
		printf("  Sequence takes %.1lf hours.  Issue 'K' from client to abort.\n",
				(exposure_time+(nexppf-1)*exposure_period)*nfrmpim/3600.0);

	// a full path name has been assembled by the caller
	if (7+strlen(image_file) >= LENFN)
		{
		printf("Full path name is too long\n");
		return 1;
		}
	p = -5+image_file+strlen(image_file);	// look for extension
	if ( (q=strrchr(p, '.')) )
		{
		strcpy(ext, q);
		*q = '\0';
		}
	else
		strcpy(ext, "");				// default is raw image
	sn=0;								// start number
	fmt=5;								// format
	if ( !(p=strrchr(image_file, '/')) )
		p=image_file;
	strcpy(base_name, p);				// for log file
	if ( (q=strrchr(p, '_')) )
		{
		q++;
		flag = isdigit(*q) && isdigit(*(q+1)) && isdigit(*(q+2));
		for (i=0; *(q+i); i++)
			if (!isdigit(*(q+i)))
				{
				flag = 0;
				break;
				}
		if (flag)					// at least 3 digits and nothing else
			{
			sn=atoi(q);
			fmt=0;
			p=q;
			while(isdigit(*q))
				{
				fmt++;
				q++;
				}
			*p='\0';
			if ((fmt<3) || (fmt==3&&n_images>999) ||(fmt==4&&n_images>9999))  
				fmt=5;
			}
		else if (*q)
			strcat(p, "_");				// force '_' ending
		}
	else
		strcat(p, "_");					// force '_' ending

	// setup rotating directories, if applicable
	if (setupDirs(image_file, ext))
		return -1;

	nextFileName(sn, fmt);				// make up the first filename
	if (n_images>=100)
		printf("  Starting %s\n", name2l);

	// start dcb log - if enabled
	dcb_open_log();

	// get a logfile buffer - keep log in memory to avoid disk seeks
	if ( (lbP=logb=malloc(500+(43+strlen(name2l))*n_images)) == NULL)
		{
		printf("camera_start() cannot allocate memory\n");
		return 1;
		}
	*lbP='\0';
	lbP+=sprintf(lbP, "t req.  t meas.     endTime               =========== File ============\n");

#ifdef USE_FORK
	// set the signal handler to catch exiting children
	if ( (sigaction (SIGCHLD, &receiveSignal, &prevAction)) == -1)
		{
		perror("sigaction setup");
		return 1;
		}
	pcnt=ns=pcmax=0;						// counters
	process_wait_flag=0;
#endif

	last_exposure=0;
	// set up printing every 100th image number
	if (n_images>=100 && exposure_period<2.0)
		{
		printf("  img "); 
		fflush(stdout);
		}

	// empty txBuffer, if applicable - typically the command acknowledge
	if (!masterProcess && strlen(txBuffer) > 0)
		{
		DBG(2, "(%s) Socket TX: %s\n", gs_timestamp(), txBuffer)
		write(ufds[1].fd, txBuffer, strlen(txBuffer));
		txBuffer[0] = '\0';
		}

	// start the first exposure
	if (dcb_expose())
		return 1;
	exp_start_time = gs_time();
	startTime = exp_start_time - tdiff;	// update camserver exposure start time

	// set up for overlapped exposures within the readout sequence
	dcb_set_autoframe();

	// start the time-point logger
//	time_log(-1, NULL);		// if this is not done, time_log is inactive

	for (i=sn, go=1; go && i<-1+n_images+sn; i+=1+multiplicity-nfrmpim)
		{
		// print every 100th image number
		// longer periods are printed in dcb_check_status()
		if (n_images>=100 &&  !((i-sn)%100) && exposure_period<2.0)
			{
			printf(" %d ", (i-sn));
			if ((i-sn) && !((i-sn)%1000))
				printf("\n      ");
			fflush(stdout);
			}
		if (acknowledge_interval && !((i-sn)%acknowledge_interval) &&
			exposure_period>=0.01)				// below 100 Hz
			sprintf(txBuffer, "%d OK img %d\x18", (int)Send, (i-sn));

 		time_log(1, "full cycle");	// start new line in logger

		// set up the next directory/filename combination - first one already done
		if (i>sn)
			nextFileName(i, fmt);

		if (frame_time >= 0.05)		// avoid this at high speed
			set_cam_stat(cam_target, image_file);

		// accumulate a (potentially) multi-frame image
		multiplicity = 0;
		if (nfrmpim>1)				// if multi-frame, clear image
			initialize_theImage(gap_fill_byte);

		while(go && multiplicity<nfrmpim)
			{
			// NULL to suppress printing image name
			if (multiplicity<-1+nfrmpim)
				p = NULL;			// without printing
			else
				p = image_file;		// for long exposures
			while( !dcb_check_status(p))		// without printing
				{
				monitor_temp_humid();
				if (socket_service())			// look for "K"
					{
					printf("*** killing exposure\n");
					DBG(3, "*** killing exposure\n");
					return -1;
					}
				}
			DBG(2, "(interface.c) Starting frame %d\n", multiplicity)

			// wait for interrupt, start next exposure & readout
			if (camera_readout(image_file, msg))	// message already printed
				break;

			time_log(0, "readout");

			if ((frame_time >= 0.02) && socket_service())
				go=0;
			}

		if (!multiplicity)		// bad readout - bail out
			break;

		if (multiplicity>nfrmpim)
			{
			lbP+=sprintf(lbP, "### multiplicity = %d\n", multiplicity);	// mark possible too long exposure
			nmissed += (multiplicity-nfrmpim)*nexppf;		// missed exposures
			dt = exposure_time*((multiplicity-1)*nexppf+1);
			}
		else
			dt = exposure_time*nexppf*nfrmpim;

		inc_mx_start_angle(multiplicity);

		if (dcb_external_enb() || dcb_external_trig())
			lbP+=sprintf(lbP, "%s  %s\n", 
						gs_timestamp(), name2l);
		else
			lbP+=sprintf(lbP, "%.4lf  %.4lf  %s  %s\n", 
						exposure_time, dt, gs_timestamp(), name2l);

		if (frame_time >= 0.01)		// avoid this at high speed
			set_cam_stat(cam_completed, name2l);

		}			// --- end of exposure series

	multiplicity = 0;
	if (go && nfrmpim>1)	// collect all but 1 frame of last multi-frame image
		{
		initialize_theImage(gap_fill_byte);	// clear image

		while (multiplicity<-1+nfrmpim)
			{
			while( !dcb_check_status(NULL))		// for long exposures
				monitor_temp_humid();
			// wait for interrupt, start next exposure & readout
			if (camera_readout(image_file, msg))	// message already printed
				break;
			}
		}

	if (n_images>=100 && exposure_period<2.0)
		printf("\n");

	// unset overlapped exposures mode
	dcb_reset_autoframe();

	// do the last image - will be read out by end_exposure()
	// after we wrap up here
	i = i>-1+n_images+sn ? -1+n_images+sn : i;
	nextFileName(i, fmt);
	set_cam_stat(cam_target, image_file);

	if (multiplicity>nfrmpim)
		{
		lbP+=sprintf(lbP, "###\n");			// mark too long exposure
		nmissed += (multiplicity-nfrmpim)*nexppf;		// missed exposures
		dt = exposure_time*((multiplicity-nfrmpim)*nexppf+1);
		}
	else
		dt = exposure_time*nexppf*nfrmpim;

	// log last image
	if (dcb_external_enb() || dcb_external_trig())
		lbP+=sprintf(lbP, "%s  %s\n", 
					gs_timestamp(), name2l);
	else
		lbP+=sprintf(lbP, "%.4lf  %.4lf  %s  %s\n", 
					exposure_time, dt, gs_timestamp(), name2l);

	// write the log
	strcpy(line, name2l);
	*(1+strrchr(line, '/')) = '\0';
	strcat(line, base_name);
	strcat(line, ".log");
	strcpy(mode,"w");					// don't append if slave
	if ( (lfp=fopen(line, mode)) )
		{
		fprintf(lfp, "%s--------- End ----------------------------\n", logb);
		fclose(lfp);
		}
	else
		{
		printf("Could not open file for writing: %s\n", line);
		}
	free (logb);
	while( !dcb_check_status(image_file))	// for long exposures
		monitor_temp_humid();

	// print timing stats
	time_log(2, NULL);

	// write & close dcb log - result in /tmp/dcblog.txt
	dcb_close_log();

	#ifdef USE_FORK
	process_wait_flag=1;		// wait for processes to finish
	#endif

#ifdef SLAVE_COMPUTER
	if (exposure_time < 1.0)
		sleep(1);				// keep priority high for a moment
#endif

	last_exposure=1;

	if (nmissed)
		{
		printf("*** Missed %d readouts.  See log for multiple exposures.\n",
				nmissed);
		DBG(1, "*** Missed %d readouts.  See log for multiple exposures.\n",
				nmissed)
		}
	}
else		// single exposure mode
	{
	dcb_set_exposure_time(exposure_time);
	// for single exposures, period must merely be longer than frame time
	// if only a single exposure/image, don't hassle the operator
	if (nexppf==1 && exposure_period<=exposure_time+0.001)
		{
		dcb_set_exposure_period(exposure_time+0.1);
		}
	else if ((exposure_time>=exposure_period) &&
				!external_enable_mode)
		{
		printf("*** Exposure period too short for NExpFrame>1\n");
		dcb_reset_exposure_mode();
		external_enable_mode = external_trigger_mode = 0;
		return 1;
		}
	dcb_set_tx_frame_limit(nfrmpim);			// n frames, 1 image

	if ( ((exposure_time+(nexppf-1)*exposure_period)*nfrmpim > 3700.0) &&
				(nexppf>1 || nfrmpim>1))
		printf("  Sequence takes %.1lf hours.  Issue 'K' from client to abort.\n",
				(exposure_time+(nexppf-1)*exposure_period)*nfrmpim/3600.0);

	// set up for external enable or external trigger , if requested
	if (external_enable_mode)
		{
		dcb_set_exp_trigger_delay(0, -1);
		dcb_set_external_enb(0);
		DBG(1, "  ExtE: N enb/frame = %d, N frame/image = %d, N img = %d\n",
				nexppf, nfrmpim, n_images)
		}
	else if (external_trigger_mode)
		{
		dcb_set_external_trigger(0);
		DBG(1, "  ExtT: N enb/frame = %d, N frame/image = %d, N img = %d\n",
				nexppf, nfrmpim, n_images)
		}
	else if (external_multi_trigger_mode)
		{
		dcb_set_multi_trigger(1);
		DBG(1, "  ExtM: N enb/frame = %d, N frame/image = %d, N img = %d\n",
				nexppf, nfrmpim, n_images)
		}
	else		// normal exposure
		{
		DBG(1, "  Expo: N enb/frame = %d, N frame/image = %d, N img = %d\n",
				nexppf, nfrmpim, n_images)
		}

#ifdef USE_FORK
	// set the signal handler to catch exiting children
	if ( (sigaction (SIGCHLD, &receiveSignal, &prevAction)) == -1)
		{
		perror("sigaction setup");
		return 1;
		}
	pcnt=ns=pcmax=0;			// counters
	process_wait_flag=1;		// wait for process to finish
#endif

	// empty txBuffer, if applicable - typically the command acknowledge
	if (!masterProcess && strlen(txBuffer) > 0)
		{
		DBG(2, "(time = %s) Socket TX: %s\n", gs_timestamp(), txBuffer)
		write(ufds[1].fd, txBuffer, strlen(txBuffer));
		txBuffer[0] = '\0';
		}

	if (dcb_expose())
		return 1;

	// see note on timings above
	exp_start_time = gs_time();
	startTime = exp_start_time - tdiff;	// update camserver exposure start time
	// the real endTime gets set later
	endTime = startTime + ((exposure_time-0.05)<0.0 ? 0.0 : (exposure_time-0.05));

	multiplicity = 0;
	if (nfrmpim>1)		// collect all but 1 frame of a multi-frame image
		{
		initialize_theImage(gap_fill_byte);	// clear image
		dcb_set_autoframe();
		while (multiplicity<-1+nfrmpim)
			{
			while( !dcb_check_status(NULL))		// for long exposures
				{
				monitor_temp_humid();
				if (socket_service())			// look for "K"
					{
					printf("*** killing exposure\n");
					DBG(3, "*** killing exposure\n");
					return -1;
					}
				}
			// wait for interrupt, start next exposure & readout
			if (camera_readout(image_file, msg))	// message already printed
				break;
			}
		dcb_reset_autoframe();
		}
	last_exposure=1;
	}

DBG(5, "End of camera_start(), pcnt = %d, tn = %.6lf\n", pcnt, gs_time())
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
**    The priority argument specifies the urgency: 0 == wait for normal end, **
**    1 == readout now, 2 == stop now                                        **
**                                                                           **
**    Returns an error flag: 0 means OK, 1 means an error                    **
**    This routine should print the error message                            **
**                                                                           **
\*****************************************************************************/

int camera_stop(int prio)
{
#ifdef REAL_HARDWARE
int i, n;

DBG(4, "---- camera_stop(%d)\n", prio)

if (prio > 1)					// 'K' or ExpEnd was given
	{
	dcb_stop();					// kills dma
	external_enable_mode = external_trigger_mode =0;
	external_multi_trigger_mode=0;
	camera_killed = 1;
	return 0;
	}

n=40;
for(i=0; i<n; i++)				// give time to finish
	{
	if (dcb_check_status(NULL))
		break;
	if (socket_service())
		break;
	monitor_temp_humid();
	}

for (i=0; i<NMOD_BANK*NBANK; i++)
	usleep(5000);				// dma can be in progress

if (dcb_calc_remaining_time() > 4.0)
	dcb_stop();					// kills dma

#if 0
// wait for the trigger - can be a long time
if (external_enable_mode || external_trigger_mode)
	while( count-- && !(gs_irq_src_status(0) & ~0x10) && (gs_status(0) & 0x10) )
		usleep(10000);
#endif

#endif

// the dcb flags for these modes are reset automatically
external_enable_mode = external_trigger_mode = 0;
external_multi_trigger_mode = 0;

return 0;
}



/*****************************************************************************\
**                                                                           **
**   camera_read_telemetry:  read camera telemetry into buffer provided      **
**                                                                           **
\*****************************************************************************/

void camera_read_telemetry(char *bufr, int size)
{
int i, n;
double t;

n = sprintf(bufr, " === Telemetry at %s ===\n", timestamp());
if (size < 2048)
	return;

#ifdef SLSP2DET_CAMERA
n+= sprintf(bufr+n, "Image format: %d(w) x %d(h) pixels\n",
		IMAGE_NCOL, IMAGE_NROW);
n+= sprintf(bufr+n, "Selected bank: %d\n", selected_bank);
n+= sprintf(bufr+n, "Selected module: %d\n", selected_mod);
n+= sprintf(bufr+n, "Selected chip: %d\n", selected_chip);
for (i=0; i<6*gs_device_count(); i++)
	if ( (t = dcb_read_temp(i)) > -99.0)
		n+= sprintf(bufr+n,
				"Channel %d: Temperature = %.1lfC, Rel. Humidity = %.1lf%%\n",
				i, dcb_read_temp(i), dcb_read_humidity(i));
#endif

return;
}



/*****************************************************************************\
**                                                                           **
**    camera_check_status:  check status of camera                           **
**                                                                           **
**    camera can call an end to an exposure or wait by returning nonzero;    **
**    this will invoke end_exposure()                                        **
**                                                                           **
\*****************************************************************************/

int camera_check_status(double time)
{

DBG(4, "---- camera_check_status()\n")

if (camera_killed)
	return 2;

// the loop to run out an exposure is in dcblib.c.  We start coming here
// (from exposure_service() in camserver) when there is < 2 s remaining

if (dcb_check_status(NULL))
	return 1;		// exposure nearly done - start wait & readout

#ifdef SLAVE_COMPUTER
if (socket_service())
	return 2;
#endif

monitor_temp_humid();
usleep(1000);		// ~3 ms delay

if (camera_killed)
	return 2;

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

DBG(3, "(interface.c) camera_shutdown: pid = %d\n", getpid())

return;
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
read_camera_setup(name, msg);	// error message returned in 'msg' - in cam_tbl.c
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
**       exposures are controlled by camserver (not by the camera)           **
**    Can be used, e.g., to monitor temperature.                             **
**                                                                           **
\*****************************************************************************/

void camera_monitor(void)
{
if ( ((++monitor_count) % 60000) )		// only check once per minute
	return;

monitor_temp_humid();					// check temp & humidity

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
#if defined MASTER_COMPUTER || SLAVE_COMPUTER
// find the index of 'cmnd' in the list
static int bin_search(CAMERA_CMND cmnd, int low, int high)
{
int mid=(low+high)/2;

//if (high < low)		// not necessary if item guaranteed to be in list
//	return -1;			// not found

if (cmnd < camera_list[mid].cmnd)
	return bin_search(cmnd, low, mid-1);
if (cmnd > camera_list[mid].cmnd)
	return bin_search(cmnd, mid+1, high);
return mid;
}
#endif


CAMERA_CMND camera_cmnd_filter(CAMERA_CMND cmnd, char *ptr, int tx)
{
CAMERA_CMND tcmnd=cmnd;

#ifdef MASTER_COMPUTER
char line[LENFN], line2[40+LENFN], *p, *q, s[16], t[]="X_";
int sock=1, err=0, x, y, z, i;
struct det_pixel *px;
double tt;

if ( !(masterProcess || controllingProcess) )	// must have control to pass cmnd
	return tcmnd;

switch (cmnd)
	{
// passed without modification - translation done by slave computer
	case SeT:
	case Reset:
	case ProG:
		if (loadingTrimFile)		// do not pass these over socket in bulk
			break;
		goto tell_all;
		break;


#ifdef WIDE_FAST
// these require translation on x
	case Cpix:
	case Cpix_x:
		x=y=-1;
		sscanf(ptr, "%d%d", &x, &y);
		if (y<0)					// invalid argument
			break;
		px = pix_add(-1, x, y, -1, -1, -1, -1, -1, -1, -1, -1, -1);
		if (px->cx < 0)		// not on a chip - main process reports error
			break;
		sock = px->mn/2;
		if (sock==0 || sock >=N_COMPUTERS)
			break;
		x -= sock*(NCOL_MOD+NCOL_BTW_MOD);
		if (cmnd==Cpix)
			sprintf(line, "Cpix %d %d\n", x, y);
		else
			sprintf(line, "Cpix_x %d %d\n", x, y);
		err += tell_other_computer(sock, tcmnd, line);
		break;

	case Trim:
		if (loadingTrimFile)		// do not pass this over socket in bulk
			break;
		z=-1;	
		sscanf(ptr, "%d%d%d", &x, &y, &z);
		if (z<0)					// query goes only to master
			break;
		px = pix_add(-1, x, y, -1, -1, -1, -1, -1, -1, -1, -1, -1);
		sock = px->mn/2;
		if (sock==0 || sock >=N_COMPUTERS)
			break;
		x -= sock*(NCOL_MOD+NCOL_BTW_MOD);
		sprintf(line, "Trim %d %d %d\n", x, y, z);
		err += tell_other_computer(sock, tcmnd, line);
		break;
#else
// these require translation on y
	case Cpix:
	case Cpix_x:
		x=y=-1;
		sscanf(ptr, "%d%d", &x, &y);
		if (y<0)					// invalid argument
			break;
		px = pix_add(-1, x, y, -1, -1, -1, -1, -1, -1, -1, -1, -1);
		if (px->cx < 0)		// not on a chip - main process reports error
			break;
		sock = px->bn/2;
		if (sock==0 || sock >=N_COMPUTERS)
			break;
		y -= sock*(NROW_MOD+NROW_BTW_BANK);
		if (cmnd==Cpix)
			sprintf(line, "Cpix %d %d\n", x, y);
		else
			sprintf(line, "Cpix_x %d %d\n", x, y);
		err += tell_other_computer(sock, tcmnd, line);
		break;

	case Trim:
		if (loadingTrimFile)		// do not pass this over socket in bulk
			break;
		z=-1;	
		sscanf(ptr, "%d%d%d", &x, &y, &z);
		if (z<0)					// query goes only to master
			break;
		px = pix_add(-1, x, y, -1, -1, -1, -1, -1, -1, -1, -1, -1);
		sock = px->bn/2;
		if (sock==0 || sock >=N_COMPUTERS)
			break;
		y -= sock*(NROW_MOD+NROW_BTW_BANK);
		sprintf(line, "Trim %d %d %d\n", x, y, z);
		err += tell_other_computer(sock, tcmnd, line);
		break;
#endif

	case ImgonlY:
		if(!*ptr)
			{
			ptr = s;
			strcpy(ptr, autofilename);		// make up a filename
			sprintf(ptr+strlen(ptr), "_%04d", filenumber);
			}
		while (sock<N_COMPUTERS)
			{
			strcpy(line, "ImgonlY ");		// note space
			if ( (p=strrchr(ptr, '/')) )
				p++;
			else
				p = ptr;
			if (! (q=strchr(p, '.')) )
				q = p+strlen(p);
			t[0] = 'A'+sock;			// generate "B_", etc.
			strcat (line, t);
			strncat(line, p, q-p);
			strcat(line, ".cbf\n");		// force cbf format
			err += tell_other_computer(sock, tcmnd, line);
			sock++;
			}
		break;

// exposure commands need modified filenames
// secondary computers are always in ExtEnable mode
// unless using differing trigger delays on the DCBs
	case Exposure:
	case ExtTrigger:
	case ExtMTrigger:
	case ExtEnable:
		while (sock<N_COMPUTERS)
			{
			// if exposure time is longer than DMA time-out,
			// inform the slave.  dma hold time is reset to 0
			// automatically at the end of an exposure series
			if (trigger_offset[0] >= 0.0)
				for(i=0; i<DET_NDCB; i++)
					{
					min_delay = min(min_delay, trigger_offset[i]);
					max_delay = max(max_delay, trigger_offset[i]);
					}
			if (max_delay-min_delay > 4.0)
				{
				err=-1;
				printf("*** Spread in delays too great: %lf sec.\n",
						max_delay-min_delay);
				break;
				}
			tt = dcb_get_exposure_period();
			if (n_images <= 1)
				tt = exposure_time + max_delay;
			strcpy(line, "ExtEnable ");		// note space
			if (trigger_offset[0] >= 0.0 && tcmnd != ExtEnable)
				strcpy(line, "ExtMTrigger ");		// note space
			if ( (p=strrchr(ptr, '/')) )
				p++;
			else
				p = ptr;
			if (! (q=strchr(p, '.')) )
				q = p+strlen(p);
			t[0] = 'A'+sock;			// generate "B_", etc.
			strcat (line, t);
			strncat(line, p, q-p);
			strcat(line, ".cbf\n");		// force cbf format
			err += tell_other_computer(sock, tcmnd, line);
			sock++;
			}
		break;


// make file available to slave
	case LdBadPixMap:
	case LdFlatField:
		if (*ptr=='0' || *ptr=='o')
			goto tell_all;
	case TrimFromFile:
		if (loadingTrimFile || !*ptr)
			break;			// missing name will be caught by main loop
		if (*ptr == '/')					// if path is given
			strcpy(line, ptr);
		else if (*ptr == '~')
			{
			strcpy(line, getenv("HOME"));
			strcat(line, ptr+1);
			}
		else								// make up full path
			{
			strcpy(line, cam_data_path);
			strcat(line, ptr);
			}
		p = strrchr(line, '/');				// point to name
		// must copy file; simple link won't work over NFS
		sprintf(line2, "cp %s /compB_images/", line);
		if (system(line2))
			{
			perror("system()");
			break;
			}
		// path from point of view of slave computer
		sprintf(line2, "%s /home/det/p2_det/images%s\n",
				camera_list[bin_search(tcmnd, 0, camera_count)].name, p);
		sock = 1;
		err += tell_other_computer(sock, tcmnd, line2);
		usleep(10000);				// give time to act
		sprintf(line2, "/compB_images/%s", p);
		unlink(line2);
		break;

	case Delay:
		if (*ptr)
			goto tell_all;
		break;

// these are ignored (handled by master computer or locally)
	case CamCmd:
	case CamSetup:
	case CamWait:
	case DataPath:
	case Df:
	case HeaderString:
	case ImgPath:
	case Send:
	case SetTrims:
	case ShowPID:
	case ShutterEnable:
	case TelemetrY:
	case MenU:
	case Status:
	case LdCmndFile:
	case Read_setup:
	case Read_signals:
	case Show:
	case Load:
	case Save:
	case LogImgFile:
	case MXsettings:
	case ExpNaming:
	case DebTime:
	case SetAckInt:
	case LogSettings:
	case DataSection:
	case SetThreshold:
		break;

	case ExpEnd:
		tcmnd = K;
		break;

// these are passed without modification
	case Calibrate:
		longsleep=1;
	case ExpTime:
	case K:
	case ResetCam:
	case ExiT:
	case QuiT:
	case ImgMode:
	case NImages:
	case ExpPeriod:
	case NExpFrame:
	case NFrameImg:
	case VDelta:
	case Tau:
	case THread:
	case SensorHtr:
	case SetLimTH:
	case Stop:
	case Fillpix:
	case SFillpix:
	case Trim_all:
	case BitMode:
	case Dcb_init:
	case GapFill:
	case CalibFile:
	case DiscardMultIm:
	case DacOffset:
	case SetSDelay:
	case Cam_start:
#ifdef DEBUG
	case CamNoop:
	case DbglvL:
#endif
tell_all:
		sprintf(line, "%s %s", camera_list[bin_search(tcmnd, 0, camera_count)].name, ptr);
		err += tell_all_other_computers(tcmnd, line);
		break;

	default:
		printf("*** Missing case: %s\n", camera_list[bin_search(tcmnd, 0, camera_count)].name);
		break;
	}

if (err)
	{
	printf("*** Error in communication: %s %s\n",
			camera_list[bin_search(tcmnd, 0, camera_count)].name, ptr);
	DBG(1, "*** Error in communication: %s %s\n", 
			camera_list[bin_search(tcmnd, 0, camera_count)].name, ptr)
	}

#endif

#ifdef SLAVE_COMPUTER		// increase verbosity for slave
if (tx)
	printf("%s %s\n", camera_list[bin_search(tcmnd, 0, camera_count)].name, ptr);
#endif

return tcmnd;		// possibly altered command
}




/*****************************************************************************\
*******************************************************************************
**                                                                           **
**    End of camserver built-in customizable functions                       **
**                                                                           **
*******************************************************************************
\*****************************************************************************/


/* --------- Local Stuff -------------------------------------------------- */


/*****************************************************************************\
**                                                                           **
**   endWriteProcess: act on the signal when a file-writing child exits      **
**                                                                           **
\*****************************************************************************/

#ifdef USE_FORK
static __sighandler_t endWriteProcess(int s, siginfo_t *info, void *add)
{
pcnt--;
//printf("Reached sighandler, pcnt =  %d; pid = %d\n", pcnt, info->si_pid);
// N.B. it is bad practice to use printf() in a signal handler
//DBG(7, "Reached sighandler, pcnt =  %d; pid = %d, tn = %.6lf\n", 
//		pcnt, info->si_pid, gs_time());
waitpid(info->si_pid, NULL, WNOHANG);
//DBG(6, "Time after waitpid(): %.6lf\n", gs_time());
return 0;
}
#endif



/*****************************************************************************\
**                                                                           **
**    setupDirs(), nextFileName(), & cleanupDirs                             **
**                                                                           **
**    set up and maintain a rotating directory scheme for experiments        **
**    with high performance file writing                                     **
**                                                                           **
\*****************************************************************************/

// for huge numbers of images, write blocks into pre-allocated subdirectories
// this is an attempt to get around a performance problem in which the computer
// fails to respond for some time after writing >~10GB.  This appears to
// be caused by the need for the operating system to expand the target 
// directory.  The best strategy (after many tests) is to have as many
// directories as the maximum number of images that can be accomodated
// physical memory, then use the directories on a round robin basis.

static int using_dirs=0, ndirs, sectno;
static char base_name[50], *name2wP, *name2lP, extension[10];


static int setupDirs(char *img_name, char *ext)
{
FILE *ifp;
char line[LENFN], *p=NULL, *q, img_path[LENFN];
int low_mem, ofd, err;
struct stat stat_buf;

name2w = image_file;				// name to write - an alias
strcpy(extension, ext);				// memorize the extension
strcpy(img_path, img_name);			// copy base name - ends in '_' already
if ( (q=strrchr(img_path, '/')) == NULL)
	{
	printf ("cannot isolate filename base from:\n\t%s\n", img_name);
	return -1;
	}
strcpy(base_name, ++q);				// save base_name
*q = '\0';							// after last '/' in path

// set up tentative paths
strcpy(name2w, img_path);			// put path into name holders
strcpy(name2l, img_path);
name2wP = name2w+strlen(name2w);	// write pointers
name2lP = name2l+strlen(name2l);

// turn this system off for now
return 0;

if (n_images<=200)					// too few images to bother with
	return 0;

ifp = fopen("/proc/meminfo", "r");
while( (fgets(line, sizeof(line), ifp)) )
	if ( (p=strstr(line, "LowTotal")) )
		break;
fclose(ifp);
p=strchr(p, ':');
sscanf(p+1, "%d", &low_mem);
//printf("Low mem total = %d kB\n", low_mem);
if (low_mem < 100*(((IMAGE_NBYTES)+1023)>>10))	// avoid integer overflow
	{
	printf("/proc/meminfo reported low memory = %d B.  We need more here.\n", low_mem);
	return -1;
	}

// We grab 96.0% of low memory, allowing ~120 kB overhead devoted to each image
ndirs = (int)(0.96*low_mem/(IMAGE_NBYTES + 1.2e5));
ndirs = (ndirs>n_images/10) ? n_images/10 : ndirs;
printf("Setting up %d subdirectories\n", ndirs);
for (sectno=ndirs-1; sectno>=0; sectno--)
	{
	sprintf(q, "sect%.3d/", sectno);		// make up name in img_path[]
	err = mkdir(img_path, 0755);
	stat(img_path, &stat_buf);
	if (err && !(errno == EEXIST && S_ISDIR(stat_buf.st_mode)))
		{
		printf("unable to make sub-directory:\n\t%s\n", img_path);
		return -1;
		}
	// warm up the directory with a test write
	strcat(img_path, "test.img");
	if ((ofd = open(img_path, O_WRONLY | O_CREAT | O_TRUNC,
				S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH)) == -1)
		{
		printf("*** Could not open image output file:\n\t%s\n", img_path);
		return -1;
		}
	write(ofd, (void *)theImage, IMAGE_SIZE*sizeof(char)/50);
	close(ofd);
	}
sectno=0;
using_dirs = 1;

return 0;
}



static void nextFileName(int n, int w)
{
// log file just gets the name
sprintf(name2lP, "%s%.*d%s", base_name, w, n, extension);

if (using_dirs)
	{
	sprintf(name2wP, "sect%.3d/%s", sectno, name2lP);
	sectno = ((++sectno)%ndirs) ? sectno : 0;
	}
else
	strcpy(name2wP, name2lP);	// no extra directory

return;
}



static int cleanupDirs(void)
{
int imgcnt;

if (!using_dirs)		// nothing to do
	return 0;

// for user's convenience, move files to the master directory
sectno=0;
for(imgcnt=0; imgcnt<n_images; imgcnt++)
	{
	sprintf(name2wP, "sect%.3d/%s%.5d%s", sectno, base_name, imgcnt, extension);
	sprintf(name2lP, "%s%.5d%s", base_name, imgcnt, extension);
	// some images may be missing - ignore, but do not increment sectno
	if (!rename(name2w, name2l))
		sectno = ((++sectno)%ndirs) ? sectno : 0;
	}
for (sectno=0; sectno<ndirs; sectno++)
	{
	// remove the test.img
	sprintf(name2wP, "sect%.3d/test.img", sectno);
	unlink(name2w);
	// remove the directory
	*strrchr(name2wP, '/')='\0';
	rmdir(name2w);
	}

using_dirs = 0;
return 0;
}



/*****************************************************************************\
**                                                                           **
**   monitor temperature and humidity                                        **
**                                                                           **
\*****************************************************************************/
/*
**  Because of the getttimeofday(), this is a slow function - don't call
**  it too often.
*/ 
static void monitor_temp_humid(void)
{
int hour, minute, chan, flag=1;							// default temp-humid sensor channel
struct timeval tv;
struct timezone tz;
double temp[6*NDCB], humid[6*NDCB], tlim[6*NDCB], tliml[6*NDCB], hlim[6*NDCB], tmax=-99.0;
FILE *ofp;
char line[LENFN], bufr[4096];

gettimeofday(&tv, &tz);
minute = (int)((unsigned long long)tv.tv_sec / 60);		// only check per minute
if (minute <= last_min)
	return;
last_min = minute;

for(chan=0; chan<6*NDCB; chan++)
	{
	temp[chan] = dcb_read_temp(chan);
	if (temp[chan] > tmax)
		tmax = temp[chan];
	}
if (tmax <= -39.0)	// power off or sensor not found
	return;

// get other data for active channels
for(chan=0; chan<6*NDCB; chan++)
	if (temp[chan]>-39.0)
		{
		humid[chan] = dcb_read_humidity(chan);
		tlim[chan] = dcb_read_temp_high_limit(chan);
		tliml[chan] = dcb_read_temp_low_limit(chan);
		hlim[chan] = dcb_read_humidity_high_limit(chan);
		}

if (last_hour == -1)				// a restart
	{
	strcpy(line, camstat_path);
	strcat(line, "TH.log");
	if ( (ofp = fopen(line, "a")) == NULL )
		{
		printf("Could not open file for appending: %s\n", line);
		return;
		}
	fprintf(ofp, "----- Restarting temperature / humidity log\n");
	fclose(ofp);
	}

#ifndef TH_CONTROL_OVERRIDE
for(chan=0; chan<6*NDCB; chan++)
	{
	if (temp[chan]>-39.0 && temp[chan]>tlim[chan]-3.0 && temp[chan]<100.0)		// 3.0 margin from hardware shutdown
		{
		printf("Temperature too high: %.1lfC (chan %d) - see TH.log\n", temp[chan], chan);
		flag = 0;
		break;
		}
	}
for(chan=0; chan<6*NDCB; chan++)		// older firmwares had a high tlo setting
	if (tliml[chan]<20.0 && temp[chan]>-39.0 && temp[chan]<tliml[chan]+3.0)		// 3.0 margin from hardware shutdown
		{
		printf("Temperature too low: %.1lfC (chan %d) - see TH.log\n", temp[chan], chan);
		flag = 0;
		break;
		}
for(chan=0; chan<6*NDCB; chan++)
	{
	if (temp[chan]>-39.0 && humid[chan]>hlim[chan]-3.0 && humid[chan]<100.0)	// 3.0 margin from hardware shutdown
		{
		printf("Humidty too high: %.1lf%% (chan %d) - see TH.log\n", humid[chan], chan);
		flag = 0;
		break;
		}
	}
// we don't flag too low humidity
#endif

hour = (int)((unsigned long long)tv.tv_sec / 3600);		// only record hourly
if (hour <= last_hour && flag)
	return;
last_hour = hour;

strcpy(line, camstat_path);
strcat(line, "TH.log");
if ( (ofp = fopen(line, "a")) == NULL )
	{
	printf("Could not open file for appending: %s\n", line);
	return;
	}
for(chan=0; chan<6*NDCB; chan++)
	if (temp[chan] > -39.0)
		{
		if (temp[chan] >= 100.0)
			fprintf(ofp, "    %s Channel %d: Bad readout - please ignore\n", timestamp(), chan);
		else if (temp[chan] > tlim[chan]-3.0 || temp[chan] < tliml[chan]+3.0 || humid[chan] > hlim[chan]-3.0)
			fprintf(ofp, "### %s Channel %d: Temperature = %.1lfC, Rel. Humidity = %.1lf%%\n",
					timestamp(), chan, temp[chan], humid[chan]);
		else
			fprintf(ofp, "    %s Channel %d: Temperature = %.1lfC, Rel. Humidity = %.1lf%%\n",
					timestamp(), chan, temp[chan], humid[chan]);
		}

fclose(ofp);

camera_read_telemetry(bufr, sizeof(bufr));
set_cam_stat(cam_telemetry, bufr);

return;
}



/*****************************************************************************\
**                                                                           **
**   setup_other_computers()                                                 **
**                                                                           **
\*****************************************************************************/
/*
** In the case of multiple computers (not multiple CPU's), get camserver
** running on the secondary machines and establish links.
**
** Setup:  master computer called "A", slave computer called "B".  Each
** computer has 2 e-net ports.  On the A computer configure port 1 for DHCP
** to be the interface to the outside world.  Configure port 2 for address
** 10.0.0.1.  The B computer has port 1 as DHCP for setup and debugging;
** ultimately this port is not connected.  Port 2 gets address 10.0.0.2,
** which is directly connected to port 2 on A with a short e-net cable.
** Make sure that A can ping B and ssh to B.
**
** Computer B has nfsserver running, exporting /home/det/p2_det/images to A;
** A mounts this export on /compB_images.  The private ip addresses and the
** communication directories are hard-coded here since there is no need to
** change them.
**
** Note that the reverse nfs configuration (computer B writing to an NFS
** mount from A) does not work nearly as well.  The setup described will
** record up to 4000 images at 200 Hz without error (300K detector).
**
** /etc/fstab entry on A
**   dec056B:/home/det/p2_det/images	/compB_images nfs async,rw,udp 0 0
** (do  not use 'noac')
**
** /etc/exports entry on B
**   /home/det/p2_det/images dec056A(rw,no_subtree_check,async)
**
**
*/
static int setup_other_computers(void)
{
int err=0;
#ifdef MASTER_COMPUTER

int sfd, status;
char remoteHost[]="10.0.0.2";		// "192.168.0.2" is a bad choice
char remotePort[]="41234";
int sock=1, s;
struct addrinfo hints;
struct addrinfo *result, *rp;

/* following the man page example for getaddrinfo(3) */

/* Obtain address(es) matching host/port */

memset(&hints, 0, sizeof(struct addrinfo));
hints.ai_family = AF_UNSPEC;		/* Allow IPv4 or IPv6 */
hints.ai_socktype = SOCK_STREAM;	/* stream socket */
hints.ai_flags = 0;
hints.ai_protocol = 0;				/* Any protocol */

s = getaddrinfo(remoteHost, remotePort, &hints, &result);
if (s != 0) 
	{
	DBG(1, "*** Error in getaddrinfo(): %s\n", gai_strerror(s))
	printf("*** Error in getaddrinfo(): %s\n", gai_strerror(s));
	return -1;
	}

/* getaddrinfo() returns a list of address structures.
** Try each address until we successfully connect(2).
** If socket(2) (or connect(2)) fails, we (close the socket
** and) try the next address. 
*/

for (rp = result; rp != NULL; rp = rp->ai_next)
	{
	sfd = socket(rp->ai_family, rp->ai_socktype,
            	 rp->ai_protocol);
	if (sfd == -1)
    	continue;
	if (connect(sfd, rp->ai_addr, rp->ai_addrlen) != -1)
    	break;                  /* Success */
	close(sfd);
	}

if (rp == NULL)						/* No address succeeded */
	{
	DBG(1, "Could not connect\n")
	printf("Could not connect\n");
	return -1;
	}

status = fcntl(sfd, F_SETFL, O_NONBLOCK);
if (-1 == status)
	{
	perror("fcntl()");
	return -1;
	}

sockets[sock]=sfd;		// mark this computer as configured
sfds[sock-1].fd=sfd;
sfds[sock-1].events = POLLIN;

freeaddrinfo(result);           /* No longer needed */

#endif
return err;
}




#ifdef MASTER_COMPUTER

/*****************************************************************************\
**                                                                           **
**   tell_other_computer()                                                   **
**                                                                           **
\*****************************************************************************/
/*
** In the case of multiple computers (not multiple CPU's), send setup
** commands over the socket
*/

static int tell_other_computer(int sock, CAMERA_CMND cmnd, char *cmd)
{
ssize_t nread=0;
int n, ret, timeout;
char *p, *q;

if (!sockets[sock])			// not configured
	return -1;

DBG(5, "master socket TX to socket #%d at %s\n", sock, gs_timestamp())
DBG(5, "   TX: %s\n", cmd)

#define POLL_ms 1

sfdBufr[0] = '\0';
if (poll(sfds, N_COMPUTERS-1, POLL_ms) > 0)
	{
	if (sfds[sock-1].revents & POLLERR || sfds[sock-1].revents & POLLHUP)
		{
		printf("Poll reports broken connection to computer %d\n", sock);
		DBG(1, "Poll reports broken connection to computer %d\n", sock)
		return -1;
		}
	if (sfds[sock-1].revents & POLLIN)		// empty the socket
		nread = read(sockets[sock], sfdBufr, sizeof(sfdBufr));
	}

n = write(sockets[sock], cmd, strlen(cmd)+1);
if (n < 0)
	{
	perror("socket write");
	return -1;
	}
else if (n==0)
	{
	printf("socket write failed: broken connection to computer %d?\n", sock);
	DBG(1, "socket write failed: broken connection to computer %d?\n", sock)
	return -1;
	}

timeout = 10*POLL_ms;
switch (cmnd)
	{
	case ImgonlY:		// leave the acknowledgement in the socket
	case ExiT:			// some comands are not acknowledged
	case QuiT:
	case MenU:
#ifdef DEBUG
	case CamNoop:
	case DbglvL:
#endif
		return 0;
		break;

	case Exposure:
	case ExtEnable:
	case ExtTrigger:
	case ExtMTrigger:
		usleep(10000);	// 10 ms
		timeout = 100*POLL_ms;
		break;

	case LdBadPixMap:
	case THread:		// needs extra time to respond
		usleep(50000);
		break;

	case ResetCam:		// "K" was given
		timeout = 7000*POLL_ms;
		if (exposure_time > 5.0)
			timeout = 2000 + (int)1000.0*exposure_time;
		break;
	default:
		timeout = 7000*POLL_ms;		// calibrate, e.g., takes a very long time
		break;
	}

//DBG(5, "Waiting for socket response\n")
ret = poll(sfds, N_COMPUTERS-1, timeout);
if (ret > 0)
	{
	if (sfds[sock-1].revents & POLLERR || sfds[sock-1].revents & POLLHUP)
		{
		printf("Poll reports broken connection to computer %d\n", sock);
		DBG(1, "Poll reports broken connection to computer %d\n", sock)
		return -1;
		}
	if (sfds[sock-1].revents & POLLIN)
		{
		nread = read(sockets[sock], sfdBufr, sizeof(sfdBufr));
//DBG(5, "Socket RX n=%d: %s\n", (int)nread, sfdBufr)
		if (nread == -1)
		  {
		  perror("socket read");
		  return -1;
		  }
	//	printf("Received %ld bytes: %s\n", (long)nread, sfdBufr);
		}
	}
else if (ret == 0)
	{
	printf("*** ERROR - no response from slave computer\n");
	DBG(1, "*** ERROR - no response from slave computer\n");
	return -1;
	}
else
	{
	perror("tell_other_computer()");
	return -1;
	}

// some comands have special processing
switch (cmnd)
	{
	case THread:
		if (nread>0)
			{
			if ( !(q=strstr(sfdBufr, "Temperature")) )
				break;
			if ( !(p=strchr(sfdBufr, 0x18)) )
				break;
			if (p < q)				// catenated messages
				p++;
			else
				p=sfdBufr;			// one message
			if ( !(p = strstr(p, "OK")) )
				break;
			p+=2;
			while (isspace(*p))
				p++;
			printf("From computer %d:\n", sock);
			if ( (q=strchr(p, 0x18)) )
				*q = '\0';
			while ( (q=strchr(p, ';')) )
				{
				*q = '\0';
				printf(" %s\n", p);
				p = q+1;				
				}
			while (isspace(*p))
				p++;
			q = p+strlen(p);
			*q = '\0';
			printf(" %s\n", p);
			}
		break;
	case LdBadPixMap:
		break;
	default:
		break;
	}

return 0;
}



/*****************************************************************************\
**                                                                           **
**   tell_all_other_computers()                                              **
**                                                                           **
\*****************************************************************************/
// Broadcast a command to all other computers

static int tell_all_other_computers(CAMERA_CMND cmnd, char *cmd)
{
int err=0;
int sock=1;

while(sock<N_COMPUTERS)
	{
	err += tell_other_computer(sock, cmnd, cmd);
	sock++;
	}

return err;
}

#endif		//MASTER_COMPUTER



/*****************************************************************************\
**                                                                           **
**   socket_service() -- tx messages and look for 'K'                        **
**                                                                           **
\*****************************************************************************/

#define POLL_ms 1		// polling time-out, ms

static int socket_service(void)
{
int err=0, cnt;
char rxBuffer[LENFN];

// in the masterProcess, ufds[1].fd is not a client socket
if (!masterProcess)
	{
	// receive selected commands and transmit selected data over socket
	if (poll(ufds, 2, POLL_ms) > 0)	// poll for input events
		{
		if (ufds[1].revents & POLLERR || ufds[1].revents & POLLHUP)
			{
			printf("Poll reports socket error\n");	// but, we just go on
			DBG(1, "Poll reports socket error\n")
			ufds[1].fd=0;
			err=1;
			}
		else if (ufds[1].revents == POLLIN)	// input from server socket
			{
			cnt = read(ufds[1].fd, rxBuffer, sizeof(rxBuffer)-1);
			DBG(2, "(interface.c) at %s cnt = %d, Socket RX: %s\n", gs_timestamp(), cnt, rxBuffer)
			if (cnt == 0)		// probably a closed socket
				{
				ufds[1].fd=0;
				strcpy(txBuffer, "13 ERR kill - socket closed?\x18");
				err=1;
				}
			else if (strchr(rxBuffer, 'k') || strchr(rxBuffer, 'K'))	// interpret the buffer
				{
#ifdef MASTER_COMPUTER
				strcpy(txBuffer, "K");
				tell_all_other_computers(ResetCam, txBuffer);
#endif
				strcpy(txBuffer, "13 ERR kill\x18");
				err=1;
				}
			}
		}
	if (strlen(txBuffer) > 0)
		{
		write(ufds[1].fd, txBuffer, strlen(txBuffer));
		txBuffer[0] = '\0';
		}
	}
if (err)
	camera_stop(2);

return err;
}



/*****************************************************************************\
**                                                                           **
**   camera_check_priority()                                                 **
**                                                                           **
\*****************************************************************************/
int camera_check_priority(void)
{
#ifdef SLAVE_COMPUTER
struct sched_param sp;
int ret;

sp.sched_priority = 99;		// for RT kernel
ret = sched_setscheduler(0, SCHED_FIFO, &sp);
ret = sched_getscheduler(0);
if (ret == SCHED_FIFO)
	{
	DBG(3, "Set SCHED_FIFO scheduling policy, priority %d\n",
			sp.sched_priority);
	}
#endif
return 0;
}



// ----------------------------------------------------------------------------

/*****************************************************************************\
**                                                                           **
**   some experimental code concerning I/o priority                          **
**                                                                           **
\*****************************************************************************/


/*  
This was abstracted from the kernel sources:


#include <sys/syscall.h>

enum {
	IOPRIO_CLASS_NONE,
	IOPRIO_CLASS_RT,
	IOPRIO_CLASS_BE,
	IOPRIO_CLASS_IDLE,
	};

enum {
	IOPRIO_WHO_PROCESS = 1,
	IOPRIO_WHO_PGRP,
	IOPRIO_WHO_USER,
	};

#define IOPRIO_CLASS_SHIFT     (13)
#define IOPRIO_PRIO_VALUE(class, data) (((class) << IOPRIO_CLASS_SHIFT) | data)

// Lower the IO priority

pid=getpid();
errno=0;
ret = syscall( SYS_ioprio_set, IOPRIO_WHO_PROCESS, pid,
		IOPRIO_PRIO_VALUE(IOPRIO_CLASS_IDLE, 7) );
if (ret)
	perror("ioptest");


*/
