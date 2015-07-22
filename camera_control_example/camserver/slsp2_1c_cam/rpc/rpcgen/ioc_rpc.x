/* add new cases to the bottom only to avoid 
** renumbering work you've already done!!!
*/


struct address {
	unsigned int addr;
	unsigned int data;
	} ;

struct vpg {
	string patname<80>;
	unsigned int divider;
	unsigned int delay15;
	unsigned int delay16;
	unsigned int addr;
	unsigned int data;
	} ;

struct detector {
	string fname<80>;
	unsigned int module;
	unsigned int chip;
	unsigned int strip;
	unsigned int data;
	} ;

struct image {
	int npix;
	unsigned int img<600>;	/* must read segmentally */
	} ;

struct arry {
	unsigned int data<16>;
	} ;

struct image_spec {
	string patname<160>;
	unsigned int data<16>;
	unsigned int mode;
	} ;

struct n_image_spec {
	string patname<160>;
	unsigned int data<16>;
	unsigned int nimg;
	unsigned int etime;
	} ;


/* XDR file to call the VPG and readout routines over rpc */

program rpcfns {
 version TWO {

/* function declarations */

/* functions for VME read write to vpg */
int rpc_vpg_write_a32(address)=1;
int rpc_vpg_read_a32(address)=2;


/* functions for VPG */
int rpc_vpg_set_clk_divider(vpg)=3;
int rpc_vpg_set_clk_delay(vpg)=4;
int rpc_vpg_write_output(vpg)=5;
int rpc_vpg_read_input(void)=6;
int rpc_vpg_loadpattern(vpg)=7;
int rpc_vpg_jump(vpg)=8;
int rpc_vpg_clk(void)=9;
int rpc_vpg_write_status(vpg)=10;
int rpc_vpg_read_status(void)=11;
int rpc_vpg_set_force(vpg)=12;
int rpc_vpg_clk_start(void)=13;
int rpc_vpg_clk_stop(void)=14;
int rpc_vpg_pc_reset(void)=15;
int rpc_vpg_clr_mem(void)=16;
int rpc_vpg_vpgini(void)=17;

/* functions for the FIFO */
int rpc_fifo_read_d32(address)=50;
int rpc_fifo_write_d32(address)=51;
int rpc_fifo_fifoini(void)=52;
int rpc_fifo_read_data(address)=53;
int rpc_fifo_read_event_cntr(address)=54;
int rpc_fifo_read_status(address)=55;
int rpc_fifo_write_data(address)=56;
int rpc_fifo_write_control(address)=57;
int rpc_fifo_write_test(address)=58;

/* functions for strip detector readout */
int rpc_sd_selectmodule(detector)=101;
int rpc_sd_selectchip(detector)=102;
int rpc_sd_selectstrip(detector)=103;

int rpc_sd_setdac(detector)=104;
int rpc_sd_reset(detector)=105;
int rpc_sd_readmodule(detector)=106;
int rpc_sd_readdet(detector)=107;
int rpc_sd_testFPGA(detector)=108;
int rpc_sd_teststshift(detector)=109;
int rpc_sd_testdtshift(detector)=110;
int rpc_sd_settrimbits(detector)=111;
int rpc_sd_initdet(detector)=112;
int rpc_sd_writest(detector)=113;
int rpc_sd_readst(detector)=114;
int rpc_sd_writedt(detector)=115;
int rpc_sd_readdt(detector)=116;
int rpc_sd_writemodFPGA(detector)=117;
int rpc_sd_readmodFPGA(detector)=118;
int rpc_sd_writedetFPGA(detector)=119;
int rpc_sd_readdetFPGA(detector)=120;

/* functions for pixel detector readout */
image rpc_px_read_image(void)=200;
int rpc_px_set_selector(arry)=201;
int rpc_vpg_set_default(int)=202;
int rpc_fifo_write_image(image_spec)=203;
int rpc_fifo_write_n_images(n_image_spec)=204;
int rpc_fifo_set_level(int)=205;

 }=2;
} = 10071971;
