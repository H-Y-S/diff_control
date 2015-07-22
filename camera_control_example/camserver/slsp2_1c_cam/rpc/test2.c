/* test2.c -- its sole function is to test linking in ../rpc */

#include <stdio.h>
#include "ppg_util.h"
#include "debug.h"

float clock_frequency;
float vpg_crystal_frequency;
unsigned int start_line;
unsigned int i2c_pattern_base=0x7800;
unsigned int theImage[5280];
int imageLength;
char cam_data_path[80];
int pgRunning[2]={0,0};

#ifdef DEBUG
FILE *dbgofp;   // global output file
int dbglvl;     // debugging control level
#endif

int main (int argc, char *argv[])

{
int key=0;
unsigned short arry[20];

#ifdef DEBUG
// Start debugger
        debug_init("debug.out", NULL);
        dbglvl=9;               // full details
#endif

if(key)
	vpg_execute_pattern(arry, 20, 1);

if(key)
	vpg_load_pattern("name", 1);

if(key)
	vpg_wait_pattern(1, 1);

if(key)
	vpg_start_clock(1);

if(key)
	vpg_stop_clk(1);

if(key)
	vpg_pc_reset(1);

if(key)
	vpg_set_jump(0, 1);

if(key)
	vpg_set_clk_divider(3, 1);

if(key)
	vpg_init();

if(key)
	vpg_clear(1);

if(key)
	vpg_step(1);

if(key)
	vpg_read_status(1);

if(key)
	vpg_read_p(0, 1);

if(key)
	vpg_read(0, 1);

if(key)
	vpg_write_status(0, 1);

if(key)
	vpg_write(0, 0, 1);

if(key)
	vpg_set_default(1);

if(key)
	fifo_init();

if(key)
	fifo_write(0, 0);

if(key)
	fifo_write_control(0);

if(key)
	fifo_write_test(0);

if(key)
	fifo_write_data(0);

if(key)
	fifo_read_status();

if(key)
	fifo_read_status_p();

if(key)
	fifo_wait_busy(1);

if(key)
	fifo_read(0);

if(key)
	fifo_read_event();

if(key)
	fifo_read_data();

if(key)
	fifo_read_image();

if(key)
	write_image_nfs("path", 0);

if(key)
	write_n_images_nfs("path", 0, 0);

if(key)
	fifo_set_true_level(0);

printf("Test completed\n");
return 0;
}
