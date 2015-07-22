// cam_tbl.h - declarations of camera-specific variables
// definitions of enums and structures for cam_tbl.c

// specific for:  tvx slsp2det camera

#ifndef CAM_TBL_H
#define CAM_TBL_H

#include "detsys.h"

// prototypes of functions
// ---- in cam_tbl.c
void read_camera_setup(char *, char *);

// global variables
extern char camera_name[];
extern char cam_definition[];
extern int camera_wide;
extern int camera_high;
extern int camera_bpp;

extern char camera_sensor[];
extern char camera_pixel_xy[50];
extern char detector_calibration_file[];


/* start of CAM_TBL enum - please do not change this comment - lines below will change */
typedef enum {
		Camera_name = 1,
		Camera_wide,
		Camera_high,
		Camera_bpp,
		Camera_sensor,
		Camera_pixel_xy,
		I2c_configuration_file,
		Detector_map,
		Detector_offsets,
		Detector_calibration_file,
		} CAM_TBL_CMND ;

typedef struct
		{
		CAM_TBL_CMND cmnd;
		char *name;
		} CAM_TBL_LIST ;

#ifdef CAM_TBL_MAIN

static CAM_TBL_LIST cam_tbl_list[] =
		{
		{ Camera_name, "Camera_name" },
		{ Camera_wide, "Camera_wide" },
		{ Camera_high, "Camera_high" },
		{ Camera_bpp, "Camera_bpp" },
		{ Camera_sensor, "Camera_sensor" },
		{ Camera_pixel_xy, "Camera_pixel_xy" },
		{ I2c_configuration_file, "I2c_configuration_file" },
		{ Detector_map, "Detector_map" },
		{ Detector_offsets, "Detector_offsets" },
		{ Detector_calibration_file, "Detector_calibration_file" },
		} ;

static int cam_tbl_count = sizeof(cam_tbl_list)/sizeof(CAM_TBL_LIST);

/* end of variable data - please do not change this comment */

#endif


#endif
