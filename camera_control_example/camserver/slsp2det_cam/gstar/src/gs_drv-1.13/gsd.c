/* gsd.c - a kernel module driver for the PSI GigaSTaR PCI card
** for linux kernel 2.6.10 ff.
**
** Adapted from "Linux Device Drivers"
** Alessandro Rubini and Jonathan Corbet,
** O'Reilly & Associates, 2001.
**
** (the 3rd edition of this valuable book was released in Feb 2005.)
**
** Also adapted from "Porting device drivers to the 2.6 kernel"
** Jonathan Corbet
** Linux World News, http://lwn.net/Articles/driver-porting/, 2003.
**
** See also:
**   linux/Documentation/DMA-API.txt
**   linux/Documentation/DMA-mapping.txt
**   linux/Documentation/pci.txt
**   linux/include/linux/pci.h
**
** GPL license declared below.
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


/*  Here is the device, as shown by `cat /proc/pci` on pc4868:
Bus  5, device   0, function  0:
 Class ff00: PCI device 10ee:0300 (Xilinx Corporation) (rev 0).
  IRQ 11.
  Master Capable.  Latency=165.  Min Gnt=20.Max Lat=15.
  Non-prefetchable 32 bit memory at 0xc230c000 [0xc230cfff].
  Non-prefetchable 32 bit memory at 0xc2300000 [0xc2307fff].

With 2 cards:
Bus  5, device   0, function  0:
 Class ff00: PCI device 10ee:0300 (Xilinx Corporation) (rev 0).
   IRQ 11.
   Master Capable.  Latency=165.  Min Gnt=20.Max Lat=15.
   Non-prefetchable 32 bit memory at 0xc2314000 [0xc2314fff].
   Non-prefetchable 32 bit memory at 0xc2300000 [0xc2307fff].
 Bus  5, device   1, function  0:
 Class ff00: PCI device 10ee:0300 (Xilinx Corporation) (rev 0).
   IRQ 11.
   Master Capable.  Latency=165.  Min Gnt=20.Max Lat=15.
   Non-prefetchable 32 bit memory at 0xc2315000 [0xc2315fff].
   Non-prefetchable 32 bit memory at 0xc2308000 [0xc230ffff].
*/


#include <linux/init.h>
#include <linux/module.h>

MODULE_LICENSE("Dual BSD/GPL");
MODULE_AUTHOR("E.F. Eikenberry");
MODULE_DESCRIPTION("driver for the PSI PMC-GigaSTaR card");
MODULE_SUPPORTED_DEVICE("PMC-GigaSTaR");

#include <linux/kernel.h>
#include <linux/pci.h>
#include <linux/spinlock.h>
#include <linux/pagemap.h>
#include <linux/time.h>
#include <linux/mm.h>
#include <linux/slab.h>
#include <linux/wait.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <asm/msr.h>
#include <asm/atomic.h>
#include <asm/uaccess.h>
#include <asm/ioctls.h>
#include <asm/io.h>

#include "gsdc.h"		// configuration constants
#include "gsd.h"

/*****************************************************************************\
**                                                                           **
**               Declarations                                                **
**                                                                           **
\*****************************************************************************/

/* prototypes of functions in the order they are presented */
static int gsfs_open(struct inode *inode, struct file *filp);
static ssize_t gsfs_read(struct file *, char *, size_t, loff_t *);
static ssize_t gsfs_write(struct file *, const char *, size_t, loff_t *);
static int gsfs_ioctl(struct inode *, struct file *, unsigned int, unsigned long);
static int gsfs_mmap(struct file *, struct vm_area_struct *);
void gsfs_vm_open(struct vm_area_struct *);
void gsfs_vm_close(struct vm_area_struct *);
#if (LINUX_VERSION_CODE) >= KERNEL_VERSION(2,6,24)
static int gsfs_vm_fault(struct vm_area_struct *, struct vm_fault *);
#else
static struct page *gsfs_vm_nopage(struct vm_area_struct *, unsigned long, int *);
#endif
static void gsfs_create_files (struct super_block *, struct dentry *);
static struct dentry *gsfs_create_file (struct super_block *,
		struct dentry *, const char *, void *);
static struct dentry *gsfs_create_dir (struct super_block *,
		struct dentry *, const char *);
static struct inode *gsfs_make_inode(struct super_block *, int);
static int allocate_dma_buffer(struct Gsfs_id *);
static int release_dma_buffer(struct Gsfs_id *);
static int gigastar_probe(struct pci_dev *, const struct pci_device_id *);
static void gigastar_remove(struct pci_dev *);
static int __init gigastar_init(void);
static void __exit gigastar_exit(void);

#if (LINUX_VERSION_CODE) <= KERNEL_VERSION(2,6,17)
static struct super_block *gsfs_get_super(struct file_system_type *,
		int, const char *, void *);
#else
int gsfs_get_super(struct file_system_type *,int, const char *,
		void *, struct vfsmount *);
#endif

#if (LINUX_VERSION_CODE) <= KERNEL_VERSION(2,6,18)
static irqreturn_t gs_irq_handler(int, void *, struct pt_regs *);
#else
static irqreturn_t gs_irq_handler(int, void *);
#endif

#if (LINUX_VERSION_CODE) >= KERNEL_VERSION(2,6,22)
#define rdtsc(low,high) \
     __asm__ __volatile__("rdtsc" : "=a" (low), "=d" (high))
#endif

// declare the module load and unload functions
module_init(gigastar_init);
module_exit(gigastar_exit);

// Global variables
static Pci_Region pci_regions[GS_MAX_DEVICES][PCI_BAR_MAX];
static unsigned int gs_irq[GS_MAX_DEVICES];
static unsigned int last_int_tsc_low, last_int_tsc_high;
static struct Gsfs_id gsfs_id[N_GS_IDS];
static struct Gsfs_id *read0_id[GS_MAX_DEVICES];	/* pointers to the individual function id's */
static struct Gsfs_id *read1_id[GS_MAX_DEVICES];
static struct Gsfs_id *write0_id[GS_MAX_DEVICES];
static struct Gsfs_id *write1_id[GS_MAX_DEVICES];
static char *month[]= {"x", "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul",
				"Aug", "Sep", "Oct", "Nov", "Dec"};
static char *day[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat", "Sun"};
DECLARE_WAIT_QUEUE_HEAD(gs_queue);


// Define the treatment of the PCI regions discovered at initialization.
// Do NOT perform a memory test on the configuration region, if present.
static u32 pci_addresses_proto[][2] =
	{
	{PCI_BASE_ADDRESS_0, DO_NOT_TEST},	// GigaSTaR BAR0 is configuration space
	{PCI_BASE_ADDRESS_1, DO_NOT_TEST},	// GigaSTaR BAR1 is read only
	{PCI_BASE_ADDRESS_2, DO_NOT_TEST},	// not implemented in PMC-GigaSTaR
	{PCI_BASE_ADDRESS_3, DO_NOT_TEST},	// not implemented in PMC-GigaSTaR
	{PCI_BASE_ADDRESS_4, DO_NOT_TEST},	// not implemented in PMC-GigaSTaR
	{PCI_BASE_ADDRESS_5, DO_NOT_TEST}	// not implemented in PMC-GigaSTaR
	} ;
static u32 pci_addresses[GS_MAX_DEVICES][PCI_BAR_MAX][2];

static struct pci_device_id gs_id_table[] = {
	{PCI_DEVICE(GS_VENDOR_ID, GS_DEVICE_ID)},
	{0},
	};

// export to system
MODULE_DEVICE_TABLE(pci, gs_id_table);

// properties of the driver
static struct pci_driver gs_driver = {
	.name		= "*PMC-GigaSTaR*",
	.id_table	= gs_id_table,
	.probe		= gigastar_probe,
	.remove		= gigastar_remove,
	};

// properties of the devices (cards) handled by the driver
static struct Gs_dev {
	int devno;
	int busy;
	wait_queue_head_t queue;
	struct pci_dev *device;
	int tx_fifo_size;			/* in dwords */
	int rx_fifo_size;			/* in dwords */
	} gs_devices[GS_MAX_DEVICES];

// operations defined for the pseudo-filesystem
static struct file_operations gsfs_file_ops = {
	.owner	= THIS_MODULE,
	.open	= gsfs_open,
	.read	= gsfs_read,
	.write	= gsfs_write,
	.ioctl	= gsfs_ioctl,
	.mmap	= gsfs_mmap,
	};

// vm operations for the nopage remapping method
static struct vm_operations_struct gsfs_vm_ops = {
    .open   = gsfs_vm_open,
    .close  = gsfs_vm_close,
#if (LINUX_VERSION_CODE) >= KERNEL_VERSION(2,6,24)
    .fault = gsfs_vm_fault,
#else
    .nopage = gsfs_vm_nopage,
#endif
	};

// superblock operations, both of which are generic kernel ops.
static struct super_operations gsfs_s_ops = {
	.statfs		= simple_statfs,
	.drop_inode	= generic_delete_inode,
	};

// properties of the pseudo-filesystem
static struct file_system_type gsfs_type = {
	.owner		= THIS_MODULE,
	.name		= "gsfs",
	.get_sb		= gsfs_get_super,
	.kill_sb	= kill_litter_super,
	};




/*****************************************************************************\
**                                                                           **
**         Define the operations permitted on the files                      **
**                                                                           **
\*****************************************************************************/

/*
** In the oficial kernel releases from kernel.org, "struct inode" changed
** from having the 'u' member to having the 'i_private' member on moving
** between linux-2.6.18.8 (20 Sep 06) and linux-2.6.19.1 (11 Dec 06).
** But, Scientif Linux 5.2 lists a 2.6.18 kernel, which nonetheless has 
** the new structure.  RedHat is a known early adopter of such things.
*/

/*
** Open a file
*/
static int gsfs_open(struct inode *inode, struct file *filp)
{
#if (LINUX_VERSION_CODE) <= KERNEL_VERSION(2,6,17)
  filp->private_data = inode->u.generic_ip;	// make the file id easier to get 
#else
  filp->private_data = inode->i_private;	// make the file id easier to get 
#endif
return 0;
}


#define TMPSIZE sizeof(regs)
/*
** Read from a file managed by this device
*/
static ssize_t gsfs_read(struct file *filp, char *buf,
		size_t count, loff_t *offset)
{
struct Gsfs_id *id=(struct Gsfs_id *)filp->private_data;
int idx = id->Idx, i, j, err, len;
unsigned int tsc_low, tsc_high, regs[32], *ui, stat;
char data[16], *c=(char *)regs;

//PDEBUG("Read method.  count = %u, offset = %u, device = %d\n", count, (int)*offset, idx);

switch (id->kind)
	{
	case Time:			// return CPU ticks as unsigned long long, or optionally CMOS time
		if (count < 8)
			return -EINVAL;				// we don't put up with such nonsense
		rdtsc(tsc_low, tsc_high);		// read the time-stamp counter
		*(unsigned int *)data = tsc_low;
		*((unsigned int *)data+1) = tsc_high;
		if (count==8)					// time-stamp counter was requested
			{
			*offset += count;
			if (copy_to_user(buf, data, count))
				return -EFAULT;
			else
				break;
			}
		// We come here if the user has typed 'cat time' on this device
		// So, we give him the CMOS time as text.  The numbers are in BCD.
		// This is slow - requires ~36 usec on 3.1 GHz dual Xeon
		while(0x80 & CMOS_READ(0xa))		// wait for any update in progress
			;
		data[8]=CMOS_READ(0x0);				// sec
		data[9]=CMOS_READ(0x2);				// min
		data[10]=CMOS_READ(0x4);			// hr
		data[11]=CMOS_READ(0x7);			// date
		data[12]=CMOS_READ(0x8);			// month
		data[13]=CMOS_READ(0x6);			// day
		data[14]=CMOS_READ(0x9);			// yr (2 digits)
		i=2000+((data[14]&0xf)+10*(data[14]>>4));	// convert BCD to decimal
		j=(data[12]&0xf)+10*(data[12]>>4);
		if (*offset > 16)	// 'cat' accesses device until return count is 0
			return 0;
		len=snprintf(c, TMPSIZE, "CMOS: %s %4d/%s/%.2x %.2x:%.2x:%.2x\n", 
				day[(int)data[13]],	i, month[j], data[11], 
				data[10], data[9], data[8]);
		if (*offset > len)
			return 0;
		if (count > len - *offset)
			count = len - *offset;
		if (copy_to_user(buf, c + *offset, count))
			return -EFAULT;
		*offset += count;
		break;

	case Bar0:
		if (count%4 || *offset%4)
			return -EINVAL;		// we don't put up with such nonsense
		if ( (count+*offset)/4 > 32)
			return -EINVAL;		// beyond end of registers
		for (i=0; i<count/4; i++)
			regs[i]=readl(pci_regions[idx][0].base+*offset/4+i);
		if (copy_to_user(buf, regs, count))
			return -EFAULT;
		*offset += count;
		break;

	case Bar1:
		if (count%4 || *offset%4)
			return -EINVAL;		// we don't put up with such nonsense
		if (count/4 > gs_devices[idx].rx_fifo_size+RX_FIFO_EXTRAS)
			return -EINVAL;
		// only *offset==0 makes sense, so we ignore it
		// but we permit multi-dword reads
		count = (count > PAGE_SIZE) ? PAGE_SIZE : count;
		if ((ui = kmalloc(count, GFP_KERNEL)) == NULL)
			return -EFAULT;
		stat = readl(pci_regions[idx][0].base+GS_STATUS_REG);
		for (i=0; i<count/4 && !(stat & GS_RX_FIFO_EMPTY); i++)
			{
			ui[i]=readl(pci_regions[idx][1].base);
			stat = readl(pci_regions[idx][0].base+GS_STATUS_REG);
			}
		count = i<count/4 ? 4*i : count;	// adjust for possible short read
		err = copy_to_user(buf, ui, count);
		kfree(ui);
		if (err)
			return -EFAULT;
		*offset += count;
		break;

	/*
	** Interrupt controlled DMA read from the PMC-GigaSTaR device RX port
	** This is writing to the host and is therefore called a "DMA write".
	** At the end of operation, exchange both buffers.
	** When a child process created by fork() exits, the wait_event_interruptible
	** is activated by a signal, hence the while() loop.
	** Offset is ignored.
	*/
	case Read0:
		while( wait_event_interruptible(gs_queue, gs_devices[idx].busy==0) )
			;
		count = 4*readl(pci_regions[idx][0].base+GS_DMA_WR_TRANSFER);
		writel(read1_id[idx]->bus_addr, (pci_regions[idx][0].base + GS_DMA_WR_TAP));
		writel(write1_id[idx]->bus_addr, (pci_regions[idx][0].base + GS_DMA_RD_TAP));
		if (count==0)
			return -EFAULT;
		break;
	case Read1:
		while( wait_event_interruptible(gs_queue, gs_devices[idx].busy==0) )
			;
		count = 4*readl(pci_regions[idx][0].base+GS_DMA_WR_TRANSFER);
		writel(read0_id[idx]->bus_addr, (pci_regions[idx][0].base + GS_DMA_WR_TAP));
		writel(write0_id[idx]->bus_addr, (pci_regions[idx][0].base + GS_DMA_RD_TAP));
		if (count==0)
			return -EFAULT;
		break;

	// reading from the "write" managers is not defined
	case Write0:
	case Write1:
		return -EFAULT;

	default:
		break;
	}

return count;
}


/*
** Write to a file managed by this device
*/
static ssize_t gsfs_write(struct file *filp, const char *buf,
		size_t count, loff_t *offset)
{
struct Gsfs_id *id=(struct Gsfs_id *)filp->private_data;
int i, k, nb, idx = id->Idx, flag;
unsigned int u, v, *ui;
spinlock_t lock;

//PDEBUG("Write method.  count = %u, offset = %u, device = %d\n", (int)count, (int)*offset, idx);

spin_lock_init(&lock);

switch (id->kind)
	{
	case Time:
		return -EINVAL;
		break;

	case Bar0:
		if (copy_from_user(&v, (unsigned int *)buf, count))
			return -EFAULT;
//PDEBUG("write method reached for bar0 - v = 0x%x\n", v);
		// permit only selected write operations to BAR0
		switch(*offset/4)
			{
			case GS_CONTROL_REG:
				if (v>0 && v<=0xff)	// only various resets are permitted
					{
					writel(v, pci_regions[idx][0].base);	// bit 18=0 turns off En_INTR
					for(i=0; i<100000; i++)					// ~3300 loops required
						if ( !(readb(pci_regions[idx][0].base) & 0x1) )
							break;
					if (i>90000)
						printk(KERN_ALERT "*** Time-out during GigaSTaR card %d reset\n", idx);
					if (gs_devices[idx].busy)	// wake up waiting process
						{
						spin_lock(&lock);
						wake_up_interruptible(&gs_queue);
						gs_devices[idx].busy=0;
						spin_unlock(&lock);
						}
					/* restore the dma buffer addresses, if they are defined */
					if (read0_id[idx])
						writel(read0_id[idx]->bus_addr, (pci_regions[idx][0].base + GS_DMA_WR_TAP));
					if (write0_id[idx])
						writel(write0_id[idx]->bus_addr, (pci_regions[idx][0].base + GS_DMA_RD_TAP));
					}
				else
					return -EINVAL;
				break;
			case GS_SET_CNTRL_REG:		// set individual bits in the control register
				if (v & GS_INT_ENABLE)	// enable interrupt
					{
					flag = 0;
					spin_lock(&lock);
					if (gs_devices[idx].busy)		// if interrupt already enabled, error
						flag = -EINVAL;
					else
						gs_devices[idx].busy = 1;
					spin_unlock(&lock);
					if (flag<0)
						return -EINVAL;
					}
				writel(v, pci_regions[idx][0].base + *offset/4);
				break;
			case GS_IRQ_SRC_REG:			// reset irq sources that have been dismissed 
				if (gs_devices[idx].busy)	// wake up waiting process
					{
					spin_lock(&lock);
					wake_up_interruptible(&gs_queue);
					gs_devices[idx].busy=0;
					spin_unlock(&lock);
					}
			case GS_RESET_CNTRL_REG:	// reset individual bits in the control register
			case GS_IRQ_MASK_REG:		// select conditons that may generate irq
			case GS_TX_FIFO_SIDEBITS:	// TX fifo sidebits
			case GS_TX_MBOX_REG:		// TX mailbox outgoing messages
			case GS_RX_FIFO_PROGFLAG:	// programmable threshold for the RX fifo
			case GS_DMA_WR_BURSTSIZE:	// write burst size, dwords; default = 0x400
			case GS_DMA_WR_TIMEOUT:		// write timeout, n * 25 us; default n = 100
			case GS_DMA_RD_BURSTSIZE:	// read burst size, dwords; default = 0x400
				writel(v, pci_regions[idx][0].base + *offset/4);
				break;
			// mmap sets buffer sizes to an integral number of pages
			// permit only reduction of those values
			case GS_DMA_WR_BUF_SIZE:
			case GS_DMA_RD_BUF_SIZE:
				u = readl(pci_regions[idx][0].base+*offset/4);
				if (v > u)
					return -EINVAL;
				writel(v, pci_regions[idx][0].base + *offset/4);
				break;
			default:
					return -EINVAL;
				break;
			}
		break;

	case Bar1:
		if (count%4 || *offset%4)
			return -EINVAL;		// we don't put up with such nonsense
		// only *offset==0 makes sense, so we ignore it
		// permit multi-dword writes
		nb = (count > PAGE_SIZE) ?  PAGE_SIZE : count;
		if ((ui = kmalloc(nb, GFP_KERNEL)) == NULL)
			return -EFAULT;
		if (copy_from_user(ui, (unsigned int *)buf, nb))
			{
			kfree(ui);
			return -EFAULT;
			}
		for (i=0; i<nb/4; i++)
			writel(ui[i], pci_regions[idx][1].base);
//PDEBUG("Bar1 writing 0x%x ...(%d values)\n", ui[0], nb/4);
		k=nb;
		while(k<count)
			{
			nb = (count-k > PAGE_SIZE) ? PAGE_SIZE : count-k;
			if (copy_from_user(ui, (unsigned int *)(buf+k), nb))
				{
				kfree(ui);
				return -EFAULT;
				}
			for (i=0; i<nb/4; i++)
				writel(ui[i], pci_regions[idx][1].base);
			k+=nb;
			}
		kfree(ui);
		break;


	// writing to the "read" managers is not defined
	case Read0:
	case Read1:
		return -EFAULT;

	/*
	** Interrupt controlled DMA write to the PMC-GigaSTaR device TX port.
	** This is reading from the host and therefore is called a "DMA read"
	** At the end of operation, exchange both buffers.
	** When a child process created by fork() exits, the wait_event_interruptible
	** is activated by a signal, hence the while() loop.
	*/
	case Write0:
		while( wait_event_interruptible(gs_queue, gs_devices[idx].busy==0) )
			;
		count = 4*readl(pci_regions[idx][0].base+GS_DMA_RD_TRANSFER);
		writel(read1_id[idx]->bus_addr, (pci_regions[idx][0].base + GS_DMA_WR_TAP));
		writel(write1_id[idx]->bus_addr, (pci_regions[idx][0].base + GS_DMA_RD_TAP));
		break;
	case Write1:
		while( wait_event_interruptible(gs_queue, gs_devices[idx].busy==0) )
			;
		count = 4*readl(pci_regions[idx][0].base+GS_DMA_RD_TRANSFER);
		writel(read0_id[idx]->bus_addr, (pci_regions[idx][0].base + GS_DMA_WR_TAP));
		writel(write0_id[idx]->bus_addr, (pci_regions[idx][0].base + GS_DMA_RD_TAP));
		break;

	default:
		break;
	}

return count;		// system retries forever while count==0
}


/*
** Ioctl control on the driver or the board(s)
** In general, ioctl's are not needed; it is preferable to use read/write methods
*/
static int gsfs_ioctl(struct inode *inode, struct file *filp,
		unsigned int cmd, unsigned long arg)
{
struct Gsfs_id *id=(struct Gsfs_id *)filp->private_data;
int idx = id->Idx, result;
spinlock_t lock;
char tsc_data[8];

//PDEBUG("ioctl device %d: 0x%p cmd: 0x%x arg: 0x%lx\n", idx, filp, cmd, arg);

spin_lock_init(&lock);
result =  -ENOTTY;

switch (cmd)
	{
	case GSD_TEST:
		PDEBUG("ioctl test reached for device: %d\n", idx);
		break;

	case GSD_INT_ENABLE:
		result = 0;
		spin_lock(&lock);
		if (gs_devices[idx].busy)		// if interrupt already enabled, error
			result = -EINVAL;
		else
			gs_devices[idx].busy = 1;
		spin_unlock(&lock);
		break;

	case GSD_INT_DISABLE:
		result = 0;
		spin_lock(&lock);
		// Disable the interrupt
		writel(GS_INT_ENABLE, pci_regions[idx][0].base+GS_RESET_CNTRL_REG);
		// wake up any waiting read or write
		if (gs_devices[idx].busy)
			wake_up_interruptible(&gs_queue);
		gs_devices[idx].busy = 0;
		spin_unlock(&lock);
		break;

	// return the time of the last interrupt
	case GSD_INT_TIME:
		*(unsigned int *)tsc_data = last_int_tsc_low;
		*((unsigned int *)tsc_data+1) = last_int_tsc_high;
		result = copy_to_user((char *)arg, tsc_data, sizeof(tsc_data));
		break;

	case GSD_INT_STATUS:
		result = copy_to_user((int *)arg, &gs_devices[idx].busy, sizeof(int));
		break;

	default:
		break;
	}

return result;
}


/*
** mmap method
*/
static int gsfs_mmap(struct file *filp, struct vm_area_struct *vma)
{
struct Gsfs_id *id=(struct Gsfs_id *)filp->private_data;
int idx = id->Idx, result, len;

//PDEBUG("mmap device %d, filp: 0x%p, vma: 0x%p, bus add: 0x%x\n", idx, filp, vma, id->bus_addr);

result =  -EINVAL;

switch (id->kind)
	{
	case Time:
	case Bar0:
	case Bar1:
		break;

	/* Set up to read from the PMC-GigaSTaR RX device into memory - allocate a DMA
	** buffer of the requested size, and transmit the buffer address (TAP) and
	** size to the device.  This is called a DMA write to the host.
	*/
	case Read0:
	case Read1:
		len = PAGE_ALIGN(vma->vm_end-vma->vm_start)>>PAGE_SHIFT;	/* length in pages */
		if (len==0 || len > DMA_N_PAGES_MAX)
			break;
		if (id->dma_length && len>id->dma_length)	/* increased size requested */
			release_dma_buffer(id);
		vma->vm_flags |= VM_RESERVED;		/* don't swap */
		if (id->dma_length==0)
			{
			id->dma_length = len;
			if ( (result = allocate_dma_buffer(id)) )
				break;
			}
		else
			result = 0;						/* success */
		vma->vm_ops = &gsfs_vm_ops;			/* introduce our vma ops to the kernel */
		vma->vm_private_data = (void *)id;	/* pass device ID to vma operations */
		gsfs_vm_open(vma);					/* fault in the first page */
			/* send target address pointer and size to card */
			/* note that vma addresses are page aligned */
		writel(id->bus_addr, (pci_regions[idx][0].base + GS_DMA_WR_TAP));
		writel((vma->vm_end-vma->vm_start)/4, (pci_regions[idx][0].base + GS_DMA_WR_BUF_SIZE));
		if (id->kind == Read0)
			read0_id[idx]=id;		/* record pointer to these data */
		else
			read1_id[idx]=id;		/* record pointer to these data */
		break;

	/* Set up to write from memory to the PMC-GigaSTaR device - allocate a DMA
	** buffer of the requested size, and transmit the buffer address (TAP) and
	** size to the device.  This is called a DMA read from the host.
	*/
	case Write0:
	case Write1:
		len = PAGE_ALIGN(vma->vm_end-vma->vm_start)>>PAGE_SHIFT;	/* length in pages */
		if (len==0 || len > DMA_N_PAGES_MAX)
			break;
		if (id->dma_length && len>id->dma_length)	/* increased size requested */
			release_dma_buffer(id);
		vma->vm_flags |= VM_RESERVED;		/* don't swap */
		if (id->dma_length==0)
			{
			id->dma_length = len;
			if ( (result = allocate_dma_buffer(id)) )
				break;
			}
		else
			result = 0;						/* success */
		vma->vm_ops = &gsfs_vm_ops;			/* introduce our vma ops to the kernel */
		vma->vm_private_data = (void *)id;	/* pass device ID to vma operations */
		gsfs_vm_open(vma);					/* fault in the first page */
			/* send target address pointer and size to card */
			/* note that vma addresses are page aligned */
		writel(id->bus_addr, (pci_regions[idx][0].base + GS_DMA_RD_TAP));
		writel((vma->vm_end-vma->vm_start)/4, (pci_regions[idx][0].base + GS_DMA_RD_BUF_SIZE));
		if (id->kind == Write0)
			write0_id[idx]=id;		/* record pointer to these data */
		else
			write1_id[idx]=id;		/* record pointer to these data */
		break;

	default:
		break;
	}

return result;
}




/*****************************************************************************\
**                                                                           **
**         vma operations                                                    **
**                                                                           **
\*****************************************************************************/

/*
** vma_open method
*/
void gsfs_vm_open(struct vm_area_struct *vma)
{
return;
}


/*
** vma_close method
*/
void gsfs_vm_close(struct vm_area_struct *vma)
{

vma->vm_flags &= ~VM_RESERVED;		/* undo reserved flag */

return;
}


/*
** nopage/fault method
*/

#if (LINUX_VERSION_CODE) >= KERNEL_VERSION(2,6,24)
static int gsfs_vm_fault(struct vm_area_struct *vma, 
		struct vm_fault *vmf)
{
struct Gsfs_id *id=(struct Gsfs_id *)vma->vm_private_data;
struct page* pageptr;
unsigned long add = (unsigned long)vmf->virtual_address;

if (add < vma->vm_start || add > vma->vm_end)
	return VM_FAULT_SIGBUS;		/* Disallow mremap */

pageptr = id->page_list[((add - vma->vm_start)>>PAGE_SHIFT)];
get_page(pageptr);
vmf->page = pageptr;

return 0;
}

#else
static struct page *gsfs_vm_nopage(struct vm_area_struct *vma, 
		unsigned long add, int *type)
{
struct Gsfs_id *id=(struct Gsfs_id *)vma->vm_private_data;
struct page* pageptr;

if (add < vma->vm_start || add > vma->vm_end)
	return NOPAGE_SIGBUS;		/* Disallow mremap */

if (type)
	*type = VM_FAULT_MINOR;

pageptr = id->page_list[((add - vma->vm_start)>>PAGE_SHIFT)];
get_page(pageptr);

return pageptr;
}
#endif



/*****************************************************************************\
**                                                                           **
**         Interrupt service routines                                        **
**                                                                           **
\*****************************************************************************/

/*
** Receive interrupts from the device.  The only action is to reenable a
** sleeping read or write operation, if present.
** The dev_id is the private parameter passed to request_irq(); it is the
** address of a member of gs_devices[] array.  In that array, devno is
** idex of its own array.  Thus, dev_id should match &gs_devices[idx],
** which is how we confirm the interrupt is ours.
*/
#if (LINUX_VERSION_CODE) <= KERNEL_VERSION(2,6,18)
  static irqreturn_t gs_irq_handler(int irq, void *dev_id, struct pt_regs *regs)
#else
  static irqreturn_t gs_irq_handler(int irq, void *dev_id)
#endif
{
int idx = ((struct Gs_dev*)dev_id)->devno;
unsigned int ui = readl(pci_regions[idx][0].base+GS_INTR_REG);

//PDEBUG("interrupt %d received - is it ours? (%p ?= %p), idx = %d\n", irq, dev_id, gs_devices+idx, idx);

/* if not our interrupt, should return immediately 
if (dev_id!=gs_devices+idx)
	return IRQ_NONE;

But, despite the teachings of Rubini's book, this does not work - the 
OS returns our dev_id even for USB mouse interrupts.  So, we ask the 
GigaSTaR board whether it is requesting an interrupt.
*/

if( !(ui & (
	GS_RX_LINK_ERROR_INT |
	GS_TX_LINK_ERROR_INT |
	GS_RX_FIFO_FULL_INT |
	GS_DMA_WR_DONE_INT |
	GS_DMA_WR_DIED_INT |
	GS_DMA_WR_STOP_INT |
	GS_DMA_WR_ABORT_INT |
	GS_DMA_RD_DONE_INT |
	GS_DMA_RD_OVFL_INT |
	GS_DMA_RD_STOP_INT |
	GS_DMA_RD_ABORT_INT
	)) )
	return IRQ_NONE;		// not caused by our board

/*
** record the time of the interrupt
*/
rdtsc(last_int_tsc_low, last_int_tsc_high);

/*
** Disable the interrupt
*/
writel(GS_INT_ENABLE, pci_regions[idx][0].base+GS_RESET_CNTRL_REG);

/*
** wake up the waiting read or write
*/
gs_devices[idx].busy = 0;
wake_up_interruptible(&gs_queue);

return IRQ_HANDLED;
}




/*****************************************************************************\
**                                                                           **
**         Create the file system representing the device(s)                 **
**                                                                           **
\*****************************************************************************/

/*
** Create the files that we export.
*/
static void gsfs_create_files (struct super_block *sb, struct dentry *root)
{
int i, m=0;
struct dentry *subdir;
char line[10];

gsfs_id[m].kind = Time;		// multiple cards get only 1 time device
gsfs_create_file(sb, root, "time", &gsfs_id[m]);
m++;

for(i=0; i<GS_MAX_DEVICES && gs_devices[i].device; i++)
	{
	sprintf(line, "gs%d", i);
	subdir = gsfs_create_dir(sb, root, line);
	if (subdir)
		{
		gsfs_id[m].kind = Bar0;
		gsfs_id[m].Idx = i;
		gsfs_id[m].device = gs_devices[i].device;
		gsfs_create_file(sb, subdir, "BAR0", &gsfs_id[m]);
		m++;
		gsfs_id[m].kind = Bar1;
		gsfs_id[m].Idx = i;
		gsfs_id[m].device = gs_devices[i].device;
		gsfs_create_file(sb, subdir, "BAR1", &gsfs_id[m]);
		m++;
		gsfs_id[m].kind = Read0;
		gsfs_id[m].Idx = i;
		gsfs_id[m].device = gs_devices[i].device;
		gsfs_id[m].dma_length = 0;
		gsfs_id[m].bus_addr = 0;
		gsfs_id[m].dma_buff = NULL;
		gsfs_create_file(sb, subdir, "read0", &gsfs_id[m]);
		read0_id[i] = &gsfs_id[m];		// init to sane values
		read1_id[i] = &gsfs_id[m];
		m++;
		gsfs_id[m].kind = Read1;
		gsfs_id[m].Idx = i;
		gsfs_id[m].device = gs_devices[i].device;
		gsfs_id[m].dma_length = 0;
		gsfs_id[m].bus_addr = 0;
		gsfs_id[m].dma_buff = NULL;
		gsfs_create_file(sb, subdir, "read1", &gsfs_id[m]);
		m++;
		gsfs_id[m].kind = Write0;
		gsfs_id[m].Idx = i;
		gsfs_id[m].device = gs_devices[i].device;
		gsfs_id[m].dma_length = 0;
		gsfs_id[m].bus_addr = 0;
		gsfs_id[m].dma_buff = NULL;
		gsfs_create_file(sb, subdir, "write0", &gsfs_id[m]);
		write0_id[i] = &gsfs_id[m];		// init to sane values
		write1_id[i] = &gsfs_id[m];
		m++;
		gsfs_id[m].kind = Write1;
		gsfs_id[m].Idx = i;
		gsfs_id[m].device = gs_devices[i].device;
		gsfs_id[m].dma_length = 0;
		gsfs_id[m].bus_addr = 0;
		gsfs_id[m].dma_buff = NULL;
		gsfs_create_file(sb, subdir, "write1", &gsfs_id[m]);
		m++;
		}
	}
return;
}


/*
** Create a file mapping a name to a register.
*/
static struct dentry *gsfs_create_file (struct super_block *sb,
		struct dentry *dir, const char *name, void *id)
{
struct dentry *dentry;
struct inode *inode;
struct qstr qname;
/*
** Make a hashed version of the name to go with the dentry.
*/
qname.name = name;
qname.len = strlen (name);
qname.hash = full_name_hash(name, qname.len);
/*
** Now we can create our dentry and the inode to go with it.
*/
dentry = d_alloc(dir, &qname);
if (! dentry)
	return 0;
inode = gsfs_make_inode(sb, S_IFREG | 0644);
if (! inode){
	dput(dentry);
	return 0;
	}
inode->i_fop = &gsfs_file_ops;
#if (LINUX_VERSION_CODE) <= KERNEL_VERSION(2,6,17)
  inode->u.generic_ip = id;
#else
  inode->i_private = id;
#endif
/*
** Put it all into the dentry cache and we're done.
*/
d_add(dentry, inode);
return dentry;
}


/*
** Create a directory which can be used to hold files.  This code is
** almost identical to the "create file" logic, except that we create
** the inode with a different mode, and use the libfs "simple" operations.
*/
static struct dentry *gsfs_create_dir (struct super_block *sb,
		struct dentry *parent, const char *name)
{
struct dentry *dentry;
struct inode *inode;
struct qstr qname;

qname.name = name;
qname.len = strlen (name);
qname.hash = full_name_hash(name, qname.len);
dentry = d_alloc(parent, &qname);
if (! dentry)
	return 0;
inode = gsfs_make_inode(sb, S_IFDIR | 0744);
if (! inode){
	dput(dentry);
	return 0;
	}
inode->i_op = &simple_dir_inode_operations;
inode->i_fop = &simple_dir_operations;

d_add(dentry, inode);
return dentry;
}


/*
** Make an inode to represent either a directory or file in the filesystem.
*/
static struct inode *gsfs_make_inode(struct super_block *sb, int mode)
{
struct inode *ret = new_inode(sb);

if (ret) {
	ret->i_mode = mode;
	ret->i_uid = ret->i_gid = 0;
#if (LINUX_VERSION_CODE) <= KERNEL_VERSION(2,6,17)
	ret->i_blksize = PAGE_CACHE_SIZE;
#endif
	ret->i_blocks = 0;
	ret->i_atime = ret->i_mtime = ret->i_ctime = CURRENT_TIME;
}
return ret;
}


/*
** "Fill" a superblock.
*/
static int gsfs_fill_super (struct super_block *sb, void *data, int silent)
{
struct inode *root;
struct dentry *root_dentry;
/*
* Basic parameters.
*/
sb->s_blocksize = PAGE_CACHE_SIZE;
sb->s_blocksize_bits = PAGE_CACHE_SHIFT;
sb->s_magic = GSFS_MAGIC;
sb->s_op = &gsfs_s_ops;
/*
** Make up an inode to represent the root directory of this filesystem.
** Its operations all come from libfs.
*/
root = gsfs_make_inode (sb, S_IFDIR | 0755);
if (! root)
	return -ENOMEM;
root->i_op = &simple_dir_inode_operations;
root->i_fop = &simple_dir_operations;
/*
** Get a dentry to represent the directory in memory.
*/
root_dentry = d_alloc_root(root);
if (! root_dentry){
	iput(root);
	return -ENOMEM;
	}
sb->s_root = root_dentry;
/*
** Make up the files which will be in this filesystem.
*/
gsfs_create_files (sb, root_dentry);
return 0;
}


/*
** Stuff to pass in when registering the filesystem.
*/


#if (LINUX_VERSION_CODE) <= KERNEL_VERSION(2,6,17)
static struct super_block *gsfs_get_super(struct file_system_type *fst,
		int flags, const char *devname, void *data)
{
return get_sb_single(fst, flags, data, gsfs_fill_super);
}
#else
int gsfs_get_super(struct file_system_type *fst,
		int flags, const char *devname, void *data, struct vfsmount *mnt)
{
return get_sb_single(fst, flags, data, gsfs_fill_super, mnt);
}
#endif


/*****************************************************************************\
**                                                                           **
**      DMA allocation and release                                           **
**                                                                           **
\*****************************************************************************/

static int allocate_dma_buffer(struct Gsfs_id *id)
{
int i;
struct pci_dev *dev=id->device;
struct page *pageptr;

id->dma_buff = pci_alloc_consistent(dev, (id->dma_length<<PAGE_SHIFT), &id->bus_addr);
if (!id->dma_buff)
	goto dma_fail;

for (i=0; i<id->dma_length; i++)
	{
	pageptr = virt_to_page(id->dma_buff + i*PAGE_SIZE);
	if (!pageptr)
		goto dma_fail;
	SetPageReserved(pageptr);
	get_page(pageptr);
	id->page_list[i] = pageptr;
	}

return 0;

dma_fail:
id->dma_buff = NULL;
id->bus_addr = 0;
id->dma_length = 0;
return -ENOMEM;
}


static int release_dma_buffer(struct Gsfs_id *id)
{
int i;
struct pci_dev *dev=id->device;


if (id->bus_addr)
	{
	for (i=0; i<id->dma_length; i++)
		ClearPageReserved(id->page_list[i]);
	pci_free_consistent(dev, (id->dma_length<<PAGE_SHIFT), id->dma_buff, id->bus_addr);
	id->dma_buff = NULL;
	id->bus_addr = 0;
	id->dma_length = 0;
	}

return 0;
}

/*****************************************************************************\
**                                                                           **
**      Load driver & capture configuration info from the board(s)           **
**                                                                           **
\*****************************************************************************/

/*
** gigastar_probe() - probe the parameters of each PMC-GigaSTaR card that has
** been found.
** It is shown how to probe many PCI parameters, but the only ones we need
** to remember here are the base address and the size.  Also shown is a 
** write/readback test for writable spaces.
** Note that some of these paramters are already available in the dev structure
** (see struct pci_dev in pci.h).   
*/
static int gigastar_probe(struct pci_dev *dev, const struct pci_device_id *id)
{
int i, j, result, fail, idx;
u8 tmp_u8;
u16 tmp_u16;
u32 tmp_u32, size, mask, d;
spinlock_t lock;
unsigned int *uip;
unsigned char gs_irqline;

//PDEBUG("GigaSTaR probe function\n");

spin_lock_init(&lock);
result = -ENODEV;	/* for the possible errors */

/* assign an unique index to this device (card) */
idx=-1;
for(i=0;i<GS_MAX_DEVICES;i++)
	if (gs_devices[i].device == NULL)
		{
		idx=i;
		break;
		}
if (idx<0)
	goto fail_config;		

if (pci_enable_device(dev))
	goto fail_config;
pci_set_master(dev);		// PMC-GigaSTaR cards are bus master capable

if (pci_read_config_byte(dev, PCI_REVISION_ID, &tmp_u8))
	goto fail_config;
//PDEBUG("PCI_REVISION_ID = 0x%x\n", tmp_u8);
if (pci_read_config_byte(dev, PCI_INTERRUPT_PIN, &tmp_u8))
	goto fail_config;
//PDEBUG("PCI_INTERRUPT_PIN = %d\n", tmp_u8);
if (!tmp_u8)		// must be >0 to do interrupts
	goto fail_config;
// note that the OS has assigned an interrupt number, which probably differs
if (pci_read_config_byte(dev, PCI_INTERRUPT_LINE, &gs_irqline))
	goto fail_config;
//PDEBUG("PCI_INTERRUPT_LINE = %d\n", gs_irqline);

pci_read_config_word(dev, PCI_COMMAND, &tmp_u16);
//PDEBUG("PCI_COMMAND = 0x%x\n", tmp_u16);
if (tmp_u16 & PCI_COMMAND_MASTER)
	PDEBUG("Master capable = y\n");
else
	{
	PDEBUG("Master capable = n -- bad news\n");
	goto fail_config;
	}
pci_read_config_byte(dev, PCI_BIST_CAPABLE, &tmp_u8);
//PDEBUG("PCI_BIST_CAPABLE = %d\n", tmp_u8);
if (tmp_u8)
	{
	pci_read_config_byte(dev, PCI_BIST_CODE_MASK, &tmp_u8);
	if (tmp_u8)
		{
		printk(KERN_INFO "gigastar device reports failed BIST\n");
		goto fail_config;
		}
	}
pci_read_config_byte(dev, PCI_BIST_CODE_MASK, &tmp_u8);
//PDEBUG("PCI_BIST_CODE_MASK = %d\n", tmp_u8);
pci_read_config_byte(dev, PCI_LATENCY_TIMER, &tmp_u8);
//PDEBUG("PCI_LATENCY_TIMER = %d\n", tmp_u8);
pci_read_config_byte(dev, PCI_MIN_GNT, &tmp_u8);
//PDEBUG("PCI_MIN_GNT = %d\n", tmp_u8);
pci_read_config_byte(dev, PCI_MAX_LAT, &tmp_u8);
//PDEBUG("PCI_MAX_LAT = %d\n", tmp_u8);
pci_read_config_byte(dev, PCI_INTERRUPT_LINE, &gs_irqline);
//PDEBUG("Before reset: PCI_INTERRUPT_LINE = %d\n", gs_irqline);



//discover the size of the pci regions
for (i=0; i<PCI_BAR_MAX; i++)
	{
	pci_regions[idx][i].base = NULL;
	pci_read_config_dword(dev, pci_addresses[idx][i][0], &tmp_u32);
	spin_lock(&lock);
	pci_write_config_dword(dev, pci_addresses[idx][i][0], ~0);
	pci_read_config_dword(dev, pci_addresses[idx][i][0], &mask);
	pci_write_config_dword(dev, pci_addresses[idx][i][0], tmp_u32);
	spin_unlock(&lock);

	if (!mask)		// regions can be dis-contiguous
		continue;
	if (tmp_u32 & PCI_BASE_ADDRESS_MEM_TYPE_64)
		PDEBUG("BASE_ADDRESS_%d is 64 bits\n", i);
	if (tmp_u32 & PCI_BASE_ADDRESS_SPACE_IO)
		{
		PDEBUG("PCI_BASE_ADDRESS_%d is I/O mapped, which we don't handle\n", i);
		goto fail_config;
		}
	tmp_u32 &= PCI_BASE_ADDRESS_MEM_MASK;
	mask &= PCI_BASE_ADDRESS_MEM_MASK;
	size = ~mask + 1;
	printk(KERN_INFO "PCI_BASE_ADDRESS_%d: 0x%x, mask: 0x%x, size: 0x%x\n", i, tmp_u32, mask, size);
	pci_regions[idx][i].base = ioremap_nocache(tmp_u32, size);
	pci_regions[idx][i].size = size;
	//PDEBUG("After remap: 0x%p\n", pci_regions[idx][i].base);
	if (pci_addresses[idx][i][1]==DO_TEST)
		{
		fail = 0;
		uip = pci_regions[idx][i].base;
		for (j=0; j<size/4; j++)
			writel(0x55555555, uip+j);
		for (j=0; j<size/4; j++)
			if ((d=readl(uip+j)) != 0x55555555)
				{
				PDEBUG("Readback error 1: j = %d, data = 0x%x\n", j, d);
				fail = 1;
				break;
				}
		for (j=0; j<size/4; j++)
			writel(0xaaaaaaaa, uip+j);
		for (j=0; j<size/4; j++)
			if ((d=readl(uip+j)) != 0xaaaaaaaa)
				{
				PDEBUG("Readback error 2: j = %d, data = 0x%x\n", j, d);
				fail = 1;
				break;
				}
		if (!fail) PDEBUG("region %d: READ/WRITE test passed.\n", i);
		}
	}

// enter this device in our table
gs_devices[idx].device = dev;

read0_id[idx]=NULL;
read1_id[idx]=NULL;
write0_id[idx]=NULL;
write1_id[idx]=NULL;

// show revision code
d=readl(pci_regions[idx][0].base + GS_PMC_VERSION);
printk(KERN_INFO"*PMC-GigaSTaR* device #%d revision code: 0x%x\n", idx, d);

// determine the sizes of the fifos - note that cases 0 & 7 are ambiguous
switch ((d & 0x1c00)>>10)		// bits 12:10 - the tx fifo size in dwords
	{
	case 0: gs_devices[idx].tx_fifo_size = 8; break;		// i.e., <16
	case 1: gs_devices[idx].tx_fifo_size = 16; break;		// default
	case 2: gs_devices[idx].tx_fifo_size = 64; break;
	case 3: gs_devices[idx].tx_fifo_size = 128; break;
	case 4: gs_devices[idx].tx_fifo_size = 512; break;
	case 5: gs_devices[idx].tx_fifo_size = 1024; break;
	case 6: gs_devices[idx].tx_fifo_size = 4096; break;
	case 7: gs_devices[idx].tx_fifo_size = 8192; break;		// i.e., >4k
	}
PDEBUG("Got TX fifo size of %d\n", gs_devices[idx].tx_fifo_size);

switch ((d & 0xe000)>>13)		// bits 15:13 - the rx fifo size in dwords
	{
	case 0: gs_devices[idx].rx_fifo_size = 2048; break;		// i.e., <4k
	case 1: gs_devices[idx].rx_fifo_size = 4096; break;
	case 2: gs_devices[idx].rx_fifo_size = 8192; break;		// default
	case 3: gs_devices[idx].rx_fifo_size = 16384; break;
	case 4: gs_devices[idx].rx_fifo_size = 32768; break;
	case 5: gs_devices[idx].rx_fifo_size = 65536; break;
	case 6: gs_devices[idx].rx_fifo_size = 131072; break;
	case 7: gs_devices[idx].rx_fifo_size = 262144; break;	// i.e., >128k
	}
PDEBUG("Got RX fifo size of %d\n", gs_devices[idx].rx_fifo_size);

// reset the card
writeb(0xff, pci_regions[idx][0].base);
for(i=0; i<100000; i++)						// ~3300 loops required
	if ( !(readb(pci_regions[idx][0].base) & 0x1) )
		break;
//PDEBUG("reset loop count was %d\n", i);
if (i>90000)
	printk(KERN_ALERT "*** Time-out during Gigastar card %d reset\n", idx);

// make sure defaults are set
writel(0x0, (pci_regions[idx][0].base + GS_IRQ_SRC_REG));
writel(0x0, (pci_regions[idx][0].base + GS_IRQ_MASK_REG));

writel(0x0, (pci_regions[idx][0].base + GS_DMA_WR_TAP));
writel(0x0, (pci_regions[idx][0].base + GS_DMA_RD_TAP));

writel(0x0, (pci_regions[idx][0].base + GS_DMA_WR_BUF_SIZE));
writel(0x0, (pci_regions[idx][0].base + GS_DMA_RD_BUF_SIZE));

// Maximum busrtsize acceptable to system appears to be 0x1000
writew(0x400, (pci_regions[idx][0].base + GS_DMA_WR_BURSTSIZE));
writew(0x400, (pci_regions[idx][0].base + GS_DMA_RD_BURSTSIZE));

// grab the irq line that has been set up by the OS
// pass our device identifier structure as the private data
//PDEBUG("dev->irq from OS: %d\n", dev->irq);
#if (LINUX_VERSION_CODE) <= KERNEL_VERSION(2,6,21)
result = request_irq(dev->irq, gs_irq_handler, SA_INTERRUPT | SA_SHIRQ,
			"gstar", &gs_devices[idx]);
#else
result = request_irq(dev->irq, gs_irq_handler, IRQF_DISABLED|IRQF_SHARED,
			"gstar", &gs_devices[idx]);
#endif

if (result)
	{
	printk(KERN_INFO"gsd: can't get assigned irq %d\n", (int)gs_irqline);
	goto fail_config;
	}
else
	{
	gs_irq[idx] = dev->irq;	// success
	printk(KERN_INFO"gsd grabbed IRQ %d\n", gs_irq[idx]);
	}

if (pci_set_consistent_dma_mask(dev, DMA_32BIT_MASK))
	{
	printk(KERN_WARNING "gsd: no DMA available\n");
	goto fail_config;
	}

return 0;

fail_config:

printk(KERN_INFO"Module loading failed: %s:%i\n", __FILE__, __LINE__);
return result;
}


/*
** Remove a device
*/
static void gigastar_remove(struct pci_dev *dev)
{
int i, idx;

for (idx=0; idx<GS_MAX_DEVICES; idx++)
	if (dev == gs_devices[idx].device)
		goto remove;
printk(KERN_INFO"*** PMC-GigaSTaR driver could not find device to remove\n");
return;

remove:

for (i=0; i<N_GS_IDS; i++)
	{
	if (gsfs_id[i].Idx == idx)
		release_dma_buffer(&gsfs_id[i]);
	}

free_irq(gs_irq[idx], &gs_devices[idx]);
printk(KERN_INFO"PMC-GigaSTaR released IRQ %d\n", gs_irq[idx]);
gs_irq[idx] = -1;

pci_disable_device(dev);
return;
}


/*
** Initialize on module loading
*/
static int __init gigastar_init(void)
{
int i, j, result;

//printk(KERN_ALERT "*** Loading PSI PMC-GigaSTaR PCI card driver\n");

/*
** Initialize tables
*/
for(i=0;i<GS_MAX_DEVICES;i++)
	{
	gs_devices[i].devno = i;
	gs_devices[i].device = NULL;
	gs_irq[i] = -1;
	}

memset(gsfs_id, 0, N_GS_IDS*sizeof(struct Gsfs_id));

for(i=0;i<GS_MAX_DEVICES;i++)
	for(j=0;j<PCI_BAR_MAX;j++)
		{
		pci_addresses[i][j][0] = pci_addresses_proto[j][0];
		pci_addresses[i][j][1] = pci_addresses_proto[j][1];
		}

/*
** Grab the GigaSTaR PCI card(s).
*/
if( (result = pci_register_driver(&gs_driver)) )
	goto fail_config;

if (gs_devices[0].device == NULL)
	{
	printk(KERN_INFO "Error: could not find PMC-GigaSTaR hardware\n");
	goto fail_config;
	}

if( (result = register_filesystem(&gsfs_type)) )
	goto fail_config;

printk(KERN_INFO "*PMC-GigaSTaR* module: init complete\n");

return 0;

fail_config:

PDEBUG("Module loading failed: %s:%i\n", __FILE__, __LINE__);
return result;
}


/*
** Unload module
*/
static void __exit gigastar_exit(void)
{
//printk(KERN_ALERT "*** Unloading PSI PMC- GigaSTaR PCI card driver\n");

unregister_filesystem(&gsfs_type);
pci_unregister_driver(&gs_driver);
return;
}
