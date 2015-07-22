// i2c_util.c - inteface functions between commands and i2c programming,
// including logging and retrieving settings
// derived from ppg_util.c

#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include <unistd.h>
#include <math.h>
#include "tvxcampkg.h"
#include "camserver.h"
#include "i2c_util.h"
#include "debug.h"

#ifdef SLSP2DET_CAMERA
#include <sys/types.h>	// open
#include <sys/stat.h>	// open
#include <fcntl.h>		// open
#endif



/******************************************************************************\
**                                                                            **
**          Global variables                                                  **
**                                                                            **
\******************************************************************************/
int filenumber;
char autofilename[LENFN] = "null";
char trimFileName[LENFN] = "";
char datfile[LENFN];
char imgfile[LENFN];
int loadingTrimFile;
int trimallVal=0;
double vcmpallVal=-99.0;
int time_lim;
int dac_offset_comp_enable=0;	// default is off, turned on for trim calculations

// internal variables
static int defaultBank = 1;
static int defaultModule = 1;

#ifdef SLSP2DET_CAMERA
int selected_chip = -9;
int selected_mod = -1;
int selected_bank = -1;
#else
 #error
#endif


/******************************************************************************\
**                                                                            **
**         Prototypes of local functions defined in this module               **
**                                                                            **
\******************************************************************************/
static char *get_bank(char *, char *);
static char *get_module(char *, char *);


/******************************************************************************\
**                                                                            **
**          Support routines                                                  **
**                                                                            **
\******************************************************************************/



/******************************************************************************\
**                                                                            **
**          i2c interpretation routines                                       **
**                                                                            **
**  These routines prepare the operator's input text before passing to the    **
** routines that generate the pattern.  The objective is to be generous       **
** in the interpreation of what was typed - if we can make sense out of it,   **
** use it even if the syntax isn't perfect.                                   **
**                                                                            **
\******************************************************************************/

/*
**  set_i2c, reset_i2c_chsel, prog_i2c, etc.
**
**  Usage (using set as the example):
**    
**      set B5_M1_CHSEL6    (canonical form, not case sensistive)
**      set B5 M1 CHSEL6    (underscores omitted)
**      set M1_CHSEL6       (bank omitted)
**      set CHSEL6          (using default bank & module)
**      set B5_M1_CHSEL6 0  (state explicitly given)
**      set B5_M1_VRF8 0.75 (a dac, as above)
**		set B03_M02			(set default selections)
*/
int set_i2c(char *ptr)
{
float farg;
int iarg, i;
char targ[10];

if ( (i=set_canonical_form(ptr)) )
	return i;	// i==1 is error, i==2 is set bank/module, i==3 is other computer

for (i=0; *(ptr+i) && *(ptr+i)!=' '; i++)
	;			// find the space before the value, if given
if (strstr(ptr, "CHSEL") != NULL)	// a chsel?
	{
	iarg = 1;		// default if no argument
	if (*(ptr+i))
		sscanf(ptr+i, "%i", &iarg);
	*(ptr+i) = '\0';		// take off value, if given
	if (set_chsel(ptr, iarg))
		{
		printf("Error from pattern composer for signal: %s\n", ptr);
		return -1;
		}
	else
		{
		sprintf(targ, " %d", iarg);		// text copy of argument
		strcat(ptr, targ);
		}
	}
else				// must be a dac - there must be a voltage given
	{
	farg = -1000.0;
	sscanf(ptr+i, "%f", &farg);
	if (farg < -100.0)
		{
		printf("Can't find the voltage for the DAC in: %s\n", ptr);
		return -1;
		}
	*(ptr+i) = '\0';		// take off voltage
	if (set_dac(ptr, farg))
		{
		printf("Error from pattern composer for signal: %s\n", ptr);
		return -1;
		}
	else
		{
		sprintf(targ, " %4.2f", farg);		// text copy of argument
		strcat(ptr, targ);
		}
	}

return 0;
}


int prog_i2c(char *ptr)
{
int iarg, i;
char targ[10], *p;

if ( (i=set_canonical_form(ptr)) )
	return i;	// i==1 is error, i==2 is set bank/module, i==3 is other computer

for (i=0; *(ptr+i) && *(ptr+i)!=' '; i++)
	;			// find the space before the value

iarg = -1000;
sscanf(ptr+i, "%i", &iarg);		// there must be a value
if ( (p=strstr(ptr, "CHSEL")) != NULL)	// a chsel?
	{
	if ( isdigit(*(p+strlen("CHSEL"))) )	// a specific CHSEL
		{
		if (iarg < 0 || iarg > 1)
			{
			printf("Hex value out of range for a CHSEL: %s\n", ptr);
			return -1;
			}
		*(ptr+i) = '\0';		// take off setting
		if (set_chsel(ptr, iarg))
			{
			printf("Error from pattern composer for signal: %s\n", ptr);
			return -1;
			}
		else
			{
			sprintf(targ, " %d", iarg);		// text copy of argument
			strcat(ptr, targ);
			}
		}
	else			// a pattern for all CHSEL's
		{
		if (iarg < 0 || iarg > 0xffff)
			{
			printf("Hex value out of range for a CHSEL pattern: %s\n", ptr);
			return -1;
			}
		*(ptr+i) = '\0';		// take off setting
		if (set_chsel(ptr, iarg))
			{
			printf("Error from pattern composer for signal: %s\n", ptr);
			return -1;
			}
		else
			{
			sprintf(targ, " %d", iarg);		// text copy of argument
			strcat(ptr, targ);
			}
		}
	}
else			// set a DAC
	{
#if defined DAC_BITS_8
	if (iarg < 0 || iarg > 0xff)
		{
		printf("Hex value out of range for a DAC: %s\n", ptr);
		return -1;
		}
#elif defined DAC_BITS_10
	if (iarg < 0 || iarg > 0x3ff)
		{
		printf("Hex value out of range for a DAC: %s\n", ptr);
		return -1;
		}
#elif defined DAC_BITS_12
	if (iarg < 0 || iarg > 0xfff)
		{
		printf("Hex value out of range for a DAC: %s\n", ptr);
		return -1;
		}
#endif
	*(ptr+i) = '\0';		// take off setting
	if (set_dac_hex(ptr, iarg))
		{
		printf("Error from pattern composer for signal: %s\n", ptr);
		return -1;
		}
	else
		{
		sprintf(targ, " %#x", iarg);		// text copy of argument
		strcat(ptr, targ);
		}
	}
return 0;
}


int reset_i2c_chsel(char *ptr)
{
if (set_canonical_form(ptr) )
	return -1;				// error

if (set_chsel(ptr, 0))
	{
	printf("Error from pattern composer for signal: %s\n", ptr);
	return -1;
	}
else
	{
	strcat(ptr, " 0");			// put in argument for comment
	}

return 0;
}



/*
** Attempt to transform input into canonical form:
**
**		B3_M2_VRFS5  (e.g.)
**
** Returns 0 if OK, non-zero if error
*/

int set_canonical_form(char *ptr)
{
int i;
char line[LENFN], *p = ptr, *q;

while(*p)
	{
	*p = toupper(*p);
	p++;
	}
p = ptr;

line[0] = '\0';
if( (p = get_bank(line, p)) == NULL)
	return 3;			// not this computer if multicomputer system
if( (p = get_module(line, p)) == NULL)
	return 3;			// not this computer if multicomputer system

// if *p is empty and 'line' is clean, user may be selecting bank and/or module
if (*p == '\0')
	{
	if ( strchr(line, 'B') || strchr(line, 'M') )
		return 2;
	printf("Cannot interpret request\n");
	return 1;	
	}

while (isspace(*p))
	p++;
// p should now point to a chip signal name - look it up
// be kind to the operator - permit a leading 0; e.g. CHSEL03
if ( (q = strchr(p, '0')) && isalpha(*(q-1)) && isdigit(*(q+1)) )
	strcpy(q, q+1);
for (i=0; i<NcSig; i++)
	if ( (q=strstr(p, cSig[i].name)) != NULL )
		break;
if (q!=p)		// garbled syntax or non-existent
	{
	printf("Cannot find signal name: %s\n", p);
	return 1;
	}

strcat(line, p);		// copy rest of argument to line
strcpy(ptr, line);		// give the new line back to caller

if ( (p = strrchr(ptr, ' ')) != NULL && !(isdigit(*(p+1))|| *(p+1)=='-') )
	*p = '\0';		// cut off possible filename

DBG(7, "Canonical form: %s\n", ptr)

return 0;
}


static char *get_bank(char *line, char *ptr)
{
char *p = ptr;
int i=defaultBank;

if (*p == 'B')		// bank was provided - set default
	{
	p++;			// next char - should be a number
#ifdef SLSP2DET_CAMERA		// only full detector uses non-default bank
	if (*p == '*')
		i = MAX_BANK_ADD;
	else if (isdigit(*p))
		{
		i = atoi(p);
	// translate bank addresses in slaves
#if defined SLAVE_COMPUTER && !defined WIDE_FAST
		if (i!=MAX_BANK_ADD)
			i-=SLAVE_COMPUTER;
		if (i<=0 || (i>NBANK && i!=MAX_BANK_ADD))
			return NULL;	// not for this computer
#else
		if (i<=0 || (i>NBANK && i!=MAX_BANK_ADD))
			{
			printf("Bank %d is out of bounds\n", i);
			return NULL;
			}
#endif
		}
	else
		{
		printf("Cannot interpret bank in: %s\n", ptr);
		return NULL;
		}
	if ((i>0 && i<=NBANK) || i==MAX_BANK_ADD)
		selected_bank = defaultBank = i;
#else
	selected_bank = defaultBank = i;
#endif
	while ( (*p) && (*p != ' ') && (*p != '_') )
		p++;			// find the separator
	if (*p)
		p++;			// advance to next char
	}
else					// nothing specified
	selected_bank = defaultBank = i;
sprintf(line+strlen(line), "B%.2d_", i);
return p;

}



static char *get_module(char *line, char *ptr)
{
char *p = ptr;
int i=defaultModule;

if (*p == 'M')		// module was provided - set default
	{
	p++;			// next char - should be a number
	if (*p == '*')
		i = MAX_MOD_ADD;
	else if (isdigit(*p))
		{
		i = atoi(p);
	// translate module addresses in slaves
#if defined SLAVE_COMPUTER && defined WIDE_FAST
		if (i!=MAX_MOD_ADD)
			i-=SLAVE_COMPUTER;
		if (i<=0 || (i>NMOD_BANK && i!=MAX_MOD_ADD))
			return NULL;	// not for this computer
#else
		if (i<=0 || (i>NMOD_BANK && i!=MAX_MOD_ADD))
			{
			printf("Module %d is out of bounds\n", i);
			return NULL;
			}
#endif
		}
	else
		{
		printf("Cannot interpret module in: %s\n", ptr);
		return NULL;
		}
	if ((i>0 && i<=NMOD_BANK) || i==MAX_MOD_ADD)
		selected_mod = defaultModule = i;
	while ( (*p) && (*p != ' ') && (*p != '_') )
		p++;			// find the separator
	if (*p)
		p++;			// advance to next char
	}
else					// nothing specified
	selected_mod = defaultModule = i;
sprintf(line+strlen(line), "M%.2d_", i);

return p;
}




/******************************************************************************\
**                                                                            **
**     Configuration - saving, loading & logging                              **
**                                                                            **
\******************************************************************************/
void load_configuration(char *ptr)
{
char line[LENFN], *p;
FILE *ifp;

line[0]='\0';
if (*ptr != '.' && *ptr != '/')		// was a path specified?
	strcpy(line, cam_data_path);	// no, use this path
strcat(line, ptr);
DBG(1, "Load configuration from %s\n", line)
if ( (ifp = fopen(line, "r")) == NULL )
	{
	printf("Unable to open configuration file:\n\t%s\n", line);
	return;
	}

while (fgets(line, sizeof(line), ifp) != NULL)
	{
	p = strchr(line, '\n');
	if (p != NULL)				// take out newline
		*p = '\0';
	p = strchr(line, '#');		// remove comments
	if (p != NULL)
		*p = '\0';
	if (line[0] == '\0')		// skip blank lines & comments
		continue;
	if (strncasecmp(line, "set ", 4) == 0)
		{
		printf("--- Setting %s\n", line+4);
		set_i2c(line+4);
		}
	else if (strncasecmp(line, "prog ", 5) == 0)
		{
		printf("--- Programming %s\n", line+5);
		prog_i2c(line+5);
		}
	else
		printf("Error interpreting line: %s\n", line);
	}

fclose (ifp);
return;
}


void save_configuration(char *ptr)
{
char line[LENFN];
FILE *ofp;

line[0]='\0';
if (*ptr != '.' && *ptr != '/')		// was a path specified?
	strcpy(line, cam_data_path);	// no, use this path
strcat(line, ptr);
DBG(1, "Save configuration to %s\n", line)
if ( (ofp = fopen(line, "w")) == NULL )
	{
	printf("Unable to open configuration file %s\n", line);
	return;
	}

saveSettings(ofp);

printf("Configuration saved to:\n\t%s\n", line);
fclose(ofp);
return;
}


void log_i2c_settings(FILE *ofp, char *path)
{
int mod, bank, save_mod=defaultModule, save_bank=defaultBank;
char line[120], line2[120], *p;
struct SETTINGS *sp;
FILE *ifp, *sfp[NBANK+1][NMOD_BANK+1];

fprintf(ofp, "Settings reported from stored values:\n\n");

if (selected_chip >= 0)
	fprintf(ofp, "Selected chip is #%d\n", selected_chip);
else if (selected_chip == -1)
	fprintf(ofp, "All chips selected\n");

#ifdef SLSP2DET_CAMERA
fprintf(ofp, "Last selected mod is #%d\n", selected_mod);
fprintf(ofp, "Last selected bank is #%d\n", selected_bank);
#endif

// other setting names are read from config file
strcpy(line, cam_data_path);
strcat(line, "settings2log.txt");
if ( (ifp = fopen(line, "r")) == NULL)
	{
	fprintf(ofp, "Could not open list of settings to log:\n\t%s\n", line);
	fprintf(ofp, "---------------------------------------------------------------------\n\n");
	return;
	}

// open a command output file for each module
for(bank=1; bank<=NBANK; bank++)
	for(mod=1; mod<=NMOD_BANK; mod++)
		sfp[bank][mod] = NULL;
for(bank=1; bank<=NBANK; bank++)
	for(mod=1; mod<=NMOD_BANK; mod++)
		{
		strcpy(line2, path);
#ifdef SLSP2DET_CAMERA
		sprintf(line2+strlen(line2), "setdacs_b%.2d_m%.2d.dat", bank, mod);
#else
		sprintf(line2+strlen(line2), "setdacs.dat");
#endif
		if ( (sfp[bank][mod]=fopen(line2, "w")) == NULL)
			{
			printf("Could not open for writing:\n\t%s\n", line2);
			goto bailout;
			}
		fprintf(sfp[bank][mod], "# %s\n", line2);
		}

while(fgets(line2, sizeof(line2), ifp))
	{
	if ( (p = strchr(line2, '\n')) )
		*p = '\0';
	if (line2[0] == '#' || line2[0] == '\0')
		continue;
	for(bank=1; bank<=NBANK; bank++)
		for(mod=1; mod<=NMOD_BANK; mod++)
			{
			strcpy(line, line2);
			defaultModule=mod;
			defaultBank=bank;
			set_canonical_form(line);
			if ( (sp = getSetting(line)) )
				{
				fprintf(ofp, "set %s   %6.3f\n", line, sp->vset);
				fprintf(sfp[bank][mod], "set %s %6.3f\n", line, sp->vset);
				}
			else
				fprintf(ofp, "%s    ??\n", line);
			}
	}

bailout:
fclose(ifp);
fprintf(ofp, "---------------------------------------------------------------------\n\n");
for(bank=1; bank<=NBANK; bank++)
	for(mod=1; mod<=NMOD_BANK; mod++)
		if (sfp[bank][mod])
			fclose(sfp[bank][mod]);
defaultModule=save_mod;
defaultBank=save_bank;
return;
}


/*****************************************************************************\
***                                                                         ***
***         get_vcmpall - set vcmpallVal & prepare text                     ***
***                                                                         ***
\*****************************************************************************/

/*
**  1)  vcmp may not be set
**  2)  vcmp may be a single (global value)
**  3)  there may be individual vcmps, but they have a single value
**  4)  the vcmps may all be different
*/
void get_vcmpall(char *line, int bank, int mod)
{
int i, count=0;
char t[50];
double val=99.0;
struct SETTINGS *sp, *sps=NULL;

defaultBank = bank;
defaultModule = mod;

strcpy(t, "VCMP");
set_canonical_form(t);
if ( (sp = getSetting(t)) && (sp->hadd.group != NOT_USED) )
	{
	sprintf(line, "%6.3f   ", sp->vset);
	return;
	}

for (i=0; i<16; i++)
	{
	sprintf(t, "VCMP%d", i);
	set_canonical_form(t);
	if ( (sp = getSetting(t)) )
		{
		sps = sp;
		if (count == 0)
			val = sp->vset;
		else if (val != sp->vset)
			break;			// they differ
		count++;
		}
	}

if (count == 0)				// none found
	strcpy(line, "   ??    ");
else if (val == sps->vset)	// global value
	{
	sprintf(line, "%6.3f   ", sps->vset);
	vcmpallVal = sps->vset;
	}
else						// they differ - assume standard filename
	{
	vcmpallVal = 99.0;
	strcpy(line, "./vcmpset");
	moduleFilename(line, NULL, "", bank, mod);
	strcat(line, "   ");
	}

return;
}


void get_vrfall(char *line, int bank, int mod)
{
int i, count=0;
char t[50];
double val=99.0;
struct SETTINGS *sp;

defaultBank = bank;
defaultModule = mod;

strcpy(t, "VRF");
set_canonical_form(t);
if ( (sp = getSetting(t)) && (sp->hadd.group != NOT_USED) )
	{
	sprintf(line, "%6.3f   ", sp->vset);
	return;
	}

for (i=0; i<16; i++)
	{
	sprintf(t, "VRF%d", i);
	set_canonical_form(t);
	if ( (sp = getSetting(t)) )
		{
		val = sp->vset;
		count++;
		break;
		}
	}

if (count == 0)				// none found
	strcpy(line, "   ??    ");
else
	sprintf(line, "%6.3f   ", sp->vset);


return;
}


void get_vrfsall(char *line, int bank, int mod)
{
int i, count=0;
char t[50];
double val=99.0;
struct SETTINGS *sp;

defaultBank = bank;
defaultModule = mod;

strcpy(t, "VRFS");
set_canonical_form(t);
if ( (sp = getSetting(t)) && (sp->hadd.group != NOT_USED) )
	{
	sprintf(line, "%6.3f   ", sp->vset);
	return;
	}

for (i=0; i<16; i++)
	{
	sprintf(t, "VRFS%d", i);
	set_canonical_form(t);
	if ( (sp = getSetting(t)) )
		{
		val = sp->vset;
		count++;
		break;
		}
	}

if (count == 0)				// none found
	strcpy(line, "   ??    ");
else
	sprintf(line, "%6.3f   ", sp->vset);

return;
}


void get_vtrmall(char *line, int bank, int mod)
{
char t[50];
struct SETTINGS *sp;

defaultBank = bank;
defaultModule = mod;

strcpy(t, "VTRM");
set_canonical_form(t);
if ( (sp = getSetting(t)) && (sp->hadd.group != NOT_USED) )
	sprintf(line, "%6.3f   ", sp->vset);
else
	strcpy(line, "   ??    ");

return;
}



/*****************************************************************************\
***                                                                         ***
***    apply_deltaV - apply a delta V to signal(s) on all modules           ***
***                                                                         ***
\*****************************************************************************/

void apply_deltaV(char *p)
{
char t[20], u[20], *ptr=p;
int i, bank, mod;
double v;
struct SETTINGS *sp;

while (isspace(*ptr))
	ptr++;

for(i=0; i<50; i++)			// isolate signal base name
	if (isalpha(*(ptr+i)))
		t[i] = toupper(*(ptr+i));
	else
		break;
t[i]='\0';

if (!strcmp(t, "CHSEL"))
	{
	printf("Can't change CHSEL\n");
	return;
	}

ptr+=i;
if (isdigit(*ptr))		// skip the number in e.g. VCMP9
	ptr++;
if (isdigit(*ptr))
	ptr++;

while (isspace(*ptr))
	ptr++;

i = sscanf(ptr, "%lf", &v);
if (i<1)
	{
	printf("Cannot find value delta in %s\n", p);
	return;
	}

if (fabs(v)>1.0)
	{
	printf("Requested delta too large: %.4lf\n", v);
	return;
	}

printf("Using PILATUS II signal definitions\n");

for(bank=1; bank<=NBANK; bank++)
	{
	for(mod=1; mod<=NMOD_BANK; mod++)
		{
		if (strcmp(t, "VCMP"))
			{
			sprintf(u, "B%.2d_M%.2d_%s", bank, mod, t);
			sp = getSetting(u);
			if (sp->hadd.group == NOT_USED)
				{
				printf("Error: signal %s is not used\n", u);
				return;
				}
			set_dac(sp->name, v+sp->vset);
	//		printf("Setting %s to %.4lf V\n", sp->name, v+sp->vset);
			}
		else		// there are 16 vcmp's
			{
			for(i=0; i<16; i++)
				{
				sprintf(u, "B%.2d_M%.2d_%s%d", bank, mod, t, i);
				sp = getSetting(u);
				if (sp->hadd.group == NOT_USED)
					{
					printf("Error: signal %s is not used\n", u);
					return;
					}
				set_dac(sp->name, v+sp->vset);
				}
			}
		}
	}

return;
}
