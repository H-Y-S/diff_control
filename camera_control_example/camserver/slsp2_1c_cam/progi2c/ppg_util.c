// ppg_util.c - inteface functions between commands and i2c/fifo programming
// derived from ppg.c

#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include <unistd.h>
#include "tvxcampkg.h"
#include "camserver.h"
#include "ppg_util.h"
#include "debug.h"

#ifdef SLSDET_CAMERA 			//left for reference
#include <sys/types.h>	// open
#include <sys/stat.h>	// open
#include <fcntl.h>		// open
#endif

//CHB 27.4.05
#ifdef P2_1CHIP_CAMERA
#include <sys/types.h>	// open
#include <sys/stat.h>	// open
#include <fcntl.h>		// open
#endif

#define LINELNG 500

#define PATCOMP "vpg517bn"		// the pattern compiler - in pattern directory

/******************************************************************************\
**                                                                            **
**          Global variables                                                  **
**                                                                            **
\******************************************************************************/
char outFileName[LINELNG] = "";
unsigned short thePattern[PAT_LNG];
unsigned short baseWord;
float clock_frequency=5.0;
float vpg_crystal_frequency=40.0;
int i2c_vpg_unit = 1;		// vpg unit for programming dacs

int autoExec=True;
TYPEOF_PIX theImage[IMAGE_SIZE];
int imageLength;
int fifo_timeout=10;
unsigned int i2c_pattern_base=0x7800;
unsigned int start_line=0;
int pgRunning[2] = {False, False};
int filenumber;
char autofilename[LENFN] = "null";
char trimFileName[LENFN] = "";
char datfile[LENFN];
char imgfile[LENFN];
int loadingTrimFile;
int trimallVal=0;
double vcmpallVal=-99.0;
int time_lim;

unsigned int base_vpg_ena_pattern=0x5c00;		// VPG1
unsigned int base_read_chip_pattern0=0x5000;	// VPG1
unsigned int base_read_chip_pattern1=0x4c00;	// VPG1
unsigned int base_enable_pattern=0x3800;		// VPG1
unsigned int base_disable_pattern=0x3400;		// VPG1
unsigned int base_trim_all_pattern=0x3000;		// VPG1

static int defaultBank = 1;
static int defaultModule = 1;


#ifdef SLSP2_1C_CAMERA
short trimValues[NROW_CHIP][NCOL_CHIP];
int selected_chip = -1;
#endif

#ifdef SLSP2_1M_CAMERA
short trimValues[NCHIP][NROW_CHIP][NCOL_CHIP];
int selected_chip = -9;
int tVos=0;
#endif

#ifdef SLSP2DET_CAMERA
short trimValues[NBANKS][NMOD_BANK][NCHIP][NROW_CHIP][NCOL_CHIP];
int selected_chip = -9;
int selected_mod = 0;
int selected_bank = -9;
int tVos=0;
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
**
*/
int set_i2c(char *ptr)
{
float farg;
int iarg, i;
char targ[10];

if ( (i=set_canonical_form(ptr)) )
	return i;			// i==-1 is error, i==2 is set bank/module

setup_pattern();
for (i=0; *(ptr+i) && *(ptr+i)!=' '; i++)
	;			// find the space before the value, if given
if (strstr(ptr, "CHSEL") != NULL)	// a chsel?
	{
	iarg = 1;		// default if no argument
	if (*(ptr+i))
		sscanf(ptr+i, "%d", &iarg);
	*(ptr+i) = '\0';		// take off value, if given
	if (set_chsel(ptr, iarg))
		{
		printf("Error return from pattern composer\n");
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
		printf("Error return from pattern composer\n");
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

if (set_canonical_form(ptr) )
	return -1;				// error

setup_pattern();
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
			printf("Error return from pattern composer\n");
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
			printf("Error return from pattern composer\n");
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
		printf("Error return from pattern composer\n");
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

setup_pattern();
if (set_chsel(ptr, 0))
	{
	printf("Error return from pattern composer\n");
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
char line[LINELNG], *p = ptr, *q;

line[0] = '\0';
if( (p = get_bank(line, p)) == NULL)
	return -1;			// error
if( (p = get_module(line, p)) == NULL)
	return -1;			// error

// if *p is empty and 'line' is clean, user may be selecting bank or module
if (*p == '\0')
	{
	if (!strchr(line, '*') && !strstr(line, "B15") && !strstr(line, "M07"))
		return 2;
	printf("Cannot interpret request\n");
	return -1;	
	}

while (isspace(*p))
	p++;
// p should now point to a chip signal name - look it up
for (i=0; i<NcSig; i++)
	if ( (q=strstr(p, cSig[i].name)) != NULL )
		break;
if (q!=p)		// garbled syntax or non-existent
	{
	printf("Cannot find signal name: %s\n", p);
	return -1;
	}

strcat(line, p);		// copy rest of argument to line
strcpy(ptr, line);		// give the new line back to caller

if ( (p = strrchr(ptr, ' ')) != NULL && !(isdigit(*(p+1))|| *(p+1)=='-') )
	*p = '\0';		// cut off possible filename

DBG(7, "Canonical form: %s\n", ptr);

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
		if (i<=0 || (i>NBANKS && i!=MAX_BANK_ADD))
			{
			printf("Bank %d is out of bounds\n", i);
			return NULL;
			}
		}
	else
		{
		printf("Cannot interpret bank in: %s\n", ptr);
		return NULL;
		}
	if (i>0 && i<=NBANKS)
		selected_bank = defaultBank = i;
#endif
	while ( (*p) && (*p != ' ') && (*p != '_') )
		p++;			// find the separator
	if (*p)
		p++;				// advance to next char
	}
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
#ifdef SLSP2_1C_CAMERA
#else
#ifdef SLSP2_1M_CAMERA
#else		// SLSP2DET_CAMERA can change default
	if (*p == '*')
		i = MAX_MOD_ADD;
	else if (isdigit(*p))
		{
		i = atoi(p);
		if (i<=0 || (i>NMOD_BANK && i!=MAX_MOD_ADD))
			{
			printf("Module %d is out of bounds\n", i);
			return NULL;
			}
		}
	else
		{
		printf("Cannot interpret module in: %s\n", ptr);
		return NULL;
		}
	if (i>0 && i<=NMOD_BANK)
		selected_mod = defaultModule = i;
#endif
#endif
	while ( (*p) && (*p != ' ') && (*p != '_') )
		p++;			// find the separator
	if (*p)
		p++;				// advance to next char
	}
sprintf(line+strlen(line), "M%.2d_", i);

return p;
}


/******************************************************************************\
**                                                                            **
**     Pattern Generation routines                                            **
**                                                                            **
\******************************************************************************/
void setup_pattern(void)
{
int i;

// Set the initial pattern to all 0
for (i = 0; i < PAT_LNG; i++)
	thePattern[i] = 0;

// baseWord is the running word to be output to pattern set
// we set bits to preserve signals in sls02++
for (i = 0; i < NpSig; i++)		// install the quiescent state
	{
	if ( (pGen[i].quiescent == 1 && pGen[i].logic == 1)  || 
					(pGen[i].quiescent == 0 && pGen[i].logic == 0) )
			baseWord |= 1 << pGen[i].bit_no;	// set negative logic lines to 1 = off
	}
patternP = thePattern;	// point to start of pattern
*patternP++ = baseWord;		// put the starting state ("quiescent") in the pattern

return;
}


/*
** If you need the bit order inverted, use:
**		selector = 0x1			and
**		selector <<= 1
*/

void write_pattern(char *filename, char *ptr)
{
FILE *ofp;
int add;
char line[80];
unsigned short *p;

if (*filename == '\0')	// no name means don't write
	return;

line[0]='\0';
if (strchr(filename, '/') == NULL)		// was a path specified?
	strcpy(line, cam_data_path);		// no, use this path
strcat(line, filename);
if ((ofp=fopen(line, "w")) == NULL)
	{
	printf("Unable to open output file for writing %s\n", line);
	return;
	}

DBG(8,"Opened %s for writing\n", line);
strcpy(filename, line);			// copy back full path for caller

fprintf(ofp, "# %s\n", ptr);	// title
add = i2c_pattern_base;

#ifdef BINARY_OUTPUT
		// bitwise code  -- these must be altered manually for new baseWord
fprintf(ofp, "0000 0000000000000000 0180\n");		// jump to 0x80
for (p=thePattern; p<patternP; p++, add++)
	{
	int i;
	unsigned short selector = 0x8000;
	fprintf(ofp,  "%04x ", add);
	for (i=0; i<16; i++)
		{
		if (selector & *p)
			fprintf(ofp, "1");
		else
			fprintf(ofp, "0");
		selector >>= 1;
		}
	fprintf(ofp, "\n");
	}
fprintf(ofp, "0000 0000000000000000 0020\n");		// wait command

#else

fprintf(ofp, "0000 %04x%2x80\n", baseWord, (add>>7));		// jump to 0x80
for (p=thePattern; p<patternP; p++, add++)
	fprintf(ofp, "%04x %04x0000\n", add, *p);
fprintf(ofp, "%04x %04x0020\n", add, baseWord);		// wait command

#endif

fclose(ofp);

return;
}


/******************************************************************************\
**                                                                            **
**     Configuration - saving, loading & logging                              **
**                                                                            **
\******************************************************************************/
void load_configuration(char *ptr)
{
char line[80], *p;
FILE *ifp;

line[0]='\0';
if (*ptr != '.' && *ptr != '/')		// was a path specified?
	strcpy(line, cam_data_path);	// no, use this path
strcat(line, ptr);
DBG(1, "Load configuration from %s\n", line);
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
		vpg_execute_pattern(thePattern, patternP-thePattern, i2c_vpg_unit);
		}
	else
		printf("Error interpreting line: %s\n", line);
	}

fclose (ifp);
return;
}


void save_configuration(char *ptr)
{
char line[80];
FILE *ofp;

line[0]='\0';
if (*ptr != '.' && *ptr != '/')		// was a path specified?
	strcpy(line, cam_data_path);	// no, use this path
strcat(line, ptr);
DBG(1, "Save configuration to %s\n", line);
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

fprintf(ofp, "Clock divider for VPG1: %d\n", clock_divider1);
if (clock_divider2 > 0)
	fprintf(ofp, "Clock divider for VPG2: %d\n", clock_divider2);

if (selected_chip >= 0)
	fprintf(ofp, "Selected chip is #%d\n", selected_chip);
else if (selected_chip == -1)
	fprintf(ofp, "All chips selected\n");

#ifdef SLSDET_CAMERA
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
		sprintf(line2+strlen(line2), "setdac_b%.2d_m%.2d.dat", bank, mod);
		if ( (sfp[bank][mod]=fopen(line2, "w")) == NULL)
			{
			printf("Could not open for writing:\n\t%s\n", line2);
			goto bailout;
			}
		fprintf(sfp[bank][mod], "# setdacs_b%.2d_m%.2d.dat", bank, mod);
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


/******************************************************************************\
**                                                                            **
**     Programming trim bits                                                  **
**                                                                            **
\******************************************************************************/


//CHB 27.04.05
#ifdef SLSP2_1C_CAMERA
void setTrims(void)
{
int row, col, n;
char line[240];
FILE *ofp, *tfp;

printf("setTrims for p2_1chip\n");


if (trimValues[0][0] < 0)	
{
	printf ("Trim array is not loaded\n");
	return;
	}

// make up patterns to program a full chip - takes 2.3 min for a bank
strcpy(line, cam_data_path);
strcat(line, "p2_loadtrim.src");		// trim pattern template
if ( (tfp=fopen(line, "r")) == NULL )
	{
	printf("Cannot find template: %s\n", line);
	return;
	}
strcpy(line, cam_data_path);
strcat(line, "p2_trimchip.src");		// trim pattern template
if ( (ofp=fopen(line, "w")) ==  NULL )
	{
	printf("Cannot open output file: %s\n", line);
	fclose(tfp);
	return;
	}
while(fgets(line, sizeof(line), tfp))
	{
	fprintf(ofp, "%s", line);	// copy template to output
	if (strstr(line, "START OF MAIN PROGRAM"))
		goto start1;
	}
printf("Start not found in template\n");
fclose(tfp);
fclose(ofp);
return;

start1:

while(fgets(line, sizeof(line), tfp))
	{
	fprintf(ofp, "%s", line);	// copy template to output
	if (strstr(line, "MAIN:"))
		goto start2;
	}
printf("MAIN: not found in template\n");
fclose(tfp);
fclose(ofp);
return;

start2:

// make up the pattern
for (row = 0; row < NROW_CHIP; row++)		// make up temp file
	{
	for (col = 0; col < NCOL_CHIP; col++)
	if (trimValues[row][col] <= 0xf)
		fprintf(ofp, "      1000 0011 0010 0011 ; jump PR0%X ! %3d %3d\n",
				trimValues[row][col], col, row);
	else
		fprintf(ofp, "      1000 0011 0010 0011 ; jump PR%X ! %3d %3d\n",
				trimValues[row][col], col, row);
	fprintf(ofp, "!\n      1000 0011 0010 0011 ; jump NXTR !\n!\n");
	}

// finish up
while(fgets(line, sizeof(line), tfp))
	{
	if (strstr(line, "END OF MAIN PROGRAM"))
		goto end1;
	}
printf("End not found in template\n");
fclose(tfp);
fclose(ofp);
return;

end1:					// copy rest of template

while(fgets(line, sizeof(line), tfp))
	fprintf(ofp, "%s", line);	// copy template to output

fclose(tfp);
fclose(ofp);

// compile the pattern
strcpy(line, "cd ");
strcat(line, cam_data_path);
strcat(line, ";");
strcat(line, PATCOMP);
strcat(line, " p2_trimchip.src >/dev/null");
n=system(line);
printf("p2_trimchip.src was compiled, return code = %d\n", n);
//printf("%s\n", line);

vpg_stop_clk(vpg_default_unit);		// load and run it
strcpy(line, "p2_trimchip.abs 0");
vpg_load_pattern(line, vpg_default_unit);
vpg_set_jump(0, vpg_default_unit);
vpg_start_clock(vpg_default_unit);
vpg_wait_pattern(10, vpg_default_unit);
vpg_stop_clk(vpg_default_unit);

DBG(3, "Finished setting trims from array\n");
return;
}
#endif		// SLSP2_1C_CAMERA


#ifdef SLSP2_1M_CAMERA
void setTrims(void)
{
int row, col, n;
char line[240];
FILE *ofp, *tfp;

printf("setTrims for p2_1mod\n");

if (selected_chip<0 || selected_chip>NCHIP)
	{
	printf ("Selected chip is: %x; exactly 1 chip must be selected\n",
			selected_chip);
	return;
	}

if (trimValues[selected_chip][0][0] < 0)
	{
	printf ("Trim array is not loaded\n");
	return;
	}

// make up patterns to program a full chip - takes 2.3 min for a bank
strcpy(line, cam_data_path);
strcat(line, "p2_loadtrim.src");		// trim pattern template
if ( (tfp=fopen(line, "r")) == NULL )
	{
	printf("Cannot find template: %s\n", line);
	return;
	}
strcpy(line, cam_data_path);
strcat(line, "p2_trimchip.src");		// trim pattern template
if ( (ofp=fopen(line, "w")) ==  NULL )
	{
	printf("Cannot open output file: %s\n", line);
	fclose(tfp);
	return;
	}
while(fgets(line, sizeof(line), tfp))
	{
	fprintf(ofp, "%s", line);	// copy template to output
	if (strstr(line, "START OF MAIN PROGRAM"))
		goto start1;
	}
printf("Start not found in template\n");
fclose(tfp);
fclose(ofp);
return;

start1:

while(fgets(line, sizeof(line), tfp))
	{
	fprintf(ofp, "%s", line);	// copy template to output
	if (strstr(line, "MAIN:"))
		goto start2;
	}
printf("MAIN: not found in template\n");
fclose(tfp);
fclose(ofp);
return;

start2:

// make up the pattern
for (row = 0; row < NROW_CHIP; row++)		// make up temp file
	{
	for (col = 0; col < NCOL_CHIP; col++)
	if (trimValues[selected_chip][row][col] <= 0xf)
		fprintf(ofp, "      1000 0011 0010 0011 ; jump PR0%X ! %3d %3d\n",
				trimValues[selected_chip][row][col], col, row);
	else
		fprintf(ofp, "      1000 0011 0010 0011 ; jump PR%X ! %3d %3d\n",
				trimValues[selected_chip][row][col], col, row);
	fprintf(ofp, "!\n      1000 0011 0010 0011 ; jump NXTR !\n!\n");
	}

// finish up
while(fgets(line, sizeof(line), tfp))
	{
	if (strstr(line, "END OF MAIN PROGRAM"))
		goto end1;
	}
printf("End not found in template\n");
fclose(tfp);
fclose(ofp);
return;

end1:					// copy rest of template

while(fgets(line, sizeof(line), tfp))
	fprintf(ofp, "%s", line);	// copy template to output

fclose(tfp);
fclose(ofp);

// compile the pattern
strcpy(line, "cd ");
strcat(line, cam_data_path);
strcat(line, ";");
strcat(line, PATCOMP);
strcat(line, " p2_trimchip.src >/dev/null");
n=system(line);
printf("p2_trimchip.src was compiled, return code = %d\n", n);
//printf("%s\n", line);

vpg_stop_clk(vpg_default_unit);		// load and run it
strcpy(line, "p2_trimchip.abs 0");
vpg_load_pattern(line, vpg_default_unit);
vpg_set_jump(0, vpg_default_unit);
vpg_start_clock(vpg_default_unit);
vpg_wait_pattern(10, vpg_default_unit);
vpg_stop_clk(vpg_default_unit);

DBG(3, "Finished setting trims from array\n");
return;
}
#endif		// SLSP2_1M_CAMERA


#ifdef SLSP2DET_CAMERA
void setTrims(void)
{
int row, col, n;
char line[240];
FILE *ofp, *tfp;

printf("setTrims for p2 detector\n");

if (selected_chip<0 || selected_chip>NCHIP)
	{
	printf ("Selected chip is: %x; exactly 1 chip must be selected\n",
			selected_chip);
	return;
	}

if (selected_mod<0 || selected_mod>NMOD_BANK)
	{
	printf ("Selected mod is: %x; exactly 1 module must be selected\n",
			selected_mod);
	return;
	}

if (selected_bank<0 || selected_bank>NBANKS)
	{
	printf ("Selected bank is: %x; exactly 1 bank must be selected\n",
			selected_bank);
	return;
	}

if (trimValues[selected_bank][selected_mod][selected_chip][0][0] < 0)
	{
	printf ("Trim array is not loaded\n");
	return;
	}

// make up patterns to program a full chip - takes 2.3 min for a bank
strcpy(line, cam_data_path);
strcat(line, "p2_loadtrim.src");		// trim pattern template
if ( (tfp=fopen(line, "r")) == NULL )
	{
	printf("Cannot find template: %s\n", line);
	return;
	}
strcpy(line, cam_data_path);
strcat(line, "p2_trimchip.src");		// trim pattern template
if ( (ofp=fopen(line, "w")) ==  NULL )
	{
	printf("Cannot open output file: %s\n", line);
	fclose(tfp);
	return;
	}
while(fgets(line, sizeof(line), tfp))
	{
	fprintf(ofp, "%s", line);	// copy template to output
	if (strstr(line, "START OF MAIN PROGRAM"))
		goto start1;
	}
printf("Start not found in template\n");
fclose(tfp);
fclose(ofp);
return;

start1:

while(fgets(line, sizeof(line), tfp))
	{
	fprintf(ofp, "%s", line);	// copy template to output
	if (strstr(line, "MAIN:"))
		goto start2;
	}
printf("MAIN: not found in template\n");
fclose(tfp);
fclose(ofp);
return;

start2:

// make up the pattern
for (row = 0; row < NROW_CHIP; row++)		// make up temp file
	{
	for (col = 0; col < NCOL_CHIP; col++)
	if (trimValues[selected_bank][selected_mod][selected_chip][row][col] <= 0xf)
		fprintf(ofp, "      1000 0011 0010 0011 ; jump PR0%X ! %3d %3d\n",
				trimValues[selected_bank][selected_mod][selected_chip][row][col], col, row);
	else
		fprintf(ofp, "      1000 0011 0010 0011 ; jump PR%X ! %3d %3d\n",
				trimValues[selected_bank][selected_mod][selected_chip][row][col], col, row);
	fprintf(ofp, "!\n      1000 0011 0010 0011 ; jump NXTR !\n!\n");
	}

// finish up
while(fgets(line, sizeof(line), tfp))
	{
	if (strstr(line, "END OF MAIN PROGRAM"))
		goto end1;
	}
printf("End not found in template\n");
fclose(tfp);
fclose(ofp);
return;

end1:					// copy rest of template

while(fgets(line, sizeof(line), tfp))
	fprintf(ofp, "%s", line);	// copy template to output

fclose(tfp);
fclose(ofp);

// compile the pattern
strcpy(line, "cd ");
strcat(line, cam_data_path);
strcat(line, ";");
strcat(line, PATCOMP);
strcat(line, " p2_trimchip.src >/dev/null");
n=system(line);
printf("p2_trimchip.src was compiled, return code = %d\n", n);
//printf("%s\n", line);

vpg_stop_clk(vpg_default_unit);		// load and run it
strcpy(line, "p2_trimchip.abs 0");
vpg_load_pattern(line, vpg_default_unit);
vpg_set_jump(0, vpg_default_unit);
vpg_start_clock(vpg_default_unit);
vpg_wait_pattern(10, vpg_default_unit);
vpg_stop_clk(vpg_default_unit);

DBG(3, "Finished setting trims from array\n");
return;
}
#endif		// SLSP2DET_CAMERA



// Poke a trim value into VPG517 pattern
// For efficiency, don't repeat pokes if bit is already set correctly
void setPatternTrimBits(int value, unsigned int base, int last)
{
vpg_stop_clk(vpg_default_unit);

//CHB 26.4.05 two additional trimbits
printf("6-bit trim version for Pilatus II\n");
if ( last<0 || ( (value&0x20) != (last&0x20) ) )
	{
	if ( value & 0x20 )		// MSB
		{
		vpg_write((base+LINE_TRIM_BIT5)<<2, TRIM_WORD_1_FOR_0, vpg_default_unit);
		vpg_write((base+LINE_TRIM_BIT5+1)<<2, TRIM_WORD_2_FOR_0, vpg_default_unit);
		}
	else
		{
		vpg_write((base+LINE_TRIM_BIT5)<<2, TRIM_WORD_1_FOR_1, vpg_default_unit);
		vpg_write((base+LINE_TRIM_BIT5+1)<<2, TRIM_WORD_2_FOR_1, vpg_default_unit);
		}
	}

if ( last<0 || ( (value&0x10) != (last&0x10) ) )
	{
	if ( value & 0x10 )
		{
		vpg_write((base+LINE_TRIM_BIT4)<<2, TRIM_WORD_1_FOR_0, vpg_default_unit);
		vpg_write((base+LINE_TRIM_BIT4+1)<<2, TRIM_WORD_2_FOR_0, vpg_default_unit);
		}
	else
		{
		vpg_write((base+LINE_TRIM_BIT4)<<2, TRIM_WORD_1_FOR_1, vpg_default_unit);
		vpg_write((base+LINE_TRIM_BIT4+1)<<2, TRIM_WORD_2_FOR_1, vpg_default_unit);
		}
	}

if ( last<0 || ( (value&0x8) != (last&0x8) ) )
	{
	if ( value & 0x8 )
		{
		vpg_write((base+LINE_TRIM_BIT3)<<2, TRIM_WORD_1_FOR_0, vpg_default_unit);
		vpg_write((base+LINE_TRIM_BIT3+1)<<2, TRIM_WORD_2_FOR_0, vpg_default_unit);
		}
	else
		{
		vpg_write((base+LINE_TRIM_BIT3)<<2, TRIM_WORD_1_FOR_1, vpg_default_unit);
		vpg_write((base+LINE_TRIM_BIT3+1)<<2, TRIM_WORD_2_FOR_1, vpg_default_unit);
		}
	}
if ( last<0 || ( (value&0x4) != (last&0x4) ) )
	{
	if ( value & 0x4 )
		{
		vpg_write((base+LINE_TRIM_BIT2)<<2, TRIM_WORD_1_FOR_0, vpg_default_unit);
		vpg_write((base+LINE_TRIM_BIT2+1)<<2, TRIM_WORD_2_FOR_0, vpg_default_unit);
		}
	else
		{
		vpg_write((base+LINE_TRIM_BIT2)<<2, TRIM_WORD_1_FOR_1, vpg_default_unit);
		vpg_write((base+LINE_TRIM_BIT2+1)<<2, TRIM_WORD_2_FOR_1, vpg_default_unit);
		}
	}
if ( last<0 || ( (value&0x2) != (last&0x2) ) )
	{
	if ( value & 0x2 )
		{
		vpg_write((base+LINE_TRIM_BIT1)<<2, TRIM_WORD_1_FOR_0, vpg_default_unit);
		vpg_write((base+LINE_TRIM_BIT1+1)<<2, TRIM_WORD_2_FOR_0, vpg_default_unit);
		}
	else
		{
		vpg_write((base+LINE_TRIM_BIT1)<<2, TRIM_WORD_1_FOR_1, vpg_default_unit);
		vpg_write((base+LINE_TRIM_BIT1+1)<<2, TRIM_WORD_2_FOR_1, vpg_default_unit);
		}
	}
if ( last<0 || ( (value&0x1) != (last&0x1) ) )
	{
	if ( value & 0x1 )		// LSB
		{
		vpg_write((base+LINE_TRIM_BIT0)<<2, TRIM_WORD_1_FOR_0, vpg_default_unit);
		vpg_write((base+LINE_TRIM_BIT0+1)<<2, TRIM_WORD_2_FOR_0, vpg_default_unit);
		}
	else
		{
		vpg_write((base+LINE_TRIM_BIT0)<<2, TRIM_WORD_1_FOR_1, vpg_default_unit);
		vpg_write((base+LINE_TRIM_BIT0+1)<<2, TRIM_WORD_2_FOR_1, vpg_default_unit);
		}
	}
return;
}

#ifdef SLSP2_1C_CAMERA
void init_trim_array(int value)
{
int x, y;

for (y = 0; y < NROW_CHIP; y++)
	for( x = 0; x < NCOL_CHIP; x++)
		trimValues[y][x] = value;

return;
}
#endif


#ifdef SLSP2_1M_CAMERA
void init_trim_array(int value)
{
int i, x, y;

for (i=0; i<NCHIP; i++)
	for (y = 0; y < NROW_CHIP; y++)
		for( x = 0; x < NCOL_CHIP; x++)
			trimValues[i][y][x] = value;

return;
}
#endif

#ifdef SLSP2DET_CAMERA
void init_trim_array(int value)
{
int i, j, k, x, y;

for(k=0; k<NBANKS; k++)
	for (j=0; j<NMOD_BANK; j++)
		for (i=0; i<NCHIP; i++)
			for (y = 0; y < NROW_CHIP; y++)
				for( x = 0; x < NCOL_CHIP; x++)
					trimValues[k][j][i][y][x] = value;

return;
}
#endif


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
struct SETTINGS *sp, *sps;

strcpy(t, "VCMP");
set_canonical_form(t);
if ( (sp = getSetting(t)) )
	{
	sprintf(line, "%6.3f   ", sp->vset);
	return;
	}

for (i=0; i<17; i++)
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

strcpy(t, "VRF");
set_canonical_form(t);
if ( (sp = getSetting(t)) )
	{
	sprintf(line, "%6.3f   ", sp->vset);
	return;
	}
for (i=0; i<17; i++)
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

strcpy(t, "VRFS");
set_canonical_form(t);
if ( (sp = getSetting(t)) )
	{
	sprintf(line, "%6.3f   ", sp->vset);
	return;
	}
for (i=0; i<17; i++)
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

strcpy(t, "VTRM");
set_canonical_form(t);
if ( (sp = getSetting(t)) )
	sprintf(line, "%6.3f   ", sp->vset);
else
	strcpy(line, "   ??    ");

return;
}
