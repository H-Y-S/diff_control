// mx_parms.h - declarations for parsing MX paramters

#ifndef CALIBS_H
#define CALIBS_H

// prototypes of functions
void show_calibration(char *);
double get_current_threshold(void);
char * get_current_gain(void);
double get_current_vrf(void);
void set_current_threshold(double);
int set_calibration(char *, char *);
void set_calibration_state(char *);


/* start of CALIBS enum - please do not change this comment - lines below will change */
typedef enum {
		Settings = 1,
		Vrf,
		Slope,
		TAu,
		Tempcoeff,
		E,
		FField,
		BPmap,
		ExtraCMD,
		Dir,
		} CALIBS_CMND ;

typedef struct
		{
		CALIBS_CMND cmnd;
		char *name;
		} CALIBS_LIST ;

#ifdef CALIBS_MAIN

static CALIBS_LIST calibs_list[] =
		{
		{ Settings, "Settings" },
		{ Vrf, "Vrf" },
		{ Slope, "Slope" },
		{ TAu, "TAu" },
		{ Tempcoeff, "Tempcoeff" },
		{ E, "E" },
		{ FField, "FField" },
		{ BPmap, "BPmap" },
		{ ExtraCMD, "ExtraCMD" },
		{ Dir, "Dir" },
		} ;

static int calibs_count = sizeof(calibs_list)/sizeof(CALIBS_LIST);

/* end of variable data - please do not change this comment */

#endif

#endif
