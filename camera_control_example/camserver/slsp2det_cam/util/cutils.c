// cutils.c - miscellaneous utilities for the sls cameras

#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <math.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "camserver.h"
#include "cam_config.h"
#include "detsys.h"
#include "tvxobject.h"
#include "imageio.h"
#include "vxi11core.h"
#include "vxi11.h"
#include "debug.h"


// firmware serial-to-parallel conversion - all versions now use this
#define FIRMWARE_PARALLEL_CONVERSION


/******************************************************************************\
**                                                                            **
**          Global variables                                                  **
**                                                                            **
\******************************************************************************/
TYPEOF_PIX theImage[IMAGE_SIZE];
unsigned int selectors[]={
	0x1,  0x2,   0x4,   0x8,   0x10,   0x20,   0x40,   0x80,
	0x100, 0x200, 0x400, 0x800, 0x1000, 0x2000, 0x4000, 0x8000, 0
	};
int img_mode=0;
int n_images=1;
int readout_bits=20;
int gap_fill_byte=0;
int *bad_pix_list=NULL;
char bad_pix_file_name[LENFN]="";
int n_bad_pix;
void *flat_field_data=NULL;		// can be float or unsigned int
char flat_field_file_name[LENFN]="";
char trim_directory[LENFN]="";
int nfrmpim=1;					// N frames per image
double set_energy_setting=0.0;

#ifdef SLSP2DET_CAMERA
short trimValues[NBANK][NMOD_BANK][NCHIP][NROW_CHIP][NCOL_CHIP];
#else
 #error
#endif

#ifdef ENABLE_DECTRIS_LOGO
TYPEOF_PIX dectris_logo[MOD_SIZE];
#endif


// declarations
static void convert_module(char *, TYPEOF_PIX *);
static void convert_module_add(char *, TYPEOF_PIX *);
static void gap_fill_module(TYPEOF_PIX *);

/*****************************************************************************\
**                                                                           **
**    initialize_theImage()                                                  **
**                                                                           **
\*****************************************************************************/
void initialize_theImage(int c)
{
int bank, mod, y;
char *p = (char *)theImage;

memset(theImage, c, sizeof(theImage));
if (c==0 || nfrmpim==1)
	return;

// zero the modules
for (bank = 1; bank <= NBANK; bank++)
	{
	for (mod = 1; mod <= NMOD_BANK; mod++)
		{
		px=pix_add(-1,-1,-1,bank,-1,-1,mod,0,0,-1,-1,-1);
		p = (char *)(theImage+px->pix);
		for (y=0; y<NROW_MOD; y++)
			{
			memset(p, 0, NCOL_MOD*sizeof(TYPEOF_PIX));
			p += NCOL_BANK*sizeof(TYPEOF_PIX);
			}
		}
	}
return;
}



/*****************************************************************************\
**                                                                           **
**    convert detector data to an image                                      **
**                                                                           **
\*****************************************************************************/

#define MODSIZE ((NROW_CHIP * NCOL_CHIP * NBITS * NCHIP)/8)

void convert_image(char *data)
{
int bank, mod;

for (bank = 1; bank <= NBANK; bank++)
	{
	for (mod = 1; mod <= NMOD_BANK; mod++)
		{
		px=pix_add(-1,-1,-1,bank,-1,-1,mod,0,0,-1,-1,-1);
		convert_module(data, theImage+px->pix);
		data += MODSIZE;
		}
	}

gap_fill_image();		// not summing, so fill immediately
return;
}

/* ------------------------------------------------- */

void convert_image_add(char *data)
{
int bank, mod;

for (bank = 1; bank <= NBANK; bank++)
	{
	for (mod = 1; mod <= NMOD_BANK; mod++)
		{
		px=pix_add(-1,-1,-1,bank,-1,-1,mod,0,0,-1,-1,-1);
		convert_module_add(data, theImage+px->pix);
		data += MODSIZE;
		}
	}

return;
}



/*****************************************************************************\
**                                                                           **
**    carry out serial-parallel and xor conversion on a module image         **
**                                                                           **
\*****************************************************************************/

static void convert_module(char *p, TYPEOF_PIX *modP)
{
int k, row, col;
unsigned int *cP[16];
#ifdef FIRMWARE_PARALLEL_CONVERSION
  union	{unsigned long long *ullp;		// aliases for a storage location
		unsigned int *uip;
		char *cp;
		} d;
  d.cp = p;
#else
  int j, tpix[16], bit_mask;
  unsigned short *usp = (unsigned short *)p;
#endif

if (!p)
	return;

// set up chip pointers (cP[]) to pixel (0,0) of the 16 chips in the module
// modP points to the upper left corner of the module position in theImage
// for p2, chips are read out most significant bit first
for (k = 0; k < 8; k++)
	{
	cP[k] = modP + (NCOL_CHIP - 1) + k*(NCOL_CHIP + NCOL_BTW_CHIP);
	cP[k+8] = modP + (NROW_MOD-1)*NCOL_BANK + k*(NCOL_CHIP + NCOL_BTW_CHIP);
	}
for (row = 0; row < NROW_CHIP; row++)
	{
	for (col = 0; col < NCOL_CHIP; col++)
		{

#ifdef FIRMWARE_PARALLEL_CONVERSION

// The rate correction is built into the xor_table

		*cP[0] = xor_table[(*d.uip) & 0xfffff];
		*cP[1] = xor_table[(*d.ullp >> 20) & 0xfffff];
		d.uip++;
		*cP[2] = xor_table[(*d.uip >> 8) & 0xfffff];
		*cP[3] = xor_table[(*d.ullp >> 28) & 0xfffff];
		d.uip++;
		*cP[4] = xor_table[(*d.ullp >> 16) & 0xfffff];
		d.uip++;
		*cP[5] = xor_table[(*d.uip >> 4) & 0xfffff];
		*cP[6] = xor_table[(*d.ullp >> 24) & 0xfffff];
		d.uip++;
		*cP[7] = xor_table[(*d.uip >> 12) & 0xfffff];
		d.uip++;

		*cP[8] = xor_table[(*d.uip) & 0xfffff];
		*cP[9] = xor_table[(*d.ullp >> 20) & 0xfffff];
		d.uip++;
		*cP[10] = xor_table[(*d.uip >> 8) & 0xfffff];
		*cP[11] = xor_table[(*d.ullp >> 28) & 0xfffff];
		d.uip++;
		*cP[12] = xor_table[(*d.ullp >> 16) & 0xfffff];
		d.uip++;
		*cP[13] = xor_table[(*d.uip >> 4) & 0xfffff];
		*cP[14] = xor_table[(*d.ullp >> 24) & 0xfffff];
		d.uip++;
		*cP[15] = xor_table[(*d.uip >> 12) & 0xfffff];
		d.uip++;

		for (k = 0; k < 8; k++)					// move to next col
			{
			cP[k]--;
			cP[k+8]++;
			}

#else		// software serial-to-parallel conversion

		memset(tpix, 0, sizeof(tpix));			// 16 temporary pixels
		bit_mask = 1<<(NBITS-1);				// point to msb in word
		for (j = 0; j < NBITS; j++)				// serial data NBITS deep
			{
			for (k = 0; k < 16; k++)			// for 16 columns of bits
				if (*usp & selectors[k])
					tpix[k] |= bit_mask;
			usp++;
			bit_mask >>= 1;						// point to next bit
			}
		for (k = 0; k < 16; k++)
			*cP[k] = xor_table[tpix[k]];		// xor and store in image
		for (k = 0; k < 8; k++)					// move to next col
			{
			cP[k]--;
			cP[k+8]++;
			}
#endif

		}
	for (k = 0; k < 8; k++)						// move to next row
		{
		cP[k] += NCOL_BANK + NCOL_CHIP;
		cP[k+8] -= NCOL_BANK + NCOL_CHIP;
		}
	}

return;
}



/*****************************************************************************\
**                                                                           **
**    carry out serial-parallel and xor conversion on a module image         **
**    add the result to a multi-frame image rather than replacing            **
**                                                                           **
\*****************************************************************************/

static void convert_module_add(char *p, TYPEOF_PIX *modP)
{
int k, row, col;
unsigned int *cP[16];
#ifdef FIRMWARE_PARALLEL_CONVERSION
  union	{unsigned long long *ullp;		// aliases for a storage location
		unsigned int *uip;
		char *cp;
		} d;
  d.cp = p;
#else
  int j, tpix[16], bit_mask;
  unsigned short *usp = (unsigned short *)p;
#endif

if (!p)
	return;

// set up chip pointers (cP[]) to pixel (0,0) of the 16 chips in the module
// modP points to the upper left corner of the module position in theImage
// for p2, chips are read out most significant bit first
for (k = 0; k < 8; k++)
	{
	cP[k] = modP + (NCOL_CHIP - 1) + k*(NCOL_CHIP + NCOL_BTW_CHIP);
	cP[k+8] = modP + (NROW_MOD-1)*NCOL_BANK + k*(NCOL_CHIP + NCOL_BTW_CHIP);
	}
for (row = 0; row < NROW_CHIP; row++)
	{
	for (col = 0; col < NCOL_CHIP; col++)
		{

#ifdef FIRMWARE_PARALLEL_CONVERSION

// The rate correction is built into the xor_table

		*cP[0] += xor_table[(*d.uip) & 0xfffff];
		*cP[1] += xor_table[(*d.ullp >> 20) & 0xfffff];
		d.uip++;
		*cP[2] += xor_table[(*d.uip >> 8) & 0xfffff];
		*cP[3] += xor_table[(*d.ullp >> 28) & 0xfffff];
		d.uip++;
		*cP[4] += xor_table[(*d.ullp >> 16) & 0xfffff];
		d.uip++;
		*cP[5] += xor_table[(*d.uip >> 4) & 0xfffff];
		*cP[6] += xor_table[(*d.ullp >> 24) & 0xfffff];
		d.uip++;
		*cP[7] += xor_table[(*d.uip >> 12) & 0xfffff];
		d.uip++;

		*cP[8] += xor_table[(*d.uip) & 0xfffff];
		*cP[9] += xor_table[(*d.ullp >> 20) & 0xfffff];
		d.uip++;
		*cP[10] += xor_table[(*d.uip >> 8) & 0xfffff];
		*cP[11] += xor_table[(*d.ullp >> 28) & 0xfffff];
		d.uip++;
		*cP[12] += xor_table[(*d.ullp >> 16) & 0xfffff];
		d.uip++;
		*cP[13] += xor_table[(*d.uip >> 4) & 0xfffff];
		*cP[14] += xor_table[(*d.ullp >> 24) & 0xfffff];
		d.uip++;
		*cP[15] += xor_table[(*d.uip >> 12) & 0xfffff];
		d.uip++;

		for (k = 0; k < 8; k++)					// move to next col
			{
			cP[k]--;
			cP[k+8]++;
			}

#else		// software serial-to-parallel conversion

		memset(tpix, 0, sizeof(tpix));			// 16 temporary pixels
		bit_mask = 1<<(NBITS-1);				// point to msb in word
		for (j = 0; j < NBITS; j++)				// serial data NBITS deep
			{
			for (k = 0; k < 16; k++)			// for 16 columns of bits
				if (*usp & selectors[k])
					tpix[k] |= bit_mask;
			usp++;
			bit_mask >>= 1;						// point to next bit
			}
		for (k = 0; k < 16; k++)
			*cP[k] += xor_table[tpix[k]];		// xor and store in image
		for (k = 0; k < 8; k++)					// move to next col
			{
			cP[k]--;
			cP[k+8]++;
			}
#endif

		}
	for (k = 0; k < 8; k++)						// move to next row
		{
		cP[k] += NCOL_BANK + NCOL_CHIP;
		cP[k+8] -= NCOL_BANK + NCOL_CHIP;
		}
	}
return;
}



/*****************************************************************************\
**                                                                           **
**    gap_fill_image() - fill in the pseudo-pixels between chips             **
**                                                                           **
\*****************************************************************************/

void gap_fill_image(void)
{
int bank=1, mod=1, *t=bad_pix_list, x, y;
TYPEOF_PIX *p = theImage;
#ifdef INTEGER_FLAT_FIELD_METHOD		// ~6x faster than floating point
 unsigned int *uif = (unsigned int *)flat_field_data;
#else
 float *ff = (float *)flat_field_data;
#endif

if (!img_mode)			// if pulse mode
	return;

#ifdef MASTER_COMPUTER
  px=pix_add(-1,-1,-1,bank,-1,-1,mod,0,0,-1,-1,-1);
  gap_fill_module(theImage+px->pix);
#else
for (bank = 1; bank <= NBANK; bank++)
	{
	for (mod = 1; mod <= NMOD_BANK; mod++)
		{
		px=pix_add(-1,-1,-1,bank,-1,-1,mod,0,0,-1,-1,-1);
		gap_fill_module(theImage+px->pix);
		}
	}
#endif

/*
** The integer flat-field algorithm stores the correction data, which are generally
** floating point values near 1.0, as integers representing the correction data
** multiplied by, e.g., 1024.  Since the image data are integers, all manipulation
** can be doe with integer aritmetic and bit-shift operations.  The resulting
** method is almost 6x faster than floating point, but limited to 0.1% precision.
*/

// optional flat-field correction
#ifdef INTEGER_FLAT_FIELD_METHOD

#ifdef MASTER_COMPUTER
if (uif)							// only the first module
	for(y=0; y<NROW_MOD; y++)
		{
		uif = (unsigned int *)flat_field_data + y*IMAGE_NCOL;
		p = theImage + y*IMAGE_NCOL;
		for(x=0; x<NCOL_MOD; x++,uif++,p++)
			if ((int)*p>0)			// preserve fill (-1 or 0)
				*p = ((*uif)*(*p)+(1<<(-1+INTEGER_FF_SHIFT)))>>INTEGER_FF_SHIFT;
		}
#else
if (uif)
	for(y=0; y<IMAGE_NROW; y++)
		for (x=0; x<IMAGE_NCOL; x++,uif++,p++)
			if ((int)*p>0)			// preserve fill (-1 or 0)
				*p = ((*uif)*(*p)+(1<<(-1+INTEGER_FF_SHIFT)))>>INTEGER_FF_SHIFT;

#endif		// MASTER_COMPUTER

#else		// INTEGER_FLAT_FIELD_METHOD is not defined

// interestingly, casting *p to double is slightly faster (~4%)
// than casting to float using -O2 optimization
#ifdef MASTER_COMPUTER
if (ff)							// only the first module
	for(y=0; y<NROW_MOD; y++)
		{
		ff = (float *)flat_field_data + y*IMAGE_NCOL;
		p = theImage + y*IMAGE_NCOL;
		for(x=0; x<NCOL_MOD; x++,ff++,p++)
			if ((int)*p>0)			// preserve fill (-1 or 0)
				*p = (TYPEOF_PIX)rint((*ff)*(double)(*p));
		}
#else
if (ff)
	for(y=0; y<IMAGE_NROW; y++)
		for (x=0; x<IMAGE_NCOL; x++,ff++,p++)
			if ((int)*p>0)			// preserve fill (-1 or 0)
				*p = (TYPEOF_PIX)rint((*ff)*(double)(*p));
#endif		// MASTER_COMPUTER

#endif		// INTEGER_FLAT_FIELD_METHOD

// optional bad pixel tagging
if (t)
	while(*t)
		theImage[*t++] = BAD_PIX_FLAG;

return;
}



// fill in virtual pixels from the pixels that received the counts
// fill in for large sensor pixels by dividing counts
// but do not lose the remainder
// use random numbers to distribute the odd counts stochastically - each left-
// over count (1 or 2) has 2/3 probability to land in the real pixel, 1/3
// probability to land in the pseudo-pixel - i.e., in proportion to their
// relative areas.

static void gap_fill_module(TYPEOF_PIX *modP)
{
int i, k, n, t;

#ifdef ENABLE_DECTRIS_LOGO
 TYPEOF_PIX *pp, *qq;
 int row, col;
#endif

k = (NROW_MOD/2)*NCOL_BANK;		// the line across the center of the sensor
for (i=0; i<NCOL_MOD; i++)
	{
	t = modP[k + i - NCOL_BANK];				// from pixel above
	if (t >= count_cutoff)
		modP[k + i] = count_cutoff;				// the pseudo-pixel
	else
		{
		n = t/3;
		modP[k + i] = n;						// the pseudo-pixel
		modP[k + i - NCOL_BANK] = n<<1;			// pixel above
		switch (t%3)
			{
			case 2:								// 2 counts left over
				if ((rand())%3)					// 2/3 probability
					modP[k + i - NCOL_BANK]++;
				else							// 1/3 probability
					modP[k + i]++;
			case 1:								// 1 count left over
				if ((rand())%3)					// 2/3 probability
					modP[k + i - NCOL_BANK]++;
				else							// 1/3 probability
					modP[k + i]++;
				break;
			case 0:
			default:
				break;
			}
		}
	t = modP[k + i + NCOL_BANK];			// from pixel below
	if (t >= count_cutoff)
		modP[k + i] = count_cutoff;			// the pseudo-pixel
	else
		{
		n = t/3;
		if (modP[k + i]+n < count_cutoff)
			modP[k + i] += n;				// the pseudo-pixel
		else
			modP[k + i] = count_cutoff;
		modP[k + i + NCOL_BANK] = n<<1;		// pixel below
		switch (t%3)
			{
			case 2:
				if ((rand())%3)
					modP[k + i + NCOL_BANK]++;
				else if (modP[k + i] < count_cutoff)
					modP[k + i]++;
			case 1:
				if ((rand())%3)
					modP[k + i + NCOL_BANK]++;
				else if (modP[k + i] < count_cutoff)
					modP[k + i]++;
			case 0:
			default:
				break;
			}
		}
	}
for (i=0; i<NROW_MOD; i++)		// the lines between chips, including the central line
	for (k=0; k<7; k++)			// 7 vertical lines
		{
		t = modP[i*NCOL_BANK + (k+1)*(NCOL_CHIP+NCOL_BTW_CHIP) - 2];				// from pixel to left
		if (t >= count_cutoff)
			modP[i*NCOL_BANK + (k+1)*(NCOL_CHIP+NCOL_BTW_CHIP) - 1] = count_cutoff;	// pseudo-pixel
		else
			{
			n = t/3;
			modP[i*NCOL_BANK + (k+1)*(NCOL_CHIP+NCOL_BTW_CHIP) - 1] = n;			// pseudo-pixel
			modP[i*NCOL_BANK + (k+1)*(NCOL_CHIP+NCOL_BTW_CHIP) - 2] = n<<1;			// pixel to left
			switch (t%3)
				{
				case 2:
					if ((rand())%3)
						modP[i*NCOL_BANK + (k+1)*(NCOL_CHIP+NCOL_BTW_CHIP) - 2]++;	// pixel to left
					else
						modP[i*NCOL_BANK + (k+1)*(NCOL_CHIP+NCOL_BTW_CHIP) - 1]++;	// pseudo-pixel
				case 1:
					if ((rand())%3)
						modP[i*NCOL_BANK + (k+1)*(NCOL_CHIP+NCOL_BTW_CHIP) - 2]++;
					else
						modP[i*NCOL_BANK + (k+1)*(NCOL_CHIP+NCOL_BTW_CHIP) - 1]++;
				case 0:
				default:
					break;
				}
			}
		t = modP[i*NCOL_BANK + (k+1)*(NCOL_CHIP+NCOL_BTW_CHIP) + 0];				// from pixel to right
		if (t >= count_cutoff)
			modP[i*NCOL_BANK + (k+1)*(NCOL_CHIP+NCOL_BTW_CHIP) - 1] = count_cutoff;	// pseudo-pixel
		else
			{
			n = t/3;
			if (modP[i*NCOL_BANK + (k+1)*(NCOL_CHIP+NCOL_BTW_CHIP) - 1]+n < count_cutoff)
				modP[i*NCOL_BANK + (k+1)*(NCOL_CHIP+NCOL_BTW_CHIP) - 1] += n;		// pseudo-pixel
			else
				modP[i*NCOL_BANK + (k+1)*(NCOL_CHIP+NCOL_BTW_CHIP) - 1] = count_cutoff;
			modP[i*NCOL_BANK + (k+1)*(NCOL_CHIP+NCOL_BTW_CHIP) + 0] = n<<1;			// pixel to right
			switch (t%3)
				{
				case 2:
					if ((rand())%3)
						modP[i*NCOL_BANK + (k+1)*(NCOL_CHIP+NCOL_BTW_CHIP) - 0]++;		// pixel to right
					else if(modP[i*NCOL_BANK + (k+1)*(NCOL_CHIP+NCOL_BTW_CHIP) - 1] < count_cutoff)
						modP[i*NCOL_BANK + (k+1)*(NCOL_CHIP+NCOL_BTW_CHIP) - 1]++;		// pseudo-pixel
				case 1:
					if ((rand())%3)
						modP[i*NCOL_BANK + (k+1)*(NCOL_CHIP+NCOL_BTW_CHIP) - 0]++;
					else if(modP[i*NCOL_BANK + (k+1)*(NCOL_CHIP+NCOL_BTW_CHIP) - 1] < count_cutoff)
						modP[i*NCOL_BANK + (k+1)*(NCOL_CHIP+NCOL_BTW_CHIP) - 1]++;
				case 0:
				default:
					break;
				}
			}
		}

// Add DECTRIS logo in each module
#ifdef ENABLE_DECTRIS_LOGO
qq=dectris_logo;
if (qq)
	for(row=0; row<NROW_MOD; row++)
		{
		pp = modP + row*NCOL_BANK;
		for(col=0; col<NCOL_MOD; col++)
			*pp++ += *qq++;
		}
#endif

return;
}

/* - no stochastic placement of left-over counts
// fill in double and quad area pixels from the pixel that received the counts
// fill in for large sensor pixels by dividing counts
// do not lose the remainder
// the 1+ used below removes some of the bias from division by 3

k = (NROW_MOD/2)*NCOL_BANK;	// the line across the center of the sensor
for (i=0; i<NCOL_MOD; i++)
	{
	n = (1 + modP[k + i - NCOL_BANK])/3;		// from pixel above
	modP[k + i - NCOL_BANK] -= n;
	modP[k + i] = n;
	n = (1 + modP[k + i + NCOL_BANK])/3;		// from pixel below
	modP[k + i + NCOL_BANK] -= n;
	modP[k + i] += n;
	}
for (i=0; i<NROW_MOD; i++)		// the lines between chips, including the central line
	for (k=0; k<7; k++)
		{
		n = (1 + modP[i*NCOL_BANK + (k+1)*(NCOL_CHIP+NCOL_BTW_CHIP) - 2])/3;	// from pixel to left
		modP[i*NCOL_BANK + (k+1)*(NCOL_CHIP+NCOL_BTW_CHIP) - 2] -= n;
		modP[i*NCOL_BANK + (k+1)*(NCOL_CHIP+NCOL_BTW_CHIP) - 1] = n;
		n = (1 + modP[i*NCOL_BANK + (k+1)*(NCOL_CHIP+NCOL_BTW_CHIP) + 0])/3;	// from pixel to right
		modP[i*NCOL_BANK + (k+1)*(NCOL_CHIP+NCOL_BTW_CHIP) + 0] -= n;
		modP[i*NCOL_BANK + (k+1)*(NCOL_CHIP+NCOL_BTW_CHIP) -1 ] += n;
		}
#endif
*/


/*
// code to look for bad readouts
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

static int nbadwritten=0;

// code to look for bad readouts
if(nbadwritten<10)
{int c=0, row, col, fd;
char line[80];
// count number of pixels above 8000
for(row=0; row<NROW_MOD; row++)
	for(col=0; col<NCOL_BANK; col++)
		if ( *(modP+col+row*NCOL_BANK) >8000)
			c++;
if (c > 1000)
	{
	sprintf(line, "/home/det/badimg%d.img", nbadwritten);
	fd=open(line, O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR);
	write(fd, modP, NROW_MOD*NCOL_MOD*4);
	close(fd);
	sprintf(line, "/home/det/badimg%d.raw", nbadwritten);
	fd=open(line, O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR);
	write(fd, usp-16*20*97*60/(8*2), 16*20*97*60/8);
	close(fd);
	nbadwritten++;
	}
}

*/

/*
// EFEtest
case EFEtest:
	// test the double & quad area pixel code - put this in tvxpkg.c
	// first, make an "uncorrected raw image"
	ObjPtr=FindObject("im0", Destination, Create, &nerr);
	QueueObject(ObjPtr, Image);
	ResizeObject(ObjPtr, NCOL_MOD, NROW_MOD, 32);
	{int ix, iy;
	int *dp=(int *)ObjPtr->Data;
	for (iy=0; iy<NROW_MOD; iy++)
		for (ix=0;ix<NCOL_MOD;ix++)
			*dp++ = 900;
	iy=NROW_MOD/2;
	dp=(int *)ObjPtr->Data;
	for (ix=0;ix<NCOL_MOD;ix++)
		*(dp+ix+iy*NCOL_MOD)=0;
	for(j=1;j<8;j++)
		for(iy=0; iy<NROW_MOD; iy++)
			*(dp+j*(NCOL_CHIP+1)+iy*NCOL_MOD-1)=0;
	}
	// fill in the double and quad area pixels
	// ---> use this code only with single module build <---
	{
	int i, k, n, t;
	unsigned int * modP=(unsigned int *)ObjPtr->Data;
k = (NROW_MOD/2)*NCOL_BANK;		// the line across the center of the sensor
for (i=0; i<NCOL_MOD; i++)
	{
	t = modP[k + i - NCOL_BANK];				// from pixel above
	if (t >= count_cutoff)
		modP[k + i] = count_cutoff;				// the pseudo-pixel
	else
		{
		n = t/3;
		modP[k + i] = n;						// the pseudo-pixel
		modP[k + i - NCOL_BANK] = n<<1;			// pixel above
		switch (t%3)
			{
			case 2:								// 2 counts left over
				if ((rand())%3)					// 2/3 probability
					modP[k + i - NCOL_BANK]++;
				else							// 1/3 probability
					modP[k + i]++;
			case 1:								// 1 count left over
				if ((rand())%3)					// 2/3 probability
					modP[k + i - NCOL_BANK]++;
				else							// 1/3 probability
					modP[k + i]++;
				break;
			case 0:
			default:
				break;
			}
		}
	t = modP[k + i + NCOL_BANK];			// from pixel below
	if (t >= count_cutoff)
		modP[k + i] = count_cutoff;			// the pseudo-pixel
	else
		{
		n = t/3;
		if (modP[k + i] < count_cutoff)
			modP[k + i] += n;				// the pseudo-pixel
		modP[k + i + NCOL_BANK] = n<<1;		// pixel below
		switch (t%3)
			{
			case 2:
				if ((rand())%3)
					modP[k + i + NCOL_BANK]++;
				else if (modP[k + i] < count_cutoff)
					modP[k + i]++;
			case 1:
				if ((rand())%3)
					modP[k + i + NCOL_BANK]++;
				else if (modP[k + i] < count_cutoff)
					modP[k + i]++;
			case 0:
			default:
				break;
			}
		}
	}
for (i=0; i<NROW_MOD; i++)		// the lines between chips, including the central line
	for (k=0; k<7; k++)			// 7 vertical lines
		{
		t = modP[i*NCOL_BANK + (k+1)*(NCOL_CHIP+NCOL_BTW_CHIP) - 2];				// from pixel to left
		if (t >= count_cutoff)
			modP[i*NCOL_BANK + (k+1)*(NCOL_CHIP+NCOL_BTW_CHIP) - 1] = count_cutoff;	// pseudo-pixel
		else
			{
			n = t/3;
			modP[i*NCOL_BANK + (k+1)*(NCOL_CHIP+NCOL_BTW_CHIP) - 1] = n;			// pseudo-pixel
			modP[i*NCOL_BANK + (k+1)*(NCOL_CHIP+NCOL_BTW_CHIP) - 2] = n<<1;			// pixel to left
			switch (t%3)
				{
				case 2:
					if ((rand())%3)
						modP[i*NCOL_BANK + (k+1)*(NCOL_CHIP+NCOL_BTW_CHIP) - 2]++;	// pixel to left
					else
						modP[i*NCOL_BANK + (k+1)*(NCOL_CHIP+NCOL_BTW_CHIP) - 1]++;	// pseudo-pixel
				case 1:
					if ((rand())%3)
						modP[i*NCOL_BANK + (k+1)*(NCOL_CHIP+NCOL_BTW_CHIP) - 2]++;
					else
						modP[i*NCOL_BANK + (k+1)*(NCOL_CHIP+NCOL_BTW_CHIP) - 1]++;
				case 0:
				default:
					break;
				}
			}
		t = modP[i*NCOL_BANK + (k+1)*(NCOL_CHIP+NCOL_BTW_CHIP) + 0];				// from pixel to right
		if (t >= count_cutoff)
			modP[i*NCOL_BANK + (k+1)*(NCOL_CHIP+NCOL_BTW_CHIP) - 1] = count_cutoff;	// pseudo-pixel
		else
			{
			n = t/3;
			if (modP[i*NCOL_BANK + (k+1)*(NCOL_CHIP+NCOL_BTW_CHIP) - 1] < count_cutoff)
				modP[i*NCOL_BANK + (k+1)*(NCOL_CHIP+NCOL_BTW_CHIP) - 1] += n;		// pseudo-pixel
			modP[i*NCOL_BANK + (k+1)*(NCOL_CHIP+NCOL_BTW_CHIP) + 0] = n<<1;			// pixel to right
			switch (t%3)
				{
				case 2:
					if ((rand())%3)
						modP[i*NCOL_BANK + (k+1)*(NCOL_CHIP+NCOL_BTW_CHIP) - 0]++;		// pixel to right
					else if(modP[i*NCOL_BANK + (k+1)*(NCOL_CHIP+NCOL_BTW_CHIP) - 1] < count_cutoff)
						modP[i*NCOL_BANK + (k+1)*(NCOL_CHIP+NCOL_BTW_CHIP) - 1]++;		// pseudo-pixel
				case 1:
					if ((rand())%3)
						modP[i*NCOL_BANK + (k+1)*(NCOL_CHIP+NCOL_BTW_CHIP) - 0]++;
					else if(modP[i*NCOL_BANK + (k+1)*(NCOL_CHIP+NCOL_BTW_CHIP) - 1] < count_cutoff)
						modP[i*NCOL_BANK + (k+1)*(NCOL_CHIP+NCOL_BTW_CHIP) - 1]++;
				case 0:
				default:
					break;
				}
			}
		}

	break;

*/

/*
// some test code - don't forget initialize_theImage()
// to use - place this before if(!mode) return;, then do rbd followed by
// expose 1 & comapre pixel values
count_cutoff=100000;
i=0;
modP[58+i*NCOL_BANK] = 5800;
modP[59+i*NCOL_BANK] = 5900;
modP[61+i*NCOL_BANK] = 6100;
modP[62+i*NCOL_BANK] = 6200;
i=1;
modP[59+i*NCOL_BANK] = count_cutoff;
i=2;
modP[61+i*NCOL_BANK] = count_cutoff;
i=3;
modP[59+i*NCOL_BANK] = count_cutoff;
modP[61+i*NCOL_BANK] = count_cutoff;
i=4;
modP[59+i*NCOL_BANK] = count_cutoff;
modP[61+i*NCOL_BANK] = 6100;
i=5;
modP[59+i*NCOL_BANK] = 5900;
modP[61+i*NCOL_BANK] = count_cutoff;

k = (NROW_MOD/2)*NCOL_BANK;		// the line across the center of the sensor
i=0;
modP[k + i - 2*NCOL_BANK]=9500;
modP[k + i - 1*NCOL_BANK]=9600;
modP[k + i + 1*NCOL_BANK]=9800;
modP[k + i + 2*NCOL_BANK]=9900;
i=1;
modP[k + i - 1*NCOL_BANK]=count_cutoff;
i=2;
modP[k + i + 1*NCOL_BANK]=count_cutoff;
i=3;
modP[k + i - 1*NCOL_BANK]=count_cutoff;
modP[k + i + 1*NCOL_BANK]=count_cutoff;
i=4;
modP[k + i - 1*NCOL_BANK]=9600;
modP[k + i + 1*NCOL_BANK]=count_cutoff;
i=5;
modP[k + i - 1*NCOL_BANK]=count_cutoff;
modP[k + i + 1*NCOL_BANK]=9800;

modP[k + 59 - 1*NCOL_BANK]=9500;
modP[k + 61 - 1*NCOL_BANK]=9600;
modP[k + 59 + 1*NCOL_BANK]=9800;
modP[k + 61 + 1*NCOL_BANK]=count_cutoff;

modP[k + 120 - 1*NCOL_BANK]=count_cutoff;
modP[k + 122 - 1*NCOL_BANK]=9600;
modP[k + 120 + 1*NCOL_BANK]=9800;
modP[k + 122 + 1*NCOL_BANK]=9900;

*/


/******************************************************************************\
**                                                                            **
**     in place truncation of an image from 32 bits to 16 bits                **
**                                                                            **
\******************************************************************************/

void truncate_image(unsigned int *img32, int len)
{
int i;
unsigned short *img16 = (unsigned short *)img32;

if (sizeof(TYPEOF_PIX) != 4)	// only for 32 bit images
	return;

for(i=0; i<len; i++)
	*img16++ = (unsigned short)(*img32++);

return;
}



#ifdef SLSP2DET_CAMERA
// system globals
struct det_pixel *px;

// globals for this module
static struct det_pixel pix_coords;
/*****************************************************************************\
***                                                                         ***
***      pix_add - return module and chip coordinates of a pixel            ***
***                                                                         ***
***     an entry of -1 means that information is sought                     ***
***     an entry >=0 means that information is provided                     ***
***                                                                         ***
\*****************************************************************************/

struct det_pixel *pix_add(int pix, int ix, int iy, int bn, int bx, int by,
		int mn, int mx, int my, int cn, int cx, int cy)
{
/*
pix_coords.pix = pix_coords.ix = pix_coords.iy = pix_coords.bn = pix_coords.bx
			= pix_coords.by = pix_coords.mn = pix_coords.mx = pix_coords.my
			= pix_coords.cn = pix_coords.cx = pix_coords.cy = -1;
*/

memset(&pix_coords, 0xff, sizeof(pix_coords));		// -1 to each entry

if (bn==0)
	return &pix_coords;			// invalid data

if (bn > 0)
	bn--;						// change 1,2,3 system to 0,1,2

if (mn==0)
	return &pix_coords;			// invalid data

if (mn > 0)
	mn--;						// change 1,2,3 system to 0,1,2

if (pix >= 0)					// given the pixel number
	{
	if (pix >= NCOL_DET*NROW_DET)
		return &pix_coords;		// invalid data
	ix = pix % NCOL_DET;
	iy = pix / NCOL_DET;		// fall through to next if
	}
if (ix >= 0 && iy >= 0)			// given image coordinates
	{
	if (ix>=NCOL_DET || iy>=NROW_DET)
		return &pix_coords;				// invalid data
	pix_coords.ix = ix;
	pix_coords.iy = iy;
	by = iy % (NROW_BANK+NROW_BTW_BANK);
	if (by >= NROW_BANK)
		goto finish;				// not in a bank
	bn = iy / (NROW_BANK+NROW_BTW_BANK);
	bx = ix;
	}
if (bn >= 0 && bx >= 0 && by >= 0)	// given bank & coordinates
	{
	if (bn>=NBANK || bx>=NCOL_BANK || by>=NROW_BANK)
		return &pix_coords;			// invalid data
	pix_coords.bn = bn + 1;			// banks numbered 1,2...
	pix_coords.bx = bx;
	pix_coords.by = by;
	mx = bx % (NCOL_MOD+NCOL_BTW_MOD);
	if (mx >= NCOL_MOD)
		goto finish_det;				// not in a module
	mn = bx / (NCOL_MOD+NCOL_BTW_MOD);
	my = by;
	}
if (bn >= 0 && mn >= 0 && mx >= 0 && my >= 0)	// given bank, module & coordinates
	{
	if (bn>=NBANK || mn>=NMOD_BANK || mx>=NCOL_MOD || my>=NROW_MOD)
		return &pix_coords;			// invalid data
	pix_coords.mn = mn+1;				// modules numbered 1,2...
	pix_coords.mx = mx;
	pix_coords.my = my;
	if (my < NROW_MOD/2)
		{
		cn = mx/(NCOL_CHIP+1);
		cx = (NCOL_CHIP+1)*(cn+1) - mx - 2;
		if (cx < 0)
			goto finish_bank;		// not in a chip
		pix_coords.cn = cn;
		pix_coords.cx = cx;
		pix_coords.cy = my;
		}
	else if (my == NROW_MOD/2)
		goto finish_bank;		// not in a chip
	else
		{
		cn = NCHIP/2 + mx/(NCOL_CHIP+1);
		cx = mx % (NCOL_CHIP+2);
		if (cx >= NCOL_CHIP)
			goto finish_bank;		// not in a chip
		pix_coords.cn = cn;
		pix_coords.cx = cx;
		pix_coords.cy = NROW_MOD - my - 1;
		}
	goto finish_bank;
	}
else if (bn >= 0 && mn >= 0 && cn >= 0 && cx >=0 && cy >= 0)
	{
	if (bn>=NBANK || mn>=NMOD_BANK || cn>=NCHIP || cx>=NCOL_CHIP || cy>=NROW_CHIP)
		return &pix_coords;			// invalid data
	pix_coords.cn = cn;
	pix_coords.cx = cx;
	pix_coords.cy = cy;
	pix_coords.mn = mn+1;		// modules numbered 1,2...
	if (cn < NCHIP/2)
		{
		pix_coords.mx = (NCOL_CHIP+1)*(cn+1) - cx - 2;
		pix_coords.my = cy;
		}
	else
		{
		pix_coords.mx = (NCOL_CHIP+2)*(cn-NCHIP/2) + cx;
		pix_coords.my = NROW_MOD - cy - 1;
		}
	goto finish_bank;
	}
else
	return &pix_coords;			// invalid data

finish_bank:
pix_coords.bx = pix_coords.mx + mn*(NCOL_MOD+NCOL_BTW_MOD) ;
pix_coords.by = pix_coords.my;

finish_det:
pix_coords.bn = bn+1;		// banks numbered 1,2...
pix_coords.ix = pix_coords.bx;
pix_coords.iy = pix_coords.by + bn*(NROW_BANK+NROW_BTW_BANK);;

finish:
pix_coords.pix = pix_coords.ix + NCOL_DET*pix_coords.iy;

return &pix_coords;
}
#endif


/*****************************************************************************\
***                                                                         ***
***      log_gpib_readings -- append readings to 'treport.txt'              ***
***                                                                         ***
\*****************************************************************************/

void log_gpib_readings(FILE *ofp)
{
#ifdef USE_GPIB
double voltages[7];
char line[300];

if (rpcopen("hpib,9"))
	{
	printf("*** Could not open RPC connection for GPIB readout\n");
	fprintf(ofp, "*** Could not open RPC connection for GPIB readout\n");
	fprintf(ofp, "\n-----------------------------------------------------------\n");
	return;
	}
/*gpibsend("MEAS:VOLT:DC? (@101,102,103,104,105,106,107)");*/
gpibsend("READ?");
gpibread(line);
rpcclose();
fprintf(ofp, "----------- Voltages & currents reported from GPIB ----------\n\n");
sscanf(line, "%lf,%lf,%lf,%lf,%lf,%lf,%lf", voltages+0, voltages+1,
		voltages+2,voltages+3,voltages+4,voltages+5,voltages+6);
fprintf(ofp, "Ignd   %.4g mA\n", 1000.0*voltages[0]);
fprintf(ofp, "Ia-    %.4g mA\n", 1000.0*voltages[1]);
fprintf(ofp, "Ic-    %.4g mA\n", 1000.0*voltages[2]);
fprintf(ofp, "Id-    %.4g mA\n", 1000.0*voltages[3]);
fprintf(ofp, "Vgnd   %.4g V\n", voltages[5]);
fprintf(ofp, "Vtrm   %.4g V\n", voltages[4]);
fprintf(ofp, "Vrf    %.4g V\n", voltages[6]);
fprintf(ofp, "\n-----------------------------------------------------------\n");
#endif

return;
}

/*
gpib_offset
{
if (rpcopen())
	{
	printf("*** Could not open RPC connection for GPIB readout\n");
	return;
	}
gpibsend("MEAS:VOLT:DC? (@101,102,103,104,105,106,107)")
//gpibsend("SENS:VOLT:DC:RANG 1, (@101,102,103,104)");
//gpibsend("SENS:VOLT:DC:RANG 10,(@105)");
//gpibsend("SENS:VOLT:DC:RES 0.001,(@101,102,103,104,105)");
//gpibsend("CALC:SCALE:OFFSET:NULL (@101,102,103,104)");
//gpibsend("CALC:SCALE:STATE ON, (@101,102,103,104)");
rpcclose();
return;
}
*/

#ifdef USE_GPIB

void gpibsend_vo(char *line)
{
if (rpcopen("hpib,9"))
	{
	printf("*** Could not open RPC connection for GPIB dvm\n");
	return;
	}
gpibsend(line);
rpcclose();
return;
}

void gpibsend_os(char *line)
{
if (rpcopen("hpib,5"))
	{
	printf("*** Could not open RPC connection for GPIB oscilloscope\n");
	return;
	}
gpibsend(line);
rpcclose();
return;
}
#define MAX_N_VOLTAGES 22

void gpibread_vo(void)
{
int i, count;
char line[20*MAX_N_VOLTAGES], *p;
double voltages[MAX_N_VOLTAGES];
if (rpcopen("hpib,9"))
	{
	printf("*** Could not open RPC connection for GPIB dvm\n");
	return;
	}
gpibsend("READ?");
gpibread(line);
rpcclose();
for (i=0, p=line, count=0; i<MAX_N_VOLTAGES; i++)
	{
	if (sscanf(p, "%lf", voltages+i) )
		count++;
	if ( (p=strchr(p, ',')) == NULL)
		break;		// we're done
	p++;
	}
for (i=0; i<count; i++)
	printf("channel %d = %.4g\n", 101+i, voltages[i]);
return;
}

void gpibread_os(char *cmd)
{
char line[200], *q, *qq;
double ampl, scale=1000.0;
int x, y;

if (rpcopen("hpib,5"))
	{
	printf("*** Could not open RPC connection for GPIB oscilloscope\n");
	return;
	}
/*gpibsend("TA:PAVA? AMPL");*/
if ( (q=strchr(cmd, '!')) )		// optional extension = (x,y) address, scale
	{
	*q='\0';					// truncate cmd for gpibsend()
	q+=3;						// skip over possible comma
	while ( (qq=strchr(q, ',')) )
		*qq=' ';				// change optional commas to spaces
	sscanf(q, "%d%d%lf", &x, &y, &scale);	// get address & scale
//	printf("%s, x= %d, y= %d, scale = %lf\n",q,x,y,scale);
	}
gpibsend(cmd);
gpibread(line);
rpcclose();
printf("response: %s\n",line);
qq=line;
if (q)
	{
	while ( (qq=strchr(qq, ',')) )
		{
		qq++;				// look for comma - number
		if (isdigit(*qq) || *qq=='-')
			break;
		}
	ampl=0.0;
	if (qq)
		sscanf(qq, "%lf", &ampl);
	else
		printf("gpibread_os - data not found\n");
//	theImage[x + y*IMAGE_NCOL]=(int)rint(fabs(1000.0*ampl));
	theImage[x + y*IMAGE_NCOL]=(int)rint(fabs(scale*ampl));
	}
/*printf("Amplitude = %.1g\n", ampl*1000);*/
return;
}

#endif



/*****************************************************************************\
***                                                                         ***
***         moduleFilename - same as in tvx                                 ***
***                                                                         ***
\*****************************************************************************/

// Make up module filenames according to enabled options. E.g.:
// sls42:      /path/base_name.dat
// sls8x2:     /path/base_name.dat    
// slsdet:     /path/base_name_b3_m1.dat
// 13 Oct 07 - always use fully expanded form
void moduleFilename(char *line, char *path, char *base, int bank, int module)
{
char tmp[50], *p, *q;

if (base[0]=='/' || base[0]=='.')	// skip leading './' etc.
	base++;
if (base[0]=='/' || base[0]=='.')
	base++;
strcpy(tmp, base);
if ( (q = strchr(tmp, '.')) )		// remove extension at first '.'
	*q = '\0';
if ( (p = strstr(tmp, "_c")) && isdigit(*(p+2)) )	// remove old suffixes
	*p = '\0';
if ( (p = strstr(tmp, "_m")) && isdigit(*(p+2)) )
	*p = '\0';
if ( (p = strstr(tmp, "_b")) && isdigit(*(p+2)) )
	*p = '\0';

if (path)				// 'line' may already contain path
	strcpy(line, path);
strcat(line, tmp);		// base file name

if (NMOD_BANK*NBANK > 1)
	sprintf(line+strlen(line), "_b%.2d_m%.2d", bank, module);
else
	sprintf(line+strlen(line), "_b%.2d_m%.2d", 1, 1);

if (q)					// if an extension was given
	{
	*q = '.';
	strcat(line, q);
	}

return;
}



/*****************************************************************************\
***                                                                         ***
***         load_bad_pixel_list()                                           ***
***                                                                         ***
\*****************************************************************************/
// load a bad pixel list from a mask image
// these pixels are set to BAD_PIX_FLAG when the image is read out
// Flagging bad pixels requires ~0.8 ms / 1000 pixels (3.2 GHz cpu)
// data are stored in bad_pixel_list[] - a list of offsets in the image

int load_bad_pixel_list(char *fn)
{
int tbuff[MAX_BAD_PIXELS+1], x, y, *t, n, xf, xl, yf, yl, n_bytes;
char *img, tfn[LENFN];
TYPEOF_PIX *p, *p0;
struct object_descriptor Obj={0};

if (*fn == '0' || !strncasecmp(fn, "off", 3))
	{
	if (bad_pix_list)
		{
		free (bad_pix_list);
		bad_pix_list=NULL;
		}
	bad_pix_file_name[0]='\0';
	n_bad_pix = 0;
	printf(" Excluded pixel flags turned off\n");
	return 0;
	}

if (*fn != '/')
	{
	printf("Full pathname required\n");
	return -1;
	}

// if fn is a link, get the underlying (true) filename
n = readlink(fn, tfn, LENFN);
if (n<=0)		// not a link
	strcpy(tfn, fn);
else
	tfn[n] = '\0';

if (!strcmp(tfn, bad_pix_file_name))
	return 0;			// nothing to do

Obj.Kind = Image;
Obj.Type = Memory;
Obj.Role = Source;
Obj.Path = fn;

if ( (Obj.Handle = open(Obj.Path, O_RDONLY)) <= 2)
	{
	printf("Could not open file for reading:\n  %s\n", Obj.Path);
	return -1;
	}

n = get_im_head(&Obj);
if (n < 0)
	{
	close(Obj.Handle);
	printf("*** Unrecognized image format\n");
	return -1;
	}

// in the case of a slave computer, height or width will not match
#ifdef SLAVE_COMPUTER
 #ifdef WIDE_FAST
  if (Obj.High!=IMAGE_NROW || sizeof(TYPEOF_PIX)!=Obj.Bpp/8)
 #else
  if (Obj.Wide!=IMAGE_NCOL || sizeof(TYPEOF_PIX)!=Obj.Bpp/8)
 #endif
#else
 if (Obj.Wide!=IMAGE_NCOL || Obj.High!=IMAGE_NROW || sizeof(TYPEOF_PIX)!=Obj.Bpp/8)
#endif
	{
	close(Obj.Handle);
	printf("File is of the wrong size or format\n");
	return -1;
	}

n_bytes = Obj.High*Obj.Wide*Obj.Bpp/8;
if ( !(img=malloc(n_bytes)) )
	{
	close(Obj.Handle);
	printf("*** Error - load_bad_pixel_list() could not allocate memory\n");
	return -1;
	}

n = read_diskimage(&Obj, 0, n_bytes, img);
close(Obj.Handle);

if (n < n_bytes)
	{
	printf("*** Error reading file - only %d bytes read\n", n);
	return -1;
	}

// select the part of the full-size image that applies to this subimage
xf = yf = 0;
xl = Obj.Wide;
yl = Obj.High;
#ifdef MASTER_COMPUTER		// only bank 1, mod 1
 xl = NCOL_MOD;
 yl = NROW_MOD;
#endif

memset(tbuff, 0, sizeof(tbuff));
t = tbuff;
p0 = (TYPEOF_PIX *)img;				// bad pixel map image

#ifdef SLAVE_COMPUTER
 #ifdef WIDE_FAST
	xf = SLAVE_COMPUTER*(NCOL_MOD + NCOL_BTW_MOD);
	for(y=0; y<yl; y++)							// wide slave, 2 dcb's
		{
		p = (TYPEOF_PIX *)img + y*Obj.Wide + xf;
		for(x=xf; x<xl; x++, p++)
			{
			if(!*p || p-p0>=n_bytes)
				continue;
			if (t>=tbuff+MAX_BAD_PIXELS)
				{
				printf("Too many bad pixels - stopped at %d\n", MAX_BAD_PIXELS);
				goto j0;
				}
			px = pix_add(-1, x-xf, y, -1, -1, -1, -1, -1, -1, -1, -1, -1);
			if (px->mn < 0)			// ignore gaps
				continue;
			*t++ = px->pix;
			}
		}
 #else							
	yf = SLAVE_COMPUTER*(NROW_BANK + NROW_BTW_BANK);
	for(y=yf; y<yl; y++)							// tall slave, 2 dcb's
		{
		p = (TYPEOF_PIX *)img + y*Obj.Wide;
		for(x=0; x<xl; x++, p++)
			{
			if(!*p || p-p0>=n_bytes)
				continue;
			if (t>=tbuff+MAX_BAD_PIXELS)
				{
				printf("Too many bad pixels - stopped at %d\n", MAX_BAD_PIXELS);
				goto j0;
				}
			px = pix_add(-1, x, y-yf, -1, -1, -1, -1, -1, -1, -1, -1, -1);
			if (px->mn < 0)			// ignore gaps
				continue;
			*t++ = px->pix;
			}
		}
 #endif
#else
	for(y=0; y<yl; y++)							// normal case
		{
		p = (TYPEOF_PIX *)img + y*Obj.Wide;
		for(x=0; x<xl; x++, p++)
			{
			if(!*p || p-p0>=IMAGE_SIZE)
				continue;
			if (t>=tbuff+MAX_BAD_PIXELS)
				{
				printf("Too many bad pixels - stopped at %d\n", MAX_BAD_PIXELS);
				goto j0;
				}
			px = pix_add(-1, x, y, -1, -1, -1, -1, -1, -1, -1, -1, -1);
			if (px->mn < 0)			// ignore gaps
				continue;
			*t++ = px->pix;
			}
		}
#endif

if (t==tbuff)
	{
	printf("Zero bad pixels found in map\n");
	free(img);
	return 0;
	}

j0:
free(img);
if (bad_pix_list)
	free(bad_pix_list);

// ensure there is a zero at the end of the list
if ( (bad_pix_list=(int *)calloc((1+t-tbuff), sizeof(int))) == NULL)
	{
	printf("load_bad_pixel_list() cannot allocate memory\n");
	bad_pix_file_name[0] = '\0';
	return -1;
	}
memcpy(bad_pix_list, tbuff, (1+t-tbuff)*sizeof(int));
n_bad_pix = (int)(t-tbuff);
#ifdef MASTER_COMPUTER
  printf(" Master computer: ");
#endif
#ifdef SLAVE_COMPUTER
  printf(" Slave computer: ");
#endif
printf("%d excluded pixels were read\n", n_bad_pix);
strcpy(bad_pix_file_name, tfn);

return 0;
}



/*****************************************************************************\
***                                                                         ***
***         load_flat_field_correction_file()                               ***
***                                                                         ***
\*****************************************************************************/
// Flat-field correction requires ~2.6 ms per module (3.2 GHz cpu)
// using the floating point method.

int load_flat_field_correction_file(char *fn)
{
int n, offset, y;
unsigned int n_bytes;
struct object_descriptor Obj={0};

if (*fn == '0' || !strncasecmp(fn, "off", 3))
	{
	if (flat_field_data)
		{
		free (flat_field_data);
		flat_field_data=NULL;
		}
	flat_field_file_name[0] = '\0';
	printf(" Flat-field correction turned off\n");
	return 0;
	}

if (*fn != '/')
	{
	printf("Full pathname required\n");
	return -1;
	}

if (!strcmp(fn, flat_field_file_name))
	return 0;			// nothing to do

Obj.Kind = Image;
Obj.Type = Memory;
Obj.Role = Source;
Obj.Path = fn;

if ( (Obj.Handle = open(Obj.Path, O_RDONLY)) <= 2)
	{
	printf("Could not open file for reading:\n  %s\n", Obj.Path);
	return -1;
	}

n = get_im_head(&Obj);
if (n < 0)
	{
	close(Obj.Handle);
	printf("*** Unrecognized image format\n");
	return -1;
	}

// in the case of a slave computer, height or width will not match
#ifdef SLAVE_COMPUTER
 #ifdef WIDE_FAST
  if (Obj.High!=IMAGE_NROW || sizeof(TYPEOF_PIX)!=Obj.Bpp/8)
 #else
  if (Obj.Wide!=IMAGE_NCOL || sizeof(TYPEOF_PIX)!=Obj.Bpp/8)
 #endif
#else
 if (Obj.Wide!=IMAGE_NCOL || Obj.High!=IMAGE_NROW || sizeof(TYPEOF_PIX)!=Obj.Bpp/8)
#endif
	{
	close(Obj.Handle);
	printf("File is of the wrong size or format\n");
	return -1;
	}

// we use the fact that sizeof(float)==sizeof(unsigned int)
if ( !(flat_field_data=(void *)malloc(IMAGE_SIZE*sizeof(float))) )
	{
	close(Obj.Handle);
	printf("load_flat_field_correction_file() could not allocate memory\n");
	return -1;
	}

// copy the part of the full-size image that applies to this subimage
for (y=0; y<IMAGE_NROW; y++)
	{
#ifdef SLAVE_COMPUTER
 #ifdef WIDE_FAST
	offset = (SLAVE_COMPUTER*(NCOL_MOD + NCOL_BTW_MOD) + y*Obj.Wide)*sizeof(TYPEOF_PIX);
	n_bytes = (2*NCOL_MOD + NCOL_BTW_MOD)*sizeof(TYPEOF_PIX);
 #else
	offset = (SLAVE_COMPUTER*NCOL_MOD*(NROW_MOD + NROW_BTW_BANK))*sizeof(TYPEOF_PIX);
	n_bytes = NCOL_MOD*sizeof(TYPEOF_PIX);
 #endif
#else							// normal case & MASTER_COMPUTER
	offset = y*Obj.Wide*Obj.Bpp/8;
	n_bytes = IMAGE_NCOL*sizeof(TYPEOF_PIX);
#endif
	n = read_diskimage(&Obj, offset, n_bytes, (char *)((float *)flat_field_data + y*IMAGE_NCOL));
	if (n < n_bytes)
		{
		close(Obj.Handle);
		printf("*** Error reading file - only %d bytes read\n", n);
		free(flat_field_data);
		flat_field_data=NULL;
		flat_field_file_name[0] = '\0';
		return -1;
		}
	}
close(Obj.Handle);

#ifdef INTEGER_FLAT_FIELD_METHOD		// convert to integers
{
unsigned int *uif = (unsigned int *)flat_field_data;
float *ff = (float *)flat_field_data;
int x;

for (y=0; y<IMAGE_NROW; y++)
	for (x=0; x<IMAGE_NCOL; x++, uif++, ff++)
		*uif = (unsigned int)rintf((*ff)*(float)(1<<INTEGER_FF_SHIFT));
}
#endif

strcpy(flat_field_file_name, fn);

return 0;
}



/*****************************************************************************\
***                                                                         ***
***          setup_file_series()                                            ***
***          next_file_in_series()                                          ***
***                                                                         ***
\*****************************************************************************/
// helper functions for ScanDAC command

/*
** setup_file_series() - initialize a series of consecutively number files
** using the full pathname as a template.  Checks for, and requires the
** existence of an index file (like tvx) in the target directory.
** Returns an error code: 0 is no error, -1 indicates an error
*/
static char ext[10];
static char basename[LENFN];
static char pathname[LENFN];

int setup_file_series(char *ptr)
{
char *p, *q, line[LENFN];
struct stat buf;

if (strlen(ptr) > LENFN-10)
	{
	printf("Pathname is too long:\n  %s\n", ptr);
	return -1;
	}
p = ptr-6+strlen(ptr);
if ( (q=strchr(p, '.')) )
	{
	strcpy(ext, q);
	*q = '\0';
	}
else
	strcpy(ext, ".tif");
if (*ptr=='/')
	strcpy(basename, ptr);
else
	{
	strcpy(basename, cam_image_path);
	strcat(basename, ptr);
	}
strcpy(line, basename);
strcat(line, ".idx");
if (stat(line, &buf))
	{
	printf("Cannot find index file:\n  %s\n", line);
	return -1;
	}
return 0;
}


/*
** next_file_in_series() - generate the next filename in the series
** set up by setup_file_series().  Increment the index file in the 
** target directory.  Format is fixed at 5 digits after the basename.
*/
char *next_file_in_series(void)
{
int n=0;
FILE *fp;
char line[LENFN], buf[10];

strcpy(line, basename);
strcat(line, ".idx");
if ( !(fp=fopen(line, "r")) )
	{
	printf("Cannot open index file for reading:\n  %s\n", line);
	return NULL;
	}
fscanf(fp, "%d", &n);
fclose(fp);
if ( !(fp=fopen(line, "w")) )
	{
	printf("Cannot open index file for writing:\n  %s\n", line);
	return NULL;
	}
n++;
sprintf(buf, "%d\n", n);
fputs(buf, fp);
fclose(fp);

strcpy(pathname, basename);
sprintf(pathname+strlen(pathname), "%.5d", n);
strcat(pathname, ext);
return pathname;
}
