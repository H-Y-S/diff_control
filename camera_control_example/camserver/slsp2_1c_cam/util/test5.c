/* test1.c -- its sole function is to test linking in ../util */
/* also can be used to generate the pseudo-random counter lookup table
** as text
*/

#include <stdio.h>
#include <sys/time.h>
#include <unistd.h>
#include <string.h>
#include "debug.h"
#include "tvxcampkg.h"
#include "camserver.h"

#ifdef DEBUG
FILE *dbgofp;   // global output file
int dbglvl;     // debugging control level
#endif

// functions defined in thei module
void detrim_gen(void);

// globals
char camstat_path[LENFN] = "\0";
char cam_data_path[LENFN] = "./cam_data/";
char cam_image_path[LENFN] = "./images/";
char cam_initial_path[LENFN] = "./";
char cam_startup_file[LENFN] = "\0";
int camPort;
char iocHost[20]="ioc038";
unsigned int selectors[17];
int selected_chip = -1;
int selected_mod=-1;
int selected_bank=-1;
unsigned int theImage[20];
int imageLength;
int vpg_default_unit;
int pgRunning[2];
unsigned int base_trim_all_pattern;
unsigned int base_enable_pattern;
unsigned int base_disable_pattern;
unsigned int base_read_chip_pattern0;
unsigned int base_read_chip_pattern1;
unsigned int base_vpg_ena_pattern;
unsigned int i2c_pattern_base;
int n_images = 0;
int n_image_expt = 1;
char image_file[10];
int i2c_vpg_unit=1;
unsigned short thePattern[20];
unsigned short *patternP;
double exposure_time;


int main (int argc, char *argv[])
{
char line[20], line2[120];
int key=0, i, ix=0, n, count=0, diff, mindiff, maxdiff;
double a=0;
struct timeval tvs, tvn;
struct timezone tz;
double sum;
/*
int j, state[NBITS], idx;
FILE *ofp;
*/

printf("Starting test\n");

#ifdef DEBUG
// Start debugger
        debug_init("debug.out", NULL);
        dbglvl=9;               // full details
#endif


n = xor_tab_initialize(-1);		// set up the xor shift register translation table
ix = ~(-1<<NBITS);
for (i=0; i<n; i++)
	if (xor_table[i] != ix)
		count++;
printf("For a count of %d, found %d unique states\n", n, count);
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
/*
sprintf(line, "prc%dbit.txt", NBITS);
if ( (ofp=fopen(line, "w")) == NULL)
	{
	printf("Could not open %s for writing\n", line);
	return 1;
	}
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
*/

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
	camera_read_setup(line, line2);

#ifdef SP8_BL38B1
// --- SPring8 BL38 stuff
if (key)
	connect_ra4();
if (key)
	disconnect_ra4();
if (key)
	cm_input(line);
if (key)
	cm_output(line);
if (key)
	cm_shutter(3);
if (key)
	cm_getmotval();
if (key)
	cm_putmotval();
if (key)
	cm_check_motion();
if (key)
	cm_moveto(line, 8.0, 10.1);
if (key)
	cm_manual(11);
if (key)
	cm_home();
if (key)
	cm_dc(line, 14.0, 6.3);
if (key)
	check_port();
// --- end of BL38 section
#endif		// SP8_BL38B1

//#ifdef SLS8x2_CAMERA
//if (key)
//	(void)pix_add(0,0,0,0,0,0);
//#endif


printf("Test completed\n");
return 0;
}




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



// dummy routines

int read_detector_info(char *p)
{return 0;}

void init_trim_array(int q)
{return;}

int write_image_nfs(char *a, int b)
{return 0;}

int write_n_images_nfs(char *a, unsigned int b, unsigned int c)
{return 0;}

int vpg_set_default(int a)
{return 0;}

int vpg_set_jump(unsigned int a, int b)
{return 0;}

int vpg_start_clock(int a)
{return 0;}

int vpg_stop_clk(int a)
{return 0;}

unsigned int vpg_read(unsigned int a, int b)
{return 0;}

int prog_i2c(char *a)
{return 0;}

void vpg_wait_pattern(int i, int j)
{return;}

int vpg_set_clk_divider(int i, int j)
{return 0;}

int fifo_set_true_level(int i)
{return 0;}

int fifo_write_control(unsigned int i)
{return 0;}

int vpg_execute_pattern(unsigned short a[], int b, int c)
{return 0;}

int write_image_pci(char *path, int mode, int bankonly)
{return 0;}

int record_n_images_pci(char *img_name, int n)
{return 0;}

//-- BHe
int vpg_write(unsigned int a, unsigned int b, int c)
{return 0;}

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

