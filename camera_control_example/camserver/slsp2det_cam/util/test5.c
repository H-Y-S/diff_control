/* test1.c -- its sole function is to test linking in ../util */
/* also can be used to generate the pseudo-random counter lookup table
** as text
*/

#include <stdio.h>
#include <sys/time.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/poll.h>
#include <fcntl.h>
#include <unistd.h>
#include "debug.h"
#include "tvxobject.h"
#include "tvxcampkg.h"
#include "camserver.h"

int DefaultHigh=1024;
int DefaultWide=1024;
int DefaultBpp=16;

#ifdef DEBUG
FILE *dbgofp;   // global output file
int dbglvl;     // debugging control level
#endif

// functions defined in this module
void detrim_gen(void);
void analyze_readout(char *);


// globals
int masterProcess;
int controllingProcess;
char camstat_path[LENFN] = "\0";
char cam_data_path[LENFN] = "./cam_data/";
char cam_image_path[LENFN] = "./images/";
char cam_initial_path[LENFN] = "./";
char cam_startup_file[LENFN] = "\0";
int camPort;
int selected_chip = -1;
int selected_mod=-1;
int selected_bank=-1;
char image_file[10], imgfile[10];
char header_string[150];
double endTime, startTime, exposure_time;
short trimValues[NBANK][NMOD_BANK][NCHIP][NROW_CHIP][NCOL_CHIP];
struct pollfd ufds[2];
char txBuffer[150] = "\0";
int n_bad_pix;
int filenumber;
int loadingTrimFile;
char autofilename[20];


int main (int argc, char *argv[])
{
char line[20], line2[120];
int key=0, i, ix=0, n, count=0, diff, mindiff, maxdiff;
double a=0;
struct timeval tvs, tvn;
struct timezone tz;
double sum=0.0;
extern double tau;
int col, row;

printf("Starting test\n");

#ifdef DEBUG
// Start debugger
        debug_init("debug.out", NULL);
        dbglvl=9;               // full details
#endif

// to test pix_add
col = 20;
row = 20;
px = pix_add(-1, col, row, -1, -1, -1, -1, -1, -1, -1, -1, -1);
if (px->cx<0)
	printf("Pixel not in a chip\n");
printf("  bank # %d, bank (x, y) = (%d, %d)\n", px->bn, px->bx, px->by);
printf("  module # %d, module (x, y) = (%d, %d)\n", px->mn, px->mx, px->my);
printf("  chip # %d, chip (x, y) = (%d, %d)\n", px->cn, px->cx, px->cy);
return 0;


// to test rate correction
tau=126.0e-9;
exposure_time = 0.1;
printf("Test using tau = %.1lfe-9, expt = %.6lf s\n", 1.0e9*tau, exposure_time);

n = xor_tab_initialize(-1);		// set up the xor shift register translation table

// if just testing rate correction, return here
// return 0;

// decode raw (serial) image - adapt this for the problem at hand
// usage:  test5 <filename>
if (argc>1)
	{
	analyze_readout(argv[1]);
	return 0;
	}

if (tau==0.0)
	{
	ix = ~(-1<<NBITS);
	for (i=0; i<n; i++)
		if (xor_table[i] != ix)
			count++;
	printf("For a count of %d, found %d unique states\n", n, count);
	}

	// print some critical values
i=0; printf("xor_table[%d] = %d\n", i, xor_table[i]);
i=1; printf("xor_table[%d] = %d\n", i, xor_table[i]);
i=2; printf("xor_table[%d] = %d\n", i, xor_table[i]);
i=n-1; printf("xor_table[%d] = %d\n", i, xor_table[i]);
i=n-2; printf("xor_table[%d] = %d\n", i, xor_table[i]);
i=n-3; printf("xor_table[%d] = %d\n", i, xor_table[i]);
i=n-4; printf("xor_table[%d] = %d\n", i, xor_table[i]);
i=n-5; printf("xor_table[%d] = %d\n", i, xor_table[i]);
i=n-6; printf("xor_table[%d] = %d\n", i, xor_table[i]);
i=n-7; printf("xor_table[%d] = %d\n", i, xor_table[i]);
i=n-8; printf("xor_table[%d] = %d\n", i, xor_table[i]);

detrim_gen();

// write the xor table as text - 43 MB for 20 bits!
#if 0
{
int j, state[NBITS], idx;
FILE *ofp;

sprintf(line, "prc%dbit.txt", NBITS);
if ( (ofp=fopen(line, "w")) == NULL)
	{
	printf("Could not open %s for writing\n", line);
	return 1;
	}
fprintf(ofp, " *** printed by util/test5.c - NBITS = %d\n", NBITS);
fprintf(ofp, " COUNT      PRC BINARY       HEX   DEC\n");
for (i=0; i<NBITS; i++)
	state[i]=1;		// initial state - all 1's
for (j=0; j<NSTATES-1; j++)
	{
	idx = 0;
	line2[0] = '\0';
	for (i=NBITS-1; i>=0; i--)
		{
		if ( (state[i]) )
			{
			idx |= 1<<i;
			strcat (line2, "1");
			}
		else
			strcat (line2, "0");
		}
	fprintf(ofp, "%7d %s %5x %7d\n", j, line2, idx, idx);
	xor_add_count(state);
	}
fclose(ofp);
}
#endif

// timing test
count=0;
maxdiff=0;
mindiff=1000000;
for(i=0;i<100000;i++)
	{
	gettimeofday(&tvs, &tz);
	gettimeofday(&tvn, &tz);
	diff=1000000*(tvn.tv_sec-tvs.tv_sec)+(tvn.tv_usec-tvs.tv_usec);
	mindiff=min(mindiff,diff);
	maxdiff=max(maxdiff,diff);
	sum+=(double)diff;
	count++;
	}
sum/=count;
printf("Timer stats: count = %d, avgdiff = %f, mindiff = %d, maxdiff = %d\n",
		count, sum, mindiff, maxdiff);



if (key)
	cam_config_initialize(NULL);

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
	camera_status(0);

if (key)
	camera_read_setup(line, line2);

if(key)
	initialize_theImage(5);

if (key)
	convert_image(line);

if (key)
	convert_image_add(line);

if (key)
	gap_fill_image();

if (key)
	setup_file_series(line);

if (key)
	next_file_in_series();

if (key)
	setTrims();

if (key)
	init_trim_array(-1);

if (key)
	parse_mx_param_string(line);

if (key)
	format_mx_params(line, 100);

if (key)
	inc_mx_start_angle(1);

if (key)
	show_calibration(line);

if (key)
	set_calibration(line, line2);

printf("Test completed\n");
return 0;
}


/****************************************************************************\
**                                                                          **
\****************************************************************************/

// generate the values for the de-trim function for p2 chip, 32-bit readout
void detrim_gen(void)
{
int bit[6], i, j, baseword, mask, entry;
FILE *ofp;

bit[0] = 0x20;		// masks to locate the respective trim bits
bit[1] = 0x400;
bit[2] = 0x200;
bit[3] = 0x40;
bit[4] = 0x8;
bit[5] = 0x10;

if ( (ofp=fopen("/tmp/detrim_prog.txt", "w")) == NULL )
	{
	printf("could not open output file\n");
	return;
	}

fprintf(ofp,"// computed by camserver/slsp2_1m_cam/util/test5\n");
for(i=0; i<64;i++)
	{
	baseword = 0xfffff;
	mask=1;
	for(j=0; j<6; j++)
		{
		if (i & mask)
			baseword &= (0xfffff & ~bit[j]);
		mask<<=1;
		}
	entry = xor_table[baseword];

//	entry &= 0xffff;		// for 16-bit readout
//	fprintf(ofp, "case %d: *usP=%d; break;\n", entry, i);

	// 32-bit readout
	fprintf(ofp, "case %d: *uiP=%d; break;\n", entry, i);
	}
//fprintf(ofp, "default: *usP=0xffff;break;\n");		// 16-bit
fprintf(ofp, "default: *uiP=0xfffff;break;\n");	// 32-bit

fclose(ofp);
printf("file /tmp/detrim_prog.txt was written\n");

return;
}



/****************************************************************************\
**                                                                          **
\****************************************************************************/
// analyze raw (serial) image files - single module
void analyze_readout(char *fname)
{
int fd, i, j, k, n, tpix[16], row, col, bit_mask, raw_pix[16], conv_pix[16];;
unsigned int *cP[16];
unsigned short us[16*20*97*60/8], *usp=us;
unsigned int mod[NROW_MOD*NCOL_MOD], *modP=mod;
char line[80];

memset(us, 0, sizeof(us));
if ((fd = open(fname, O_RDONLY))< 0)
	{
	printf("Can't open file: %s\n", fname);
	return;
	}
read(fd, us, sizeof(us));
close(fd);
memset(mod, 0xff, sizeof(mod));


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

// capture the bit pattern for this pixel in the 16 chips
for (k=0;  k<16; k++)
	raw_pix[k] = conv_pix[k] = 0;
for (k=0;  k<16; k++)
	{
	raw_pix[k] = tpix[k];
	conv_pix[k] = xor_table[tpix[k]];
	if (conv_pix[k] > 100)			// condition to trap
		i++;						// debugger handle
	}

		for (k = 0; k < 16; k++)
			*cP[k] = xor_table[tpix[k]];		// xor and store in image
		for (k = 0; k < 8; k++)					// move to next col
			{
			cP[k]--;
			cP[k+8]++;
			}
		}
	for (k = 0; k < 8; k++)						// move to next row
		{
		cP[k] += NCOL_BANK + NCOL_CHIP;
		cP[k+8] -= NCOL_BANK + NCOL_CHIP;
		}
	}

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

strcpy(line, "/tmp/test5.img");
fd=open(line, O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR);
write(fd, mod, NROW_MOD*NCOL_MOD*4);
close(fd);

printf("File was written: %s\n", line);

return;
}




/****************************************************************************\
**                                                                          **
\****************************************************************************/
// dummy routines

int read_detector_info(char *p)
{return 0;}

int set_i2c(char *a)
{return 0;}

int prog_i2c(char *a)
{return 0;}

int dcb_initialize(char *a)
{return 0;}

int dcb_get_gsdevno(int a, int b)
{return 0;}

int dcb_insert_row_col_token(int devno)
{return 0;}

int dcb_trim_pixels(int a)
{return 0;}

int dcb_trim_all(int a, int b)
{return 0;}

int dcb_trim_fifo_reset(int a)
{return 0;}

int dcb_trim_fifo_ld(int a, int b)
{return 0;}

int dcb_expose(void)
{return 0;}

int dcb_check_status(char *a)
{return 0;}

int dcb_start_read_image(void)
{return 0;}

double dcb_wait_read_image(char **a)
{return 0.0;}

int dcb_reset_image_fifo(int a)
{return 0;}

int dcb_image_fifo_enable(int a)
{return 0;}

double dcb_calc_remaining_time(void)
{return 0.0;}

void dcb_set_autoframe(void)
{return;}

void dcb_reset_autoframe(void)
{return;}

int dcb_reset_exposure_mode(void)
{return 0;}

int dcb_set_external_enb(int a)
{return 0;}

int dcb_set_multi_trigger(int state)
{return 0;}

int dcb_external_enb(void)
{return 0;}

int dcb_external_trig(void)
{return 0;}

int dcb_external_multi_trig(void)
{return 0;}

int dcb_set_exposure_time(double a)
{return 0;}

int dcb_set_exposure_period(double a)
{return 0;}

double dcb_get_exposure_period(void)
{return 0.0;}

int dcb_set_exp_trigger_delay(double a, int b)
{return 0;}

double dcb_get_exp_trigger_delay(int a)
{return 0;}

int dcb_set_external_trigger(int a)
{return 0;}

int dcb_set_exposure_count_limit(unsigned int a)
{return 0;}

int dcb_set_tx_frame_limit(int a)
{return 0;}

int dcb_set_bank_module_address(int a, int b)
{return 0;}

unsigned int dcb_get_exposures_in_frame(void)
{return 0;}

double dcb_get_measured_exposure_time(void)
{return 0.0;}

void dcb_open_log(void)
{return;}

void dcb_close_log(void)
{return;}

int dcb_stop(void)
{return 0;}

unsigned int dcb_get_exposure_count_limit(void)
{return 0;}

double dcb_overhead_time(void)
{return 0.0;}

int dcb_write_data_allpix(int a)
{return 0;}

int dcb_set_readout_bits(int a)
{return 0;}

int dcb_get_readout_bits(void)
{return 0;}

int dcb_set_dma_holdoff_time(double a)
{return 0;}

double dcb_get_dma_holdoff_time(void)
{return 0.0;}

double dcb_read_temp(int a)
{return 0.0;}

double dcb_read_humidity(int a)
{return 0.0;}

double dcb_read_temp_high_limit(int a)
{return 0.0;}

double dcb_read_temp_low_limit(int a)
{return 0.0;}

double dcb_read_humidity_high_limit(int a)
{return 0.0;}

int gs_device_count(void)
{return 1;}

int gs_set_t0(void)
{return 0;}

double gs_time(void)
{return 0.0;}

char *gs_timestamp(void)
{return imgfile;}

int gs_status(int a)
{return 0;}

int gs_irq_src_status(int a)
{return 0;}

double time_log(int a, char *b)
{return 0.0;}

int read_detector_map(char *a)
{return 0;}

int read_detector_offsets(char *a)
{return 0;}

int set_canonical_form(char *a)
{return 0;}

void get_vrfall(char *a, int b, int c)
{return;}

#ifdef USE_GPIB
int gpibread(char *a)
{return 0;}

int gpibsend(char *a)
{return 0;}

int rpcopen(char *a)
{return 0;}

void *rpcclose(void)
{return NULL;}
#endif

