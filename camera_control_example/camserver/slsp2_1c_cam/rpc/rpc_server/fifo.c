// fifo.c - communicate with the str7090 vme fifo
// EFE June 00++

//#define SLS06
//#define SLS07
// SLS08 readout ChB Dez03
//#define SLS08

// PILATUS2 chip, using vme fifo.  The chip is read with 2 patterns,
// first the lower half, then the upper half
#define SLS10

//#define USE_TOP_16_BITS 	// test dynamic range by deleting the lowest 4 bits
#define WRITE_32_BITS

#define SLS8x2_CAMERA		// enable no-BCB version of fifo_write_n_images

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
//#include <sys/types.h>
//#include <sys/stat.h>
#include <timers.h>
#include <fcntl.h>
#include <math.h>
#include "ioc.h"

#define fifo_base 0x5000000			// fifo base address
#define fifo_data_reg 0x0
#define fifo_event_counter 0x4
#define fifo_status_reg 0x8
#define fifo_control_reg 0x8
#define fifo_test_reg 0xc
#define fifo_status_mask 0xff		// isolate meaningful bits in status
#define fifo_event_counter_mask 0x3fff	// isolate meaningful bits in counter
#define fifo_event_status_mask 0xc000	// isolate overflow & timeout

#define OK 0
#define FAIL -1
#define True 1
#define False 0
#define PRINT False
#define PIXELS_PER_READ 500			// max pixels per gulp

#ifdef SLS06
#define COLS 44
#define ROWS 78
#define NROWS_MOD 157				// includes double pixels in sensor
#define NCOLS_MOD 366

#define NBITS 15					// no. bits in SR counter
#define NSTATES 32768	// 2**NBITS
#define fbBIT 13		// feed-back bit to use in additon to last bit
#endif

#ifdef SLS07
#define COLS 1
#define ROWS 6
#define NROWS_MOD 6
#define NCOLS_MOD 2

#define NBITS 18				// no. bits in SR counter
#define NSTATES 262144	// 2**NBITS
#define fbBIT 10		// feed-back bit to use in additon to last bit
#endif

#ifdef SLS08
#define COLS 60
#define ROWS 50
#define NROWS_MOD 60				
#define NCOLS_MOD 50
#define NBITS 20				// no. bits in SR counter
#define NSTATES 1048576	// 2**NBITS
#define fbBIT 2		// feed-back bit to use in additon to last bit

#define IMAGE_SIZE NROWS_MOD*NCOLS_MOD
#endif


#ifdef SLS10
#define COLS 60
#define ROWS 97
#define NCOLS_MOD 60
#define NROWS_MOD 97				
#define NBITS 20			// no. bits in SR counter
#define NSTATES 1048576		// 2**NBITS
#define fbBIT 2				// feed-back bit to use in additon to last bit

#define IMAGE_SIZE NROWS_MOD*NCOLS_MOD
#endif


#define BITMASK (-1+(1<<NBITS))
#define MOD_IMAGE_SIZE NROWS_MOD*NCOLS_MOD				//  57,462
#define LENGTHFN 160

int sysBusToLocalAdrs(int, int, unsigned int *);	// from vxWorks
int vxMemProbe(char *, int, int, char *);			// from vxWorks
static int fifo_read_stat(void);
static void xor_decode_image(int, int);
static int xor(int, int);
static int xor_tab_initialize(int);
static void xor_add_count(int []);
static void wait_for_pattern(int, FILE *);
static void delay_1us(void);
static void load_pattern_addresses(void);


// global variables
static unsigned int cpu_base_fifo;		// fifo address in cpu mem address space
static int fifoIsInitialized = False;
static unsigned int selector[17];
static int nchips;
static int pixels_remaining = 0;
static int bit_count = 0;
static int pixel_count;
static int event_count;
static int pixels_done;
static int write_count=0;
static int state[NBITS];
static int fifo_data_true_level = 0;	// accounts for data inversion
static int last_mode;
static int d_dly=100;					// software clock basis
static int rdinput_time;				// time for read_input(), ns
static int readchip_pattern0=0x7000;		// [baser0]
static int readchip_pattern1=0x7000;		// [baser1]
static int expose_pattern=0x5400;
static int delayus_pattern=0x5000;
static int enable_pattern=0x5000;
static int disable_pattern=0x4c00;


// arrays
static unsigned int pixels[PIXELS_PER_READ];

#ifdef SLS06
static unsigned short theImage[MOD_IMAGE_SIZE];
static unsigned short xor_table[NSTATES];
#endif

#ifdef SLS07
static unsigned short *pix16b;
static unsigned int theImage[MOD_IMAGE_SIZE];
static unsigned int xor_table[NSTATES];
#endif
//static unsigned short modImage[MOD_IMAGE_SIZE];
static char previous_image[120] = "";

#ifdef SLS08
static unsigned short *pix16b;
static unsigned int theImage[IMAGE_SIZE];
static unsigned int xor_table[NSTATES];
#endif

#ifdef SLS10
static unsigned int theImage[IMAGE_SIZE];
static unsigned int xor_table[NSTATES];
static unsigned int tmpimg[IMAGE_SIZE];
static unsigned char *a, t;
#endif



// initialize the fifo - must be performed before any other operation
int fifoini(void)
{
int i, n, status;
long diff;
char line[50];
FILE *ofp;
#ifdef SLS06
unsigned short *xortmp;
#endif
#ifdef SLS07
#error - SLS07 no longer defined
#endif
#ifdef SLS08
unsigned int *xortmp;
#endif
#ifdef SLS10
unsigned int *xortmp;
#endif
struct timespec ts, tn;

if (fifoIsInitialized)		// don't need to redo initialization
	{
#ifdef SLS06
	if ( (xortmp=(unsigned short *)malloc(NSTATES*sizeof(unsigned short)))==NULL )
		{
		printf("fifo.c could not allocate memeory\n");
		return FAIL;
		}
#endif
#ifdef SLS08
	if ( (xortmp=(unsigned int *)malloc(NSTATES*sizeof(unsigned int)))==NULL )
		{
		printf("fifo.c could not allocate memeory\n");
		return FAIL;
		}
#endif
#ifdef SLS10
	if ( (xortmp=(unsigned int *)malloc(NSTATES*sizeof(unsigned int)))==NULL )
		{
		printf("fifo.c could not allocate memeory\n");
		return FAIL;
		}
#endif
	for (i=0; i<NSTATES; i++)
		xortmp[i] = xor_table[i];
	xor_tab_initialize(-1);			// renew the xor table
	for (i=0, n=0; i<NSTATES; i++)
		if (xor_table[i] != xortmp[i])
			{			// the first & last entires depend on mode or inversion
			if (i==0 && (xortmp[i]==0 || xortmp[i]==0xffff) )
				continue;
			if (i==NSTATES-1 && (xortmp[i]==0 || xortmp[i]==0xffff) )
				continue;
			n++;
			}
	if (n)
		{
		printf("xor table corrupt in %d entries - see xordiagnose.txt\n", n);
		strcpy(line, "/xordiagnose.txt");
		if ( (ofp = fopen (line, "w")) == NULL)
			printf("could not open %s for writing\n", line);
		else
			{
			for (i=1; i<NSTATES; i++)
				if (xor_table[i] != xortmp[i])
					fprintf(ofp, "index %5d, correct value %5d. corrupt value %5d\n",
							i, xor_table[i],xortmp[i]); 
			fclose(ofp);
			}
		}
	printf("xor table renewed; mode is calibrate pulses\n");
	free (xortmp);
	last_mode = 0;		// default mode is calibrate pulses
	fifo_write_control(0x1b4);	// zero out fifo & set standard state
//	load_pattern_addresses();

	return OK;
	}

cpu_base_fifo = 0;
status = sysBusToLocalAdrs(0xd, fifo_base, &cpu_base_fifo);

if(PRINT) printf("FIFO address mapping status returned %d\n", status);
if(PRINT) printf("Pointing to %#x\n", cpu_base_fifo);

if(status)
     {
     printf(" FIFO cannot be initilized\n");
     return FAIL;
	 }

if(vxMemProbe( (char *)(cpu_base_fifo + fifo_status_reg), VX_READ, 4,
			(char *)&status) == ERROR)
	{
	printf("\n---***---VME bus error initializing FIFO\n");
	return FAIL;
	}


#ifdef SLS06
printf("initialize fifo for SLS06: NBITS = %d, NSTATES = %d\n", NBITS, NSTATES);
#endif
#ifdef SLS08
printf("initialize fifo for SLS08: NBITS = %d, NSTATES = %d\n", NBITS, NSTATES);
#endif
#ifdef SLS10
printf("initialize fifo for SLS10: NBITS = %d, NSTATES = %d\n", NBITS, NSTATES);
#endif



xor_tab_initialize(-1);
last_mode = 0;		// default mode is calibrate pulses

if(PRINT) printf(" FIFO is initialized\n");
fifoIsInitialized = True;
fifo_write_control(0x1b4);	// zero out fifo & set standard state

// calibrate the software clock
// Experiment showed that while the real-time clock gives time with a
// "precision" of 1 ns, the time between ticks is 13 ms (80 Hz).  If you
// want finer granularity, you need to implement your own clock.

clock_gettime(CLOCK_REALTIME, &ts);		// time at start
for (i=0; i<1000000; i++)				// 1.0e6 calls
	delay_1us();
clock_gettime(CLOCK_REALTIME, &tn);		// time now
diff=1000000000*(tn.tv_sec-ts.tv_sec)+(tn.tv_nsec-ts.tv_nsec);	// ns
d_dly = (int)(0.5+(1.0e9*(double)d_dly/(double)diff));
printf("Calibrating internal clock: d_dly = %d\n", d_dly);

// the time required for the read_input() function is a significant time delay
clock_gettime(CLOCK_REALTIME, &ts);		// time at start
for (i=0; i<1000000; i++)				// 1.0e6 calls
	read_input();
clock_gettime(CLOCK_REALTIME, &tn);		// time now
diff=1000000000*(tn.tv_sec-ts.tv_sec)+(tn.tv_nsec-ts.tv_nsec);	// ns
rdinput_time=(int)(0.5+(double)diff/1.0e6);
printf("Time for read_input(): %d ns\n", rdinput_time);

load_pattern_addresses();

return OK;
}


// load pattern base addresses from <config_path>/ldpatterns.gl
static void load_pattern_addresses(void)
{
int n=0;
char line[120], *p;
FILE* ifp;

#ifndef SLS10
error - this code is specific for SLS10
#endif

strcpy(line, config_path);
strcat(line, "/ldpatterns.gl");
if ( (ifp=fopen(line, "r")) == NULL)
	printf("Could not open configuration file %s\n", line);
else
	{
	while (fgets(line, sizeof(line), ifp))
		{
		if ( (p=strstr(line, "baser0 = ")) )
			{sscanf(p+strlen("baser0 = "), "%x", &readchip_pattern0);
			printf("Setting readchip_pattern0 address: 0x%x\n", readchip_pattern0);
			n++;
			}
		else if ( (p=strstr(line, "baser1 = ")) )
			{sscanf(p+strlen("baser1 = "), "%x", &readchip_pattern1);
			printf("Setting readchip_pattern1 address: 0x%x\n", readchip_pattern1);
			n++;
			}
		else if ( (p=strstr(line, "exposems = ")) )
			{sscanf(p+strlen("exposems = "), "%x", &expose_pattern);
			printf("Setting expose_pattern address: 0x%x\n", expose_pattern);
			n++;
			}
		else if ( (p=strstr(line, "delayus = ")) )
			{sscanf(p+strlen("delayus = "), "%x", &delayus_pattern);
			printf("Setting delayus_pattern address: 0x%x\n", delayus_pattern);
			n++;
			}
		else if ( (p=strstr(line, "enable = ")) )
			{sscanf(p+strlen("enable = "), "%x", &enable_pattern);
			printf("Setting enable_pattern address: 0x%x\n", enable_pattern);
			n++;
			}
		else if ( (p=strstr(line, "disable = ")) )
			{sscanf(p+strlen("disable = "), "%x", &disable_pattern);
			printf("Setting disable_pattern address: 0x%x\n",disable_pattern);
			n++;
			}
		}
	fclose(ifp);
	printf("--- Configured %d pattern addresses from ldpatterns.gl\n", n);
	}
}


// read 32 bit data from the fifo
unsigned int fifo_read_d32(unsigned int addr)
{
return *(unsigned int *)(cpu_base_fifo + addr);
}


// read the fifo data register (0x0)
unsigned int fifo_read_data(void)
{
if (!fifoIsInitialized)
	fifoini();

return fifo_read_d32(fifo_data_reg);
}


// read the fifo event counter (0x4)
unsigned int fifo_read_event_cntr(void)
{
unsigned int data;

if (!fifoIsInitialized)
	fifoini();

data = fifo_read_d32(fifo_event_counter) & fifo_event_counter_mask;
if(PRINT) printf ("FIFO event counter is: %#x\n", data);

return data;
}


// read the fifo status register (0x8)
unsigned int fifo_read_status(void)
{
unsigned int data;

if (!fifoIsInitialized)
	fifoini();

data = fifo_read_d32(fifo_status_reg) & fifo_status_mask;
if(PRINT) printf ("FIFO status is: %#x\n", data);

return data;
}


// read status with only printing -- used in test functions
static int fifo_read_stat(void)
{
fifo_read_status();
return OK;
}


// write 32 bit data to the fifo
int fifo_write_d32(unsigned int addr, unsigned int data)
{
*(unsigned int *)(cpu_base_fifo + addr) = data;
return 0;
}


// write to the data register of the fifo - VME input must first be enabled
int fifo_write_data(unsigned int data)
{
if (!fifoIsInitialized)
	fifoini();

//if(PRINT) printf("Writing to fifo data register: %#x\n", data);
fifo_write_d32(fifo_data_reg, data);
write_count++;

return OK;
}


// write data word to the fifo control register
int fifo_write_control(unsigned int data)
{
if (!fifoIsInitialized)
	fifoini();

if(PRINT) printf("Writing to control register: %#x\n", data);
fifo_write_d32(fifo_control_reg, data);

return OK;
}


// write data word to the test function register
int fifo_write_test(unsigned int data)
{
if (!fifoIsInitialized)
	fifoini();

if(PRINT) printf("Writing to test function register: %#x\n", data);
fifo_write_d32(fifo_test_reg, data);

return OK;
}


/*
**  Test writing to and reading from the fifo.
**    It turns out that the fifo will accept 64K data points, and read them
**  back correctly.  But the word counter only has 14 bits, so counts beyond
**  16383 are reported incorrectly.  Bit 14 in that register is the timeout 
**  bit, and is always set at the end of sequence.
**    Of course, the designer expects you to write many "events", each smaller
**  than this limit.
*/


int fifo_test1(void)
{
int i, n=16000, q=11, wc, flag=False;	// 16383 is the max
unsigned int datum;

fifoini();						// initialize fifo
fifo_write_control(0x1f0);		// set standard state
printf("#1: ");fifo_read_stat();
fifo_write_control(0x8);		// set VME input to fifo
fifo_write_control(0x100);		// clear fifo
fifo_write_control(0x1);		// disable timeout
printf("#2: ");fifo_read_stat();
fifo_write_test(0x1);			// set test gate
printf("#3: ");fifo_read_stat();
for (i=0; i<n; i++)
	{
	if (i+q == 31329)
		fifo_write_data(i);		// intentional error
	else
		*(unsigned int *)(cpu_base_fifo + fifo_data_reg) = i+q;
	}
printf("#4: ");fifo_read_stat();
fifo_write_control(0x10);		// enable timeout
printf("#5: ");fifo_read_stat();
while (fifo_read_d32(fifo_status_reg) & 0x10)
	;			// wait for busy to reset (supposedly 8 us)

printf("#6: ");fifo_read_stat();
fifo_write_control(0x80);		// reset fifo source to ECL
fifo_write_control(0x4);		// set output from fifo to VME
// get the word count, masking out timeout bit
wc = fifo_read_d32(fifo_event_counter);
printf("Event counter raw readout: %#x\n", wc);
wc &= 0x3fff;
wc--;			// there is a dummy word at the end

// readback event counter
i = 0xff & fifo_read_d32(fifo_event_counter);
printf("Reading back %d words in %d events\n", wc, i);
if (wc != n)
	printf("Bad number of words - should be %d\n", n);

for (i=0; i<n; i++)
	{
	datum = *(unsigned int *)(cpu_base_fifo + fifo_data_reg);
	if (i+q != datum)
		{
		flag = True;
		printf("Check failed: i = %d, data = %#x, should be %#x\n", i, datum, i+q);
		}
	}

if (flag)
	printf("Readback found errors\n");
else
	printf("Readback completed successfully\n");

return OK;
}


// accept an array of selectors & read the fifo.  A one bit in a selector
// selects a column in the fifo.  The selectors can be in any order.  So,
// selector #0 could read col #5 of the fifo, selector #1 could read col #0
// of the fifo, etc.  Then, returned image #0 would come from col #5, returned
// image #1 would have come from col #0, etc.
int px_set_selector(int len, unsigned int data[])
{
int i, j, k, img_offset, chip_row, chip_col, idx, mone;
unsigned int tpix[16];
unsigned int fifo_word;

if (len<16)
	{
	printf("Data too short in px_set_selector: %d\n", len);
	return FAIL;
	}
else
	if(PRINT)printf("FIFO set selector got len = %d\n", len);

for(i=0, nchips=0; i<16; i++)
	{
	if (data[i] == 0)
		break;
	nchips++;
	selector[i] = data[i];	// selector array for chips at readout
	}

if (nchips>1 && nchips<15)
	for(i=nchips; i<16; i++)
		selector[i] = 0;

//if(PRINT)
//	for(i=0;i<nchips;i++) printf("Selector %d = %#2x\n", i, selector[i]);


//  Read the word count from the fifo and set pixels_remaining
while (fifo_read_status() & 0x10)
	;		// wait for busy to reset
fifo_write_control(0x80);		// set fifo source to ECL
fifo_write_control(0x4);		// set output from fifo to VME
bit_count = fifo_read_event_cntr();
bit_count &= 0x3fff;		// 14 bit counter - remove status bits
//printf("bit_count as read: %d\n",bit_count);

// sls06 has 51480 bits, which exceeds 14-bit counter capacity.  But, the
// bits are nonetheless in the fifo.  Here we adjust, assuming that the
// number of bits is a correct multiple of NBITS.  We could also read status
// after each data read.
// bit_count is 1+NBITS*COLS*ROWS after adjusment, if all is working well
k = bit_count;
for (i=0; i<3 && bit_count%NBITS; i++)
	bit_count += 16384;			// 14-bit counter overflow
if (bit_count%NBITS)	// failed
	{
//	bit_count = k;			// leave it untouched
	printf("non-commensurate bit_count: %d\n", bit_count);
	}
else
	printf("bit_count was: %d; adjusted to: %d\n", k, bit_count);
//printf("bit_count after adjustment: %d\n",bit_count);

pixel_count = bit_count/NBITS;
pixel_count = pixel_count > COLS*ROWS ? COLS*ROWS : pixel_count;
//printf("pixel count %d\n", pixel_count);
event_count = fifo_read_event_cntr();
if (event_count > 1)
	printf("Found %d events; only the first is processed\n", event_count);

mone = -1;
idx = 0;
for (i=0; i<NBITS; i++)
	idx |= 1<<i;
mone &= idx;		// mask to NBITS
if (fifo_data_true_level == 0)
	memset(theImage, 0, sizeof(theImage));	// blank the image - do not use 0xff
else
	{
	for (i=0; i<MOD_IMAGE_SIZE; i++)
	theImage[i] = mone;
	}
#ifdef SLS06
if (nchips==1)
	{
	// single chip readout
	for (i=0; i<pixel_count; i++)	// total number of reads from fifo
		{
		for (k=0; k<nchips; k++)	// tpix assembles the pixels from 15 bits
			tpix[k] = 0;
		for (j=0; j<NBITS; j++)
			{
			fifo_word = fifo_read_data();
			for (k=0; k<nchips; k++)	// step thru the columns of data
				{
				tpix[k] <<= 1;
				if (fifo_word & selector[k])
					tpix[k] |= 0x1;
				}
			}
		j = COLS*(1+i/COLS) - i%COLS - 1;	// offset to flip each row
		for (k=0; k<nchips; k++)			// store data as fields for each chip
			theImage[j + k*pixel_count] = tpix[k];
		}
	}
#endif
#ifdef SLS08
printf("nchips=%d\n",nchips);
if (nchips==1)
	{
	// single chip readout
	printf("SLS08 single chip readout\n");
	for (i=0; i<pixel_count; i++)	// total number of reads from fifo
		{
		for (k=0; k<nchips; k++)	// tpix assembles the pixels from 15 bits
			tpix[k] = 0;
		for (j=0; j<NBITS; j++)
			{
			fifo_word = fifo_read_data();
			for (k=0; k<nchips; k++)	// step thru the columns of data
				{
				tpix[k] <<= 1;
				if (fifo_word & selector[k])
					tpix[k] |= 0x1;
				}
			}
//		j = COLS*(1+i/COLS) - i%COLS - 1;	// offset to flip each row
		for (k=0; k<nchips; k++)			// store data as fields for each chip
//			theImage[j + k*pixel_count] = tpix[k];
			theImage[i] = tpix[k];
		}
	pixel_count *= nchips;
	}
#endif
#ifdef SLS10
printf("nchips=%d\n",nchips);
if (nchips==1)
	{
	// single chip readout
	printf("SLS10 single chip readout: %d pixels\n", pixel_count);
	for (i=0; i<pixel_count; i++)	// total number of reads from fifo
		{
		for (k=0; k<nchips; k++)	// tpix assembles the pixels from 20 bits
			tpix[k] = 0;
		for (j=0; j<NBITS; j++)
			{
			fifo_word = fifo_read_data();
			for (k=0; k<nchips; k++)	// step thru the columns of data
				{
				tpix[k] <<= 1;
				if (fifo_word & selector[k])
					tpix[k] |= 0x1;
				}
			}
		j = COLS*(1+i/COLS) - i%COLS - 1;	// offset to flip each row
		for (k=0; k<nchips; k++)			// store data as fields for each chip
			theImage[j + k*pixel_count] = tpix[k];
		}
	pixel_count *= nchips;
	}
#endif

else
	{
	// a straight readout of 8x2 module, with extra pixels (resulting from
	// double and quad pixels on sensor) set to 0.
	// the selectors are taken from bit patterns in 'camera.def'.  selector 0
	// is the upper-left image, selector 7 is upper-right, selector 8 is
	// lower-left, and selector 15 is lower-right.

	chip_row = 0;
	chip_col = 0;

	for (i=0; i<pixel_count; i++)	// total number of reads from fifo
		{
		for (k=0; k<16; k++)	// tpix assembles the pixels from 15 bits
			tpix[k] = 0;
		// serial to parallel conversion
		for (j=0; j<NBITS; j++)
			{
			fifo_word = fifo_read_data();
//			fifo_word = *(unsigned int *)(cpu_base_fifo + fifo_data_reg);
			for (k=0; k<16; k++)	// step thru the columns of data
				{
				tpix[k] <<= 1;
				if (fifo_word & selector[k])
					tpix[k] |= 0x1;
				}
			}
		// tpix[] now contains 1 pixel of data for each chip being read
		// put pixels into image
		img_offset = chip_row*NCOLS_MOD + COLS - chip_col - 1;
		for (k = 0; k < 8; k++)
			theImage[img_offset + k*(COLS+2)] = tpix[k];
		img_offset = (NROWS_MOD - chip_row - 1)*NCOLS_MOD + chip_col;
		for (k = 8; k < 16; k++)
			theImage[img_offset + (k-8)*(COLS+2)] = tpix[k];

		chip_col++;
		chip_col %= COLS;
		if (chip_col == 0)
			chip_row++;
		}
	pixel_count *= 16;

	pixel_count += 7*2*(NROWS_MOD - 1) + NCOLS_MOD;	// adjust for filled-in pixels
	}

pixels_remaining = pixel_count;
pixels_done = 0;

printf("fifo contains %d words; write_count is %d; pixels to xmit %d\n",
		bit_count, write_count, pixel_count);
write_count=0;

return OK;
}


int px_read_image(unsigned int *len, int *npix, unsigned int *data[])
{
int i;

// set the return values
*npix = pixel_count;
*len = pixels_remaining > PIXELS_PER_READ ? PIXELS_PER_READ : pixels_remaining;
pixels_remaining -= *len;
*data = pixels;

// prepare the data
for (i=0; i<*len; i++)
	pixels[i] = (unsigned int)theImage[i+pixels_done];

pixels_done += *len;
if(PRINT)printf("Reading image from FIFO: npix = %d\n", *len);

if (pixels_remaining == 0)
	{
	fifo_read_data();			// one more read to set empty flag
	if (fifo_read_status() & 0x80)
		printf("End of data - FIFO is empty\n");
	else
		printf("*** End of data but FIFO is NOT empty\n");
	}
return OK;
}



// write image directly to disk thru NFS
int fifo_write_image(char *path, int len, unsigned int data[], unsigned int mode)
{
char imagefile[LENGTHFN];
int i, nb, ix, iy;
int ofd, pixcount;

memset(theImage, 0, sizeof(theImage));

printf("start fifo_write_image - path: %s\n", path);
if (mode != last_mode)
	{
	last_mode = mode;
	if (mode)
		xor_tab_initialize(0);
	else
		xor_tab_initialize(-1);
	}

strcpy(imagefile, image_path);		// local path, relative to site of boot-up
strcat(imagefile, path);

if ( (ofd = open(imagefile, O_WRONLY | O_CREAT, 0664)) == -1)
	{
	printf("Could not open %s for writing\n", imagefile);
	return FAIL;
	}

#ifdef SLS06
// for PPC in IOC - swap bytes
for (i=0; i<pixel_count; i++)
	theImage[i] = (theImage[i]>>8) | (theImage[i]<<8);

nb = write(ofd, (void *)theImage, pixel_count*sizeof(unsigned short));
if (nb > 0 && nb != pixel_count*sizeof(unsigned short))
	printf("fifo_write_image short write: %d bytes\n", nb);
else if (nb < 0)
	{
	perror("fifo_write_image");
	return FAIL;
	}
close(ofd);
#endif

#ifdef SLS07
// for PPC in IOC - swap bytes
pix16b = (unsigned short *)theImage;			// in-place truncation
for (i=0; i<pixel_count; i++)
	{
	pix16b[i] = (unsigned short)theImage[i];		// truncate
	pix16b[i] = (pix16b[i]>>8) | (pix16b[i]<<8);	// byte swap
	}

nb = write(ofd, (void *)pix16b, pixel_count*sizeof(unsigned short));
if (nb > 0 && nb != pixel_count*sizeof(unsigned short))
	printf("fifo_write_image short write: %d bytes\n", nb);
else if (nb < 0)
	{
	perror("fifo_write_image");
	return FAIL;
	}
close(ofd);
#endif

#ifdef SLS08
// for PPC in IOC - swap bytes
pix16b = (unsigned short *)theImage;			// in-place truncation
for (i=0; i<pixel_count; i++)
	{
	if (theImage[i]&0xffff0000)
		printf("Non-truncatable value: %d\n", theImage[i]);
	pix16b[i] = (unsigned short)theImage[i];		// truncate
	pix16b[i] = (pix16b[i]>>8) | (pix16b[i]<<8);	// byte swap
	}

nb = write(ofd, (void *)pix16b, pixel_count*sizeof(unsigned short));
if (nb > 0 && nb != pixel_count*sizeof(unsigned short))
	printf("fifo_write_image short write: %d bytes\n", nb);
else if (nb < 0)
	{
	perror("fifo_write_image");
	return FAIL;
	}
close(ofd);
#endif

#ifdef SLS10

fifo_write_control(0x1a5);				// clear fifo, no timeout, ECL->fifo
wait_us(2);
set_default_vpg(1);						// transfer data from module to fifo
clk_stop();
vme_jump(readchip_pattern1);			// readchip pattern - bottom half
clk_start();
wait_for_pattern(50002, NULL);
fifo_write_control(0x18);				// enable timeout
while (fifo_read_status() & 0x10) ;		// wait for busy to reset

px_set_selector(len, data);				// read bottom half of image from fifo
pixcount=pixel_count;

for(iy=0; iy<48; iy++)					// move 48 rows of data to bottom of tmp
	for (ix=0; ix<COLS; ix++)
		tmpimg[COLS*(iy+49)+ix] = theImage[COLS*(iy)+ix];

fifo_write_control(0x1a5);				// clear fifo, no timeout, ECL->fifo
wait_us(2);
set_default_vpg(1);						// transfer data from module to fifo
clk_stop();
vme_jump(readchip_pattern0);			// readchip pattern - top half
clk_start();
wait_for_pattern(50002, NULL);
fifo_write_control(0x18);				// enable timeout
while (fifo_read_status() & 0x10) ;		// wait for busy to reset

px_set_selector(len, data);				// read image from fifo
pixcount+=pixel_count;					// new total

pixcount = pixcount > NCOLS_MOD*NROWS_MOD ? NCOLS_MOD*NROWS_MOD : pixcount;

for(iy=49;iy<ROWS;iy++)					// bring back the lower half
	for(ix=0; ix<COLS; ix++)
		theImage[COLS*(iy)+ix]=tmpimg[COLS*(iy)+ix];

xor_decode_image(pixcount, mode);

// for PPC in IOC - reorder bytes
for (i=0; i<pixcount; i++)
	{
	theImage[i] &= BITMASK;
	a=(unsigned char *)(theImage+i);
	t=*a;
	*a=*(a+3);
	*(a+3)=t;
	t=*(a+1);
	*(a+1)=*(a+2);
	*(a+2)=t;
	}
nb = write(ofd, (void *)theImage, pixcount*sizeof(unsigned int));
if (nb > 0 && nb != pixcount*sizeof(unsigned int))
	printf("fifo_write_image short write: %d bytes\n", nb);
else if (nb < 0)
	{
	perror("fifo_write_image");
	return FAIL;
	}
close(ofd);

#endif			// SLS10


printf("fifo_write_image, %d pixels, mode %d: %s\n", pixcount, mode, imagefile);

return pixcount;
}


// write multiple, timed images to disk thru NFS
// note that the clock has a precision of 1 ns, but a granularity of 13 ms.
// this is the 1 module configuration for bl38b1 at SPring8.  It is completely
// different from the 1 bank setup for SLS.  VPG2 is used for timing here, 
// and VPG1 controls the readout
int fifo_write_n_images(char *path, int len, unsigned int data[], 
		unsigned int nimages, unsigned int exptime)
{
int count, ofd;
int num2write=MOD_IMAGE_SIZE*sizeof(unsigned short);
int i, imgcnt, nb, ddiff, odiff, maxd, mind;
char imagefile[LENGTHFN], *q, logfile[LENGTHFN];
long diff, mexpt;
struct timespec tb, ts, tn;
FILE *ofp;
double sumx, sumx2;

#ifndef SLS8x2_CAMERA
printf("*** This fifo_write_n_images is only for sls8x2 - no BCB\n");
return FAIL;
#endif

#ifndef TWO_VPGS
printf("*** fifo_write_n_images requires 2 VPGs\n");
return FAIL;
#endif

if (strcmp(path, previous_image) == NULL)
	{
	printf("************** ABORT DUPLICATE IMAGE ATTEMPT *************\n");
	return FAIL;
	}
strcpy(previous_image, path);

printf("Starting %d images: %s\n", nimages, path);

xor_tab_initialize(0);

// open the logfile
strcpy(logfile, image_path);
strcat(logfile, path);
if ( (q=strrchr(logfile, '/')) )
	{
	q++;
	strcpy(q, "logfile.txt");
	if ( (ofp=fopen(logfile, "a")) == NULL)
		{
		printf("Unable to open logfile: %s\n", logfile);
		return FAIL;
		}
	fprintf(ofp, "index  elapsed, ms    measured expt, ms      filename\n");
	}
else
	{
	printf("Cannot parse path: %s\n", path);
	return FAIL;
	}

// prepare the base filename
strcpy(imagefile, image_path);
strcat(imagefile, path);
if ( (q=strrchr(imagefile, '_')) )
	q++;
else if ( (q=strrchr(imagefile, '.')) )
	*q++ = '_';
else
	{
	strcat(imagefile, "_");
	q=imagefile+strlen(imagefile);
	}

count=0;
sumx=sumx2=0.0;
mind=1000000;
maxd=0;
odiff=0;

clock_gettime(CLOCK_REALTIME, &tb);		// time at beginning

set_default_vpg(1);
set_clk_divider(1);						// 10 MHz
clk_stop();
wait_us(1);

set_default_vpg(2);
set_clk_divider(1);						// 10 MHz
clk_stop();
wait_us(1);

// clear the module
set_default_vpg(1);
clk_stop();
vme_jump(readchip_pattern0);				// readchip10m pattern
clk_start();
wait_for_pattern(50000, ofp);

// start the first image
clock_gettime(CLOCK_REALTIME, &ts);		// time at start

// enable counting - vpg1  --  make the first exposure
set_default_vpg(1);
clk_stop();
vme_jump(enable_pattern);
clk_start();
wait_us(2);
set_default_vpg(2);
clk_stop();
vme_jump(expose_pattern);
clk_start();
wait_for_pattern(20000000, ofp);		// 20 sec max
set_default_vpg(1);
clk_stop();
vme_jump(disable_pattern);
clk_start();
wait_us(2);

#if 0
// ---------- just readout
fifo_write_control(0x1a5);				// clear fifo, no timeout, ECL->fifo
wait_us(2);
set_default_vpg(1);						// transfer data from module to fifo
clk_stop();
vme_jump(readchip_pattern0);				// readchip10m pattern
clk_start();
wait_for_pattern(50002, ofp);
fifo_write_control(0x18);				// enable timeout
while (fifo_read_status() & 0x10) ;		// wait for busy to reset

strcat(imagefile, "000.img");
ofd = open(imagefile, O_WRONLY | O_CREAT, 0664);
px_set_selector(len, data);		// read image from fifo - whole module only
xor_decode_image(pixel_count, 1);
for (i=0; i<pixel_count; i++)
	theImage[i] = (theImage[i]>>8) | (theImage[i]<<8);
nb = write(ofd, (void *)theImage, pixel_count*sizeof(unsigned short));
if (nb > 0 && nb != pixel_count*sizeof(unsigned short))
	printf("fifo_write_image short write: %d bytes\n", nb);
else if (nb < 0)
	{
	perror("fifo_write_image");
	return FAIL;
	}
close(ofd);
fclose(ofp);
printf("fifo_write_image, %d pixels:\n\t%s\n", pixel_count, imagefile);
return pixel_count;
// ----------------------
#endif

set_default_vpg(2);
clk_stop();
vme_jump(delayus_pattern);				// time the next operation - vpg2
clk_start();

// get time for previous image.  Since this gives control to the system,
// do it only inside the delayus_pattern pattern
clock_gettime(CLOCK_REALTIME, &tn);		// time now
mexpt=1000*(tn.tv_sec-ts.tv_sec)+((tn.tv_nsec-ts.tv_nsec)/1000000);	// ms

fifo_write_control(0x1a5);				// clear fifo, no timeout, ECL->fifo
wait_us(2);
set_default_vpg(1);						// transfer data from module to fifo
clk_stop();
vme_jump(readchip_pattern0);				// readchip10m pattern
clk_start();
wait_for_pattern(50002, ofp);
fifo_write_control(0x18);				// enable timeout
while (fifo_read_status() & 0x10) ;		// wait for busy to reset

// collect the remaining images while writing
for (imgcnt=1; imgcnt<nimages; imgcnt++)
	{
	set_default_vpg(2);					// wait for fixed ~6 ms delay
	wait_for_pattern(8000, ofp);

// data are in fifo
// enable counting - start next exposure
	set_default_vpg(1);
	clk_stop();
	vme_jump(enable_pattern);
	clk_start();
	wait_us(2);
	set_default_vpg(2);
	vme_jump(expose_pattern);
	clk_start();
	clock_gettime(CLOCK_REALTIME, &ts);		// time at start

// write image from fifo to disk
	px_set_selector(len, data);				// read image from fifo - whole module only
	xor_decode_image(pixel_count, 1);

	fifo_read_data();						// one more read to set empty flag
	if (fifo_read_status() & 0x80)
		printf("End of data - FIFO is empty\n");
	else
		printf("*** End of data but FIFO is NOT empty\n");

// write theImage to disk
// for PowerPC in IOC - swap bytes
	for (i=0; i<pixel_count; i++)
		theImage[i] = (theImage[i]>>8) | (theImage[i]<<8);

// write the image to NFS
	if (count<10)
		sprintf(q, "000%d.img", count);
	else if (count<100)
		sprintf(q, "00%d.img", count);
	else if (count<1000)
		sprintf(q, "0%d.img", count);
	else
		sprintf(q, "%d.img", count);
	if ( (ofd = open(imagefile, O_WRONLY | O_CREAT, 0664)) == -1)
		{
		printf("Could not open %s for writing\n", imagefile);
		return FAIL;
		}
	nb = write(ofd, (void *)theImage, num2write);
	if (nb > 0 && nb != num2write)
		printf("fifo_write_n_images short write: %d bytes\n", nb);
	else if (nb < 0)
		{
		perror("fifo_write_image");
		return FAIL;
		}
	close(ofd);
	printf("fifo_write_n_images, %d pixels: %s\n", pixel_count, imagefile);
// tn refers to the end of the previous exposure
	diff=1000*(tn.tv_sec-tb.tv_sec)+((tn.tv_nsec-tb.tv_nsec)/1000000);	// ms
	fprintf(ofp, "%4d  %10ld     %10ld      %s\n", count, diff, mexpt, imagefile);

// statistics
	ddiff=diff-odiff;
	odiff=diff;
	sumx+=ddiff;
	sumx2+=ddiff*ddiff;
	mind=ddiff<mind ? ddiff : mind;
	maxd=ddiff>maxd ? ddiff : maxd;
	count++;

// wait for end of exposure
	set_default_vpg(2);
	wait_for_pattern(20000002, ofp);		// 20 sec max
	set_default_vpg(1);
	clk_stop();
	vme_jump(disable_pattern);
	clk_start();
	wait_us(2);

	set_default_vpg(2);						// start 6.5 ms delay
	clk_stop();
	vme_jump(delayus_pattern);				// time the next operation - vpg2
	clk_start();

// move data from module to fifo
	clock_gettime(CLOCK_REALTIME, &tn);		// time now - end of exposure
	mexpt=1000*(tn.tv_sec-ts.tv_sec)+((tn.tv_nsec-ts.tv_nsec)/1000000);	// ms

	fifo_write_control(0x1a5);				// clear fifo, no timeout, ECL->fifo
	wait_us(2);
	set_default_vpg(1);						// transfer data from module to fifo
	clk_stop();
	vme_jump(readchip_pattern0);				// readchip10m pattern
	clk_start();
	wait_for_pattern(50002, ofp);
	fifo_write_control(0x18);				// enable timeout
	while (fifo_read_status() & 0x10) ;		// wait for busy to reset
	}

// write the last image from fifo to disk
set_default_vpg(2);							// wait for fixed ~6 ms delay
wait_for_pattern(8002, ofp);
px_set_selector(len, data);					// read image from fifo - whole module only
xor_decode_image(pixel_count, 1);

fifo_read_data();							// one more read to set empty flag
if (fifo_read_status() & 0x80)
	printf("End of data - FIFO is empty\n");
else
	printf("*** End of data but FIFO is NOT empty\n");

// write theImage to disk
// for PowerPC in IOC - swap bytes
for (i=0; i<pixel_count; i++)
	theImage[i] = (theImage[i]>>8) | (theImage[i]<<8);

// write the image to NFS
if (count<10)
	sprintf(q, "000%d.img", count);
else if (count<100)
	sprintf(q, "00%d.img", count);
else if (count<1000)
	sprintf(q, "0%d.img", count);
else
	sprintf(q, "%d.img", count);
if ( (ofd = open(imagefile, O_WRONLY | O_CREAT, 0664)) == -1)
	{
	printf("Could not open %s for writing\n", imagefile);
	return FAIL;
	}
nb = write(ofd, (void *)theImage, num2write);
if (nb > 0 && nb != num2write)
	printf("fifo_write_n_images short write: %d bytes\n", nb);
else if (nb < 0)
	{
	perror("fifo_write_image");
	return FAIL;
	}
close(ofd);
printf("fifo_write_n_images, %d pixels:\n\t%s\n", pixel_count, imagefile);
diff=1000*(tn.tv_sec-tb.tv_sec)+((tn.tv_nsec-tb.tv_nsec)/1000000);	// ms
fprintf(ofp, "%4d  %10ld     %10ld      %s\n", count, diff, mexpt, imagefile);

// statistics
ddiff=diff-odiff;
odiff=diff;
sumx+=ddiff;
sumx2+=ddiff*ddiff;
mind=ddiff<mind ? ddiff : mind;
maxd=ddiff>maxd ? ddiff : maxd;
count++;

if (count > 3)
	{
	sumx/=count;
	sumx2/=count;
	fprintf(ofp, "Jitter: N = %d, mean = %.1f, stdev = %.1f, min = %d, max = %d\n",
				count, sumx, sqrt(sumx2-sumx*sumx), mind, maxd);
	}
fclose(ofp);

set_default_vpg(1);
return OK;
}


int fifo_set_level(int value)
{
if (value != fifo_data_true_level)
	{
	fifo_data_true_level = value;
	xor_tab_initialize(-1);
	last_mode = 0;		// default is calibrate pulses
	printf("fifo_set_level - changing true level to %d\n", value);
	}
else
	fifo_data_true_level = value;
return OK;
}



/*****************************************************************************\
***                                                                         ***
***      Timer functions                                                    ***
***                                                                         ***
\*****************************************************************************/

// wait for pattern to finish
static void wait_for_pattern(int max_us, FILE *ofp)
{
int rval, diff=0, maxt;

rval = read_input();
if ( !(rval & 0x80) )
	{
	printf("wait_for_pattern(%d): pattern already done\n", max_us);
	if (ofp)
		fprintf(ofp, "wait_for_pattern(%d): pattern already done\n", max_us);
	return;
	}
// compensate for read_input() calls - careful of overflow - 21 sec max
maxt = 1 + (max_us*100)/(100+(rdinput_time/10));
while ( (rval & 0x80) && (diff < maxt) )
	{
	rval = read_input();
	delay_1us();
	diff++;
	}
diff += (diff*rdinput_time)/1000;	// compensate for read_input() calls
#if 0
if ((rval&0x80))
	printf("(%d): timed out after %d us\n", max_us, diff);
else
	printf("(%d): wait state detected after %d us\n", max_us, diff);
#endif
return;
}


// software timing routines - the realtime clock has a granularity of 16.7 ms !!
static void delay_1us(void)
{
int i, j=0;
for (i=0; i<d_dly; i++)
	j++;
return;
}


// wait for number of microseconds
void wait_us(int us)
{
int i;
for (i=0; i<us; i++)
	delay_1us();
return;
}


/*****************************************************************************\
***                                                                         ***
***      XOR functions                                                      ***
***                                                                         ***
\*****************************************************************************/
static void xor_decode_image(int length, int mode)
{
int i, k;

for (i=0; i<length; i++)
	theImage[i] = xor_table[theImage[i]];

#ifdef SLS08
return;
#endif
#ifdef SLS10
return;
#endif

if (!mode)
	return;

// fill in double and quad area pixels from the pixel that received the counts
// fill in for large sensor pixels by dividing counts
// do not lose the remainder

k = (NROWS_MOD/2)*NCOLS_MOD;	// the line across the center of the sensor
for (i=0; i<NCOLS_MOD; i++)
	{
	theImage[k + i] = theImage[k + i - NCOLS_MOD]/2;		// from pixel above
	theImage[k + i - NCOLS_MOD] -= theImage[k + i];
	}
for (i=0; i<NROWS_MOD; i++)		// the lines between chips
	for (k=0; k<7; k++)
		{
		theImage[i*NCOLS_MOD + (k+1)*(COLS+2) - 2] =
				theImage[i*NCOLS_MOD + (k+1)*(COLS+2) - 3]/2;	// from pixel to left
		theImage[i*NCOLS_MOD + (k+1)*(COLS+2) - 3] -=
				theImage[i*NCOLS_MOD + (k+1)*(COLS+2) - 2];
		theImage[i*NCOLS_MOD + (k+1)*(COLS+2) - 1] = 
				theImage[i*NCOLS_MOD + (k+1)*(COLS+2) - 0]/2;	// from pixel to right
		theImage[i*NCOLS_MOD + (k+1)*(COLS+2) - 0] -=
				theImage[i*NCOLS_MOD + (k+1)*(COLS+2) - 1];
		}

return;
}


static int xor_tab_initialize(int zstate)
{
int i, j, idx, count=0;

idx = 0;
j = max(16, NBITS);
for (i=0; i<j; i++)
	idx |= 1<<i;
zstate &= idx;		// mask to NBITS or 16

for (i=0; i<NBITS; i++)
	state[i]=1;		// initial state - all 1's
	
for (i=0; i<NSTATES; i++)
	xor_table[i]=zstate;			// empty result table

if (fifo_data_true_level == 1)
	{
	for (j=0; j<NSTATES-1; j++)			// simulate NSTATES-1 counts
		{
		xor_add_count(state);
		count++;
		idx = 0;
		for (i=0; i<NBITS; i++)		// put the resulting bit pattern in table
			if  ( (state[i]) )
				idx |= 1<<i;
		xor_table[idx] = (count % (NSTATES-1));
		}
	}
else			// inverted data
	{
	for (j=0; j<NSTATES-1; j++)			// simulate NSTATES-1 counts
		{
		xor_add_count(state);
		count++;
		idx = 0;
		for (i=0; i<NBITS; i++)		// put the resulting bit pattern in table
			if  ( !(state[i]) )
				idx |= 1<<i;
		xor_table[idx] = (count % (NSTATES-1));
		}
	}

return NSTATES;		// tell caller how many entries
}


static void xor_add_count(int stat[])
{
int i;
int new_state[NBITS];

// the general case -- shift-register without feedback-- note that 0 is not set
for (i=0; i<NBITS-1; i++)
	new_state[i+1] = stat[i];

// now, the specific feedback -- always the last bit plus one other
new_state[0]= xor(stat[NBITS-1], stat[fbBIT]);

// copy the new state into state register
for (i=0; i<NBITS; i++)
	stat[i] = new_state[i];

return;
}


static int xor(int a, int b)
{
if (a == 0)
	return b;
else
	return !b;
}

