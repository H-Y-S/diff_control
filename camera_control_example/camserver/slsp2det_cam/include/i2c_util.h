// i2c_util.h - declarations related to pattern generator and fifo

#ifndef I2C_UTIL_H
#define I2C_UTIL_H

#include "pix_detector.h"


// global variables in i2c_util.c
extern int filenumber;
extern char autofilename[];
extern short trimValues[][NMOD_BANK][NCHIP][NROW_CHIP][NCOL_CHIP];
extern char trimFileName[];
extern char datfile[];
extern char imgfile[];
extern int loadingTrimFile;
extern int trimallVal;
extern int selected_chip;
extern int selected_mod;		// not in 1 mod camera
extern int selected_bank;		// not in 1 mod camera
extern int time_lim;
extern int dac_offset_comp_enable;

// prototypes of functions

// functions defined in i2c_util.c
int set_i2c(char *);
int prog_i2c(char *);
int reset_i2c_chsel(char *);
int set_canonical_form(char *);
void load_configuration(char *);
void save_configuration(char *);
void log_i2c_settings(FILE *, char *);
void get_vcmpall(char *, int, int);
void get_vrfall(char *, int, int);
void get_vrfsall(char *, int, int);
void get_vtrmall(char *, int, int);
void apply_deltaV(char *);


#endif		// I2C_UTIL_H
