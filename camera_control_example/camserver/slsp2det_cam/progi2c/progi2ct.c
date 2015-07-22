// dac1t.c - program to test dac1, the pattern composer

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include "i2c_util.h"
#include "pix_detector.h"
#include "debug.h"

#define DEF_FILE "../cam_data/p2_1mod.def"

#ifdef DEBUG
FILE *dbgofp;   // global output file
int dbglvl;     // debugging control level
#endif

char cam_data_path[80] = "./";
char cam_image_path[80] = "./";
char camstat_path[80] = "./";
double exposure_time;
int n_bad_pix;



// Main program to test pattern composer

int main (int argc, char *argv[])
{
int ret, key=0;
char line[20];

#ifdef DEBUG
debug_init("debug.out", NULL);   // turn on debugging
dbglvl = 9;     // and turn it all the way up
#endif

// the following check on linking
if(key)
	set_canonical_form(line);

DBG(5, "Debugging started\n");

read_detector_info(DEF_FILE);	// fill the structures from the detector definition

ret = set_dac("B5_M1_VCMP3", 0.85);
if (ret)
	printf("Error return from pattern composer\n");
else
	printf("Successful return from the pattern composer\n");


#ifdef DEBUG
fclose(dbgofp);
#endif

return 0;
}
