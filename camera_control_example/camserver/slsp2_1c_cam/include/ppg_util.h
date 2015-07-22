// ppg_util.h - declarations related to pattern generator and fifo

#ifndef PPG_UTIL_H
#define PPG_UTIL_H

#include "pix_detector.h"


//#define HOST "ioc038"
//#define HOST "ioc062"
#define MIN_CLOCK 1					// clock divider
#define MAX_CLOCK 15				// clock divider
//#define I2C_PATTERN_BASE 0x7800		// = n * 0x80, n >= 1

// following are defined in detsys.h, included by pix_detector.h
//#define NBITS 15					// # of bits in pixel counter
//#define NROWS_MOD 157				// includes double pixels in sensor
//#define NCOLS_MOD 366
//#define IMAGE_SIZE NROWS_MOD*NCOLS_MOD
//#define NCOLS 44					// single chip
//#define NROWS 78
//#define NCHIPS 16

// Programming trim bits with settrm3.abs pattern
//#define BASE_SETTRIM_PATTERN 0x5800		// address in vpg for trim 1 pixel
#define OFFSET_SETTRIM_NXTR 0x80		// offset in pattern for NXTR entry
#define OFFSET_SETTRIM_PATTERN 0x180	// offset in pattern to pixel load part
#define OFFSET_SETTRIM_PSEL 0x200		// offset in pattern for PSEL signal

// for sls06
// Programming trim bits with trmall.abs pattern
//#define BASE_TRIM_ALL_PATTERN 0x7000	// address in vpg for trim all pix
/*#define OFFSET_TRIM_ALL_PATTERN 0x80	// offset in pattern to pixel load part

#define LINE_TRIM_BIT3 0x18			// line for bit 2 = MSB (no. 0 is first)
#define LINE_TRIM_BIT2 0xc			// line for bit 8
#define LINE_TRIM_BIT1 0x0			// line for bit 14
#define LINE_TRIM_BIT0 0x2			// line for bit 13

// Words to program 1's or 0's - sls06
#define TRIM_WORD_1_FOR_0 0x3bfc0000	// to program a 0 - first word
#define TRIM_WORD_2_FOR_0 0x7bfc0000	// to program a 0 - second word
#define TRIM_WORD_1_FOR_1 0xbbfc0000	// to program a 1 - first word
#define TRIM_WORD_2_FOR_1 0xfbfc0000	// to program a 1 - second word
*/

// for p2_1chip
// CHB, 26.4.05
// Programming trim bits with trmall.abs pattern
//#define BASE_TRIM_ALL_PATTERN 0x7000	// address in vpg for trim all pix
#define OFFSET_TRIM_ALL_PATTERN 0x0	// offset in pattern to pixel load part

#define LINE_TRIM_BIT5 0x11e			// MSB 
#define LINE_TRIM_BIT4 0x120			
#define LINE_TRIM_BIT3 0x11a			
#define LINE_TRIM_BIT2 0x114			
#define LINE_TRIM_BIT1 0x112			
#define LINE_TRIM_BIT0 0x11c			// LSB

// Words to program 1's or 0's - sls06
#define TRIM_WORD_1_FOR_0 0x44c30000	// to program a 0 - first word
#define TRIM_WORD_2_FOR_0 0x04c30000	// to program a 0 - second word
#define TRIM_WORD_1_FOR_1 0xc4c30000	// to program a 1 - first word
#define TRIM_WORD_2_FOR_1 0x84c30000	// to program a 1 - second word


// global variables
extern unsigned int i2c_pattern_base;
extern unsigned int start_line;
extern unsigned int xor_table[];
extern int xor_tab_len;
extern unsigned int theImage[];
extern int imageLength;
extern int fifo_timeout;
extern int i2c_vpg_unit;

extern char outFileName[];
extern unsigned short thePattern[];
extern unsigned short baseWord;
extern float clock_frequency;
extern float vpg_crystal_frequency;
extern int autoExec;
extern int pgRunning[];
extern int filenumber;
extern char autofilename[];
extern short trimValues[][NCOL_CHIP];
extern char trimFileName[];
extern char datfile[];
extern char imgfile[];
extern int loadingTrimFile;
extern int trimallVal;
extern int selected_chip;
//extern int selected_mod;		// not SLS8x2_CAMERA
//extern int selected_bank;		// not SLS8x2_CAMERA
extern unsigned int base_vpg_ena_pattern;
extern unsigned int base_read_chip_pattern0;
extern unsigned int base_read_chip_pattern1;
extern unsigned int base_enable_pattern;
extern unsigned int base_disable_pattern;
extern unsigned int base_trim_all_pattern;
extern int time_lim;

// prototypes of functions

// functions defined in ppg_util.c
void make_test_image(int);
int set_i2c(char *);
int prog_i2c(char *);
int reset_i2c_chsel(char *);
int set_canonical_form(char *);
void setup_pattern(void);
void write_pattern(char *, char *);
void load_configuration(char *);
void save_configuration(char *);
void setTrims(void);
void setPatternTrimBits(int, unsigned int, int);
void init_trim_array(int);
void log_i2c_settings(FILE *, char *);
void get_vcmpall(char *, int, int);
void get_vrfall(char *, int, int);
void get_vrfsall(char *, int, int);
void get_vtrmall(char *, int, int);

// functions defined in rpc_client.c
int vpg_execute_pattern(unsigned short [], int, int);
int vpg_start_clock(int);
int vpg_stop_clk(int);
int vpg_pc_reset(int);
int vpg_set_jump(unsigned int, int);
int vpg_set_clk_divider(int, int);
int vpg_load_pattern(char *, int);
int vpg_init(void);
int vpg_clear(int);
int vpg_step(int);
unsigned int vpg_read(unsigned int, int);
void vpg_read_p(unsigned int, int);
unsigned int vpg_read_status(int);
void vpg_read_status_p(int);
int vpg_write(unsigned int, unsigned int, int);
int vpg_write_status(unsigned int, int);
int vpg_set_default(int);
void vpg_wait_pattern(int, int);

int fifo_init(void);
int fifo_read(unsigned int);
int fifo_read_status(void);
void fifo_read_status_p(void);
int fifo_read_event(void);
int fifo_read_data(void);
int fifo_write(unsigned int, unsigned int);
int fifo_write_control(unsigned int);
int fifo_write_test(unsigned int);
int fifo_write_data(unsigned int);
int fifo_read_image(void);
int fifo_wait_busy(int);
int write_image_nfs(char *, int);
int write_n_images_nfs(char *, unsigned int, unsigned int);
int fifo_set_true_level(int);

#endif		// PPG_UTIL_H
