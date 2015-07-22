// gsdc.h - PMC Gigastar configuration info to share with applications
// tabs = 4 to line up columns

/*
** Copyright (C) 2004-2009 Eric F. Eikenberry
** Paul Scherrer Institute, CH-5232 Villigen-PSI, Switzerland.
** All rights reserved.
*/

#ifndef GSCD_H
#define GSCD_H

#define GS_MAX_DEVICES 5	/* maximum number of PMC GigaSTaR PCI cards */

/*
** offsets of 32-bit registers from the device base
*/
#define GS_CONTROL_REG			0		// global control register
#define GS_SET_CNTRL_REG		1		// set individual bits in control register
#define GS_RESET_CNTRL_REG		2		// reset individual bits in control register
#define GS_STATUS_REG			3		// status register
#define GS_IRQ_SRC_REG			4		// read & clear potential interrupt sources
#define GS_IRQ_MASK_REG			5		// select which conditions may generate interrupts
#define GS_INTR_REG				6		// read actual source(s) of interrupt
#define GS_RX_MBOX_REG			8		// mailbox for incoming messages
#define GS_TX_MBOX_REG			9		// mailbox for outgoing messages
#define GS_RX_FIFO_SIDEBITS		10		// RX fifo fill level and sidebits
#define GS_TX_FIFO_SIDEBITS		11		// TX fifo sidebits
#define GS_RX_FIFO_PROGFLAG		12		// programmable flag (threshold) of RX fifo
#define GS_LVDS_RX_IO_REG		16		// LVDS I/O from the RX link
#define GS_LVDS_TX_IO_REG		17		// LVDS I/O from the TX link
#define GS_DMA_WR_TAP			20		// write target (card to memory)
#define GS_DMA_WR_BUF_SIZE		21		// write buffer size
#define GS_DMA_WR_BURSTSIZE		22		// write burst size (16 bit); default = 0x400 dwords
#define GS_DMA_WR_TIMEOUT		23		// timeout for write operations, n*25 us; default n=100
#define GS_DMA_RD_TAP			24		// read target (memory to card)
#define GS_DMA_RD_BUF_SIZE		25		// read buffer size
#define GS_DMA_RD_BURSTSIZE		26		// read burst size (16-bit); default = 0x400 dwords
#define GS_DMA_WR_TRANSFER		28		// DMA write: # of data transferred
#define GS_DMA_RD_TRANSFER		29		// DMA read: # of data transferred
#define GS_PMC_VERSION			31		// hardware & firmware revision

/*
** Bit masks for global control register(0), and set(1) / reset(2) control registers
** Best to use these in set/reset control registers to manipulate bits in global control
*/
#define GS_RES_RX_FIFO			0x4			// reset RX fifo
#define GS_RES_TX_FIFO			0x8			// reset TX fifo
#define GS_INT_RESET			0x20		// reset interrupts
#define GS_MODE_MASK			0x300		// set mode: idle, R, T or U
#define GS_ID_ADR_MASK			0xf000		// set device ID
#define GS_ENB_SIDEBIT_ADR		0x10000		// enable side bit addressing
#define GS_ENB_FRM_SYNC			0x20000		// enable enclosed data frame (see docs)
#define GS_INT_ENABLE			0x40000		// enable interrupts
#define GS_ENB_DMA_TIMEOUT		0x80000		// enable timeout on DMA write
#define GS_FIRE_DMA_WRITE		0x100000	// start DMA write (RX to host)
#define GS_FORCE_DMA_WRITE		0x200000	// fore DMA write, ignoring burst size
#define GS_FIRE_DMA_READ		0x400000	// start DMA read (host to TX)
#define GS_RUN_PMC				0x1000000	// set run mode

/*
** Note that the mode state must be left-shifted 8 bits to set the mode in the
** control register.  Reading the mode in the status register requires no shift
*/
#define GS_MODE_I				0x0			// idle mode
#define GS_MODE_R				0x1			// R mode
#define GS_MODE_T				0x2			// T mode
#define GS_MODE_U				0x3			// U mode
#define GS_CTRL_REG_MODE_SHIFT	8			// bit shift for mode in control register

/*
** Bits in global status register (offset 3)
*/
#define GS_MODE					0x3			// read mode: 01 = R mode, 10 = T, 11 = U
#define GS_SIDEBIT_ADDR			0X4			// side bit addressing enabled
#define GS_FRM_SYNC				0X8			// enclosed data frame sync enabled
#define GS_INTR					0X10		// global interrupt enabled
#define GS_DMA_TIMEOUT			0X20		// DMA timeout timer enabled
#define GS_PMC_RUN				0X40		// run mode enabled
#define GS_INIT_RXTX			0x80		// initialization in progress
#define GS_RX_LINK_ERR			0x100
#define GS_TX_LINK_ERR			0x200
#define GS_RX_FLAG_O			0X800		// GigaSTaR chipset flag level
#define GS_RX_FIFO_EMPTY		0x1000
#define GS_RX_FIFO_PARTIAL		0x2000		// programmable; default partial level = 256
#define GS_RX_FIFO_FULL			0x4000
#define GS_TX_FIFO_EMPTY		0x10000
#define GS_TX_FIFO_FULL			0x40000
#define GS_RX_MBOX_FULL			0x100000
#define GS_TX_MBOX_EMPTY		0x400000

/*
** IRQ bits - applicable to IRQ source register (offset 4), IRQ mask register
** (offset 5) and interrupt register (offset 6).
** The source register shows the hardware logic state.
** The mask controls which of the source signals may be permitted to cause an interrrupt.
** The interrupt register shows which condition did cause an interrupt
**
** Note: DMA read = host to TX (reading from host);
**       DMA write = RX to host (writing to host).
**
*/
#define GS_RX_LINK_ERROR_INT	0x1			// GigaSTaR RX chip link error
#define GS_TX_LINK_ERROR_INT	0x2			// GigaSTaR TX chip link error
#define GS_RX_FLAG_TOGGLE_INT	0x4			// GigaSTaR FLAGO - see docs
#define GS_RX_LAST_FRAME_INT	0x8			// RX stop mark detected
#define GS_RX_FIFO_NOT_MT_INT	0x10		// RX fifo not empty
#define GS_RX_FIFO_PART_INT		0x20		// RX fifo above set threshold
#define GS_RX_FIFO_FULL_INT		0x40		// RX fifo full
#define GS_TX_FIFO_FULL_INT		0x400		// TX fifo full
#define GS_RX_FIFO_SBITS_MASK_INT	0xf0000	// mask for the 4 side bits
#define GS_RX_MBOX_MSG_INT		0x10000		// RX mailbox has a new message
#define GS_RX_MBOX_OVFL_INT		0x20000		// RX mailbox overflow
#define GS_TX_MBOX_OVFL_INT		0x80000		// TX mialbox overflow
#define GS_DMA_WR_DONE_INT		0x1000000	// successful DMA write (RX to host)
#define GS_DMA_WR_DIED_INT		0x2000000	// DMA write timeout - RX underrun
#define GS_DMA_WR_STOP_INT		0x4000000	// DMA write - max # retries exceeded
#define GS_DMA_WR_ABORT_INT		0x8000000	// PCI-bus target or master abort on RX
#define GS_DMA_RD_DONE_INT		0x10000000	// successful DMA read (host to TX)
#define GS_DMA_RD_OVFL_INT		0x20000000	// DMA read - TX fifo overflow
#define GS_DMA_RD_STOP_INT		0x40000000	// DMA read - max # retries exceeded
#define GS_DMA_RD_ABORT_INT		0x80000000	// PCI-bus target or master abort on TX


// our private ioctl's
#define GSD_MAGIC 'g'
#define GSD_TEST		_IO(GSD_MAGIC, 1)
#define GSD_INT_ENABLE	_IO(GSD_MAGIC, 2)
#define GSD_INT_DISABLE	_IO(GSD_MAGIC, 3)
#define GSD_INT_TIME   _IOR(GSD_MAGIC, 4, char[8])
#define GSD_INT_STATUS _IOR(GSD_MAGIC, 5, int)



#if 0		// examples of how to construct other ioctl's
#define GSD_INT_KILL     _IO(GSD_MAGIC, 4)
#define GSD_GET_STATUS  _IOR(GSD_MAGIC, 5, size)
#define GSD_CONFIG_PUT  _IOW(GSD_MAGIC, 6, size)
#define GSD_CONFIG_GET  _IOR(GSD_MAGIC, 7, size)
#endif


#endif
