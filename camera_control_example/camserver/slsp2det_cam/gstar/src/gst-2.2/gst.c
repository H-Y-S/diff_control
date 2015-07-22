/*
** gst.c - gigastar adapter card test program
*/

/*
** Copyright (C) 2004-2006 Eric F. Eikenberry
** Paul Scherrer Institute, CH-5232 Villigen-PSI, Switzerland.
** All rights reserved.
**
** GPL license declared below.
*/

/* from commando.c - an easy-to-use command dispatching program
**
**	Copyright 1999 - 2006 by Eric F. Eikenberry.
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
** EFE 19 Nov 04
*/

#define COMMAND_MAIN	// turn on space allocation in gst.h

#include <stdio.h>
#include <unistd.h>
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

#include "gst.h"
#include "gsdc.h"
#include "gslib.h"

// compiler switch not defined in the standalone setup
#ifdef _SLS_CAMERA_
#include "detsys.h"
#include "pix_detector.h"
#endif

#undef COMMAND_MAIN

#define True 1
#define False 0
#define MAXPROCS 200		// number of forked processes



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
void menu();
static int menuPrint_helper(char **, char **);
static __sighandler_t sighandler(int, siginfo_t *, void *);


/******************************************************************************\
**                                                                            **
**          Global variables                                                  **
**                                                                            **
\******************************************************************************/
static char buf[0x1000000];
static int pagesize;
static int pcnt=0;
static struct sigaction showRet = { {(__sighandler_t)sighandler}, {{0}}, SA_SIGINFO, NULL };

#ifdef _SLS_CAMERA_
static unsigned int img0[NROW_DET*NCOL_DET], img1[NROW_DET*NCOL_DET];
double exposure_time;		// needed by xor_tab.c
#endif

/******************************************************************************\
**                                                                            **
**          Command dispatcher                                                **
**                                                                            **
\******************************************************************************/

static int command_dispatcher(COMMAND_CMND cmnd, char *ptr)
{
int i, ii, j, k, l, mm, n, mapsize, count, cnt, idx, last, ofd;
int  tmodcnt;
char *p, *q, *pn, *qn, line[150];
double ttime, t1;
unsigned int *uip, chksum, chksumn, chk2, txdata[0x20000], rxdata[0x20000], t;
unsigned int chkp0, chkp1, *pImage;
struct timeval tv;
struct timezone tz;

#ifdef _SLS_CAMERA_
int bank, mod;
int bankoffset, offset, writeflag, modcnt, nt, ns, pcmax, nerr;
double  t0, tsum, tdmax;
unsigned int *theImage;
pid_t pid;
FILE* logfp;
#endif


	ii=tmodcnt=0;		// keep the compiler happy
	ttime=0.0;
	pImage=NULL;

	DBG(8, "User argument: %s\n", ptr)

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

	// set the zero time - both tsc & cmos
		case T0:
			gs_set_t0(0);
			break;

	// print elapsed time
		case Rdt:
			printf("Elapsed Time: %.6lf sec\n", gs_time() );
			break;

	// print timestamp
		case Timestamp:
			printf("%s\n", gs_timestamp());
			break;


	// measure the time to call each of the clocks
	// average time for a call to gs_time():           0.95 us.
	// average time for a call to gettimeofday():      2.8 us
	// average time for a call to read cmos time:     35.7 us
	// compare these times at runlevel 1 - they can be much faster there
		case TimeT:
			gs_set_t0(0);
			for (i=0; i<1000000; i++)
				t1 = gs_time();
			printf("average time for gs_time(): %.3lf usec\n", t1);
			gs_set_t0(0);
			for (i=0; i<10000; i++)
				gettimeofday(&tv, &tz);
			t1=gs_time();
			printf("average time for gettimeofday(): %.1lf usec\n", 100*t1);
			break;


	// get status words for device i
		case status:
			i=0;
			sscanf(ptr, "%d", &i);
			printf("status:      0x%x\n", gs_status(i));
			printf("irq int status:  0x%x\n", gs_irq_status(i));
			printf("irq src status:  0x%x\n", gs_irq_src_status(i));
			break;

	// reset device i
		case reset:
			i=0;
			sscanf(ptr, "%d", &i);
			gs_reset_all(i);
			break;

	// start device
		case Start:
			i=0;
			sscanf(ptr, "%d", &i);
			gs_start(i);
			break;

	// bbar0r i j k - block read bar0: device=i, offset=j, n_dwords=k
		case Bbar0r:
			i=j=0;
			k=1;		// default
			sscanf(ptr, "%d%d%d", &i, &j, &k);
			if (k==0)
				break;
			k = j+k>32 ? 32-j : k;
			memset(buf, 0x55, 32*4);
			uip = gs_read_bar0_reg(i, 4*j, 4*k);
			if (!uip)
				break;
			for(l=0;l<k;l++)
				printf("%2d  0x%x\n", j+l, uip[l]);
			break;

	// full status  -- print status report of device i
		case FullStat:
			i=0;
			sscanf(ptr, "%d", &i);
			n=gs_status(i);
			switch (n & GS_MODE)
				{
				case GS_MODE_I:
					printf("  Device mode:   idle\n");
					break;
				case GS_MODE_R:
					printf("  Device mode:   R-mode\n");
					break;
				case GS_MODE_T:
					printf("  Device mode:   T-mode\n");
					break;
				case GS_MODE_U:
					printf("  Device mode:   U-mode\n");
					break;
				default:
					break;
				}
			printf("%s\n", n&GS_PMC_RUN ?
					"  Run mode:      enabled" : "  Run mode:      disabled");
			printf("%s\n", n&GS_INTR ? 
				"  Global interrupt: enabled" : "  Global interrupt: disabled");
			printf("%s\n", n&GS_DMA_TIMEOUT ?
					"  DMA timeout:   enabled" : "  DMA timeout:   disabled");
			if (n&GS_RX_LINK_ERR)
				printf("  RX link error reported\n");
			if (n&GS_TX_LINK_ERR)
				printf("  TX link error reported\n");
			if (n&GS_RX_FIFO_EMPTY)
				printf("  RX fifo:       empty\n");
			else if (n&GS_RX_FIFO_FULL)
				printf("  RX fifo:       full\n");
			else  if (n&GS_RX_FIFO_PARTIAL)
				printf("  RX fifo:       partially full\n");
			else
				printf("  RX fifo:       not empty\n");
			if (n&GS_TX_FIFO_EMPTY)
				printf("  TX fifo:       empty\n");
			else if (n&GS_TX_FIFO_FULL)
				printf("  TX fifo:       full\n");
			else
				printf("  TX fifo:    not empty\n");
			printf("%s\n", n&GS_RX_MBOX_FULL ?
				"  RX mailbox:   full" : "  RX mailbox:    not full");
			printf("%s\n", n&GS_TX_MBOX_EMPTY ?
				"  TX mailbox:    empty" : "  TX mailbox:    not empty");
			break;

	// full irq status
		case FIrqstat:
			i=0;
			sscanf(ptr, "%d", &i);
			n=gs_irq_src_status(i);
			printf("irq status: 0x%x - transient bits recorded:\n", n);
			if(n&GS_RX_LINK_ERROR_INT)
				printf("GigaSTaR RX chip link error\n");
			if(n&GS_TX_LINK_ERROR_INT)
				printf("GigaSTaR TX chip link error\n");
			if(n&GS_RX_FIFO_NOT_MT_INT)
				printf("RX fifo not empty\n");
			if(n&GS_RX_FIFO_PART_INT)
				printf("RX fifo partially filled\n");
			if(n&GS_RX_FIFO_FULL_INT)
				printf("RX fifo full\n");
			if(n&GS_TX_FIFO_FULL_INT)
				printf("TX fifo full\n");
			if(n&GS_DMA_WR_DONE_INT)
				printf("successful DMA write (RX to host)\n");
			if(n&GS_DMA_WR_DIED_INT)
				printf("DMA write timeout - RX underrun\n");
			if(n&GS_DMA_WR_STOP_INT)
				printf("DMA write - max # retries exceeded\n");
			if(n&GS_DMA_WR_ABORT_INT)
				printf("PCI-bus target or master abort on RX\n");
			if(n&GS_DMA_RD_DONE_INT)
				printf("successful DMA read (host to TX)\n");
			if(n&GS_DMA_RD_OVFL_INT)
				printf("DMA read - TX fifo overflow\n");
			if(n&GS_DMA_RD_STOP_INT)
				printf("DMA read - max # retries exceeded\n");
			if(n&GS_DMA_RD_ABORT_INT)
				printf("PCI-bus target or master abort on TX\n");
			break;

		case Ioctl:
			printf("%.6lf\n", gs_last_interrupt_time(0));
			break;

#define NCYCLES 4096
#define NEXTRAS 10
	// TX data by programmed I/O; RX data by programmed I/O
	// also investigate depth of RX fifo
	// *** requires a loopback connection - TX to RX on the card
	// Or, a second card in the power-on default R-mode will work as well

	// 512 cycles is the theoretical capactiy of the RX fifo (8K dwords).
	// However, it reports 'not full'.  Test15 shows that 10 more
	// writes are required to fill it.  

	// revision 1.2 installed - RX fifo is 64K, requiring 4096 cycles
	// extra dwords in the fpga is still 10
	
/* If NEXTRAS=0, everything is as expected - 8192 dwords in, 8192 dwords
** out, and the checksum agrees.  The RX-fifo is "partially full"
** If NEXTRAS=10, the RX-fifo is full.  Read-back of 8192+10=8202 dwords
** is accurate.  But the fifo remains "full" (not "partial") until the
** last datum is read; then it becomes "empty".
** If NEXTRAS=16, 6 data are transmitted to the "full" fifo, and are not
** recovered by readback (as expected).
** So, the only problem is that the fifo remains "full" until it is
** "empty".
**
** revision 1.2 - FIFO fills with 65546 dwords
*/
		case Pio1:		// programmed I/O fill rx fifo, 16 dwords at a time, & check
			mm=0;
			sscanf(ptr, "%d", &mm);
			gs_reset_all(mm);
			gs_start(mm);
			memset(rxdata, 0x12345, sizeof(rxdata));
			memset(txdata, 0, sizeof(txdata));
			chksum=0;
			for(j=0; j<NCYCLES; j++)
				{
				for(i=0;i<16;i++)
					{
					txdata[j*16+i]=random();
					chksum^=txdata[j*16+i];
					}
		//		n=gs_tx_data(mm, txdata+j*16, 16*4);		// transmit data 16 at a time
				}
			n=gs_tx_data(mm, txdata, 16*NCYCLES*4);		// transmit data as 1 block

			printf("First regular datum sent: 0x%x\n", txdata[0]);
			printf("Last regular datum sent: 0x%x, chk: 0x%x\n",
						txdata[16*NCYCLES-1], chksum);

			n=gs_status(mm);				// print status of RX fifo
			printf("After sending %d dwords:  ", 16*NCYCLES);
			if (n&GS_RX_FIFO_EMPTY)
				printf("  RX fifo is empty\n");
			else if (n&GS_RX_FIFO_FULL)
				printf("  RX fifo is full\n");
			else  if (n&GS_RX_FIFO_PARTIAL)
				printf("  RX fifo is partially full\n");
			else
				printf("  RX fifo is not empty\n");

		// finish filling the fifo
			for(j=0; j<NEXTRAS; j++)
				{
				txdata[16*NCYCLES+j]=random();
				chksum^=txdata[16*NCYCLES+j];
				n=gs_tx_data(mm, txdata+16*NCYCLES+j, 4);		// transmit data
				printf("extra transmission %d: 0x%x: chk: 0x%x ",
						j, txdata[16*NCYCLES+j], chksum);
					{int n=gs_status(mm);		// preserve the other 'n'
					if (n&GS_RX_FIFO_EMPTY)
						printf("  RX fifo is empty\n");
					else if (n&GS_RX_FIFO_FULL)
						printf("  RX fifo is full\n");
					else  if (n&GS_RX_FIFO_PARTIAL)
						printf("  RX fifo is partially full\n");
					else
						printf("  RX fifo is not empty\n");
					}
				}

			printf("After sending %d dwords:  ", NEXTRAS+16*NCYCLES);
			n=gs_status(mm);				// print status of RX fifo
			if (n&GS_RX_FIFO_EMPTY)
				printf("  RX fifo is empty\n");
			else if (n&GS_RX_FIFO_FULL)
				printf("  RX fifo is full\n");
			else  if (n&GS_RX_FIFO_PARTIAL)
				printf("  RX fifo is partially full\n");
			else
				printf("  RX fifo is not empty\n");

			n=0;					// number of bytes read
			n=gs_rx_data(mm, rxdata, 16*NCYCLES*4);
			printf("Read %d dwords\n", n/4);

			chk2=0;			// compute chksum so far
			for (i=0;i<16*NCYCLES;i++)
				chk2^=rxdata[i];
			printf("First regular datum received: 0x%x\n", rxdata[0]);
			printf("Last regular datum received: 0x%x, chk: 0x%x\n",
				rxdata[16*NCYCLES-1], chk2);

		// compare the arrays
			for(i=0;i<16*NCYCLES;i++)
				if(txdata[i] != rxdata[i])
					break;
			if (i<16*NCYCLES)
				printf("No compare at i=%d, tx: 0x%x, rx: 0x%x\n",
						i, txdata[i], rxdata[i]);

		// read the extras
			for (j=0; j<NEXTRAS; j++)
				{
				n+=gs_rx_data(mm, rxdata+n/4, 4);
				chk2^=rxdata[(n/4)-1];
				printf("extra read %d:  0x%x: chk: 0x%x  ",
							j, rxdata[(n/4)-1], chk2);
					{int n=gs_status(mm);		// preserve the other 'n'
					if (n&GS_RX_FIFO_EMPTY)
						printf("  RX fifo is empty\n");
					else if (n&GS_RX_FIFO_FULL)
						printf("  RX fifo is full\n");
					else  if (n&GS_RX_FIFO_PARTIAL)
						printf("  RX fifo is partially full\n");
					else
						printf("  RX fifo is not empty\n");
					}
				}

			l=0;
			for (i=0;i<n/4;i++)
				l^=rxdata[i];
			printf("n = %d, Input chksum = 0x%x, output chksum = 0x%x\n",
					n/4, chksum, l);
			printf("%s\n", chksum==l ? "*** OK ***" : "*** NO MATCH ***");
			break;
#undef NCYCLES
#undef NEXTRAS


#define NPAGES 57
	// interrupt testing - write a block by dma, read by dma with interrupt
	// *** requires a loopback connection - TX to RX on the card
	// Or, a second card in the power-on default R-mode will work as well
		case Test18:
			mm=0;
			sscanf(ptr, "%d", &mm);
		// comment out the next 3 lines to test withour re-initializing each time
		// in that case, use 'reset' and 'start' to reinitialize.
			if (gs_reset_all(mm))
				break;
			gs_start(mm);
			mapsize=NPAGES*pagesize;		// in bytes

		// set interrupt on RX dma (i.e., dma write) finished & errors
			gs_set_irq_mask(mm,
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
			if ( (p=gs_rx_dma_setup(mm, mapsize))==NULL )
				break;

		// map npages for TX
			if ( (q=gs_tx_dma_setup(mm, mapsize))==NULL )
				break;

		// set up TX data
			chksum=0;
			uip=(unsigned int *)q;
			for (i=0;i<mapsize/4;i++)	// fill the tx buffer
				{
				uip[i]=random();
				chksum^=uip[i];
				}

		// enable interrupt using the mask set above
			gs_set_control(mm, GS_INT_ENABLE);

		// enable dma write to host
			gs_set_control(mm, GS_FIRE_DMA_WRITE);

		// set up timer
			gs_set_t0(0);
			printf("Time before: %.6lf\n", gs_time() );

		// start transfer - transmit npages of data
			gs_set_control(mm, GS_FIRE_DMA_READ);

		// what happens if the interrupt expires before we call gs_rx_dma()?
		//	sleep(1);		// it's OK - no problem with sleep()

		// wait for interrupt
			n = gs_rx_dma(mm);

		// show the elapsed time
			printf("Time after: %.6lf\n", gs_time() );	// ~2.8 ms, 0x400 bs

			printf("read returned 0x%x bytes = 0x%x dwords\n", n,n/4);
		
		// look at status
			t=gs_irq_src_status(mm);
			printf("irq status: 0x%x", t);
			if (t & GS_DMA_WR_DONE_INT)
				printf(":  DMA write was successful\n");
			else
				printf(":  DMA write was NOT successful\n");

		// clear the interrupt
			gs_clear_irq(mm);

		// verify data
			l=0;
			uip=(unsigned int *)p;
			for (i=0;i<mapsize/4;i++)
				l^=uip[i];
			printf("Checksums: 0x%x,  0x%x\n", chksum, l);
			
			printf("IRQ source status: 0x%x\n", gs_irq_src_status(mm));

			printf("%s\n", chksum==l&&n==mapsize ? "*** OK ***" : "*** BAD MATCH ***");

			break;
#undef NPAGES


#define NTRIES 10000
#define NPAGES 57
	// interrupt testing - write a block by dma, read by dma with interrupt
	// uses 2 buffers each for reading & writing, but does not interleave them
	// *** requires a loopback connection - TX to RX on the card
	// Or, a second card in the power-on default R-mode will work as well
		case Test19:
			mm=0;
			sscanf(ptr, "%d", &mm);
			if (gs_reset_all(mm))
				break;
			gs_start(mm);
			mapsize=NPAGES*pagesize;		// in bytes

		// set interrupt on RX dma (i.e., dma write) finished & errors
		// do not set for TX dma done (GS_DMA_RD_DONE_INT)
			gs_set_irq_mask(mm,
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

			ttime=0;		// total time
			count=0;
			for(ii=0;ii<NTRIES;ii++)
				{

			// map npages for RX
				if ( (p=gs_rx_dma_setup(mm, mapsize))==NULL )
					break;

			// map npages for TX
				if ( (q=gs_tx_dma_setup(mm, mapsize))==NULL )
					break;

			// set up TX data
				chksum=0;
				uip=(unsigned int *)q;
				for (i=0;i<mapsize/4;i++)
					{
					uip[i]=random();
					chksum^=uip[i];
					}

			// enable interrupt using the mask set above
				gs_set_control(mm, GS_INT_ENABLE);

			// enable dma write to host: RX dma
				gs_set_control(mm, GS_FIRE_DMA_WRITE);

			// set up timer
				gs_set_t0(0);

			// start transfer - transmit npages of data: TX dma
				gs_set_control(mm, GS_FIRE_DMA_READ);

			// wait for interrupt
				n = gs_rx_dma(mm);

				if (n != NPAGES*pagesize)
					{
					printf("\ntry # %d: Error: read returned 0x%x bytes = 0x%x dwords\n", ii,n,n/4);
					printf("irq source status: 0x%x\n", gs_irq_src_status(mm));
					break;
					}

				ttime+=gs_time();
				count++;

			// look at status
				t=gs_irq_src_status(mm);

			// clear the interrupt
				gs_clear_irq(mm);

				if (!(t & GS_DMA_WR_DONE_INT))
					{
					printf("\ntry # %d: irq source status: 0x%x: DMA write was NOT successful\n", ii,t);
					break;
					}

			// verify data
				l=0;
				uip=(unsigned int *)p;
				for (i=0;i<mapsize/4;i++)
					l^=uip[i];
				if (chksum != l)
					{
					printf("\ntry # %d:  Checksum failure: 0x%x,  0x%x\n", ii, chksum, l);
					printf("irq source status: 0x%x\n", gs_irq_src_status(mm));
					printf("irq status: 0x%x\n", gs_irq_status(mm));
					break;
					}

				if ((ii%100)==0 && ii!=0)
					{
					if ((ii%1000)==0)
						{
						printf("%5d\n", ii);
						fflush(stdout);
						}
					else
						{
						printf("%5d ", ii);
						fflush(stdout);
						}
					}
				}
			printf("\nFor %d loops, avg time: %.6lf sec\n", count, ttime/count);
			break;
#undef NPAGES
#undef NTRIES



#define NTRIES 1000
#define NPAGES 57
	// interrupt testing - write a block by dma, read by dma with interrupt
	// uses 2 buffers each for reading & writing
	// interleaved buffering
	// *** requires a loopback connection - TX to RX on the card
	// Or, a second card in the power-on default R-mode will work as well
		case Test20:
			mm=0;
			sscanf(ptr, "%d", &mm);
			if (gs_reset_all(mm))
				break;
			gs_start(mm);
			mapsize=NPAGES*pagesize;		// in bytes

		// set interrupt on RX dma (i.e., dma write) finished & errors
		// do not set for TX dma done (GS_DMA_RD_DONE_INT)
			gs_set_irq_mask(mm,
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

			ttime=0;		// total time
			count=0;
			cnt=0;

		// set up timer
			gs_set_t0(0);

		// map npages for RX
			if ( (p=gs_rx_dma_setup(mm, mapsize))==NULL )
				break;

		// map npages for TX
			if ( (q=gs_tx_dma_setup(mm, mapsize))==NULL )
				break;

		// set up TX data
			chksum=0;
			uip=(unsigned int *)q;
			for (i=0;i<mapsize/4;i++)
				{
				uip[i]=random();
				chksum^=uip[i];
				}

		// enable dma write to host: RX dma
			gs_set_control(mm, GS_FIRE_DMA_WRITE);

		// start transfer - transmit npages of data: TX dma
			gs_set_control(mm, GS_FIRE_DMA_READ);

		// enable interrupt using the mask set above
			gs_set_control(mm, GS_INT_ENABLE);

			for(ii=0;ii<NTRIES-1;ii++)
				{

// **********************************  start Test20 loop

			// at this point, q is emptying, and p is filling

			// if we set up a new RX buffer here, the gs_rx_dma()
			// points to the wrong buffer

			// get new buffer for TX
				if ( (qn=gs_tx_dma_setup(mm, mapsize))==NULL )
					break;

			// set up new TX data
				chksumn=0;
				uip=(unsigned int *)qn;
				for (i=0;i<mapsize/4;i++)
					{
					uip[i]=random();
					chksumn^=uip[i];
					}

			// will we be waiting for interrupt, or is it already done?
				t=gs_irq_src_status(mm);
				if ( !(t & GS_DMA_WR_DONE_INT) )
					cnt++;

			// wait for interrupt & previous transaction to be finished
				n = gs_rx_dma(mm);
				count++;

				if (n != NPAGES*pagesize)
					{
					printf("\ntry # %d: Error: read returned 0x%x bytes = 0x%x dwords\n", ii,n,n/4);
					printf("irq source status: 0x%x\n", gs_irq_src_status(mm));
					break;
					}

			// look at irq status
				t=gs_irq_src_status(mm);

			// clear the interrupt
				gs_clear_irq(mm);

				if (!(t & GS_DMA_WR_DONE_INT))
					{
					printf("\ntry # %d: irq source status: 0x%x: DMA write was NOT successful\n", ii,t);
					break;
					}

			// get new buffer for RX
				if ( (pn=gs_rx_dma_setup(mm, mapsize))==NULL )
					break;

			// enable dma write to host: RX dma
				gs_set_control(mm, GS_FIRE_DMA_WRITE);

			// start transfer - transmit npages of data: TX dma
				gs_set_control(mm, GS_FIRE_DMA_READ);

			// enable interrupt using the mask set above
				gs_set_control(mm, GS_INT_ENABLE);

			// while transfer is cooking, verify the data received above
				l=0;
				uip=(unsigned int *)p;
				for (i=0;i<mapsize/4;i++)
					l^=uip[i];
				if (chksum != l)
					{
					printf("\ntry # %d:  Checksum failure: 0x%x,  0x%x\n", ii, chksum, l);
		//			break;
					}

			// exchange the buffers
				q=qn;
				p=pn;
				chksum=chksumn;

				if ((ii%100)==0 && ii!=0)
					{
					if ((ii%1000)==0)
						{
						printf("%5d\n", ii);
						fflush(stdout);
						}
					else
						{
						printf("%5d ", ii);
						fflush(stdout);
						}
					}
// **********************************  Test20 loop
				}

		// get the last transfer

		// wait for interrupt & previous transaction to be finished
			n = gs_rx_dma(mm);
			count++;

			if (n != NPAGES*pagesize)
				{
				printf("\ntry # %d: Error: read returned 0x%x bytes = 0x%x dwords\n", ii,n,n/4);
				printf("irq source status: 0x%x\n", gs_irq_src_status(mm));
				}

		// look at irq status
			t=gs_irq_src_status(mm);

		// clear the interrupt
			gs_clear_irq(mm);

			if (!(t & GS_DMA_WR_DONE_INT))
				{
				printf("\ntry # %d: irq source status: 0x%x: DMA write was NOT successful\n", ii,t);
				}

		// verify the data 
			l=0;
			uip=(unsigned int *)p;
			for (i=0;i<mapsize/4;i++)
				l^=uip[i];
			if (chksum != l)
				{
				printf("\ntry # %d:  Checksum failure: 0x%x,  0x%x\n", ii, chksum, l);
				break;
				}

		// time for the entire operation
			ttime+=gs_time();

			printf("\nFor %d loops, avg time for full loop: %.6lf\n", count, ttime/count);
			printf("Approx. %.2lf E06 bytes/sec (full duplex/two way)\n", 
						(double)mapsize*(double)count/(ttime*1.0e6));
			printf("Waited for interrupt %d times\n", cnt);
			break;
#undef NPAGES
#undef NTRIES



#define NPAGES 57
		// source end - test one-way TX between computers
		// *** requires 2 computers - this one serves as the data source
		// start this (continuous) loop first in the second computer, 
		// then use Test22 or Test23 in the primary computer
		// This loop waits for a trigger, then transmits a block of
		// data with the value of the trigger word embedded in the
		// checksum, so the receiver can evaluate data integrity.
		// A trigger word of 0 causes the loop to terminate.
		// I find that a 3 GHz machine in SINGLE USER mode is required
		// to keep the pipeline full - otherwise I get RX fifo under-runs,
		// dma timeouts, and errors.
		case Test21:
			mm=0;
			if (gs_reset_all(mm))
				break;
			gs_start(mm);
			mapsize=NPAGES*pagesize;		// in bytes

		// set interrupt on TX dma (i.e., dma read) finished & errors
		// do not set for RX dma done (GS_DMA_WR_DONE_INT)
		// additionally, we get many RX_LINK_ERRs, so ignore them
			gs_set_irq_mask(mm,
						/*	GS_RX_LINK_ERROR_INT |	*/
							GS_TX_LINK_ERROR_INT |
						/*	GS_DMA_WR_DONE_INT | 	*/
							GS_DMA_WR_DIED_INT | 
							GS_DMA_WR_STOP_INT | 
							GS_DMA_WR_ABORT_INT |
							GS_DMA_RD_DONE_INT |  
							GS_DMA_RD_OVFL_INT |
							GS_DMA_RD_STOP_INT |
							GS_DMA_RD_ABORT_INT
							);

		// increase the tx burstsize - 0x1000 is the largest acceptable
		//	gs_set_tx_burstsize(0, 0x1000);

		// map npages for TX 0
			if ( (q=gs_tx_dma_setup(mm, mapsize))==NULL )
				break;

		// set up TX 0 data
			chksum=0;
			uip=(unsigned int *)q;
			for (i=0;i<-1+mapsize/4;i++)
				{
				uip[i]=random();
				chksum^=uip[i];
				}
			chkp0 = chksum;		// remember partial sum
			last=i;
			idx=0;
			uip[last]=chkp0^idx;	// overall chksum = idx

		// get second buffer for TX
			if ( (qn=gs_tx_dma_setup(mm, mapsize))==NULL )
				break;
		// set up TX 1 data
			chksum=0;
			uip=(unsigned int *)qn;
			for (i=0;i<-1+mapsize/4;i++)
				{
				uip[i]=random();
				chksum^=uip[i];
				}
			chkp1 = chksum;		// remember partial sum

		// initial status
			t = gs_status(mm);
			if (t&GS_RX_FIFO_EMPTY)
				printf("  RX fifo is empty\n");
			else if (t&GS_RX_FIFO_FULL)
				printf("  RX fifo is full\n");
			else  if (t&GS_RX_FIFO_PARTIAL)
				printf("  RX fifo is partially full\n");
			else
				printf("  RX fifo is not empty\n");

			count = 0;
			k=0;

// **********************************  start Test21 loop
			while(1)
				{

				t = gs_status(mm);
				if( !(t&GS_RX_FIFO_EMPTY) )
					k++;
			// wait for control word
				while( (t&GS_RX_FIFO_EMPTY) )
					t = gs_status(mm);

			// start transfer - transmit npages of data: TX dma
				gs_set_control(mm, GS_FIRE_DMA_READ);

			// enable interrupt using the mask set above
				gs_set_control(mm, GS_INT_ENABLE);

			// read the control word to clear the RX buffer
				n = gs_rx_data(mm, (char *)rxdata, 4);

			// put the idx in the next checksum
				if (count%2)
					{			// count odd
					uip=(unsigned int *)q;
					uip[last]=chkp0^idx;
					}
				else
					{			// count even
					uip=(unsigned int *)qn;
					uip[last]=chkp1^idx;
					}

			// wait for interrupt
				n = gs_tx_dma(mm);

			// look at irq status
				t=gs_irq_src_status(mm);

			// clear the interrupt
				gs_clear_irq(mm);

			// this causes too many errors
		//		if (count && t!=0x10000010)
		//			printf("Loop: %d, idx: %d, control %d, irq source status: 0x%x\n", count, idx, rxdata[0], t);
				count++;
				idx++;

			// test control word for end
				if (rxdata[0]==0)
					break;

			// swap buffers
				(void)gs_tx_dma_setup(mm, mapsize);

				}
// **************************************

			t = gs_status(mm);
			if (t&GS_RX_FIFO_EMPTY)
				printf("  RX fifo is empty\n");
			else if (t&GS_RX_FIFO_FULL)
				printf("  RX fifo is full\n");
			else  if (t&GS_RX_FIFO_PARTIAL)
				printf("  RX fifo is partially full\n");
			else
				printf("  RX fifo is not empty\n");

			printf("Data blocks transmitted: %d\n", count);
			printf("Control word was already there %d times\n",k);

			break;
#undef NPAGES



#define NTRIES 3000	// 300 = 30 modules * 10 frames
#define NPAGES 57
		// receive blocks of data from a remote source (e.g., a detector)
		// and write them to disk
		// send a datum to the remote source to trigger transfer
		// *** requires 2 computers - this one is the receiver
		case Test22:
			mm=0;
			if (gs_reset_all(mm))
				break;
			gs_start(mm);
			mapsize=NPAGES*pagesize;		// in bytes

		// set interrupt on RX dma (i.e., dma write) finished & errors
		// do not set for TX dma done (GS_DMA_RD_DONE_INT)
		// do not respond to RX_LINK_ERRs
			gs_set_irq_mask(mm,
					/*		GS_RX_LINK_ERROR_INT |	*/
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

		// map npages for RX 0
			if ( (p=gs_rx_dma_setup(mm, mapsize))==NULL )
				break;

		// enable dma write to host: RX dma to receive data
			gs_set_control(mm, GS_FIRE_DMA_WRITE);

		// enable interrupt using the mask set above
			gs_set_control(mm, GS_INT_ENABLE);

		// set up timer
			gs_set_t0(0);
			printf("Time before: %.6lf\n", gs_time() );

		// transmit a control word to start DMA transmission from source
			txdata[0]=1000000;
			gs_tx_data(mm, txdata, 4);

		// wait for interrupt
			n = gs_rx_dma(mm);

		// get irq status
			t=gs_irq_src_status(mm);

		// clear the interrupt
			gs_clear_irq(mm);

			if (n != NPAGES*pagesize)
				{
				printf("\ntry # %d: Error: read returned 0x%x bytes = 0x%x dwords\n", ii,n,n/4);
				printf("irq source status: 0x%x\n", gs_irq_src_status(mm));
				}

// ************************************** Test22 loop
			for(ii=0; ii<NTRIES-2; ii++)
				{

			// map npages for RX 1 if ii==0; else change buffers
				if ( (pn=gs_rx_dma_setup(mm, mapsize))==NULL )
					break;

			// enable dma write to host: RX dma to receive data
				gs_set_control(mm, GS_FIRE_DMA_WRITE);

			// enable interrupt using the mask set above
				gs_set_control(mm, GS_INT_ENABLE);

			// transmit a control word to start DMA transmission from source
				txdata[0]=1000001+ii;
				gs_tx_data(mm, txdata, 4);

			// compute checksum of previously received data
				l=0;
				uip=(unsigned int *)p;
				for (i=0;i<mapsize/4;i++)
					l^=uip[i];
		//		printf("loop %d: buffer %p: received checksum: 0x%x\n", ii, p, l);
				if (ii != l)
					{
					printf("Error: loop %d: checksum = %d\n", ii, l);
					gs_clear_irq(mm);		// clear interrupt
					break;
					}

			// write data to disk
				sprintf(line, "/raid/test1/file_%.5d.grbg", ii);
				if ( (ofd=open(line, O_WRONLY | O_CREAT, S_IRWXU)) == -1)
					{
					printf("Could not open: %s\n", line);
					break;
					}
				write(ofd, p, mapsize);
				close(ofd);

			// wait for interrupt
				n = gs_rx_dma(mm);

			// get irq status
				t=gs_irq_src_status(mm);

			// clear the interrupt
				gs_clear_irq(mm);

				if (n != NPAGES*pagesize)
					{
					printf("try # %d: Error: read returned 0x%x bytes = 0x%x dwords\n", ii,n,n/4);
					printf("status: 0x%x\n", gs_status(mm));
					printf("irq source status: 0x%x\n", t);
					}

			// change buffers
				p=pn;

				}
// ************************************** Test22 loop

		// enable dma write to host: RX dma to receive data
			gs_set_control(mm, GS_FIRE_DMA_WRITE);

		// enable interrupt using the mask set above
			gs_set_control(mm, GS_INT_ENABLE);

		// transmit a control word to start DMA transmission from source
			txdata[0]=0x0;
			gs_tx_data(mm, txdata, 4);

		// compute checksum of previously received data
			l=0;
			uip=(unsigned int *)p;
			for (i=0;i<mapsize/4;i++)
				l^=uip[i];
	//		printf("loop %d: buffer %p: received checksum: 0x%x\n", ii, p, l);
			if (ii != l)
				{
				printf("Error: loop %d: checksum = %d\n", ii, l);
				}

		// write data to disk
			sprintf(line, "/raid/test1/file_%.5d.grbg", ii);
			if ( (ofd=open(line, O_WRONLY | O_CREAT, S_IRWXU)) == -1)
				{
				printf("Could not open: %s\n", line);
				break;
				}
			write(ofd, p, mapsize);
			close(ofd);

		// change buffers
			p=gs_rx_dma_setup(mm, mapsize);
			ii++;

		// wait for interrupt
			n = gs_rx_dma(mm);

		// get irq status
			t=gs_irq_src_status(mm);

		// clear the interrupt
			gs_clear_irq(mm);

			if (n != NPAGES*pagesize)
				{
				printf("\ntry # %d: Error: read returned 0x%x bytes = 0x%x dwords\n", ii,n,n/4);
				printf("irq source status: 0x%x\n", gs_irq_src_status(mm));
				}

		// get time
			ttime = gs_time();

		// compute checksum of received data
			l=0;
			uip=(unsigned int *)p;
			for (i=0;i<mapsize/4;i++)
				l^=uip[i];
	//		printf("loop %d: buffer %p: received checksum: 0x%x\n", ii, p, l);
			if (ii != l)
				{
				printf("Error: loop %d: checksum = %d\n", ii, l);
				}

		// write data to disk
			sprintf(line, "/raid/test1/file_%.5d.grbg", ii);
			if ( (ofd=open(line, O_WRONLY | O_CREAT, S_IRWXU)) == -1)
				{
				printf("Could not open: %s\n", line);
				break;
				}
			write(ofd, p, mapsize);
			close(ofd);

		// get time
			ttime = gs_time();

		// show the elapsed time
			printf("Time after: %.6lf\n", ttime );
			printf("%d data blocks received; avg. time per block %.6lf\n",
					ii+1, ttime/(ii+1));
			printf("Approx. %.2lf E06 bytes/sec (half duplex)\n", 
						(double)mapsize*(double)(ii+1)/(ttime*1.0e6));

/*
-- without writing to disk
Time before: 0.000001
Time after: 0.680798
300 data blocks received; avg. time per block 0.002269
Approx. 102.88 E06 bytes/sec (half duplex)

With writing to disk: ~100 E06 bytes/sec
i.e., writing to disk costs ~ nothing.

With writing 3000 files to disk, and a much faster source machine:
Time before: 0.000005
Time after: 5.897747
3000 data blocks received; avg. time per block 0.001966
Approx. 118.76 E06 bytes/sec (half duplex)

in single user mode, this improves to 119.89 E06 bytes/sec (half duplex)
*/

			break;
#undef NPAGES
#undef NTRIES



#ifdef _SLS_CAMERA_
#define NIMAGES 9000
#define NPAGES 57
#define USE_FORK
		// receive blocks of data from a remote source (e.g., a detector)
		// send a datum to the remote source to trigger transfer
		// *** requires 2 computers - this one is the receiver; use Test21 
		// as the source.
		// similar to Test22, but with bank/module data processing to form 
		// detector "images" on disk; 1 'image' = 30 modules
		case Test23:
			xor_tab_initialize(0);
			mm=0;
			if (gs_reset_all(mm))
				break;
			gs_start(mm);
			mapsize=NPAGES*pagesize;		// in bytes
			writeflag = 0;
			theImage = img0;
			printf("mapsize is %d; IMAGE_NBYTES is %d\n", mapsize, IMAGE_NBYTES);
			modcnt=0;
			tsum=0.0;
			nt=0;
			k=0;
			ns=0;
			pcmax=0;
			nerr=0;
			tdmax=0;

		// set rx timeout - no, make the source better behaved
	//		gs_set_rx_timeout(mm, 100);	// ~ 25 ms

		// set up a log file
			if ( (logfp = fopen("/tmp/gst.log", "w")) == NULL )
				{
				printf("Could not open log file /tmp/gst.log\n");
				break;
				}

		// set interrupt on RX dma (i.e., dma write) finished & errors
		// do not set for TX dma done (GS_DMA_RD_DONE_INT)
		// do not repsond to RX_LINK_ERRs
			gs_set_irq_mask(mm,
					/*		GS_RX_LINK_ERROR_INT |	*/
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

		// map npages for RX 0
			if ( (p=gs_rx_dma_setup(mm, mapsize))==NULL )
				break;

		// map npages for RX 1
			if ( (pn=gs_rx_dma_setup(mm, mapsize))==NULL )
				break;

		// point dma to RX 0 buffer
			gs_rx_dma_setup(mm, mapsize);

		// set up timer
			gs_set_t0(0);
			printf("Time before: %.6lf\n", gs_time() );
			t0=gs_time();

			for (ii=0; ii<NIMAGES; ii++)
				{

// ************************************** Test23 loop

				for (bank=1; bank<=NBANK; bank++)
					{
					bankoffset = (bank-1)*(NROW_BANK + NROW_BTW_BANK);
					for (mod=1; mod<=NMOD_BANK; mod++)
						{

					// enable dma write to host: RX dma to receive data
						gs_set_control(mm, GS_FIRE_DMA_WRITE);

					// enable interrupt using the mask set above
						gs_set_control(mm, GS_INT_ENABLE);

					// transmit a control word to start DMA transmission from source
					// the control number is returned in the checksum of the next datum
						txdata[0]=1000000+modcnt;
						gs_tx_data(mm, txdata, 4);

						if (modcnt)		// only after first module is read
							{

						// compute checksum of previously received data
							l=0;
							uip=(unsigned int *)p;
							for (i=0;i<mapsize/4;i++)
								l^=uip[i];
							// the first module has the old control from test21 
							if (modcnt>1 && modcnt-1 != l)
								{
								printf("Error:  module %d: checksum = %d\n", modcnt-1, l);
								}

						// make an image from the previously received data
							offset = bankoffset + (mod-1)*(NCOL_MOD+NCOL_BTW_MOD);
			//				convert_image((unsigned short *)p, theImage+offset, 0);


						// if the flag is set, write previous image to disk
							if (writeflag)
								{
#ifdef USE_FORK
						// fork a process, which will be executed by another processor
								if (pcnt>MAXPROCS)	// we hope this never happens - but it does
									{
									j=10000*(pcnt-MAXPROCS);
									usleep(j);		// give away tens of milliseconds
									ns+=j;
									}
								pcnt++;
								pcmax = pcnt>pcmax ? pcnt : pcmax;
								t1=gs_time();
								tdmax = t1-t0>tdmax ? t1-t0 : tdmax;
								fprintf(logfp, "%s  %8.3lf  %4d  file_%.5d.img\n",
										gs_timestamp(), 1000*(t1-t0), pcnt, ii-1);
								t0=t1;
								fseek(logfp, 0, SEEK_END);		// prevent duplicate listings
								pid=fork();
								switch (pid)
									{
									case -1:
										printf("Error in fork\n");
										goto bailout;
									case 0:		// child process - do the write
										nice(2);		// lower our priority
										sprintf(line, "/raid/test1/file_%.5d.img", ii-1);
										if ( (ofd=open(line, O_WRONLY | O_CREAT, S_IRUSR|S_IWUSR)) == -1)
											{
											printf("Could not open: %s\n", line);
											exit (-1);
											}
										write(ofd, pImage, IMAGE_NBYTES);
										if ( close(ofd) == -1)
											printf("*** Error closing %s\n", line);
										exit (0);
									default:		// parent process
										break;
									}
#else
								sprintf(line, "/raid/test1/file_%.5d.img", ii-1);
								if ( (ofd=open(line, O_WRONLY | O_CREAT, S_IRUSR|S_IWUSR)) == -1)
									{
									printf("Could not open: %s\n", line);
									break;
									}
								write(ofd, pImage, IMAGE_NBYTES);
								close(ofd);
#endif
								writeflag = 0;
								if (((ii-1)%100)==0)		// show the operator we are still alive
									printf("Image %d\n", ii-1);
								}
							}

					// test if DMA already done
						if (gs_irq_src_status(mm)&GS_DMA_WR_DONE_INT)
							k++;

					// wait for interrupt
						n = gs_rx_dma(mm);
						modcnt++;

					// get irq status
						t=gs_irq_src_status(mm);

					// clear the interrupt
						gs_clear_irq(mm);

						if (n != mapsize)
							{
							printf("try # %d: Error: read returned %d bytes = %d dwords\n", modcnt,n,n/4);
							printf("status: 0x%x\n", gs_status(mm));
							printf("irq source status: 0x%x\n", t);
							if (nerr>10)
								goto bailout;
							gs_reset(mm);
							gs_start(mm);
							nerr++;
							}

					// change buffers
						p=pn;
						pn=gs_rx_dma_setup(mm, mapsize);

						}		// for (mod=1; mod<=NMOD_BANK; mod++)
					}		// for (bank=1; bank<=NBANK; bank++)
				if (theImage==img0)
					{
					theImage=img1;
					pImage = img0;
					}
				else
					{
					theImage=img0;
					pImage = img1;
					}
			// signal to write the image to disk
				writeflag = 1;

// ************************************** Test23 loop

				}		// for (ii=0; ii<NIMAGES; ii++)


		// prepare the last module data
		// compute checksum of previously received data
			l=0;
			uip=(unsigned int *)p;
			for (i=0;i<mapsize/4;i++)
				l^=uip[i];
			if (modcnt-1 != l)
				{
				printf("Error:  module %d: checksum = %d\n", modcnt-1, l);
				}

		// place the module in the image
			offset = bankoffset + (mod-1)*(NCOL_MOD+NCOL_BTW_MOD);
	//		convert_image((unsigned short *)p, theImage+offset, 0);

		// write the last image
			t1=gs_time();
			tdmax = t1-t0>tdmax ? t1-t0 : tdmax;
			fprintf(logfp, "%s  %8.3lf  %4d  file_%.5d.img\n",
					gs_timestamp(), 1000*(t1-t0), pcnt, ii-1);
			sprintf(line, "/raid/test1/file_%.5d.img", ii-1);
			if ( (ofd=open(line, O_WRONLY | O_CREAT, S_IRUSR|S_IWUSR)) == -1)
				{
				printf("Could not open: %s\n", line);
				break;
				}
			write(ofd, pImage, IMAGE_NBYTES);
			close(ofd);
			writeflag = 0;

		// get time
			ttime = gs_time();
			tmodcnt=modcnt;

		// turn off the source

		// enable dma write to host: RX dma to receive data
			gs_set_control(mm, GS_FIRE_DMA_WRITE);

		// enable interrupt using the mask set above
			gs_set_control(mm, GS_INT_ENABLE);

		// transmit a control word to start DMA transmission from source
			txdata[0]=0x0;
			gs_tx_data(mm, txdata, 4);

		// wait for interrupt
			n = gs_rx_dma(mm);

		// get irq status
			t=gs_irq_src_status(mm);

		// clear the interrupt
			gs_clear_irq(mm);

			if (n != NPAGES*pagesize)
				{
				printf("\ntry # %d: Error: read returned 0x%x bytes = 0x%x dwords\n", ii,n,n/4);
				printf("irq source status: 0x%x\n", gs_irq_src_status(mm));
				}

		// compute checksum of received data
			l=0;
			uip=(unsigned int *)p;
			for (i=0;i<mapsize/4;i++)
				l^=uip[i];
	//		printf("module %d: buffer %p: received checksum: 0x%x\n", modcnt-1, p, l);
			if (modcnt-1 != l)
				{
				printf("Error: module %d: checksum = %d\n", modcnt-1, l);
				}
		bailout:
			fclose(logfp);
		// show the elapsed time
			printf("Time after: %.6lf\n", ttime );
			printf("%d modules received; avg. time per module %.6lf\n",
					tmodcnt, ttime/tmodcnt);
			printf("%d images written to disk\n", ii);
			printf("Approx %.1lf images/sec written to disk\n", (double)ii/ttime);
	//		printf("Avg. conversion time %.6lf for %d conversions = %.3lf sec total\n", tsum/nt, nt, tsum);
			printf("Interrupt already done %d times\n", k);
			printf("%d errors were recorded\n", nerr);
#ifdef USE_FORK
			printf("Used %d milliseconds of usleep(3) to control process count\n", ns/1000);
			printf("We got up to %d processes\n", pcmax);
			printf("Average image time: %.3lf ms; longest: %.3lf ms\n", 1000*ttime/NIMAGES, 1000*tdmax);
#endif
/*
In single user mode for 2000 images (60000 modules, 11.6 GB) get 11.5 images/s
without fork(), 16.6 images/s with fork().  After 'startx', this remained
unchanged.  For this measure, it is important that the files do not pre-exist;
if files are being overwritten, the procedure slows to 14.1 images/s.
*/

			break;
#undef NPAGES
#undef NIMAGES
#endif			// _SLS_CAMERA_



		// send 1 dword and receive response while logging errors
		// This command runs on the transmitting computer
		// Start Test90 on the receiving computer before using this.
		case Test80:
			mm=0;
			gs_reset_all(mm);
			gs_start(mm);
			txdata[0] = 0xdeadbeef;
			t=gs_irq_src_status(mm);
			printf("IRQ source status before TX: 0x%x\n", t);
			n=gs_tx_data(mm, txdata, 4);	// transmit 1 dword
			printf("gs_tx_data() returned count: %d\n", n);
			t=gs_irq_src_status(mm);
			printf("IRQ source status after TX: 0x%x\n", t);
			gs_clear_irq(mm);
			t1=0.0;
			while( (t=gs_status(mm)) & GS_RX_FIFO_EMPTY)
				{
				if (t1==0.0 && (t&GS_RX_LINK_ERR || t&GS_TX_LINK_ERR))
					{
					printf("Link error in loop: Status: 0x%x\n", t);
					t1=gs_time();
					}
				}
			ttime=gs_time();
			if (t1!=0.0)
				printf("Link error lasted %.3lf msec\n", 1.0e3*(ttime-t1));
			n=gs_rx_data(mm, rxdata, 4);
			t=gs_irq_src_status(mm);
			printf("IRQ source status after RX: 0x%x\n", t);
			printf("gs_rx_data() returned count: %d, data: 0x%x\n", n, rxdata[0]);
			break;




		// Receive 1 dword, and send it back while logging errors.
		// This (continuous) loop runs on the receiving computer
		// Start this loop before using Test80
		case Test90:
			mm=0;
			while(1)
				{
				gs_reset_all(mm);
				gs_start(mm);
				t=gs_status(mm);
				t1=0.0;
				printf("... Status before while: 0x%x\n", t);
				while( (t=gs_status(mm)) & GS_RX_FIFO_EMPTY)
					{
					if (t1==0.0 && (t&GS_RX_LINK_ERR || t&GS_TX_LINK_ERR))
						{
						printf("* Error in loop: Status: 0x%x\n", t);
						t1=gs_time();
						}
					}
				ttime=gs_time();
				if (t1!=0.0)
					printf("Link error lasted %.3lf msec\n", 1.0e3*(ttime-t1));
				printf("IRQ source status (1): 0x%x\n", gs_irq_src_status(mm));
				t=gs_status(mm);
				printf("Status before RX: 0x%x\n", t);
				n=gs_rx_data(mm, rxdata, 4);
				t=gs_status(mm);
				printf("Status after RX: 0x%x\n", t);
				if ( (n!=4)||(rxdata[0]!=0xdeadbeef) )
					printf("Error: n= %d, rxdata = 0x%x\n",n, rxdata[0]);
				t=gs_irq_src_status(mm);
				printf("IRQ source status on RX: 0x%x\n", t);
				gs_clear_irq(mm);
				txdata[0] = ((rxdata[0]&0xffff)<<16) | ((rxdata[0]&0xffff0000)>>16);
				n=gs_tx_data(mm, txdata, 4);
				if ( (n!=4) )
					printf("Error: TX count = %d\n", n);
				t=gs_irq_src_status(mm);
				printf("IRQ source status on TX: 0x%x\n", t);
				}
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

sigaction(SIGCHLD, &showRet, NULL);		// signal handler for exit from fork

pagesize=getpagesize();
if (gs_initialize(NULL))
	printf("Bad return from initialize\n");

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

void menu()
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


static __sighandler_t sighandler(int s, siginfo_t *info, void *add)
{
pcnt--;
//printf("Reached sighandler, pcnt =  %d; pid = %d\n", pcnt, info->si_pid);
waitpid(info->si_pid, NULL, WNOHANG);
return 0;
}
