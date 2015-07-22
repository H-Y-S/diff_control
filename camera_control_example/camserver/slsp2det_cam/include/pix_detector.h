// pix_detector.h - definitions/declarations related to programming dacs

#ifndef PIX_DETECTOR_H
#define PIX_DETECTOR_H

#include "detsys.h"
#include "camserver.h"		// LENFN

/*****************************************************************************\
**                                                                           **
**          Definitions                                                      **
**                                                                           **
\*****************************************************************************/

#define MAXADD 2700			// 45 addresses (signals) per module, 60 modules

#ifndef max
  #define max(a,b) (((a) > (b)) ? (a) : (b))
#endif
#ifndef min
  #define min(a,b) (((a) < (b)) ? (a) : (b))
#endif

#ifndef True
  #define True 1
  #define False 0
#endif


enum DET_GROUP	{DAC1 = 1,
				DAC2,
				CHIPSEL,
				NOT_USED,
				UnknownG
				} ;

struct DET_ADD {unsigned short bank;		// standard 5 part detector hardware address
				unsigned short module;
				enum DET_GROUP group;		// DAC1, DAC2 or CHIPSEL
				unsigned short unit;		// U1 - U4
				unsigned short output;		// OUT1 - OUT17
				} ;




#if defined DAC_BITS_8
 #define MAX_DAC_OUTPUT 7			// outputs 0 thru 7
#elif defined DAC_BITS_10 || defined DAC_BITS_12
 #define MAX_DAC_OUTPUT 3			// outputs 0 thru 3
#endif

#define MAX_DAC_UNIT 4				// 4 dacs on 1 bus
#define MAX_CHSEL_UNIT 1			// only 1 chsel chip
#define MAX_CHSEL_OUTPUT 15			// this is 17 in octal

struct SETTINGS	{struct DET_ADD hadd;	// 5-part hardware address
				unsigned short binv;	// binary value set by program
				float vset;				// setting requested by caller
				char name[15];			// signal name in text
				struct SETTINGS *linkP;	// pointer to next member of list
				} ;

enum PATTERN_SIGNAL {				// signals for i2c buses
		DAC1_SDA = 0,
		DAC2_SDA,
		CHSEL_SDA
		} ;
#define NpSig 1+(int)CHSEL_SDA		// make sure to use last in list here


// All signal names we have used.  Not all are used in a given application,
// but all must be defined to avoid error messages.
enum CHIP_SIGNAL {
		VRF = 0,
		VRFS,
		VCCA,
		VTRM,
		VDEL,
		VCAL,
		VADJ,
		VRES,
		VCMP,
		VCMP0, VCMP1,  VCMP2,  VCMP3,  VCMP4,  VCMP5,  VCMP6,  VCMP7, VCMP8,
		VCMP9, VCMP10, VCMP11, VCMP12, VCMP13, VCMP14, VCMP15,
		CHSEL,
		CHSEL0, CHSEL1,  CHSEL2,  CHSEL3,  CHSEL4,  CHSEL5,  CHSEL6,  CHSEL7,
		CHSEL8, CHSEL9, CHSEL10, CHSEL11, CHSEL12, CHSEL13, CHSEL14, CHSEL15
		} ;
#define NcSig 1+(int)CHSEL15	// make sure to use last in list here
#define NcSig_NS 1+(int)VCMP15	// no selects - for detector_map

enum SIGNAL_KIND
		{NotKnown = 0,			// 'Unknown' causes conflict
		TypeVoltage,
		TypeLevel,
		TypePattern
		} ;

struct PATTERN_SIGNAL_NAMES
		{enum PATTERN_SIGNAL sig;
		 char *name;
		} ;

struct CHIP_SIGNAL_NAMES
		{enum CHIP_SIGNAL sig;
		 char *name;
		 } ;

struct DETECTOR_INFO
		{struct DET_ADD hadd;
		enum SIGNAL_KIND kind;
		float lowL;
		float hiL;
		float oSet;
		float slope;
		} ;

struct TRIMBIN			// data for 1 module
		{int bank;
		int mod;
		float vtrm;
		float vrf;
		float vrfs;
		float vcal;
		float vdel;
		float vadj;
		float vcmp[NCHIP];
		float vcca;
		char tb[NCHIP][NROW_CHIP][NCOL_CHIP];
		} ;


struct DETTRIM			// data for a detector
		{char detector_name[20];
		int npix_x, npix_y;
		char flat_field_loE[LENFN], flat_field_hiE[LENFN];
		char bad_pix_loE[LENFN], bad_pix_hiE[LENFN];
		struct TRIMBIN module_data[NBANK][NMOD_BANK];
		} ;


/*****************************************************************************\
**                                                                           **
**        Protoypes of functions                                             **
**                                                                           **
\*****************************************************************************/
// --- in signals.c
int read_detector_info(char *);
int read_detector_map(char *);
int read_detector_offsets(char *);
int c_signal(char *, int *);
void set_signal(char *, int);
float module_offset(int, int, enum CHIP_SIGNAL);

// --- in progi2c.c
int set_dac(char *, float);
int set_dac_hex(char *, int);
int set_chsel(char *, unsigned short);
int program_voltage(struct SETTINGS *, unsigned short);
int program_chsel(struct SETTINGS *, unsigned short);
enum PATTERN_SIGNAL pattern_signal(char *);
enum CHIP_SIGNAL parse_address(char *, struct SETTINGS*);
enum PATTERN_SIGNAL p_signal(char *);
int storeSetting(struct SETTINGS *);
struct SETTINGS *getSetting(char *);
int saveSettings(FILE *);


/*****************************************************************************\
**                                                                           **
**         Global variables                                                  **
**                                                                           **
\*****************************************************************************/

extern struct PATTERN_SIGNAL_NAMES pSig[];
extern struct CHIP_SIGNAL_NAMES cSig[];
extern int bank_add_lo;
extern int bank_add_hi;
extern int module_add_lo;
extern int module_add_hi;
extern struct DETECTOR_INFO hDef[];
extern unsigned short dac_unit_info[];
extern unsigned short dac_out_info[];
extern unsigned short chsel_unit_info[];
extern unsigned short chsel_out_info[];

#endif	// PIX_DETECTOR_H
