/* ./demo_cam/util/test1.c -- its sole function is to test linking in ../util */

#include <stdio.h>
#include "debug.h"
#include "cam_config.h"
#include "camserver.h"

#ifdef DEBUG
FILE *dbgofp;   // global output file
int dbglvl;     // debugging control level
#endif

// globals
char camstat_path[LENFN] = "\0";
char cam_data_path[LENFN] = "./cam_data/";
char cam_image_path[LENFN] = "./images/";
char cam_initial_path[LENFN] = "./";
char cam_startup_file[LENFN] = "\0";
int camPort;
double exposure_time;
int masterProcess = True;


int main (int argc, char *argv[])

{
char line[20], line2[20];
int key=0;
double a=0;

printf("Starting test\n");

#ifdef DEBUG
// Start debugger
        debug_init("debug.out", NULL);
        dbglvl=9;               // full details
#endif


if (key)
	shutter_on_off(0);
	
if (key)
	camera_prepare(&a);
	
if (key)
	camera_initialize();
	
if (key)
	camera_start(&a);

if (key)
	camera_read_telemetry(line, sizeof(line));

if (key)
	camera_reset();

if (key)
	camera_read_setup(line, line2);



printf("Test completed\n");
return 0;
}


// dummy routines
void read_detector_info(void)
{return;}
