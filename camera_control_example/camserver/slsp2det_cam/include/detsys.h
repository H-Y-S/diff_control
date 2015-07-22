/* detsys.h - declarations specific to the slsp2det detector system
** 2 bank version
*/

/*
** Be sure to check dcb.h
*/


#ifndef DETSYS_H
#define DETSYS_H

/*
** define exactly 1 camera option
**
** single chip is not supported here - see other directory branch
*/

//#define SLSP2_1M_CAMERA			// 100K -- single module
//#define SLSP2_2M_CAMERA			// 200K - 2 modules, high form, 2 DCBs
//#define SLSP2_2MW_CAMERA			// 200K wide form, BCB, 1 DCB
//#define SLSP2_2MS_CAMERA			// 200K slave computer, high form, 2 DCBs
//#define SLSP2_2MWS_CAMERA			// 200KW slave computer, wide form, 2 DCBs
//#define SLSP2_3M2_CAMERA			// 300K -- 3 modules, high form, 3 DCBs, 2 computers
//#define SLSP2_3M1_CAMERA			// 300K -- 3 modules, high form, 3 DCBs, 1 computer
//#define SLSP2_3MW_CAMERA			// 300KW -- 3 modules, wide form, 1 DCB
//#define SLSP2_3MWF2_CAMERA		// 300KWF -- 3 modules, wide form, 3 DCBs, 2 computers
//#define SLSP2_3MWF1_CAMERA		// 300KWF -- 3 modules, wide form, 3 DCBs, 1 computer
#define SLSP2_10M_CAMERA			// 1M -- 5 banks x 2 modules
//#define SLSP2DET_CAMERA_1BANK		// 500K - 5 modules, BCB, 1 DCB
//#define SLSP2DET_CAMERA_2BANK		// 2 x 500K = 1MW
//#define SLSP2DET_CAMERA_3BANK
//#define SLSP2DET_CAMERA_4BANK
//#define SLSP2DET_CAMERA_5BANK
//#define SLSP2DET_CAMERA_6BANK
//#define SLSP2DET_CAMERA_8BANK			// 2M - normal
//#define SLSP2DET_CAMERA_8BANK_4B		// 2M - 4 central banks
//#define SLSP2DET_CAMERA_8BANK_2B		// 2M - 2 central banks
//#define SLSP2DET_CAMERA_8BANK_2M		// 2M - 2 central modules
//#define SLSP2DET_CAMERA_8BANK_8M		// 2M - 4 central banks x 2 left mods
//#define SLSP2DET_CAMERA_12BANK		// 6M
//#define SLSP2PS_CAMERA				// probe station



/*
** Configuring a new camera format (not one of the presets here) requires:
** NBANK, NDCB, NBANK_DCB & BANKENP1
*/


//#define USE_GPIB					// gpib multimeter and oscilloscope

/*
** Note that 'DMA_N_PAGES_MAX' in gsd.h must match detector readout
** format.  For the per-module readout, need only DMA pages for one
** module.  But for AUTO read of the entire detector, DMA buffers
** must hold NBANK * NMOD_BANK modules.
**
**
** The module enable pattern (register: DCB_MODULE_ENABLE_REG) can
** be calculated from NMOD_BANK, assuming that the modules are placed
** on the bank in order.  
**
** But, the bank enable pattern (register: DCB_BANK_ENABLE_REG)
** is difficult to calculate because it must match the settings
** of the switches on the BCBs.
**
** Consider a 6 bank detector with 2 dcb's.  If the bcb's are 
** configured as 1,...6, the first dcb should get a pattern of
** 000111 and the second dcb should receive 111000.  But, it is 
** also possible to have 6 banks numbered 1,2,3 and 1,2,3 on
** separate dcb's - the important point is that the addresses
** of the bcb's match the enable pattern.  See the firmware
** docs for more details.
**
** For 12 banks, I've assumed the banks are numbered 1,...6 and 1,...6;
** not 1,...6 and 9,...14.
**
** There must be a bank enable pattern for each DCB in 'dcblib.c'
**
** A custom module enable pattern, e.g., only the center module in a 
** 3-module bank, may be specified by:
**		#define MOD_ENB_PATTERN 2
** In addition, for this situation use
**		#define USE_BCB
** otherwise NMOD_BANK==1 implies no BCB.
*/

#undef MOD_ENB_PATTERN		// define this for a custom pattern
#undef USE_BCB				// NMOD_BANK==1 normally implies no BCB

// unique detector names prevent multiple definitions
#if defined SLSP2_1M_CAMERA							// 1 module
  #define NMOD_BANK 1								// 487 pix wide
  #define NBANK 1									// 195 pix high
  #define NBANK_DCB 1								// banks on 1 dcb
  #define DETECTOR "PILATUS 100K"
  #define NDCB 1			// number of dcb's == number of gigastar's
  #define BANKENP1 0		// not used in single module
  #define BANKENP2 0		// unused
  #define BANKENP3 0		// unused
  #define BANKENP4 0		// unused
  #define MAXPROC 3500		// depends on amount of memory
  #define USE_TMPFN			// use a temporary filename
  #define TH_CONTROL_OVERRIDE
#endif

#if defined SLSP2_2M_CAMERA							// 2 module high - not slave
  #define NMOD_BANK 1								// 487 pix wide
  #define NBANK 2									// 407 pix high
  #define NBANK_DCB 1								// banks on 1 dcb
  #define DETECTOR "PILATUS 200K"
  #define NDCB 2			// number of dcb's == number of gigastar's
  #define BANKENP1 0		// not used in single module
  #define BANKENP2 0		// unused
  #define BANKENP3 0		// unused
  #define BANKENP4 0		// unused
  #define MAXPROC 2000		// depends on amount of memory
  #define USE_TMPFN			// use a temporary filename
#endif

#if defined SLSP2_2MW_CAMERA						// 2 module wide, BCB
  #define NMOD_BANK 2								// 981 pix wide
  #define NBANK 1									// 195 pix high
  #define NBANK_DCB 1								// banks on 1 dcb
  #define DETECTOR "PILATUS 200KW"
  #define NDCB 1			// number of dcb's == number of gigastar's
// needed for 3-wide BCB with positions 2 & 3 filled
  #define MOD_ENB_PATTERN 6	// define this for a custom pattern
  #define BANKENP1 0x1		// 1 bank
  #define BANKENP2 0		// unused
  #define BANKENP3 0		// unused
  #define BANKENP4 0		// unused
  #define MAXPROC 2000		// depends on amount of memory
  #define USE_TMPFN			// use a temporary filename
#endif

#if defined SLSP2_2MS_CAMERA						// 2 module high, slave, 2 DCBs
  #define NMOD_BANK 1								// 487 pix wide
  #define NBANK 2									// 407 pix high
  #define NBANK_DCB 1								// banks on 1 dcb
  #define DETECTOR "PILATUS 200KS"
  #define NDCB 2			// number of dcb's == number of gigastar's
  #define BANKENP1 0		// not used in single module
  #define BANKENP2 0		// unused
  #define BANKENP3 0		// unused
  #define BANKENP4 0		// unused
  #define MAXPROC 2000		// depends on amount of memory
  #define USE_TMPFN			// use a temporary filename
// omit this definition for a real 2 module detector !!!!
  #define SLAVE_COMPUTER 1
#endif

#if defined SLSP2_2MWS_CAMERA						// 2 module wide, slave, 2 DCBs
  #define NMOD_BANK 2								// 981 pix wide
  #define NBANK 1									// 195 pix high
  #define NBANK_DCB 1								// banks on 1 dcb
  #define WIDE_FAST
  #define DETECTOR "PILATUS 200KWS"
  #define NDCB 2			// number of dcb's == number of gigastar's
  #define BANKENP1 0		// not used in single module
  #define BANKENP2 0		// unused
  #define BANKENP3 0		// unused
  #define BANKENP4 0		// unused
  #define MAXPROC 2000		// depends on amount of memory
  #define USE_TMPFN			// use a temporary filename
// omit this definition for a real 2 module detector !!!!
  #define SLAVE_COMPUTER 1
#endif

#if defined SLSP2_3M2_CAMERA						// 3 modules, high form, 3 DCBs, 2 computers
  #define NMOD_BANK 1								// 487 pix wide
  #define NBANK 3									// 619 pix high
  #define NBANK_DCB 1								// banks on 1 dcb
  #define DETECTOR "PILATUS 300K-2"
  #define NDCB 1			// number of dcb's == number of gigstar's this computer
  #define DET_NDCB 3		// total number of DCBs in detector
  #define BANKENP1 0		// not used in single module
  #define BANKENP2 0		// unused
  #define BANKENP3 0		// unused
  #define BANKENP4 0		// unused
  #define MAXPROC 2000		// depends on amount of memory
  #define MASTER_COMPUTER	// master of a multi-computer camera
  #define N_COMPUTERS 2
  #define USE_TMPFN			// use a temporary filename
#endif

#if defined SLSP2_3M1_CAMERA						// 3 modules, high form, 3 DCBs, 1 computer
  #define NMOD_BANK 1								// 487 pix wide
  #define NBANK 3									// 619 pix high
  #define NBANK_DCB 1								// banks on 1 dcb
  #define DETECTOR "PILATUS 300K-1"
  #define NDCB 3			// number of dcb's == number of gigstar's this computer
  #define DET_NDCB 3		// total number of DCBs in detector
  #define BANKENP1 0		// not used in single module
  #define BANKENP2 0		// unused
  #define BANKENP3 0		// unused
  #define BANKENP4 0		// unused
  #define MAXPROC 2000		// depends on amount of memory
  #define USE_TMPFN			// use a temporary filename
#endif

#if defined SLSP2_3MW_CAMERA						// 3 modules, wide form, 1 DCB
  #define NMOD_BANK 3								// 1475 pix wide
  #define NBANK 1									// 195 pix high
  #define NBANK_DCB 1								// banks on 1 dcb
  #define DETECTOR "PILATUS 300KW"
  #define NDCB 1			// number of dcb's == number of gigstar's
  #define BANKENP1 0x1		// 1 bank
  #define BANKENP2 0		// unused
  #define BANKENP3 0		// unused
  #define BANKENP4 0		// unused
  #define MAXPROC 2000		// depends on amount of memory
  #define USE_TMPFN			// use a temporary filename
#endif

#if defined SLSP2_3MWF2_CAMERA						// 3 modules, wide form, 3 DCBs, 2 computers
  #define NMOD_BANK 3								// 1475 pix wide
  #define NBANK 1									// 195 pix high
  #define NBANK_DCB 1								// banks on 1 dcb
  #define WIDE_FAST
  #define DETECTOR "PILATUS 300KWF-2"
  #define NDCB 1			// number of dcb's == number of gigstar's this computer
  #define DET_NDCB 3		// total number of DCBs in detector
  #define BANKENP1 0		// not used in single module
  #define BANKENP2 0		// unused
  #define BANKENP3 0		// unused
  #define BANKENP4 0		// unused
  #define MAXPROC 2000		// depends on amount of memory
  #define MASTER_COMPUTER	// master of a multi-computer camera
  #define N_COMPUTERS 2
  #define USE_TMPFN			// use a temporary filename
#endif

#if defined SLSP2_3MWF1_CAMERA						// 3 modules, wide form, 3 DCBs, 1 computer
  #define NMOD_BANK 3								// 1475 pix wide
  #define NBANK 1									// 195 pix high
  #define NBANK_DCB 1								// banks on 1 dcb
  #define WIDE_FAST
  #define DETECTOR "PILATUS 300KWF-1"
  #define NDCB 3			// number of dcb's == number of gigstar's this computer
  #define DET_NDCB 3		// total number of DCBs in detector
  #define BANKENP1 0		// not used in single module
  #define BANKENP2 0		// unused
  #define BANKENP3 0		// unused
  #define BANKENP4 0		// unused
  #define MAXPROC 2000		// depends on amount of memory
  #define USE_TMPFN			// use a temporary filename
#endif

#if defined SLSP2_10M_CAMERA						// 1M -- 5 banks x 2 modules
  #define NMOD_BANK 2								// 981 pix wide
  #define NBANK 5									// 1043 pix high
  #define NBANK_DCB 5								// banks on 1 dcb
  #define DETECTOR "PILATUS 1M"
  #define NDCB 1			// number of dcb's == number of gigstar's
  #define BANKENP1 0x1f		// 5 banks
  #define BANKENP2 0		// unused
  #define BANKENP3 0		// unused
  #define BANKENP4 0		// unused
  #define MAXPROC 500		// depends on amount of memory
  #define USE_TMPFN			// use a temporary filename
#endif

#if defined SLSP2DET_CAMERA_1BANK					// 1 banks, 5 mods == 500K
  #define NMOD_BANK 5								// 2463 pix wide
  #define NBANK 1									// 195 pix high
  #define NBANK_DCB 1								// banks on 1 dcb
  #define DETECTOR "PILATUS 500K"
  #define NDCB 1			// number of dcb's == number of gigstar's
  #define BANKENP1 0x1		// 1 bank
  #define BANKENP2 0		// unused
  #define BANKENP3 0		// unused
  #define BANKENP4 0		// unused
  #define MAXPROC 500		// depends on amount of memory
  #define USE_TMPFN			// use a temporary filename
#endif

#if defined SLSP2DET_CAMERA_2BANK					// 2 banks, 5 mods == 1MW
  #define NMOD_BANK 5								// 2463 pix wide
  #define NBANK 2									// 407 pix high
  #define NBANK_DCB 2								// banks on 1 dcb
  #define DETECTOR "2 BANK"
  #define NDCB 1			// number of dcb's == number of gigstar's
  #define BANKENP1 0x3		// 2 banks
  #define BANKENP2 0		// unused
  #define BANKENP3 0		// unused
  #define BANKENP4 0		// unused
  #define MAXPROC 500		// depends on amount of memory
  #define USE_TMPFN			// use a temporary filename
#endif

#if defined SLSP2DET_CAMERA_3BANK					// 3 banks, 5 mods
  #define NMOD_BANK 5								// 2463 pix wide
  #define NBANK 3									// 619 pix high
  #define NBANK_DCB 3								// banks on 1 dcb
  #define DETECTOR "3 BANK"
  #define NDCB 1			// number of dcb's == number of gigstar's
  #define BANKENP1 0x7		// 3 banks
  #define BANKENP2 0		// unused
  #define BANKENP3 0		// unused
  #define BANKENP4 0		// unused
  #define MAXPROC 500		// depends on amount of memory
  #define USE_TMPFN			// use a temporary filename
#endif

#if defined SLSP2DET_CAMERA_4BANK					// 4 banks, 5 mods
  #define NMOD_BANK 5								// 2463 pix wide
  #define NBANK 4									// 831 pix high
  #define NBANK_DCB 4								// banks on 1 dcb
  #define DETECTOR "4 BANK"
  #define NDCB 1			// number of dcb's == number of gigstar's
  #define BANKENP1 0xf		// 4 banks
  #define BANKENP2 0		// unused
  #define BANKENP3 0		// unused
  #define BANKENP4 0		// unused
  #define MAXPROC 500		// depends on amount of memory
  #define USE_TMPFN			// use a temporary filename
#endif

#if defined SLSP2DET_CAMERA_5BANK					// 5 banks, 5 mods
  #define NMOD_BANK 5								// 2463 pix wide
  #define NBANK 5									// 1043 pix high
  #define NBANK_DCB 5								// banks on 1 dcb
  #define DETECTOR "5 BANK"
  #define NDCB 1			// number of dcb's == number of gigstar's
  #define BANKENP1 0x1f		// 5 banks
  #define BANKENP2 0		// unused
  #define BANKENP3 0		// unused
  #define BANKENP4 0		// unused
  #define MAXPROC 500		// depends on amount of memory
  #define USE_TMPFN			// use a temporary filename
#endif

#if defined SLSP2DET_CAMERA_6BANK					// 6 banks, 5 mods
  #define NMOD_BANK 5								// 2463 pix wide
  #define NBANK 6									// 1255 pix high
  #define NBANK_DCB 6								// banks on 1 dcb
  #define DETECTOR "6 BANK"
  #define NDCB 1			// number of dcb's == number of gigstar's
  #define BANKENP1 0x3f		// 6 banks
  #define BANKENP2 0		// unused
  #define BANKENP3 0		// unused
  #define BANKENP4 0		// unused
  #define MAXPROC 500		// depends on amount of memory
  #define USE_TMPFN			// use a temporary filename
#endif

#if defined SLSP2DET_CAMERA_8BANK					// normal 2M
  #define NMOD_BANK 3								// 1475 pix wide
  #define NBANK 8									// 1679 pix high
  #define NBANK_DCB 4								// banks on 1 dcb
  #define DETECTOR "PILATUS 2M"
  #define NDCB 2			// number of dcb's == number of gigstar's
//  #define USE_BCB			// define for the 2 module readout mode
  #define BANKENP1 0xf		// 4 banks
  #define BANKENP2 0xf		// 4 banks
  #define BANKENP3 0		// unused
  #define BANKENP4 0		// unused
  #define MAXPROC 1600		// depends on amount of memory
  #define USE_TMPFN			// use a temporary filename
#endif

#if defined SLSP2DET_CAMERA_8BANK_4B					// 2M - 4 central banks
  #define NMOD_BANK 3									// 1475 pix wide
  #define NBANK 4										// 831 pix high
  #define NBANK_DCB 2									// banks on 1 dcb
  #define DETECTOR "PILATUS 2M_4B"
  #define NDCB 2			// number of dcb's == number of gigstar's
  #define BANKENP1 0xc		// 1 bank - nr. 4
  #define BANKENP2 0x3		// 1 bank - nr. 5
  #define BANKENP3 0		// unused
  #define BANKENP4 0		// unused
  #define MAXPROC 1600		// depends on amount of memory
  #define USE_TMPFN			// use a temporary filename
#endif

#if defined SLSP2DET_CAMERA_8BANK_2B					// 2M - 2 central banks
  #define NMOD_BANK 3									// 1475 pix wide
  #define NBANK 2										// 407 pix high
  #define NBANK_DCB 1									// banks on 1 dcb
  #define DETECTOR "PILATUS 2M_2B"
  #define NDCB 2			// number of dcb's == number of gigstar's
  #define BANKENP1 0x8		// 1 bank - nr. 4
  #define BANKENP2 0x1		// 1 bank - nr. 5
  #define BANKENP3 0		// unused
  #define BANKENP4 0		// unused
  #define MAXPROC 1600		// depends on amount of memory
  #define USE_TMPFN			// use a temporary filename
#endif

#if defined SLSP2DET_CAMERA_8BANK_8M					// 2M - 4banks x 2 mod
  #define NMOD_BANK 2									// 981 pix wide
  #define NBANK 4										// 831 pix high
  #define NBANK_DCB 2									// banks on 1 dcb
  #define DETECTOR "PILATUS 2M_8M"
  #define NDCB 2			// number of dcb's == number of gigstar's
  #define BANKENP1 0xc		// 1 bank - nr. 4
  #define BANKENP2 0x3		// 1 bank - nr. 5
  #define BANKENP3 0		// unused
  #define BANKENP4 0		// unused
  #define MAXPROC 1600		// depends on amount of memory
  #define USE_TMPFN			// use a temporary filename
#endif

#if defined SLSP2DET_CAMERA_8BANK_2M					// 2M - 2 central modules
  #define NMOD_BANK 1									// 487 pix wide
  #define NBANK 2										// 407 pix high
  #define NBANK_DCB 1									// banks on 1 dcb
  #define USE_BCB			// NMOD_BANK==1 normally implies no BCB
  #define DETECTOR "PILATUS 2M_2M"
  #define NDCB 2			// number of dcb's == number of gigstar's
  #define MOD_ENB_PATTERN 2	// define this for a custom pattern
  #define BANKENP1 0x8		// bank 4
  #define BANKENP2 0x1		// bank 5
  #define BANKENP3 0		// unused
  #define BANKENP4 0		// unused
  #define MAXPROC 3000		// depends on amount of memory
  #define USE_TMPFN			// use a temporary filename
#endif

#if defined SLSP2DET_CAMERA_12BANK					// normal 6M
  #define NMOD_BANK 5								// 2463 pix wide
  #define NBANK 12									// 2527 pix high
  #define NBANK_DCB 6								// banks on 1 dcb
  #define DETECTOR "PILATUS 6M"
  #define NDCB 2			// number of dcb's == number of gigstar's
  #define BANKENP1 0x3f		// 6 banks, dcb #1 (0011 1111)
//  #define BANKENP2 0x3f00	// 6 banks, dcb #2 (0011 1111 0000 0000)
  #define BANKENP2 0x3f		// 6 banks, dcb #2 (0011 1111) as if independent
  #define BANKENP3 0		// unused
  #define BANKENP4 0		// unused
  #define MAXPROC 1200		// depends on amount of memory
  #define USE_TMPFN			// use a temporary filename
  #define N_TH_SENSORS 3	// minimum number of temp/himidity sensors
#endif

#if defined SLSP2PS_CAMERA
  #define NMOD_BANK 1								// 487 pix wide
  #define NBANK 1									// pix high
  #define NBANK_DCB 1								// banks on 1 dcb
  #define DETECTOR "PS"
  #define NDCB 1			// number of dcb's == number of gigstar's
  #define BANKENP1 0		// not used in single module
  #define BANKENP2 0		// unused
  #define BANKENP3 0		// unused
  #define BANKENP4 0		// unused
  #define MAXPROC 500		// depends on amount of memory
#endif

#ifndef DETECTOR
  #error - ##### No camera defined #####
#endif

#if NMOD_BANK*NBANK>1 && ((NMOD_BANK>1 && !defined WIDE_FAST) || defined USE_BCB)
  #define ENABLE_BCBS 1
#else
  #define ENABLE_BCBS 0
#endif

#ifndef DET_NDCB
  #define DET_NDCB NDCB		// toal number of DCBs in detector
#endif

#ifdef MOD_ENB_PATTERN
 #if MOD_ENB_PATTERN>=0x10
  #define N_PATTERN_BITS 5
 #elif MOD_ENB_PATTERN>=0x8
  #define N_PATTERN_BITS 4
 #elif MOD_ENB_PATTERN>=0x4
  #define N_PATTERN_BITS 3
 #else
  #define N_PATTERN_BITS 2
 #endif
#endif


#define NBITS 20
#define NSTATES (1<<NBITS)	// 2**NBITS
#define fbBIT 2				// feed-back bit to use in addition to last bit

// 32-bit images
typedef unsigned int TYPEOF_PIX;

#define DATA_TRUE_LEVEL 1		// accounts for data inversion

#define MAX_TRIM_VALUE 0x3f

#define MAX_MOD_ADD 7		// selects all modules
#define MAX_BANK_ADD 15		// selects all banks

#define NROW_CHIP 97
#define NCOL_CHIP 60
#define NCHIP 16

#define NCOL_BTW_CHIP 1
#define NROW_BTW_CHIP 1

#define NROW_MOD (2*NROW_CHIP+NCOL_BTW_CHIP)		// 195 for p2 modules
#define NCOL_MOD (8*(NCOL_CHIP+NROW_BTW_CHIP)-1)	// 487 for p2 modules

#define MOD_SIZE (NCOL_MOD * NROW_MOD)

/*
** PILATUS II - 1 banks:  2463 x 195
**              2 banks:  2463 x 407
**              3 banks:  2463 x 619
**              4 banks:  2463 x 831
**              5 banks:  2463 x 1043
**				6 banks:  2463 x 1255
**				8 banks:  1475 x 1679		2M
**              12 banks: 2463 x 2527		6M
**              3 banks:  487  x 619		300K
*/

#define NCOL_BTW_MOD 7
#define NROW_BANK NROW_MOD
#define NCOL_BANK (NCOL_MOD*NMOD_BANK+(NMOD_BANK-1)*NCOL_BTW_MOD)

#define NROW_BTW_BANK 17
#define NROW_DET (NROW_BANK*NBANK+(NBANK-1)*NROW_BTW_BANK)
#define NCOL_DET NCOL_BANK

#define IMAGE_NROW NROW_DET
#define IMAGE_NCOL NCOL_DET

#define IMAGE_SIZE (IMAGE_NROW*IMAGE_NCOL)
#define IMAGE_NBYTES (IMAGE_SIZE*(int)sizeof(TYPEOF_PIX))

#define ACTIVE_PIX (NBANK*NMOD_BANK*NCHIP*NROW_CHIP*NCOL_CHIP)

//#define DAC_BITS_12
#define DAC_BITS_10


/*
** If count rate correction is enabled by tau > 0.0, limit the maximum
** corrected rate times tau product to the parameter below.  Above this 
** value return a cutoff value, which should be fairly obvious in the data.
**
** E.g., if tau = 126 ns, the corrected count rate becomes unreliable
** above 4.0e6 counts/s, giving tau * rate = 0.5040.  If the amplifier
** is set to be slower, tau will increase and the maximum useful 
** corrected rate will decrease.
**
** For further discussion, see xor_tab.c in util/
*/
#define MAX_TAU_RATE_PRODUCT 0.5


/*
** The following are used to construct the reduced dynamic range lookup
**
** This turns out not to work.  The parallel reload of the baseword
** requires ~1 ms settling time, which negates the expected gain in speed
*/
#define BASEWORD_4BIT 0xfffff		// should become 0x1eb20
#define BASEWORD_8BIT 0xfffff		// should become 0xe684


/*
** The following control the bad (unreliable) pixel list
** If the size is too large, a performance penalty results
*/
#define MAX_BAD_PIXELS 9000
#define BAD_PIX_FLAG -2


/*
** Turn on the all-integer flat-field correction method
** Comment out to turn off
*/
#define INTEGER_FLAT_FIELD_METHOD
#define INTEGER_FF_SHIFT 10


/*
** If defined, add the DECTRIS logo to each module at a depth of 1 count
*/
//#define ENABLE_DECTRIS_LOGO



/* ------------------------------------------ */

// global variables

// in xor_tab.c
extern int data_true_level;
extern unsigned int xor_table[];
extern double tau;
extern int count_cutoff;

// in cutils.c
struct det_pixel {
				int pix;	// pixel number
				int ix;		// image coord.
				int iy;
				int bn;		// bank number
				int bx;		// bank coord.
				int by;
				int mn;		// module number
				int mx;		// module coord.
				int my;
				int cn;		// chip no.
				int cx;		// chip coord.
				int cy;
				};

extern struct det_pixel *px;
extern unsigned int selectors[];
extern int img_mode;
extern int n_images;
extern TYPEOF_PIX theImage[];
extern int gap_fill_byte;
extern int *bad_pix_list;
extern char bad_pix_file_name[];
extern int n_bad_pix;
extern void *flat_field_data;
extern char flat_field_file_name[];
extern char trim_directory[];
extern int nfrmpim;
extern double set_energy_setting;

#ifdef ENABLE_DECTRIS_LOGO
extern TYPEOF_PIX dectris_logo[];
#endif


// in interface.c
extern int external_enable_mode;
extern int external_trigger_mode;
extern int external_multi_trigger_mode;
extern int kill_mutiple_images;
extern int acknowledge_interval;
extern double trigger_offset[];





/* ------------------------------------------ */
// function prototypes
// in settrims.c
void setTrims(void);
void init_trim_array(int);


// in cutils.c
void convert_image(char *);
void convert_image_add(char *);
void gap_fill_image(void);
void truncate_image(unsigned int *, int);
struct det_pixel *pix_add(int, int, int, int, int, int, int, int, int, int, int, int);
void moduleFilename(char *, char *, char *, int, int);
void log_gpib_readings(FILE *);
void initialize_theImage(int);
int load_bad_pixel_list(char *);
int load_flat_field_correction_file(char *);
int setup_file_series(char *);
char *next_file_in_series(void);

// in xor_tab.c
int xor_tab(void);
int xor_tab_initialize(int);


#ifdef USE_GPIB
void gpibsend_vo(char *);
void gpibsend_os(char *);
void gpibread_vo(void);
void gpibread_os(char *);
#endif

#endif	//DETSYS_H 
