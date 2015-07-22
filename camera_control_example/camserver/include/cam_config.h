/* cam_config.h - definitions of enums and structures for cam_config.c
** This file, below "start of CAM_CONFIG ...", is regenerated automatically 
** using this present file as a template, by misc/mkheader, called in the 
** Makefile.
*/

// Note that this header allocates space, something you should not do in a
// header file.  But, this is a one-use only header, so this doesn't cause
// problems.

#ifndef CAM_CONFIG_H
#define CAM_CONFIG_H

#include <poll.h>
#include "camserver.h"

#define SYSTEM_CAM_CONFIG_FILE "/usr/local/etc/camrc"
#define LOCAL_CAM_CONFIG_FILE "./camrc"
#define USER_CAM_CONFIG_FILE "$HOME/.camrc"


typedef enum {
	CamExposing,
	CamPreparing,
	CamWaiting,
	CamIdle } CAM_STATE;

typedef enum {
		cam_def_file,
		cam_name,
		cam_state,
		cam_target,
		cam_completed,
		cam_time,
		cam_exptime,
		cam_shutter,
		cam_telemetry,
		cam_variables,
		cam_controlling_pid
		} CAM_STATUS;

// prototypes of functions
void set_cam_stat(CAM_STATUS, char *);		// cam_config.c
int print_cam_setup(char *);				// cam_config.c
void cam_config_initialize(char *);			// cam_config.c
void camera_update_variables(void);			// cam_config.c
int camera_readout(char *, char *);			// interface.c
void shutter_on_off(int);					// interface.c
int camera_prepare(double *);				// interface.c
int camera_initialize(void);				// interface.c
int camera_start(double *);					// interface.c
int camera_stop(int);						// interface.c
void camera_read_telemetry(char *, int);	// interface.c
int camera_check_status(double);			// interface.c
void camera_reset(void);					// interface.c
int camera_read_setup(char *, char *);		// interface.c
void camera_shutdown(void);					// interface.c
void camera_monitor(void);					// interface.c
int camera_status(int);						// interface.c
int camera_check_priority(void);			// interface.c
CAMERA_CMND camera_cmnd_filter(CAMERA_CMND, char *, int);	// interface.c

// Global variables
extern int masterProcess;
extern int controllingProcess;
extern char camstat_path[];
extern char txBuffer[];
extern int camPort;
extern char cam_data_path[];
extern char cam_image_path[];
extern char cam_initial_path[];
extern char cam_startup_file[];
extern char header_string[];
extern CAM_STATE Cam_State;
extern char image_file[];
extern double endTime;
extern double startTime;
extern double exposure_time;
extern struct pollfd ufds[];

/* start of CAM_CONFIG enum - please do not change this comment - lines below will change */
typedef enum {
		Cam_image_mode = 1,
		Cam_tau,
		Cam_threshold_setting,
		Cam_gapfill,
		Cam_badpixmap,
		Cam_flatfield,
		Cam_ackinterval,
		Cam_trim_directory,
#if 0
		Cam_ExpPeriod,
		Cam_NImages,
		Cam_NExpFrame,
		Cam_NFrameImg,
		Cam_Delay,
#endif
		Camstat_path,
		Cam_definition_file,
		Cam_data_path,
		Cam_exposure_time,
		Cam_image_path,
		Cam_startup_file,
		Camera_port_number,
#ifdef DEBUG
		Dbglvl,
#endif
		} CAM_CONFIG_CMND ;

typedef struct
		{
		CAM_CONFIG_CMND cmnd;
		char *name;
		} CAM_CONFIG_LIST ;

#ifdef CAM_CONFIG_MAIN

static CAM_CONFIG_LIST cam_config_list[] =
		{
		{ Cam_image_mode, "Cam_image_mode" },
		{ Cam_tau, "Cam_tau" },
		{ Cam_threshold_setting, "Cam_threshold_setting" },
		{ Cam_gapfill, "Cam_gapfill" },
		{ Cam_badpixmap, "Cam_badpixmap" },
		{ Cam_flatfield, "Cam_flatfield" },
		{ Cam_ackinterval, "Cam_ackinterval" },
		{ Cam_trim_directory, "Cam_trim_directory" },
#if 0
		{ Cam_ExpPeriod, "Cam_ExpPeriod" },
		{ Cam_NImages, "Cam_NImages" },
		{ Cam_NExpFrame, "Cam_NExpFrame" },
		{ Cam_NFrameImg, "Cam_NFrameImg" },
		{ Cam_Delay, "Cam_Delay" },
#endif
		{ Camstat_path, "Camstat_path" },
		{ Cam_definition_file, "Cam_definition_file" },
		{ Cam_data_path, "Cam_data_path" },
		{ Cam_exposure_time, "Cam_exposure_time" },
		{ Cam_image_path, "Cam_image_path" },
		{ Cam_startup_file, "Cam_startup_file" },
		{ Camera_port_number, "Camera_port_number" },
#ifdef DEBUG
		{ Dbglvl, "Dbglvl" },
#endif
		} ;

static int cam_config_count = sizeof(cam_config_list)/sizeof(CAM_CONFIG_LIST);

/* end of variable data - please do not change this comment */

#endif

// prototypes of functions, contd.
void camera_record_variable(CAM_CONFIG_CMND, char *);	// cam_config.c

#endif
