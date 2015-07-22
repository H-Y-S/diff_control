/*
** dcbt.c - test program for the dcb through the gigastar link
*/

/*
** Copyright (C) 2004-2010 Eric F. Eikenberry
** Paul Scherrer Institute, CH-5232 Villigen-PSI, Switzerland.
** All rights reserved.
** GPL license declared below.
*/

/* from commando.c - an easy-to-use command dispatching program
**
**	Copyright 1999 - 2010 by Eric F. Eikenberry.
**
** Usage:
**   command <arguments>
**
** where 'command' is a command defined in commando's menu (see 'hello'
** command below), and 'arguments' is any text.
**
** To add new functions:
**
**  (1) Add new cases to the command dispatch loop as desired.  Be sure
**        to terminate the case with 'break'.
**  (2) Implement your new command, using as data the text at *ptr.
**
** See the example of the 'menu' command implemented below.  Note that the 
** command in the switch-case must differ from the subroutine called, say
** by having the first letter capitalized.
**
** debug.c is included for help in debugging.  It can be omitted by undefining
** DEBUG in the makefile, or renedered silent by setting dbglvl to 0.
**
**
**
**   This program is free software; you can redistribute it and/or modify
**   it under the terms of the GNU General Public License as published by
**   the Free Software Foundation; either version 2 of the License, or
**   (at your option) any later version.
**
**   This program is distributed in the hope that it will be useful,
**   but WITHOUT ANY WARRANTY; without even the implied warranty of
**   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
**   GNU General Public License for more details.
**
**   You should have received a copy of the GNU General Public License
**   along with this program; if not, write to the Free Software
**   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307, USA.
**
**
** EFE 6 Sep 2005
*/

#define COMMAND_MAIN	// turn on space allocation in commando.h

#include <stdio.h>
#include <unistd.h>
#include <math.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <time.h>
#include <sys/time.h>
#include <fcntl.h>
#include <curses.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include <signal.h>
#include <errno.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <readline/readline.h>
#include <readline/history.h>

#include "debug.h"
#include "dcbt.h"
#include "dcb.h"
#include "gsdc.h"
#include "gslib.h"
#include "dcblib.h"
#include "detsys.h"
#include "time_log.h"

#undef COMMAND_MAIN

#define True 1
#define False 0



#ifdef DEBUG	// implement debugger - global
FILE *dbgofp;   // global output file
int dbglvl;     // debugging control level
#endif


/******************************************************************************\
**                                                                            **
**          Protytpes of functions defined in this module                     **
**                                                                            **
\******************************************************************************/
static int command_interpreter(char *);
static int command_dispatcher(COMMAND_CMND, char *);
static void menu();
static int menuPrint_helper(char **, char **);


/******************************************************************************\
**                                                                            **
**          Global variables                                                  **
**                                                                            **
\******************************************************************************/
static char buf[0x1000000];
TYPEOF_PIX theImage[IMAGE_SIZE];
char *theData;
static int notmapped=1;
static char cmd_txt[20];
double exposure_time;		// needed by xor_tab.c
#define MODSIZE ((NROW_CHIP * NCOL_CHIP * NBITS * NCHIP)/8)
static int mapsize=(NBANK_DCB * NMOD_BANK * MODSIZE);
static char *dmabuf[GS_MAX_DEVICES];
char camstat_path[80] = "./";
char cam_image_path[80] = "./";
int n_bad_pix;


/******************************************************************************\
**                                                                            **
**          Local functions                                                   **
**                                                                            **
\******************************************************************************/

static void ldcb_bcb_reset(int gsdevno)
{
DBG(3, "--- dcb_bcb_reset()\n")

// clear the PMC gigastar(s)
gs_set_control(gsdevno, GS_RES_RX_FIFO);
gs_clear_irq(gsdevno);

# if MULTI_MODULE_DETECTOR

// reset the BCBs before the DCB
dcb_write_register(gsdevno, BCB_RESET_REG, BCB_RESET_ALL);
printf("clearing BCB\n");
#endif

// reset DCB, but not dcb gigstar (causes link error)
dcb_write_register(gsdevno, DCB_RESET_REG,
	/*	DCB_TX_FIFO_RESET |		may not work at 50 MHz */
	/*	DCB_RX_FIFO_RESET |		may not work at 50 MHz */
		DCB_INT_FIFO_RESET |
		DCB_CONVERT_RESET |
		DCB_TRIM_FIFO_RESET |
		DCB_FRAME_COUNT_RESET
		);

return;
}



static char *ldcb_read_banks(int gsdevno)
{
DBG(3, "--- dcb_read_banks()\n")

// reset DCB fifo
//dcb_write_register(gsdevno, DCB_RESET_REG, DCB_INT_FIFO_RESET);

// clear the gigastar
//gs_set_control(gsdevno, GS_RES_RX_FIFO);
//gs_clear_irq(gsdevno);

// set the DMA WR timeout  -- 0x1000 * 250 us = 1.024 s
(void)gs_set_rx_timeout(gsdevno, 0x1000);

// set interrupt on RX dma (i.e., dma write) finished & errors
gs_set_irq_mask(gsdevno,
				GS_RX_LINK_ERROR_INT |
				GS_TX_LINK_ERROR_INT |
				GS_DMA_WR_DONE_INT |
				GS_DMA_WR_DIED_INT |
				GS_DMA_WR_STOP_INT |
				GS_DMA_WR_ABORT_INT |
		/*		GS_DMA_RD_DONE_INT |  */
				GS_DMA_RD_OVFL_INT |
				GS_DMA_RD_STOP_INT |
				GS_DMA_RD_ABORT_INT
				);

DBG(3, "--- set up DMA for 0x%x dwords, device %d\n", mapsize/4, gsdevno)

// map pages for RX
if ( (dmabuf[gsdevno]=gs_rx_dma_setup(gsdevno, mapsize))==NULL )
	{
	printf("*** dcb_read_banks(), device %d: could not map pages\n", gsdevno);
	return NULL;
	}

// enable interrupt using the mask set above
gs_set_control(gsdevno, GS_INT_ENABLE);

// enable dma write to host
gs_set_control(gsdevno, GS_FIRE_DMA_WRITE);
DBG(3, "--- fire DMA write (E): %s\n", gs_timestamp())
	
dcb_enable_image_tx(gsdevno);				// enable TX to bcb

// enable the readout
dcb_write_command(gsdevno, DCB_READOUT_MODULES, 0);
dcb_write_command_no_ack(gsdevno, DCB_TRIGGER_AUTO_FIFO_READ);

return dmabuf[gsdevno];
}



static int ldcb_rx_data_dma(int gsdevno)
{
int n;
unsigned int t;

DBG(3, "--- dcb_rx_data_dma() (A): %s\n", gs_timestamp())

// wait for interrupt
n = gs_rx_dma(gsdevno);

DBG(3, "--- dcb_rx_data_dma() (B): %s\n", gs_timestamp())

// capture status
t=gs_irq_src_status(gsdevno);

// clear the interrupt
gs_clear_irq(gsdevno);

// error messages - the most probable error is DMA timeout, but printing
// the error harms performance
if ( (t&GS_DMA_WR_DIED_INT) )		// dma timeout
	{
	printf("***  ERROR: DMA write timeout DCB %d: received 0x%x dwords ***\n", gsdevno, n/4);
	DBG(4, "***  ERROR: DMA write timeout DCB %d: received 0x%x dwords ***\n", gsdevno, n/4)
	}
else								// more serious errors
	{
	if (n != mapsize )
		printf("*** ERROR: DCB %d read returned 0x%x dwords ***\n", gsdevno, n/4);
	if ( !(t & GS_DMA_WR_DONE_INT) )
		{
		printf("***  DCB %d DMA write was NOT successful; gs irq status = 0x%x\n", gsdevno, t);
		DBG(1, "***  DCB %d DMA write was NOT successful; gs irq status = 0x%x\n", gsdevno, t)
		}
	}

dcb_image_fifo_disable(gsdevno);		// EFE: SHOULD THIS BE HERE?? (e.g., if per-module)
return 0;
}


/******************************************************************************\
**                                                                            **
**          Command dispatcher                                                **
**                                                                            **
\******************************************************************************/

static int command_dispatcher(COMMAND_CMND cmnd, char *ptr)
{
unsigned int udword, ui[32], *uip, t, i2c_command;
int i, j, l, n, gsdevno=0, modsize, ofd, count, flag;
int bank, mod, bus, data, nexp, timeout;
char *p=NULL, *dmabuf=NULL, line[150], line2[150], byte;
double ddata;
unsigned short *usp;
double ts, tt, tp;
double exp_time, fr_time;
//unsigned short tmpim[5820*20];


	DBG(8, "User argument: (%s) %s\n", cmd_txt, ptr)

		// execute the operator's request
		// ptr gives the argument list (case-sensitive)
		
/*****************************************************************************\
**                                                                           **
**       Command Dispatch List                                               **
**                                                                           **
**       Enter new commands here as new 'case's.                             ** 
**       'mkheader' automagically takes care of the details.                 **
**                                                                           **
\*****************************************************************************/

	/* start of main loop switch - please preserve this line for mkheader */
	switch (cmnd)
		{
// --------------- DMA timeout test -------------------------------------
		case DMA_time:
			// clear the gigastar
			gs_set_control(gsdevno, GS_RES_RX_FIFO);
			gs_clear_irq(gsdevno);

			// set the DMA WR timeout  -- 0x1000 * 250 us = 1.024 s
			(void)gs_set_rx_timeout(gsdevno, 0x1000);

			// set interrupt on RX dma (i.e., dma write) finished & errors
			gs_set_irq_mask(gsdevno,
							GS_RX_LINK_ERROR_INT |
							GS_TX_LINK_ERROR_INT |
							GS_DMA_WR_DONE_INT |
							GS_DMA_WR_DIED_INT |
							GS_DMA_WR_STOP_INT |
							GS_DMA_WR_ABORT_INT |
					/*		GS_DMA_RD_DONE_INT |  */
							GS_DMA_RD_OVFL_INT |
							GS_DMA_RD_STOP_INT |
							GS_DMA_RD_ABORT_INT
							);

			DBG(3, "--- set up DMA for 0x%x dwords, device %d\n", mapsize/4, gsdevno)

			// enable interrupt using the mask set above
			gs_set_control(gsdevno, GS_INT_ENABLE);

			// enable dma write to host
			gs_set_control(gsdevno, GS_FIRE_DMA_WRITE);
			DBG(3, "--- fire DMA write (E): %s\n", gs_timestamp())
			ts = gs_time();

			// capture status
			t=gs_irq_src_status(gsdevno);
			while( !(t & GS_DMA_WR_DIED_INT))
				{
				t=gs_irq_src_status(gsdevno);
				usleep(1);
				}

			tt = gs_time() - ts;
			printf("Measured time = %.6lf\n", tt);
			printf("Clock period = %.6lf\n", tt/0x1000);
			break;

// --------------- 12 bank tests ----------------------------------------
		case TT1:
			gs_last_interrupt_time(0);	// syslog mark
			gs_last_interrupt_time(1);	// syslog mark
			dcb_set_bank_module_address(15, 7);

			ldcb_bcb_reset(0);
			gs_last_interrupt_time(0);	// syslog mark
			ldcb_read_banks(0);
			gs_last_interrupt_time(0);	// syslog mark
			ldcb_rx_data_dma(0);
			gs_last_interrupt_time(0);	// syslog mark

			ldcb_bcb_reset(1);
			gs_last_interrupt_time(1);	// syslog mark
			ldcb_read_banks(1);
			gs_last_interrupt_time(1);
			ldcb_rx_data_dma(1);
			gs_last_interrupt_time(1);
			gs_last_interrupt_time(0);	// syslog mark
			break;

		case TT2:		// error unless wait
			data = 0x9d367;
			dcb_write_command(0, DCB_WRITE_DATA, data);
			dcb_write_command(1, DCB_WRITE_DATA, data);
			break;

		case TT3:		// error unless wait
			data = 0x9d367;
			dcb_write_command(0, DCB_WRITE_DATA, data);
			dcb_write_command(0, DCB_WRITE_DATA, data);
			break;

		case TT4:		// OK
			data = 100;
			dcb_write_command(0, DCB_N_CALIBRATE, data);
			dcb_write_command(1, DCB_N_CALIBRATE, data);
			break;

		case TT5:		// OK
			data = 5;
			dcb_write_command(0, DCB_TRIM_ALL_VAL, (MAX_TRIM_VALUE & data) );
			dcb_write_command(1, DCB_TRIM_ALL_VAL, (MAX_TRIM_VALUE & data) );
			break;

		case Stop:
			dcb_stop();
			if (dcb_reset_exposure_mode())
				printf("dcb_reset_exposure_mode() returned an error\n");
			break;

		case TT6:
			dcb_reset_exposure_mode();
			dcb_set_register_bits(1, DCB_CTRL_REG, DCB_EXPOSURE_CONTROL);
			dcb_image_fifo_enable(1);
			dcb_image_fifo_disable(1);
			dcb_reset_exposure_mode();
			dcb_stop();
			break;


// --------------- RX link test -----------------------------------------
		case Link_test:
			{
			double tli, et;

			printf("GigaSTaR link integrity test\n");
			for(gsdevno=0; gsdevno<gs_device_count(); gsdevno++)
				{
				gs_set_control(gsdevno, GS_RES_RX_FIFO);
				gs_clear_irq(gsdevno);
				dcb_write_command(gsdevno, DCB_STOP, 0);

				// reset DCB, but not dcb gigstar (causes link error)
				dcb_write_register(gsdevno, DCB_RESET_REG,
					/*	DCB_TX_FIFO_RESET |		may not work at 50 MHz */
					/*	DCB_RX_FIFO_RESET |		may not work at 50 MHz */
						DCB_INT_FIFO_RESET |
						DCB_SEQUENCE_GEN_RESET |
						DCB_CONVERT_RESET |
						DCB_TRIM_FIFO_RESET |
						DCB_FRAME_COUNT_RESET |
						DCB_BOARD_COMM_RESET |
						DCB_SHT_SOFT_RESET |
						DCB_AUTO_FIFO_READ_RESET
						);

				# if MULTI_MODULE_DETECTOR

				dcb_set_bank_module_address(15, 7);		// select all
				dcb_write_register(gsdevno, BCB_RESET_REG, BCB_RESET_ALL);

				#endif

				gs_set_irq_mask(gsdevno,
								GS_RX_LINK_ERROR_INT |
								GS_TX_LINK_ERROR_INT |
								GS_DMA_WR_DONE_INT |
								GS_DMA_WR_DIED_INT |
								GS_DMA_WR_STOP_INT |
								GS_DMA_WR_ABORT_INT |
						/*		GS_DMA_RD_DONE_INT |  */
								GS_DMA_RD_OVFL_INT |
								GS_DMA_RD_STOP_INT |
								GS_DMA_RD_ABORT_INT
								);

				gs_set_control(gsdevno, GS_INT_ENABLE);
				printf("Device %d interrupt enabled: %s\n", gsdevno, timestamp());
				}

			gs_set_t0(0.0);		// synchronize clocks
			et=0.0;				// elapsed time
			printf("%6d", (int)et);
			fflush(stdout);
			while(1)
				{
				tli = gs_last_interrupt_time(0);
				if (tli>0.0)
					{
					printf("\n");
					fflush(stdout);
					et+=tli;
					printf("### ERROR: Hardware glitch - unexpected interrupt posted\n");
					printf("    Elapsed time: %.3lf\n", et);
					for(gsdevno=0; gsdevno<gs_device_count(); gsdevno++)
						printf("*** Device %d: gs irq status = 0x%x\n", gsdevno,
								gs_irq_src_status(gsdevno));
					for(gsdevno=0; gsdevno<gs_device_count(); gsdevno++)
						{
						gs_clear_irq(gsdevno);
						gs_set_control(gsdevno, GS_INT_ENABLE);
						printf("Device %d interrupt re-enabled: %s\n", gsdevno, timestamp());
						}
					printf("%6d", (int)et);
					fflush(stdout);
					gs_set_t0(0.0);		// synchronize clocks
					}
				sleep(1);
				printf("\b\b\b\b\b\b%6d", (int)(et+gs_time()));
				fflush(stdout);
				}
			}
			break;

// ----------------------------------------------------------------------

		case Reset:				// issue a reset for dcb on gs0
			i=gs_reset_all(gsdevno);
			if(i)
				printf("Bad retrun from gs_reset_all() = %d\n", i);
			i=gs_start(gsdevno);
			if(i)
				printf("Bad retrun from gs_start() = %d\n", i);
			gs_clear_irq(gsdevno);

#if 0
			// first, stop image mode, else you cannot write to dcb
			udword = 0;
			udword |= DCB_STOP;
			gs_tx_data(gsdevno, &udword, 4);
			printf("STOP - transmitted 0x%x\n", udword);
			for(i=0;i<999999;i++) n++;	// a little delay
			uip=(unsigned int*)buf;
			*uip=0;
			n=gs_rx_data(gsdevno, buf, 4);
			printf("STOP - received %d bytes: 0x%x\n", n, *uip);
#endif

			// make up reset instruction word
			udword = 0;
			udword |= DCB_WRITE;
			udword |= DCB_RESET_REG;
			udword |= DCB_RESET_ALL;

udword |= 0xffff;	// hit 'em all

			gs_tx_data(gsdevno, &udword, 4);
			printf("Transmitted 0x%x\n", udword);
			for(i=0;i<999999;i++) n++;	// a little delay
			uip=(unsigned int*)buf;
			*uip=0;
			n=gs_rx_data(gsdevno, buf, 4);
			printf("Received %d bytes: 0x%x\n", n, *uip);
			break;

		case GSreset:			// reset the pmc gigastar card
			if(gs_reset(gsdevno))
				printf("Bad retrun from gs_reset()\n");
			gs_start(gsdevno);
			gs_clear_irq(gsdevno);
			// clear the RX fifo by reading
			for(i=0;i<999999;i++) n++;	// a little delay
			n=0;
			uip=gs_read_bar0_reg(gsdevno, 4*GS_RX_FIFO_SIDEBITS, 4);
			l=0xffff&uip[0];
			while(l)
				{
//printf("uip[0] = 0x%x\n", uip[0]);
				n+=gs_rx_data(gsdevno, buf, l*4);
				for(i=0;i<999999;i++) n++;	// a little delay
				uip=gs_read_bar0_reg(gsdevno, 4*GS_RX_FIFO_SIDEBITS, 4);
				l=0xffff&uip[0];
				}
			if(n)
				printf("Read %d dwords\n", n/4);
			break;

		case sGSreset:				// small gigastar reset - fio & irq
			gs_set_control(gsdevno, GS_RES_RX_FIFO);
			gs_clear_irq(gsdevno);
			break;

		case Version:			// read the version of dcb
			gsdevno=0;
			sscanf(ptr, "%i", &gsdevno);
			udword = 0;
			udword |= DCB_READ;
			udword |= DCB_VERSION_REG;
			gs_tx_data(gsdevno, &udword, 4);
			printf("Transmitted 0x%x to device %d\n", udword, gsdevno);
			for(i=0;i<999999;i++) n++;	// a little delay
			uip=(unsigned int*)buf;
			*uip=0;
			n=gs_rx_data(gsdevno, buf, 4);
			printf("Received %d bytes: 0x%x\n", n, *uip);
			break;

		// read until no data are received
		case Runout:
			gsdevno=0;
			sscanf(ptr, "%i", &gsdevno);
			printf("Runout dcb %d\n", gsdevno);
			uip=(unsigned int*)buf;
			*uip=0;
			count=0;
			flag=0;
			n=gs_rx_data(gsdevno, buf, 4);
			if (n)
				udword=*uip;
			while(n)
				{
				count++;
				n=gs_rx_data(gsdevno, buf, 4);
				if (*uip != udword)
					flag++;
				}
			if(count)
				{
				printf("Count was %d\n", count);
				if (flag)
					printf("First datum: 0x%x; last: 0x%x\n", udword, *uip);
				else
					printf("First datum: 0x%x; all the same\n", udword);
				}
			break;

/* portable runout
{int n,count=0,gsdevno=1;char buf[4];
n=gs_rx_data(gsdevno, buf, 4);
while(n)
	{
	count++;
	n=gs_rx_data(gsdevno, buf, 4);
	}
DBG(4, "[][] Count was %d\n", count)
}
*/


#define NINTS 10
// send integers & read them back - requires dcb in repeater mode (sw 1)
		case Test11:
			for(i=0;i<NINTS;i++)
				ui[i]=i+10;
			gs_tx_data(gsdevno, ui, NINTS*4);
			for(i=0;i<NINTS;i++)
				ui[i]=0;
			n=gs_rx_data(gsdevno, ui, NINTS*4);
			printf("Received %d bytes\n", n);
			for(i=0;i<NINTS;i++)
				printf("%d   %d\n", i, ui[i]);
			break;
#undef NINTS

		case ReadControl:		// read the dcb control register
			udword = 0;
			udword |= DCB_READ;
			udword |= DCB_CTRL_REG;
			gs_tx_data(gsdevno, &udword, 4);
			printf("Transmitted 0x%x\n", udword);
			for(i=0;i<999999;i++) n++;	// a little delay
			uip=(unsigned int*)buf;
			*uip=0;
			n=gs_rx_data(gsdevno, buf, 4);
			printf("Received %d bytes: 0x%x\n", n, *uip);
			break;

		case ReadBCBControl:
			udword = 0;
			udword |= DCB_READ;
			udword |= BCB_CTRL_REG;
			gs_tx_data(gsdevno, &udword, 4);
			printf("Transmitted 0x%x\n", udword);
			for(i=0;i<999999;i++) n++;	// a little delay
			uip=(unsigned int*)buf;
			*uip=0;
			n=gs_rx_data(gsdevno, buf, 4);
			printf("Received %d bytes: 0x%x\n", n, *uip);
			break;

		// set HW delay in BCB
		case SetHWdelay:
			udword = 0;
			udword |= DCB_WRITE;
			udword |= BCB_CTRL_REG;
			udword |= 0x4;
			gs_tx_data(gsdevno, &udword, 4);
			printf("Transmitted 0x%x\n", udword);
			for(i=0;i<999999;i++) n++;	// a little delay
			uip=(unsigned int*)buf;
			*uip=0;
			n=gs_rx_data(gsdevno, buf, 4);
			printf("Received %d bytes: 0x%x\n", n, *uip);
			break;

		case WriteControl:		// write the dcb control register
			udword=0;
			sscanf(ptr, "%i", &udword);
			udword |= DCB_WRITE;
			udword |= DCB_CTRL_REG;
			gs_tx_data(gsdevno, &udword, 4);
			printf("Transmitted 0x%x\n", udword);
			for(i=0;i<999999;i++) n++;	// a little delay
			uip=(unsigned int*)buf;
			*uip=0;
			n=gs_rx_data(gsdevno, buf, 4);
			printf("Received %d bytes: 0x%x\n", n, *uip);
			break;

		case Status:			// read the dcb status register
			udword = 0;
			udword |= DCB_READ;
			udword |= DCB_STATUS_REG;
			gs_tx_data(gsdevno, &udword, 4);
			printf("Transmitted 0x%x\n", udword);
			for(i=0;i<999999;i++) n++;	// a little delay
			uip=(unsigned int*)buf;
			*uip=0;
			n=gs_rx_data(gsdevno, buf, 4);
			printf("Received %d bytes: 0x%x\n", n, *uip);
			break;

		case FStatus:			// read the dcb fifo status register
			udword = 0;
			udword |= DCB_READ;
			udword |= DCB_FIFO_STATUS_REG;
			gs_tx_data(gsdevno, &udword, 4);
			printf("Transmitted 0x%x\n", udword);
			for(i=0;i<999999;i++) n++;	// a little delay
			uip=(unsigned int*)buf;
			*uip=0;
			n=gs_rx_data(gsdevno, buf, 4);
			printf("Received %d bytes: 0x%x\n", n, *uip);
			break;

		// get status words from PMC Gigastar
		case GSstatus:
			printf("gs status:      0x%x\n", gs_status(gsdevno));
			printf("irq int status:  0x%x\n", gs_irq_status(gsdevno));
			printf("irq src status:  0x%x\n", gs_irq_src_status(gsdevno));
			uip=gs_read_bar0_reg(gsdevno, 4*GS_RX_FIFO_SIDEBITS, 4);
			printf("gs RX-fifo fill level = 0x%x\n", 0xffff&uip[0]);
			break;


		case ReadLED:			// read the dcb led register
			udword = 0;
			udword |= DCB_READ;
			udword |= DCB_LED_REG;
			gs_tx_data(gsdevno, &udword, 4);
			printf("Transmitted 0x%x\n", udword);
			for(i=0;i<999999;i++) n++;	// a little delay
			uip=(unsigned int*)buf;
			*uip=0;
			n=gs_rx_data(gsdevno, buf, 4);
			printf("Received %d bytes: 0x%x\n", n, *uip);
			break;

		case ReadDS:			// read the dcb dip switch register
			udword = 0;
			udword |= DCB_READ;
			udword |= DCB_DIP_SW_REG;
			gs_tx_data(gsdevno, &udword, 4);
			printf("Transmitted 0x%x\n", udword);
			for(i=0;i<999999;i++) n++;	// a little delay
			uip=(unsigned int*)buf;
			*uip=0;
			n=gs_rx_data(gsdevno, buf, 4);
			printf("Received %d bytes: 0x%x\n", n, *uip);
			break;

#if 0		// dip switch enable register - no longer used
		case ReadDSE:			// read the dcb dip switch enable register
			udword = 0;
			udword |= DCB_READ;
			udword |= DCB_DIP_SW_EN_REG;
			gs_tx_data(gsdevno, &udword, 4);
			printf("Transmitted 0x%x\n", udword);
			for(i=0;i<999999;i++) n++;	// a little delay
			uip=(unsigned int*)buf;
			*uip=0;
			n=gs_rx_data(gsdevno, buf, 4);
			printf("Received %d bytes: 0x%x\n", n, *uip);
			break;

		case WriteDSE:		// write the dcb dip switch enable register
			udword=0;
			sscanf(ptr, "%i", &udword);
			udword |= DCB_WRITE;
			udword |= DCB_DIP_SW_EN_REG;
			gs_tx_data(gsdevno, &udword, 4);
			printf("Transmitted 0x%x\n", udword);
			for(i=0;i<999999;i++) n++;	// a little delay
			uip=(unsigned int*)buf;
			*uip=0;
			n=gs_rx_data(gsdevno, buf, 4);
			printf("Received %d bytes: 0x%x\n", n, *uip);
			break;
#endif

		case ReadDelay:			// read the dcb dip switch enable register
			udword = 0;
			udword |= DCB_READ;
			udword |= DCB_BCB_DELAY_REG;
			gs_tx_data(gsdevno, &udword, 4);
			printf("Transmitted 0x%x\n", udword);
			for(i=0;i<999999;i++) n++;	// a little delay
			uip=(unsigned int*)buf;
			*uip=0;
			n=gs_rx_data(gsdevno, buf, 4);
			printf("Received %d bytes: 0x%x\n", n, *uip);
			break;

		case WriteDelay:		// write the dcb dip switch enable register
			udword=0;
			sscanf(ptr, "%i", &udword);
			udword |= DCB_WRITE;
			udword |= DCB_BCB_DELAY_REG;
			gs_tx_data(gsdevno, &udword, 4);
			printf("Transmitted 0x%x\n", udword);
			for(i=0;i<999999;i++) n++;	// a little delay
			uip=(unsigned int*)buf;
			*uip=0;
			n=gs_rx_data(gsdevno, buf, 4);
			printf("Received %d bytes: 0x%x\n", n, *uip);
			break;

		case ReadHWR:			// read the dcb half word register
			udword = 0;
			udword |= DCB_READ;
			udword |= DCB_HALF_DWORD_REG;
			gs_tx_data(gsdevno, &udword, 4);
			printf("Transmitted 0x%x\n", udword);
			for(i=0;i<999999;i++) n++;	// a little delay
			uip=(unsigned int*)buf;
			*uip=0;
			n=gs_rx_data(gsdevno, buf, 4);
			printf("Received %d bytes: 0x%x\n", n, *uip);
			break;


		case ReadTR:			// read the dcb temp register
			udword = 0;
			udword |= DCB_READ;
			udword |= DCB_TEMPERATURE_REG;
			gs_tx_data(gsdevno, &udword, 4);
			printf("Transmitted 0x%x\n", udword);
			for(i=0;i<999999;i++) n++;	// a little delay
			uip=(unsigned int*)buf;
			*uip=0;
			n=gs_rx_data(gsdevno, buf, 4);
			printf("Received %d bytes: 0x%x\n", n, *uip);
			break;


		case ReadHR:			// read the dcb humidity register
			udword = 0;
			udword |= DCB_READ;
			udword |= DCB_HUMIDITY_REG;
			gs_tx_data(gsdevno, &udword, 4);
			printf("Transmitted 0x%x\n", udword);
			for(i=0;i<999999;i++) n++;	// a little delay
			uip=(unsigned int*)buf;
			*uip=0;
			n=gs_rx_data(gsdevno, buf, 4);
			printf("Received %d bytes: 0x%x\n", n, *uip);
			break;


		// fill all pixels with a value
		case Fill:
			data=0x0f0f0;
			gsdevno=0;
			sscanf(ptr, "%i%i", &data, &gsdevno);
			printf("Filling device %d with 0x%x\n", gsdevno, data);
			dcb_write_command(gsdevno, DCB_WRITE_DATA, data);
			break;


		// readout module
		case Rdout:
			modsize=(NROW_CHIP * NCOL_CHIP * NBITS * NCHIP)/8;		// bytes

			printf("modsize is 0x%x dwords\n", modsize/4);

		// extend the DMA WR timeout
			(void)gs_set_rx_timeout(gsdevno, 0x1000);

		// set interrupt on RX dma (i.e., dma write) finished & errors
			gs_set_irq_mask(gsdevno,
							GS_RX_LINK_ERROR_INT |
							GS_TX_LINK_ERROR_INT |
							GS_DMA_WR_DONE_INT |
							GS_DMA_WR_DIED_INT |
							GS_DMA_WR_STOP_INT |
							GS_DMA_WR_ABORT_INT |
					/*		GS_DMA_RD_DONE_INT |  */
							GS_DMA_RD_OVFL_INT |
							GS_DMA_RD_STOP_INT |
							GS_DMA_RD_ABORT_INT
							);

		// map npages for RX
			if ( (p=gs_rx_dma_setup(gsdevno, modsize))==NULL )
				{
				printf("Could not map pages (1)\n");
				break;
				}

			uip=gs_read_bar0_reg(gsdevno, 0, 32*4);
			printf("write buf size: 0x%x, read buf size: 0x%x\n",
					uip[21], uip[25]);

		// enable interrupt using the mask set above
			gs_set_control(gsdevno, GS_INT_ENABLE);

		// enable dma write to host
			gs_set_control(gsdevno, GS_FIRE_DMA_WRITE);

		// set the image TX enable flag in the dcb.
			dcb_enable_image_tx(0);


		if (MULTI_MODULE_DETECTOR)
			{
		// start a readout of all selected modules (just 1 now)
			dcb_write_command(gsdevno, DCB_READOUT_MODULES, 0);

		// read the module into the dcb
			t = DCB_TRIGGER_READSTROBE;
			gs_tx_data(gsdevno, &t, 4);
			printf("writing 0x%x\n", t);
			}
		else
			{
			// start a readout
				t = DCB_READOUT_MODULES;
				gs_tx_data(gsdevno, &t, 4);
			}



		// wait for interrupt
			n = gs_rx_dma(gsdevno);
			printf("read returned 0x%x bytes = 0x%x dwords\n", n, n/4);

		// look at status
			t=gs_irq_src_status(gsdevno);
			printf("irq status: 0x%x", t);
			if (t & GS_DMA_WR_DONE_INT)
				printf(":  DMA write was successful\n");
			else
				printf(":  DMA write was NOT successful\n");

		// clear the interrupt
			gs_clear_irq(gsdevno);
			
		// look at some values
			usp=(unsigned short *)p;
			for(i=0; i<50; i++)
				printf("%2d   0x%x\n", i, *(usp+i));

		// run out fifo
			count=0;
			n=gs_rx_data(gsdevno, buf, 4);
			while(n && count<100000)
				{
				count++;
				n=gs_rx_data(gsdevno, buf, 4);
				}
			if(count)
				printf("Unread dwords in FIFO: count was %d\n", count);

			break;


		// fill and read an image - like readback
		case Img:
//while(1){
			data=0x7fffe;
			sscanf(ptr, "%i", &data);

			// now with the dcb
			dcb_set_module_address(gsdevno, 4);
			dcb_set_bank_address(gsdevno, 2);

			modsize=(NROW_CHIP * NCOL_CHIP * NBITS * NCHIP)/8;		// bytes

			printf("modsize is 0x%x dwords\n", modsize/4);

		// extend the DMA WR timeout
			(void)gs_set_rx_timeout(gsdevno, 0x8000);

		// set interrupt on RX dma (i.e., dma write) finished & errors
			gs_set_irq_mask(gsdevno,
							GS_RX_LINK_ERROR_INT |
							GS_TX_LINK_ERROR_INT |
							GS_DMA_WR_DONE_INT |
							GS_DMA_WR_DIED_INT |
							GS_DMA_WR_STOP_INT |
							GS_DMA_WR_ABORT_INT |
					/*		GS_DMA_RD_DONE_INT |  */
							GS_DMA_RD_OVFL_INT |
							GS_DMA_RD_STOP_INT |
							GS_DMA_RD_ABORT_INT
							);

		// map npages for RX
			if ( (p=gs_rx_dma_setup(gsdevno, modsize))==NULL )
				{
				printf("Could not map pages (1)\n");
				break;
				}

#if 0
		// 2nd map npages for RX
			if ( (q=gs_rx_dma_setup(gsdevno, modsize))==NULL )
				{
				printf("Could not map pages (2)\n");
				break;
				}
#endif

			uip=gs_read_bar0_reg(gsdevno, 0, 32*4);
			printf("write buf size: 0x%x, read buf size: 0x%x\n",
					uip[21], uip[25]);

		// enable interrupt using the mask set above
			gs_set_control(gsdevno, GS_INT_ENABLE);

		// enable dma write to host
			gs_set_control(gsdevno, GS_FIRE_DMA_WRITE);

		// fill the pixels
			dcb_write_command(gsdevno, DCB_WRITE_DATA, data);

		// set the image TX enable flag in the dcb - move data to bcb.
			dcb_enable_image_tx(0);

		if (MULTI_MODULE_DETECTOR)
			{
		// start a readout of all selected modules (just 1 now)
			dcb_write_command(gsdevno, DCB_READOUT_MODULES, 0);

		// select 1 module at a time

		// check the control register
			udword = 0;
			udword |= DCB_READ;
			udword |= DCB_CTRL_REG;
			gs_tx_data(gsdevno, &udword, 4);
			printf("Transmitted 0x%x\n", udword);
			for(i=0;i<999999;i++) n++;	// a little delay
			uip=(unsigned int*)buf;
			*uip=0;
			n=gs_rx_data(gsdevno, buf, 4);
			printf("Received %d bytes: 0x%x\n", n, *uip);

		// read the module into the dcb
			t = DCB_TRIGGER_READSTROBE;
			gs_tx_data(gsdevno, &t, 4);
			printf("writing 0x%x\n", t);
			}
		else
			{
		// start a readout
			t = DCB_READOUT_MODULES;
			gs_tx_data(gsdevno, &t, 4);
			}

#if 0
			for(i=0;i<4;i++)
				{
				uip=gs_read_bar0_reg(gsdevno, 0, 32*4);
				printf("RX dwords: 0x%x, TX dwords: 0x%x, fifo fill: 0x%x, irq src: 0x%x\n",
						uip[28], uip[29], 0xffff&uip[10], uip[4]);
				sleep(1);
				}
#endif
		
#if 1
		// wait for interrupt
			n = gs_rx_dma(gsdevno);
			printf("read returned 0x%x bytes = 0x%x dwords\n", n, n/4);
#endif

		// look at status
			t=gs_irq_src_status(gsdevno);
			printf("irq status: 0x%x", t);
			if (t & GS_DMA_WR_DONE_INT)
				printf(":  DMA write was successful\n");
			else
				printf(":  DMA write was NOT successful\n");

		// clear the interrupt
			gs_clear_irq(gsdevno);
			
		// look at some values
			usp=(unsigned short *)p;
			for(i=0; i<30; i++)
				printf("%2d   0x%x\n", i, *(usp+i));

		// run out fifo
			count=0;
			n=gs_rx_data(gsdevno, buf, 4);
			while(n)
				{
				count++;
				n=gs_rx_data(gsdevno, buf, 4);
				}
			if(count)
				printf("Unread dwords in FIFO: count was %d\n", count);
//usleep(1000);}
			break;


		//  integrated image fill & read - arg is data to write
		case I2:
			data=0xfffff;
			sscanf(ptr, "%i", &data);

			modsize=(NROW_CHIP * NCOL_CHIP * NBITS * NCHIP)/8;		// bytes
			printf("modsize is 0x%x dwords\n", modsize/4);

		// super clear the gigstar
			i=gs_reset_all(gsdevno);
			if(i)
				printf("Bad return from gs_reset_all() = %d\n", i);
			i=gs_start(gsdevno);
			if(i)
				printf("Bad return from gs_start() = %d\n", i);
			gs_clear_irq(gsdevno);
			notmapped=1;

		// stop transparent mode
			udword = 0;
			udword |= DCB_STOP;
			gs_tx_data(gsdevno, &udword, 4);
			for(i=0;i<9999;i++) n++;	// a little delay
			uip=(unsigned int*)buf;
			*uip=0;
			n=gs_rx_data(gsdevno, buf, 4);
			printf("--- received %d bytes from DCB_STOP\n", n);	// 0 is normal
			
		// reset DCB, but not dcb gigstar (causes link error)
		// make up reset instruction word
			udword = 0;
			udword |= DCB_WRITE;
			udword |= DCB_RESET_REG;
			udword |= DCB_TX_FIFO_RESET;
			udword |= DCB_RX_FIFO_RESET;
			udword |= DCB_INT_FIFO_RESET;
			udword |= DCB_CONVERT_RESET;
			gs_tx_data(gsdevno, &udword, 4);
			for(i=0;i<9999;i++) n++;	// a little delay
			uip=(unsigned int*)buf;
			*uip=0;
			n=gs_rx_data(gsdevno, buf, 4);
			printf("--- received %d bytes from DCB_RESET\n", n);	// 4 is normal

		// check the DCB fifo status
			udword = 0;
			udword |= DCB_READ;
			udword |= DCB_FIFO_STATUS_REG;
			gs_tx_data(gsdevno, &udword, 4);
			for(i=0;i<9999;i++) n++;	// a little delay
			uip=(unsigned int*)buf;
			*uip=0;
			n=gs_rx_data(gsdevno, buf, 4);
			printf("--- received %d bytes from DCB_READ_FIFO_STATUS\n", n);	// 4 is normal
			if ( !(uip[0]&DCB_INT_FIFO_EMPTY) )
				printf("*** DCB internal fifo not empty\n");
			
		// clear the gigastar
			gs_set_control(gsdevno, GS_RES_RX_FIFO);
			gs_clear_irq(gsdevno);

		// extend the DMA WR timeout  -- 0x1000 * 250 us = 1.024 s
			(void)gs_set_rx_timeout(gsdevno, 0x1000);

//while(1)
//{
		// set interrupt on RX dma (i.e., dma write) finished & errors
			gs_set_irq_mask(gsdevno,
							GS_RX_LINK_ERROR_INT |
							GS_TX_LINK_ERROR_INT |
							GS_DMA_WR_DONE_INT |
							GS_DMA_WR_DIED_INT |
							GS_DMA_WR_STOP_INT |
							GS_DMA_WR_ABORT_INT |
					/*		GS_DMA_RD_DONE_INT |  */
							GS_DMA_RD_OVFL_INT |
							GS_DMA_RD_STOP_INT |
							GS_DMA_RD_ABORT_INT
							);

		// map npages for RX
			if (notmapped)
				{
				if ( (dmabuf=gs_rx_dma_setup(gsdevno, modsize))==NULL )
					{
					printf("Could not map pages (1)\n");
					break;
					}
				notmapped=0;
				}
			else
				gs_write_bar0_reg(gsdevno, 4*GS_DMA_WR_BUF_SIZE, modsize/4);
			p=dmabuf;

		// write out some numbers
			printf("Data buffer before image:\n");
			uip = (unsigned int *)p;
			for(i=0;i<4;i++)
				{
				for(j=0;j<8;j++)
					printf("0x%x  ", uip[8*i+j]);
				printf("\n");
				}

		// enable interrupt using the mask set above
			gs_set_control(gsdevno, GS_INT_ENABLE);

		// enable dma write to host
			gs_set_control(gsdevno, GS_FIRE_DMA_WRITE);

		// set the image TX enable flag in the dcb.
			dcb_enable_image_tx(0);

		// fill the pixels
			printf("Filling pixels with data pattern = 0x%x\n", data);
			dcb_write_command(gsdevno, DCB_WRITE_DATA, data);

	//		data=10;
	//		dcb_write_command(gsdevno, DCB_N_CALIBRATE, data);

	//		data=0x0f0f0;
	//		dcb_write_command(gsdevno, DCB_TRIM_ALL_VAL, data);

		// trigger a readout
			t = DCB_READOUT_MODULES;
			gs_tx_data(gsdevno,  &t, 4);

		// wait for interrupt
			n = gs_rx_dma(gsdevno);
			printf("read returned 0x%x bytes = 0x%x dwords\n", n, n/4);
			uip=gs_read_bar0_reg(gsdevno, 0, 32*4);
			printf("RX dwords: 0x%x, TX dwords: 0x%x, fifo fill: 0x%x, irq src: 0x%x\n",
					uip[28], uip[29], 0xffff&uip[10], uip[4]);
			
		// look at status
			t=gs_irq_src_status(gsdevno);
			printf("irq status: 0x%x", t);
			if (t & GS_DMA_WR_DONE_INT)
				printf(":  DMA write was successful\n");
			else
				printf(":  DMA write was NOT successful\n");

		// clear the interrupt
			gs_clear_irq(gsdevno);

		// run out the fifo
			count=0;
			n=gs_rx_data(gsdevno, buf, 4);
			while(n && count<100000)
				{
				count++;
				n=gs_rx_data(gsdevno, buf, 4);
				}
			if(count)
				printf("Unread dwords in FIFO: count was %d\n", count);

//}

		// write out some results
			printf("Data buffer after image:\n");
			usp = (unsigned short *)p;
			for(i=0;i<30;i++)
				printf("%2d   0x%x\n", i, usp[i]);

			memset(line, 0, sizeof(line));
			strcpy(line,
	"---------------------------------------------------------------");
			memset(line2, 0, sizeof(line2));
			strcpy(line2,
	"                                                               ");
			byte=0;
			for(i=0,j=2;i<40;i++)
				{
				if(i>0 && !(i%4))
					{line2[j-2] = byte+'0';
					if(byte>9)
						line2[j-2]+=7;
					byte=0;
					}
				if(i>0 && !(i%4))
					line[j++]=' ';
				if(i>0 && !(i%20))
					line[j++]=' ';
				byte<<=1;
				if (usp[i])
					{line[j++]='1';
					byte |= 1;
					}
				else
					line[j++]='0';
				}
			line2[j-2] = byte+'0';
			if(byte>9)
				line2[j-2]+=7;
			printf("%s\n", line);
			printf("%s\n", line2);
			count=0;
			for(j=0;j<5820;j++)
				{flag=0;
				for(i=0;i<20;i++)
					if(usp[20*j+i] != usp[i])
						flag++;
				if (flag)
					count++;
				}
			printf("--- %d pixels did NOT match the first pixel\n", count);
			printf(
	"--------------------------------------------------------------------\n");

		// convert the image & write it
			memset(theImage, 0x0, sizeof(theImage));
			convert_image(dmabuf);
			if ( (ofd=open("/tmp/theimage.img",O_WRONLY | O_CREAT | O_TRUNC,
							S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH)) == -1  )
				{
				printf("Could not open image file for writiing\n");
				break;
				}
			write(ofd, theImage, IMAGE_NBYTES);
			close(ofd);

		// print some pixels
			printf("Pixels from image - different chips:\n");
			for (i=0; i<8; i++)
				{
				printf("%3d:   ", 61*i);
				for (j=0; j<8; j++)
					printf("0x%x  ", theImage[61*i+j]);
				printf("\n");
				}
			printf(
	"--------------------------------------------------------------------\n");
			printf(
	"----------------- End ----------------------------------------------\n");

			break;



		// test writing registers
		case RegT:
			printf("....Exposure time:\n");
			dcb_set_exposure_time(0.0);
			printf("Exposure time set to: %lf\n", 
					dcb_get_exposure_time());
			dcb_set_exposure_time(3.14159);
			printf("Exposure time set to: %lf\n", 
					dcb_get_exposure_time());

			printf("....Exposure period\n");
			dcb_set_exposure_period(0.0);
			printf("Exposure period set to: %lf\n", 
					dcb_get_exposure_period());
			dcb_set_exposure_period(0.4321);
			printf("Exposure period set to: %lf\n", 
					dcb_get_exposure_period());

			printf("....TX frame limit\n");
			dcb_set_tx_frame_limit(0);
			printf("TX frame limit set to:%d\n",
					dcb_get_tx_frame_limit());
			dcb_set_tx_frame_limit(19);
			printf("TX frame limit set to:%d\n",
					dcb_get_tx_frame_limit());
			// exercise the exposure time register - up to 1e7 sec!
			tt=1.0e-7;
			for(i=0; i<15; i++)
				{
				dcb_set_exposure_time(tt);
				ts = dcb_get_exposure_time();
				if (fabs(ts - tt) > 40e-9)
					{
					printf("Error: t set = %.8lf, t read = %.8lf\n", tt, ts);
					break;
					}
				tt*=10.0;
				}
				// exercise the exposure period register
			tt=1.0e-7;
			for(i=0; i<15; i++)
				{
				dcb_set_exposure_period(tt);
				ts = dcb_get_exposure_period();
				if (fabs(ts - tt) > 40e-9)
					{
					printf("Error: t set = %.8lf, t read = %.8lf\n", tt, ts);
					break;
					}
				tt*=10.0;
				}
		break;

		// expose only - no readout - just fill the chips
		case ExpT:
			ddata=1.0;
			sscanf(ptr, "%lf", &ddata);
			dcb_set_exposure_time(ddata);
			dcb_set_exposure_period(ddata+0.1);
			dcb_set_exposure_count_limit(1);
			dcb_set_tx_frame_limit(1);
			gsdevno=0;
		// start the exposure
		// there is no acknowledge from the expose command
			udword = DCB_EXPOSE;
			gs_tx_data(gsdevno, &udword, 4);
			break;

		// make an exposure
		case Exposure:
			ddata=0.01;
			sscanf(ptr, "%lf", &ddata);
			dcb_set_exposure_time(ddata);
			dcb_set_exposure_count_limit(1);
			dcb_set_tx_frame_limit(1);
			gsdevno=0;

			modsize=(NROW_CHIP * NCOL_CHIP * NBITS * NCHIP)/8;		// bytes
			printf("modsize is 0x%x dwords\n", modsize/4);

		// super clear the gigstar
			i=gs_reset_all(gsdevno);
			if(i)
				printf("Bad return from gs_reset_all() = %d\n", i);
			i=gs_start(gsdevno);
			if(i)
				printf("Bad return from gs_start() = %d\n", i);
			gs_clear_irq(gsdevno);
			notmapped=1;

		// stop transparent mode
			udword = 0;
			udword |= DCB_STOP;
			gs_tx_data(gsdevno, &udword, 4);
			for(i=0;i<9999;i++) n++;	// a little delay
			uip=(unsigned int*)buf;
			*uip=0;
			n=gs_rx_data(gsdevno, buf, 4);
			printf("--- received %d bytes from DCB_STOP\n", n);	// 0 is normal
			
		// reset DCB, but not dcb gigstar (causes link error)
		// make up reset instruction word
			udword = 0;
			udword |= DCB_WRITE;
			udword |= DCB_RESET_REG;
			udword |= DCB_TX_FIFO_RESET;
			udword |= DCB_RX_FIFO_RESET;
			udword |= DCB_INT_FIFO_RESET;
			udword |= DCB_CONVERT_RESET;
			gs_tx_data(gsdevno, &udword, 4);
			for(i=0;i<9999;i++) n++;	// a little delay
			uip=(unsigned int*)buf;
			*uip=0;
			n=gs_rx_data(gsdevno, buf, 4);
			printf("--- received %d bytes from DCB_RESET\n", n);	// 4 is normal

		// check the DCB fifo status
			udword = 0;
			udword |= DCB_READ;
			udword |= DCB_FIFO_STATUS_REG;
			gs_tx_data(gsdevno, &udword, 4);
			for(i=0;i<9999;i++) n++;	// a little delay
			uip=(unsigned int*)buf;
			*uip=0;
			n=gs_rx_data(gsdevno, buf, 4);
			printf("--- received %d bytes from DCB_READ_FIFO_STATUS\n", n);	// 4 is normal
			if ( !(uip[0]&DCB_INT_FIFO_EMPTY) )
				printf("*** DCB internal fifo not empty\n");
			
		// clear the gigastar
			gs_set_control(gsdevno, GS_RES_RX_FIFO);
			gs_clear_irq(gsdevno);

		// extend the DMA WR timeout  -- 0x1000 * 250 us = 1.024 s
			(void)gs_set_rx_timeout(gsdevno, 0x8000);

		// set interrupt on RX dma (i.e., dma write) finished & errors
			gs_set_irq_mask(gsdevno,
							GS_RX_LINK_ERROR_INT |
							GS_TX_LINK_ERROR_INT |
							GS_DMA_WR_DONE_INT |
							GS_DMA_WR_DIED_INT |
							GS_DMA_WR_STOP_INT |
							GS_DMA_WR_ABORT_INT |
					/*		GS_DMA_RD_DONE_INT |  */
							GS_DMA_RD_OVFL_INT |
							GS_DMA_RD_STOP_INT |
							GS_DMA_RD_ABORT_INT
							);

		// map npages for RX
			if (notmapped)
				{
				if ( (dmabuf=gs_rx_dma_setup(gsdevno, modsize))==NULL )
					{
					printf("Could not map pages (1)\n");
					break;
					}
				notmapped=0;
				}
			else
				gs_write_bar0_reg(gsdevno, 4*GS_DMA_WR_BUF_SIZE, modsize/4);
			p=dmabuf;

		// enable interrupt using the mask set above
			gs_set_control(gsdevno, GS_INT_ENABLE);

		// enable dma write to host
			gs_set_control(gsdevno, GS_FIRE_DMA_WRITE);

		// set the image TX enable flag in the dcb.
			dcb_enable_image_tx(0);

		// start the exposure
		// there is no acknowledge from the exspose command
			udword = DCB_EXPOSE;
			gs_tx_data(gsdevno, &udword, 4);

//---------------------

		// wait for interrupt
			n = gs_rx_dma(gsdevno);
			printf("read returned 0x%x bytes = 0x%x dwords\n", n, n/4);
			uip=gs_read_bar0_reg(gsdevno, 0, 32*4);
			printf("RX dwords: 0x%x, TX dwords: 0x%x, fifo fill: 0x%x, irq src: 0x%x\n",
					uip[28], uip[29], 0xffff&uip[10], uip[4]);
			
		// look at status
			t=gs_irq_src_status(gsdevno);
			printf("irq status: 0x%x", t);
			if (t & GS_DMA_WR_DONE_INT)
				printf(":  DMA write was successful\n");
			else
				printf(":  DMA write was NOT successful\n");

		// clear the interrupt
			gs_clear_irq(gsdevno);

		// write out some results
			printf("Data buffer after image:\n");
			usp = (unsigned short *)p;
			for(i=0;i<30;i++)
				printf("%2d   0x%x\n", i, usp[i]);

		// run out fifo
			count=0;
			n=gs_rx_data(gsdevno, buf, 4);
			while(n && count<100000)
				{
				count++;
				n=gs_rx_data(gsdevno, buf, 4);
				}
			if(count)
				printf("Unread dwords in FIFO: count was %d\n", count);

			memset(line, 0, sizeof(line));
			strcpy(line,
	"---------------------------------------------------------------");
			memset(line2, 0, sizeof(line2));
			strcpy(line2,
	"                                                               ");
			byte=0;
			for(i=0,j=2;i<40;i++)
				{
				if(i>0 && !(i%4))
					{line2[j-2] = byte+'0';
					if(byte>9)
						line2[j-2]+=7;
					byte=0;
					}
				if(i>0 && !(i%4))
					line[j++]=' ';
				if(i>0 && !(i%20))
					line[j++]=' ';
				byte<<=1;
				if (usp[i])
					{line[j++]='1';
					byte |= 1;
					}
				else
					line[j++]='0';
				}
			line2[j-2] = byte+'0';
			if(byte>9)
				line2[j-2]+=7;
			printf("%s\n", line);
			printf("%s\n", line2);
			count=0;
			for(j=0;j<5820;j++)
				{flag=0;
				for(i=0;i<20;i++)
					if(usp[20*j+i] != usp[i])
						flag++;
				if (flag)
					count++;
				}
			printf("--- %d pixels did NOT match the first pixel\n", count);
			printf(
	"--------------------------------------------------------------------\n");

		// convert the image & write it
			memset(theImage, 0x0, sizeof(theImage));
			convert_image(dmabuf);
			if ( (ofd=open("/tmp/theimage.img",O_WRONLY | O_CREAT | O_TRUNC,
							S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH)) == -1  )
				{
				printf("Could not open image file for writiing\n");
				break;
				}
			write(ofd, theImage, IMAGE_NBYTES);
			close(ofd);
		// print some pixels
			printf("Pixels from image - different chips:\n");
			for (i=0; i<8; i++)
				{
				printf("%3d:   ", 61*i);
				for (j=0; j<8; j++)
					printf("0x%x  ", theImage[61*i+j]);
				printf("\n");
				}
			printf(
	"--------------------------------------------------------------------\n");
			printf(
	"----------------- End ----------------------------------------------\n");

			break;

		// test bank and module address 
		case Adds:
			printf("....Bank address\n");
			dcb_set_bank_address(gsdevno, 0);
			printf("Bank address set to: %d\n",
					dcb_get_bank_address(gsdevno));
			dcb_set_bank_address(gsdevno, 1);
			printf("Bank address set to: %d\n",
					dcb_get_bank_address(gsdevno));

			printf("....Module address\n");
			dcb_set_module_address(gsdevno, 0);
			printf("Module address set to: %d\n",
					dcb_get_module_address(gsdevno));
			dcb_set_module_address(gsdevno, 1);
			printf("Module address set to: %d\n",
					dcb_get_module_address(gsdevno));
			break;

		// select bank and module
		case SelBM:
			mod = 1;
			bank = 1;
			sscanf(ptr, "%i%i", &bank, &mod);
			dcb_set_module_address(gsdevno, mod);
			dcb_set_bank_address(gsdevno, bank);
			break;

		// read the dCB version
		case DVersion:
			gsdevno=0;
			sscanf(ptr, "%i", &gsdevno);
			udword = dcb_read_register(gsdevno, DCB_VERSION_REG);
			printf("Device %d version: 0x%x\n", gsdevno, udword);
			udword = dcb_read_register(gsdevno, DCB_FIRMWARE_BUILD_REG);
			printf("Device %d build: 0x%x\n", gsdevno, udword);
			break;

		// read the BCB version
		case BVersion:
			gsdevno=0;
			bank=1;
			sscanf(ptr, "%i%i", &gsdevno, &bank);
			dcb_set_bank_address(gsdevno, bank);
			udword = dcb_read_register(gsdevno, BCB_VERSION_REG);
			printf("Device %d Bank %d version: 0x%x\n", gsdevno, bank, udword);
			udword = dcb_read_register(gsdevno, BCB_FIRMWARE_BUILD_REG);
			printf("Device %d Bank %d build: 0x%x\n", gsdevno, bank, udword);
			break;

		// read the frame count register
		case FrameCount:
			printf("TX frame count is: %d\n",
					dcb_get_tx_frame_count());
			break;

		// write to the BCB reset register
		case BCBreset:
			data = 0x0;
			sscanf(ptr, "%i", &data);
			dcb_write_register(0, BCB_RESET_REG, data);
			break;

		// activate one of the loops
		case Loop:
		// select a pixel
			dcb_reset_row_col(gsdevno);
			dcb_insert_row_col_token(gsdevno);
			
			for(i=0; i<1; i++)				// advance row token
				dcb_advance_row_token(gsdevno);
			for(i=0; i<1; i++)				// advance column token
				dcb_advance_col_token(gsdevno);

	//		data=10;
	//		t = DCB_TRIGGER_DCAL | data;	// activate calibrate loop
	//		gs_tx_data(gsdevno,  &t, 4);

			data=0xc0;
			t = DCB_DIN_VALUE | data;		// activate din loop
			gs_tx_data(gsdevno,  &t, 4);

			break;

		case Trim:
			data=0x0;
			sscanf(ptr, "%i", &data);

			printf("*** FIXME ***\n");
			break;

		case TrmAll:
			data=0x0;
			sscanf(ptr, "%i", &data);

			dcb_trim_allpix(data);
			break;

		// fill with calibrate pulses
		case Cfill:
			data=5;
			sscanf(ptr, "%i", &data);

			dcb_write_command(gsdevno, DCB_N_CALIBRATE, data);
			break;

		// give calibrate to a single pixel & readout
		case T20:
			dcb_reset_row_col(0);
			dcb_insert_row_col_token(0);
			dcb_trigger_dcal(0, 7);
			dcb_apply_din_value(0, 0xfffff);
			break;

		// does expose properly wait?
		case T21:
			dcb_set_exposure_time(2.0);
			dcb_set_exposure_period(2.1);
			dcb_set_exposure_count_limit(1);	// 1 exposure per frame
			dcb_set_tx_frame_limit(1);	// 1 frame
			ts = gs_time();
			dcb_expose();
usleep(1);
			dcb_enable_image_tx(0);
tt=0.1;
while(tt>0.01)
{
t = dcb_get_remaining_time();
printf("Remaining time is %lf\n", tt);
usleep(100000);
dcb_enable_image_tx(0);
}
			dcb_wait_read_image(&theData);
			printf("elapsed time %lf\n", gs_time()-ts);
			break;

		// does stop reset tx_enable?
		case T22:
			t = dcb_read_register(0, DCB_CTRL_REG);
			printf("control register is: 0x%x\n", t);
			dcb_enable_image_tx(0);
			printf("control register is: 0x%x\n", t);
			dcb_stop();
			printf("control register is: 0x%x\n", t);
			break;

		// measured readout time
		// NOTE!!!: turn off dcb_enable_image_tx(0) in dcblib.c
		// note2: do a previous read to set up dma
		// found 7.32 ms for 25 MHz design
		case T23:
			dcb_set_exposure_time(0.001);
			dcb_set_exposure_period(1.0);
			dcb_set_exposure_count_limit(1);	// 1 exposure per frame
			dcb_set_tx_frame_limit(1);	// 1 frame
			time_log(-1, NULL);		// start the time logger
			for(i=0;i<50;i++)
				{
				dcb_expose();
				usleep(1000); 		// ~2 ms - exposure completes
				time_log(1,NULL);	// start new line
				dcb_enable_image_tx(0);		// enable readout
				gs_rx_dma(0);		// wait for readout
				time_log(0,NULL);
				gs_clear_irq(0);
				}
			time_log(2, NULL);
			break;

		// are we getting the interrupts properly from a multi-exposure?
		case T24:
		// --- clear the dcb & gigastar, but not too much
		// stop dcb image mode & reset tx enable
			dcb_stop();

#if 0
		// reset DCB, but not dcb gigstar (causes link error)
			dcb_write_register(gsdevno, DCB_RESET_REG,
					DCB_TX_FIFO_RESET |
					DCB_RX_FIFO_RESET |
					DCB_INT_FIFO_RESET |
					DCB_CONVERT_RESET
					);

		// clear the gigastar
			gs_set_control(gsdevno, GS_RES_RX_FIFO);
			gs_clear_irq(gsdevno);
#endif

		// --- set up the exposure parameters
			j=5;
			dcb_set_exposure_time(0.5);
			dcb_set_exposure_period(1.0);
			dcb_set_exposure_count_limit(1);	// 1 exposure per frame
			dcb_set_tx_frame_limit(j);	// j frames
			printf("exposure time set to %lf\n", dcb_get_exposure_time());
			printf("exposure period set to %lf\n", dcb_get_exposure_period());
			printf("number of frames set to %d\n", dcb_get_tx_frame_limit());
			
			modsize=(NROW_CHIP * NCOL_CHIP * NBITS * NCHIP)/8;		// bytes

		// set the DMA WR timeout
			gs_set_rx_timeout(gsdevno, 0x2000);		// ~2 sec

		// --- prepare for the first exposure
		// set interrupt on RX dma (i.e., dma write) finished & errors
			gs_set_irq_mask(gsdevno,
							GS_RX_LINK_ERROR_INT |
							GS_TX_LINK_ERROR_INT |
							GS_DMA_WR_DONE_INT |
							GS_DMA_WR_DIED_INT |
							GS_DMA_WR_STOP_INT |
							GS_DMA_WR_ABORT_INT |
					/*		GS_DMA_RD_DONE_INT |  */
							GS_DMA_RD_OVFL_INT |
							GS_DMA_RD_STOP_INT |
							GS_DMA_RD_ABORT_INT
							);

		// map npages for RX
			dmabuf=gs_rx_dma_setup(0, modsize);

		// enable interrupt using the mask set above
			gs_set_control(gsdevno, GS_INT_ENABLE);

		// enable dma & image read
			gs_set_control(gsdevno, GS_FIRE_DMA_WRITE);
			dcb_enable_image_tx(0);

		// --- start the multi-exposure
			udword = DCB_EXPOSE;
			gs_tx_data(gsdevno, &udword, 4);
			printf("start expose at %s\n", gs_timestamp());
			ts = gs_time();

		// wait for interrupt
			n = gs_rx_dma(gsdevno);
			if (n != modsize )
				printf("*** ERROR: read returned 0x%x dwords ***\n", n/4);

		// look at status
			t=gs_irq_src_status(gsdevno);
			if ( !(t & GS_DMA_WR_DONE_INT) )
				printf("***  DMA write was NOT successful\n");

		// clear the interrupt
			gs_clear_irq(gsdevno);

			printf("interrupt received at %s: delta T = %lf\n",
					gs_timestamp(), gs_time()-ts);

		// --- loop - we just wait for interrupts
			while(--j)
				{
				// set interrupt on RX dma (i.e., dma write) finished & errors
				gs_set_irq_mask(gsdevno,
								GS_RX_LINK_ERROR_INT |
								GS_TX_LINK_ERROR_INT |
								GS_DMA_WR_DONE_INT |
								GS_DMA_WR_DIED_INT |
								GS_DMA_WR_STOP_INT |
								GS_DMA_WR_ABORT_INT |
						/*		GS_DMA_RD_DONE_INT |  */
								GS_DMA_RD_OVFL_INT |
								GS_DMA_RD_STOP_INT |
								GS_DMA_RD_ABORT_INT
								);

			// map npages for RX
				dmabuf=gs_rx_dma_setup(0, modsize);

			// enable interrupt using the mask set above
				gs_set_control(gsdevno, GS_INT_ENABLE);

			// enable dma & image read
				gs_set_control(gsdevno, GS_FIRE_DMA_WRITE);
				dcb_enable_image_tx(0);

			// wait for interrupt
				n = gs_rx_dma(gsdevno);
				if (n != modsize )
					printf("*** ERROR: read returned 0x%x dwords ***\n", n/4);

			// look at status
				t=gs_irq_src_status(gsdevno);
				if ( !(t & GS_DMA_WR_DONE_INT) )
					printf("***  DMA write was NOT successful\n");

			// clear the interrupt
				gs_clear_irq(gsdevno);

				printf("interrupt received at %s: delta T = %lf\n",
						gs_timestamp(), gs_time()-ts);
				}

			printf("total number of frames transmitted %d\n", dcb_get_tx_frame_count());
			break;

		// can I read out during a timed exposure?  Needed for >1 module
		// but, this should be illegal until we get the BCB
		case T25:
		// stop dcb image mode & reset tx enable
			dcb_stop();

		// --- set up the exposure parameters
			dcb_set_exposure_time(3.0);
			dcb_set_exposure_period(4.0);
			dcb_set_exposure_count_limit(1);	// 1 exposure per frame
			dcb_set_tx_frame_limit(0);
			printf("exposure time set to %lf\n", dcb_get_exposure_time());
			printf("exposure period set to %lf\n", dcb_get_exposure_period());
			printf("number of frames set to %d\n", dcb_get_tx_frame_limit());
			
			modsize=(NROW_CHIP * NCOL_CHIP * NBITS * NCHIP)/8;		// bytes

		// set the DMA WR timeout
			gs_set_rx_timeout(gsdevno, 0x5000);		// ~2 sec

		// --- start the exposure
			udword = DCB_EXPOSE;
			gs_tx_data(gsdevno, &udword, 4);
			printf("start expose at %s\n", gs_timestamp());
			ts = gs_time();

		// --- prepare for the first readout
		// set interrupt on RX dma (i.e., dma write) finished & errors
			gs_set_irq_mask(gsdevno,
							GS_RX_LINK_ERROR_INT |
							GS_TX_LINK_ERROR_INT |
							GS_DMA_WR_DONE_INT |
							GS_DMA_WR_DIED_INT |
							GS_DMA_WR_STOP_INT |
							GS_DMA_WR_ABORT_INT |
					/*		GS_DMA_RD_DONE_INT |  */
							GS_DMA_RD_OVFL_INT |
							GS_DMA_RD_STOP_INT |
							GS_DMA_RD_ABORT_INT
							);

		// map npages for RX
			dmabuf=gs_rx_dma_setup(0, modsize);
			usp=(unsigned short *)dmabuf;

		// enable interrupt using the mask set above
			gs_set_control(gsdevno, GS_INT_ENABLE);

		// enable dma & image read
			gs_set_control(gsdevno, GS_FIRE_DMA_WRITE);
			dcb_enable_image_tx(0);

//#if 0
// ==== sit back & wait
		// wait 1 sec, then do a readout
			sleep(1);

		// start a readout
			t = DCB_READOUT_MODULES;
			gs_tx_data(gsdevno, &t, 4);

		// wait for interrupt
			n = gs_rx_dma(gsdevno);
			printf("interrupt received at %s: delta T = %lf\n",
					gs_timestamp(), gs_time()-ts);
			if (n != modsize )
				printf("*** ERROR: read returned 0x%x dwords ***\n", n/4);

		// reading kills ENB - put it back
			dcb_trigger_enable(0);

		// look at status
			t=gs_irq_src_status(gsdevno);
			printf("irq status: 0x%x", t);
			if (t & GS_DMA_WR_DONE_INT)
				printf(":  DMA write was successful\n");
			else
				printf(":  DMA write was NOT successful\n");

		// clear the interrupt
			gs_clear_irq(gsdevno);
			
		// convert the image & write it
			memset(theImage, 0x0, sizeof(theImage));
			convert_image(dmabuf);
			strcpy(line, "/tmp/theimage0.img");
			if ( (ofd=open(line,O_WRONLY | O_CREAT | O_TRUNC,
							S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH)) == -1  )
				{
				printf("Could not open image file for writiing\n");
				break;
				}
			write(ofd, theImage, IMAGE_NBYTES);
			close(ofd);
			printf("image %s was written\n", line);

		// set the DMA WR timeout
			gs_set_rx_timeout(gsdevno, 0x5000);		// ~5 sec

		// --- prepare for the exposure readout
		// set interrupt on RX dma (i.e., dma write) finished & errors
			gs_set_irq_mask(gsdevno,
							GS_RX_LINK_ERROR_INT |
							GS_TX_LINK_ERROR_INT |
							GS_DMA_WR_DONE_INT |
							GS_DMA_WR_DIED_INT |
							GS_DMA_WR_STOP_INT |
							GS_DMA_WR_ABORT_INT |
					/*		GS_DMA_RD_DONE_INT |  */
							GS_DMA_RD_OVFL_INT |
							GS_DMA_RD_STOP_INT |
							GS_DMA_RD_ABORT_INT
							);

		// map npages for RX
			dmabuf=gs_rx_dma_setup(0, modsize);
			usp=(unsigned short *)dmabuf;

		// enable interrupt using the mask set above
			gs_set_control(gsdevno, GS_INT_ENABLE);

		// enable dma & image read
			gs_set_control(gsdevno, GS_FIRE_DMA_WRITE);
			dcb_enable_image_tx(0);
//#endif
// ================

		// wait for interrupt
			n = gs_rx_dma(gsdevno);
			if (n != modsize )
				printf("*** ERROR: read returned 0x%x dwords ***\n", n/4);

		// look at status
			t=gs_irq_src_status(gsdevno);
			printf("irq status: 0x%x", t);
			if (t & GS_DMA_WR_DONE_INT)
				printf(":  DMA write was successful\n");
			else
				printf(":  DMA write was NOT successful\n");

		// clear the interrupt
			gs_clear_irq(gsdevno);

			printf("interrupt received at %s: delta T = %lf\n",
					gs_timestamp(), gs_time()-ts);

		// stop counting x-rays
			dcb_stop();

		// convert the image & write it
			memset(theImage, 0x0, sizeof(theImage));
			convert_image(dmabuf);
			strcpy(line, "/tmp/theimage1.img");
			if ( (ofd=open(line,O_WRONLY | O_CREAT | O_TRUNC,
							S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH)) == -1  )
				{
				printf("Could not open image file for writiing\n");
				break;
				}
			write(ofd, theImage, IMAGE_NBYTES);
			close(ofd);
			printf("image %s was written\n", line);

			break;

		// is time remaining register OK on multi-exposures?
		case T27:
		// --- clear the dcb & gigastar, but not too much
		// stop dcb image mode & reset tx enable
			dcb_stop();

#if 0
		// reset DCB, but not dcb gigstar (causes link error)
			dcb_write_register(gsdevno, DCB_RESET_REG,
					DCB_TX_FIFO_RESET |
					DCB_RX_FIFO_RESET |
					DCB_INT_FIFO_RESET |
					DCB_CONVERT_RESET
					);

		// clear the gigastar
			gs_set_control(gsdevno, GS_RES_RX_FIFO);
			gs_clear_irq(gsdevno);
#endif

		// --- set up the exposure parameters
			j=5;
			dcb_set_exposure_time(5.0);
			dcb_set_exposure_period(6.0);
			dcb_set_exposure_count_limit(1);	// 1 exposure per frame
			dcb_set_tx_frame_limit(j);	// j frames
			printf("exposure time set to %lf\n", dcb_get_exposure_time());
			printf("exposure period set to %lf\n", dcb_get_exposure_period());
			printf("number of frames set to %d\n", dcb_get_tx_frame_limit());
			
			modsize=(NROW_CHIP * NCOL_CHIP * NBITS * NCHIP)/8;		// bytes

		// set the DMA WR timeout
			gs_set_rx_timeout(gsdevno, 0x8000);		// ~8 sec

		// --- prepare for the first exposure
		// set interrupt on RX dma (i.e., dma write) finished & errors
			gs_set_irq_mask(gsdevno,
							GS_RX_LINK_ERROR_INT |
							GS_TX_LINK_ERROR_INT |
							GS_DMA_WR_DONE_INT |
							GS_DMA_WR_DIED_INT |
							GS_DMA_WR_STOP_INT |
							GS_DMA_WR_ABORT_INT |
					/*		GS_DMA_RD_DONE_INT |  */
							GS_DMA_RD_OVFL_INT |
							GS_DMA_RD_STOP_INT |
							GS_DMA_RD_ABORT_INT
							);

		// map npages for RX
			dmabuf=gs_rx_dma_setup(0, modsize);
			usp=(unsigned short *)dmabuf;

		// enable interrupt using the mask set above
			gs_set_control(gsdevno, GS_INT_ENABLE);

		// enable dma & image read
			gs_set_control(gsdevno, GS_FIRE_DMA_WRITE);
			dcb_enable_image_tx(0);

		// --- start the multi-exposure
			udword = DCB_EXPOSE;
			gs_tx_data(gsdevno, &udword, 4);
			printf("start expose at %s\n", gs_timestamp());
			ts = gs_time();

		// print some times
			tt = dcb_get_remaining_time();
			printf("time remaining is %lf\n", tt);
			sleep(1);
			while(tt>1.5)		// this one works
				{
				tt = dcb_get_remaining_time();
				printf("time remaining is %lf\n", tt);
				sleep(1);
				}

		// wait for interrupt
			n = gs_rx_dma(gsdevno);
			if (n != modsize )
				printf("*** ERROR: read returned 0x%x dwords ***\n", n/4);

		// look at status
			t=gs_irq_src_status(gsdevno);
			if ( !(t & GS_DMA_WR_DONE_INT) )
				printf("***  DMA write was NOT successful\n");

		// clear the interrupt
			gs_clear_irq(gsdevno);

			printf("interrupt received at %s: delta T = %lf\n",
					gs_timestamp(), gs_time()-ts);

		// convert the image & write it
			memset(theImage, 0x0, sizeof(theImage));
			convert_image(dmabuf);
			sprintf(line, "/tmp/theimage%d.img", 5-j);
			if ( (ofd=open(line,O_WRONLY | O_CREAT | O_TRUNC,
							S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH)) == -1  )
				{
				printf("Could not open image file for writiing\n");
				break;
				}
			write(ofd, theImage, IMAGE_NBYTES);
			close(ofd);
			printf("image %s was written\n", line);

		// --- loop - we just wait for interrupts
			while(--j)
				{
				// set interrupt on RX dma (i.e., dma write) finished & errors
				gs_set_irq_mask(gsdevno,
								GS_RX_LINK_ERROR_INT |
								GS_TX_LINK_ERROR_INT |
								GS_DMA_WR_DONE_INT |
								GS_DMA_WR_DIED_INT |
								GS_DMA_WR_STOP_INT |
								GS_DMA_WR_ABORT_INT |
						/*		GS_DMA_RD_DONE_INT |  */
								GS_DMA_RD_OVFL_INT |
								GS_DMA_RD_STOP_INT |
								GS_DMA_RD_ABORT_INT
								);

			// map npages for RX
				dmabuf=gs_rx_dma_setup(0, modsize);
				usp=(unsigned short *)dmabuf;

			// enable interrupt using the mask set above
				gs_set_control(gsdevno, GS_INT_ENABLE);

			// enable dma & image read
				gs_set_control(gsdevno, GS_FIRE_DMA_WRITE);
				dcb_enable_image_tx(0);

			// print some times
			// the observed 975 ms of zero is the time from the end
			// of one exposure in the frame, and the beginning of the next
				tp = gs_time();
				tt = dcb_get_remaining_time();
				printf("time remaining is %lf; delta T = %lf\n",
						tt, gs_time()-tp);
				for(i=0;i<100000 && tt<0.1;i++)
					{
					tt = dcb_get_remaining_time();
					usleep(1000);	// ~2 ms
					}
				printf("i = %d, time remaining is %lf; dT = %lf\n",
						i, tt, gs_time()-tp);
				sleep(1);
				if (tt<0.1)		// when immediately zero
					{
					for (i=0; i<3; i++)
						{
						tt = dcb_get_remaining_time();
						printf("time remaining is %lf\n", tt);
						sleep(1);
						}
					}
				else
					while(tt>1.5)
						{
						tt = dcb_get_remaining_time();
						printf("time remaining is %lf\n", tt);
						sleep(1);
						}

			// wait for interrupt
				n = gs_rx_dma(gsdevno);
				if (n != modsize )
					printf("*** ERROR: read returned 0x%x dwords ***\n", n/4);

			// look at status
				t=gs_irq_src_status(gsdevno);
				if ( !(t & GS_DMA_WR_DONE_INT) )
					printf("***  DMA write was NOT successful\n");

			// clear the interrupt
				gs_clear_irq(gsdevno);

				printf("interrupt received at %s: delta T = %lf\n",
						gs_timestamp(), gs_time()-ts);

			// convert the image & write it
				memset(theImage, 0x0, sizeof(theImage));
				convert_image(dmabuf);
				sprintf(line, "/tmp/theimage%d.img", 5-j);
				if ( (ofd=open(line,O_WRONLY | O_CREAT | O_TRUNC,
								S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH)) == -1  )
					{
					printf("Could not open image file for writiing\n");
					break;
					}
				write(ofd, theImage, IMAGE_NBYTES);
				close(ofd);
				printf("image %s was written\n", line);
				}

			printf("total number of frames transmitted %d\n", dcb_get_tx_frame_count());

			break;

		// is frame_count updated even when dma does not happen?  No.
		case T28:
			printf("initial frame count: %d\n", dcb_get_tx_frame_count());
			dcb_tx_frame_count_reset(0);
			printf("after reset:         %d\n", dcb_get_tx_frame_count());

			dcb_set_exposure_time(0.05);
			dcb_set_exposure_period(0.1);
			dcb_set_exposure_count_limit(1);	// 1 exposure per frame
			dcb_set_tx_frame_limit(1);	// 1 frame
			dcb_expose();
			sleep(1);
			printf("after expose:         %d\n", dcb_get_tx_frame_count());
			udword = DCB_EXPOSE;
			gs_tx_data(0, &udword, 4);
			sleep(1);
			printf("after expose:         %d\n", dcb_get_tx_frame_count());
			gs_tx_data(0, &udword, 4);
			sleep(1);
			printf("after expose:         %d\n", dcb_get_tx_frame_count());
			break;

		// test last interrupt time
		case T29:
			printf("%.6lf\n", gs_last_interrupt_time(0));
			break;

		case Exp2:
			ddata=0.01;
			sscanf(ptr, "%lf", &ddata);
			
			dcb_set_exposure_time(ddata);
			dcb_set_exposure_period(1.0+ddata);
			dcb_expose();
			dcb_wait_read_image(&theData);
			convert_image(theData);
			
			if ( (ofd=open("/tmp/theimage.img",O_WRONLY | O_CREAT | O_TRUNC,
							S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH)) == -1  )
				{
				printf("Could not open image file for writiing\n");
				break;
				}
			write(ofd, theImage, IMAGE_NBYTES);
			close(ofd);
			printf("/tmp/theimage.img was written\n");
			break;

		// just read an image
		case Rd2:
			dcb_start_read_image();
			dcb_wait_read_image(&theData);
			convert_image(theData);

			if ( (ofd=open("/tmp/theimage.img",O_WRONLY | O_CREAT | O_TRUNC,
							S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH)) == -1  )
				{
				printf("Could not open image file for writiing\n");
				break;
				}
			write(ofd, theImage, IMAGE_NBYTES);
			close(ofd);
			printf("/tmp/theimage.img was written\n");
			break;

		// readout detector using AUTO fifo read
		// only for firmware supported whole detector read
		case Rd3:
			data=0x7fffe;		// default pattern
			sscanf(ptr, "%i", &data);

		// select all modules
			dcb_set_bank_module_address(15, 7);		// select all

		// reset the BCBs before the DCB
			dcb_write_register(gsdevno, BCB_RESET_REG, BCB_RESET_ALL);

		// reset DCB, but not dcb gigstar (causes link error)
			dcb_write_register(gsdevno, DCB_RESET_REG,
					DCB_TX_FIFO_RESET |
					DCB_RX_FIFO_RESET |
					DCB_INT_FIFO_RESET |
					DCB_CONVERT_RESET |
					DCB_TRIM_FIFO_RESET |
					DCB_FRAME_COUNT_RESET
					);

		// set the format registers - working with 2 bank setup
			dcb_set_bank_enable_pattern(0, 0x3);		// 0...11
			dcb_set_module_enable_pattern(0, 0x1f);		// 0...11111

		// fill the pixels
			printf("Filling pixels with data pattern = 0x%x\n", data);
			dcb_write_command(gsdevno, DCB_WRITE_DATA, data);

		// enable TX to bcb
			dcb_enable_image_tx(0);

		// readout modules to BCBs
			dcb_write_command(0, DCB_READOUT_MODULES, 0);

		// set up interrupt
			gs_set_control(0, GS_RES_RX_FIFO);
			gs_clear_irq(0);
			(void)gs_set_rx_timeout(0, 0x1000);
			gs_set_irq_mask(0,
							GS_RX_LINK_ERROR_INT |
							GS_TX_LINK_ERROR_INT |
							GS_DMA_WR_DONE_INT |
							GS_DMA_WR_DIED_INT |
							GS_DMA_WR_STOP_INT |
							GS_DMA_WR_ABORT_INT |
					/*		GS_DMA_RD_DONE_INT |  */
							GS_DMA_RD_OVFL_INT |
							GS_DMA_RD_STOP_INT |
							GS_DMA_RD_ABORT_INT
							);

		// map pages for RX
			modsize=(NROW_CHIP * NCOL_CHIP * NBITS * NCHIP)/8;		// bytes
			if ( (dmabuf=gs_rx_dma_setup(0, NBANK*NMOD_BANK*modsize))==NULL )
				{
				printf("*** dcb_read_module(): could not map pages (1)\n");
				break;
				}

		// enable interrupt using the mask set above
			gs_set_control(0, GS_INT_ENABLE);

		// enable dma write to host
			gs_set_control(0, GS_FIRE_DMA_WRITE);

		// trigger the full detector readout
			dcb_write_command_no_ack(0, DCB_TRIGGER_AUTO_FIFO_READ);

		// wait for interrupt
			n = gs_rx_dma(0);

		// capture status
			t=gs_irq_src_status(0);

		// clear the interrupt
			gs_clear_irq(0);

		// error messages - the most probable error is DMA timeout, but printing
		// the error harms performance
			if ( (t&GS_DMA_WR_DIED_INT) )		// dma timeout
				printf("***  ERROR: DMA write timeout\n");
			if (n != NBANK*NMOD_BANK*modsize )
				printf("***  ERROR: read returned 0x%x dwords ***\n", n/4);
			else
				printf("Ok - read returned 0x%x dwords\n", n/4);
			if ( !(t & GS_DMA_WR_DONE_INT) )
				printf("***  DMA write was NOT successful; gs irq status = 0x%x\n", t);

			printf("Data buffer after image:\n");
			usp = (unsigned short *)dmabuf;
			for(i=0;i<30;i++)
				printf("%2d   0x%x\n", i, usp[i]);

		// convert the image
			memset(theImage, 0, IMAGE_NBYTES);

		// write the image to /tmp

			break;

		// readout out full detector on a per-module basis
		case Rd4:
			data=0x7fffe;		// default pattern
			sscanf(ptr, "%i", &data);

		// select all modules
			dcb_set_bank_module_address(15, 7);		// select all

		// reset the BCBs before the DCB
			dcb_write_register(gsdevno, BCB_RESET_REG, BCB_RESET_ALL);

		// reset DCB, but not dcb gigstar (causes link error)
			dcb_write_register(gsdevno, DCB_RESET_REG,
					DCB_TX_FIFO_RESET |
					DCB_RX_FIFO_RESET |
					DCB_INT_FIFO_RESET |
					DCB_CONVERT_RESET |
					DCB_TRIM_FIFO_RESET |
					DCB_FRAME_COUNT_RESET
					);

		// fill the pixels
			printf("Filling pixels with data pattern = 0x%x\n", data);
			dcb_write_command(gsdevno, DCB_WRITE_DATA, data);

		// enable TX to bcb
			dcb_enable_image_tx(0);

		// readout modules to BCBs
			dcb_write_command(0, DCB_READOUT_MODULES, 0);

		// set up interrupt
			gs_set_control(0, GS_RES_RX_FIFO);
			gs_clear_irq(0);
			(void)gs_set_rx_timeout(0, 0x1000);
			gs_set_irq_mask(0,
							GS_RX_LINK_ERROR_INT |
							GS_TX_LINK_ERROR_INT |
							GS_DMA_WR_DONE_INT |
							GS_DMA_WR_DIED_INT |
							GS_DMA_WR_STOP_INT |
							GS_DMA_WR_ABORT_INT |
					/*		GS_DMA_RD_DONE_INT |  */
							GS_DMA_RD_OVFL_INT |
							GS_DMA_RD_STOP_INT |
							GS_DMA_RD_ABORT_INT
							);

		// map pages for RX
			modsize=(NROW_CHIP * NCOL_CHIP * NBITS * NCHIP)/8;		// bytes
			if ( (dmabuf=gs_rx_dma_setup(0, modsize))==NULL )
				{
				printf("*** dcb_read_module(): could not map pages (1)\n");
				break;
				}

		// enable interrupt using the mask set above
			gs_set_control(0, GS_INT_ENABLE);

		// enable dma write to host
			gs_set_control(0, GS_FIRE_DMA_WRITE);

		// select module 1,1
			dcb_set_bank_module_address(1, 1);

		// trigger the first module read
			dcb_write_command_no_ack(0, DCB_TRIGGER_READSTROBE);

		// wait for interrupt
			n = gs_rx_dma(0);

		// capture status
			t=gs_irq_src_status(0);

		// clear the interrupt
			gs_clear_irq(0);

		// error messages - the most probable error is DMA timeout, but printing
		// the error harms performance
			if ( (t&GS_DMA_WR_DIED_INT) )		// dma timeout
				printf("***  ERROR: DMA write timeout\n");
			if (n != modsize )
				printf("***  ERROR: read returned 0x%x dwords ***\n", n/4);
			else
				printf("Ok - read returned 0x%x dwords\n", n/4);
			if ( !(t & GS_DMA_WR_DONE_INT) )
				printf("***  DMA write was NOT successful; gs irq status = 0x%x\n", t);

			printf("Data buffer after image:\n");
			usp = (unsigned short *)dmabuf;
			for(i=0;i<30;i++)
				printf("%2d   0x%x\n", i, usp[i]);

		// readout the other modules

		// convert the image
			memset(theImage, 0, IMAGE_NBYTES);
		
		// write the image to /tmp

			break;

		// expose, then readout as a single  block
		case Exp4:
			exp_time = 0.0001;		// exposure time
			fr_time = 0.1;			// exposure period = frame time
			nexp = 1;				// number of exposures

			gsdevno=0;

		// set exposure time
			dcb_set_exposure_time(exp_time);

		// set frame time
			fr_time = 1.0;
			dcb_set_exposure_period(fr_time);

		// set exposure limit
			dcb_set_exposure_count_limit(nexp);

		// set frame limit
			dcb_set_tx_frame_limit(1);

		// set bank and module format registers
			dcb_set_bank_enable_pattern(0, 0x3);		// 0...11
			dcb_set_module_enable_pattern(0, 0x1f);		// 0...11111

		// set the DMA write timeout
			timeout = (int)((1.0 + fr_time)/0.000250);
			gs_set_rx_timeout(gsdevno, timeout);
			printf("Setting timeout to: 0x%x\n", timeout);

// loop on nexp
for(j=0;j<nexp;j++)
{
		// select all modules
			dcb_set_bank_address(0, 15);
			dcb_set_module_address(0, 7);

		// reset pixel-side FPGAs (0xfd37....)
			dcb_write_register(gsdevno, BCB_RESET_REG, BCB_RESET_ALL);

		// reset DCB, but not dcb gigstar (causes link error)
			dcb_write_register(gsdevno, DCB_RESET_REG,
					DCB_TX_FIFO_RESET |
					DCB_RX_FIFO_RESET |
					DCB_INT_FIFO_RESET |
					DCB_CONVERT_RESET |
					DCB_TRIM_FIFO_RESET |
					DCB_FRAME_COUNT_RESET
					);

		// set up interrupt
			gs_set_control(0, GS_RES_RX_FIFO);
			gs_clear_irq(0);
			(void)gs_set_rx_timeout(0, 0x1000);
			gs_set_irq_mask(0,
							GS_RX_LINK_ERROR_INT |
							GS_TX_LINK_ERROR_INT |
							GS_DMA_WR_DONE_INT |
							GS_DMA_WR_DIED_INT |
							GS_DMA_WR_STOP_INT |
							GS_DMA_WR_ABORT_INT |
					/*		GS_DMA_RD_DONE_INT |  */
							GS_DMA_RD_OVFL_INT |
							GS_DMA_RD_STOP_INT |
							GS_DMA_RD_ABORT_INT
							);

		// map pages for RX
			modsize=(NROW_CHIP * NCOL_CHIP * NBITS * NCHIP)/8;		// bytes
			if ( (dmabuf=gs_rx_dma_setup(0, NBANK*NMOD_BANK*modsize))==NULL )
				{
				printf("*** dcb_read_module(): could not map pages (1)\n");
				break;
				}

		// enable interrupt using the mask set above
			gs_set_control(0, GS_INT_ENABLE);

		// select module 1, 1
	//		dcb_set_bank_address(0, 1);
	//		dcb_set_module_address(0, 1);

		// reset DCB image fifo and conversion registers
			dcb_write_register(gsdevno, DCB_RESET_REG, DCB_INT_FIFO_RESET);

		// enable dma write to host
			gs_set_control(0, GS_FIRE_DMA_WRITE);

		// trigger auto_fifo_read
			dcb_write_command_no_ack(0, DCB_TRIGGER_AUTO_FIFO_READ);

		// enable image tx to bcb
			dcb_enable_image_tx(0);

		// start exposure - once only
			if (j==0)
				dcb_write_command_no_ack(gsdevno, DCB_EXPOSE);

printf("Time before wait for int: %s\n", gs_timestamp());
		// wait for interrrupt
			n = gs_rx_dma(0);
printf("Time after wait for int: %s\n", gs_timestamp());

		// capture status
			t=gs_irq_src_status(0);

		// clear the interrupt
			gs_clear_irq(0);

		// error messages - the most probable error is DMA timeout, but printing
		// the error harms performance
			if ( (t&GS_DMA_WR_DIED_INT) )		// dma timeout
				printf("***  ERROR: DMA write timeout\n");
			if (n != NBANK*NMOD_BANK*modsize )
				printf("***  ERROR: read returned 0x%x dwords ***\n", n/4);
			else
				printf("Ok - read returned 0x%x dwords\n", n/4);
			if ( !(t & GS_DMA_WR_DONE_INT) )
				printf("***  DMA write was NOT successful; gs irq status = 0x%x\n", t);

		// convert image
		// write to disk


// end of loop on nexp
}

			break;

		// read the remaining time register
		case Timeleft:
			printf("%lf\n", dcb_get_remaining_time());
			break;


// ========== i2c stuff =================================================

		// Prog DAC:  pdac hexdata (24 bits)
		case PDAC:
			data=0;
			sscanf(ptr, "%i", &data);

			bank=1;
			mod=1;
			bus=2;		// 1 or 2 for dac1 or dac2

			if (bus==2)
				i2c_command = DCB_I2C_DAC2;		// dac2 channel
			else
				i2c_command = DCB_I2C_DAC1;		// dac1 channel

			// Always power up the chip, although this is really only
			// needed once.  But, we cannot tell if the dcb has been 
			// powered down and up since the last call.
			if (dcb_prog_i2c(bank, mod, DCB_I2C_DAC2, 0x78f03c))
				break;
			if (dcb_prog_i2c(bank, mod, DCB_I2C_DAC2, 0x7af03c))
				break;
			if (dcb_prog_i2c(bank, mod, DCB_I2C_DAC1, 0x78f03c))
				break;
			if (dcb_prog_i2c(bank, mod, DCB_I2C_DAC1, 0x7af03c))
				break;
			if (dcb_prog_i2c(bank, mod, DCB_I2C_DAC1, 0xb8f03c))
				break;
			if (dcb_prog_i2c(bank, mod, DCB_I2C_DAC1, 0xbaf03c))
				break;

			udword=data;
			if (dcb_prog_i2c(bank, mod, i2c_command, udword))
				break;

			break;


		// SetDACs - set all the dac voltages
		case SetDACs:
			bank=15;
			mod=7;

			// Always power up the chip, although this is really only
			// needed once.  But, we cannot tell if the dcb has been 
			// powered down and up since the last call.

			i2c_command = DCB_I2C_DAC2;
			// === wake up unit 0, DAC2 SDA
			udword = 0x78 <<16;
			udword |= 0xf0 <<8;
			udword |= 0x3c;
			if (dcb_prog_i2c(bank, mod, i2c_command, udword))
				break;

			// signal = B01_M01_VTRM, v = 1.999625, data = 0x338
			udword = 0x78 <<16;
			udword |= 0x0c <<8;
			udword |= 0xe0;
			if (dcb_prog_i2c(bank, mod, i2c_command, udword))
				break;

			// signal = B01_M01_VCCA, v = 0.500484, data = 0xcd
			udword = 0x78 <<16;
			udword |= 0x13 <<8;
			udword |= 0x34;
			if (dcb_prog_i2c(bank, mod, i2c_command, udword))
				break;

			// signal = B01_M01_VRFS, v = 0.699078, data = 0x11f
			udword = 0x78 <<16;
			udword |= 0x24 <<8;
			udword |= 0x7c;
			if (dcb_prog_i2c(bank, mod, i2c_command, udword))
				break;

			// signal = B01_M01_VRF, v = -0.199855, data = 0xd8
			udword = 0x78 <<16;
			udword |= 0x33 <<8;
			udword |= 0x60;
			if (dcb_prog_i2c(bank, mod, i2c_command, udword))
				break;

			// === wake up unit 1, DAC2 SDA
			udword = 0x7a <<16;
			udword |= 0xf0 <<8;
			udword |= 0x3c;
			if (dcb_prog_i2c(bank, mod, i2c_command, udword))
				break;

			// signal = B01_M01_VADJ, v = 1.199814, data = 0x345
			udword = 0x7a <<16;
			udword |= 0xd <<8;
			udword |= 0x14;
			if (dcb_prog_i2c(bank, mod, i2c_command, udword))
				break;

			// signal = B01_M01_VCAL, v = 0.299469, data = 0x7a (=> 0x21e8)
			// signal = B01_M01_VCAL, v = 0.500, data = 0xcd (=> 0x2334)
			// signal = B01_M01_VCAL, v = 0.700, data = 0x11f (=> 0x247c)
			udword = 0x7a <<16;
		//	udword |= 0x21e8;		// v = 0.3
			udword |= 0x2334;		// v = 0.5
		//	udword |= 0x247c;		// v = 0.7
			if (dcb_prog_i2c(bank, mod, i2c_command, udword))
				break;

			// signal = B01_M01_VDEL, v = 0.800797, data = 0x149
			udword = 0x7a <<16;
			udword |= 0x35 <<8;
			udword |= 0x24;
			if (dcb_prog_i2c(bank, mod, i2c_command, udword))
				break;

			i2c_command = DCB_I2C_DAC1;
			// === wake up unit 0(L), DAC1 SDA
			udword = 0x78 <<16;
			udword |= 0xf0 <<8;
			udword |= 0x3c;
			if (dcb_prog_i2c(bank, mod, i2c_command, udword))
				break;

			// signal = B01_M01_VCMP0, v = 0.599781, data = 0xf6
			udword = 0x78 <<16;
			udword |= 0x03 <<8;
			udword |= 0xd8;
			if (dcb_prog_i2c(bank, mod, i2c_command, udword))
				break;

			// signal = B01_M01_VCMP1, v = 0.599781, data = 0xf6
			udword = 0x78 <<16;
			udword |= 0x13 <<8;
			udword |= 0xd8;
			if (dcb_prog_i2c(bank, mod, i2c_command, udword))
				break;

			// signal = B01_M01_VCMP2, v = 0.599781, data = 0xf6
			udword = 0x78 <<16;
			udword |= 0x23 <<8;
			udword |= 0xd8;
			if (dcb_prog_i2c(bank, mod, i2c_command, udword))
				break;

			// signal = B01_M01_VCMP3, v = 0.599781, data = 0xf6
			udword = 0x78 <<16;
			udword |= 0x33 <<8;
			udword |= 0xd8;
			if (dcb_prog_i2c(bank, mod, i2c_command, udword))
				break;

			// === wake up unit 1(L), DAC1 SDA
			udword = 0x7a <<16;
			udword |= 0xf0 <<8;
			udword |= 0x3c;
			if (dcb_prog_i2c(bank, mod, i2c_command, udword))
				break;

			// signal = B01_M01_VCMP4, v = 0.599781, data = 0xf6
			udword = 0x7a <<16;
			udword |= 0x03 <<8;
			udword |= 0xd8;
			if (dcb_prog_i2c(bank, mod, i2c_command, udword))
				break;

			// signal = B01_M01_VCMP5, v = 0.599781, data = 0xf6
			udword = 0x7a <<16;
			udword |= 0x13 <<8;
			udword |= 0xd8;
			if (dcb_prog_i2c(bank, mod, i2c_command, udword))
				break;

			// signal = B01_M01_VCMP6, v = 0.599781, data = 0xf6
			udword = 0x7a <<16;
			udword |= 0x23 <<8;
			udword |= 0xd8;
			if (dcb_prog_i2c(bank, mod, i2c_command, udword))
				break;

			// signal = B01_M01_VCMP7, v = 0.599781, data = 0xf6
			udword = 0x7a <<16;
			udword |= 0x33 <<8;
			udword |= 0xd8;
			if (dcb_prog_i2c(bank, mod, i2c_command, udword))
				break;

			// === wake up unit 0(M), DAC1 SDA
			udword = 0xb8 <<16;
			udword |= 0xf0 <<8;
			udword |= 0x3c;
			if (dcb_prog_i2c(bank, mod, i2c_command, udword))
				break;

			// signal = B01_M01_VCMP8, v = 0.599781, data = 0xf6
			udword = 0xb8 <<16;
			udword |= 0x13 <<8;
			udword |= 0xd8;
			if (dcb_prog_i2c(bank, mod, i2c_command, udword))
				break;

			// signal = B01_M01_VCMP9, v = 0.599781, data = 0xf6
			udword = 0xb8 <<16;
			udword |= 0x23 <<8;
			udword |= 0xd8;
			if (dcb_prog_i2c(bank, mod, i2c_command, udword))
				break;

			// signal = B01_M01_VCMP10, v = 0.599781, data = 0xf6
			udword = 0xb8 <<16;
			udword |= 0x33 <<8;
			udword |= 0xd8;
			if (dcb_prog_i2c(bank, mod, i2c_command, udword))
				break;

			// signal = B01_M01_VCMP11, v = 0.599781, data = 0xf6
			udword = 0xb8 <<16;
			udword |= 0x03 <<8;
			udword |= 0xd8;
			if (dcb_prog_i2c(bank, mod, i2c_command, udword))
				break;

			// === wake up unit 1(M), DAC1 SDA
			udword = 0xba <<16;
			udword |= 0xf0 <<8;
			udword |= 0x3c;
			if (dcb_prog_i2c(bank, mod, i2c_command, udword))
				break;

			// signal = B01_M01_VCMP12, v = 0.599781, data = 0xf6
			udword = 0xba <<16;
			udword |= 0x33 <<8;
			udword |= 0xd8;
			if (dcb_prog_i2c(bank, mod, i2c_command, udword))
				break;

			// signal = B01_M01_VCMP13, v = 0.599781, data = 0xf6
			udword = 0xba <<16;
			udword |= 0x23 <<8;
			udword |= 0xd8;
			if (dcb_prog_i2c(bank, mod, i2c_command, udword))
				break;

			// signal = B01_M01_VCMP14, v = 0.599781, data = 0xf6
			udword = 0xba <<16;
			udword |= 0x13 <<8;
			udword |= 0xd8;
			if (dcb_prog_i2c(bank, mod, i2c_command, udword))
				break;

			// signal = B01_M01_VCMP15, v = 0.599781, data = 0xf6
			udword = 0xba <<16;
			udword |= 0x03 <<8;
			udword |= 0xd8;
			if (dcb_prog_i2c(bank, mod, i2c_command, udword))
				break;


			break;

		// Prog CHSEL:  pchsel pattern
		case PCHSEL:
			data=0;
			sscanf(ptr, "%i", &data);

			bank=1;
			mod=1;
			i2c_command = DCB_I2C_CHSEL;

			udword = 0x40 <<16;
			udword |= (data & 0xffff);
			if (dcb_prog_i2c(bank, mod, i2c_command, udword))
				break;

			break;


		// SelAll - activate all chip selects
		case SelAll:
			bank=1;
			mod=1;

			// CHSEL sda
			i2c_command = DCB_I2C_CHSEL;

			// signal = B01_M01_CHSEL, data = 0xffff
			udword = 0x40 <<16;
			udword |= 0xffff;
			if (dcb_prog_i2c(bank, mod, i2c_command, udword))
				break;

			break;


// ======================================================================
		case Exit:
		case Quit:
			return 1;		// signal to exit
			break;

		case Menu:			// note upper case M
			menu();			// note lower case m
			break;

#ifdef DEBUG
		case Dbglvl:		// set or verify the debug level
			if (*ptr)
				sscanf(ptr, "%d", &dbglvl);
			else
				printf("dbglvl is %d\n", dbglvl);
			break;
#endif

		default:
			break;			// should never get here
		}

	/* end of main loop switch  - please preserve this line for mkheader */
return 0;
}


/******************************************************************************\
**                                                                            **
**          Main                                                              **
**                                                                            **
\******************************************************************************/

int main(int argc, char *argv[])
{
int Go;
char *line;

#ifdef DEBUG
debug_init("debug.out", argv[1]);	// Start the debugger in specified directory
dbglvl=8;		// pretty full details to debug file
#endif

if (dcb_initialize("unknown"))
	printf("Bad return from dcb_initialize()\n");

if (gs_start(0))
	printf("Bad return from gs_start()\n");

xor_tab_initialize(-1);

Go = True;
while(Go)
	{
		// Get a command line from the operator
	line = readline("*");
	
	if (line && *line)
		add_history (line);

	if (command_interpreter(line))
		Go = False;

	free(line);
	}
return 0;
}


/******************************************************************************\
**                                                                            **
**          Command Interpreter                                               **
**                                                                            **
\******************************************************************************/
static int command_interpreter(char *ptr)
{
int i, j, n, idx=0;
char *q;

		// find operator's command in the list
	while( (q = strchr(ptr, '\t')) )	// change tabs to spaces
		*q = ' ';
	if ( (q = strrchr(ptr, '\n')) )		// remove trailing newline
		*q = '\0';
	while ( isspace(*ptr) )		// skip whitespace
		ptr++;
	if (*ptr == '\0')		// empty line?
		return 0;
	for (n=0; *(ptr+n); n++)	// count characters in command
		if (isspace(*(ptr+n)))
			break;
	j = 0;			// count matching names
	for (i = 0; i < command_count; i++)
		if ( strncasecmp(ptr, command_list[i].name, n) == 0 )
			{
			idx = i;		// permit unambiguous abbreviations
			j++;
			if ( n == strlen(command_list[i].name) )
				{
				j = 1;		// to skip next block
				break;		// exact match is exempt from ambiguity check
				}
			}
	if (j != 1)
		{
		for (i=0; *(ptr+i) && !isspace(*(ptr+i)); i++)
			;
		*(ptr+i) = '\0';		// isolate command
		if (j == 0)
			printf("Command not found: '%s'\n", ptr);
		else
			printf("Command '%s' is ambiguous.\n", ptr);
		return 0;
		}
	strncpy(cmd_txt, ptr, n);	// save copy of command
	cmd_txt[n] = '\0';

		// prepare the arguments for the command
		// these convenience functions may be contraindicated in some cases
	ptr += n;		// skip over command name
	while ( isspace(*ptr) )		// skip whitespace
		ptr++;
	for (i=strlen(ptr)-1; i && isspace(*(ptr+i)); i--)
		*(ptr+i) = '\0';				// trim trailing blanks, if any
	for (i=0; *(ptr+i); i++)	// remove extra blanks
		{
		while (*(ptr+i) == ' ' && *(ptr+i+1) == ' ')
			strcpy(ptr+i, ptr+i+1);			// move left 1 space
		}

return command_dispatcher(command_list[idx].cmnd, ptr);
}


/******************************************************************************\
**                                                                            **
**          Support routines                                                  **
**                                                                            **
\******************************************************************************/

static void menu()
{
char *list[500];
char line[120];
int i, j, n, col, ncols;

if (command_count == 0)		// most unlikely
	return;

	// construct a list of pointers to the names
n = command_count;
for (i=0; i<n; i++)
	list[i] = command_list[i].name;

	// sort the pointers into alphabetical order
qsort (list, (size_t)n, sizeof(char *), (__compar_fn_t)menuPrint_helper);	/* sort the list */

	// print the names with 12 column alignment
//ncols = -1+display_width();
ncols = 79;
for (i = 0; i < ncols; i++)
	line[i] = ' ';		/* blank the output line */
col = 5;					/* start printing in col 5 */
for (j = 0; j < n; j++)
	{if (col + strlen( list[j] ) >= ncols)
		{line[ncols] = '\0';
		printf(line);		/* line is full, so print */
		printf("\n");
		for (i = 0; i < ncols; i++)
			line[i] = ' ';
		col = 5;
		}
	for (i = 0; *(list[j]+i);  )		/* transfer name */
		line[col++] = *(list[j]+i++);
	col = 5+12*(1+(++col-6)/12);		/* one blank & tab over */
	}
line[col] = '\0';
printf(line);		/* print partial line */
printf("\n");

return;
}


static int menuPrint_helper(char **s, char **t)
{
return (strcasecmp(*s, *t));	/* compare names */
}


