// gsd.h - declarations for the gs_drv module

#ifndef GSD_H
#define GSD_H

#ifndef LINUX_VERSION_CODE
#  include <linux/version.h>
#endif

#if (LINUX_VERSION_CODE) < KERNEL_VERSION(2,6,6)
#  error "This kernel is too old: not supported by this module"
#endif

#ifndef CONFIG_PCI
#  error "This driver requires PCI support"
#endif

/* 
** Some definitions in the next section may need to be modified by the user
** Note that the definition of 'GS_MAX_DEVICES' is in gsdc.h, the header
** that is shared with applications.
*/

#define GS_VENDOR_ID 0x10ee
#define GS_DEVICE_ID 0x300

/*
** DMA-capable pages are a relatively scarce resource in the system, so set
** number of pages requested for each buffer to just cover your application.
** The following example is for 1 module of the PILATUS II pixel detector:
**    (60 columns * 97 rows * 20 bits/pix * 2) / 4096 =~ 57 pages
** where the '2' is from the 16 chips which in parallel write 2 bytes per clock
**
** Expand to read a full DCB (envisioned up to 6 banks = 7 MByte)
**
** The maximum size of a DMA allocation is controlled by MAX_ORDER in
** include/linux/mmzone.h.  'cat /proc/buddyinfo' will show what memory
** blocks are available in the system.  You may need a fresh boot.
*/
#define NM 5		// number of modules per bank
#define NB 6		// number of banks on 1 DCB

#define DMA_N_PAGES_MAX (PAGE_ALIGN(NM*NB*60*97*20*2)>>PAGE_SHIFT)

/*
** End of user-configurable definitions
*/

#define PCI_BAR_MAX 6		/* max number of BAR's in a pci device */

#define GSFS_MAGIC 0x20041206	/* unique identifier for file systems */

/* the kinds of "sub-devices" we support */
typedef enum {
	Read0 = 0,
	Read1,
	Write0,
	Write1,
	Bar0,
	Bar1,
	Time			/* must be last in list */
	} Gsfs_subdev;

#define N_GS_SUBDEV (1+(int)Time)

#define N_GS_IDS (1+N_GS_SUBDEV*GS_MAX_DEVICES)	/* time; (Read0, etc) each card */

/*
** The fpga contains some extra dwords for the RX fifo.  These are declared
** here to permit read operations on the extended size.
*/
#define RX_FIFO_EXTRAS 10

#define DO_TEST 1
#define DO_NOT_TEST 0


typedef struct {
	unsigned int *base;
	u32 size;
	} Pci_Region;

struct Gsfs_id {
	Gsfs_subdev kind;
	int Idx;
	void *device;
	int dma_length;			/* dma buffer length in pages */
	dma_addr_t bus_addr;	/* dma buffer bus address */
	void *dma_buff;			/* dma buffer virtual address */
	struct page *page_list[DMA_N_PAGES_MAX];
	};


/* used in setting up pci region to device mappings */
#define PCI_REGION_0 0
#define PCI_REGION_1 1
#define PCI_REGION_2 2
#define PCI_REGION_3 3
#define PCI_REGION_4 4
#define PCI_REGION_5 5
#define PCI_NONE    -1


/* to read the CMOS clock */
#define RTC_PORT(x) (0x70 + (x))
#define CMOS_READ(addr) ({ \
outb_p((addr),RTC_PORT(0)); \
inb_p(RTC_PORT(1)); \
})


#undef PDEBUG					/* undef it, just in case */
#ifdef DEBUG
#  ifdef __KERNEL__
     /* This one if debugging is on, and kernel space */
#    define PDEBUG(fmt, args...) printk( KERN_INFO "gsd: " fmt, ## args)
#  else
     /* This one for user space */
#    define PDEBUG(fmt, args...) fprintf(stderr, fmt, ## args)
#  endif
#else
#  define PDEBUG(fmt, args...) /* not debugging: nothing */
#endif


#endif
