/* signals.c - structures to hold the definitions of signals and routines
** for accessing them.
*/



#include <stdio.h>
#include <math.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include <unistd.h>
#include "ppg_util.h"
#include "pix_detector.h"
#include "cam_config.h"
#include "debug.h"


/*****************************************************************************\
**                                                                           **
**         Arrays of structures to hold detector parameters                  **
**                                                                           **
\*****************************************************************************/

#if defined DAC_BITS_8
unsigned short dac_unit_info[] = 
			{				// bit patterns to select DAC units
			0x00,			// there is no unit 0
			0x50,			// DAC_U1
			0x52,			// DAC_U2
			0x54,			// DAC_U3
			0x56			// DAC_U4
			} ;
#elif defined DAC_BITS_12
unsigned short dac_unit_info[] = 
			{				// bit patterns to select DAC units
			0x00,			// there is no unit 0
			0x78,			// DAC_U1
			0x7a,			// DAC_U2
			0xb8,			// DAC_U3
			0xba			// DAC_U4
			} ;
#endif



unsigned short chsel_unit_info[] =
			{		
			0x00,			// there is no unit 0
			0x40			// bit pattern for write CHSEL unit (only unit 1)
			} ;

#if defined DAC_BITS_8
unsigned short dac_out_info[] = 
			{			// bit pattern to select DAC output in unit
			0x0,		// DAC_OUT0
			0x1,		// DAC_OUT1
			0x2,		// DAC_OUT2
			0x3,		// DAC_OUT3
			0x4,		// DAC_OUT4
			0x5,		// DAC_OUT5
			0x6,		// DAC_OUT6
			0x7			// DAC_OUT7
			} ;
#elif defined DAC_BITS_12
unsigned short dac_out_info[] = 
			{			// bit pattern to select DAC output in unit
			0x0,		// DAC_OUT0
			0x10,		// DAC_OUT1
			0x20,		// DAC_OUT2
			0x30,		// DAC_OUT3
			} ;
#endif

unsigned short chsel_out_info[] = 
			{				// these are positions in a 16-bit word
			0x100,			// OUT0
			0x200,			// OUT1
			0x400,			// OUT2
			0x800,			// OUT3
			0x1000,			// OUT4
			0x2000,			// OUT5
			0x4000,			// OUT6
			0x8000,			// OUT7 - OUT7 is emitted first in serial pattern
			0x1,			// OUT10
			0x2,			// OUT11
			0x4,			// OUT12
			0x8,			// OUT13
			0x10,			// OUT14
			0x20,			// OUT15
			0x40,			// OUT16
			0x80			// OUT17 - OUT17 is emitted just after OUT0
			} ;				// we emit this word starting with the high bit


struct PATTERN_SIGNAL_NAMES pSig[] =
		{
		{ DAC1_SDA, "DAC1_SDA" },
		{ DAC2_SDA, "DAC2_SDA" },
		{ CHSEL_SDA, "CHSEL_SDA" },
		{ SCL, "SCL" },
		{ MOD_ADD_0, "MOD_ADD_0" },
		{ MOD_ADD_1, "MOD_ADD_1" },
		{ MOD_ADD_2, "MOD_ADD_2" },
		{ BANK_ADD_0, "BANK_ADD_0" },
		{ BANK_ADD_1, "BANK_ADD_1" },
		{ BANK_ADD_2, "BANK_ADD_2" },
		{ BANK_ADD_3, "BANK_ADD_3" }
		} ;


// All signal names we have used.  Not all are used in a given application,
// but all must be defined to avoid error messages.
struct CHIP_SIGNAL_NAMES cSig[] = 
		{
		{ VRF, "VRF" },
		{ VRFS, "VRFS" },
		{ VCCA, "VCCA" },
		{ VTRM, "VTRM" },
		{ VDEL, "VDEL" },
		{ VCAL, "VCAL" },
		{ VADJ, "VADJ" },
		{ VRES, "VRES" },
		{ VCMP, "VCMP" },
		{ VCMP0, "VCMP0" },
		{ VCMP1, "VCMP1" },
		{ VCMP2, "VCMP2" },
		{ VCMP3, "VCMP3" },
		{ VCMP4, "VCMP4" },
		{ VCMP5, "VCMP5" },
		{ VCMP6, "VCMP6" },
		{ VCMP7, "VCMP7" },
		{ VCMP8, "VCMP8" },
		{ VCMP9, "VCMP9" },
		{ VCMP10, "VCMP10" },
		{ VCMP11, "VCMP11" },
		{ VCMP12, "VCMP12" },
		{ VCMP13, "VCMP13" },
		{ VCMP14, "VCMP14" },
		{ VCMP15, "VCMP15" },
		{ CHSEL, "CHSEL" },
		{ CHSEL0, "CHSEL0" },
		{ CHSEL1, "CHSEL1" },
		{ CHSEL2, "CHSEL2" },
		{ CHSEL3, "CHSEL3" },
		{ CHSEL4, "CHSEL4" },
		{ CHSEL5, "CHSEL5" },
		{ CHSEL6, "CHSEL6" },
		{ CHSEL7, "CHSEL7" },
		{ CHSEL8, "CHSEL8" },
		{ CHSEL9, "CHSEL9" },
		{ CHSEL10, "CHSEL10" },
		{ CHSEL11, "CHSEL11" },
		{ CHSEL12, "CHSEL12" },
		{ CHSEL13, "CHSEL13" },
		{ CHSEL14, "CHSEL14" },
		{ CHSEL15, "CHSEL15" },
		} ;		


struct PGEN_INFO pGen[NpSig];	// holds pattern generator info, 1 struct per signal

struct DETECTOR_INFO hDef[NcSig];	// holds detector hardware date, 1 struct per signal

int bank_add_lo = 0;	// 0 does not address any hardware
int bank_add_hi = 0;
int module_add_lo = 0;
int module_add_hi = 0;

int nStatesClk = 1;
int nStatesDataSetup = 1;
int nStatesDataHold = 1;

#define pFLAG 20		// pattern flag

/*****************************************************************************\
**                                                                           **
**        read_detector_info                                                 **
**                                                                           **
\*****************************************************************************/


int read_detector_info(char *fname)
{
FILE *ifp;
int i, freq, err=0;
int hint= NcSig-1;
char line[120], *p;
enum PATTERN_SIGNAL psig;
int csig;

line[0] = '\0';
if (fname[0] != '/')
	strcpy(line, cam_data_path);
strcat(line, fname);

DBG(1, "Reading detector hardware definitions from %s\n", line);
if ((ifp = fopen(line, "r")) == NULL)
	{
	DBG(1, "***Cannot open hardware definitions file: %s\n", line);
	printf("***Cannot open hardware definitions file:\n");
	printf("\t%s\n", line);
	return -1;
	}

DBG(1, "...Read detector hardware definitions from: %s\n", line);
printf("...Read detector hardware definitions from:\n");
printf("\t%s\n", line);

/*
**  Version check - make sure we're using the right program for the hardware
*/
if ( (fgets(line, sizeof(line), ifp)) != NULL)
	{
	printf("%s", line);
	}
else
	{
	printf ("The file is empty:\n\t%s\n", line);
	return -1;
	}


for (i = 0; i< NpSig; i++)
	{
	pGen[i].bit_no = -1;	// flags for later checking
	pGen[i].logic = -1;
	}
for (i = 0; i < NcSig; i++)
	{
	hDef[i].lowL = -1000.0;	// flags for later checking
	hDef[i].hiL = -1000.0;
	hDef[i].oSet = -1000.0;
	hDef[i].slope = -1000.0;
	hDef[i].hadd.group = UnknownG;
	hDef[i].hadd.unit = 100;
	hDef[i].hadd.output = 100;
	hDef[i].kind = NotKnown;
	}
/*
** more efficient scans are easy to make, but why bother? This function is only 
** called once per detector initialization
*/
while ( (fgets(line, sizeof(line), ifp)) != NULL)
	{
	if ( (p = strchr(line, '#')) != NULL )
		*p = '\0';	// knock off comments
	if (line[0] == '\0')
		continue;
	if ( isspace(line[0]) )		// skip blank lines
		continue;
	if (strstr(line, "hardware_address_of_") == line)
		{
		psig = p_signal(line + strlen("hardware_address_of_"));
		if ( strstr(line, "NOT_USED") )
			{
			pGen[(int)psig].bit_no = -2;
			continue;
			}
		if ( strstr(line, "bit_") )
			sscanf(4+strstr(line, "bit_"), "%d", &pGen[(int)psig].bit_no);
		for (i = 0; i < NpSig; i++)
			if (i != (int)psig && pGen[i].bit_no == pGen[(int)psig].bit_no)
				{
				printf("Duplicate bit: %s\n", line+strlen("hardware_address_of_"));
				exit (1);
				}
		}
	else if (strstr(line, "asserted_level_of_") == line)
		{
		psig = p_signal(line + strlen("asserted_level_of_"));
		sscanf(4+strstr(line, "lvl_"), "%d", &pGen[(int)psig].logic);
		}
	else if (strstr(line, "quiescent_level_of_") == line)
		{
		psig = p_signal(line + strlen("quiescent_level_of_"));
		sscanf(4+strstr(line, "lvl_"), "%d", &pGen[(int)psig].quiescent);
		}
	else if (strstr(line, "BANK_ADD_LO") == line)
		{
		if ( strstr(line, "NOT_USED") )
			bank_add_lo = -1;
		else
			sscanf(line+strlen("BANK_ADD_LO"), "%d", &bank_add_lo);
		}
	else if (strstr(line, "BANK_ADD_HI") == line)
		{
		if ( strstr(line, "NOT_USED") )
			bank_add_hi = -1;
		else
			sscanf(line+strlen("BANK_ADD_HI"), "%d", &bank_add_hi);
		}
	else if (strstr(line, "MODULE_ADD_LO") == line)
		{
		if ( strstr(line, "NOT_USED") )
			module_add_lo = -1;
		else
			sscanf(line+strlen("MODULE_ADD_LO"), "%d", &module_add_lo);
		}
	else if (strstr(line, "MODULE_ADD_HI") == line)
		{
		if ( strstr(line, "NOT_USED") )
			module_add_hi = -1;
		else
			sscanf(line+strlen("MODULE_ADD_HI"), "%d", &module_add_hi);
		}
	else if (strstr(line, "PATTERN_BASE_WORD") == line)
		{
		sscanf(line+strlen("PATTERN_BASE_WORD"), "%hx", &baseWord);
		}
	else if (strstr(line, "I2C_VPG_UNIT") == line)
		{
		sscanf(line+strlen("I2C_VPG_UNIT"), "%d", &i);
		if (i<1 || i>2)
			printf("Illegal VPG unit number %d\n", i);
		else
			i2c_vpg_unit = i;
		}
	else if (strstr(line, "CLOCK_FREQUENCY") == line)
		{
		sscanf(line+strlen("CLOCK_FREQUENCY"), "%f", &clock_frequency);
		freq = (int) (0.5 + clock_frequency);
		nStatesClk = max(1, 1 + (1.3*freq) );			// for 5 MHz, need 7 states
		nStatesDataSetup = max (1, 1 + nStatesClk/2);	// for 5 MHz, need 4
		nStatesDataHold = max(1, nStatesClk - nStatesDataSetup) ;	// e.g., 3
		}
	else if (strstr(line, "VPG_CRYSTAL_MHZ") == line)
		{
		sscanf(line+strlen("VPG_CRYSTAL_MHZ"), "%f", &vpg_crystal_frequency);
		}
	else if ( -1 != (csig = c_signal(line, &hint)) )
		{
		set_signal(line, csig);
		}
	else
		{
		printf("Unrecognized line in %s: %s\n", fname, line);
		err++;
		}
	}

fclose (ifp);

// Now, check that everything is there
for (i = 0; i< NpSig; i++)
	{
	if (pGen[i].bit_no == -2)	// NOT_USED
		continue;
	if (pGen[i].bit_no == -1)
		{
		printf("Failed to find bit position for %s\n", pSig[i].name);
		DBG(1, "Failed to find bit position for %s\n", pSig[i].name);
		exit (1);
		}
	if (pGen[i].logic == -1)
		{
		printf("Failed to find logic level for %s\n", pSig[i].name);
		DBG(1, "Failed to find logic level for %s\n", pSig[i].name);
		exit (1);
		}
	}

if (bank_add_lo == 0 || bank_add_hi == 0)
	{
	printf("Failed to find valid detector bank\n");
	DBG(1, "Failed to find valid detector bank\n");
	exit (1);
	}
if (module_add_lo == 0 || module_add_hi == 0)
	{
	printf("Failed to find valid detector module\n");
	DBG(1, "Failed to find valid detector module\n");
	exit (1);
	}

for (i = 0; i < NcSig; i++)
	if (hDef[i].hadd.group == NOT_USED)
		continue;
	else if (hDef[i].lowL < -100.0 || hDef[i].hiL < -100.0 || 
				hDef[i].oSet < -100.0 || hDef[i].slope < -100.0 ||
				hDef[i].hadd.group == UnknownG || hDef[i].hadd.unit > 20 ||
				hDef[i].hadd.output > pFLAG || hDef[i].kind == NotKnown)
		{
		printf("Failed to find definition of %s\n", cSig[i].name);
		DBG(1, "Failed to find definition of %s\n", cSig[i].name);
		err++;
		}

// check bounds
for (i = 0; i < NcSig; i++)
	if (hDef[i].hadd.group == NOT_USED)
		continue;
	else if (hDef[i].hadd.group == DAC1 || hDef[i].hadd.group == DAC2)
		{
		if (hDef[i].hadd.unit < 1 || hDef[i].hadd.unit > MAX_DAC_UNIT)
			printf("Dac unit number out of bounds: %s\n", cSig[i].name);
		else if (hDef[i].hadd.output > MAX_DAC_OUTPUT)
			printf("Dac output number out of bounds: %s\n", cSig[i].name);
		}
	else if (hDef[i].hadd.group == CHIPSEL)
		{
		if (hDef[i].hadd.unit < 1 || hDef[i].hadd.unit > MAX_CHSEL_UNIT)
			printf("Chsel unit number out of bounds: %s\n", cSig[i].name);
		else if (hDef[i].hadd.output!=pFLAG && hDef[i].hadd.output > MAX_CHSEL_OUTPUT)
			printf("Chsel output number out of bounds: %s\n", cSig[i].name);
		}
	else
		{
		printf("Found unknown hardware group for %s\n", cSig[i].name);
		DBG(3, "Found unknown hardware group for %s\n", cSig[i].name);
		err++;
		}

if (err)
	sleep(3);
DBG(3, "Finished reading definitions\n");
return err;
}


/*
** Return pattern generator signal code matching a name, e.g. DAC1_SDA
*/
enum PATTERN_SIGNAL p_signal(char *p)
{
int i;

for(i=0; i<NpSig; i++)
	if (strstr(p, pSig[i].name) == p)	// name should match at 1st character
		return pSig[i].sig;

printf("Failed to find signal in: %s\n", p);
return -1;
}


/*
** Return chip signal matching a name,e.g. VSF-
*/
int c_signal(char *line, int *hint)
{
int i;

// if the signals in the definition file are in the same order as the names
// in cSig[], then 'hint' avoids excess searching

for (i = 0; i < NcSig; i++)
	{
	(*hint)++;
	*hint %= NcSig;
	if ( strstr(line, cSig[*hint].name) == line && 
				isspace(line[strlen(cSig[*hint].name)]) )
		return *hint;
	}
return -1;
}

/*
** Read the hardware parameters for a chip signal from 'line'
*/
void set_signal(char *line, int i)
{
char *p;
int n;

if (hDef[i].hadd.group != UnknownG)
	printf("ERROR - redefinition of signal: %s\n", cSig[i].name);

if ( (p = strstr(line, "DAC")) != NULL )
	{
	if(*(p+3) == '1')
		hDef[i].hadd.group = DAC1;
	if(*(p+3) == '2')
		hDef[i].hadd.group = DAC2;
	sscanf(p+6, "%hu", &hDef[i].hadd.unit);
	sscanf(p+11, "%hu", &hDef[i].hadd.output);
	sscanf(p+12, "%f %f %f %f", &hDef[i].lowL, &hDef[i].hiL, &hDef[i].oSet, 
			&hDef[i].slope);
	hDef[i].kind = TypeVoltage;
	for (n = 0; n < NcSig; n++)		// check for duplicates
		if (n!=i && hDef[i].hadd.group==hDef[n].hadd.group &&
					hDef[i].hadd.unit==hDef[n].hadd.unit &&
					hDef[i].hadd.output==hDef[n].hadd.output)
			{
			printf("Duplicate hardware definition: %s\n", p);
			DBG(2, "Duplicate hardware definition: %s\n", p);
		//	exit (0);
			}
	DBG(5, "Defining DAC%d_U%d_OUT%d %f %f %f %f\n", hDef[i].hadd.group,
		hDef[i].hadd.unit, hDef[i].hadd.output, hDef[i].lowL, hDef[i].hiL,
		hDef[i].oSet, hDef[i].slope);
	}
else if ( (p = strstr(line+7, "CHSEL")) != NULL )
	{							// +7 to move past string match in col 1
	hDef[i].hadd.group = CHIPSEL;
	sscanf(p+7, "%hu", &hDef[i].hadd.unit);
	if ( strstr(line, "PATTERN") )
		{
		hDef[i].hadd.output = pFLAG;
		hDef[i].kind = TypePattern;
		DBG(5, "Defining CHSEL_U%d_PATTERN\n", hDef[i].hadd.unit);
		}
	else
		{
		sscanf(p+12, "%ho", &hDef[i].hadd.output);		// octal !!
		hDef[i].kind = TypeLevel;
		DBG(5, "Defining CHSEL_U%d_OUT%d\n", hDef[i].hadd.unit,
				hDef[i].hadd.output);
		}
	hDef[i].lowL = 0.0;		// flags that information was found
	hDef[i].hiL = 0.0;
	hDef[i].oSet = 0.0;
	hDef[i].slope = 0.0;
	for (n = 0; n < NcSig; n++)		// check for duplicates
		if (n!=i && hDef[i].hadd.group==hDef[n].hadd.group &&
					hDef[i].hadd.unit==hDef[n].hadd.unit &&
					hDef[i].hadd.output==hDef[n].hadd.output)
			{
			printf("Duplicate hardware definition: %s\n", p);
			DBG(2, "Duplicate hardware definition: %s\n", p);
		//	exit (0);
			}
	}
else if ( (p = strstr(line, "NOT_USED")) != NULL )
	{
	hDef[i].hadd.group = NOT_USED;
	DBG(5, "Defining: %s", line);		// there is '\n' in line
	}
else
	{
	printf("Cannot find DAC or CHSEL definition in %s\n", line);
	DBG(1, "Cannot find DAC or CHSEL definition in %s\n", line);
	exit (1);
	}

return;
}
