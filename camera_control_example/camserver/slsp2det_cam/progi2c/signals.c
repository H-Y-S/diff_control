/* signals.c - structures to hold the definitions of signals and routines
** for accessing them.
*/


#include <stdio.h>
#include <math.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include <unistd.h>
#include "camserver.h"
#include "i2c_util.h"
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
#elif defined DAC_BITS_10 || defined DAC_BITS_12
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
#elif defined DAC_BITS_10 || defined DAC_BITS_12
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
		{ CHSEL_SDA, "CHSEL_SDA" }
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


struct DETECTOR_INFO hDef[NcSig];	// holds detector hardware data, 1 struct per signal

int bank_add_lo = 0;	// 0 does not address any hardware
int bank_add_hi = 0;
int module_add_lo = 0;
int module_add_hi = 0;


// banks & mods are numbered 1..N in files, but 0..N-1 in the array
static struct
	{int modsn;				// module serial number
	float offset[NcSig_NS];
	} detector_map[NBANK][NMOD_BANK];
static int detector_map_initialized = 0;


#define pFLAG 20		// pattern flag - to detect problem with output

/*****************************************************************************\
**                                                                           **
**        read_detector_info                                                 **
**                                                                           **
\*****************************************************************************/


int read_detector_info(char *fname)
{
FILE *ifp;
int i, err=0, bank, mod;
int hint= NcSig-1;
char line[LENFN], *p;
int csig;
struct SETTINGS set;

line[0] = '\0';
if (fname[0] != '/')
	strcpy(line, cam_data_path);
strcat(line, fname);

DBG(1, "Reading detector hardware definitions from %s\n", line)
if ((ifp = fopen(line, "r")) == NULL)
	{
	DBG(1, "***Cannot open hardware definitions file: %s\n", line)
	printf("***Cannot open hardware definitions file:\n");
	printf("\t%s\n", line);
	return -1;
	}

DBG(1, "...Read detector hardware definitions from: %s\n", line)
printf("...Read detector hardware definitions from:\n");
printf("\t%s\n", line);

#if defined DAC_BITS_8
printf("...Using *** 8-bit *** dacs on mcbs\n");
DBG(1, "...Using *** 8-bit *** dacs on mcbs\n")
#elif defined DAC_BITS_10
printf("...Using *** 10-bit *** dacs on mcbs\n");
DBG(1, "...Using *** 10-bit *** dacs on mcbs\n")
#elif defined DAC_BITS_12
printf("...Using *** 12-bit *** dacs on mcbs\n");
DBG(1, "...Using *** 12-bit *** dacs on mcbs\n")
#endif

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
if (bank_add_lo == 0 || bank_add_hi == 0)
	{
	printf("Failed to find valid detector bank\n");
	DBG(1, "Failed to find valid detector bank\n")
	exit (1);
	}
if (module_add_lo == 0 || module_add_hi == 0)
	{
	printf("Failed to find valid detector module\n");
	DBG(1, "Failed to find valid detector module\n")
	exit (1);
	}

// store unused signals so they show up properly from getSetting
for (i = 0; i < NcSig; i++)
	if (hDef[i].hadd.group == NOT_USED)
		for (bank=1; bank<=NBANK; bank++)
			for (mod=1; mod<=NMOD_BANK; mod++)
				{
				set.hadd = hDef[i].hadd;
				set.hadd.bank = bank;
				set.hadd.module = mod;
				sprintf(set.name, "B%.2d_M%.2d_%s", bank, mod, cSig[i].name);
				storeSetting(&set);
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
		DBG(1, "Failed to find definition of %s\n", cSig[i].name)
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
		DBG(3, "Found unknown hardware group for %s\n", cSig[i].name)
		err++;
		}

if (err)
	sleep(3);
DBG(3, "Finished reading definitions\n")
return err;
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
			DBG(1, "Duplicate hardware definition: %s\n", p)
		//	exit (0);
			}
	DBG(5, "Defining DAC%d_U%d_OUT%d %f %f %f %f\n", hDef[i].hadd.group,
		hDef[i].hadd.unit, hDef[i].hadd.output, hDef[i].lowL, hDef[i].hiL,
		hDef[i].oSet, hDef[i].slope)
	}
else if ( (p = strstr(line+7, "CHSEL")) != NULL )
	{							// +7 to move past string match in col 1
	hDef[i].hadd.group = CHIPSEL;
	sscanf(p+7, "%hu", &hDef[i].hadd.unit);
	if ( strstr(line, "PATTERN") )
		{
		hDef[i].hadd.output = pFLAG;
		hDef[i].kind = TypePattern;
		DBG(5, "Defining CHSEL_U%d_PATTERN\n", hDef[i].hadd.unit)
		}
	else
		{
		sscanf(p+12, "%ho", &hDef[i].hadd.output);		// octal !!
		hDef[i].kind = TypeLevel;
		DBG(5, "Defining CHSEL_U%d_OUT%d\n", hDef[i].hadd.unit,
				hDef[i].hadd.output)
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
			DBG(1, "Duplicate hardware definition: %s\n", p)
		//	exit (0);
			}
	}
else if ( (p = strstr(line, "NOT_USED")) != NULL )
	{
	hDef[i].hadd.group = NOT_USED;
	DBG(5, "Defining: %s", line)		// there is '\n' in line
	}
else
	{
	printf("Cannot find DAC or CHSEL definition in %s\n", line);
	DBG(1, "Cannot find DAC or CHSEL definition in %s\n", line)
	exit (1);
	}

return;
}



/*****************************************************************************\
**                                                                           **
**        read_detector_map -- the map of module locations                   **
**                                                                           **
\*****************************************************************************/

int read_detector_map(char *fname)
{
FILE *ifp;
char line[LENFN], *p;
int bank, module, sn;

if (detector_map_initialized)
	{
	printf("*** ERROR - detector map is already initialized\n");
	DBG(1, "*** ERROR - detector map is already initialized\n")
	}

memset(detector_map, 0, sizeof(detector_map));
detector_map_initialized = 1;

line[0] = '\0';
if (fname[0] != '/')
	strcpy(line, cam_data_path);
strcat(line, fname);

DBG(1, "Reading detector module map from %s\n", line)
if ((ifp = fopen(line, "r")) == NULL)
	{
	DBG(1, "***Cannot open module map file: %s\n", line)
//	printf("***Cannot open module map file:\n");
//	printf("\t%s\n", line);
	return -1;
	}

DBG(1, "...Read detector module map from: %s\n", line)
printf("...Read detector module map from:\n");
printf("\t%s\n", line);

while ( (fgets(line, sizeof(line), ifp)) != NULL)
	{
	if ( (p = strchr(line, '#')) != NULL )
		*p = '\0';	// knock off comments
	p = line;
	while (isspace(*p))
		p++;
	if (*p == '\0')		// blank line?
		continue;
	if (line[0] == '\0' || line[0] == '\n')
		continue;
	sn = -1;
	sscanf(line, "%d%d%d", &bank, &module, &sn);
	if (sn<0)
		{
		DBG(1, "!!! Error in module map: %s\n", line)
		printf("!!! Error in module map: %s\n", line);
		continue;
		}
	if (bank<=0 || bank>NBANK || module<= 0 || module>NMOD_BANK)
		continue;
	if (detector_map[bank-1][module-1].modsn)
		{
		printf("!!! Duplicate module: bank = %d, module = %d, S/N = %d\n",
				bank, module, sn);
		DBG(1, "!!! Duplicate module: bank = %d, module = %d, S/N = %d\n",
				bank, module, sn)
		}

	// banks & mods are numbered 1..N in the file, but 0..N-1 in the array
	detector_map[bank-1][module-1].modsn = sn;
	}

fclose (ifp);

for (bank=1; bank<=NBANK; bank++)
	for (module=1; module<=NMOD_BANK; module++)
		if (detector_map[bank-1][module-1].modsn == 0)
		{
		printf("!!! bank = %d, module = %d missing from map\n", bank, module);
		DBG(1, "!!! bank = %d, module = %d missing from map\n", bank, module)
		}
// flag offsets as unitialized
for (bank=1; bank<=NBANK; bank++)
	for (module=1; module<=NMOD_BANK; module++)
		detector_map[bank-1][module-1].offset[0] = -99.0;

return 0;
}


/*****************************************************************************\
**                                                                           **
**        read_detector_offsets  -- module specific voltage offsets          **
**                                                                           **
\*****************************************************************************/

int read_detector_offsets(char *fname)
{
FILE *ifp;
char line[LENFN], *p, signal[10];
int sn, n, i, bank, mod;
float offset;

if (!detector_map_initialized)
	{
	printf("*** ERROR - must read detector map before offsets\n");
	DBG(1, "*** ERROR - must read detector map before offsets\n");
	return -1;
	}

line[0] = '\0';
if (fname[0] != '/')
	strcpy(line, cam_data_path);
strcat(line, fname);

DBG(1, "Reading detector voltage offsets from %s\n", line)
if ((ifp = fopen(line, "r")) == NULL)
	{
	DBG(1, "***Cannot open voltage offsets file: %s\n", line)
//	printf("***Cannot open voltage offsets file:\n");
//	printf("\t%s\n", line);
	return -1;
	}

DBG(1, "...Read detector voltage offsets from: %s\n", line)
printf("...Read detector voltage offsets from:\n");
printf("\t%s\n", line);

while ( (fgets(line, sizeof(line), ifp)) != NULL)
	{
	if ( (p = strchr(line, '#')) != NULL )
		*p = '\0';	// knock off comments
	if (line[0] == '\0' || line[0] == '\n')
		continue;
	p = line;
	while (isspace(*p))
		p++;
	if (*p == '\0')		// blank line?
		continue;
	sn = -1;
	sn = atoi(p);		// module serial number
	while (isspace(*p) || isdigit(*p))
		p++;
	if (*p == '\0')		// blank line?
		continue;
	n = 0;
	while (isalnum(*(p+n)))
		n++;
	strncpy(signal, p, n);			// signal name
	signal[n] = '\0';
	p+=n;
	offset = -99.0;
	while (isalpha(*p))
		p++;
	sscanf(p, "%f", &offset);
	if (offset<-10.0 || sn<0)
		{
		printf("!!! Error in offset list: %s\n", line);
		DBG(1, "!!! Error in offset list: %s\n", line)
		continue;
		}
// banks & mods are numbered 1..N in the file, but 0..N-1 in the array
	for (bank=1; bank<=NBANK; bank++)
		for (mod=1; mod<=NMOD_BANK; mod++)
			if (detector_map[bank-1][mod-1].modsn == sn)
				goto foundsn;

// Extra information in the offset file is not an error
// One could, in priciple, have one file for all modules ever made
//	printf("!!! Error in offset list: S/N not found: %s\n", line);
//	DBG(1, "!!! Error in offset list: S/N not found: %s\n", line)

	continue;
foundsn:
	if (detector_map[bank-1][mod-1].offset[0] <= -10.0)
		detector_map[bank-1][mod-1].offset[0] = 0.0;	// mark module is listed
	for(i=0; i<NcSig_NS; i++)
		if ( !strcasecmp(signal, cSig[i].name) )
			goto foundsig;
	printf("!!!  Error in offset list: unrecognized signal: %s\n", line);
	DBG(1, "!!!  Error in offset list: unrecognized signal: %s\n", line)
	continue;

foundsig:
	detector_map[bank-1][mod-1].offset[i] = offset;
	}
fclose (ifp);

for (bank=1; bank<=NBANK; bank++)
	for (mod=1; mod<=NMOD_BANK; mod++)
		if (detector_map[bank-1][mod-1].offset[0] <= -10.0)
			{
			printf("!!! bank = %d, module = %d missing offsets\n", bank, mod);
			DBG(1, "!!! bank = %d, module = %d missing offsets\n", bank, mod);
			detector_map[bank-1][mod-1].offset[0] = 0.0;
			}

return 0;
}



/*****************************************************************************\
**                                                                           **
**            module_offset()  -- get offset from detector_map               **
**                                                                           **
\*****************************************************************************/
// banks and modules are numbered 1...N.

float module_offset(int bank, int mod, enum CHIP_SIGNAL sig)
{
if (!detector_map_initialized)
	return 0.0;

if (sig >= NcSig_NS)		// no offsets for chsel's
	return 0.0;

// bank 15 or module 7 get 0.0
if (bank<=0 || bank==15 || mod<= 0 || mod==7)
	return 0.0;

// in the array, banks and mods are numbered 0...N-1
return detector_map[bank-1][mod-1].offset[(int)sig];
}
