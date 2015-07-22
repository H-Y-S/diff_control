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
#include "i2c_util.h"
#include "pix_detector.h"
#include "dcb.h"
#include "dcblib.h"
#include "debug.h"


/*****************************************************************************\
**                                                                           **
**        prototypes of internal functions                                   **
**                                                                           **
\*****************************************************************************/

static int storeSetting_helper(struct SETTINGS *);


/*****************************************************************************\
**                                                                           **
**          Global allocations                                               **
**                                                                           **
\*****************************************************************************/
static struct SETTINGS settings[MAXADD];
static struct SETTINGS *setP = settings;
static struct SETTINGS *setListHP = NULL;


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
#ifdef DEBUG
float vt = value;
#endif

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

// adjust value according to the module-specific offset map of this detector
if (dac_offset_comp_enable)
	{
	value -= module_offset(tset.hadd.bank, tset.hadd.module, signal);
	DBG(5, "set %s %.3f -- used offset to adjust V to %.3f\n", hdwadd, vt, value)
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
#elif defined DAC_BITS_10		// reference = 2.5 v
sbinv = (int)rint( (1024.0 * (value-hDef[(int)signal].oSet) /
			(2.5 * hDef[(int)signal].slope)) );
binv = min(max(sbinv, 0), 1023);
value = hDef[(int)signal].oSet + (binv * hDef[(int)signal].slope * 2.5 / 1024.0);
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
#ifdef MASTER_COMPUTER
 #ifdef WIDE_FAST
  if (tset.hadd.module>1 && tset.hadd.module!=MAX_MOD_ADD)
 #else
  if (tset.hadd.bank>NDCB && tset.hadd.bank!=MAX_BANK_ADD)
 #endif
	printf("    (not this computer)\n");
#endif
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

#if defined DAC_BITS_8			// reference = 5.0 v
binv = min(max(bval, 0), 0xff);
value = hDef[(int)signal].oSet + (binv * hDef[(int)signal].slope * 5.0 / 256.0);
#elif defined DAC_BITS_10		// reference = 2.5 v
binv = min(max(bval, 0), 0x3ff);
value = hDef[(int)signal].oSet + (binv * hDef[(int)signal].slope * 2.5 / 1024.0);
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
		tset->hadd.bank = 0;
		if ( *(atom+1) == '*' && *(atom+2) == '*' )
			tset->hadd.bank = 15;
		else
			sscanf(atom+1,"%hu", &tset->hadd.bank);
		//check limits - 15 selects all banks
		if ( 	(tset->hadd.bank != 15) &&
				((tset->hadd.bank < bank_add_lo) || 
				(tset->hadd.bank > bank_add_hi)) )
			{
			printf("Bank address not defined: %hu\n", tset->hadd.bank);
			return (enum CHIP_SIGNAL)-1;
			}
		DBG(7, "got bank = %d\n", tset->hadd.bank)
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
		tset->hadd.module = 0;
		if ( *(atom+1) == '*' && *(atom+2) == '*' )
			tset->hadd.module = 7;
		else
			sscanf(atom+1, "%hu", &tset->hadd.module);
		//check limits - 7 selects all modules
		if ( 	(tset->hadd.module != 7) &&
				((tset->hadd.module < module_add_lo) ||
				 (tset->hadd.module > module_add_hi)) )
			{
			printf("Module address not defined: %hu\n", tset->hadd.module);
			return -1;
			}
		DBG(7, "got module = %d\n", tset->hadd.module)
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
DBG(7, "got signal: %s\n", cSig[(int)signal].name)
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
unsigned int sdaComm;

#ifdef MASTER_COMPUTER
 #ifdef WIDE_FAST
  if (set->hadd.module>1 && set->hadd.module!=MAX_MOD_ADD)
 #else
  if (set->hadd.bank>NDCB && set->hadd.bank!=MAX_BANK_ADD)
 #endif
	return err;		// out-of-bounds bank or module kept to store value
#endif

// get the appropriate SDA command
if (set->hadd.group == DAC1)
	sdaComm = DCB_I2C_DAC1;
else if (set->hadd.group == DAC2)
	sdaComm = DCB_I2C_DAC2;
else
	{
	printf("Can't match hardware group %d\n", (int)set->hadd.group);
	return 1;
	}

// generate the dcb command
#if defined DAC_BITS_8
dcb_prog_i2c(set->hadd.bank, set->hadd.module, sdaComm, 
		dac_unit_info[set->hadd.unit]<<16 |
		dac_out_info[set->hadd.output]<<8 |
		data );
#elif defined DAC_BITS_10
// first the power up signal
dcb_prog_i2c(set->hadd.bank, set->hadd.module, sdaComm, 
		dac_unit_info[set->hadd.unit]<<16 | 0xf03c );
// program the voltage
dcb_prog_i2c(set->hadd.bank, set->hadd.module, sdaComm, 
		dac_unit_info[set->hadd.unit]<<16 |
		dac_out_info[set->hadd.output]<<8 |
		(data&0x3ff)<<2 );
#elif defined DAC_BITS_12
// first the power up signal
dcb_prog_i2c(set->hadd.bank, set->hadd.module, sdaComm, 
		dac_unit_info[set->hadd.unit]<<16 | 0xf03c );
// program the voltage
dcb_prog_i2c(set->hadd.bank, set->hadd.module, sdaComm, 
		dac_unit_info[set->hadd.unit]<<16 |
		dac_out_info[set->hadd.output]<<8 |
		(data&0xfff) );
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

#ifdef MASTER_COMPUTER
 #ifdef WIDE_FAST
  if (set->hadd.module>1 && set->hadd.module!=MAX_MOD_ADD)
 #else
  if (set->hadd.bank>NDCB && set->hadd.bank!=MAX_BANK_ADD)
 #endif
	return err;		// out-of-bounds bank or module kept to store value
#endif

// change the 'data' bit pattern to output pattern
outword = 0;
for (i = 0; i<16; i++)
	if ( data & (1<<i) )
		outword |= chsel_out_info[i];

dcb_prog_i2c(set->hadd.bank, set->hadd.module, DCB_I2C_CHSEL, 
		chsel_unit_info[1]<<16 |
		(outword&0xffff) );

return err;
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

int storeSetting(struct SETTINGS *set)
{
#ifdef SLSP2DET_CAMERA
int bank, mod, err;
struct SETTINGS tset;

if (set->hadd.bank == MAX_BANK_ADD && set->hadd.module == MAX_MOD_ADD)	// all modules
	{
	(void)memcpy(&tset, set, sizeof(struct SETTINGS));
	for (bank=1; bank<=NBANK; bank++)
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
	for (bank=1; bank<=NBANK; bank++)
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
	else if (p->hadd.group != NOT_USED)
		sprintf(line, "set %s %5.3f", p->name, p->vset);
	if (p->hadd.group != NOT_USED)
		fprintf(ofp, "%s\n", line);
	p = p->linkP;
	}

return 0;
}

