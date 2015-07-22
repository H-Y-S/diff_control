/* progi2c.c - pattern composer to program I2C components

This code is specialized for the I2C protocol as defined by, e.g., Philips.

At a medium level in a system, we perhaps would like to invoke:

			set_voltage("B1_M3_VSF-", 0.85);

where the string designates Bank 1, Module 3, voltage VSF-.  We want to set it
to 0.85 volts.  Or, we might want to invoke

             set_select("B3_M2_CHSEL8", 1);

where the signal is CHSEL8, a Boolean value.

The steps in processing, e.g. for the voltage, are:

	(1) Resolve bank and module information into a bit pattern.
	(2) Invoke:

			set_mod_voltage(bank_mod_add, "VSF-", 0.85);

where bank_mod_add is the bank & module information from (1).  This routine
performs the following steps:

	(3) Look up properties of VSF- in "hardware.def" and retrieve the hardware
definition, the lower and upper limits of the setting, and the slope and offset.
	(4) Verify that the requested value is in range; if not return an error.
	(5) Calculate the binary value to set.
	(6) Record the setttings in the master table of settings
	(7) Invoke:

			program_voltage(bank_mod_add, "DAC2_U3_OUT3", 122)

where DAC2_U3_OUT3 is the hardware address of VSF-, and "122" is the decimal 
value to program.  This routine performs the following steps:

	(8) Set up bit pattern for DAC2 SCL, with asserted level
	(9) Set up bit pattern for DAC2 SDA, with asserted level
	(10) Call subroutines to make up the bit pattern
	(11) Write pattern to the pattern generator.

*/


#include <stdio.h>
#include <math.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include "ppg_util.h"
#include "pix_detector.h"
#include "debug.h"


/*****************************************************************************\
**                                                                           **
**        prototypes of internal functions                                   **
**                                                                           **
\*****************************************************************************/
static void expandDataSetup(void);
static void expandDataHold(void);
static void expandClk(void);
static int storeSetting(struct SETTINGS *);
static int storeSetting_helper(struct SETTINGS *);

/*****************************************************************************\
**                                                                           **
**          Global allocations                                               **
**                                                                           **
\*****************************************************************************/
unsigned short *patternP;
struct SETTINGS settings[MAXADD];
struct SETTINGS *setP = settings;
struct SETTINGS *setListHP = NULL;


/*****************************************************************************\
**                                                                           **
**          Internal allocations                                             **
**                                                                           **
\*****************************************************************************/



/*****************************************************************************\
**                                                                           **
**       set_dac                                                             **
**                                                                           **
\*****************************************************************************/

int set_dac(char *hdwadd, float value)
{
int err=0, sbinv;
struct SETTINGS tset;
enum CHIP_SIGNAL signal;
unsigned short binv;
char *p;

signal = parse_address(hdwadd, &tset);		// get bank, module and signal name
if (signal == (enum CHIP_SIGNAL)-1)
	return 1;

if (hDef[(int)signal].hadd.group == NOT_USED)
	{
	printf("Signal is not defined: %s\n", cSig[(int)signal].name);
	return 1;
	}

// Is this signal of the right type?  (Can't set a voltage on a selector, etc.)
if (hDef[(int)signal].kind != TypeVoltage)
	{
	printf("Can't set voltage on %s\n", cSig[(int)signal].name);
	return 1;
	}

// compare requested setting to hardware limits
if (value < hDef[(int)signal].lowL)
	{
	printf("Triming setting: %5.3f for %s to hardware limit: %5.3f\n",
			value, cSig[(int)signal].name, hDef[(int)signal].lowL);
	value = hDef[(int)signal].lowL;
	}
else if (value > hDef[(int)signal].hiL)
	{
	printf("Trimming setting: %5.3f for %s to hardware limit: %5.3f\n",
			value, cSig[(int)signal].name, hDef[(int)signal].hiL);
	value = hDef[(int)signal].hiL;
	}

// convert the requested setting to an integer for programming
#if defined DAC_BITS_8		// reference = 5.0 v
sbinv = (int)rint( (256.0 * (value-hDef[(int)signal].oSet) /
			(5.0 * hDef[(int)signal].slope)) );
binv = min(max(sbinv, 0), 255);
value = hDef[(int)signal].oSet + (binv * hDef[(int)signal].slope * 5.0 / 256.0);
#elif defined DAC_BITS_12		// reference = 2.5 v
sbinv = (int)rint( (4096.0 * (value-hDef[(int)signal].oSet) /
			(2.5 * hDef[(int)signal].slope)) );
binv = min(max(sbinv, 0), 4095);
value = hDef[(int)signal].oSet + (binv * hDef[(int)signal].slope * 2.5 / 4096.0);
#else
#error
#endif
if ( (p=strstr(hdwadd, "B15")) ) {*(p+1)='*'; *(p+2)='*';}
if ( (p=strstr(hdwadd, "M07")) ) {*(p+1)='*'; *(p+2)='*';}
printf("  Setting %s voltage to integer: %#2x = ~%5.3fV\n", hdwadd, binv, value);
tset.binv = binv;
tset.vset = value;

err = program_voltage(&tset, binv);
if (err)
	return err;

// store settings in the master table
err = storeSetting(&tset);

return err;
}


/*****************************************************************************\
**                                                                           **
**       set_dac_hex                                                         **
**                                                                           **
\*****************************************************************************/

int set_dac_hex(char *hdwadd, int bval)
{
int err=0;
struct SETTINGS tset;
enum CHIP_SIGNAL signal;
unsigned short binv;
float value;
char *p;

signal = parse_address(hdwadd, &tset);		// get bank, module and signal name
if (signal == (enum CHIP_SIGNAL)-1)
	return 1;

if (hDef[(int)signal].hadd.group == NOT_USED)
	{
	printf("Signal is not defined: %s\n", cSig[(int)signal].name);
	return 1;
	}

// Is this signal of the right type?  (Can't set a voltage on a selector, etc.)
if (hDef[(int)signal].kind != TypeVoltage)
	{
	printf("Can't set voltage on %s\n", cSig[(int)signal].name);
	return 1;
	}

#if defined DAC_BITS_8		// reference = 5.0 v
binv = min(max(bval, 0), 0xff);
value = hDef[(int)signal].oSet + (binv * hDef[(int)signal].slope * 5.0 / 256.0);
#elif defined DAC_BITS_12		// reference = 2.5 v
binv = min(max(bval, 0), 0xfff);
value = hDef[(int)signal].oSet + (binv * hDef[(int)signal].slope * 2.5 / 4096.0);
#endif
if ( (p=strstr(hdwadd, "B15")) ) {*(p+1)='*'; *(p+2)='*';}
if ( (p=strstr(hdwadd, "M07")) ) {*(p+1)='*'; *(p+2)='*';}
printf("  Setting %s voltage to integer: %#2x = ~%5.3fV\n", hdwadd, binv, value);
tset.binv = binv;
tset.vset = value;

err = program_voltage(&tset, binv);
if (err)
	return err;

// store settings in the master table
err = storeSetting(&tset);

return err;
}


/*****************************************************************************\
**                                                                           **
**       set_chsel                                                           **
**                                                                           **
\*****************************************************************************/

int set_chsel(char *hdwadd, unsigned short data)
{
int i, err = 0, flag=0, pat=0, map=0, chip;
struct SETTINGS tset, tset2, *tsP;
enum CHIP_SIGNAL signal;
char sigName[20], *p, hdwadd2[20];

strcpy(hdwadd2, hdwadd);
signal = parse_address(hdwadd, &tset);
if (signal == (enum CHIP_SIGNAL)-1)
	return 1;

if (hDef[(int)signal].hadd.group == NOT_USED)
	{
	printf("Signal is not defined: %s\n", cSig[(int)signal].name);
	return 1;
	}

// Is this signal of the right type?  (Can't set a voltage on a selector, etc.)
if (hDef[(int)signal].kind == TypeLevel)	// individual CHSEL
	{
	if ( (p=strstr(hdwadd2, "B15")) ) {*(p+1)='*'; *(p+2)='*';}
	if ( (p=strstr(hdwadd2, "M07")) ) {*(p+1)='*'; *(p+2)='*';}
	printf ("  Setting %s to %d\n", hdwadd2, data);
	// find the current settings - we must program 16 at a time
	strcpy(sigName, hdwadd);		// e.g. B11_M6_CHSEL3
	if ( (p=strstr(sigName, "B15")) ) {*(p+1)='0'; *(p+2)='1';}
	if ( (p=strstr(sigName, "M07")) ) {*(p+2)='1';}
	p = strstr(sigName, "CHSEL") + strlen("CHSEL");
	chip = atoi(p);				// pick up the chip number
	*p = '\0';
	for (i=0; i<16; i++)
		{
		sprintf(p, "%d", i);
		signal = parse_address(sigName, &tset2);
		if (tset2.hadd.group == NOT_USED)
			continue;
		if ( !(tsP=getSetting(sigName)) )
			continue;			// if undefined, assume 0
		if (tsP->binv)
			{
			pat |= (1<<tsP->hadd.output);	// hardware pattern
			map |= (1<<i);					// logical pattern
			}
		}
	pat &= ~(1 << tset.hadd.output);
	pat |= (data << tset.hadd.output);
	err = program_chsel(&tset, pat);
	map |= (data << chip);
	data = map;					// for recording
	if (err)
		return err;
	}
else if (hDef[(int)signal].kind == TypePattern)		// a bit pattern for CHSEL
	{
	// convert the given logical pattern to the hardware pattern
	strcpy(sigName, hdwadd);		// e.g. B11_M6_CHSEL3
	*(p = strstr(sigName, "CHSEL") + strlen("CHSEL")) = '\0';
	if (data==0 || data==0xffff)
		{
		pat = data;
		if (data == 0xffff)
			flag = 16;
		}
	else
		{
		for (i=0; i<16; i++)
			{
			if ( !(data & (1<<i)) )
				continue;
			sprintf(p, "%d", i);
			signal = parse_address(sigName, &tset);
			if (tset.hadd.group == NOT_USED)
				continue;
			pat |= (1<<tset.hadd.output);
			flag++;
			}
		*p = '\0';
		}
	signal = parse_address(sigName, &tset);		// get tset for whole pattrn
	err = program_chsel(&tset, pat);
	if (err)
		return err;
	if ( (p=strstr(hdwadd2, "B15")) ) {*(p+1)='*'; *(p+2)='*';}
	if ( (p=strstr(hdwadd2, "M07")) ) {*(p+1)='*'; *(p+2)='*';}
	printf ("  Setting %s PATTERN to 0x%x (hardware pattern: 0x%x)\n", hdwadd2, data, pat);
	}
else
	{
	printf("Can't set level on %s\n", cSig[(int)signal].name);
	return 1;
	}

// 16 chsels have been programmed.  Record the settings
selected_chip = -1;
strcpy(sigName, hdwadd);		// e.g. B11_M6_CHSEL3
p = strstr(sigName, "CHSEL") + strlen("CHSEL");		// point to integer
*p = '\0';			// just plain 'CHSEL' without number
signal = parse_address(sigName, &tset);
tset.binv = tset.vset = data;
err = storeSetting(&tset);			// store the pattern
if (err)
	return err;
for (i = 0; i < 16; i++)
	{
	sprintf(p, "%d", i);
	signal = parse_address(sigName, &tset);
	if (tset.hadd.group == NOT_USED)
		continue;		// either 0 or 16 should fail
	if (data & 0x1)
		{
		tset.binv = tset.vset = 1;
		if (selected_chip < 0)
			selected_chip = i;
		}
	else
		tset.binv = tset.vset = 0;
	err = storeSetting(&tset);		// store individual settings
	if (err)
		return err;
	data >>= 1;
	}
if (flag > 1)
	selected_chip = -1;		// flag for multiple selection, generally all

return err;
}


/*****************************************************************************\
**                                                                           **
**         parse_address                                                     **
**                                                                           **
\*****************************************************************************/

// parse the hardware address into its components
enum CHIP_SIGNAL parse_address(char *hdwadd, struct SETTINGS *tset)
{
int i;
char *pa, *sp, atom[20];
enum CHIP_SIGNAL signal;

for (i = 0, sp = hdwadd, pa = atom; *sp && *sp != '_'; )
	*pa++ = *sp++;			// isolate 1st field, should be Bxx
*pa = '\0';

switch (toupper(atom[0]))
	{
	case 'B':
		if ( bank_add_lo == -1 )	// NOT_USED
			break;
		sscanf(atom+1,"%d", (int *)&tset->hadd.bank);
		//check limits
		if ( (tset->hadd.bank < bank_add_lo) || (tset->hadd.bank > bank_add_hi) )
			{
			printf("Bank address not defined: %hu\n", tset->hadd.bank);
			return (enum CHIP_SIGNAL)-1;
			}
		DBG(5, "got bank = %d\n", tset->hadd.bank);
		break;
	default:
		break;
	}

for (i = 0, sp++, pa = atom; *sp && *sp != '_'; )
	*pa++ = *sp++;		// isolate 2nd field, should be Mxx
*pa = '\0';

switch (toupper(atom[0]))
	{
	case 'M':
		if ( module_add_lo == -1 )	// NOT_USED
			break;
		sscanf(atom+1, "%d", (int *)&tset->hadd.module);
		//check limits
		if ( (tset->hadd.module < module_add_lo) || (tset->hadd.module > module_add_hi) )
			{
			printf("Module address not defined: %hu\n", tset->hadd.module);
			return -1;
			}
		DBG(5, "got module = %d\n", tset->hadd.module);
		break;
	default:
		break;
	}

for (sp++, pa = atom; *sp; )
	*pa++ = *sp++;				// isolate 3rd field, probably a signal name
*pa = '\0';						// terminate the string
signal = (enum CHIP_SIGNAL)-1;	// flag for not found
for (i = 0; i < NcSig; i++)		// linear probe on list of ~95 items - could be made more efficient?
	if (strcmp(atom, cSig[i].name) == 0)
		{
		signal = cSig[i].sig;
		break;
		}
if (signal == (enum CHIP_SIGNAL)-1)
	{
	printf("Signal not recognized: %s\n", atom);
	return (enum CHIP_SIGNAL)-1;
	}
DBG(3, "got signal: %s\n", cSig[(int)signal].name);
//printf("Signal name: %s\n", hdwadd);
// copy hardware definitions
tset->hadd.group = hDef[(int)signal].hadd.group;
tset->hadd.unit = hDef[(int)signal].hadd.unit;
tset->hadd.output = hDef[(int)signal].hadd.output;
strcpy(tset->name, hdwadd);

return signal;
}


/*****************************************************************************\
**                                                                           **
**         program_voltage                                                   **
**                                                                           **
\*****************************************************************************/

int program_voltage(struct SETTINGS *set, unsigned short data)
{
int err=0;
unsigned short sda, scl;
int sdaBit, sclBit, sdaLogic, sclLogic;
enum PATTERN_SIGNAL sdaSig, sclSig;

if ( (err = set_bank_mod(set)) )
	return err;

// set up bit mask for appropriate SDA
if (set->hadd.group == DAC1)
	sdaSig = pattern_signal("DAC1_SDA");
else if (set->hadd.group == DAC2)
	sdaSig = pattern_signal("DAC2_SDA");
else
	{
	printf("Can't match hardware group %d\n", (int)set->hadd.group);
	return 1;
	}
sdaBit = pGen[(int)sdaSig].bit_no;
sdaLogic = pGen[(int)sdaSig].logic;
sda = 1 << sdaBit;		// the sda bit mask

// set up bit mask for SCL
sclSig = pattern_signal("SCL");

sclBit = pGen[(int)sclSig].bit_no;
sclLogic = pGen[(int)sclSig].logic;
scl = 1 << sclBit;		// the scl bit mask

if (sclLogic != sdaLogic)	// I can't imagine these being different
	{
	printf("Sda and scl logic conventions don't match\n");
	return 1;
	}

// generate the pattern
#if defined DAC_BITS_8
start_signal(sda, scl, sdaLogic);
program_byte(sda, scl, sdaLogic, dac_unit_info[set->hadd.unit]);
program_byte(sda, scl, sdaLogic, dac_out_info[set->hadd.output]);
program_byte(sda, scl, sdaLogic, data);
stop_signal(sda, scl, sdaLogic);
#elif defined DAC_BITS_12
// first the power up signal
start_signal(sda, scl, sdaLogic);
program_byte(sda, scl, sdaLogic, dac_unit_info[set->hadd.unit]);
program_byte(sda, scl, sdaLogic, 0xf0);
program_byte(sda, scl, sdaLogic, 0x3c);
stop_signal(sda, scl, sdaLogic);
// program the voltage
start_signal(sda, scl, sdaLogic);
program_byte(sda, scl, sdaLogic, dac_unit_info[set->hadd.unit]);
program_byte(sda, scl, sdaLogic, dac_out_info[set->hadd.output] | (data>>8));
program_byte(sda, scl, sdaLogic, data&0xff);
stop_signal(sda, scl, sdaLogic);
#endif


return err;
}


/*****************************************************************************\
**                                                                           **
**         program_chsel                                                     **
**                                                                           **
\*****************************************************************************/

// 'data' is a bit pattern for all CHSELs in this bank and module
int program_chsel(struct SETTINGS *set, unsigned short data)
{
int err = 0, outword, i;
unsigned short sda, scl;
int sdaBit, sclBit, sdaLogic, sclLogic;
enum PATTERN_SIGNAL sdaSig, sclSig;

if ( (err = set_bank_mod(set)) )
	return err;

// set up bit mask for appropriate SDA
if (set->hadd.group == CHIPSEL)
	sdaSig = pattern_signal("CHSEL_SDA");
else
	{
	printf("Can't match hardware group %d\n", (int)set->hadd.group);
	return 1;
	}
sdaBit = pGen[(int)sdaSig].bit_no;
sdaLogic = pGen[(int)sdaSig].logic;
sda = 1 << sdaBit;		// the sda bit mask

// set up bit mask for appropriate SCL
if (set->hadd.group == CHIPSEL)
	sclSig = pattern_signal("SCL");
else
	{
	printf("Can't match hardware group %d\n", (int)set->hadd.group);
	return 1;
	}
sclBit = pGen[(int)sclSig].bit_no;
sclLogic = pGen[(int)sclSig].logic;
scl = 1 << sclBit;		// the scl bit mask

// change the 'data' bit pattern to output pattern
outword = 0;
for (i = 0; i<16; i++)
	if ( data & (1<<i) )
		outword |= chsel_out_info[i];

// generate the pattern
start_signal(sda, scl, sdaLogic);
program_byte(sda, scl, sdaLogic, chsel_unit_info[set->hadd.unit]);
program_byte(sda, scl, sdaLogic, outword>>8);
program_byte(sda, scl, sdaLogic, outword);
stop_signal(sda, scl, sdaLogic);

return err;
}


/*****************************************************************************\
**                                                                           **
**       set_bank_mod                                                        **
**                                                                           **
**       set bank and module addresses into baseWord                         **
**       then output baseWord to select bank and module                      **
**                                                                           **
\*****************************************************************************/

int set_bank_mod(struct SETTINGS *set)
{
int modBit0, modBit1, modBit2, 
			modLogic0, modLogic1, modLogic2, bankBit0, bankBit1, bankBit2, 
			bankBit3, bankLogic0, bankLogic1, bankLogic2, bankLogic3;
unsigned short word, mask;
enum PATTERN_SIGNAL modSig0, modSig1, modSig2,
			bankSig0, bankSig1, bankSig2, bankSig3;

// change bank address in base word
bankSig0 = pattern_signal("BANK_ADD_0");
bankBit0 = pGen[(int)bankSig0].bit_no;
bankLogic0 = pGen[(int)bankSig0].logic;

bankSig1 = pattern_signal("BANK_ADD_1");
bankBit1 = pGen[(int)bankSig1].bit_no;
bankLogic1 = pGen[(int)bankSig1].logic;

bankSig2 = pattern_signal("BANK_ADD_2");
bankBit2 = pGen[(int)bankSig2].bit_no;
bankLogic2 = pGen[(int)bankSig2].logic;

bankSig3 = pattern_signal("BANK_ADD_3");
bankBit3 = pGen[(int)bankSig3].bit_no;
bankLogic3 = pGen[(int)bankSig3].logic;

if (bankLogic0 != bankLogic1 || bankLogic0 != bankLogic2 || bankLogic0 != bankLogic3)
	{
	printf("Bank logic levels don't match\n");
	return 1;
	}

// we put the bank address bits in the right place even if they are not adjacent
word = 0;
word |= ((set->hadd.bank & 0x8) >> 3) << bankBit3;
word |= ((set->hadd.bank & 0x4) >> 2) << bankBit2;
word |= ((set->hadd.bank & 0x2) >> 1) << bankBit1;
word |= (set->hadd.bank & 0x1) << bankBit0;

// make a corresponding mask
mask = 0;
mask |= 1 << bankBit3;
mask |= 1 << bankBit2;
mask |= 1 << bankBit1;
mask |= 1 << bankBit0;

baseWord &= ~mask;	// zero the previous bits
if (bankLogic0 == 0)	// negative logic
	word = (~word) & mask;
baseWord |= word;	// put in the new bits

// change address module in base word
modSig0 = pattern_signal("MOD_ADD_0");
modBit0 = pGen[(int)modSig0].bit_no;
modLogic0 = pGen[(int)modSig0].logic;

modSig1 = pattern_signal("MOD_ADD_1");
modBit1 = pGen[(int)modSig1].bit_no;
modLogic1 = pGen[(int)modSig1].logic;

modSig2 = pattern_signal("MOD_ADD_2");
modBit2 = pGen[(int)modSig2].bit_no;
modLogic2 = pGen[(int)modSig2].logic;

if (modLogic0 != modLogic1 || modLogic0 != modLogic2)
	{
	printf("Module logic levels don't match\n");
	return 1;
	}

// we put the module address bits in the right place even if they are not adjacent
word = 0;
word |= ((set->hadd.module & 0x4) >> 2) << modBit2;
word |= ((set->hadd.module & 0x2) >> 1) << modBit1;
word |= (set->hadd.module & 0x1) << modBit0;

// make a corresponding mask
mask = 0;
mask |= 1 << modBit2;
mask |= 1 << modBit1;
mask |= 1 << modBit0;

baseWord &= ~mask;	// zero the previous bits
if (bankLogic0 == 0)	// negative logic
	word = (~word) & mask;
baseWord |= word;	// put in the new bits

*patternP++ = baseWord;		// select bank & module

return 0;
}


/*****************************************************************************\
**                                                                           **
**         pattern_signal                                                    **
**                                                                           **
\*****************************************************************************/

enum PATTERN_SIGNAL pattern_signal(char *name)
{
int i;

for(i = 0; i < NpSig; i++)
	if ( strcmp(name, pSig[i].name) == 0 )
		return pSig[i].sig;

printf ("Pattern signal not found %s\n", name);
return 0;
}


/*****************************************************************************\
**                                                                           **
**        Routines to maintain the parameter list                            **
**                                                                           **
\*****************************************************************************/

static int storeSetting(struct SETTINGS *set)
{
#ifdef SLSDET_CAMERA
int bank, mod, err;
struct SETTINGS tset;

if (set->hadd.bank == MAX_BANK_ADD && set->hadd.module == MAX_MOD_ADD)	// all modules
	{
	(void)memcpy(&tset, set,sizeof(struct SETTINGS));
	for (bank=1; bank<=NBANKS; bank++)
		{
		tset.hadd.bank = bank;
		*(tset.name+strlen("B")) = '0' + (bank/10);
		*(tset.name+strlen("Bx")) = '0' + (bank%10);
		for (mod=1; mod<=NMOD_BANK; mod++)
			{
			tset.hadd.module = mod;
			*(tset.name+strlen("Bxx_M0")) = '0' + (mod%10);
			if ( (err = storeSetting_helper(&tset)) )
				return err;
			}
		}
	return 0;
	}
else if (set->hadd.bank == MAX_BANK_ADD)		// 1 module in all banks
	{
	(void)memcpy(&tset, set, sizeof(struct SETTINGS));
	for (bank=1; bank<=NBANKS; bank++)
		{
		tset.hadd.bank = bank;
		*(tset.name+strlen("B")) = '0' + (bank/10);
		*(tset.name+strlen("Bx")) = '0' + (bank%10);
		if ( (err = storeSetting_helper(&tset)) )
			return err;
		}
	return 0;
	}
else if (set->hadd.module == MAX_MOD_ADD)			// all modules in 1 bank
	{
	(void)memcpy(&tset, set, sizeof(struct SETTINGS));
	for (mod=1; mod<=NMOD_BANK; mod++)
		{
		tset.hadd.module = mod;
		*(tset.name+strlen("Bxx_M0")) = '0' + (mod%10);
		if ( (err = storeSetting_helper(&tset)) )
			return err;
		}
	return 0;
	}
else
	return storeSetting_helper(set);	// indivdual setting
#else

return storeSetting_helper(set);

#endif
}


static int storeSetting_helper(struct SETTINGS *set)
{
struct SETTINGS *p=setListHP, *q, **pp=&setListHP;
int val;

// store settings in the master table
if (setP-settings > MAXADD-2)
	{
	printf("***Error - settings table is full\n");
	return -1;
	}

val = -1;
while (p && ((val = strcmp(p->name, set->name)) > 0) )
	{					// go thru the list looking for match or alphabetic location
	pp = &(p->linkP);	// pp is a pointer to the previous pointer
	p = p->linkP;
	}

if (val)		// store a new member
	{
	(void)memcpy(setP, set, sizeof(struct SETTINGS));	// store in first blank
	setP->linkP = p;	// new member points to lexically later entry, possibly null
	*pp = setP;			// set up-link pointer
	setP++;
	}
else			// update an old member
	{
	q = p->linkP;		// save link pointer from overwriting
	(void)memcpy(p, set, sizeof(struct SETTINGS));		// store new info
	p->linkP = q;		// restore link pointer
	}

return 0;
}


struct SETTINGS *getSetting(char *ptr)
{
struct SETTINGS *p=setListHP;
int val;
char *q;

// find a previously stored setting, return NULL if not found
if ( (q = strchr(ptr, ' ')) )
	*q = '\0';		// this changes the caller's copy - any problems?
val = -1;
while (p && ((val = strcmp(p->name, ptr)) > 0) )
	{					// go thru the list looking for match or alphabetic location
	p = p->linkP;
	}

return val ? NULL : p;
}


int saveSettings(FILE *ofp)
{
struct SETTINGS *p=setListHP;
char line[50];

if (p == NULL)
	{
	printf("Nothing to save - list is empty\n");
	return -1;
	}

while (p)
	{
	if (p->hadd.group == CHIPSEL)
		{
		if ( isdigit (*(strstr(p->name, "CHSEL") + strlen("CHSEL"))) )
			{
			p = p->linkP;
			continue;
			}
		sprintf(line, "prog %s 0x%x", p->name, p->binv);
		}
	else
		sprintf(line, "set %s %5.3f", p->name, p->vset);
	fprintf(ofp, "%s\n", line);
	p = p->linkP;
	}

return 0;
}

/*****************************************************************************\
**                                                                           **
**  start_signal, program_byte and stop_signal                               **
**                                                                           **
**   Here we generate the patterns.  The logic is set up for the I2C spec    **
**   from Philips.  In that spec, the queiscent state is SDA = SCL = High.   **
**   The asserted value for SCL is a 0 to 1 transition.  The asserted value  **
**   for data is is High.                                                    **
**                                                                           **
**   All of these values may be inverted if there is an inverter driving the **
**   line - this is called here "negative" logic.                            **
**                                                                           **
\*****************************************************************************/


void start_signal(unsigned short sda, unsigned short scl, unsigned short logic)
{

int LengthOfThisPattern = 2;
if (patternP - thePattern >= PAT_LNG - LengthOfThisPattern)
	{
	printf("Out of space for pattern\n");
	exit (1);
	}

// start = (1) sda low, then (2) scl low

if (logic == 1)		// positive logic
	{
	*patternP++ = baseWord &= ~sda; expandDataSetup();
	*patternP++ = baseWord &= ~scl; expandClk();
	}
else			// negative logic
	{
	*patternP++ = baseWord |= sda; expandDataSetup();
	*patternP++ = baseWord |= scl; expandClk();
	}
	
return;
}

/*
** Program 1 byte - "address", "command" or data - according to I2C protocl
*/
void program_byte(unsigned short sda, unsigned short scl,
				unsigned short logic, unsigned short data)
{
unsigned short selector = 0x80;

int LengthOfThisPattern = 27;
if (patternP - thePattern >= PAT_LNG - LengthOfThisPattern)
	{
	printf("Out of space for pattern\n");
	exit (1);
	}

if (logic == 1)		// positive logic
	{
	for ( ; selector; selector >>= 1)
		{
		if (selector & data)					// data =1
			*patternP++ = baseWord |= sda;		// data on (high)
		else									// data =0
			*patternP++ = baseWord &= ~sda;		// data off (low)
		expandDataSetup();
		*patternP++ = baseWord |= scl; expandClk();			// clock on (high)
		*patternP++ = baseWord &= ~scl; expandDataHold();	// clock off
		}
	// acknowledge sequence
	*patternP++ = baseWord &= ~sda; expandDataSetup();		// data off (low)
	*patternP++ = baseWord |= scl; expandClk();				// clock on (high)
	*patternP++ = baseWord &= ~scl; expandDataHold();		// clock off
	}
else			// negative logic
	{
	for ( ; selector; selector >>= 1)
		{
		if (selector & data)					// data =1
			*patternP++ = baseWord &= ~sda;		// data on (low)
		else									// data =0
			*patternP++ = baseWord |= sda;		// data off (high)
		expandDataSetup();
		*patternP++ = baseWord &= ~scl; expandClk();		// clock on (low)
		*patternP++ = baseWord |= scl; expandDataHold();	// clock off 
		}
	// acknowledge sequence
	*patternP++ = baseWord |= sda; expandDataSetup();		// data off (high)
	*patternP++ = baseWord &= ~scl; expandClk();			// clock on (low)
	*patternP++ = baseWord |= scl; expandDataHold();		// clock off
	}

return;
}


void stop_signal(unsigned short sda, unsigned short scl, unsigned short logic)
{

int LengthOfThisPattern = 2;
if (patternP - thePattern >= PAT_LNG - LengthOfThisPattern)
	{
	printf("Out of space for pattern\n");
	exit (1);
	}

// stop signal = (1) scl high, then (2) sda high
if (logic == 1)			// positive logic
	{
	*patternP++ = baseWord |= scl; expandClk();
	*patternP++ = baseWord |= sda; expandDataHold();
	}
else				// negative logic
	{
	*patternP++ = baseWord &= ~scl; expandClk();
	*patternP++ = baseWord &= ~sda; expandDataHold();
	}
	
return;
}


/*****************************************************************************\
**                                                                           **
**      functions to expand states to the required number of clocks          **
**                                                                           **
\*****************************************************************************/

static void expandDataSetup(void)
{
int i;

if (nStatesDataSetup == 1)
	return;			// nothing to do

for (i = 1; i < nStatesDataSetup; i++)
	*patternP++ = *(patternP-1);

return;
}


static void expandDataHold(void)
{
int i;

if (nStatesDataHold == 1)
	return;			// nothing to do

for (i = 1; i < nStatesDataHold; i++)
	*patternP++ = *(patternP-1);

return;
}


static void expandClk(void)
{
int i;

if (nStatesClk == 1)
	return;			// nothing to do

for (i = 1; i < nStatesClk; i++)
	*patternP++ = *(patternP-1);

return;
}



