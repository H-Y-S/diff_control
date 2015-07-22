/* detsys.h - declarations specific to the slsp2_1c detector system
*/

#ifndef DETSYS_H
#define DETSYS_H

//#define USE_GPIB				// gpib multimeter and oscilloscope
//#define SP8_BL38B1			// SPring8 BL38B1 options

#define NBITS 20
#define NSTATES (1<<NBITS)	// 2**NBITS
#define fbBIT 2				// feed-back bit to use in additon to last bit

// 32-bit images
typedef unsigned int TYPEOF_PIX;

#define FIFO_DATA_TRUE_LEVEL 1	// accounts for data inversion

#define MAX_TRIM_VALUE 63

#define NROW_CHIP 97
#define NCOL_CHIP 60
#define NCHIP 1

#define NROW_MOD NROW_CHIP
#define NCOL_MOD NCOL_CHIP

#define NMOD_BANK 1
#define NBANK 1

#define DAC_BITS_8

/* ------------------------------------------ */

#define IMAGE_NCOL NCOL_MOD
#define IMAGE_SIZE NROW_MOD*NCOL_MOD
#define IMAGE_NBYTES IMAGE_SIZE*4

// in rpc_client.c
extern unsigned int selectors[];
extern char iocHost[];
extern int clock_divider1;
extern int clock_divider2;
extern int img_mode;
extern int n_images;
extern int vpg_default_unit;

// function prototypes - in cutils.c
int write_image_pci(char *, int, int);
void xor_decode_image(int);
void truncate_image(unsigned int *, int);
struct module_pixel *pix_add(int, int, int, int, int, int);
void moduleFilename(char *, char *, char *, int, int);
void log_gpib_readings(FILE *);

// funtion protoypes - in pci_utils.c
int write_image_pci(char *, int, int);
int record_n_images_pci(char *, int);

// function prototypes - in interface.c
int camera_vpg_start(double);

#ifdef USE_GPIB
void gpibsend_vo(char *);
void gpibsend_os(char *);
void gpibread_vo(void);
void gpibread_os(char *);
#endif

#endif	//DETSYS_H 
