/*
** dcblib.c - a library of interface functions to the dcb
*/

/*
** Copyright (C) 2004-2010 Eric F. Eikenberry
** Paul Scherrer Institute, CH-5232 Villigen-PSI, Switzerland.
** All rights reserved.
** GPL license declared below.
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
** EFE 3 Feb 06
*/

/*
    DCB timing is simple while the system is lightly loaded.  With heavy
	loading, the timing can become very complex, owing to the fact that there
	are many possible situations.  
	
	Factors that cause the system to be overloaded are:
	(1) times too short;
	(2) too little available memory;
	(3) image directory too big (too many files);
	(4) overwriting existing image filenames
	
	Steps to take:
	(1) do 'deleteallobjs' in tvx (tvx can hold >50 % of system memory);
	(2) do 'rm *' in the image directory
	(3) have a cup of coffee - the computer is always better after a rest
		(during which the OS cleans up some system things)
	
	Discussion.  My standard 50 Hz test with a 90Sr source is:
		nimgages 1000
		exptime  0.012
		expper   0.020
	This discussion pertains to a single module system.

	Let:
	
	ts   = exposure start time (a clock time)
	texp = exposure time (a time interval) - e.g., 0.012 s
	te   = exposure end time (a clock time) = ts+texp
	tp   = exposre period (a time interval) - e.g., 0.020 s
	trdo = readout time (a time interval) - e.g., 0.00729 s
	thof = holdoff time (a time interval) - e.g. 0.00730 s
	tov  = overhead time (a time interval) - e.g. 0.00732 s
	tenb = time at which readout is enabled
	ti   = time at last interrupt (a clock time)
	tn   = time now: when prcoessing a reaadout begins (a clock time)

	Assume that we are in the normal situation: we are in the middle of 
	a long series and the computer has been responding OK (in a timely 
	fashion) up until now.  What normally happens?
	
	
	What can happen?

(to be continued)

*  holdoff time = time before ts when readout is blocked
* overhead time = trdo measured in the computer from tenb to ti
constraint: tp > texp+thof
* ti is tightly constrained as to when it can occur: after te+trdo and
before ts(next); e.g., from ts+0.0193 to ts+0.01999, a window of ~690 us.

Times and timing.

There are 3 timers in the system:
1) OS wall clock, accessed by gettimeofday()
2) the CPU timestamp counter, accessed by gs_time()
3) the DCB crystal clock

At the end of a 36,000 sec exposure, these differ by ~1 s, and the question
is "who is right"?

*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <math.h>
#include <sys/time.h>
#include <sys/utsname.h>

#include "debug.h"
#include "dcb.h"
#include "gslib.h"
#include "dcblib.h"
#include "gsdc.h"
#include "time_log.h"

#include "RELEASE.inc"	// code release date - allocates space


#define LENGTHFN 160
#define MODSIZE ((NROW_CHIP * NCOL_CHIP * NBITS * NCHIP)/8)

#if !ENABLE_BCBS
 #define MAPSIZE MODSIZE
#else
 #define MAPSIZE (NBANK_DCB * NMOD_BANK * MODSIZE)
#endif

// centralized temporary debug enable - comment out for higher speed
#define TDBG(a...) DBG(6, ## a)
//#define TDBG(x, a...)

// internal functions defined in this module
static char *dcb_read_module(int, int, int);
static char *dcb_read_banks(int);
static int dcb_rx_data_dma(int);
static unsigned int dcb_get_exp_count_in_frame(void);
static void dcb_bcb_reset(int);
static void dcb_bcb_reset_full(int);
static void dcb_read_measured_exposure_time(void);
static unsigned int dcb_get_exp_count_in_frame_snc_read(void);
static int dcb_set_enable_holdoff(double);
static void log_version_init(char *);
static void log_version(char *);
static int dcb_arm_multi_trigger(void);
#ifdef DEBUG
static char *intrprt_cmd(unsigned int);
static char *intrprt_reg(unsigned int);
#endif


// global variables for this module
static int dcbs_are_initialized=0;
static char *dmabuf[GS_MAX_DEVICES];
static int autoFrame=0;
static int longPeriod=0;
static double exposure_time = 0.0;		// shadows the variable in camserver.c
static double exposure_period = 0.0;
static double exposure_start_time;
static double frame_group_start_time = 0.0;
static double enable_time;
static double clock_skew_factor;
static double measured_exposure_time=0.0;
static char *logbuf=NULL, *logbufP;
static int logbufL=0;
static int tx_frame_limit;
static int nexppf;
static unsigned int exp_count_in_frame;			// programmed into DCB
static unsigned int exposures_in_frame=0;		// reported by DCB
static int external_enable=0;
static int external_trigger=0;
static int multi_trigger=0;
static int in_calpix_mode=0;
static int dma_timeout;
static int readout_n_bits=20;
static int mapsize=MAPSIZE;
static int ROI_readout=0;
static int last_readout_enabled=0;
static int th_sensing_enabled[6][NDCB];
static int trigger_offset_mode=0;
static int first_exposure_flag;
static double total_sequence_time;
static double dcb_clock_period=0.0;
static double overhead_time=0.0;
static double extra_overhead_time=0.0;
static double transfer_time=0.0;
static char theData[MAPSIZE*NDCB];
static unsigned int dcb_firmware_build=0;
static double dma_holdoff_time=0;
static double dma_clock_period = 0.000250;
#if ENABLE_BCBS
static unsigned int bcb_firmware_build=0;
static double bcb_clock_skew_factor=1.0;
static double bcb_clock_period=0.0;
static unsigned int dcb_min_read_time;
static unsigned int bcb_min_read_time;
#endif

/*
**  dcb_initialize() -- initialize a detector 
**  devno = pmc gigastar device number in computer
*/
int dcb_initialize(char *camera_name)
{
int i, n, gsdevno=0, dcb_count, mn, modenp, flag;
unsigned int t, f, *uip, dword;
double tn, ts, tt=0.0, dcp;
char line[150];

#if ENABLE_BCBS
 int nbits=0;
#endif

#ifndef TH_CONTROL_OVERRIDE
	int chan;
 double rh;
#endif

TDBG ("--- dcb_initialize()\n")

log_version_init(camera_name);

if (gs_initialize(NULL))	// initialize all gigastar cards
	{
	printf("Bad return from gs_initialize()\n");
	return -1;
	}

// get the count of pmc gigastar cards, and presumably the count of dcb's
if (gs_device_count() <= 0)
	{
	printf("No PMC GiaSTaR cards found\n");
	return -1;
	}
dcb_count = gs_device_count();

if (dcb_count < NDCB)
	{
	printf("dcb_count (%d) must be >= NDCB (%d)\n", dcb_count, NDCB);
	ERROR_RETURN;
	}
if (dcb_count > NDCB)
	{
	sprintf(line, "Found %d PMC GigaSTaR devices, but using only %d", dcb_count, NDCB);
	printf("%s\n", line);
	DBG(3, "%s\n", line)
	log_version(line);
	}
dcb_count = NDCB;

for(gsdevno=0; gsdevno<dcb_count; gsdevno++)
	{
	if (gs_start(gsdevno))
		{
		printf("Bad return from gs_start()\n");
		return -1;
		}
	uip=gs_read_bar0_reg(gsdevno, 4*GS_PMC_VERSION, 4);
	sprintf(line, "PMC GigaSTaR device no. %d, Version 0x%x", gsdevno, uip[0]);
	printf("%s\n", line);
	DBG(3, "%s\n", line)
	log_version(line);
	}

/*
** The given readout time is used to ensure that there is adequate time
** for readout plus overhead at the end of a frame:
** 		exposure_time + overhead_time <= exposure_period
**
** For the 67 MHz DCB, readout time (including some extra clocks
** for margin) is 170,500 clocks, or 2.54 ms.
**
** Holdoff time is the time before ENB goes active during which readout
** is blocked, even if DCB_SET_IMAGE_TX_ENB is given.  For now, it is
** taken to be the same as the readout time.
**
** Transfer time is the time required to move all data from the BCBs to
** computer memory.
*/


// determine the DMA clock period, required since using PCIe adapter
// even worse, cards have different clock periods - take the shortest
for(gsdevno=0; gsdevno<dcb_count; gsdevno++)
	{
	gs_clear_irq(gsdevno);

	// set the DMA WR timeout  -- 0x1000 * 250 us = 1.024 s for PCIx
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

	// map a page to set up PSI PMC-GigaSTaR card on a fresh computer reboot
	// otherwise not needed
	gs_rx_dma_setup(gsdevno, 0x1000);

	// enable interrupt using the mask set above
	gs_set_control(gsdevno, GS_INT_ENABLE);

	// enable dma write to host
	gs_set_control(gsdevno, GS_FIRE_DMA_WRITE);
	ts = gs_time();

	// capture status
	t=gs_irq_src_status(gsdevno);
	while(tt<2.0 && !(t & GS_DMA_WR_DIED_INT))
		{
		t=gs_irq_src_status(gsdevno);
		tt = gs_time() - ts;
		usleep(1);
		}
	if (tt < 2.0)
		dcp = tt/0x1000;
	else
		{
		printf("*** Could not get dma_clock_period\n");
		ERROR_RETURN;
		}
	DBG(3, "dma_clock_period device no. %d = %.6lf\n", gsdevno, dcp)
	sprintf(line, "Maximum DMA timeout device no. %d: %.1lf s",gsdevno, 0xffff*dcp);
//	printf("%s\n", line);
	DBG(3, "%s\n", line)
	log_version(line);
	dma_clock_period = min(dma_clock_period, dcp);
	}
sprintf(line, "Maximum DMA timeout = %.1lf s", 0xffff*dma_clock_period);
printf("%s\n", line);
DBG(3, "%s\n", line)
log_version(line);


// silently ignore typical startup errors
for(gsdevno=0; gsdevno<dcb_count; gsdevno++)
	{
	dcb_write_command_no_ack(gsdevno, DCB_STOP);
	for(i=n=0; i<500 && !n; i++)		// wait a little
		n=gs_rx_data(gsdevno, &t, 4);
	t = DCB_WRITE | DCB_BANK_ADD_REG | 0x7;
	gs_tx_data(gsdevno, &t, 4);			// ignore a frequent startup error
	for(i=n=0; i<500 && !n; i++)		// wait a little
		n=gs_rx_data(gsdevno, &t, 4);
	}

#if ENABLE_BCBS

for(gsdevno=0; gsdevno<dcb_count; gsdevno++)
	{
	printf("---\n");

	// stop dcb image mode & reset tx enable
	gs_clear_irq(gsdevno);
	dcb_write_command(gsdevno, DCB_STOP, 0);

	// reset PMC gigastar, DCB & BCBs
	dcb_bcb_reset_full(gsdevno);

	// check the DCB fifo status
	dword = dcb_read_register(gsdevno, DCB_FIFO_STATUS_REG);
	if ( !(dword & DCB_INT_FIFO_EMPTY) )
		printf("*** DCB internal fifo not empty\n");

	// read the version number a few times (doesn't always work 1st time)
	for (i=0; i<5; i++)
		dword = dcb_read_register(gsdevno, DCB_VERSION_REG);
	if (dword)
		{
		sprintf(line, "PMC GigaSTaR device #%d: DCB Hardware/Firmware version: 0x%.4x", 
				gsdevno, 0xffff & dword);
		printf("%s\n", line);
		DBG(3, "%s\n", line)
		log_version(line);
		}
	else
		{
		printf("*** Device #%d *** Could not read the DCB firmware version\n",
				gsdevno);
		ERROR_RETURN;
		}

	// read the build
	dword = dcb_read_register(gsdevno, DCB_FIRMWARE_BUILD_REG);
	if (dword)
		{
		sprintf(line, "PMC GigaSTaR device #%d: DCB Firmware build: 0x%.4x", 
				gsdevno, dword);
		printf("%s\n", line);
		DBG(3, "%s\n", line)
		log_version(line);
		}
	else
		{
		printf("*** Device #%d *** Could not read the DCB firmware build\n",
				gsdevno);
		ERROR_RETURN;
		}

#ifdef OVERRIDE_DCB_BUILD_CONTROL
	if (dcb_firmware_build != OVERRIDE_DCB_BUILD_CONTROL)
		{
		printf("Overriding DCB build: found 0x%.4x; using 0x%.4x\n",
				dcb_firmware_build, OVERRIDE_DCB_BUILD_CONTROL);
		dword = OVERRIDE_DCB_BUILD_CONTROL;
		}
#endif

	if (dcb_firmware_build && dcb_firmware_build!=dword)
		{
		printf("Error: inconsistent DCB build numbers\n");
		ERROR_RETURN;
		}
	else
		dcb_firmware_build = dword;

	switch (dcb_firmware_build)		// multi-module (with BCB) only
		{
		case 0x0a10:		// old, multi-module
		case 0x0a9d:		// multi-extt, multi-module
		case 0x0ca8:		// multi-extt, multi-module
		case 0x0d30:		// 32-bit nexpf, multi-module
		case 0x10f9:		// use with BCB build 10f8, multi-module
		case 0x165c:		// port to Xilinx tools, use with BCB build 0x161c, multi-module
		case 0x1a3c:		// 45C temp limit chan 2, use with BCB build 0x161c, multi-module
		case 0x1ac8:		// 45C limit chan 2, power up enable depends on DIP SW4, multi-module
		case 0x1b60:		// DCB_MIN_READ_TIME_REG R/W, 3 T sensors, 15C lo T lim, 12 bad reads needed, multi-module
		case 0x1bb8:		// 30% RH max on chan. 2
			break;
		default:
			printf("*** Error:  DCB firmware (0x%x) is not supported\n", dcb_firmware_build);
			ERROR_RETURN;
			break;
		}

	// check consistency of DCB type
	dword = DCB_DCB_TYPE & dcb_read_register(gsdevno, DCB_STATUS_REG);
	if ((dword && !MULTI_MODULE_DETECTOR) || (!dword && MULTI_MODULE_DETECTOR))
		{
		printf("*** Error: DCB type does not match detector geometry\n");
		ERROR_RETURN;
		}

	// clear the DCB control register
	dcb_clear_register_bits(gsdevno, DCB_CTRL_REG,
					DCB_REDUCED_DYNAMIC_RANGE |
					DCB_REDUCED_DYN_RANGE_ENB |
					DCB_DELAY_SELECT |
					DCB_ENABLE_IMAGE_TX |
					DCB_ENB_MULTI_EXT_TRIGGER |
					DCB_EXT_EXP_DEBOUNCE_ENB |
					DCB_EXPOSURE_CONTROL |
					DCB_EXT_TRIGGER_CONTROL |
					DCB_SHT_SENSOR_HEATER |
					DCB_SHT_SENSOR_RESOL |
					DCB_REF_COUNTER_ENB 
					);

	// report some DCB settings and set parameters
	sprintf(line, "DCB DOUT sampling delay setting (hardware, unit %d) = 0x%x", gsdevno, 
		dcb_get_delay_setting(gsdevno) );
	printf("%s\n", line);
	log_version(line);
	sprintf(line, "GigaSTaR equalization (unit %d) is set to %d", gsdevno,
			(DCB_GIGASTAR_EQU_ENB_SW & dcb_get_dip_switch_setting(gsdevno)) ? 1 : 0);
	printf("%s\n", line);
	log_version(line);
	f = dcb_read_register(gsdevno, DCB_CLOCK_FREQ_REG);
	if (f > 0)
		{
		if (f==67)
			sprintf(line, "DCB %d design frequency = 66 2/3 MHz", gsdevno);
		else
			sprintf(line, "DCB %d design frequency = %d MHz", gsdevno, f);
		printf("%s\n", line);
		DBG(3, "%s\n", line)
		log_version(line);
		}
	else
		{
		printf("*** Error: Invalid DCB design frequency\n");
		return -1;
		}
	switch (dcb_firmware_build)		// multi-module (with BCB) only
		{
		// older versions
		case 0x0a10:		// old, multi-module
		case 0x0a9d:		// multi-extt, multi-module
		case 0x0ca8:		// multi-extt, multi-module
		case 0x0d30:		// 32-bit nexpf, multi-module
		case 0x10f9:		// use with BCB build 10f8, multi-module
		case 0x165c:		// uses Xilinx tools, use with BCB build 0x161c, multi-module
		case 0x1a3c:		// 45C temp limit chan 2, use with BCB build 0x161c, multi-module
			break;
		// newer versions starting with 0x1ac8
		default:
			sprintf(line, "Power board remote enable (unit %d) is set to %d", gsdevno,
					(DCB_PWR_BOARD_RMT_ENB_SW & dcb_get_dip_switch_setting(gsdevno)) ? 1 : 0);
			printf("%s\n", line);
			log_version(line);
			break;
		}

	dcb_set_module_address(gsdevno, 1);		// select module 1, e.g.

	for (n=1; n<=NBANK; n++)				// enumerate banks
		{
		if (gsdevno != dcb_get_gsdevno(n, n))
			continue;

/*
** During detector assembly, banks may not have addresses starting at 1
** Find the nth bank in the enable pattern.  We assume the banks are
** contiguoulsy addressed after that.  We assume only DCB 1 has this odd
** configuration.
*/
		i=n;
#if defined USE_BCB
		if ( BANKENP1 && !(BANKENP1&1) )		// if bank 1 not enabled
			{
			for(i=1; !(BANKENP1 & (1<<(i-1))); i++)
				;
			i += n-1;
			}
#endif
		dcb_set_bank_address(gsdevno, i);

		dword = dcb_read_register(gsdevno, BCB_VERSION_REG);
		if (dword)
			{
			sprintf(line, "PMC GigaSTaR device #%d: BCB %d Hardware/Firmware version: 0x%.4x", 
					gsdevno, i, 0xffff & dword);
			printf("%s\n", line);
			DBG(3, "%s\n", line)
			log_version(line);
			}
		else
			printf("*** Device #%d *** Could not read the BCB %d firmware version\n",
					gsdevno, i);

		dword = dcb_read_register(gsdevno, BCB_FIRMWARE_BUILD_REG);
		if (dword)
			{
			sprintf(line, "PMC GigaSTaR device #%d: BCB %i Firmware build: 0x%.4x", 
					gsdevno, i, dword);
			printf("%s\n", line);
			DBG(3, "%s\n", line)
			log_version(line);
			}
		else
			{
			printf("*** Device #%d *** Could not read the BCB %i firmware build\n",
					gsdevno, i);
			ERROR_RETURN;
			}

#ifdef OVERRIDE_BCB_BUILD_CONTROL
		if (bcb_firmware_build != OVERRIDE_BCB_BUILD_CONTROL)
			{
			sprintf(line, "Overriding BCB build: found 0x%.4x; using 0x%.4x\n",
					bcb_firmware_build, OVERRIDE_BCB_BUILD_CONTROL);
			printf("%s\n", line);
			log_version(line);
			}
		dword = OVERRIDE_BCB_BUILD_CONTROL;
#endif

		if (bcb_firmware_build && bcb_firmware_build!=dword)
			{
			printf("*** Device #%d *** BCB %i inconsistent build numbers\n",
					gsdevno, i);
			ERROR_RETURN;
			}
		else
			bcb_firmware_build = dword;

		switch (bcb_firmware_build)				// determine DCB - BCB compatibility
			{
			case 0xa0c:
				switch (dcb_firmware_build)		// multi-module only
					{
					case 0x0a10:				// old, multi-module
					case 0x0a9d:				// multi-extt, multi-module
					case 0x0ca8:				// multi-extt, multi-module
						break;
					case 0x0d30:				// 32-bit nexpf, multi-module - DCB
					case 0x10f9:				// use with BCB build 10f8, multi-module
					case 0x165c:				// port to Xilinx tools, use with BCB build 0x161c, multi-module
					case 0x1a3c:				// 45C temp limit chan 2, use with BCB build 0x161c, multi-module
					case 0x1ac8:				// 45C limit chan 2, power up enable depends on DIP SW4, multi-module
					case 0x1b60:				// DCB_MIN_READ_TIME_REG R/W, 3 T sensors, 15C lo T lim, 12 bad reads needed, multi-module
					case 0x1bb8:				// 30% RH max on chan. 2
						printf("*** Error: BCB - DCB firmware mismatch\n");
						ERROR_RETURN;
						break;
					default:
						printf("*** Missing case(A): dcb_firmware_build = 0x%.4x\n", dcb_firmware_build);
						break;
					}
				break;
			case 0xd30:							// 32-bit nexpf - BCB
				switch (dcb_firmware_build)		// multi-module only
					{
					case 0x0a10:				// old, multi-module
					case 0x0a9d:				// multi-extt, multi-module
					case 0x0ca8:				// multi-extt, multi-module
					case 0x10f9:				// use with BCB build 10f8, multi-module
					case 0x165c:				// port to Xilinx tools, use with BCB build 0x161c, multi-module
					case 0x1a3c:				// 45C temp limit chan 2, use with BCB build 0x161c, multi-module
					case 0x1ac8:				// 45C limit chan 2, power up enable depends on DIP SW4, multi-module
					case 0x1b60:				// DCB_MIN_READ_TIME_REG R/W, 3 T sensors, 15C lo T lim, 12 bad reads needed, multi-module
					case 0x1bb8:				// 30% RH max on chan. 2
						printf("*** Error: BCB - DCB firmware mismatch\n");
						ERROR_RETURN;
					case 0x0d30:				// 32-bit nexpf, multi-module - DCB
						break;
					default:
						printf("*** Missing case(A): dcb_firmware_build = 0x%.4x\n", dcb_firmware_build);
						break;
					}
				break;
			case 0x10f8:		// use with DCB build 10F9
				switch (dcb_firmware_build)		// multi-module only
					{
					case 0x0a10:				// old, multi-module
					case 0x0a9d:				// multi-extt, multi-module
					case 0x0ca8:				// multi-extt, multi-module
					case 0x0d30:				// 32-bit nexpf, multi-module - DCB
					case 0x165c:				// port to Xilinx tools, use with BCB build 0x161c, multi-module
					case 0x1a3c:				// 45C temp limit chan 2, use with BCB build 0x161c, multi-module
					case 0x1ac8:				// 45C limit chan 2, power up enable depends on DIP SW4, multi-module
					case 0x1b60:				// DCB_MIN_READ_TIME_REG R/W, 3 T sensors, 15C lo T lim, 12 bad reads needed, multi-module
					case 0x1bb8:				// 30% RH max on chan. 2
						printf("*** Error: BCB - DCB firmware mismatch\n");
						ERROR_RETURN;
						break;
					case 0x10f9:				// use with BCB build 10f8, multi-module
						break;
					default:
						printf("*** Missing case(A): dcb_firmware_build = 0x%.4x\n", dcb_firmware_build);
						break;
					}
				break;
			case 0x161c:		// use with DCB build 0x165c
				switch (dcb_firmware_build)		// multi-module only
					{
					case 0x0a10:				// old, multi-module
					case 0x0a9d:				// multi-extt, multi-module
					case 0x0ca8:				// multi-extt, multi-module
					case 0x0d30:				// 32-bit nexpf, multi-module - DCB
					case 0x10f9:				// use with BCB build 10f8, multi-module
						printf("*** Error: BCB - DCB firmware mismatch\n");
						ERROR_RETURN;
						break;
					case 0x165c:				// port to Xilinx tools, use with BCB build 0x161c, multi-module
					case 0x1a3c:				// 45C temp limit chan 2, use with BCB build 0x161c, multi-module
					case 0x1ac8:				// 45C limit chan 2, power up enable depends on DIP SW4, multi-module
					case 0x1b60:				// DCB_MIN_READ_TIME_REG R/W, 3 T sensors, 15C lo T lim, 12 bad reads needed, multi-module
					case 0x1bb8:				// 30% RH max on chan. 2
						break;
					default:
						printf("*** Missing case(A): dcb_firmware_build = 0x%.4x\n", dcb_firmware_build);
						break;
					}
				break;
			case 0x1ae8:		// 67 MHz, MIN_READ_TIME_REG, REF_COUNTER - use with DCB build 0x165c++
			case 0x1b3c:		// 100 MHz - use with DCB build 0x165c++
			case 0x1b40:		// 75 MHz - use with DCB build 0x165c++
				switch (dcb_firmware_build)		// multi-module only
					{
					case 0x0a10:				// old, multi-module
					case 0x0a9d:				// multi-extt, multi-module
					case 0x0ca8:				// multi-extt, multi-module
					case 0x0d30:				// 32-bit nexpf, multi-module - DCB
					case 0x10f9:				// use with BCB build 10f8, multi-module
						printf("*** Error: BCB - DCB firmware mismatch\n");
						ERROR_RETURN;
						break;
					case 0x165c:				// port to Xilinx tools, use with BCB build 0x161c, multi-module
					case 0x1a3c:				// 45C temp limit chan 2, use with BCB build 0x161c, multi-module
					case 0x1ac8:				// 45C limit chan 2, power up enable depends on DIP SW4, multi-module
					case 0x1b60:				// DCB_MIN_READ_TIME_REG R/W, 3 T sensors, 15C lo T lim, 12 bad reads needed, multi-module
					case 0x1bb8:				// 30% RH max on chan. 2
						break;
					default:
						printf("*** Missing case(A): dcb_firmware_build = 0x%.4x\n", dcb_firmware_build);
						break;
					}
				break;
			default:
				printf("*** Error:  BCB firmware (0x%.4x) is not supported\n", bcb_firmware_build);
				ERROR_RETURN;
				break;
			}

		f = dcb_read_register(gsdevno, BCB_CLOCK_FREQ_REG);
		if (f > 0)
			{
			sprintf(line, "BCB design frequency = %d MHz", f);
			printf("%s\n", line);
			DBG(3, "%s\n", line)
			log_version(line);
			}
		else
			{
			printf("*** Error: Invalid BCB design frequency\n");
			ERROR_RETURN;
			}

		// set the DOUT sampling delay
		dcb_set_module_address(gsdevno, 7);		// select all modules
		// use software sampling delay
		dcb_clear_register_bits(gsdevno, BCB_CTRL_REG, BCB_DELAY_SELECT);
		// load the sampling delay
		switch (bcb_firmware_build)
			{
			case 0x0a0c:      	// 50MHz - use with DCB build 0A10, 0A9D & 0CA8
			case 0x0d30:		// 32-bit nexpf - use with DCB build 0D30
			case 0x10f8:		// use with DCB build 10F9
			case 0x161c:		// uses Xilinx tools - use with DCB build 0x165c
				dcb_set_delay_setting(gsdevno, DOUT_SAMPLING_DELAY_1);
				break;
			case 0x1ae8:		// 67 MHz, MIN_READ_TIME_REG, REF_COUNTER - use with DCB build 0x165c++
			case 0x1b3c:		// 100 MHz - use with DCB build 0x165c++
			case 0x1b40:		// 75 MHz - use with DCB build 0x165c++
				dcb_set_delay_setting(gsdevno, DOUT_SAMPLING_DELAY_2);
				break;
			default:
				printf("*** Missing case(J): bcb_firmware_build = 0x%.4x\n", bcb_firmware_build);
				break;
			}
		dcb_set_module_address(gsdevno, 1);		// clear select all
		}				// enumerate banks

	dcb_set_bank_module_address(1+gsdevno*NBANK_DCB, 1);	// select any module on this device
	sprintf(line, "DCB #%d: BCB DOUT sampling delays (software) set to 0x%x\n", 
			gsdevno, dcb_get_delay_setting(gsdevno));
	printf("%s\n", line);
	log_version(line);

	// 0x1000 seems to be largest acceptable to the system
	gs_set_rx_burstsize(gsdevno, 0x1000);
	
	// enable temperature / humidity monitor, if present
	for(i=0, flag=0; i<6; i++)
		{
		th_sensing_enabled[i][gsdevno] = 1;
		if (dcb_read_temp(i + 6*gsdevno) <= -39.0)	// failed or non-existent channel
			th_sensing_enabled[i][gsdevno] = 0;
		else
			flag++;
		}
	if (flag)
		{
		for(i=0, n=0; i<6; i++)
			n += th_sensing_enabled[i][gsdevno];
		sprintf(line, "%d Temperature / Humidity sensor(s) enabled on DCB %d", n, gsdevno);
		printf("%s\n", line);
		DBG(3, "%s\n", line)
		log_version(line);
		}
	else
		{
		sprintf(line, "Temperature / Humidity sensing disabled on DCB %d", gsdevno);
		printf("%s\n", line);
		DBG(3, "%s\n", line)
		log_version(line);
		}

	// if there is a humidity sensor, we require 3 good readings over several
	// seconds before proceeding, but 1 reading too high is fatal
#ifndef TH_CONTROL_OVERRIDE
	while(gs_time() < 5.0)		// wait for a valid humidity reading
		sleep(1);
	for(i=0; i<3; i++)
		{
		for(chan=0; chan <6; chan++)
			{
			if (th_sensing_enabled[chan][gsdevno] &&
					((rh = dcb_read_humidity(chan+6*gsdevno))+4.0 > 
						dcb_read_humidity_high_limit(chan+6*gsdevno)))
				{
				sprintf(line, "*** ERROR - Rel. Humidity (%.1f%%, channel %d) is too high",
						rh, chan+6*gsdevno);
				printf("%s\n", line);
				DBG(1, "%s\n", line)
				ERROR_RETURN;
				}
			}
		sleep(2);
		}
#endif

	}		// loop over PMC GigaSTaR cards

// set some defaults from the master DCB
switch (dcb_firmware_build)		// multi-module only
	{
	case 0x165c:		// uses Xilinx tools, use with BCB build 0x161c, multi-module
	case 0x1a3c:		// 45C temp limit chan 2, use with BCB build 0x161c, multi-module
	case 0x1ac8:		// 45C limit chan 2, power up enable depends on DIP SW4, multi-module
	case 0x1b60:		// DCB_MIN_READ_TIME_REG R/W, 3 T sensors, 15C lo T lim, 12 bad reads needed
	case 0x1bb8:		// 30% RH max on chan. 2
		extra_overhead_time = EXTRA_OVERHEAD_TIME;
		break;
	default:
		extra_overhead_time = EXTRA_OVERHEAD_TIME_OLD;
		break;
	}
gsdevno = 0;
dcb_set_bank_module_address(1, 1);
f = dcb_read_register(gsdevno, DCB_CLOCK_FREQ_REG);
if (f==67)
	dcb_clock_period = 1.0 / 66666666.67;
else
	dcb_clock_period = 1.0 / ((double)f * 1.0e6);
f = dcb_read_register(gsdevno, BCB_CLOCK_FREQ_REG);
if (f==67)
	bcb_clock_period = 1.0 / 66666666.67;
else
	bcb_clock_period = 1.0 / ((double)f * 1.0e6);
// set up timings
switch (bcb_firmware_build)
	{
	case 0x0a0c:      	// 50MHz - use with DCB build 0A10, 0A9D & 0CA8
	case 0x0d30:		// 32-bit nexpf - use with DCB build 0D30
	case 0x10f8:		// use with DCB build 10F9
	case 0x161c:		// uses Xilinx tools - use with DCB build 0x165c
		t = 4*dcb_read_register(gsdevno, DCB_MIN_READ_TIME_REG);	// clocks for readout
		overhead_time = (double)t * dcb_clock_period + extra_overhead_time;
		break;
	// new versions have clock info in the BCB
	case 0x1ae8:		// 67 MHz, MIN_READ_TIME_REG, REF_COUNTER - use with DCB build 0x165c++
	case 0x1b3c:		// 100 MHz - use with DCB build 0x165c++
	case 0x1b40:		// 75 MHz - use with DCB build 0x165c++
		t = 4*dcb_read_register(gsdevno, BCB_MIN_READ_TIME_REG);	// clocks for readout
		overhead_time = (double)t * bcb_clock_period + extra_overhead_time;
		break;
	default:
		printf("*** Missing case(J): bcb_firmware_build = 0x%.4x\n", bcb_firmware_build);
		break;
	}
// transfer time to computer
DBG(3, "dcb_clock_period = %.3lf ns, bcb_clock_period = %.3lf ns\n",
		1.0e9*dcb_clock_period, 1.0e9*bcb_clock_period)
transfer_time = (((20 + 97 * 60 * 20) * dcb_clock_period) * 
		NBANK_DCB * NMOD_BANK) + EXTRA_TRANSFER_TIME;
dcb_set_enable_holdoff(extra_overhead_time);
DBG(3, "Setting overhead_time = %.6lf, transfer_time = %.6lf, enable_holdoff = %.6lf\n",
		overhead_time, transfer_time, extra_overhead_time)

// make up the module enable pattern from the bank geometry
mn=1;			// keep the compiler happy
#ifdef MOD_ENB_PATTERN
  modenp = MOD_ENB_PATTERN;
#else
for (modenp=0, mn=1; mn<=NMOD_BANK; mn++)		// modules are numbered 1, 2, 3...
	{
	modenp <<= 1;
	modenp |= 1;
	}
#endif

// set the bank enable patterns and the module enable pattern
// count the bits in the bank patterns as a consistency check

dcb_set_bank_enable_pattern(0, BANKENP1);
for(i=0,n=BANKENP1;i<16;i++,n>>=1)nbits+=(n&1);
dcb_set_module_enable_pattern(0, modenp);

#if (NDCB == 2)
	dcb_set_bank_enable_pattern(1, BANKENP2);
	for(i=0,n=BANKENP2;i<16;i++,n>>=1)nbits+=(n&1);
	dcb_set_module_enable_pattern(1, modenp);
 #if (NDCB == 3)
	dcb_set_bank_enable_pattern(2, BANKENP3);
	for(i=0,n=BANKENP3;i<16;i++,n>>=1)nbits+=(n&1);
	dcb_set_module_enable_pattern(2, modenp);
  #if (NDCB == 4)
	dcb_set_bank_enable_pattern(3, BANKENP4);
	for(i=0,n=BANKENP4;i<16;i++,n>>=1)nbits+=(n&1);
	dcb_set_module_enable_pattern(3, modenp);
  #endif
 #endif
#endif

if (nbits != NBANK)
	{
	printf("*** ERROR: N banks (%d) != N bits in bank enable patterns (%d)\n",
			NBANK, nbits);
	ERROR_RETURN;
	}

/*
** Calibrate the BCB reference clock against the computer clock
*/
switch (bcb_firmware_build)
	{
	default:
		break;
	case 0x1ae8:		// 67 MHz, MIN_READ_TIME_REG, REF_COUNTER - use with DCB build 0x165c++
	case 0x1b3c:		// 100 MHz - use with DCB build 0x165c++
	case 0x1b40:		// 75 MHz - use with DCB build 0x165c++
		dcb_set_register_bits(0, BCB_RESET_REG, BCB_REF_COUNTER_RESET);	// zero counter
		ts = gs_time();
		dcb_set_register_bits(0, BCB_CTRL_REG, BCB_REF_COUNTER_ENB);	// start counter
		sleep(2);
		tn = gs_time();
		dcb_clear_register_bits(0, BCB_CTRL_REG, BCB_REF_COUNTER_ENB);	// stop counter
		bcb_clock_skew_factor = bcb_get_ref_counter_time()/(tn -ts);
		DBG(3, "BCB to CPU clock skew factor = %.9lf\n", bcb_clock_skew_factor);
		break;
	}
/*
** The DCB needs to know the readout time, which now is gotten from the BCB
** This makes the DCB independent of the BCB timing.
*/
switch (dcb_firmware_build)		// multi-module only
	{
	default:
		break;
	case 0x1b60:		// DCB_MIN_READ_TIME_REG R/W, 3 T sensors, 15C lo T lim, 12 bad reads needed
	case 0x1bb8:		// 30% RH max on chan. 2
		// read min readout time (in clocks) from BCB, multiply by the ratio of
		// clock periods to translate value to DCB clocks
		gsdevno = 0;
		bcb_min_read_time = dcb_read_register(gsdevno, BCB_MIN_READ_TIME_REG);
		dcb_min_read_time = (unsigned int)(1.0 + bcb_min_read_time*bcb_clock_period/dcb_clock_period);
		for(gsdevno=0; gsdevno<dcb_count; gsdevno++)
			dcb_write_register(gsdevno, DCB_MIN_READ_TIME_REG, dcb_min_read_time);
		break;
	}


#else		// ------------------ NO BCB's

for(gsdevno=0; gsdevno<dcb_count; gsdevno++)
	{
	// issue a dcb_stop(), but silently ignore the startup errors
	dcb_write_command_no_ack(gsdevno, DCB_STOP);
	for(i=n=0; i<500 && !n; i++)		// wait a little
		n=gs_rx_data(gsdevno, &t, 4);
	t = DCB_WRITE | DCB_BANK_ADD_REG | 0x7;
	gs_tx_data(gsdevno, &t, 4);			// ignore a frequent startup error
	for(i=n=0; i<500 && !n; i++)		// wait a little
		n=gs_rx_data(gsdevno, &t, 4);

	// stop dcb image mode & reset tx enable
	dcb_stop();

	dcb_set_bank_module_address(15, 7);		// select all
	dcb_bcb_reset_full(gsdevno);

	// check the DCB fifo status
	dword = dcb_read_register(gsdevno, DCB_FIFO_STATUS_REG);
	if ( !(dword & DCB_INT_FIFO_EMPTY) )
		printf("*** DCB internal fifo not empty\n");

	// read the version number a few times (doesn't always work 1st time)
	for (i=0; i<5; i++)
		dword = dcb_read_register(gsdevno, DCB_VERSION_REG);
	if (dword)
		{
		sprintf(line, "PMC GigaSTaR device #%d: DCB Hardware/Firmware version: 0x%.4x", 
				gsdevno, 0xffff & dword);
		printf("%s\n", line);
		DBG(3, "%s\n", line)
		log_version(line);
		}
	else
		{
		printf("*** Device #%d *** Could not read the DCB firmware version\n",
				gsdevno);
		ERROR_RETURN;
		}

	// read the build
	dword = dcb_read_register(gsdevno, DCB_FIRMWARE_BUILD_REG);
	if (dword)
		{
		sprintf(line, "PMC GigaSTaR device #%d: DCB Firmware build: 0x%.4x", 
				gsdevno, dword);
		printf("%s\n", line);
		DBG(3, "%s\n", line)
		log_version(line);
		}
	else
		{
		printf("*** Device #%d *** Could not read the DCB firmware build\n",
				gsdevno);
		ERROR_RETURN;
		}

#ifdef OVERRIDE_DCB_BUILD_CONTROL
	if (dcb_firmware_build != OVERRIDE_DCB_BUILD_CONTROL)
		{
		sprintf(line, "Overriding DCB build: found 0x%.4x; using 0x%.4x\n",
				dcb_firmware_build, OVERRIDE_DCB_BUILD_CONTROL);
		printf("%s\n", line);
		log_version(line);
		dword = OVERRIDE_DCB_BUILD_CONTROL;
		}
#endif

	if (dcb_firmware_build && dcb_firmware_build!=dword)
		{
		printf("*** Device #%d *** inconsistent build number\n", gsdevno);
		ERROR_RETURN;
		}
	else
		dcb_firmware_build = dword;

	switch (dcb_firmware_build)		// SINGLE module
		{
  		case 0x08c0:		// before limo swap, single-module
		case 0x096c:		// old-style fillpix, single-module
		case 0x0a11:		// new-style fillpix, single-module
		case 0x0a9c:		// multi-extt, single-module
		case 0x0d31:		// 32-bit nexpf, single-module
		case 0x0d78:		// improved multi-trigger, single-module
		case 0x0d94:		// enable holdoff, single-module
		case 0x0d98:		// optional parallel write, single-module
		case 0x10f8:		// temp & hdty lower limits, single module
		case 0x1658:		// port to Xilinx tools, single module
		case 0x1ad0:		// corrected DCB_MIN_READ_TIME_REG, single module
		case 0x1c54:		// T: 55/15, 45/15, 45/15, H: 80, 80, 30, single-module
			break;
		case 0x0a10:		// old, multi-module
		case 0x0a9d:		// multi-extt, multi-module
		case 0x0ca8:		// multi-extt, multi-module
		case 0x0d30:		// 32-bit nexpf, multi-module
		case 0x10f9:		// use with BCB build 10f8, multi-module
		case 0x165c:		// port to Xilinx tools, use with BCB build 0x161c, multi-module
		case 0x1a3c:		// 45C temp limit chan 2, use with BCB build 0x161c, multi-module
		case 0x1ac8:		// 45C limit chan 2, power up enable depends on DIP SW4, multi-module
		case 0x1b60:		// DCB_MIN_READ_TIME_REG R/W, 3 T sensors, 15C lo T lim, 12 bad reads needed, multi-module
		case 0x1bb8:		// 30% RH max on chan. 2
			printf("*** Error:  Firmware (0x%.4x) is multi-module only\n", dcb_firmware_build);
			ERROR_RETURN;
			break;
		default:
			printf("*** Error:  Firmware (0x%.4x) is not supported\n", dcb_firmware_build);
			ERROR_RETURN;
			break;
		}

	// clear most of the DCB control register
	dcb_clear_register_bits(gsdevno, DCB_CTRL_REG,
					DCB_REDUCED_DYNAMIC_RANGE |
					DCB_REDUCED_DYN_RANGE_ENB |
			/*		DCB_DELAY_SELECT |				*/
					DCB_ENABLE_IMAGE_TX |
					DCB_ENB_MULTI_EXT_TRIGGER |
					DCB_EXT_EXP_DEBOUNCE_ENB |
					DCB_EXPOSURE_CONTROL |
					DCB_EXT_TRIGGER_CONTROL |
					DCB_SHT_SENSOR_HEATER |
					DCB_SHT_SENSOR_RESOL |
					DCB_REF_COUNTER_ENB 
					);

	// enable temperature / humidity monitor, if present
	for(i=0, flag=0; i<6; i++)
		{
		th_sensing_enabled[i][gsdevno] = 1;
		// sometimes dcb_read_temp() returns -39.64 rather than -99.0
		if (dcb_read_temp(i + 6*gsdevno) <= -39.0)
			th_sensing_enabled[i][gsdevno] = 0;
		else
			flag++;
		}
	if (flag)
		{
		for(i=0, n=0; i<6; i++)
			n += th_sensing_enabled[i][gsdevno];
		sprintf(line, "%d Temperature / Humidity sensor(s) enabled on DCB %d", n, gsdevno);
		printf("%s\n", line);
		DBG(3, "%s\n", line)
		log_version(line);
		}
	else
		{
		sprintf(line, "Temperature / Humidity sensing disabled on DCB %d", gsdevno);
		printf("%s\n", line);
		DBG(3, "%s\n", line)
		log_version(line);
		}

	// report some DCB settings and set operating parameters
	sprintf(line, "DCB DOUT sampling delay setting (hardware, unit %d) = 0x%x", gsdevno, 
			dcb_get_delay_setting(gsdevno) );
	printf("%s\n", line);
	log_version(line);
	sprintf(line, "GigaSTaR line equalization (unit %d) is set to %d", gsdevno,
			(DCB_GIGASTAR_EQU_ENB_SW & dcb_get_dip_switch_setting(gsdevno)) ? 1 : 0);
	printf("%s\n", line);
	DBG(3, "%s\n", line)
	log_version(line);
	switch (dcb_firmware_build)		// single-module only
		{
		case 0x1658:		// uses Xilinx tools, single-module
		case 0x1ad0:		// corrected DCB_MIN_READ_TIME_REG, single-module
		case 0x1c54:		// T: 55/15, 45/15, 45/15, H: 80, 80, 30, single-module
			extra_overhead_time = EXTRA_OVERHEAD_TIME;
			break;
		default:
			extra_overhead_time = EXTRA_OVERHEAD_TIME_OLD;
			break;
		}

#ifndef TH_CONTROL_OVERRIDE
	while(gs_time() < 5.0)		// wait for a valid humidity reading
		sleep(1);
	for(i=0; i<3; i++)
		{
		for(chan=0; chan <6; chan++)
			{
			if (th_sensing_enabled[chan][gsdevno] &&
					((rh = dcb_read_humidity(chan+6*gsdevno))+4.0 > 
						dcb_read_humidity_high_limit(chan+6*gsdevno)))
				{
				sprintf(line, "*** ERROR - Rel. Humidity (%.1f%%, channel %d) is too high",
						rh, chan+6*gsdevno);
				printf("%s\n", line);
				DBG(1, "%s\n", line)
				ERROR_RETURN;
				}
			}
		sleep(2);
		}
#endif

	f = dcb_read_register(gsdevno, DCB_CLOCK_FREQ_REG);
	if (f > 0)
		{
		if (f==67)
			sprintf(line, "DCB %d design frequency = 66 2/3 MHz", gsdevno);
		else
			sprintf(line, "DCB %d design frequency = %d MHz", gsdevno, f);
		printf("%s\n", line);
		DBG(3, "%s\n", line)
		log_version(line);
		}
	else
		{
		printf("*** Error: Invalid DCB design frequency\n");
		ERROR_RETURN;
		}
	if (f==67)
		dcb_clock_period = 1.0 / 66666666.66;
	else
		dcb_clock_period = 1.0 / ((double)f * 1.0e6);
	t = 4*dcb_read_register(gsdevno, DCB_MIN_READ_TIME_REG);	// clocks for readout
	overhead_time = (double)t * dcb_clock_period + extra_overhead_time;
	transfer_time = 0;
	dcb_set_enable_holdoff(extra_overhead_time);
	DBG(3, "Setting overhead_time = %.6lf\n", overhead_time)

	dcb_set_delay_setting(gsdevno, 0);		// restore hardware delay (single module)

	mn=1;			// keep the compiler happy
	modenp=0;
	}		// loop over PMC GigaSTaR cards

#endif		// MULTI_MODULE_DETECTOR

/*
** Verify that required temperature/humidity sensors have been found
*/
#if defined N_TH_SENSORS && !defined TH_CONTROL_OVERRIDE
for(gsdevno=0, n=0; gsdevno<dcb_count; gsdevno++)
	for(i=0; i<6; i++)
		if (th_sensing_enabled[i][gsdevno])
			n++;
if (n < N_TH_SENSORS)
	{
	sprintf(line, "*** ERROR - %d required temp / humidity sensors not found", N_TH_SENSORS);
	printf("%s\n", line);
	DBG(1, "%s\n", line)
	ERROR_RETURN;
	}
#endif

/*
** Calibrate the system clock drift against the dcb clock.
**
** The CPU clock runs slightly slower than the DCB crystal oscillator - that is,
** in a given time interval, the CPU clock records fewer nanoseconds than the
** DCB.  Since the DCB clock is not usually available, we develop a drift
** compensation factor to apply to our internal clock.
*/
	dcb_set_register_bits(0, DCB_RESET_REG, DCB_REF_COUNTER_RESET);	// zero counter
	ts = gs_time();
	dcb_set_register_bits(0, DCB_CTRL_REG, DCB_REF_COUNTER_ENB);	// start counter
	sleep(2);
	tn = gs_time();
	dcb_clear_register_bits(0, DCB_CTRL_REG, DCB_REF_COUNTER_ENB);	// stop counter
	clock_skew_factor = dcb_get_ref_counter_time()/(tn -ts);

	//printf("DCB to CPU clock skew factor = %.9lf\n", clock_skew_factor);
	DBG(3, "DCB to CPU clock skew factor = %.9lf\n", clock_skew_factor);

dcb_write_data_allpix(0xfffff);				// default fill

#if 0
if (fabs(1.0-clock_skew_factor)>0.05)
	{double rct;
	FILE *ifp;
	char line[150];
	printf("Systime = %s\n", timestamp());
	dcb_set_register_bits(0, DCB_RESET_REG, DCB_REF_COUNTER_RESET);	// zero counter
	ts = gs_time();
	dcb_set_register_bits(0, DCB_CTRL_REG, DCB_REF_COUNTER_ENB);	// start counter
	sleep(2);
	tn = gs_time();
	dcb_clear_register_bits(0, DCB_CTRL_REG, DCB_REF_COUNTER_ENB);	// stop counter
	rct = dcb_get_ref_counter_time();
	clock_skew_factor = rct/(tn -ts);
	printf("Systime = %s, rct = %.6lf, tdiff = %.6lf\n", 
			timestamp(), rct, (tn -ts));
printf("DCB to CPU clock skew factor repeat = %.9lf\n", clock_skew_factor);
DBG(3, "DCB to CPU clock skew factor repeat = %.9lf\n", clock_skew_factor);
	ifp=fopen("/proc/cpuinfo", "r");
	while (fgets(line, sizeof(line), ifp))
		if (!strncmp(line, "cpu MHz", strlen("cpu MHz")))
			break;
	fclose(ifp);
	printf("%s", line);
	}
#endif

// result of this was the discovery that the "67" MHz design runs at 66,666,666 Hz
#if 0					// compare timers by watching
{double tt=0.0, rct, rctl=-1.0, tdiff=0.0, gst;
struct timeval tv;
struct timezone tz;
printf("\nTimer comparison:    gs_time                dcb_time\n");
printf("%12.6lf\n", 0.0);
gettimeofday(&tv, &tz);
ts = (double)tv.tv_sec + (double)tv.tv_usec/1e6;		// start time
gs_set_t0(0.0);											// zero the gstar clock
dcb_set_register_bits(0, DCB_RESET_REG, DCB_REF_COUNTER_RESET);	// zero counter
dcb_set_register_bits(0, DCB_CTRL_REG, DCB_REF_COUNTER_ENB);	// start counter
sleep(5);
while(tdiff<100.0)
	{
	gettimeofday(&tv, &tz);
	tdiff = ((double)tv.tv_sec + (double)tv.tv_usec/1e6)-ts;
	gst = gs_time();
	rct = dcb_get_ref_counter_time();
	if (rct<rctl)						// compensate wrap-around of 32-bit counter
		tt+=dcb_clock_period*(double)(1LL<<32);
	rctl=rct;
	printf("%12.6lf  %12.6lf (%6.3lf%%)  %12.6lf (%6.3lf%%)\n", 
		tdiff, gst, 100.0*(gst-tdiff)/tdiff, rct+tt, 100.0*(rct+tt-tdiff)/tdiff);
	sleep(5);
	}
dcb_clear_register_bits(0, DCB_CTRL_REG, DCB_REF_COUNTER_ENB);	// stop counter
}
#endif

// set some defaults on the master DCB
dcb_set_exposure_time(1.0);
dcb_set_exposure_period(1.05);
dcb_set_exp_trigger_delay(0.0, -1);
dcb_set_exposure_count_limit(1);
dcb_set_tx_frame_limit(1);
dcb_set_debounce_time(0.0);
dcb_set_multi_trigger(0);
first_exposure_flag=0;

dcbs_are_initialized=1;

// power up the modules - it is important that the DACs be set soon after
// no need to test SW4
switch (dcb_firmware_build)
	{
	// multi-module systems (with BCB)
	case 0x1ac8:		// 45C limit chan 2, power up enable depends on DIP SW4, multi-module
	case 0x1b60:		// DCB_MIN_READ_TIME_REG R/W, 3 T sensors, 15C lo T lim, 12 bad reads needed, multi-module
	case 0x1bb8:		// 30% RH max on chan. 2
	// single module(s) systems (i.e., no BCBs)
	case 0x1c54:		// T: 55/15, 45/15, 45/15, H: 80, 80, 30, single-module
		printf("... Enabling module power supplies\n");
		for(gsdevno=0; gsdevno<dcb_count; gsdevno++)
			dcb_set_register_bits(gsdevno, DCB_CTRL_REG, DCB_MODULE_POWER_UP);
		break;
	default:
		break;
	}

return 0;
}




static void dcb_bcb_reset_full(int gsdevno)
{
TDBG("--- dcb_bcb_reset_full()\n")

// clear the PMC gigastar
gs_set_control(gsdevno, GS_RES_RX_FIFO);
gs_clear_irq(gsdevno);

// reset DCB, but not dcb gigstar (causes link error)
// one could equally use dcb_set_register_bits() here
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
		DCB_SHT_COMM_RESET |
		DCB_AUTO_FIFO_READ_RESET
		);

#if ENABLE_BCBS

dcb_set_bank_module_address(15, 7);		// select all
dcb_write_register(gsdevno, BCB_RESET_REG, BCB_RESET_ALL);

#endif

return;
}




static void dcb_bcb_reset(int gsdevno)
{
TDBG("--- dcb_bcb_reset()\n")

// clear the PMC gigastar
gs_set_control(gsdevno, GS_RES_RX_FIFO);
gs_clear_irq(gsdevno);

// reset DCB, but not dcb gigstar (causes link error)
// one could equally use dcb_set_register_bits() here
dcb_write_register(gsdevno, DCB_RESET_REG,
	/*	DCB_TX_FIFO_RESET |		may not work at 50 MHz */
	/*	DCB_RX_FIFO_RESET |		may not work at 50 MHz */
		DCB_INT_FIFO_RESET |
		DCB_SEQUENCE_GEN_RESET |
		DCB_CONVERT_RESET |
		DCB_TRIM_FIFO_RESET |
		DCB_FRAME_COUNT_RESET |
		DCB_BOARD_COMM_RESET |
	/*	DCB_SHT_SOFT_RESET |	blocks T shutdown for 1 sec. */
	/*	DCB_SHT_COMM_RESET			*/
		DCB_AUTO_FIFO_READ_RESET
		);

# if ENABLE_BCBS

dcb_set_bank_module_address(15, 7);		// select all
dcb_write_register(gsdevno, BCB_RESET_REG, BCB_RESET_ALL);

#endif

return;
}




/*
**  dcb_prog_i2c() -- program either a dac voltage or chsel pattern
**  on an mcb 
**  the pmc gigastar devno is determined from the bank & module address
**  using the declared geometry of the detector
*/
int dcb_prog_i2c(int bank, int module, unsigned int command,
		unsigned int data)
{
int devno, i;

TDBG("--- dcb_prog_i2c()\n")

switch (command)		// sanity check
	{
	case DCB_I2C_DAC1:
	case DCB_I2C_DAC2:
	case DCB_I2C_CHSEL:
		break;
	default:
		printf("Unknown i2c command in dcb_prog_i2c: 0x%x\n", command);
		return -1;
	}

if (bank != MAX_BANK_ADD)
	{
	// determine the pmc gigastar device number from the bank
	devno = dcb_get_gsdevno(bank, module);

	// set the bank and module
	dcb_set_bank_module_address(bank, module);

	return dcb_write_command(devno, command, (0xffffff & data) );
	}

#if defined SLAVE_COMPUTER && defined WIDE_FAST
if (module != MAX_MOD_ADD)
	{
	devno = dcb_get_gsdevno(module, module);

	return dcb_write_command(devno, command, (0xffffff & data) );
	}
#endif

// bank == MAX_BANK_ADD - special case for all banks on all dcb's
dcb_set_bank_module_address(bank, module);
for (i=1; i<=NDCB*NBANK_DCB; i+=NBANK_DCB)		// step thru dcb's
	{
	devno = dcb_get_gsdevno(i, i);

	if (dcb_write_command(devno, command, (0xffffff & data)) )
		return -1;
	}

return 0;
}



/*
**  dcb_start_read_image() -- start readout of a detector image
**  Set up the first module, then use dcb_wait_read_image()
*/
int dcb_start_read_image(void)
{
int gsdevno;

TDBG("--- dcb_start_read_image() -----------------------------------\n")

exposure_time = 0.0;					// flag that this is not an exposure
measured_exposure_time = 0.0;
gs_set_t0(0.0);							// synchronize clocks
last_readout_enabled=0;
dcb_reset_exposure_mode();

dcb_set_bank_module_address(15, 7);		// select all
for(gsdevno=0; gsdevno<NDCB; gsdevno++)
	dcb_bcb_reset(gsdevno);

for(gsdevno=0; gsdevno<NDCB; gsdevno++)
	{
	# if MULTI_MODULE_DETECTOR

	 if (!dcb_read_banks(gsdevno))		// set up dcb reads
		 return -1;

	#else		// single module

	if ( !dcb_read_module(0, 1, 1) )	// start start first module read
		return -1;

	#endif
	}

// keep the compiler happy - one or the other of these is unused
if(0) dcb_read_banks(0);
if(0) dcb_read_module(0, 1, 1);

return 0;
}




/*
**  dcb_expose() - setup & start an exposure
**  use with dcb_wait_read_image() to make exposures
*/
#define TBRK 4.0		// time break point, sec
int dcb_expose(void)
{
int gsdevno=0, timeout, i;
double frame_time;

TDBG("--- dcb_expose()\n")

dcb_set_bank_module_address(15, 7);		// select all

// the following setup is not repeated during a multi-exposure
if (!autoFrame)
	{
	exposure_time = dcb_get_exposure_time();
	exposure_period = dcb_get_exposure_period();
	if (external_enable && exposure_time>=exposure_period)
		exposure_time = exposure_period - 10.0e-9;
	nexppf = max(1, dcb_get_exposure_count_limit());	// n exposures per frame
	if (exposure_time <= 0.0)
		{
		printf ("Illegal exposure time: %.5lf\n", exposure_time);
		return -1;
		}
	tx_frame_limit = dcb_get_tx_frame_limit();

	TDBG("--- Settings: exp lim %u, frame lim %d\n",\
	dcb_get_exposure_count_limit(), tx_frame_limit);

	if (!external_enable && !external_trigger && 
			(exposure_time+overhead_time > nexppf*exposure_period))
		{
		printf("*** Period is too short for the exposure time\n");
		return 1;
		}

	for (i=0; i<NDCB; i++)
		dcb_bcb_reset(i);
	measured_exposure_time = 0.0;
	first_exposure_flag=1;

	frame_time = exposure_period * exp_count_in_frame + 
			exposure_time + overhead_time;

	total_sequence_time =
			(exposure_period * exp_count_in_frame)*(tx_frame_limit -1) +
			exposure_period * (exp_count_in_frame-1) + exposure_time;
	last_readout_enabled=0;

	// gigastar timeout is multiples of ~250 us (~125 us with PCIe)
	// see below how >TBRK sec is treated differently
	// timeout == 0xffff corresponds to ~16.4 sec (8.2 sec with PCIe)
	// N.B. above we selected the shortest dma_clock_period, if >1 found
	if (frame_time >= TBRK)
		dma_timeout = (int)((1.0 + TBRK)/dma_clock_period);
	else if (frame_time >= TBRK/2.0)
		dma_timeout = (int)((1.0 + frame_time)/dma_clock_period);
	else
		dma_timeout = (int)((0.2 + 2.0 * frame_time)/dma_clock_period);
	dma_timeout = min(dma_timeout, 0xffff);

	// for external trigger, only the initial timeout is lengthened,
	// except in multi-trigger mode
	// for external enable, timeout is always lenghtened
	if (external_enable)
		timeout = dma_timeout = 0xffff;
	else if (external_trigger || multi_trigger)
		timeout = 0xffff;
	else
		timeout = dma_timeout;

	// set the DMA WR timeout - only device 0 is used for timing
	gs_set_rx_timeout(0, timeout);
	TDBG("--- Set DMA timeout (A), dev 0 = %f (0x%x)\n",
			(float)timeout*dma_clock_period, timeout)
	// other dcb's
	timeout += 0x100;		// a little longer to avoid race
	timeout = min(timeout, 0xffff);
	for (i=1; i<NDCB; i++)
		gs_set_rx_timeout(i, timeout);
	TDBG("Timing margin (calculated for 50 MHz design) = %.6lf\n", 
			exposure_period-exposure_time-overhead_time-0.000023)

#if INDEPENDENT_DCBs
	if (external_enable)
		// set external enable on the other dcb's
		for (i=1; i<NDCB; i++)
			dcb_set_register_bits(i, DCB_CTRL_REG, DCB_EXPOSURE_CONTROL);
	else if (external_trigger || multi_trigger)
		// set external trigger on the other dcb's
		for (i=1; i<NDCB; i++)
			dcb_set_register_bits(i, DCB_CTRL_REG, DCB_EXT_TRIGGER_CONTROL);
#else
	// set external enable on the other dcb's unless trigger_offset_mode
	if (trigger_offset_mode)
		for (i=1; i<NDCB; i++)
			dcb_set_register_bits(i, DCB_CTRL_REG, DCB_EXT_TRIGGER_CONTROL);
	else
		for (i=1; i<NDCB; i++)
			dcb_set_register_bits(i, DCB_CTRL_REG, DCB_EXPOSURE_CONTROL);
#endif

// enable master dcb
	if (external_enable)
		dcb_set_register_bits(gsdevno, DCB_CTRL_REG, DCB_EXPOSURE_CONTROL);
	else if (external_trigger)
		dcb_set_register_bits(gsdevno, DCB_CTRL_REG, DCB_EXT_TRIGGER_CONTROL);
	if (multi_trigger)
		dcb_arm_multi_trigger();
	}


// enable master dcb last
for (gsdevno = -1+NDCB; gsdevno>=0; gsdevno--)
	{
	// set interrupt on RX dma (i.e., dma write) finished & errors
	gs_set_irq_mask(gsdevno,
					GS_RX_LINK_ERROR_INT |
					GS_TX_LINK_ERROR_INT |
					GS_RX_FIFO_FULL_INT |
					GS_DMA_WR_DONE_INT |
					GS_DMA_WR_DIED_INT |
					GS_DMA_WR_STOP_INT |
					GS_DMA_WR_ABORT_INT |
			/*		GS_DMA_RD_DONE_INT |  */
					GS_DMA_RD_OVFL_INT |
					GS_DMA_RD_STOP_INT |
					GS_DMA_RD_ABORT_INT
					);

TDBG("--- set up DMA (A) for 0x%x dwords, device %d\n", mapsize/4, gsdevno)

	// map npages for RX - the two dma buffers will alternate
	if ( (dmabuf[gsdevno]=gs_rx_dma_setup(gsdevno, mapsize))==NULL )
		{
		printf("*** (dcblib.c): Could not map pages for device %d\n", gsdevno);
		return -1;
		}

	// frame_limit, frame_time, exposure_count & exposure_time
	// have already been set

	// enable interrupt below

	switch (dcb_firmware_build)	// will free-run if n_images>1
		{
		case 0x0d30:			// 32-bit nexpf, multi-module
		case 0x0d31:			// 32-bit nexpf, single-module
			if (multi_trigger)
				dcb_set_tx_frame_limit(1);
			break;
		default:
			break;
		}

	}


// Shorter exposures set the interrupt, and return so that the
// system can continue processing.  Eventually, camera_check_status()
// will return with nonzero result, triggering a readout.
// Longer exposures start printing time, and set a flag.  Near
// the end of the exposure, the interrupt is set, and readout
// is triggered by nonzero return from camera_check_status().
// The order (first dma, then expose) may be important if the
// exposure is very short.

if (external_enable || external_trigger || multi_trigger)
	{
	if (first_exposure_flag && exposure_time+(nexppf-1)*exposure_period > 
			0x5fff*dma_clock_period)
		{
		dma_holdoff_time = exposure_time+(nexppf-1)*exposure_period;
		TDBG("--- Set DMA_holdoff time (A) to: %lf\n", dma_holdoff_time)
		}
	if (first_exposure_flag && (exposure_time+(nexppf-1)*exposure_period > 2.0 ||
			dma_holdoff_time>0.0))
		{
		longPeriod = 1;
		return 0;
		}
	else if (!first_exposure_flag && (nexppf*exposure_period > 2.0 ||
			dma_holdoff_time>0.0))
		{
		longPeriod = 1;
		return 0;
		}
	else
		{
		// enable master dcb last
		for (gsdevno = -1+NDCB; gsdevno>=0; gsdevno--)
			{
			// clear the PMC gigastar of any interrupt condition
			gs_set_control(gsdevno, GS_RES_RX_FIFO);
			gs_clear_irq(gsdevno);

			// reset DCB fifo
			dcb_write_register(gsdevno, DCB_RESET_REG, DCB_INT_FIFO_RESET);
			dcb_image_fifo_enable(gsdevno);

			// enable interrupt using the mask previously set
			gs_set_control(gsdevno, GS_INT_ENABLE);

			// enable dma write to host
			gs_set_control(gsdevno, GS_FIRE_DMA_WRITE);
			TDBG("--- fire DMA write (A) dcb %d: %s\n", gsdevno, gs_timestamp())

			// set the image TX enable flag in the dcb.
			dcb_enable_image_tx(gsdevno);

			#if ENABLE_BCBS

			  dcb_write_command_no_ack(gsdevno, DCB_TRIGGER_AUTO_FIFO_READ);

			#endif
			}
		}
	return 0;
	}

if (nexppf*exposure_period < TBRK)		// short exposures - no countdown
	{
	// enable master dcb last
	for (gsdevno = -1+NDCB; gsdevno>=0; gsdevno--)
		{
		// clear the PMC gigastar of any interrupt condition
		gs_set_control(gsdevno, GS_RES_RX_FIFO);
		gs_clear_irq(gsdevno);

		// reset DCB fifo
		dcb_write_register(gsdevno, DCB_RESET_REG, DCB_INT_FIFO_RESET);
		dcb_image_fifo_enable(gsdevno);

		// enable interrupt using the mask previously set
		gs_set_control(gsdevno, GS_INT_ENABLE);

		// enable dma write to host
		gs_set_control(gsdevno, GS_FIRE_DMA_WRITE);
		TDBG("--- fire DMA write (B) dcb %d: %s\n", gsdevno, gs_timestamp())

		// set the image TX enable flag in the dcb.
		dcb_enable_image_tx(gsdevno);

		#if ENABLE_BCBS

		  dcb_write_command_no_ack(gsdevno, DCB_TRIGGER_AUTO_FIFO_READ);

		#endif
		}
	}
else			// longer exposures - show a countdown; enable readout later
	{
	// set the flag
	longPeriod = 1;

	// print the first line of time for long exposures.  In the middle 
	// of a sequence of frames, this is done in dcb_check_status()
	 if (!autoFrame)
		{
		printf("\b\b\b\b\b\b\b%7d", (int)(exposure_time+(nexppf-1)*exposure_period));
		fflush(stdout);
		}
	}

// within an autoFrame sequence, we do not give the expose command
if (!autoFrame)
	{
	// start the exposure
#if INDEPENDENT_DCBs
	for (gsdevno = 1; gsdevno<NDCB; gsdevno++)
		dcb_write_command_no_ack(gsdevno, DCB_EXPOSE);
#endif

	// mark the start time of the first exposure
	exposure_start_time = gs_time();
	// start the master timer last
	dcb_write_command_no_ack(0, DCB_EXPOSE);
	}

return 0;
}



/*
** dcb_check_status() - i.e., status of exposure in progress
** For >2sec left (a long exposure), continue printing time left.
** At end of long exposure, finish printing & enable interrupt.
**
** If we are near the end, enable readout.
**
** This routine may be called very heavily; the caller should
** introduce some wait states to reduce load.
*/
int dcb_check_status(char *label)
{
int gsdevno;
double tr;

//TDBG("--- dcb_check_status() - time: %.6lf\n", gs_time())

if (dma_holdoff_time==0.0 && nexppf*exposure_period < 0.01)	// high speed operation
	return 1;					// enable readout immediately

if ((dma_holdoff_time==0.0) && !longPeriod &&
		(external_enable || external_trigger)) 	// go on to readout
	{
	if (label && tx_frame_limit<=100)	// avoid too much clutter
		printf("--  %s\n", label);
	return 1;
	}

// set the device number for the master DCB controller.
gsdevno = 0;
tr = dcb_calc_remaining_time();

if (longPeriod)
	{
	TDBG("dcb_check_status() - tr = %lf\n", tr)
	if (external_enable)
		{
		if (tr<-2.0)
			{
			usleep((int)(-0.5e6*tr));
			return 0;
			}
		}
	else		// not external_enable
		{
		if (tr < -0.03)		// waiting for end of frame period
			{
			usleep((int)(-0.5e6*tr));
			return 0;
			}
		if (tr<0.0 && (exposure_time>2.0 || nexppf*exposure_period>2.0))	// start printing time for new exposure
			{
			printf("\b\b\b\b\b\b\b%7d", (int)(exposure_time+(nexppf-1)*exposure_period-0.5));
			fflush(stdout);
			usleep((int)(-1.0e6*tr));
			return 0;
			}
		}
	if (tr >= 2.0)
		{
		if (tr < exposure_time+(nexppf-1)*exposure_period)
			{
			if (external_trigger && first_exposure_flag)
				{
				// set start time for this exposure
				exposure_start_time = gs_time() + tr - exposure_time -
						(nexppf-1)*exposure_period;
				gs_set_t0(-exposure_start_time);
				exposure_start_time = 0.0;
				first_exposure_flag = 0;
				}
			if (!external_enable)
				{
				printf("\b\b\b\b\b\b\b%7d", (int)tr);
				fflush(stdout);
				}
			}

		// if we are in this routine, we must not have any useful
		// processing to do, so sleep it off
		usleep(500000);
		return 0;
		}

	// near the end of a long exposure
	longPeriod = 0;

	// enable master dcb last
	for (gsdevno = -1+NDCB; gsdevno>=0; gsdevno--)
		{
		// clear the PMC gigastar of any interrupt condition
		gs_set_control(gsdevno, GS_RES_RX_FIFO);
		gs_clear_irq(gsdevno);

		// reset DCB fifo
		dcb_write_register(gsdevno, DCB_RESET_REG, DCB_INT_FIFO_RESET);
		dcb_image_fifo_enable(gsdevno);

		// enable interrupt using the mask previously set
		gs_set_control(gsdevno, GS_INT_ENABLE);

		// enable dma write to host
		gs_set_control(gsdevno, GS_FIRE_DMA_WRITE);
		TDBG("--- fire DMA write (C) dcb %d: %s\n", gsdevno, gs_timestamp())

		// set the image TX enable flag in the dcb.
		dcb_enable_image_tx(gsdevno);

		#if ENABLE_BCBS

		  dcb_write_command_no_ack(gsdevno, DCB_TRIGGER_AUTO_FIFO_READ);

		#endif
		}
	// finish printing
	printf("\b\b\b\b\b\b\b");
	fflush(stdout);
	}

TDBG("--- dcb_check_status() enable readout: %s\n", gs_timestamp())
if ( label && (tx_frame_limit<100 || exposure_period>=2.0) )
	printf("--  %s\n", label);

return 1;		// enable readout
}

#undef TBRK



/*
**  dcb_wait_read_image() -- wait for previously set-up dma to end, then
**  read entire detector image.
**  Note that img[] must already exist
**  mode = 0 for pulses, 1 for x-rays
**  Interleaves module readout and data conversion.
**  Returns the timestamp at the end of the exposure, corrected for overhead
*/
double dcb_wait_read_image(char **dataP)
{
int i, err, gsdevno=0;
unsigned int n;
double tn, tb, ti, tend, ts, tr=0.0, est;
char *dmabuffer[GS_MAX_DEVICES];
char sym[]="  ";

TDBG("\n--- dcb_wait_read_image() --------------------------------------------\n")

if (last_readout_enabled > 1)
	{
	exposures_in_frame=exp_count_in_frame;	// just the expected number
	return total_sequence_time;
	}

tb = time_log(0, "before intrpt");	// if not active, same as gs_time()

/*
** Wait for interrupt.  The time required here is highly variable.
*/
err = dcb_rx_data_dma(0);			// first is always gsdevno=0

tn = time_log(0, "after intrpt");	// if not active, same as gs_time()

ti = gs_last_interrupt_time(0);

#if MULTI_MODULE_DETECTOR
  tend = ti-overhead_time-transfer_time;
  // wait for interrupts & capture other dma buffer pointers
  for (gsdevno=1; gsdevno<NDCB; gsdevno++)
	  {
	  err += dcb_rx_data_dma(gsdevno);
	  ti = max(ti, gs_last_interrupt_time(gsdevno));
	  }
#else
  tend = ti-overhead_time;
#endif

// a dummy for now to keep compiler happy
n=dcb_get_exp_count_in_frame_snc_read();

// capture the dma buffer addresses before switching buffers
for (gsdevno = 0; gsdevno<NDCB; gsdevno++)
	dmabuffer[gsdevno] = dmabuf[gsdevno];

#if MULTI_MODULE_DETECTOR	// what should we do for 1 module?
if (err)					// serious error (bus overload?) - try recovery
	{
	// wait for 1 module readtime
	usleep(5000);
	// clear PMC GigaSTaR fifo
	for (gsdevno=0; gsdevno<NDCB; gsdevno++)
		{
		gs_set_control(gsdevno, GS_RES_RX_FIFO);
		gs_clear_irq(gsdevno);
		}
	// clear BCB fifos
	dcb_set_bank_module_address(15, 7);		// select all
	for (gsdevno=0; gsdevno<NDCB; gsdevno++)
		dcb_write_register(gsdevno, BCB_RESET_REG, BCB_EXT_FIFO_MASTER_RESET);
	TDBG("*** Error recovery attempted: ti = %.6lf, tn = %.6lf\n", ti, tn)
	return -1.0;
	}
#endif

if (err)
	return -1.0;

/*
** If in external trigger mode, we have just received a trigger
** and switch over to normal mode and reset the dma timeout.
** From the interrupt we know the time at the start of the exposure
** and reset both gs_time() and gs_timestamp() accordingly.
** The interrupt marks the end of data transfer to the PC.
** If no BCB, we are just beyond the end of the readout, with the
** next exposure soon to start (or already started if the timing is 
** tight).  With a BCB, the next exposure may be well underway, possibly
** nearing its end if the timing is tight.
*/
if (external_trigger)
	{
	external_trigger = 0;
		// for the exp just finished
	exposure_start_time = tend-exposure_time-(nexppf-1)*exposure_period;
	if (first_exposure_flag)						// can be reset in dcb_check_status()	
		gs_set_t0(-exposure_start_time);			// adjust start time
	gs_set_rx_timeout(0, dma_timeout);				// master device
	tn = gs_time();
	TDBG("--- Set DMA timeout (B), dev 0 = %f, tn = %.6lf\n",
			dma_timeout*dma_clock_period, tn)
	// other dcb's
	for (i=1; i<NDCB; i++)
		gs_set_rx_timeout(i, dma_timeout+0x100);	// a little longer
	// set time for start of 2nd exposure after external trigger
	// may be shortly before the exposure (no BCB) or within exposure
	exposure_start_time = 0.0;		// for the last exposure on the new scale
	first_exposure_flag = 0;
	}

if (first_exposure_flag && (multi_trigger || external_enable))
	{
	if (nexppf > 1)
		exposure_start_time = tend-nexppf*exposure_period;	// exp just finished
	else
		exposure_start_time = tend-exposure_time;		// exp just finished
	gs_set_t0(-exposure_start_time);				// adjust start time
	exposure_start_time = 0.0;		// exposure just finished on new scale
	first_exposure_flag = 0;
	}
if ( (external_enable || multi_trigger) && 
		(nexppf*exposure_period>dma_timeout*dma_clock_period) &&
		(tn < (total_sequence_time - 0.001)*clock_skew_factor) )
	dma_holdoff_time = nexppf*exposure_period;
else
	dma_holdoff_time = 0.0;

n = 0;
if (exposure_time > 0.0)		// if ==0, this is not an exposure
	{
	tr = dcb_get_remaining_time();
	tn = gs_time();

	// it is an error to access this register at any other time
	n = dcb_get_exp_count_in_frame();

	if (external_enable)
		{
		dcb_read_measured_exposure_time();
		exposure_time = measured_exposure_time;	// correct the operators estimate
		}

	// estimate the start time of the next exposure, which may already
	// be in progress
	ts = est = exposure_start_time;		// exposure just finished
	if (tn > ts-2.0)
		ts = exposure_start_time + nexppf*exposure_period/clock_skew_factor;
	if (tr > 0.0 && nexppf==1)					// measured time remaining
		ts = tn + tr - exposure_time;
	exposure_start_time = ts;

	TDBG("set exposure_start_time (D) = %.6lf, tn = %.6lf, tr = %.6lf,tend = %lf\n", 
			exposure_start_time, tn, tr, tend)

	// enable the first readout of the next exposure, if applicable
	if (autoFrame && !last_readout_enabled)
		dcb_expose();
/*
** If the bus is overloaded, we can be beyond the end of the dcb exposure
** sequence.  We can do 1 more readout of the last accumumlated exposures
** (frequently >>1), then block further readouts.
*/
#ifdef DEBUG
	if (!external_enable && !external_trigger && !multi_trigger)
		{
		if (tn > (total_sequence_time - 0.001)*clock_skew_factor)
			last_readout_enabled++;
		if (last_readout_enabled==1)
			TDBG("Last readout enabled at tn = %.6lf\n", gs_time())
	//	DBG(2, "Wait time for interrupt: %.6lf\n", (tn-tb));
		TDBG("dT start to intrpt: %lf, exp in frame = %u, ti = %.6lf, time is %.6lf\n", 
					ti-est, n, ti, tn)
		if (tn > exposure_start_time + exposure_time + nexppf*exposure_period)
			TDBG("\t\t>>>>> Late: Exposure end was at %.6lf\n", 
					est + exposure_time + nexppf*exposure_period)
		}
	else
		TDBG("exp in frame = %u, measured time = %.6lf, time is %.6lf\n",
				n, measured_exposure_time, gs_time())
#else
	if (!external_enable && !external_trigger && !multi_trigger &&
			(tn > (total_sequence_time - 0.001)*clock_skew_factor) )
		last_readout_enabled++;
#endif
	switch (dcb_firmware_build)
		{
		// single-module
		case 0x0d31:			// 32-bit nexpf, single-module
		case 0x0d78:			// improved multi-trigger, single-module
		case 0x0d94:			// enable holdoff, single-module
		case 0x0d98:			// optional parallel write, single-module
		case 0x10f8:			// temp & hdty lower limits, single module
		case 0x1658:			// port to Xilinx tools, single-module
		case 0x1ad0:			// corrected DCB_MIN_READ_TIME_REG, single-module
		case 0x1c54:			// T: 55/15, 45/15, 45/15, H: 80, 80, 30, single-module
		// multi-module
		case 0x0d30:			// 32-bit nexpf, multi-module
		case 0x10f9:			// use with BCB build 10f8, multi-module
		case 0x165c:			// port to Xilinx tools, use with BCB build 0x161c, multi-module
		case 0x1a3c:			// 45C temp limit chan 2, use with BCB build 0x161c, multi-module
		case 0x1ac8:			// 45C limit chan 2, power up enable depends on DIP SW4, multi-module
		case 0x1b60:			// DCB_MIN_READ_TIME_REG R/W, 3 T sensors, 15C lo T lim, 12 bad reads needed, multi-module
		case 0x1bb8:			// 30% RH max on chan. 2
			if (n==0xffffffff)	// bad readout
				n=0;
			break;
		// older versions
		case 0x0a10:			// old, multi-module
		case 0x0a9d:			// multi-extt, multi-module
		case 0x0ca8:			// multi-extt, multi-module
  		case 0x08c0:			// before limo swap, single-module
		case 0x096c:			// old-style fillpix, single-module
		case 0x0a11:			// new-style fillpix, single-module
		case 0x0a9c:			// multi-extt, single-module
			if (n==0xffff)		// bad readout
				n=0;
			break;
		default:
			printf("*** Missing case(B): dcb_firmware_build = 0x%.4x\n", dcb_firmware_build);
			break;
		}
	exposures_in_frame = n;
	}
else
	dcb_write_data_allpix(0xfffff);		// restore standard state if just reading

/*
** transfer the data to a holding buffer
** This moves the data out of the dma buffers into a holding place
** in preparation for a fork().  On a single processor system (1 core)
** this is a waste of time, but on a multi-procesor system this moves
** the time-consuming image conversion to another CPU.  See note in
** interface.c
*/
for (gsdevno = 0; gsdevno<NDCB; gsdevno++)
	memcpy(theData+gsdevno*mapsize, dmabuffer[gsdevno], mapsize);
*dataP = theData;		// pass result to caller

if (exposure_time==0.0)		// flag for not an exposure
	return 0.0;

/*
** Give best approximation of the start of the next exposure.
** If we are in a frame series, autoFrame is turned off BEFORE
** the last dcb_wait_read_image() of the series.
*/
if (autoFrame && !external_enable && !external_trigger)
	{
	strcpy(sym, "  ");
	if (tn-tb>exposure_period)
		strcpy(sym, "##");
	else if (tn-tb<0.001)
		strcpy(sym, "**");

	if (logbuf && (logbufP-logbuf < logbufL-200))
		logbufP+=sprintf(logbufP,
"%sn = %4d, twi = %.6lf, ts = %9.6lf, tn = %9.6lf, dt = %.6lf, ti = %9.6lf, td = %9.6lf, tdw = %9.6lf\n",
			sym,
			-1+dcb_get_tx_frame_count(),
			tn - tb,
			exposure_start_time - frame_group_start_time,
			tn - frame_group_start_time,
			tn-exposure_start_time,
			ti - frame_group_start_time,
			tn - ti,
			tn - enable_time );

	if (n!=1)
		if (logbuf && (logbufP-logbuf < logbufL-200))
			logbufP+=sprintf(logbufP, "*** dcb exposure count register n = %d\n", n);
	}

// Give best approximation of end of the just-processed exposure
return tend;
}



int dcb_set_readout_bits(int nbits)
{
#if ENABLE_BCBS
int gsdevno;
#endif

if (nbits!=4 && nbits!=8)
	nbits=20;
readout_n_bits=nbits;

// set the mapsize and fill pixels with the starting value
switch (readout_n_bits)
	{
	case 4:
		mapsize=MAPSIZE;
		dcb_write_data_allpix(BASEWORD_4BIT);
		break;
	case 8:
		mapsize=MAPSIZE;
		dcb_write_data_allpix(BASEWORD_8BIT);
		break;
	default:
		mapsize=MAPSIZE;
		dcb_write_data_allpix(0xfffff);
		break;
	}

// reprogram the FIFOs on the BCBs - not needed for single modules
#if ENABLE_BCBS
dcb_set_bank_module_address(15, 7);		// select all modules
for (gsdevno = 0; gsdevno<NDCB; gsdevno++)
	dcb_set_register_bits(gsdevno, BCB_RESET_REG, 
			BCB_EXT_FIFO_MASTER_RESET | BCB_EXT_FIFO_PARTIAL_RESET);
#endif

return 0;
}


/*
**    To do:
** calculate overhead_time & transfer_time
** calculate mapsize
** adjust convert_imaage - does the conversion restrict ROI parameters?
** is the parameter ROI_readout actually useful?
*/
int dcb_set_roi(int x, int nx, int y, int ny)
{
unsigned int col=(x<<8)|nx, row=(y<<8)|ny;
int gsdevno=0;

if (x<0 || nx<=0 || x+nx>=NCOL_CHIP || y<0 || ny<=0 || y+ny>=NROW_CHIP)
	{
	printf("Illegal ROI parameters: %d %d %d %d\n", x, nx, y, ny);
	TDBG("Illegal ROI parameters: %d %d %d %d\n", x, nx, y, ny)
	return -1;
	}

if (x==0 && nx==NCOL_CHIP && y==0 && ny==NROW_CHIP)
	ROI_readout=0;
else
	ROI_readout=1;

#if ENABLE_BCBS
  dcb_set_bank_module_address(15, 7);		// select all

  for(gsdevno=0; gsdevno<NDCB; gsdevno++)
	  {
	  dcb_write_register(gsdevno, BCB_ROI_COL_SET_REG, col);
	  dcb_write_register(gsdevno, BCB_ROI_ROW_SET_REG, row);
	  }

  mapsize=MAPSIZE;

#else			// single modules

  for(gsdevno=0; gsdevno<NDCB; gsdevno++)
	  {
  	  dcb_write_register(gsdevno, DCB_ROI_COL_SET_REG, col);
  	  dcb_write_register(gsdevno, DCB_ROI_ROW_SET_REG, row);
	  }

  mapsize=MAPSIZE;

#endif

return 0;
}



int dcb_get_roi(int *x, int *nx, int *y, int *ny)
{
unsigned int ui;

#if ENABLE_BCBS
dcb_set_bank_module_address(1, 1);		// select a module

ui = dcb_read_register(0, BCB_ROI_COL_SET_REG);
*ny = ui & 0x3f;
*y = (ui>>8) & 0x3f;
ui = dcb_read_register(0, BCB_ROI_ROW_SET_REG);
*nx = ui & 0x7f;
*x = (ui>>8) & 0x7f;

#else			// single modules
dcb_set_bank_module_address(1, 1);		// select a module

ui = dcb_read_register(0, DCB_ROI_COL_SET_REG);
*ny = ui & 0x3f;
*y = (ui>>8) & 0x3f;
ui = dcb_read_register(0, DCB_ROI_ROW_SET_REG);
*nx = ui & 0x7f;
*x = (ui>>8) & 0x7f;

#endif

return 0;
}




int dcb_get_readout_bits(void)
{return readout_n_bits;}



void dcb_set_autoframe(void)
{
TDBG("--- dcb_set_autoframe()\n")

autoFrame=1;
frame_group_start_time = exposure_start_time;
return;
}



void dcb_reset_autoframe(void)
{
TDBG("--- dcb_reset_autoframe()\n")

autoFrame=0;
frame_group_start_time = 0.0;
return;
}



int dcb_reset_exposure_mode(void)
{
int gsdevno;

TDBG("--- dcb_reset_exposure_mode(); fc = %d, fl = %d\n",\
dcb_get_tx_frame_count(), dcb_get_tx_frame_limit())

external_enable = 0;
external_trigger = 0;
for(gsdevno = 0; gsdevno<NDCB; gsdevno++)
	{
	if (dcb_clear_register_bits(gsdevno, DCB_CTRL_REG, 
				DCB_EXPOSURE_CONTROL |
				DCB_EXT_TRIGGER_CONTROL |
				DCB_ENB_MULTI_EXT_TRIGGER))
		return -1;
	if (dcb_reset_image_fifo(gsdevno))
		return -1;
	}
dcb_set_multi_trigger(0);
exposure_start_time = 0.0;
first_exposure_flag = 0;
autoFrame = 0;
dma_holdoff_time = 0.0;
return 0;
}



int dcb_set_external_enb(int gsdevno)
{
TDBG("--- dcb_set_external_enb()\n")
external_enable = 1;
return 0;
}



int dcb_set_external_trigger(int gsdevno)
{
TDBG("--- dcb_set_external_trigger()\n")
external_trigger = 1;
return 0;
}


// set or reset multiple-external trigger mode
int dcb_set_multi_trigger(int state)
{
int gsdevno;

TDBG("--- dcb_set_multi_trigger (%d)\n", state)

//verify that firmware is compatible
switch (dcb_firmware_build)
	{
	// older versions
  	case 0x08c0:		// before limo swap, single-module
	case 0x096c:		// old-style fillpix, single-module
	case 0x0a11:		// new-style fillpix, single-module
	case 0x0a10:		// old, multi-module
		if (state)		// attempt to set on old firmware
			printf("*** Error:  Firmware (0x%.4x) does not support multi-triggering\n", dcb_firmware_build);
		return -1;
		break;
	// newer versions
	case 0x0a9c:		// multi-extt, single-module
	case 0x0d31:		// 32-bit nexpf, single-module
	case 0x0d78:		// improved multi-trigger, single-module
	case 0x0d94:		// enable holdoff, single-module
	case 0x0d98:		// optional parallel write, single-module
	case 0x10f8:		// temp & hdty lower limits, single module
	case 0x1658:		// port to Xilinx tools, single module
	case 0x1ad0:		// corrected DCB_MIN_READ_TIME_REG, single-module
	case 0x1c54:		// T: 55/15, 45/15, 45/15, H: 80, 80, 30, single-module
	// multi-module
	case 0x0a9d:		// multi-extt, multi-module
	case 0x0ca8:		// multi-extt, multi-module
	case 0x0d30:		// 32-bit nexpf, multi-module
	case 0x10f9:		// use with BCB build 10f8, multi-module
	case 0x165c:		// port to Xilinx tools, use with BCB build 0x161c, multi-module
	case 0x1a3c:		// 45C temp limit chan 2, use with BCB build 0x161c, multi-module
	case 0x1ac8:		// 45C limit chan 2, power up enable depends on DIP SW4, multi-module
	case 0x1b60:		// DCB_MIN_READ_TIME_REG R/W, 3 T sensors, 15C lo T lim, 12 bad reads needed, multi-module
	case 0x1bb8:		// 30% RH max on chan. 2
		break;
	default:
		printf("*** Missing case(C): dcb_firmware_build = 0x%.4x\n", dcb_firmware_build);
		return -1;
		break;
	}

if (state)
	multi_trigger=1;	// mark the state for later use
else
	{
	multi_trigger=0;
	for(gsdevno = 0; gsdevno<NDCB; gsdevno++)	// doesn't hurt to do all
		{
		switch (dcb_firmware_build)
			{
			case 0x0d78:		// improved multi-trigger, single-module
			case 0x0d94:		// enable holdoff, single-module
			case 0x0d98:		// optional parallel write, single-module
			case 0x10f8:		// temp & hdty lower limits, single module
			case 0x1658:		// port to Xilinx tools, single module
			case 0x1ad0:		// corrected DCB_MIN_READ_TIME_REG, single-module
			case 0x1c54:		// T: 55/15, 45/15, 45/15, H: 80, 80, 30, single-module
			// multi-module
			case 0x10f9:		// use with BCB build 10f8, multi-module
			case 0x165c:		// port to Xilinx tools, use with BCB build 0x161c, multi-module
			case 0x1a3c:		// 45C temp limit chan 2, use with BCB build 0x161c, multi-module
			case 0x1ac8:		// 45C limit chan 2, power up enable depends on DIP SW4, multi-module
			case 0x1b60:		// DCB_MIN_READ_TIME_REG R/W, 3 T sensors, 15C lo T lim, 12 bad reads needed, multi-module
			case 0x1bb8:		// 30% RH max on chan. 2
				if (dcb_clear_register_bits(gsdevno, DCB_CTRL_REG, 
							DCB_EXT_TRIGGER_MODE))
					return -1;
			// fall through
			default:
				if (dcb_clear_register_bits(gsdevno, DCB_CTRL_REG, 
							DCB_ENB_MULTI_EXT_TRIGGER))
					return -1;
				break;	
			}
		}
	}
return 0;
}


static int dcb_arm_multi_trigger(void)
{
int gsdevno;

TDBG("--- dcb_arm_multi_trigger ()\n")

if (!multi_trigger)
	return -1;

gsdevno = 0;			// master DCB only
switch (dcb_firmware_build)
	{
	case 0x0d78:		// improved multi-trigger, single-module
	case 0x0d94:		// enable holdoff, single-module
	case 0x0d98:		// optional parallel write, single-module
	case 0x10f8:		// temp & hdty lower limits, single module
	case 0x1658:		// port to Xilinx tools, single module
	case 0x1ad0:		// corrected DCB_MIN_READ_TIME_REG, single-module
	case 0x1c54:		// T: 55/15, 45/15, 45/15, H: 80, 80, 30, single-module
	// multi-module
	case 0x10f9:		// use with BCB build 10f8, multi-module
	case 0x165c:		// port to Xilinx tools, use with BCB build 0x161c, multi-module
	case 0x1a3c:		// 45C temp limit chan 2, use with BCB build 0x161c, multi-module
	case 0x1ac8:		// 45C limit chan 2, power up enable depends on DIP SW4, multi-module
	case 0x1b60:		// DCB_MIN_READ_TIME_REG R/W, 3 T sensors, 15C lo T lim, 12 bad reads needed, multi-module
	case 0x1bb8:		// 30% RH max on chan. 2
		if (dcb_set_register_bits(gsdevno, DCB_CTRL_REG, 
					DCB_EXT_TRIGGER_MODE))
			return -1;
	// fall through
	default:
		if (dcb_set_register_bits(gsdevno, DCB_CTRL_REG, 
					DCB_ENB_MULTI_EXT_TRIGGER))
			return -1;
		break;	
	}
return 0;
}


// report whether external enable is set
int dcb_external_enb(void)
{return external_enable;}


// report whether external trigger is set
int dcb_external_trig(void)
{return external_trigger;}



// report whether external multi trigger is set
int dcb_external_multi_trig(void)
{return multi_trigger;}



/*
**  dcb_read_module() -- subroutine to read 1 module
**  starts the read and returns - caller should use 
**  dcb_rx_data_dma() to wait for dma completion
*/
static char *dcb_read_module(int gsdevno, int bank, int module)
{
int to;
TDBG("--- dcb_read_module()\n")

// assume that PMC GigaSTaR and dcb have been cleared by dcb_bcb_reset()

// set the bank and module
dcb_set_bank_module_address(bank, module);

to = 0x1000;
// set the DMA WR timeout  -- 0x1000 * 250 us = 1.024 s
(void)gs_set_rx_timeout(gsdevno, to);
TDBG("--- Set DMA timeout (C), dev 0 = %f (0x%x)\n", (float)to*dma_clock_period, to)

// set interrupt on RX dma (i.e., dma write) finished & errors
gs_set_irq_mask(gsdevno,
				GS_RX_LINK_ERROR_INT |
				GS_TX_LINK_ERROR_INT |
				GS_RX_FIFO_FULL_INT |
				GS_DMA_WR_DONE_INT |
				GS_DMA_WR_DIED_INT |
				GS_DMA_WR_STOP_INT |
				GS_DMA_WR_ABORT_INT |
		/*		GS_DMA_RD_DONE_INT |  */
				GS_DMA_RD_OVFL_INT |
				GS_DMA_RD_STOP_INT |
				GS_DMA_RD_ABORT_INT
				);

TDBG("--- set up DMA (B) for 0x%x dwords, device %d\n", mapsize/4, gsdevno)

// map pages for RX
if ( (dmabuf[gsdevno]=gs_rx_dma_setup(gsdevno, mapsize))==NULL )
	{
	printf("*** dcb_read_module(), device %d: could not map pages\n", gsdevno);
	return NULL;
	}

// enable interrupt using the mask set above
gs_set_control(gsdevno, GS_INT_ENABLE);

dcb_image_fifo_enable(gsdevno);

// enable dma write to host
gs_set_control(gsdevno, GS_FIRE_DMA_WRITE);
TDBG("--- fire DMA write (D) dcb %d: %s\n", gsdevno, gs_timestamp())
	
dcb_enable_image_tx(gsdevno);				// enable TX to bcb

// start the readout
#if ENABLE_BCBS

dcb_write_command_no_ack(gsdevno, DCB_TRIGGER_READSTROBE);

#else

// set the image TX enable flag in the dcb.
dcb_write_command_no_ack(gsdevno, DCB_READOUT_MODULES);

#endif

return dmabuf[gsdevno];
}




/*
**  dcb_read_banks() -- subroutine to read banks of 1 dcb
**  starts the read and returns - caller should use 
**  dcb_rx_data_dma() to wait for dma completion
*/
static char *dcb_read_banks(int gsdevno)
{
TDBG("--- dcb_read_banks()\n")

#if MULTI_MODULE_DETECTOR

// assume that PMC GigaSTaR and dcb have been cleared by dcb_bcb_reset()

// set the DMA WR timeout  -- 0x1000 * 250 us = 1.024 s
(void)gs_set_rx_timeout(gsdevno, 0x1000);

// set interrupt on RX dma (i.e., dma write) finished & errors
gs_set_irq_mask(gsdevno,
				GS_RX_LINK_ERROR_INT |
				GS_TX_LINK_ERROR_INT |
				GS_RX_FIFO_FULL_INT |
				GS_DMA_WR_DONE_INT |
				GS_DMA_WR_DIED_INT |
				GS_DMA_WR_STOP_INT |
				GS_DMA_WR_ABORT_INT |
		/*		GS_DMA_RD_DONE_INT |  */
				GS_DMA_RD_OVFL_INT |
				GS_DMA_RD_STOP_INT |
				GS_DMA_RD_ABORT_INT
				);

TDBG("--- set up DMA (C) for 0x%x dwords, device %d\n", mapsize/4, gsdevno)

// map pages for RX
if ( (dmabuf[gsdevno]=gs_rx_dma_setup(gsdevno, mapsize))==NULL )
	{
	printf("*** dcb_read_banks(), device %d: could not map pages\n", gsdevno);
	return NULL;
	}

// enable interrupt using the mask set above
gs_set_control(gsdevno, GS_INT_ENABLE);

dcb_image_fifo_enable(gsdevno);

// enable dma write to host
gs_set_control(gsdevno, GS_FIRE_DMA_WRITE);
TDBG("--- fire DMA write (E) dcb %d: %s\n", gsdevno, gs_timestamp())
	
dcb_enable_image_tx(gsdevno);				// enable TX to bcb

// enable the readout
#if ((NMOD_BANK>1 && !defined WIDE_FAST) || defined USE_BCB)
 dcb_write_command(gsdevno, DCB_READOUT_MODULES, 0);
#else
 dcb_write_command_no_ack(gsdevno, DCB_READOUT_MODULES);
#endif
dcb_write_command_no_ack(gsdevno, DCB_TRIGGER_AUTO_FIFO_READ);

return dmabuf[gsdevno];

#else

return dcb_read_module(gsdevno, 1, 1);

#endif
}




/*
** dcb_rx_data_dma() - wait for dma completion & check status
*/
static int dcb_rx_data_dma(int gsdevno)
{
int n, err=0;
unsigned int t;

DBG(5, "--- dcb_rx_data_dma() (A): %s, tn = %.6lf\n", gs_timestamp(), gs_time())

// wait for interrupt
n = gs_rx_dma(gsdevno);

DBG(5, "--- dcb_rx_data_dma() (B): %s, tn = %.6lf\n", gs_timestamp(), gs_time())

// capture status
t=gs_irq_src_status(gsdevno);

// clear the interrupt
gs_clear_irq(gsdevno);

// error messages - the most probable error is DMA timeout, but printing
// the error harms performance
if ( (t&GS_DMA_WR_DIED_INT) )		// dma timeout
	{
	printf("*** ERROR: DMA write timeout DCB %d: received 0x%x dwords ***\n", gsdevno, n/4);
	DBG(1, "*** ERROR: DMA write timeout DCB %d: received 0x%x dwords ***\n", gsdevno, n/4);
	if (logbuf && (logbufP-logbuf < logbufL-100))
		logbufP+=sprintf(logbufP, "=== DMA write timeout ===\n");
	err++;
	}
else								// more serious errors
	{
	if (n != mapsize )
		{
		printf("*** ERROR: DCB %d read returned 0x%x dwords ***\n", gsdevno, n/4);
		DBG(1, "*** ERROR: DCB %d read returned 0x%x dwords ***\n", gsdevno, n/4);
		err++;
		}
	if ( !(t & GS_DMA_WR_DONE_INT) )
		{
		printf("*** DCB %d DMA write was NOT successful; gs irq status = 0x%x\n", gsdevno, t);
		DBG(1, "*** DCB %d DMA write was NOT successful; gs irq status = 0x%x\n", gsdevno, t);
// bit 4 is inverted, so status = 0x10 corresponds to no bits set
// bit 0 set is a link error
		err++;
		}
	}

if (err)			// stop auto fifo read
	{
	dcb_write_command_no_ack(gsdevno, DCB_SET | DCB_RESET_REG | DCB_AUTO_FIFO_READ_RESET);
	dcb_bcb_reset(gsdevno);
	}
else if (!multi_trigger)
	{
	dcb_image_fifo_disable(gsdevno);		// EFE: SHOULD THIS BE HERE?? (e.g., if per-module)
	dcb_clear_register_bits(gsdevno, DCB_CTRL_REG, DCB_EXT_TRIGGER_CONTROL);
	}
return err;
}



/*
** dcb_set_exposure_time() - given time in seconds
*/
int dcb_set_exposure_time(double time)
{
long long ll;		// need 48 bits
int gsdevno=0;

if (time < 0.0 || time > dcb_clock_period*(double)(-1+(1LL<<48)))
	{
	printf("Illegal exposure time: %lf\n", time);
	return -1;
	}
for (gsdevno = 0; gsdevno<NDCB; gsdevno++)
	{
	ll = (long long)rint(time/dcb_clock_period);
	if (dcb_write_register(gsdevno, DCB_EXP_TIME_LOW_REG, (ll & 0xffff)))
		return -1;
	ll >>= 16;
	if (dcb_write_register(gsdevno, DCB_EXP_TIME_MED_REG, (ll & 0xffff)))
		return -1;
	ll >>= 16;
	if (dcb_write_register(gsdevno, DCB_EXP_TIME_HI_REG, (ll & 0xffff)))
		return -1;
	if (!trigger_offset_mode)
		break;
	}

return 0;
}



/*
** dcb_set_exposure_period() - given time in seconds
*/
int dcb_set_exposure_period(double time)
{
long long ll;		// need 48 bits
int gsdevno=0;

if (time < 0.0 || time > dcb_clock_period*(double)(-1+(1LL<<48)))
	{
	printf("Illegal exposure period: %lf\n", time);
	return -1;
	}
exposure_period = time;
for (gsdevno = 0; gsdevno<NDCB; gsdevno++)
	{
	ll = (long long)rint(time/dcb_clock_period);
	if (dcb_write_register(gsdevno, DCB_EXP_PERIOD_LOW_REG, (ll & 0xffff)))
		return -1;
	ll >>= 16;
	if (dcb_write_register(gsdevno, DCB_EXP_PERIOD_MED_REG, (ll & 0xffff)))
		return -1;
	ll >>= 16;
	if (dcb_write_register(gsdevno, DCB_EXP_PERIOD_HI_REG, (ll & 0xffff)))
		return -1;
	if (!trigger_offset_mode)
		break;
	}

return 0;
}



/*
** dcb_set_exp_trigger_delay() given time in seconds
** devno<0 is default mode.  devno>=0 programs specific DCB
*/
int dcb_set_exp_trigger_delay(double time, int devno)
{
unsigned int ui;		// need 32 bits
int gsdevno=0;

if (time < 0.0 || time > dcb_clock_period*(double)(-1+(1LL<<32)))
	{
	printf("Illegal trigger delay: %lf\n", time);
	return -1;
	}

if (devno>=0 && devno<NDCB)
	gsdevno = devno;
else if (devno>0)
	{
	printf("Illegal DCB device number: %d\n", devno);
	return -1;
	}

// write the time to the requested dcb, or to dcb 0 if default mode
ui = (unsigned int)rint(time/dcb_clock_period);
if (dcb_write_register(gsdevno, DCB_EXP_TRIG_DELAY_LOW_REG, (ui & 0xffff)))
	return -1;
ui >>= 16;
if (dcb_write_register(gsdevno, DCB_EXP_TRIG_DELAY_HI_REG, (ui & 0xffff)))
	return -1;

#if ENABLE_BCBS
if (devno>=0 && time>0.0)
	{
	printf("*** ERROR - cannot use offset triggers in a BCB detector\n");
	DBG(1, "*** ERROR - cannot use offset triggers in a BCB detector\n")
	return -1;
	}
#else
if (devno>=0 && time>0.0)
	{
	trigger_offset_mode = 1;
	return 0;
	}
#endif

trigger_offset_mode = 0;			// turn off mode

for (gsdevno = 1; gsdevno<NDCB; gsdevno++)
	{
	if (dcb_write_register(gsdevno, DCB_EXP_TRIG_DELAY_LOW_REG, (ui & 0xffff)))
		return -1;
	ui >>= 16;
	if (dcb_write_register(gsdevno, DCB_EXP_TRIG_DELAY_HI_REG, (ui & 0xffff)))
		return -1;
	}

return 0;
}



/*
** dcb_set_debounce_time(() - given time in seconds
** always device no. 0
*/
int dcb_set_debounce_time(double time)
{
long long ll;
int gsdevno=0;

if (time < 0.0 || time > dcb_clock_period*(double)(-1+(1LL<<32)))
	{
	printf("Illegal debounce time: %lf\n", time);
	return -1;
	}
if (time > 0.0)
	dcb_set_register_bits(gsdevno, DCB_CTRL_REG, DCB_EXT_EXP_DEBOUNCE_ENB);
else
	dcb_clear_register_bits(gsdevno, DCB_CTRL_REG, DCB_EXT_EXP_DEBOUNCE_ENB);

ll = (long long)rint(time/dcb_clock_period);
if (dcb_write_register(gsdevno, DCB_DEBOUNCE_TIME_LOW_REG, (ll & 0xffff)))
	return -1;
ll >>= 16;
if (dcb_write_register(gsdevno, DCB_DEBOUNCE_TIME_HI_REG, (ll & 0xffff)))
	return -1;

#if INDEPENDENT_DCBs

for (gsdevno = 1; gsdevno<NDCB; gsdevno++)
	{
	if (time > 0.0)
		dcb_set_register_bits(gsdevno, DCB_CTRL_REG, DCB_EXT_EXP_DEBOUNCE_ENB);
	else
		dcb_clear_register_bits(gsdevno, DCB_CTRL_REG, DCB_EXT_EXP_DEBOUNCE_ENB);

	ll = (long long)rint(time/dcb_clock_period);
	if (dcb_write_register(gsdevno, DCB_DEBOUNCE_TIME_LOW_REG, (ll & 0xffff)))
		return -1;
	ll >>= 16;
	if (dcb_write_register(gsdevno, DCB_DEBOUNCE_TIME_HI_REG, (ll & 0xffff)))
		return -1;
	}

#endif

return 0;
}



/*
**  dcb_set_bank_module_address() - set the bank and module address
*/
int dcb_set_bank_module_address(int bank, int mod)
{
# if MULTI_MODULE_DETECTOR

int devno, i=0;
if (bank < 0 || ((bank > NBANK) && (bank != 15)))
	{
	printf("Illegal bank address: %d\n", bank);
	return -1;
	}
if (mod < 0 || ((mod > NMOD_BANK) && (mod != 7)))
	{
	printf("Illegal module address: %d\n", mod);
	return -1;
	}

// if, e.g., MOD_ENB_PATTERN==6, then modules 1 & 2 become 2 & 3
#ifdef MOD_ENB_PATTERN
if (mod != MAX_MOD_ADD)
	for(i=0; i<N_PATTERN_BITS; i++)
		if (!(MOD_ENB_PATTERN & (1<<i)))
			mod++;
#endif

if (bank != MAX_BANK_ADD)		// normal case
	{
	devno = dcb_get_gsdevno(bank, mod);
#if defined USE_BCB
// perhaps translate bank - see note about detector assembly above
	i=bank;
	if ( BANKENP1 && !(BANKENP1&1) )		// if bank 1 not enabled
		{
		for(i=1; !(BANKENP1 & (1<<(i-1))); i++)
			;
		i += bank-1;
		}
	bank=i;
#endif
#if defined WIDE_FAST
	if (dcb_write_register(devno, DCB_BANK_ADD_REG, bank))
		return -1;
#else
	if (dcb_write_register(devno, DCB_BANK_ADD_REG, bank-devno*NBANK_DCB))
		return -1;
#endif
	return dcb_write_register(devno, DCB_MOD_ADD_REG, mod);
	}

// bank==15 - special case of all banks on all dcb's
for (i=1; i<=NDCB*NBANK_DCB; i+=NBANK_DCB)
	{
	devno = dcb_get_gsdevno(i, i);

	if (dcb_write_register(devno, DCB_BANK_ADD_REG, bank))
		return -1;
	if (dcb_write_register(devno, DCB_MOD_ADD_REG, mod))
		return -1;
	}

#else		// single module

int b=bank>0 ? 1 : 0, m=mod>0 ? 1 : 0;
if (dcb_write_register(0, DCB_BANK_ADD_REG, b))
	return -1;
if (dcb_write_register(0, DCB_MOD_ADD_REG, m))
	return -1;

#endif

return 0;
}



/*
** dcb_get_gsdevno() - return gsdevno from the bank number
** according to the declared detector geometry
** banks are numbered 1, 2, ... NBANK
** modules are numbered 1,2, ... NMOD_BANK
*/
int dcb_get_gsdevno(int bank, int mod)
{
#if defined WIDE_FAST && defined SLAVE_COMPUTER
// only 1 bank for now; mod is either 1 or 2
// bank==mod in requests that don't know the WIDE_FAST layout
if (bank>1 && bank!=mod)
	{
	printf("*** Error: request for device no. > NDCB\n");
	return 0;
	}
if (mod<=0 || mod>2)
	{
	printf("*** Error: request for device no. > NDCB\n");
	return 0;
	}
return mod-1;
#elif defined WIDE_FAST
// only 1 bank for now; mod is either 1 or 2 or 3
// bank==mod in requests that don't know the WIDE_FAST layout
if (bank>1 && bank!=mod)
	{
	printf("*** Error: request for device no. > NDCB\n");
	return 0;
	}
if (mod<=0 || mod>3)
	{
	printf("*** Error: request for device no. > NDCB\n");
	return 0;
	}
return mod-1;
#else
int n = bank;
if (bank > 0)
	n = (bank-1) / (NBANK_DCB);
if (n >= NDCB )
	{
	printf("*** Error: request for device no. > NDCB\n");
	return 0;
	}
else
	return n;
#endif
}



/*
** dcb_get_exposure_time()
** always device no. 0
*/
double dcb_get_exposure_time(void)
{
long long ll;		// need 48 bits
ll = (long long)dcb_read_register(0, DCB_EXP_TIME_HI_REG);
ll <<= 16;
ll |= (long long)dcb_read_register(0, DCB_EXP_TIME_MED_REG);
ll <<= 16;
ll |= (long long)dcb_read_register(0, DCB_EXP_TIME_LOW_REG);
return (dcb_clock_period * (double)ll);
}



double dcb_get_exposure_period(void)
{
long long ll;		// need 48 bits
ll = (long long)dcb_read_register(0, DCB_EXP_PERIOD_HI_REG);
ll <<= 16;
ll |= (long long)dcb_read_register(0, DCB_EXP_PERIOD_MED_REG);
ll <<= 16;
ll |= (long long)dcb_read_register(0, DCB_EXP_PERIOD_LOW_REG);
return (dcb_clock_period * (double)ll);
}



/*
** dcb_get_exp_trigger_delay()
** devno<0 is default mode.  devno>=0 queries specific DCB
*/
double dcb_get_exp_trigger_delay(int devno)
{
unsigned int ui;		// need 32 bits
int gsdevno=0;

if (devno>=0 && devno<NDCB)
	gsdevno = devno;
else if (devno>0)
	{
	printf("Illegal DCB device number: %d\n", devno);
	return -1;
	}

ui = (unsigned int)dcb_read_register(gsdevno, DCB_EXP_TRIG_DELAY_HI_REG);
ui <<= 16;
ui |= (unsigned int)dcb_read_register(gsdevno, DCB_EXP_TRIG_DELAY_LOW_REG);
return (dcb_clock_period * (double)ui);
}



static void dcb_read_measured_exposure_time(void)
{
long long ll;		// need 48 bits
ll = (long long)dcb_read_register(0, DCB_ENB_TIME_MEAS_HI_REG);
ll <<= 16;
ll |= (long long)dcb_read_register(0, DCB_ENB_TIME_MEAS_MED_REG);
ll <<= 16;
ll |= (long long)dcb_read_register(0, DCB_ENB_TIME_MEAS_LOW_REG);
measured_exposure_time = (dcb_clock_period * (double)ll);
return;
}



double dcb_get_measured_exposure_time(void)
{return measured_exposure_time;}



double dcb_get_ref_counter_time(void)
{
unsigned int ui;
ui = (unsigned int)dcb_read_register(0, DCB_REF_COUNTER_HI_REG);
ui <<= 16;
ui |= (unsigned int)dcb_read_register(0, DCB_REF_COUNTER_LOW_REG);
return (dcb_clock_period * (double)ui);
}



double bcb_get_ref_counter_time(void)
{
#if ENABLE_BCBS
 unsigned int ui;
 ui = (unsigned int)dcb_read_register(0, BCB_REF_COUNTER_HI_REG);
 ui <<= 16;
 ui |= (unsigned int)dcb_read_register(0, BCB_REF_COUNTER_LOW_REG);
 return (bcb_clock_period * (double)ui);
#else
 return 0.0;
#endif
}



double dcb_get_debounce_time(void)
{
long long ll;
ll = (long long)dcb_read_register(0, DCB_DEBOUNCE_TIME_HI_REG);
ll <<= 16;
ll |= (long long)dcb_read_register(0, DCB_DEBOUNCE_TIME_LOW_REG);
return (dcb_clock_period * (double)ll);
}



/*
** Calculate the remaining time in an exposure
**
** read the high-order 32-bits of the exposure timer register in dcb 0
** each lsb in the register represents ~1 ms in a 67 MHz design
** if no exposure is in progress, the registers return -1 (16-bit);
**
** if the exposure start time is in the future, it means we are waiting
** for the previous exposure period to finish, or are between exposures.
**
** Note that the granularity of the DCB_EXPSR_TIMER_LOW_REG is ~1 ms,
** so we do not read it if we expect the answer to be 0
*/
double dcb_calc_remaining_time(void)
{
double t, tn=gs_time();

// remaining time in exposure, if we are in the middle of the exposure
t = (exposure_start_time - tn)*clock_skew_factor + exposure_time +
		(nexppf-1)*exposure_period;

TDBG("t = %lf, est = %lf, tn = %lf, csf = %lf, et = %lf\n",
		t, exposure_start_time, tn, clock_skew_factor, exposure_time)

// time remainng reg is always 0 during external enable, so cannot be used
if (external_enable)
	{
	if (first_exposure_flag)
		t = dma_holdoff_time - tn;
	else
		t = exposure_start_time + (nexppf-1)*exposure_period +
			exposure_time - tn;
	t=min(4.0, t);		// allow "K" from client to get in
	return -t;
	}

// if the start is in the future, return time until start as a negative
if (tn < exposure_start_time)
	return (tn - exposure_start_time);

// negative t means we are late - return 0
t = (t<0.0) ? 0.0 : t;

// the hardware register holds the remaining time in current exposure
if (nexppf > 1)
	return t;

// avoid reading DCB_EXPSR_TIMER_REG if dma may happen
// register returns 0 in the idle period between exposures
if (t > (10.0 * 65536.0 * dcb_clock_period) )		// 10 lsb's ~ 10 ms
	{
	t = dcb_get_remaining_time();					// the real remaining time
	TDBG("hardware t = %lf\n", t)
	if (t>exposure_time)
		{
		printf("*** ERROR reading remaining time\n");
		DBG(1, "*** ERROR reading remaining time\n")
		}
	if (t == 0.0)		// 0 means not exposing: exposure will begin soon
		t += exposure_time;
	}

return t;
}



/*
** Read the 32-bit exposure timer register and return the time in seconds.
** Reading the register very close to the end of an exposure is an error, 
** since the result can be mixed up with DMA.
** This register contains the upper 32 bits (out of 48) of the exposure
** timer, so 1 LSB is ~1 ms at 67 MHz.
** The register holds zero between exposures in a sequence, and is -1 when
** no exposure is scheduled.
** Since this is a running counter, it is possible that there would be a
** carry from the low order word to the high order word between register
** reads.  To avoid this, it is arranged that reading the high order word
** latches the entire value.  Thus, it is important to read the high order
** word first.
*/
double dcb_get_remaining_time(void)
{
double t;
unsigned int ui;	// 32 bits

ui = dcb_read_register(0, DCB_EXPSR_TIMER_HI_REG);
ui <<= 16;
ui |= dcb_read_register(0, DCB_EXPSR_TIMER_LOW_REG);
t = 65536.0 * dcb_clock_period * (double)ui;
if (ui == 0xffffffff)
	t = -t;		// a large negative number
return t;
}



// number of exposures per transmitted frame - set all dcb's
int dcb_set_exposure_count_limit(unsigned int count)
{
int gsdevno;

switch (dcb_firmware_build)
	{
	// older versions
  	case 0x08c0:		// before limo swap, single-module
	case 0x096c:		// old-style fillpix, single-module
	case 0x0a9c:		// multi-extt, single-module
	case 0x0a11:		// new-style fillpix, single-module
	// multi-module
	case 0x0a10:		// old, multi-module
	case 0x0a9d:		// multi-extt, multi-module
	case 0x0ca8:		// multi-extt, multi-module
		if (count > 0xffff)
			{
			printf("*** Firmware does not support exposure count >0xffff\n");
			return -1;
			}
		for (gsdevno = 0; gsdevno<NDCB; gsdevno++)
			if (dcb_write_register(gsdevno, DCB_EXP_LIMIT_REG_LOW, (count & 0xffff)))
				return -1;
		break;
	// newer versions
	case 0x0d30:		// 32-bit nexpf, multi-module
	case 0x0d31:		// 32-bit nexpf, single-module
	case 0x0d78:		// improved multi-trigger, single-module
	case 0x0d94:		// enable holdoff, single-module
	case 0x0d98:		// optional parallel write, single-module
	case 0x10f8:		// temp & hdty lower limits, single module
	case 0x1658:		// port to Xilinx tools, single module
	case 0x1ad0:		// corrected DCB_MIN_READ_TIME_REG, single-module
	case 0x1c54:		// T: 55/15, 45/15, 45/15, H: 80, 80, 30, single-module
	// multi-module
	case 0x10f9:		// use with BCB build 10f8, multi-module
	case 0x165c:		// port to Xilinx tools, use with BCB build 0x161c, multi-module
	case 0x1a3c:		// 45C temp limit chan 2, use with BCB build 0x161c, multi-module
	case 0x1ac8:		// 45C limit chan 2, power up enable depends on DIP SW4, multi-module
	case 0x1b60:		// DCB_MIN_READ_TIME_REG R/W, 3 T sensors, 15C lo T lim, 12 bad reads needed, multi-module
	case 0x1bb8:		// 30% RH max on chan. 2
		for (gsdevno = 0; gsdevno<NDCB; gsdevno++)
			{
			if (dcb_write_register(gsdevno, DCB_EXP_LIMIT_REG_LOW, (count & 0xffff)))
				return -1;
			if (dcb_write_register(gsdevno, DCB_EXP_LIMIT_REG_HI, ((count>>16) & 0xffff)))
				return -1;
			}
		break;
	default:
		printf("*** Missing case(D): dcb_firmware_build = 0x%.4x\n", dcb_firmware_build);
		break;
	}
exp_count_in_frame = count;

return 0;
}



// number of frames to transmit - set all dcb's
int dcb_set_tx_frame_limit(int lim)
{
int gsdevno;

for (gsdevno = 0; gsdevno<NDCB; gsdevno++)
	if (dcb_write_register(gsdevno, DCB_TX_FR_LIM_REG, (lim & 0xffff)))
		return -1;
return 0;
}



// exposures in last frame, set just after DMA
// it is an error to query the exposures_in_frame register at other times
unsigned int dcb_get_exposures_in_frame(void)
{
return exposures_in_frame;
}



// stop exposure or calpix mode
int dcb_stop(void)
{
int gsdevno=0;

TDBG("--- dcb_stop()\n")

in_calpix_mode = 0;		// stop turns off calpix
if (longPeriod)			// stop count-down printing
	{
	printf("\b\b\b\b\b\b\b       \b\b\b\b\b\b\b");
	fflush(stdout);
	longPeriod=0;
	}
for (gsdevno = 0; gsdevno<NDCB; gsdevno++)
	{
	gs_clear_irq(gsdevno);
	if (dcb_write_command(gsdevno, DCB_STOP, 0))
		return -1;
	}
return 0;
}



unsigned int dcb_get_exposure_count_limit(void)
{
unsigned int ui=0;

switch (dcb_firmware_build)
	{
	// newer versions
	case 0x0d31:		// 32-bit nexpf, single-module
	case 0x0d30:		// 32-bit nexpf, multi-module
	case 0x0d78:		// improved multi-trigger, single-module
	case 0x0d94:		// enable holdoff, single-module
	case 0x0d98:		// optional parallel write, single-module
	case 0x10f8:		// temp & hdty lower limits, single module
	case 0x1658:		// port to Xilinx tools, single module
	case 0x1ad0:		// corrected DCB_MIN_READ_TIME_REG, single-module
	case 0x1c54:		// T: 55/15, 45/15, 45/15, H: 80, 80, 30, single-module
	// multi-module
	case 0x10f9:		// use with BCB build 10f8, multi-module
	case 0x165c:		// port to Xilinx tools, use with BCB build 0x161c, multi-module
	case 0x1a3c:		// 45C temp limit chan 2, use with BCB build 0x161c, multi-module
	case 0x1ac8:		// 45C limit chan 2, power up enable depends on DIP SW4, multi-module
	case 0x1b60:		// DCB_MIN_READ_TIME_REG R/W, 3 T sensors, 15C lo T lim, 12 bad reads needed, multi-module
	case 0x1bb8:		// 30% RH max on chan. 2
		ui = dcb_read_register(0, DCB_EXP_LIMIT_REG_HI)<<16;
	// older versions
  	case 0x08c0:		// before limo swap, single-module
	case 0x096c:		// old-style fillpix, single-module
	case 0x0a11:		// new-style fillpix, single-module
	case 0x0a10:		// old, multi-module
	case 0x0a9c:		// multi-extt, single-module
	case 0x0a9d:		// multi-extt, multi-module
	case 0x0ca8:		// multi-extt, multi-module
		ui |= 0xffff&dcb_read_register(0, DCB_EXP_LIMIT_REG_LOW);
		break;
	default:
		printf("*** Missing case(E): dcb_firmware_build = 0x%.4x\n", dcb_firmware_build);
		break;
	}

return ui;
}



int dcb_get_tx_frame_limit(void)
{return dcb_read_register(0, DCB_TX_FR_LIM_REG);}

int dcb_get_tx_frame_count(void)
{return dcb_read_register(0, DCB_TX_FR_COUNT_REG);}



/*
** this register must be read only immediately after an interrupt,
** so we keep this as a private function
*/
static unsigned int dcb_get_exp_count_in_frame(void)
{
unsigned int ui=0;		// need 32 bits

// must select a single module for reading
// after auto_read, module and/or bank address are arbitrary
dcb_set_bank_module_address(1, 1);

switch (dcb_firmware_build)
	{
	// newer versions
	case 0x0d30:		// 32-bit nexpf, multi-module
	case 0x0d31:		// 32-bit nexpf, single-module
	case 0x0d78:		// improved multi-trigger, single-module
	case 0x0d94:		// enable holdoff, single-module
	case 0x0d98:		// optional parallel write, single-module
	case 0x10f8:		// temp & hdty lower limits, single module
	case 0x1658:		// port to Xilinx tools, single module
	case 0x1ad0:		// corrected DCB_MIN_READ_TIME_REG, single-module
	case 0x1c54:		// T: 55/15, 45/15, 45/15, H: 80, 80, 30, single-module
	// multi-module
	case 0x10f9:		// use with BCB build 10f8, multi-module
	case 0x165c:		// port to Xilinx tools, use with BCB build 0x161c, multi-module
	case 0x1a3c:		// 45C temp limit chan 2, use with BCB build 0x161c, multi-module
	case 0x1ac8:		// 45C limit chan 2, power up enable depends on DIP SW4, multi-module
	case 0x1b60:		// DCB_MIN_READ_TIME_REG R/W, 3 T sensors, 15C lo T lim, 12 bad reads needed, multi-module
	case 0x1bb8:		// 30% RH max on chan. 2
		ui = dcb_read_register(0, DCB_BCB_EXPSR_COUNT_REG_HI)<<16;
	// older versions
	case 0x0a10:		// old, multi-module
	case 0x0a9d:		// multi-extt, multi-module
	case 0x0ca8:		// multi-extt, multi-module
  	case 0x08c0:		// before limo swap, single-module
	case 0x096c:		// old-style fillpix, single-module
	case 0x0a11:		// new-style fillpix, single-module
	case 0x0a9c:		// multi-extt, single-module
		ui |= 0xffff&dcb_read_register(0, DCB_BCB_EXPSR_COUNT_REG_LOW);
		break;
	default:
		printf("*** Missing case(F): dcb_firmware_build = 0x%.4x\n", dcb_firmware_build);
		break;
	}

return ui;
}



/*
** Do not read this register during readout.  Get exposure count in frame
** since last readout.
*/
static unsigned int dcb_get_exp_count_in_frame_snc_read(void)
{
unsigned int ui=0;

// must select a single module for reading
// after auto_read, module and/or bank address are arbitrary
dcb_set_bank_module_address(1, 1);

switch (dcb_firmware_build)
	{
	// older versions
	case 0x0d30:		// 32-bit nexpf, multi-module
	case 0x0a9d:		// multi-extt, multi-module
	case 0x0ca8:		// multi-extt, multi-module
	case 0x0d31:		// 32-bit nexpf, single-module
	case 0x0a10:		// old, multi-module
  	case 0x08c0:		// before limo swap, single-module
	case 0x096c:		// old-style fillpix, single-module
	case 0x0a11:		// new-style fillpix, single-module
	case 0x0a9c:		// multi-extt, single-module
		break;
	// newer versions
	case 0x0d78:		// improved multi-trigger, single-module
	case 0x0d94:		// enable holdoff, single-module
	case 0x0d98:		// optional parallel write, single-module
	case 0x10f8:		// temp & hdty lower limits, single module
	case 0x1658:		// port to Xilinx tools, single module
	case 0x1ad0:		// corrected DCB_MIN_READ_TIME_REG, single-module
	case 0x1c54:		// T: 55/15, 45/15, 45/15, H: 80, 80, 30, single-module
	// multi-module
	case 0x10f9:		// use with BCB build 10f8, multi-module
	case 0x165c:		// port to Xilinx tools, use with BCB build 0x161c, multi-module
	case 0x1a3c:		// 45C temp limit chan 2, use with BCB build 0x161c, multi-module
	case 0x1ac8:		// 45C limit chan 2, power up enable depends on DIP SW4, multi-module
	case 0x1b60:		// DCB_MIN_READ_TIME_REG R/W, 3 T sensors, 15C lo T lim, 12 bad reads needed, multi-module
	case 0x1bb8:		// 30% RH max on chan. 2
		ui = dcb_read_register(0, DCB_BCB_EXPSRS_SNC_READ_HI)<<16;
		ui |= 0xffff&dcb_read_register(0, DCB_BCB_EXPSRS_SNC_READ_LO);
		break;
	default:
		printf("*** Missing case(G): dcb_firmware_build = 0x%.4x\n", dcb_firmware_build);
		break;
	}

return ui;

}




/*
**  dcb_read_register() -- read 16 bits from a DCB register and
**  verfiy the response (acknowledge)
*/
unsigned int dcb_read_register(int devno, unsigned int reg)
{
unsigned int udword, buf=0;
int i, n;

udword = DCB_READ;
udword |= reg;
dcb_write_command_no_ack(devno, udword);
for (i=n=0; i<50 && !n; i++)
	n = gs_rx_data(devno, &buf, 4);
if (!n)
	{
	printf("(dcblib.c) -- ERROR reading DCB %d register\n", devno);
	DBG(4, "(dcblib.c) -- ERROR reading DCB %d register\n", devno);
	udword = gs_status(devno);
	if (udword & (GS_RX_LINK_ERR | GS_TX_LINK_ERR) )
		{
		printf("(dcblib.c) -- ERROR DCB %d link error\n", devno);
		DBG(4, "(dcblib.c) -- ERROR DCB %d link error\n", devno);
		}
	else
		{
		printf("(dcblib.c) -- ERROR DCB %d status = 0x%x\n", devno, udword);
		DBG(4, "(dcblib.c) -- ERROR DCB %d status = 0x%x\n", devno, udword);
		}
	}
TDBG("Read %d bytes from DCB %d; data = 0x%x; %s\n", n, devno, buf, intrprt_reg(reg))
return (0xffff & buf);
}



/*
**  dcb_write_register() --  write 16 bits to a DCB register and
**  verify the response (acknowledge)
*/
int dcb_write_register(int devno, unsigned int reg, unsigned int data)
{
return dcb_write_command(devno, DCB_WRITE, (reg | (0xffff & data)) );
}



/*
**  dcb_set_register_bits() --  set a pattern of bits in a DCB register
*/
int dcb_set_register_bits(int devno, unsigned int reg, unsigned int data)
{
return dcb_write_command(devno, DCB_SET, (reg | (0xffff & data)) );
}




/*
**  dcb_clear_register_bits() --  reset a pattern of bits in a DCB register
*/
int dcb_clear_register_bits(int devno, unsigned int reg, unsigned int data)
{
return dcb_write_command(devno, DCB_CLEAR, (reg | (0xffff & data)) );
}



/*
**  dcb_write_command() - write command with 24 bits of data,
**  and verify the response (acknowledge)
**  The acknowledge is actually a read of the register after the
**  requested action.  Therefore, setting and clearing bit patterns
**  requires special treatment.  Also, the reset register always returns 0.
**  N.B.: do not use this for commands which have NO acknowledge
*/
int dcb_write_command(int devno, unsigned int command, unsigned int data)
{
unsigned int t=0, udword, reg;
int i, n;

udword = command;
udword |= (0xffffff & data);

TDBG("writing    dcb %d  0x%x: %s\n", devno, udword, intrprt_cmd(udword))

if (in_calpix_mode)
	{
	switch (command)
		{
		case DCB_INSERT_COL_TOKEN:
		case DCB_INSERT_ROW_TOKEN:
		case DCB_INSERT_ROW_COL_TOKEN:
		case DCB_ADVANCE_COL_TOKEN:
		case DCB_ADVANCE_ROW_TOKEN:
		case DCB_ADVANCE_ROW_COL_TOKEN:
		case DCB_RESET_ROW_COL:
		case DCB_CALPIX:
		case DCB_CALPIX_X:
			break;
		default:
			dcb_stop();		// turn off calpix mode
			break;
		}
	}
else if (command==DCB_CALPIX || command==DCB_CALPIX_X)
	in_calpix_mode = 1;

gs_tx_data(devno, &udword, 4);
// e.g., DCB_WRITE_DATA needs i = 1231 wait loops
// e.g., DCB_N_CALIBRATE needs i = 403595 for 100 calibrates
for(i=n=0; i<500 && !n; i++)		// wait a little
	n=gs_rx_data(devno, &t, 4);
if (n && (t == udword))				// normal (good) result
	return 0;

// the reset registers return 0 as acknowledge
if ( n && ((data&0xff0000) == DCB_RESET_REG) && (t == (udword&0xffff0000)) )
	return 0;

if ( n && ((data&0xff0000) == BCB_RESET_REG) && (t == (udword&0xffff0000)) )
	return 0;

// temp/humidity reg returns status info
if ( n && ((data&0xff0000) == DCB_T_H_SENSOR_STATUS_REG) && 
		(udword == (t&0xffff0007)) )
	return 0;

switch (command)
	{
	// some commands take longer
# if ENABLE_BCBS
	case DCB_READOUT_MODULES:	// a no ACK command in single module
#endif
	case DCB_WRITE_DATA:
	case DCB_TRIM_ALL_VAL:
	case DCB_N_CALIBRATE:
	case DCB_TRIM_PIXELS:
		for(i=n=0; i<5000 && !n; i++)
			{
			usleep(1000);		// ~3 ms
			n=gs_rx_data(devno, &t, 4);
			}
		if (n && (t == udword))
			return 0;
		break;
	case DCB_SET:				// check only the affected bits
		if (n && (((t&0xffff0000) | (data&0xffff)) == udword))
			return 0;
		break;
	case DCB_CLEAR:				// check only the affected bits
		if (n && (((t&0xffff0000) | (~t&data&0xffff)) == udword))
			return 0;
		break;
	case DCB_WRITE:		// T/H ack is previous setting, not present command
		reg = data & 0xff0000;
		if (reg == DCB_TEMP_ALERT_THR_REG ||
			reg == DCB_TEMP_ALERT_LOW_THR_REG ||
			reg == DCB_HDTY_ALERT_THR_REG ||
			reg == DCB_HDTY_ALERT_LOW_THR_REG )
				return 0;
		break;
	default:
		break;
	}

if (n)	// if we got an ack, but not correct, dwords may be stuck in the fifo
	{
	int n, t, tt;							// shadow the outer params

	DBG(3, "Retrying acknowledge - returned dword = 0x%x, command = 0x%x\n", t, udword);

	while(gs_rx_data(devno, &tt, 4))		// get the last dword in the fifo
		t=tt;
	n=4;

	if (n && (t == udword))				// normal (good) result
		return 0;

	// the reset registers return 0 as acknowledge
	if ( n && ((data&0xff0000) == DCB_RESET_REG) && (t == (udword&0xffff0000)) )
		return 0;

	if ( n && ((data&0xff0000) == BCB_RESET_REG) && (t == (udword&0xffff0000)) )
		return 0;

	// temp/humidity reg returns status info
	if ( n && ((data&0xff0000) == DCB_T_H_SENSOR_STATUS_REG) && 
			(udword == (t&0xffff0007)) )
		return 0;

	switch (command)
		{
		// some commands take longer
	# if ENABLE_BCBS
		case DCB_READOUT_MODULES:	// a no ACK command in single module
	#endif
		case DCB_WRITE_DATA:
		case DCB_TRIM_ALL_VAL:
		case DCB_N_CALIBRATE:
		case DCB_TRIM_PIXELS:
			for(i=n=0; i<5000 && !n; i++)
				{
				usleep(1000);		// ~3 ms
				n=gs_rx_data(devno, &t, 4);
				}
			if (n && (t == udword))
				return 0;
			break;
		case DCB_SET:				// check only the affected bits
			if (n && (((t&0xffff0000) | (data&0xffff)) == udword))
				return 0;
			break;
		case DCB_CLEAR:				// check only the affected bits
			if (n && (((t&0xffff0000) | (~t&data&0xffff)) == udword))
				return 0;
			break;
		default:
			break;
		}
	printf("*** ERROR - retry failed in dcb_write_command()\n");
	DBG(4, "*** ERROR - retry failed in dcb_write_command()\n")
	}

if (n)
	switch (t & 0xff0000)
		{
		case 0xdd0000:
		case 0xff0000:
			printf("*** ERROR: illegal operation (negative acknowledge)\n");
			printf("n = %d, returned dword = 0x%x, command = 0x%x\n", n, t, udword);
			DBG(4, "*** ERROR: illegal operation (negative acknowledge)\n");
			DBG(4,"n = %d, returned dword = 0x%x, command = 0x%x\n", n, t, udword); 
			break;
		default:
			break;
		}
printf("*** (dcblib.c) -- ERROR writing DCB %d register\n", devno);
printf("n = %d, returned dword = 0x%x, command = 0x%x\n", n, t, udword);
DBG(4, "*** (dcblib.c) -- ERROR writing DCB %d register\n", devno);
DBG(4, "n = %d, returned dword = 0x%x, command = 0x%x\n", n, t, udword);
return -1;
}


/*
**  Some commands have no acknowledge:
**		DCB_TRIGGER_READSTROBE
**		DCB_EXPOSE
**		DCB_READOUT_MODULES  (only single module mode)
**		DCB_TRIGGER_AUTO_FIFO_READ
**		DCB_SET_IMAGE_TX_ENB
*/
int dcb_write_command_no_ack(int devno, unsigned int command)
{
if (in_calpix_mode)
	dcb_stop();
TDBG("writing na dcb %d  0x%x: %s\n", devno, command, intrprt_cmd(command))
gs_tx_data(devno, &command, 4);
return 0;
}



/*
** write data word to all modules
*/
int dcb_write_data_allpix(int data)
{
dcb_set_bank_module_address(15, 7);		// select all
return dcb_write_data_modpix(data);
}



/*
** write data word to all selected modules
*/
int dcb_write_data_modpix(int data)
{
int gsdevno;

data &= 0xfffff;

switch (dcb_firmware_build)
	{
	// older versions
  	case 0x08c0:		// before limo swap, single-module
	case 0x096c:		// old-style fillpix, single-module
		for (gsdevno=0; gsdevno<NDCB; gsdevno++)
			if (dcb_write_command(gsdevno, DCB_WRITE_DATA, data))
				return -1;
		break;
	// newer versions
	case 0x0a11:		// new-style fillpix, single-module
	case 0x0a9c:		// multi-extt, single-module
	case 0x0d31:		// 32-bit nexpf, single-module
	case 0x0d78:		// improved multi-trigger, single-module
	case 0x0d94:		// enable holdoff, single-module
	case 0x0d98:		// optional parallel write, single-module
	case 0x10f8:		// temp & hdty lower limits, single module
	case 0x1658:		// port to Xilinx tools, single module
	case 0x1ad0:		// corrected DCB_MIN_READ_TIME_REG, single-module
	case 0x1c54:		// T: 55/15, 45/15, 45/15, H: 80, 80, 30, single-module
	// multi-module
	case 0x0a10:		// old, multi-module
	case 0x0a9d:		// multi-extt, multi-module
	case 0x0ca8:		// multi-extt, multi-module
	case 0x0d30:		// 32-bit nexpf, multi-module
	case 0x10f9:		// use with BCB build 10f8, multi-module
	case 0x165c:		// port to Xilinx tools, use with BCB build 0x161c, multi-module
	case 0x1a3c:		// 45C temp limit chan 2, use with BCB build 0x161c, multi-module
	case 0x1ac8:		// 45C limit chan 2, power up enable depends on DIP SW4, multi-module
	case 0x1b60:		// DCB_MIN_READ_TIME_REG R/W, 3 T sensors, 15C lo T lim, 12 bad reads needed, multi-module
	case 0x1bb8:		// 30% RH max on chan. 2
		for (gsdevno=0; gsdevno<NDCB; gsdevno++)
			{
			if (dcb_write_register(gsdevno, DCB_BCB_WRITE_DATA_LOW_REG, data))	// load register
				return -1;
			if (dcb_write_register(gsdevno, DCB_BCB_WRITE_DATA_HI_REG, (data>>16) ))
				return -1;
			if (dcb_write_command(gsdevno, DCB_WRITE_DATA, data))	// load pixels - new dcb ignores data
				return -1;
			}
		break;
	default:
		printf("*** Missing case(H): dcb_firmware_build = 0x%.4x\n", dcb_firmware_build);
		break;
	}

return 0;
}


/*
** write data word to all selected modules using slow method
*/
int dcb_write_data_allpix_slow(int data)
{
int i, n, gsdevno;

for (gsdevno=0; gsdevno<NDCB; gsdevno++)
	{
	switch (dcb_firmware_build)
		{
		// older versions
 	 	case 0x08c0:		// before limo swap, single-module
		case 0x096c:		// old-style fillpix
			break;
		// newer versions
		case 0x0a11:		// new-style fillpix, single-module
		case 0x0a9c:		// multi-extt, single-module
		case 0x0d31:		// 32-bit nexpf, single-module
		case 0x0d78:		// improved multi-trigger, single-module
		case 0x0d94:		// enable holdoff, single-module
		case 0x0d98:		// optional parallel write, single-module
		case 0x10f8:		// temp & hdty lower limits, single module
		case 0x1658:		// port to Xilinx tools, single module
		case 0x1ad0:		// corrected DCB_MIN_READ_TIME_REG, single-module
		case 0x1c54:		// T: 55/15, 45/15, 45/15, H: 80, 80, 30, single-module
		// multi-module
		case 0x0a10:		// old, multi-module
		case 0x0a9d:		// multi-extt, multi-module
		case 0x0ca8:		// multi-extt, multi-module
		case 0x0d30:		// 32-bit nexpf, multi-module
		case 0x10f9:		// use with BCB build 10f8, multi-module
		case 0x165c:		// port to Xilinx tools, use with BCB build 0x161c, multi-module
		case 0x1a3c:		// 45C temp limit chan 2, use with BCB build 0x161c, multi-module
		case 0x1ac8:		// 45C limit chan 2, power up enable depends on DIP SW4, multi-module
		case 0x1b60:		// DCB_MIN_READ_TIME_REG R/W, 3 T sensors, 15C lo T lim, 12 bad reads needed, multi-module
		case 0x1bb8:		// 30% RH max on chan. 2
			if (dcb_write_register(gsdevno, DCB_BCB_WRITE_DATA_LOW_REG, data))	// load register
				return -1;
			if (dcb_write_register(gsdevno, DCB_BCB_WRITE_DATA_HI_REG, (data>>16) ))
				return -1;
			break;
		default:
			printf("*** Missing case(I): dcb_firmware_build = 0x%.4x\n", dcb_firmware_build);
			break;
		}
	dcb_reset_row_col(gsdevno);				// reset both shift registers
	dcb_insert_row_token(gsdevno);
	for(n=0;n<NROW_CHIP;n++)
		{
		dcb_insert_col_token(gsdevno);
		for(i=0;i<NCOL_CHIP;i++)
			{
			if (dcb_apply_din_value(gsdevno, data))
				return -1;
			dcb_advance_col_token(gsdevno);
			}
		dcb_advance_row_token(gsdevno);
		}
	}
return 0;
}


/*
** set all trims in the detector to a common value
*/
int dcb_trim_allpix(int data)
{
int gsdevno;

for (gsdevno=0; gsdevno<NDCB; gsdevno++)
	if (dcb_write_command(gsdevno, DCB_TRIM_ALL_VAL, (MAX_TRIM_VALUE & data) ))
		return -1;
return 0;
}




/*
** send calibrates to all selected modules
*/
int dcb_send_n_calibrates(int data)
{
int gsdevno;
dcb_reset_exposure_mode();
for (gsdevno=0; gsdevno<NDCB; gsdevno++)
	if (dcb_write_command(gsdevno, DCB_N_CALIBRATE, data))
		return -1;
return 0;
}




/*
**  set the software sampling delay setting 
*/
int dcb_set_delay_setting_all(int data)
{
int gsdevno;

dcb_set_bank_module_address(15, 7);		// select all
for (gsdevno=0; gsdevno<NDCB; gsdevno++)
	{
# if ENABLE_BCBS
	dcb_clear_register_bits(gsdevno, BCB_CTRL_REG, BCB_DELAY_SELECT);
#endif
	dcb_set_delay_setting(gsdevno, data);
	}
return 0;
}



// trigger enable holdoff - to delay the trigger after end of last transmission
// used to allow chips to settle before next exposure
static int dcb_set_enable_holdoff(double time)
{
unsigned int ui;			// need 32 bits
int gsdevno=0;

switch (dcb_firmware_build)
	{
	case 0x0d94:		// enable holdoff, single-module
	case 0x0d98:		// optional parallel write, single-module
	case 0x10f8:		// temp & hdty lower limits, single module
	case 0x1658:		// port to Xilinx tools, single module
	case 0x1ad0:		// corrected DCB_MIN_READ_TIME_REG, single-module
	case 0x1c54:		// T: 55/15, 45/15, 45/15, H: 80, 80, 30, single-module
	// multi-module
	case 0x10f9:		// use with BCB build 10f8, multi-module
	case 0x165c:		// port to Xilinx tools, use with BCB build 0x161c, multi-module
	case 0x1a3c:		// 45C temp limit chan 2, use with BCB build 0x161c, multi-module
	case 0x1ac8:		// 45C limit chan 2, power up enable depends on DIP SW4, multi-module
	case 0x1b60:		// DCB_MIN_READ_TIME_REG R/W, 3 T sensors, 15C lo T lim, 12 bad reads needed, multi-module
	case 0x1bb8:		// 30% RH max on chan. 2
		break;
	default:
//		printf(" Firmware 0x%.4x does not support enable holdoff\n", dcb_firmware_build);
		return 0;
		break;
	}

if (time < 0.0 || time > dcb_clock_period*(double)(-1+(1LL<<32)))
	{
	printf("Illegal enable holdoff time: %lf\n", time);
	return -1;
	}
ui = (unsigned int)rint(time/dcb_clock_period);
if (dcb_write_register(gsdevno, DCB_TRIG_EN_HOLDOFF_LO_REG, (ui & 0xffff)))
	return -1;
ui >>= 16;
if (dcb_write_register(gsdevno, DCB_TRIG_EN_HOLDOFF_HI_REG, (ui & 0xffff)))
	return -1;

#if INDEPENDENT_DCBs

for (gsdevno = 1; gsdevno<NDCB; gsdevno++)
	{
	ui = (unsigned int)rint(time/dcb_clock_period);
	if (dcb_write_register(gsdevno, DCB_TRIG_EN_HOLDOFF_LO_REG, (ui & 0xffff)))
		return -1;
	ui >>= 16;
	if (dcb_write_register(gsdevno, DCB_TRIG_EN_HOLDOFF_HI_REG, (ui & 0xffff)))
		return -1;
	}

#endif

return 0;
}




//----- trivial (low level) functions --------

int dcb_insert_col_token(int devno)
{return dcb_write_command(devno, DCB_INSERT_COL_TOKEN, 0);}

int dcb_insert_row_token(int devno)
{return dcb_write_command(devno, DCB_INSERT_ROW_TOKEN, 0);}

int dcb_insert_row_col_token(int devno)
{return dcb_write_command(devno, DCB_INSERT_ROW_COL_TOKEN, 0);}

int dcb_advance_col_token(int devno)
{return dcb_write_command(devno, DCB_ADVANCE_COL_TOKEN, 0);}

int dcb_advance_row_token(int devno)
{return dcb_write_command(devno, DCB_ADVANCE_ROW_TOKEN, 0);}

int dcb_advance_row_col_token(int devno)
{return dcb_write_command(devno, DCB_ADVANCE_ROW_COL_TOKEN, 0);}

int dcb_reset_row_col(int devno)
{return dcb_write_command(devno, DCB_RESET_ROW_COL, 0);}

int dcb_set_bank_address(int devno, int bank)
{return dcb_write_register(devno, DCB_BANK_ADD_REG, bank-devno*NBANK_DCB);}

int dcb_set_module_address(int devno, int mod)
{return dcb_write_register(devno, DCB_MOD_ADD_REG, mod);}

int dcb_set_bank_enable_pattern(int devno, unsigned int pat)
{return dcb_write_register(devno, DCB_BANK_ENABLE_REG, pat);}

int dcb_set_module_enable_pattern(int devno, unsigned int pat)
{return dcb_write_register(devno, DCB_MODULE_ENABLE_REG, pat);}

int dcb_reset_image_fifo(int devno)
{return dcb_write_command(devno, DCB_RESET_IMAGE_FIFO, 0);}

int dcb_image_fifo_enable(int devno)
{return dcb_write_command(devno, DCB_IMAGE_FIFO_ENABLE, 1);}

int dcb_image_fifo_disable(int devno)
{return dcb_write_command(devno, DCB_IMAGE_FIFO_ENABLE, 0);}

int dcb_trigger_psel(int devno)
{return dcb_write_command(devno, DCB_TRIGGER_PSEL, 0);}

int dcb_trigger_enable(int devno)
{return dcb_write_command(devno,DCB_TRIGGER_ENABLE, 0);}

int dcb_trigger_dcal(int devno, int data)
{return dcb_write_command(devno, DCB_TRIGGER_DCAL, data);}

int dcb_start_calpix(int devno)
{return dcb_write_command(devno, DCB_CALPIX, 0);}

int dcb_start_calpix_x(int devno)
{return dcb_write_command(devno, DCB_CALPIX_X, 0);}

int dcb_apply_din_value(int devno, int data)
{return dcb_write_command(devno, DCB_DIN_VALUE, data);}

int dcb_trim_pixels(int devno)
{return dcb_write_command(devno, DCB_TRIM_PIXELS, 0);}

int dcb_get_enable_state(int devno)
{return DCB_ENABLE_SIGNAL_STATE & dcb_read_register(devno, DCB_STATUS_REG);}

int dcb_get_bank_address(int devno)
{return 0xf & dcb_read_register(devno, DCB_BANK_ADD_REG);}

int dcb_get_module_address(int devno)
{return 0x7 & dcb_read_register(devno, DCB_MOD_ADD_REG);}

int dcb_get_dip_switch_setting(int devno)
{return 0x3ff & dcb_read_register(devno, DCB_DIP_SW_REG);}

int dcb_tx_frame_count_reset(int devno)
{return dcb_write_register(devno, DCB_RESET_REG, DCB_FRAME_COUNT_RESET);}	

int dcb_trim_fifo_reset(int devno)
{return dcb_write_register(devno, DCB_RESET_REG, DCB_TRIM_FIFO_RESET);}	

int dcb_trim_fifo_ld(int devno, int tval)
{return dcb_write_register(devno, DCB_BCB_TRIM_FIFO_IN_REG, 
		(MAX_TRIM_VALUE & tval) );	}

double dcb_overhead_time(void)
{return overhead_time;}



// DCB signal sampling delay
int dcb_get_delay_setting(int devno)
{
#if ENABLE_BCBS			// software delay only
	return 0xff & dcb_read_register(devno, DCB_BCB_DELAY_REG);
#else
int mode=DCB_DELAY_SELECT & dcb_read_register(devno, DCB_CTRL_REG);
if (mode)				// hardware delay
	return 0xff & dcb_read_register(devno, DCB_BCB_DELAY_REG)>>8;
else					// software delay
	return 0xff & dcb_read_register(devno, DCB_BCB_DELAY_REG);
#endif
}



// DCB signal sampling delay
int dcb_set_delay_setting(int devno, int val)
{
#if ENABLE_BCBS		// software delay only
	return dcb_write_register(devno, DCB_BCB_DELAY_REG, 0xff&val);
#else							// single module
int mode = DCB_DELAY_SELECT & dcb_read_register(devno, DCB_CTRL_REG);
int set = 0xff00 & dcb_read_register(devno, DCB_BCB_DELAY_REG);		// curent hardware setting
// pretend to write both hardware and software values to avoid read errors
if (mode && val)				// hardware mode && nonzero val: change to software
	{
	if (dcb_clear_register_bits(devno, DCB_CTRL_REG, DCB_DELAY_SELECT))
		return -1;
	return dcb_write_register(devno, DCB_BCB_DELAY_REG, set|(0xff&val));
	}
else if (val)					// software mode, new value
	return dcb_write_register(devno, DCB_BCB_DELAY_REG, set|(0xff&val));
else							// val==0 - switch back to hardware mode
	if (dcb_set_register_bits(devno, DCB_CTRL_REG, DCB_DELAY_SELECT))
		return -1;
#endif
return 0;
}



// TX enable gives no acknowledge because it could interefere
// with data already in the dcb fifo
// this signal does not require bank/module selection
int dcb_enable_image_tx(int gsdevno)
{
dcb_write_command_no_ack(gsdevno, DCB_SET_IMAGE_TX_ENB);
if (gsdevno==0)
	enable_time = gs_time();
return 0;
}




/*****************************************************************************\
**                                                                           **
**    Sensirion SHT11 / SHT71 temperature and humidity sensor                **
**                                                                           **
**    For Vdd = 3.3 V and using the 14-bit temperature sense                 **
**          T = -39.64 + 0.01 * SOt                                          **
**    where T = temperature in C, and SOt is temp sensor output              **
**                                                                           **
**    Using the 12-bit relative humidity sense                               **
**          RHl =  -4 + 0.0405 * SOrh - 2.8e-6 * SOrh * SOrh                 **
**    where RHl = relative humidity linear, and SOrh is rh sensor output     **
**    This gives negative values at very low humidities, which we truncate   **
**                                                                           **
**    Then,                                                                  **
**          RHt = (T - 25.0) * (0.01 + 0.00008 * SOrh) + RHl                 **
**    where RHt = true relative humidity                                     **
**                                                                           **
**    There can be up to 6 sensors per DCB.  Channels are numbered 0...5     **
**                                                                           **
**	Temperature preset limits:
**
**	The upper temperature thresholds are as follows:
**	DCB single, build 0x165c++, (only 0 and 1 are used):
**	ch 0 - 55C
**	ch 1 - 50C
**	ch 2 - 40C
**	ch 3 - 45C
**	ch 4 - 55C
**	ch 5 - 60C
**
**	DCB multi, build 0x1a3c++, (only 0, 1, and 2 are used):
**	ch 0 - 55C
**	ch 1 - 35C
**	ch 2 - 45C & 30% RH cutoff
-- after 0x1b60 the following are not present
**	ch 3 - 40C
**	ch 4 - 45C
**	ch 5 - 50C
**
**	Lower temperature preset is -39C.
**	Humidity preset limit is 80%
 \*****************************************************************************/

double dcb_read_temp(int channel)
{
int gsdevno = channel/6, c = (channel-6*gsdevno), i, n;
unsigned int udword, t, stat;

if (channel<0 || gsdevno>=NDCB)
	return -99.0;

if (!th_sensing_enabled[c][gsdevno])
	return -99.0;

// if the power is off, don't fill the screen with messages
udword = DCB_READ | DCB_T_H_SENSOR_STATUS_REG;
dcb_write_command_no_ack(gsdevno, udword);
for (i=n=0; i<50 && !n; i++)
	n = gs_rx_data(gsdevno, &t, 4);
if (!n)
	return -88.0;					// nobody home

// first, set the channel = c (0...5)
if (dcb_write_register(gsdevno, DCB_T_H_SENSOR_STATUS_REG, c))
	return -88.0;

// check for errors
// CRC & TX errors can also be seen in the DCB status register (0xF1)
stat = dcb_read_register(gsdevno, DCB_T_H_SENSOR_STATUS_REG);
if ( (stat & 1<<(4+c)) | (stat & 1<<(10+c)) )
	{
	dcb_write_register(gsdevno, DCB_RESET_REG,		// try a reset
			DCB_SHT_SOFT_RESET |
			DCB_SHT_COMM_RESET
			);
	sleep(2);
	stat = dcb_read_register(gsdevno, DCB_T_H_SENSOR_STATUS_REG);
	if ( (stat & 1<<(4+c)) | (stat & 1<<(10+c)) )
		return -88.0;
	}

/*
** Re-reading is no help for error condition because DCB is merely 
** returning the contents of a stored register, not actually re-reading
** the temperature.  The DCB re-reads the temp sensor every few seconds.
*/
t = dcb_read_register(gsdevno, DCB_TEMPERATURE_REG);

#ifdef DEBUG
if (t==0 || t>=0x3fff)
	DBG(1, "*** ERROR - Bad read (0x%x) from T sensor\n", t)
#endif

return -39.64 + 0.01 * t;
}




double dcb_read_humidity(int channel)
{
int h, gsdevno = channel/6, c = (channel-6*gsdevno);
unsigned int stat;

if (channel<0 || gsdevno>=NDCB)
	return -99.0;

if (!th_sensing_enabled[c][gsdevno])
	return -99.0;

// first, set the channel = c (0...5)
if (dcb_write_register(gsdevno, DCB_T_H_SENSOR_STATUS_REG, c))
	return -99.0;

// check for errors
// CRC & TX errors can also be seen in the DCB status register (0xF1)
stat = dcb_read_register(gsdevno, DCB_T_H_SENSOR_STATUS_REG);
if ( (stat & 1<<(4+c)) | (stat & 1<<(10+c)) )
	return -99.0;

/*
** Re-reading is no help for error condition because DCB is merely 
** returning the contents of a stored register, not actually re-reading
** the humidity.  The DCB re-reads the humid sensor every few seconds.
*/
h = dcb_read_register(gsdevno, DCB_HUMIDITY_REG);

#ifdef DEBUG
if (h==0 || h>=0xfff)
	DBG(1, "*** ERROR - Bad read (0x%x) from H sensor\n", h)
#endif

return max(0, (dcb_read_temp(channel) - 25.0)*(0.01 + 0.00008*h) -4.0 + 0.0405*h - 2.8e-6*h*h);
}




int dcb_sensor_heater_control(int channel, int control)
{
int gsdevno = channel/6, c = (channel-6*gsdevno);
unsigned int stat;

if (channel<0 || gsdevno>=NDCB)
	{
	printf("Illegal channel number: %i\n", channel);
	return -99;
	}

if (!th_sensing_enabled[c][gsdevno])
	return -99;

// first, set the channel = c
if (dcb_write_register(gsdevno, DCB_T_H_SENSOR_STATUS_REG, c))
	return -99;

// check for errors
// CRC & TX errors cab also be seen in the DCB status register (0xF1)
stat = dcb_read_register(gsdevno, DCB_T_H_SENSOR_STATUS_REG);
if ( (stat & 1<<(4+c)) | (stat & 1<<(10+c)) )
	{
	printf ("Cannot find a T/H sensor on DCB # %d, channel %d\n", gsdevno, c);
	return -99;
	}
// control <0 means query state
if (control>=1)					// turn on with 1
	dcb_set_register_bits(gsdevno, DCB_CTRL_REG, DCB_SHT_SENSOR_HEATER);
else if (control==0)			// turn off
	dcb_clear_register_bits(gsdevno, DCB_CTRL_REG, DCB_SHT_SENSOR_HEATER);

return DCB_SHT_SENSOR_HEATER & dcb_read_register(gsdevno, DCB_CTRL_REG);
}



// declarations
static double dcb_read_th_limit(int, int);
static int dcb_set_temp_limit(int, double, int);
static int dcb_set_humidity_limit(int, double, int);


double dcb_read_temp_high_limit(int channel)
{
double v = dcb_read_th_limit(channel, DCB_TEMP_ALERT_THR_REG);
if (v > -99.0)
	v = -39.64 + 0.01*v;
return v;
}

double dcb_read_temp_low_limit(int channel)
{
double v = dcb_read_th_limit(channel, DCB_TEMP_ALERT_LOW_THR_REG);
if (v > -99.0)
	v = -39.64 + 0.01*v;
return v;
}

double dcb_read_humidity_high_limit(int channel)
{
double h = dcb_read_th_limit(channel, DCB_HDTY_ALERT_THR_REG);
if (h > -99.0)
	h = -4.0 + 0.0405*h - 2.8e-6*h*h;		// limit value at 25 deg
return h;
}

double dcb_read_humidity_low_limit(int channel)
{
double h = dcb_read_th_limit(channel, DCB_HDTY_ALERT_LOW_THR_REG);
if (h > -99.0)
	h = -4.0 + 0.0405*h - 2.8e-6*h*h;		// limit value at 25 deg
return h;
}


int dcb_set_temp_high_limit(int channel, double lim)
{
return dcb_set_temp_limit(channel, lim, DCB_TEMP_ALERT_THR_REG);
}

int dcb_set_temp_low_limit(int channel, double lim)
{
return dcb_set_temp_limit(channel, lim, DCB_TEMP_ALERT_LOW_THR_REG);
}

int dcb_set_humidity_high_limit(int channel, double lim)
{
return dcb_set_humidity_limit(channel, lim, DCB_HDTY_ALERT_THR_REG);
}

int dcb_set_humidity_low_limit(int channel, double lim)
{
return dcb_set_humidity_limit(channel, lim, DCB_HDTY_ALERT_LOW_THR_REG);
}




static double dcb_read_th_limit(int channel, int reg)
{
int gsdevno = channel/6, c = (channel-6*gsdevno), i, n;
unsigned int udword, t, stat;

if (channel<0 || gsdevno>=NDCB)
	return -99.0;

if (!th_sensing_enabled[c][gsdevno])
	return -99.0;

// if the power is off, don't fill the screen with messages
udword = DCB_READ | DCB_T_H_SENSOR_STATUS_REG;
dcb_write_command_no_ack(gsdevno, udword);
for (i=n=0; i<50 && !n; i++)
	n = gs_rx_data(gsdevno, &t, 4);
if (!n)
	return -99.0;					// nobody home

// first, set the channel = c (0...5)
if (dcb_write_register(gsdevno, DCB_T_H_SENSOR_STATUS_REG, c))
	return -99.0;

// check for errors
// CRC & TX errors can also be seen in the DCB status register (0xF1)
stat = dcb_read_register(gsdevno, DCB_T_H_SENSOR_STATUS_REG);
if ( (stat & 1<<(4+c)) | (stat & 1<<(10+c)) )
	return -99.0;

return (double)dcb_read_register(gsdevno, reg);
}




static int dcb_set_temp_limit(int channel, double lim, int reg)
{
int gsdevno = channel/6, c = (channel-6*gsdevno), data;
unsigned int stat;

if (channel<0 || gsdevno>=NDCB)
	{
	printf("Illegal channel number: %i\n", channel);
	return -99;
	}

if (!th_sensing_enabled[c][gsdevno])
	return -99;

// first, set the channel = c
if (dcb_write_register(gsdevno, DCB_T_H_SENSOR_STATUS_REG, c))
	return -99;

// check for errors
// CRC & TX errors cab also be seen in the DCB status register (0xF1)
stat = dcb_read_register(gsdevno, DCB_T_H_SENSOR_STATUS_REG);
if ( (stat & 1<<(4+c)) | (stat & 1<<(10+c)) )
	{
	printf ("Cannot find a T/H sensor on DCB # %d, channel %d\n", gsdevno, c);
	return -99;
	}

// calculate binary value
data = (int)rint(100.0*(lim+39.64));
if (data<0 || data>0x3fff)
	return -99;
return dcb_write_register(gsdevno, reg, data);
}




static int dcb_set_humidity_limit(int channel, double lim, int reg)
{
int gsdevno = channel/6, c = (channel-6*gsdevno), data;
unsigned int stat;
double a, b, cc, t=0.0;

if (channel<0 || gsdevno>=NDCB)
	{
	printf("Illegal channel number: %i\n", channel);
	return -99;
	}

if (!th_sensing_enabled[c][gsdevno])
	return -99;

// first, set the channel = c
if (dcb_write_register(gsdevno, DCB_T_H_SENSOR_STATUS_REG, c))
	return -99;

// check for errors
// CRC & TX errors cab also be seen in the DCB status register (0xF1)
stat = dcb_read_register(gsdevno, DCB_T_H_SENSOR_STATUS_REG);
if ( (stat & 1<<(4+c)) | (stat & 1<<(10+c)) )
	{
	printf ("Cannot find a T/H sensor on DCB # %d, channel %d\n", gsdevno, c);
	return -99;
	}

// calculate binary value
a = 2.8e-6;
b = -0.0405;
cc = lim + 4.0;						// assume 25 deg
if ((b*b - 4.0*a*cc)>0)
	t = sqrt(b*b - 4.0*a*cc);
data = (int)rint((-b - t)/(2.0*a));		// this quadratic branch seems correct
if (data<0 || data>0xfff)
	return -99;

return dcb_write_register(gsdevno, reg, data);
}





/* ======================================================================== */
/*
** Functions to log hardware version numbers to a file
*/
extern char camstat_path[];
static int log_enable=0;

static void log_version_init(char *camera_name)
{
FILE *ofp;
char line[150];
struct utsname buf;

strcpy(line, camstat_path);
strcat(line, "HWVersions");

if ( (ofp = fopen(line, "w")) == NULL )
	{
	printf("Could not open file for writing: %s\n", line);
	return;
	}

fprintf(ofp, "%s\n", timestamp());
fprintf(ofp, "Code release: %s\n", release);
printf("...Code release: %s\n", release);
fprintf(ofp, "Compile date: %.2d%.2d%.2d\n", YEAR, MONTH, DAY);
fprintf(ofp, "Camera name: %s\n", camera_name);
fprintf(ofp, "Image format: %d(w) x %d(h) pixels\n", IMAGE_NCOL, IMAGE_NROW);
uname(&buf);
fprintf(ofp, "%s %s\n", buf.sysname, buf.release);

fclose(ofp);
log_enable=1;

return;
}



static void log_version(char *t)
{
FILE *ofp;
char line[150];

if (!log_enable)
	return;

strcpy(line, camstat_path);
strcat(line, "HWVersions");

if ( (ofp = fopen(line, "a")) == NULL )
	printf("Could not open file for appending: %s\n", line);
else
	{
	fprintf(ofp, "%s\n", t);
	fclose(ofp);
	}
}




/* ======================================================================== */
#define LINEWIDTH 150
void dcb_open_log(void)
{
int nlines=2000;
// or use dcb_get_tx_frame_limit()

return;		// turn off logging

if ( (logbuf=malloc(nlines*LINEWIDTH)) == NULL)
	printf ("dcblib() could not allocate memory\n");
memset(logbuf, 0, nlines*LINEWIDTH);
logbufP = logbuf;
logbufL = nlines*LINEWIDTH;
return;
}


void dcb_close_log(void)
{
char *p, *q;
FILE *ofp;

if (!logbuf)
	return;

if ( (ofp=fopen("/tmp/dcblog.txt", "w")) == NULL)
	printf("dcblib could not open file for writing\n");
else
	{
	if (logbuf && (logbufP-logbuf < logbufL-100))
		logbufP+=sprintf(logbufP, "===================== END =========================\n");
	p = logbuf;
	q = strchr(p, '\n');
	while (1)
		{
		if (q)
			*q = '\0';
		else
			break;
		fprintf(ofp, "%s\n", p);
		p = q+1;
		q = strchr(p, '\n');
		}
	fclose(ofp);
	}

free (logbuf);
logbuf = NULL;
logbufL = 0;
return;
}



/* ======================================================================== */
/* ======================================================================== */
/* ======================================================================== */


/*   test code to dump register contents
{int m;
// BCB registers
m=(0x1a<<18);TDBG(" content of reg 0x%x = 0x%x\n", m, dcb_read_register(0, m))
m=(0x1a<<19);TDBG(" content of reg 0x%x = 0x%x\n", m, dcb_read_register(0, m))
m=(0x1a<<16);TDBG(" content of reg 0x%x = 0x%x\n", m, dcb_read_register(0, m))
m=(0x1b<<16);TDBG(" content of reg 0x%x = 0x%x\n", m, dcb_read_register(0, m))
m=(0x1c<<16);TDBG(" content of reg 0x%x = 0x%x\n", m, dcb_read_register(0, m))
m=(0x30<<16);TDBG(" content of reg 0x%x = 0x%x\n", m, dcb_read_register(0, m))
m=(0x31<<16);TDBG(" content of reg 0x%x = 0x%x\n", m, dcb_read_register(0, m))
m=(0x32<<16);TDBG(" content of reg 0x%x = 0x%x\n", m, dcb_read_register(0, m))
m=(0x33<<16);TDBG(" content of reg 0x%x = 0x%x\n", m, dcb_read_register(0, m))
m=(0x35<<16);TDBG(" content of reg 0x%x = 0x%x\n", m, dcb_read_register(0, m))
m=(0x36<<16);TDBG(" content of reg 0x%x = 0x%x\n", m, dcb_read_register(0, m))
m=(0x37<<16);TDBG(" content of reg 0x%x = 0x%x\n", m, dcb_read_register(0, m))
m=(0x38<<16);TDBG(" content of reg 0x%x = 0x%x\n", m, dcb_read_register(0, m))
m=(0x3b<<16);TDBG(" content of reg 0x%x = 0x%x\n", m, dcb_read_register(0, m))
m=(0x3e<<16);TDBG(" content of reg 0x%x = 0x%x\n", m, dcb_read_register(0, m))
m=(0x3f<<16);TDBG(" content of reg 0x%x = 0x%x\n", m, dcb_read_register(0, m))
// DCB registers
m=(0x68<<16);TDBG(" content of reg 0x%x = 0x%x\n", m, dcb_read_register(0, m))
m=(0x69<<16);TDBG(" content of reg 0x%x = 0x%x\n", m, dcb_read_register(0, m))
m=(0x6a<<16);TDBG(" content of reg 0x%x = 0x%x\n", m, dcb_read_register(0, m))
m=(0x6b<<16);TDBG(" content of reg 0x%x = 0x%x\n", m, dcb_read_register(0, m))
m=(0x70<<16);TDBG(" content of reg 0x%x = 0x%x\n", m, dcb_read_register(0, m))
m=(0x71<<16);TDBG(" content of reg 0x%x = 0x%x\n", m, dcb_read_register(0, m))
m=(0x72<<16);TDBG(" content of reg 0x%x = 0x%x\n", m, dcb_read_register(0, m))
m=(0x73<<16);TDBG(" content of reg 0x%x = 0x%x\n", m, dcb_read_register(0, m))
m=(0x74<<16);TDBG(" content of reg 0x%x = 0x%x\n", m, dcb_read_register(0, m))
m=(0x75<<16);TDBG(" content of reg 0x%x = 0x%x\n", m, dcb_read_register(0, m))
m=(0x76<<16);TDBG(" content of reg 0x%x = 0x%x\n", m, dcb_read_register(0, m))
m=(0x77<<16);TDBG(" content of reg 0x%x = 0x%x\n", m, dcb_read_register(0, m))
m=(0xd0<<16);TDBG(" content of reg 0x%x = 0x%x\n", m, dcb_read_register(0, m))
m=(0xd1<<16);TDBG(" content of reg 0x%x = 0x%x\n", m, dcb_read_register(0, m))
m=(0xd2<<16);TDBG(" content of reg 0x%x = 0x%x\n", m, dcb_read_register(0, m))
m=(0xd3<<16);TDBG(" content of reg 0x%x = 0x%x\n", m, dcb_read_register(0, m))
m=(0xd4<<16);TDBG(" content of reg 0x%x = 0x%x\n", m, dcb_read_register(0, m))
m=(0xd5<<16);TDBG(" content of reg 0x%x = 0x%x\n", m, dcb_read_register(0, m))
m=(0xd6<<16);TDBG(" content of reg 0x%x = 0x%x\n", m, dcb_read_register(0, m))
m=(0xd7<<16);TDBG(" content of reg 0x%x = 0x%x\n", m, dcb_read_register(0, m))
m=(0xd8<<16);TDBG(" content of reg 0x%x = 0x%x\n", m, dcb_read_register(0, m))
m=(0xd9<<16);TDBG(" content of reg 0x%x = 0x%x\n", m, dcb_read_register(0, m))
m=(0xda<<16);TDBG(" content of reg 0x%x = 0x%x\n", m, dcb_read_register(0, m))
m=(0xdb<<16);TDBG(" content of reg 0x%x = 0x%x\n", m, dcb_read_register(0, m))
m=(0xdc<<16);TDBG(" content of reg 0x%x = 0x%x\n", m, dcb_read_register(0, m))
m=(0xdd<<16);TDBG(" content of reg 0x%x = 0x%x\n", m, dcb_read_register(0, m))
m=(0xde<<16);TDBG(" content of reg 0x%x = 0x%x\n", m, dcb_read_register(0, m))
m=(0xdf<<16);TDBG(" content of reg 0x%x = 0x%x\n", m, dcb_read_register(0, m))
m=(0xe0<<16);TDBG(" content of reg 0x%x = 0x%x\n", m, dcb_read_register(0, m))
m=(0xe1<<16);TDBG(" content of reg 0x%x = 0x%x\n", m, dcb_read_register(0, m))
m=(0xe2<<16);TDBG(" content of reg 0x%x = 0x%x\n", m, dcb_read_register(0, m))
m=(0xe3<<16);TDBG(" content of reg 0x%x = 0x%x\n", m, dcb_read_register(0, m))
m=(0xe4<<16);TDBG(" content of reg 0x%x = 0x%x\n", m, dcb_read_register(0, m))
m=(0xe5<<16);TDBG(" content of reg 0x%x = 0x%x\n", m, dcb_read_register(0, m))
m=(0xe6<<16);TDBG(" content of reg 0x%x = 0x%x\n", m, dcb_read_register(0, m))
m=(0xe7<<16);TDBG(" content of reg 0x%x = 0x%x\n", m, dcb_read_register(0, m))
m=(0xe8<<16);TDBG(" content of reg 0x%x = 0x%x\n", m, dcb_read_register(0, m))
m=(0xe9<<16);TDBG(" content of reg 0x%x = 0x%x\n", m, dcb_read_register(0, m))
m=(0xea<<16);TDBG(" content of reg 0x%x = 0x%x\n", m, dcb_read_register(0, m))
m=(0xeb<<16);TDBG(" content of reg 0x%x = 0x%x\n", m, dcb_read_register(0, m))
m=(0xec<<16);TDBG(" content of reg 0x%x = 0x%x\n", m, dcb_read_register(0, m))
m=(0xed<<16);TDBG(" content of reg 0x%x = 0x%x\n", m, dcb_read_register(0, m))
m=(0xee<<16);TDBG(" content of reg 0x%x = 0x%x\n", m, dcb_read_register(0, m))
m=(0xef<<16);TDBG(" content of reg 0x%x = 0x%x\n", m, dcb_read_register(0, m))
m=(0xf0<<16);TDBG(" content of reg 0x%x = 0x%x\n", m, dcb_read_register(0, m))
m=(0xf1<<16);TDBG(" content of reg 0x%x = 0x%x\n", m, dcb_read_register(0, m))
m=(0xf2<<16);TDBG(" content of reg 0x%x = 0x%x\n", m, dcb_read_register(0, m))
m=(0xf3<<16);TDBG(" content of reg 0x%x = 0x%x\n", m, dcb_read_register(0, m))
m=(0xf4<<16);TDBG(" content of reg 0x%x = 0x%x\n", m, dcb_read_register(0, m))
m=(0xf5<<16);TDBG(" content of reg 0x%x = 0x%x\n", m, dcb_read_register(0, m))
m=(0xf6<<16);TDBG(" content of reg 0x%x = 0x%x\n", m, dcb_read_register(0, m))
m=(0xf7<<16);TDBG(" content of reg 0x%x = 0x%x\n", m, dcb_read_register(0, m))
m=(0xf8<<16);TDBG(" content of reg 0x%x = 0x%x\n", m, dcb_read_register(0, m))
m=(0xf9<<16);TDBG(" content of reg 0x%x = 0x%x\n", m, dcb_read_register(0, m))
m=(0xfa<<16);TDBG(" content of reg 0x%x = 0x%x\n", m, dcb_read_register(0, m))
m=(0xfb<<16);TDBG(" content of reg 0x%x = 0x%x\n", m, dcb_read_register(0, m))
m=(0xfc<<16);TDBG(" content of reg 0x%x = 0x%x\n", m, dcb_read_register(0, m))
m=(0xfd<<16);TDBG(" content of reg 0x%x = 0x%x\n", m, dcb_read_register(0, m))
m=(0xfe<<16);TDBG(" content of reg 0x%x = 0x%x\n", m, dcb_read_register(0, m))
m=(0xff<<16);TDBG(" content of reg 0x%x = 0x%x\n", m, dcb_read_register(0, m))
}
*/


#ifdef DEBUG
static char cmdtxt[60];
static char *intrprt_cmd(unsigned int dword)
{
unsigned int cmd = dword & (0xff<<24);
strcpy(cmdtxt, "cmd = ");
switch (cmd)
	{
	case DCB_I2C_DAC1:
		strcat(cmdtxt, "DCB_I2C_DAC1");
		break;
	case DCB_I2C_DAC2:
		strcat(cmdtxt, "DCB_I2C_DAC2");
		break;
	case DCB_I2C_CHSEL:
		strcat(cmdtxt, "DCB_I2C_CHSEL");
		break;
	case DCB_TRIGGER_ENABLE:
		strcat(cmdtxt, "DCB_TRIGGER_ENABLE");
		break;
	case DCB_STOP:
		strcat(cmdtxt, "DCB_STOP");
		break;
	case DCB_EXPOSE:
		strcat(cmdtxt, "DCB_EXPOSE");
		break;
	case DCB_SET_IMAGE_TX_ENB:
		strcat(cmdtxt, "DCB_SET_IMAGE_TX_ENB");
		break;
	case DCB_TRIGGER_AUTO_FIFO_READ:
		strcat(cmdtxt, "DCB_TRIGGER_AUTO_FIFO_READ");
		break;
	case DCB_TRIGGER_READSTROBE:
		strcat(cmdtxt, "DCB_TRIGGER_READSTROBE");
		break;
	case DCB_INSERT_COL_TOKEN:
		strcat(cmdtxt, "DCB_INSERT_COL_TOKEN");
		break;
	case DCB_INSERT_ROW_TOKEN:
		strcat(cmdtxt, "DCB_INSERT_ROW_TOKEN");
		break;
	case DCB_INSERT_ROW_COL_TOKEN:
		strcat(cmdtxt, "DCB_INSERT_ROW_COL_TOKEN");
		break;
	case DCB_ADVANCE_COL_TOKEN:
		strcat(cmdtxt, "DCB_ADVANCE_COL_TOKEN");
		break;
	case DCB_ADVANCE_ROW_TOKEN:
		strcat(cmdtxt, "DCB_ADVANCE_ROW_TOKEN");
		break;
	case DCB_ADVANCE_ROW_COL_TOKEN:
		strcat(cmdtxt, "DCB_ADVANCE_ROW_COL_TOKEN");
		break;
	case DCB_RESET_ROW_COL:
		strcat(cmdtxt, "DCB_RESET_ROW_COL");
		break;
	case DCB_RESET_IMAGE_FIFO:
		strcat(cmdtxt, "DCB_RESET_IMAGE_FIFO");
		break;
	case DCB_IMAGE_FIFO_ENABLE:
		strcat(cmdtxt, "DCB_IMAGE_FIFO_ENABLE");
		break;
	case DCB_TRIGGER_PSEL:
		strcat(cmdtxt, "DCB_TRIGGER_PSEL");
		break;
	case DCB_TRIGGER_DCAL:
		strcat(cmdtxt, "DCB_TRIGGER_DCAL");
		break;
	case DCB_DIN_VALUE:
		strcat(cmdtxt, "DCB_DIN_VALUE");
		break;
	case DCB_CALPIX:
		strcpy(cmdtxt, "DCB_CALPIX");
		break;
	case DCB_CALPIX_X:
		strcat(cmdtxt, "DCB_CALPIX_X");
		break;
	case DCB_READOUT_MODULES:
		strcat(cmdtxt, "DCB_READOUT_MODULES");
		break;
	case DCB_WRITE_DATA:
		strcat(cmdtxt, "DCB_WRITE_DATA");
		break;
	case DCB_TRIM_ALL_VAL:
		strcat(cmdtxt, "DCB_TRIM_ALL_VAL");
		break;
	case DCB_TRIM_PIXELS:
		strcat(cmdtxt, "DCB_TRIM_PIXELS");
		break;
	case DCB_N_CALIBRATE:
		strcat(cmdtxt, "DCB_N_CALIBRATE");
		break;
	case DCB_READ:
		strcat(cmdtxt, "DCB_READ; ");
		strcat(cmdtxt, intrprt_reg(dword));
		break;
	case DCB_WRITE:
		strcat(cmdtxt, "DCB_WRITE;");
		strcat(cmdtxt, intrprt_reg(dword));
		break;
	case DCB_SET:
		strcat(cmdtxt, "DCB_SET;");
		strcat(cmdtxt, intrprt_reg(dword));
		break;
	case DCB_CLEAR:
		strcat(cmdtxt, "DCB_CLEAR;");
		strcat(cmdtxt, intrprt_reg(dword));
		break;
	default:
		strcpy(cmdtxt, "unknown");
	}
return cmdtxt;
}


static char regtxt[40];
static char *intrprt_reg(unsigned int dword)
{
unsigned int reg = dword & (0xff<<16);
strcpy(regtxt, " reg = ");
switch (reg)
	{
	case BCB_ROI_COL_SET_REG:
		strcat(regtxt, "BCB_ROI_COL_SET_REG");
		break;
	case BCB_ROI_ROW_SET_REG:
		strcat(regtxt, "BCB_ROI_ROW_SET_REG");
		break;
	case BCB__1C__REG:
		strcat(regtxt, "BCB__1C__REG");
		break;
	case BCB_CTRL_REG:
		strcat(regtxt, "BCB_CTRL_REG");
		break;
	case BCB__31__REG:
		strcat(regtxt, "BCB__31__REG");
		break;
	case BCB_FIFO_STATUS_REG:
		strcat(regtxt, "BCB_FIFO_STATUS_REG");
		break;
	case BCB_LED_REG:
		strcat(regtxt, "BCB_LED_REG");
		break;
	case BCB__35__REG:
		strcat(regtxt, "BCB__35__REG");
		break;
	case BCB__36__REG:
		strcat(regtxt, "BCB__36__REG");
		break;
	case BCB_RESET_REG:
		strcat(regtxt, "BCB_RESET_REG");
		break;
	case BCB_CLOCK_FREQ_REG:
		strcat(regtxt, "BCB_CLOCK_FREQ_REG");
		break;
	case BCB_FIRMWARE_BUILD_REG:
		strcat(regtxt, "BCB_FIRMWARE_BUILD_REG");
		break;
	case BCB_VERSION_REG:
		strcat(regtxt, "BCB_VERSION_REG");
		break;
	case DCB_BCB__68__REG:
		strcat(regtxt, "DCB_BCB__68__REG");
		break;
	case DCB_BCB_EXPSR_COUNT_REG_LOW:
		strcat(regtxt, "DCB_BCB_EXPSR_COUNT_REG_LOW");
		break;
	case DCB_BCB_EXPSR_COUNT_REG_HI:
		strcat(regtxt, "DCB_BCB_EXPSR_COUNT_REG_HI");
		break;
	case DCB_BCB_EXPSRS_SNC_READ_HI:
		strcat(regtxt, "DCB_BCB_EXPSRS_SNC_READ_HI");
		break;
	case DCB_BCB_EXPSRS_SNC_READ_LO:
		strcat(regtxt, "DCB_BCB_EXPSRS_SNC_READ_LO");
		break;
	case DCB_BCB__6A__REG:
		strcat(regtxt, "DCB_BCB__6A__REG");
		break;
	case DCB_BCB__6B__REG:
		strcat(regtxt, "DCB_BCB__6B__REG");
		break;
	case DCB_BCB_WRITE_DATA_HI_REG:
		strcat(regtxt, "DCB_BCB_WRITE_DATA_HI_REG");
		break;
	case DCB_BCB_WRITE_DATA_LOW_REG:
		strcat(regtxt, "DCB_BCB_WRITE_DATA_LOW_REG");
		break;
	case DCB_BCB_TRIM_FIFO_IN_REG:
		strcat(regtxt, "DCB_BCB_TRIM_FIFO_IN_REG");
		break;
	case DCB_BCB_DELAY_REG:
		strcat(regtxt, "DCB_BCB_DELAY_REG");
		break;
	case DCB_BCB__77__REG:
		strcat(regtxt, "DCB_BCB__77__REG");
		break;
	case DCB_TEMP_ALERT_THR_REG:
		strcat(regtxt, "DCB_TEMP_ALERT_THR_REG");
		break;
	case DCB_HDTY_ALERT_THR_REG:
		strcat(regtxt, "DCB_HDTY_ALERT_THR_REG");
		break;
	case DCB_TEMP_ALERT_LOW_THR_REG:
		strcat(regtxt, "DCB_TEMP_ALERT_LOW_THR_REG");
		break;
	case DCB_HDTY_ALERT_LOW_THR_REG:
		strcat(regtxt, "DCB_HDTY_ALERT_LOW_THR_REG");
		break;
	case DCB_TRIG_EN_HOLDOFF_HI_REG:
		strcat(regtxt, "DCB_TRIG_EN_HOLDOFF_HI_REG");
		break;
	case DCB_TRIG_EN_HOLDOFF_LO_REG:
		strcat(regtxt, "DCB_TRIG_EN_HOLDOFF_LO_REG");
		break;
	case DCB_DEBOUNCE_TIME_HI_REG:
		strcat(regtxt, "DCB_DEBOUNCE_TIME_HI_REG");
		break;
	case DCB_DEBOUNCE_TIME_LOW_REG:
		strcat(regtxt, "DCB_DEBOUNCE_TIME_LOW_REG");
		break;
	case DCB_REF_COUNTER_HI_REG:
		strcat(regtxt, "DCB_REF_COUNTER_HI_REG");
		break;
	case DCB_REF_COUNTER_LOW_REG:
		strcat(regtxt, "DCB_REF_COUNTER_LOW_REG");
		break;
	case DCB_ROI_COL_SET_REG:
		strcat(regtxt, "DCB_ROI_COL_SET_REG");
		break;
	case DCB_ROI_ROW_SET_REG:
		strcat(regtxt, "DCB_ROI_ROW_SET_REG");
		break;
	case DCB_ENB_TIME_MEAS_HI_REG:
		strcat(regtxt, "DCB_ENB_TIME_MEAS_HI_REG");
		break;
	case DCB_ENB_TIME_MEAS_MED_REG:
		strcat(regtxt, "DCB_ENB_TIME_MEAS_MED_REG");
		break;
	case DCB_ENB_TIME_MEAS_LOW_REG:
		strcat(regtxt, "DCB_ENB_TIME_MEAS_LOW_REG");
		break;
	case DCB_BANK_ADD_REG:
		strcat(regtxt, "DCB_BANK_ADD_REG");
		break;
	case DCB_MOD_ADD_REG:
		strcat(regtxt, "DCB_MOD_ADD_REG");
		break;
	case DCB__E2__REG:
		strcat(regtxt, "DCB__E2__REG");
		break;
	case DCB_EXP_LIMIT_REG_LOW:
		strcat(regtxt, "DCB_EXP_LIMIT_REG_LOW");
		break;
	case DCB_EXP_LIMIT_REG_HI:
		strcat(regtxt, "DCB_EXP_LIMIT_REG_HI");
		break;
	case DCB_TX_FR_LIM_REG:
		strcat(regtxt, "DCB_TX_FR_LIM_REG");
		break;
	case DCB_TX_FR_COUNT_REG:
		strcat(regtxt, "DCB_TX_FR_COUNT_REG");
		break;
	case DCB_EXP_TRIG_DELAY_HI_REG:
		strcat(regtxt, "DCB_EXP_TRIG_DELAY_HI_REG");
		break;
	case DCB_EXP_TRIG_DELAY_LOW_REG:
		strcat(regtxt, "DCB_EXP_TRIG_DELAY_LOW_REG");
		break;
	case DCB_EXP_TIME_HI_REG:
		strcat(regtxt, "DCB_EXP_TIME_HI_REG");
		break;
	case DCB_EXP_TIME_MED_REG:
		strcat(regtxt, "DCB_EXP_TIME_MED_REG");
		break;
	case DCB_EXP_TIME_LOW_REG:
		strcat(regtxt, "DCB_EXP_TIME_LOW_REG");
		break;
	case DCB_EXPSR_TIMER_HI_REG:
		strcat(regtxt, "DCB_EXPSR_TIMER_HI_REG");
		break;
	case DCB_EXPSR_TIMER_LOW_REG:
		strcat(regtxt, "DCB_EXPSR_TIMER_LOW_REG");
		break;
	case DCB_EXP_PERIOD_HI_REG:
		strcat(regtxt, "DCB_EXP_PERIOD_HI_REG");
		break;
	case DCB_EXP_PERIOD_MED_REG:
		strcat(regtxt, "DCB_EXP_PERIOD_MED_REG");
		break;
	case DCB_EXP_PERIOD_LOW_REG:
		strcat(regtxt, "DCB_EXP_PERIOD_LOW_REG");
		break;
	case DCB_CTRL_REG:
		strcat(regtxt, "DCB_CTRL_REG");
		break;
	case DCB_STATUS_REG:
		strcat(regtxt, "DCB_STATUS_REG");
		break;
	case DCB_FIFO_STATUS_REG:
		strcat(regtxt, "DCB_FIFO_STATUS_REG");
		break;
	case DCB_LED_REG:
		strcat(regtxt, "DCB_LED_REG");
		break;
	case DCB_DIP_SW_REG:
		strcat(regtxt, "DCB_DIP_SW_REG");
		break;
	case DCB_BANK_ENABLE_REG:
		strcat(regtxt, "DCB_BANK_ENABLE_REG");
		break;
	case DCB_MODULE_ENABLE_REG:
		strcat(regtxt, "DCB_MODULE_ENABLE_REG");
		break;
	case DCB_RESET_REG:
		strcat(regtxt, "DCB_RESET_REG");
		break;
	case DCB_CLOCK_FREQ_REG:
		strcat(regtxt, "DCB_CLOCK_FREQ_REG");
		break;
	case DCB_HALF_DWORD_REG:
		strcat(regtxt, "DCB_HALF_DWORD_REG");
		break;
	case DCB_T_H_SENSOR_STATUS_REG:
		strcat(regtxt, "DCB_T_H_SENSOR_STATUS_REG");
		break;
	case DCB_MIN_READ_TIME_REG:
		strcat(regtxt, "DCB_MIN_READ_TIME_REG");
		break;
	case DCB_TEMPERATURE_REG:
		strcat(regtxt, "DCB_TEMPERATURE_REG");
		break;
	case DCB_HUMIDITY_REG:
		strcat(regtxt, "DCB_HUMIDITY_REG");
		break;
	case DCB_FIRMWARE_BUILD_REG:
		strcat(regtxt, "DCB_FIRMWARE_BUILD_REG");
		break;
	case DCB_VERSION_REG	:
		strcat(regtxt, "DCB_VERSION_REG	");
		break;
	default:
		strcpy(regtxt, "unknown reg");
		break;
	}
return regtxt;
}
#endif		// DEBUG



/* ======================================================================== */
/* ======================================================================== */
/* ======================================================================== */


/* ======================================================================== */
/*
** test writing trims by dma
**
** By dma, this does NOT work.  Apparently a slight pause after each command
** is needed.  The #if statement allows either dma or sequential write.
**
Other changes required to implment this:
#define TX_MAPSIZE (NROW_CHIP * NCOL_CHIP * sizeof(unsigned int))
  in dcblib.h:
void dcb_trim_fifo_ld_dma(int, unsigned short *);
  in test5.c:
void dcb_trim_fifo_ld_dma(int a, unsigned short *b)
{return;}
  in settrims.c:
dcb_trim_fifo_ld_dma(devno, 
	(unsigned short *)&trimValues[selected_bank-1][selected_mod-1][selected_chip][0][0]);
(in place of the loop on dcb_trim_fifo_ld() )
**
*/


/* ======================================================================== */

#if 0
void dcb_trim_fifo_ld_dma(int gsdevno, unsigned short *trims)
{
char *txdmabuf=NULL;
int i, n, x, y;
unsigned int *bp, t;
double tt=0.0, ts;

printf("Entering dcb_trim_fifo_ld_dma\n");
TDBG("Entering dcb_trim_fifo_ld_dma\n");
gs_clear_irq(gsdevno);

// set a low burst size
gs_set_rx_burstsize(gsdevno, 0x1);

ts = gs_time();

// set interrupt on TX dma (i.e., dma read) finished & errors
gs_set_irq_mask(gsdevno,
				GS_RX_LINK_ERROR_INT |
				GS_TX_LINK_ERROR_INT |
		/*		GS_DMA_WR_DONE_INT |
				GS_DMA_WR_DIED_INT |
				GS_DMA_WR_STOP_INT |
				GS_DMA_WR_ABORT_INT |  */
				GS_DMA_RD_DONE_INT |
				GS_DMA_RD_OVFL_INT |
				GS_DMA_RD_STOP_INT |
				GS_DMA_RD_ABORT_INT
				);

// map pages for TX
if ( (txdmabuf = gs_tx_dma_setup(gsdevno, TX_MAPSIZE))==NULL )
	{
	printf("*** dcb_trim_fifo_ld_dma(), device %d: could not map pages\n", gsdevno);
	return;
	}
bp=(unsigned int *)txdmabuf;

// prepare data
for (y=0; y<NROW_CHIP; y++)
	for (x=0; x<NCOL_CHIP; x++)
		*bp++ = DCB_WRITE | DCB_BCB_TRIM_FIFO_IN_REG | *trims++;

#if 1		// if 1 use dma; if 0 use write loop
// enable interrupt using the mask set above
gs_set_control(gsdevno, GS_INT_ENABLE);

// enable dma write to host
gs_set_control(gsdevno, GS_FIRE_DMA_READ);

// wait for interrupt
n = gs_tx_dma(gsdevno);

// empty rx buffer
while ( (n = gs_rx_data(gsdevno, &t, 4)) )
	;

// capture status
t=gs_irq_src_status(gsdevno);

tt = gs_time() - ts;
while(tt<2.0 && !(t & GS_DMA_RD_DONE_INT))
	{
	t=gs_irq_src_status(gsdevno);
	tt = gs_time() - ts;
	usleep(1);
	printf("Looping looping...\n");		// never taken
	}

#else

// send the made-up instructions one-by-one with a pause
bp=(unsigned int *)txdmabuf;
for (y=0; y<NROW_CHIP; y++)
	for (x=0; x<NCOL_CHIP; x++)
		{
		gs_tx_data(gsdevno, bp++, 4);
		for(i=n=0; i<500 && !n; i++)		// wait a little
			n=gs_rx_data(gsdevno, &t, 4);
		}
tt = gs_time() - ts;

#endif

printf("Leaving dcb_trim_fifo_ld_dma - time = %.6lf - stat = 0x%x\n", tt, t);
TDBG("Leaving dcb_trim_fifo_ld_dma - time = %.6lf - stat = 0x%x\n", tt, t);

return;
}

#endif

