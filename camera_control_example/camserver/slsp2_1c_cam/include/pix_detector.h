// pix_detector.h

#ifndef PIX_DETECTOR_H
#define PIX_DETECTOR_H

#include "detsys.h"

/*****************************************************************************\
**                                                                           **
**          Definitions                                                      **
**                                                                           **
\*****************************************************************************/

#define MAXADD 4800			// 80 addresses per module, 60 modules
#define PAT_LNG 0x800		// pattern length - cf. I2C_PATTERN_BASE

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

// functions defined in xor_tab.c
int xor_tab(void);
int xor_tab_initialize(int);
extern int fifo_data_true_level;

extern unsigned int xor_table[];

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
 #define MAX_DAC_OUTPUT 7			// outputs 0 thru 3
#elif defined DAC_BITS_12
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

enum PATTERN_SIGNAL {			// bit positions in pattern generator
		DAC1_SDA = 0,
		DAC2_SDA,
		CHSEL_SDA,
		SCL,
		MOD_ADD_0,
		MOD_ADD_1,
		MOD_ADD_2,
		BANK_ADD_0,
		BANK_ADD_1,
		BANK_ADD_2,
		BANK_ADD_3
		} ;
#define NpSig 1+(int)BANK_ADD_3		// make sure to use last in list here


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

struct PGEN_INFO
		{int bit_no;
		 int logic;
		 int quiescent;
		 } ;

struct DETECTOR_INFO
		{struct DET_ADD hadd;
		enum SIGNAL_KIND kind;
		float lowL;
		float hiL;
		float oSet;
		float slope;
		} ;


/*****************************************************************************\
**                                                                           **
**        Protoypes of functions                                             **
**                                                                           **
\*****************************************************************************/
// --- in signals.c
int read_detector_info(char *);
int c_signal(char *, int *);
void set_signal(char *, int);

// --- in progi2c.c
int set_dac(char *, float);
int set_dac_hex(char *, int);
int set_chsel(char *, unsigned short);
int program_voltage(struct SETTINGS *, unsigned short);
int program_chsel(struct SETTINGS *, unsigned short);
void start_signal(unsigned short, unsigned short, unsigned short);
void program_byte(unsigned short, unsigned short, unsigned short, unsigned short);
void stop_signal(unsigned short, unsigned short, unsigned short);
enum PATTERN_SIGNAL pattern_signal(char *);
enum CHIP_SIGNAL parse_address(char *, struct SETTINGS*);
int set_bank_mod(struct SETTINGS*);
enum PATTERN_SIGNAL p_signal(char *);
struct SETTINGS *getSetting(char *);
int saveSettings(FILE *);


/*****************************************************************************\
**                                                                           **
**         Global variables                                                  **
**                                                                           **
\*****************************************************************************/

extern struct PATTERN_SIGNAL_NAMES pSig[];
extern struct CHIP_SIGNAL_NAMES cSig[];
extern unsigned short thePattern[];
extern unsigned short *patternP;
extern unsigned short baseWord;
extern struct SETTINGS settings[];
extern struct SETTINGS *setP;
extern struct SETTINGS *setListHP;
extern int bank_add_lo;
extern int bank_add_hi;
extern int module_add_lo;
extern int module_add_hi;
extern struct PGEN_INFO pGen[];
extern struct DETECTOR_INFO hDef[];
extern unsigned short dac_unit_info[];
extern unsigned short dac_out_info[];
extern unsigned short chsel_unit_info[];
extern unsigned short chsel_out_info[];
extern int nStatesClk;
extern int nStatesDataSetup;
extern int nStatesDataHold;
extern float clock_frequency;

#endif	// PIX_DETECTOR_H
