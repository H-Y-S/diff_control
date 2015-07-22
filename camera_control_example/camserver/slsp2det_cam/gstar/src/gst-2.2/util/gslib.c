/* gslib.c - an interface library for the PSI PMC GigaSTaR card
**
** Copyright (C) 2004-2010 Eric F. Eikenberry
** Paul Scherrer Institute, CH-5232 Villigen-PSI, Switzerland.
** All rights reserved.
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
** EF Eikenberry, Dec. 2004
*/


/*
  Note: in the case of multiple devices, the devices are probably numbered
0, 1, ... (see cat /proc/pci ).  This will give rise to correspondingly 
numbered directory entries ( gs0, gs1, ...) in the directory where the 
gsfs pseudo-filesystem is mounted.  But, the physical correspondance to
the devices at the back of the computer will have to be determined
experimentally (i.e. - is gs0 nearest the fan, or farthest away?).


List of functions in this library:

// initializing
int gs_initialize(char *);

// reset & start
int gs_start(int);
int gs_reset(int);
int gs_reset_all(int);

// reading status
int gs_status(int);
int gs_irq_status(int);
int gs_irq_src_status(int);
int gs_interrupt_status(int);
unsigned int *gs_read_bar0_reg(int, int, int);
double gs_last_interrupt_time(int);

// (re)setting control parameters
int gs_device_count(void);
int gs_set_control(int , unsigned int);
int gs_reset_control(int, unsigned int);
int gs_set_irq_mask(int, unsigned int);
int gs_clear_irq(int);
int gs_set_rx_burstsize(int, unsigned int );
int gs_set_rx_timeout(int, unsigned int);
int gs_set_tx_burstsize(int, unsigned int);
int gs_write_bar0_reg(int, int, unsigned int);

// programmed I/O data transfer
size_t gs_rx_data(int, void *, size_t);
size_t gs_tx_data(int, void *, size_t);

// dma setup and execution
void *gs_rx_dma_setup(int, unsigned int);
void *gs_tx_dma_setup(int, unsigned int);
size_t gs_rx_dma(int);
size_t gs_tx_dma(int);

// timer functions
int gs_set_t0(double);
double gs_time(void);
char *gs_timestamp(void);




      ---------- gs_initialize() ----------

int gs_initialize(char *path);

  where 'path' points to the gsfs mount point.  If path is NULL, look
for directory 'gsdev' in the user's home directory, or if that is not
found, use the default compiled-in below.

  After resolving the path:
    1) open the file descriptors for each of the component services
	2) initialize the timer

  Return value: 0 if no error; -1 if path or gigastar device(s) not found.



      ---------- gs_start() ----------

int gs_start(int dev);

  Start the PMC gigastar device 'dev'.  This consists of:
	 1) issue the run command (global control register bit 24)
	 2) set the device to the 'U' mode

  Return value: 0 if no error; -1 if the device is not initialized



      ---------- gs_reset() ----------

int gs_reset(int dev)

  Reset PMC gigastar device by writing 0xff to the global control register.
Requires gs_start() to be issued in order to continue.

  Return value: 0 if no error; -1 if the device is not initialized



      ---------- gs_reset_all() ----------

int gs_reset_all(int dev)

  Reset PMC gigastar device by writing 0xff to the global control register,
then unmap the DMA memory buffers, set the buffer sizes to zero, and set
the read and write buffer indexes to zero.  Requires gs_start() to be issued
in order to continue.

  Return value: 0 if no error; -1 if the device is not initialized



      ---------- gs_status() ----------

int gs_status(int dev)

  Read the gigastar status word (BAR0 + 0x3) of device 'dev'.

  Return value: status if no error; -1 if the device is not initialized



      ---------- gs_irq_status()  ----------

int gs_irq_status(int dev)

  Read the gigastar interrupt register (BAR0 + 0x6) of device 'dev'.  This
shows which conditon caused the last interrupt.

  Return value: irq status (the interrupt request source register) if
no error;  -1 if the device is not initialized



      ---------- gs_irq_src_status()  ----------

int gs_irq_src_status(int dev)

  Read the gigastar irq source status word (BAR0 + 0x4) of device 'dev'.
This shows what (transient) conditions that could potentially cause
interrupts have occurred since the last clear.  The actual cause of 
the interrupt is gotten from gs_irq_status().

  Return value: irq source status (the interrupt request source register)
if no error;  -1 if the device is not initialized



      ---------- gs_irterrupt_status()  ----------

int gs_interrupt_status(int dev)

  Read the driver interrupt busy flag of device 'dev'.
 
  Return value: 1 if interrupt busy is set, 0 if not set or error



      ---------- gs_read_bar0_reg() ----------

unsigned int *gs_read_bar0_reg(int dev, int offset, int count)

  Read 'count/4' registers at 'offset' in BAR0 of device 'dev'.

  Return value: pointer to the array of requested registers, or NULL
on error.



      ---------- gs_last_interrupt_time() ----------

double gs_last_interrupt_time(int dev)

  Read the recorded time of the last interrupt via ioctl()
  
  Return value: elapsed time in seconds since initialization with apparent
sub-micorsecond precsion.


      ---------- gs_device_count() ----------

int gs_device_count(void)

  Return value: the number of initialized PMC Gigastar cards; 0 indicates
that there are no cards, or that the driver has not been installed, or 
that initialization has not been performed.



      ---------- gs_set_control() ----------

int gs_set_control(int dev, unsigned int pattern)

  Write a bit pattern to the PMC Gigastar set control register (BAR0 + 0x1)
of device 'dev'.

  Return value: 0 if no error; -1 if the device is not initialized



      ---------- gs_reset_control() ----------

int gs_reset_control(int dev, unsigned int pattern)

  Write a bit 'pattern' to the PMC gigastar reset control register (BAR0 + 0x2)
of device 'dev'.

  Return value: 0 if no error; -1 if the device is not initialized



      ---------- gs_set_irq_mask() ----------

int gs_set_irq_mask(int dev, unsigned int pattern)

  Set a bit 'pattern' into the IRQ source mask of device 'dev'.  This is
used to select which conditions can cause interrupts.

  Return value: 0 if no error; -1 if the device is not initialized


      ---------- gs_clear_irq() ----------

int gs_clear_irq(int dev);

  Clear the irq's of device 'dev' by writing -1 to the interrupt source
register.

  Return value: 0 if no error; -1 if the device is not initialized



      ---------- gs_set_rx_burstsize() ----------

int gs_set_rx_burstsize(int dev, unsigned int pattern);

  Set the DMA burstsize for receive transactions (PMC Gigastar card receiver
to memory) for  device 'dev' by writing 'pattern' to the burstsisize register.  
  0x400 is the default; 0x1000 seems to be the largest acceptable value
for the operating system.

  Return value: 0 if no error; -1 if the device is not initialized



      ---------- gs_set_rx_timeout() ----------

int gs_set_rx_timeout(int dev, unsigned int pattern);

  Set the timeout for receive transactions (PMC Gigastar card to memory)
for device 'dev'.  Default is 0x64, about 25 ms.  Maximum is 0xffff,
about 16 s.

  Return value: 0 if no error; -1 if the device is not initialized



      ---------- gs_set_tx_burstsize() ----------

int gs_set_tx_burstsize(int dev, unsigned int pattern);

  Set the DMA burstsize for transmit tansactions (memory to PMC Gigastar card
transmitter) for  device 'dev' by writing 'pattern' to the burstsisize register.
  0x400 is the default.

  Return value: 0 if no error; -1 if the device is not initialized



      ---------- gs_write_bar0_reg() ----------

int gs_write_bar0_reg(int dev, int offset, unsigned int pattern)

  Write 'pattern' to the BAR0 register at 'offset' in device 'dev'.  Not
all operations are permitted by the driver.

  Return value: 0 if no error; -1 if the device is not initialized, or
on invalid operation.



      ---------- gs_rx_data() ----------

size_t gs_rx_data(int dev, void *buf, int count);	  

  Get unsigned 32-bit words from the PMC Gigastar receive data buffer 
(Rx FIFO) of device 'dev' into array buf treated as an array of dwords.  
'count' is the number of bytes to transfer (must be a multiple of 4).  
This function does not use DMA.  Be alert to the possibility of partial
reads (check the return value).
  This call does not block - you get whatever is there.

  Return value: number of bytes transferred; -1 if the device is not initialized, or
or on read error.


      ---------- gs_tx_data() ----------

size_t gs_tx_data(int dev, void *buf, int count);

  Put unsigned 32-bit words from buf to the PMC Gigastar transmit data buffer
(Tx FIFO) of device 'dev'.  'count' is the number of bytes to transmit.
This function does not use DMA.

  Return value: number of bytes transferred; -1 if the device is not initialized, or
or on write error.



       ---------- gs_rx_dma_setup() ----------

void *gs_rx_dma_setup(int dev, unsigned int mapsize);

  Set up for DMA receive transactions from the PMC Gigastar RX fifo of device
'dev' into memory.  If not already allocated, this maps 2 DMA buffers of size
'mapsize' (bytes) and sets a pointer to the first buffer.  On subsequent calls,
this function swaps buffer pointers.  Note that it is not necessary to use
double buffering if time to be saved by concurrency is not needed.

  Return value: pointer to the first buffer.



      ---------- gs_tx_dma_setup() ----------

void *gs_tx_dma_setup(int dev, unsigned int mapsize);

  Set up for DMA transmit transactions from memory to the PMC Gigastar
transmitter.  If not already allocated, this maps 2 DMA buffers of size
'mapsize' (bytes) and sets a pointer to the first buffer.  On subsequent calls,
this function swaps buffer pointers.  Note that it is not necessary to use
double buffering if time to be saved by concurrency is not needed.

  Return value:  pointer to the first buffer.



      ---------- gs_rx_dma() ----------

size_t gs_rx_dma(int dev);

  Receive 'mapsize' (declared in gs_rx_dma_setup()) bytes from the PMC Gigastar 
RX fifo into memory by DMA.

  Return value: number of bytes received; -1 if the device is not initialized,
or read error.



      ---------- gs_tx_dma() ----------

size_t gs_tx_dma(int dev);

  Transmit 'mapsize' (declared in gs_tx_dma_setup()) bytes from memory to the
PMC Gigastar TX fifo by DMA.

  Return value: number of bytes transmitted;  -1 if the device is not initialized,
or read error.



     ---------- gs_set_t0() ----------

int gs_set_t0(double dt);

  Set the elapsed time to zero.  This function is automatically called during
initialization.

  If dt is not 0, only record dt as an additive constant to be added to
gs_time() and gs_timestamp().  This mode is useful when the true origin time
only becomes available, e.g., at the first interrrupt.

  Return value: 0 on success, -1 on device not found or read error



      ---------- gs_time() ----------

double gs_time(void);

  Calculate elapsed time in seconds from the CPU TSC (time stamp counter).
This is a feature of the Linux driver, not of the PMC GigaSTaR card per se.
Note that under Linux, at least for the 2.4 kernel, if the load level is 
very high, the gettimeofday() function can lose more than 25% of its time.
This provides a hardware timer as a workaround.  Also note that gettimeofday()
is a slow function, requiring 30-40 usec.  gs_time() requires < 1 usec.

  For this function to work properly, CPU frequency scaling should be turned
off ("CONFIG_CPU_FREQ is not set" in kernel config file), even for desktop 
machines.

  Return value: elapsed time in seconds with sub-microsecond apparent precision



      ---------- gs_timestamp() ----------

char *gs_timestamp(void);

  Print the current time to a text buffer with millisecond apparent precision.
This uses the stored t0 obtained with gettimeofday() at initialization, or 
at the most recent gs_set_t0(), together with the measured time from the
CPU's timestamp counter (TSC).  If gettimeofday() returned an inaccurate value,
the absolute time printed by this routine will be wrong, but the relative
times of events should be accurate.

  Return value: pointer to a text buffer giving the current time to
millisecond apparent precision.




*/


#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/user.h>	/* pagesize */
#include <sys/mman.h>	/* mmap */

#include "gsdc.h"
#include "gslib.h"
#include "debug.h"


// internal functions
// static void gs_terminate(void);


// global variables
static char gs_path[150] = "/home/det/gstar/gsdev/";	// default path
static unsigned long long time0, systime0;
static double cpu_Hz = 3066272000.0;		// dual xeon, 3.0 GHz, reset by init
static unsigned int buf[32];
static char tbuf[30];
static int device_count=0;			// number of PSI PMC-GigaSTaR cards
static double time_offset=0.0;

static struct {int time_fd;			// file descriptors for the functions
			  int BAR0_fd;
			  int BAR1_fd;
			  int read0_fd;
			  int read1_fd;
			  int write0_fd;
			  int write1_fd;
			  int readsize;			// DMA buffer sizes
			  int writesize;
			  int read_idx;
			  int write_idx;
			  char *read0buf;		// DMA buffer pointers
			  char *read1buf;
			  char *write0buf;
			  char *write1buf;
			  } gs_info[GS_MAX_DEVICES];


/*
** Initialize (or re-initialize) all PSI PMC-GigaSTaR cards
** If path is NULL, use the previously set, or the built-in path.
** Reset the unit, close existing file descriptors, and set up
** new descriptors.  Finally, put the unit is a standby state.
*/
int gs_initialize(char *path)
{
char line[150], line2[150];
int dev;
struct stat stat_buf;
FILE *ifp;
double MHz;

for(dev=0; dev<GS_MAX_DEVICES; dev++)
	{
	if (gs_info[dev].BAR0_fd)	// previously set?
		{
		gs_reset(dev);
		close(gs_info[dev].BAR0_fd);
		if (gs_info[dev].BAR1_fd)
			close(gs_info[dev].BAR1_fd);
		if (gs_info[dev].read0_fd)
			close(gs_info[dev].read0_fd);
		if (gs_info[dev].read1_fd)
			close(gs_info[dev].read1_fd);
		if (gs_info[dev].write0_fd)
			close(gs_info[dev].write0_fd);
		if (gs_info[dev].write1_fd)
			close(gs_info[dev].write1_fd);
		if (dev==0 && gs_info[dev].time_fd)
			close(gs_info[dev].time_fd);
		}
	}

memset(gs_info, 0, sizeof(gs_info));
device_count=0;

if (!path)
	{			// no path given; use defaults
	if ( (getcwd(line2, sizeof(line))) == NULL)
		return -1;
	strcat(line2, "/gsdev");
	if (stat(line2, &stat_buf))
		strcpy(line2, gs_path);		// pick up the built-in path
	path=line2;
	}

if (stat(path, &stat_buf))
	{
	printf("Cannot stat path:\n\t%s\n", path);
	return -1;
	}
strcpy(line, path);
if ( *(line-1+strlen(line)) != '/')
	strcat(line, "/");
strcat(line, "gs0/BAR0");
if (stat(line, &stat_buf))
	{
	printf("Cannot stat PMC Gigastar device (gs0)\n");
	return -1;
	}
*strrchr(line, '/') = '\0';
*(1+strrchr(line, '/')) = '\0';
strcpy(gs_path, line);

for(dev=0; dev<GS_MAX_DEVICES; dev++)
	{
	sprintf(line, "%sgs%d/BAR0", gs_path, dev);
	if (stat(line, &stat_buf))
		{
		if (dev == 0)
			{
			printf("Cannot stat file: %s\n", line);
			return -1;
			}
		else
			break;		// fewer than max cards
		}
	if( (gs_info[dev].BAR0_fd = open(line, O_RDWR))<0 )
		{
		printf("Could not open %s\n", line);
		return -1;
		}
	fcntl(gs_info[dev].BAR0_fd, F_SETFD, FD_CLOEXEC);	// close on fork

	gs_reset(dev);

	sprintf(line, "%sgs%d/BAR1", gs_path, dev);
	if( (gs_info[dev].BAR1_fd = open(line, O_RDWR))<0 )
		{
		printf("Could not open %s\n", line);
		return -1;
		}
	fcntl(gs_info[dev].BAR1_fd, F_SETFD, FD_CLOEXEC);

	sprintf(line, "%sgs%d/read0", gs_path, dev);
	if( (gs_info[dev].read0_fd = open(line, O_RDONLY))<0 )
		{
		printf("Could not open %s\n", line);
		return -1;
		}
	fcntl(gs_info[dev].read0_fd, F_SETFD, FD_CLOEXEC);

	sprintf(line, "%sgs%d/read1", gs_path, dev);
	if( (gs_info[dev].read1_fd = open(line, O_RDONLY))<0 )
		{
		printf("Could not open %s\n", line);
		return -1;
		}
	fcntl(gs_info[dev].read1_fd, F_SETFD, FD_CLOEXEC);

	sprintf(line, "%sgs%d/write0", gs_path, dev);		// must be O_RDWR for mmap
	if( (gs_info[dev].write0_fd = open(line, O_RDWR))<0 )
		{
		printf("Could not open %s\n", line);
		return -1;
		}
	fcntl(gs_info[dev].write0_fd, F_SETFD, FD_CLOEXEC);

	sprintf(line, "%sgs%d/write1", gs_path, dev);		// must be O_RDWR for mmap
	if( (gs_info[dev].write1_fd = open(line, O_RDWR))<0 )
		{
		printf("Could not open %s\n", line);
		return -1;
		}
	fcntl(gs_info[dev].write1_fd, F_SETFD, FD_CLOEXEC);

	if (dev==0)
		{
		strcpy(line, gs_path);
		strcat(line, "time");
		if ( (gs_info[dev].time_fd=open(line, O_RDONLY)) == -1)
			{
			printf("Cannot open the PMC Gigastar time device\n");
			return -1;
			}
		fcntl(gs_info[dev].time_fd, F_SETFD, FD_CLOEXEC);

		// get cpu clock frequency from /proc
		if ( (ifp = fopen("/proc/cpuinfo", "r")) == NULL)
			{
			printf("Could not open /proc/cpuinfo\n");
			return -1;
			}
		while (fgets(line, sizeof(line), ifp))
			{
			if (!strncmp(line, "cpu MHz", strlen("cpu MHz")))
				goto jmp;
			}
		fclose(ifp);
		printf("Could not find 'cpu MHz' line\n");
		return 0;

		jmp:
		fclose(ifp);
		sscanf((1+strchr(line, ':')), "%lf", &MHz);
		cpu_Hz = 1.0e6*MHz;
		}
	else
		gs_info[dev].time_fd=gs_info[0].time_fd;

	// put the card into a standby state
	gs_start(dev);
	}

// this causes problems when a child process exits
// atexit(gs_terminate);

gs_set_t0(0.0);		// set elapsed time to 0 & iniitialize CMOS timer
printf("%d PSI PMC GigaSTaR card(s) initialized\n", dev);
DBG(3, "%d PSI PMC GigaSTaR card(s) initialized\n", dev)
device_count=dev;
return 0;
}



int gs_start(dev)
{
int i;

if (gs_info[dev].BAR0_fd==0)
	{
	printf("PMC-GigaSTaR card %d is not initialized\n", dev);
	return -1;
	}

gs_set_control(dev,
				GS_MODE_U<<GS_CTRL_REG_MODE_SHIFT |
				GS_ENB_DMA_TIMEOUT |
				GS_RUN_PMC
				);

// wait for the RX link - but not forever
for(i=0; i<999 && (gs_status(dev) & GS_RX_LINK_ERR); i++)
	;

return 0;
}



int gs_reset(int dev)
{
unsigned int mask;

if (gs_info[dev].BAR0_fd==0)
	{
	printf("PMC-GigaSTaR card %d is not initialized\n", dev);
	return -1;
	}

// must give reset directly to the control register
mask = 0xff;			// reset mask
gs_write_bar0_reg(dev, GS_CONTROL_REG, mask);
if (gs_status(dev) & GS_INIT_RXTX)
	{
	printf("PMC-GigaSTaR reset was not completed\n");
	return -1;
	}

return 0;
}



int gs_reset_all(int dev)
{

if (gs_reset(dev))
	return -1;

if (gs_info[dev].readsize)	// unmap buffers
	{
	munmap(gs_info[dev].read0buf, gs_info[dev].readsize);
	if (gs_info[dev].read1buf)
		munmap(gs_info[dev].read1buf, gs_info[dev].readsize);
	gs_info[dev].read0buf=NULL;
	gs_info[dev].read1buf=NULL;
	}
gs_info[dev].readsize = 0;

if (gs_info[dev].writesize)	// unmap buffers
	{
	munmap(gs_info[dev].write0buf, gs_info[dev].writesize);
	if (gs_info[dev].write1buf)
		munmap(gs_info[dev].write1buf, gs_info[dev].writesize);
	gs_info[dev].write0buf=NULL;
	gs_info[dev].write1buf=NULL;
	}
gs_info[dev].writesize = 0;

gs_info[dev].read_idx=0;
gs_info[dev].write_idx=0;

return 0;
}



int gs_status(int dev)
{
unsigned int *uip=gs_read_bar0_reg(dev, 4*GS_STATUS_REG, 4);

if (uip)
	return uip[0];
else
	printf("Error reading gs status register\n");
return 0;
}



// this shows the cause of the last interrupt
int gs_irq_status(int dev)
{
unsigned int *uip=gs_read_bar0_reg(dev, 4*GS_INTR_REG, 4);

if (uip)
	return uip[0];
else
	printf("Error reading gs interrupt register\n");
return 0;
}



int gs_interrupt_status(int dev)
{
int stat;

if (gs_info[dev].BAR0_fd==0)
	{
	printf("PMC-GigaSTaR card %d is not initialized\n", dev);
	return 0;
	}

if (ioctl(gs_info[dev].BAR0_fd, GSD_INT_STATUS, &stat))
	{
	printf("  ioctl error: %s\n", strerror(errno));
	return 0;
	}

return stat;
}




// this shows potentially interruptible conditions since last clear
int gs_irq_src_status(int dev)
{
unsigned int *uip=gs_read_bar0_reg(dev, 4*GS_IRQ_SRC_REG, 4);

if (uip)
	return uip[0];
else
	printf("Error reading gs irq src register\n");
return 0;
}



// offset and count are in bytes
unsigned int *gs_read_bar0_reg(int dev, int offset, int count)
{
int num;

if (gs_info[dev].BAR0_fd==0)
	{
	printf("PMC-GigaSTaR card %d is not initialized\n", dev);
	return NULL;
	}

count = count+offset>4*32 ? 4*32-offset : count;
if (count<=0)
	return NULL;
lseek(gs_info[dev].BAR0_fd, offset, SEEK_SET);
if ( (num = read(gs_info[dev].BAR0_fd, buf, count))<0 )
	{
	printf("  read error: %s\n", strerror(errno));
	return NULL;
	}

return buf;
}


// return the recorded time of the last interrupt
double gs_last_interrupt_time(int dev)
{
char bufr[8];
unsigned long long t;

if (gs_info[dev].BAR0_fd==0)
	{
	printf("PMC-GigaSTaR card %d is not initialized\n", dev);
	return 0.0;
	}

if (ioctl(gs_info[dev].BAR0_fd, GSD_INT_TIME, bufr))
	{
	printf("  ioctl error: %s\n", strerror(errno));
	return 0.0;
	}
t = *((unsigned long long *)bufr);

if (t > time0)
	return time_offset+(double)(t-time0)/cpu_Hz;	// elapsed time in seconds

return 0.0;				// no interrupt yet this period
}




int gs_device_count(void)
{
return device_count;
}



int gs_set_control(int dev, unsigned int pattern)
{
return gs_write_bar0_reg(dev, 4*GS_SET_CNTRL_REG, pattern);
}



int gs_reset_control(int dev, unsigned int pattern)
{
return gs_write_bar0_reg(dev, 4*GS_RESET_CNTRL_REG, pattern);
}



int gs_set_irq_mask(int dev, unsigned int pattern)
{
return gs_write_bar0_reg(dev, 4*GS_IRQ_MASK_REG, pattern);
}



int gs_clear_irq(int dev)
{
return gs_write_bar0_reg(dev, 4*GS_IRQ_SRC_REG, 0xffffffff);;
}



// yes, it is correct that the RX function uses DMA write
int gs_set_rx_burstsize(int dev, unsigned int pattern)
{
return gs_write_bar0_reg(dev, 4*GS_DMA_WR_BURSTSIZE, pattern);
}



int gs_set_rx_timeout(int dev, unsigned int pattern)
{
return gs_write_bar0_reg(dev, 4*GS_DMA_WR_TIMEOUT, pattern);
}



// yes, it is correct that the TX function uses DMA read
int gs_set_tx_burstsize(int dev, unsigned int pattern)
{
return gs_write_bar0_reg(dev, 4*GS_DMA_RD_BURSTSIZE, pattern);
}



// offset is in bytes.
int gs_write_bar0_reg(int dev, int offset, unsigned int pattern)
{
int num;

if (gs_info[dev].BAR0_fd==0)
	{
	printf("PMC-GigaSTaR card %d is not initialized\n", dev);
	return -1;
	}

lseek(gs_info[dev].BAR0_fd, offset, SEEK_SET);
if ( (num = write(gs_info[dev].BAR0_fd, &pattern, 4))<0 )
	{
	// setting the interrupt when it is already set gives this error
	printf("  write bar0 error: %s\n", strerror(errno));
	return -1;
	}

return 0;
}



size_t gs_rx_data(int dev, void *buf, size_t count)
{
int k;
unsigned int status;
ssize_t num=0;

if (gs_info[dev].BAR0_fd==0)
	{
	printf("PMC-GigaSTaR card %d is not initialized\n", dev);
	return -1;
	}

if ( (num = read(gs_info[dev].BAR1_fd, buf, count))<0 )
	{
	printf("  rx_data read error: %s\n", strerror(errno));
	return (int)num;
	}
	
// driver returns up to PAGE_SIZE bytes per read
while(num < count)
	{
	if ( (k = read(gs_info[dev].BAR1_fd, buf+num, count-num))<0 )
		{
		printf("  rx_data read error: %s\n", strerror(errno));
		break;
		}
	if (k==0)		// check for fifo empty
		{
		status = gs_status(dev);
		if (status & GS_RX_FIFO_EMPTY)
			break;
		}
	num+=k;
	}

return num;
}



size_t gs_tx_data(int dev, void *buf, size_t count)
{
ssize_t num;

if (gs_info[dev].BAR0_fd==0)
	{
	printf("PMC-GigaSTaR card %d is not initialized\n", dev);
	return -1;
	}

if ( (num = write(gs_info[dev].BAR1_fd, buf, count))<0 )
	printf("  tx_data write error: %s\n", strerror(errno));

return num;
}



/*
** For double buffering, call this routine once, then use gs_rx_dma()
** For single buffering, call this function for each loop
*/
void *gs_rx_dma_setup(int dev, unsigned int mapsize)
{
char *q;

if (gs_info[dev].BAR0_fd==0)
	{
	printf("PMC-GigaSTaR card %d is not initialized\n", dev);
	return NULL;
	}

// if a size change, unmap old buffers
if (gs_info[dev].readsize && mapsize!=gs_info[dev].readsize)
	{
	munmap(gs_info[dev].read0buf, gs_info[dev].readsize);
	if (gs_info[dev].read1buf)
		munmap(gs_info[dev].read1buf, gs_info[dev].readsize);
	gs_info[dev].read0buf=NULL;
	gs_info[dev].read1buf=NULL;
	gs_info[dev].readsize = 0;
	gs_info[dev].read_idx = 0;
	}

// if readsize is 0, allocate 1st dma buffer & return it
if (!gs_info[dev].readsize)
	{
	gs_info[dev].read0buf = mmap(NULL, mapsize, PROT_READ, MAP_SHARED, gs_info[dev].read0_fd, 0);
	if (gs_info[dev].read0buf==MAP_FAILED)
		{
		printf("mmap for read failed\n");
		return NULL;
		}
	// mmap allocates integral number of pages
	// the desired transfer may be smaller
	if (gs_write_bar0_reg(dev, 4*GS_DMA_WR_BUF_SIZE, mapsize/4))
		printf("*** error setting DMA buffer size (1)\n");
	gs_info[dev].readsize = mapsize;
	return gs_info[dev].read0buf;
	}

// if read1buf is NULL, allocate 2nd dma buffer & return it
if (!gs_info[dev].read1buf)
	{
	gs_info[dev].read1buf = mmap(NULL, mapsize, PROT_READ, MAP_SHARED, gs_info[dev].read1_fd, 0);
	if (gs_info[dev].read1buf==MAP_FAILED)
		{
		printf("mmap for read failed\n");
		return NULL;
		}
	if (gs_write_bar0_reg(dev, 4*GS_DMA_WR_BUF_SIZE, mapsize/4))
		printf("*** error setting DMA buffer size (2)\n");
	gs_info[dev].read_idx = 1;
	return gs_info[dev].read1buf;
	}

// flip the buffers
gs_info[dev].read_idx = !gs_info[dev].read_idx;
if (gs_info[dev].read_idx)
	q = gs_info[dev].read1buf;
else
	q = gs_info[dev].read0buf;

return q;
}



/*
** For double buffering, call this routine once, then use gs_tx_dma()
** For single buffering, call this function for each loop
*/
void *gs_tx_dma_setup(int dev, unsigned int mapsize)
{
char *q;

if (gs_info[dev].BAR0_fd==0)
	{
	printf("PMC-GigaSTaR card %d is not initialized\n", dev);
	return NULL;
	}

// if a size change, unmap old buffers
if (gs_info[dev].writesize && mapsize!=gs_info[dev].writesize)
	{
	munmap(gs_info[dev].write0buf, gs_info[dev].writesize);
	if (gs_info[dev].write1buf)
		munmap(gs_info[dev].write1buf, gs_info[dev].writesize);
	gs_info[dev].write0buf=NULL;
	gs_info[dev].write1buf=NULL;
	gs_info[dev].writesize = 0;
	gs_info[dev].write_idx = 0;
	}

// if writesize is 0, allocate 1st dma buffer & return it
if (!gs_info[dev].writesize)
	{
	gs_info[dev].write0buf = mmap(NULL, mapsize, PROT_WRITE, MAP_SHARED, gs_info[dev].write0_fd, 0);
	if (gs_info[dev].write0buf==MAP_FAILED)
		{
		printf("mmap for write failed\n");
		return NULL;
		}
	// mmap allocates integral number of pages
	// the desired transfer may be smaller
	if (gs_write_bar0_reg(dev, 4*GS_DMA_RD_BUF_SIZE, mapsize/4))
		printf("*** error setting DMA buffer size (1)\n");
	gs_info[dev].writesize = mapsize;
	return gs_info[dev].write0buf;
	}

// if write1buf is NULL, allocate 2nd dma buffer & return it
if (!gs_info[dev].write1buf)
	{
	gs_info[dev].write1buf = mmap(NULL, mapsize, PROT_WRITE, MAP_SHARED, gs_info[dev].write1_fd, 0);
	if (gs_info[dev].write1buf==MAP_FAILED)
		{
		printf("mmap for write failed\n");
		return NULL;
		}
	if (gs_write_bar0_reg(dev, 4*GS_DMA_RD_BUF_SIZE, mapsize/4))
		printf("*** error setting DMA buffer size (2)\n");
	gs_info[dev].write_idx = 1;
	return gs_info[dev].write1buf;
	}

// flip the buffers
gs_info[dev].write_idx = !gs_info[dev].write_idx;
if (gs_info[dev].write_idx)
	q = gs_info[dev].write1buf;
else
	q = gs_info[dev].write0buf;

return q;
}



size_t gs_rx_dma(int dev)
{
ssize_t num=0;

if (gs_info[dev].BAR0_fd==0)
	{
	printf("PMC-GigaSTaR card %d is not initialized\n", dev);
	return -1;
	}

// note that buf & count are dummy arguments for the gs driver

if (gs_info[dev].read_idx)
	num = read(gs_info[dev].read1_fd, buf, sizeof(buf));
else
	num = read(gs_info[dev].read0_fd, buf, sizeof(buf));

return num;
}



size_t gs_tx_dma(int dev)
{
ssize_t num;

if (gs_info[dev].BAR0_fd==0)
	{
	printf("PMC-GigaSTaR card %d is not initialized\n", dev);
	return -1;
	}

// note that buf & count are dummy arguments for the gs driver

if (gs_info[dev].write_idx)
	num = write(gs_info[dev].write1_fd, buf, sizeof(buf));
else
	num = write(gs_info[dev].write0_fd, buf, sizeof(buf));

if (num < 0)
	printf("  tx_dma write error: %s\n", strerror(errno));

return num;
}



int gs_set_t0(double dt)
{
char bufr[16];
struct timeval tv;
struct timezone tz;

if (gs_info[0].time_fd==0)
	{
	printf("PMC-GigaSTaR card 0 is not initialized\n");
	return -1;
	}

if (dt != 0.0)
	{
	time_offset = dt;
	return 0;
	}

time_offset = 0.0;
lseek(gs_info[0].time_fd, 0, SEEK_SET);
if ( (read(gs_info[0].time_fd, bufr, 8))<0 )
	{
	printf("  time device read error A: %s\n", strerror(errno));
	return -1;
	}
time0 = *((unsigned long long *)bufr);
gettimeofday(&tv, &tz);
// # of seconds since 00:00:00, January 1, 1970
systime0 = 1000*(unsigned long long)tv.tv_sec+(tv.tv_usec/1000);
if (tv.tv_usec%1000 > 500)
	systime0++;

return 0;
}



double gs_time(void)
{
char bufr[8];

if (gs_info[0].time_fd==0)
	{
	printf("PMC-GigaSTaR card 0 is not initialized\n");
	return 0.0;
	}

lseek(gs_info[0].time_fd, 0, SEEK_SET);
if ( (read(gs_info[0].time_fd, bufr, 8))<0 )
	{
	printf("  time device read error B: %s\n", strerror(errno));
	return 0.0;
	}

// elapsed time in seconds
return time_offset+(double)(*((unsigned long long *)bufr)-time0)/cpu_Hz;
}



char *gs_timestamp(void)
{
unsigned long long s;
double x = gs_time();
time_t t;

s = systime0 + (unsigned long long)(1000.0*x + 0.0005);
t = (time_t)(s/1000);
strftime(tbuf, sizeof(tbuf), "%Y-%b-%dT%H:%M:%S", localtime(&t));
sprintf(tbuf+strlen(tbuf), ".%.3d", (int)(s%1000));

return tbuf;
}



#if 0		// not used - see note above
// internal functions
static void gs_terminate(void)
{
int dev;

for(dev=0; dev<device_count; dev++)
	(void)gs_reset(dev);
if (gs_info[dev].readsize)	// unmap buffers
	{
	munmap(gs_info[dev].read0buf, gs_info[dev].readsize);
	if (gs_info[dev].read1buf)
		munmap(gs_info[dev].read1buf, gs_info[dev].readsize);
	gs_info[dev].read0buf=NULL;
	gs_info[dev].read1buf=NULL;
	}

if (gs_info[dev].writesize)	// unmap buffers
	{
	munmap(gs_info[dev].write0buf, gs_info[dev].writesize);
	if (gs_info[dev].write1buf)
		munmap(gs_info[dev].write1buf, gs_info[dev].writesize);
	gs_info[dev].write0buf=NULL;
	gs_info[dev].write1buf=NULL;
	}

return;
}
#endif
