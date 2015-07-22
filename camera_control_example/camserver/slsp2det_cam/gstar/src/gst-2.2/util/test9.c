/* test9.c -- its function is to test linking in ../util */

#include <stdio.h>
#include "debug.h"
#include "detsys.h"
#include "gslib.h"

#ifdef DEBUG
FILE *dbgofp;   // global output file
int dbglvl;     // debugging control level
#endif


int main (int argc, char *argv[])
{
int key=0, i, t;
unsigned int *tp;
char *buf=NULL;
double d;

#ifdef DEBUG
// Start debugger
debug_init("debug.out", argv[1]);
dbglvl=9;               // full details
#endif

if (key)
	gs_initialize(buf);

if (key)
	i = gs_reset(0);

if (key)
	i = gs_start(0);

if (key)
	t = gs_status(0);

if (key)
	t = gs_irq_status(0);

if (key)
	tp = gs_read_bar0_reg(0, 0, 4);

if (key)
	gs_set_control(0, 0x11);

if (key)
	gs_reset_control(0, 0x11);

if (key)
	gs_set_irq_mask(0, 0x5555);

if (key)
	gs_clear_irq(0);

if (key)
	t = gs_rx_data(0, &t, 4);

if (key)
	t = gs_tx_data(0, &t, 4);

if (key)
	buf = gs_rx_dma_setup(0, 0x1000);

if (key)
	buf = gs_tx_dma_setup(0, 0x1000);

if (key)
	t = gs_rx_dma(0);

if (key)
	t = gs_tx_dma(0);

if (key)
	i = gs_set_t0(0);

if (key)
	d = gs_time();


printf("Test9 completed: %s\n", timestamp());
return 0;
}


// dummy functions
