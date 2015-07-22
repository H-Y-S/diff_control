// calibs.c - show or set energy and threshold calibrations

#define CALIBS_MAIN		// turn on space allocations in calibs.h


#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <string.h>
#include <ctype.h>
#include "tvxcampkg.h"
#include "camserver.h"
#include "debug.h"




// local functions
static CALIBS_CMND find_calibs_command(char **);
static double read_vcmp(char *);


/*****************************************************************************\
**                                                                           **
**    Static variables                                                       **
**                                                                           **
\*****************************************************************************/

#define SPEC_MAX 10

typedef enum {
		LOWG=0,				// values are used as array subscripts
		MIDG=1,
		HIGHG=2,
		UHIGHG=3
		}setting_type;		// gain settings


static struct
			{
			double vrf;
			double slope;
			double tau;
			double tcoeff;
			int nfile;						// no. of dirs at this setting
			struct	
				{
				double e;					// energy setting
				double vc;					// vcmp
				char ff[LENFN];				// flat-field file
				char bpm[LENFN];			// bad pixel map
				char extra[LENFN];			// optional command file
				char dir[LENFN];			// dir of autotrim & setting files
				} trim_spec[SPEC_MAX];		// different energies
			} setting[1+(int)UHIGHG];		// LOWG, MIDG, etc.

static setting_type current_set=MIDG;
static char current_gain[40]="not implemented";
static int current_tfile=0;
static double current_th=0.0;
static double vcmp_set=0.0;


/*****************************************************************************\
**                                                                           **
**   show_calibration - show the current calibration setup                   **
**                                                                           **
\*****************************************************************************/

void show_calibration(char *buf)
{
char line[LENFN];
int i=0;

if (current_th == 0.0)
	{
	strcpy(buf, "Threshold has not been set");
	return;
	}

switch (current_set)
	{
	case LOWG:
		strcpy(line, "low gain");
		break;
	case MIDG:
		strcpy(line, "mid gain");
		break;
	case HIGHG:
		strcpy(line, "high gain");
		break;
	case UHIGHG:
		strcpy(line, "ultra high gain");
		break;
	default:
		strcpy(line, "unknown gain");
		break;
	}

i+=sprintf(buf+i, "Settings: %s; threshold: %.0lf eV; vcmp: %.3lf V\n", 
		line, current_th, vcmp_set);
i+=sprintf(buf+i, " Trim directory:\n  %s", 
		setting[current_set].trim_spec[current_tfile].dir);

return;
}



/*****************************************************************************\
**                                                                           **
**   get_current_threshold - return the current setting                      **
**                                                                           **
\*****************************************************************************/
double get_current_threshold(void)
{
return current_th;
}



/*****************************************************************************\
**                                                                           **
**   set_current_threshold - set stored value (internal use only)            **
**                                                                           **
\*****************************************************************************/
void set_current_threshold(double v)
{
current_th= v;
return;
}



/*****************************************************************************\
**                                                                           **
**   set_calibration - set up a detector calibration script                  **
**                                                                           **
\*****************************************************************************/

int set_calibration(char *ptr, char *msg)
{
char line[80+LENFN], *p, *q, trimpath[LENFN], line2[120], line3[120];
int spec=0, i, bank, mod, chip;
FILE *ifp, *ofp;
CALIBS_CMND cmnd;
setting_type set=MIDG, new_set;
double e, emin, emax, ediff, vcmp, vcref=0.0, vc=0.0, new_th, vcsum;
double vcca_set[NBANK+1][NMOD_BANK+1];

if ( *ptr=='0' || !strncasecmp(ptr, "off", 4) )
	{
	current_th=0.0;
	camera_record_variable(Cam_threshold_setting, "0");
	}

// read the calibration file & get parameters
if ( (ifp=fopen(detector_calibration_file, "r")) == NULL)
	{
	printf("Could not open for reading:\n  %s\n", detector_calibration_file);
	return -1;
	}

memset(setting, 0, sizeof(setting));
memset(vcca_set, 0, sizeof(vcca_set));
msg[0]='\0';

while ( (fgets(line, sizeof(line), ifp) != NULL) )
	{
	if ( (p = strchr(line, '#')) )
		*p = '\0';						// cut off comments
	p = line;
	while (isspace(*p))					// skip leading whitespace
		p++;
	q = p-1+strlen(p);
	while ( isspace(*q) )				// remove trailing whitespace
		*q-- = '\0';
	if ( !(cmnd = find_calibs_command(&p)) )
		continue;
	if (!*p)
		continue;
	while (isspace(*p) || *p=='=')		// space over to argument
		p++;
	/* start of main loop switch - please preserve the format for mkheader */
	switch (cmnd)
		{
		case Settings:
			q=p;
			while(*q)
				{
				*q = tolower(*q);
				q++;
				}
			if (strstr(p, "uhighg"))
				set=UHIGHG;
			else if (strstr(p, "highg"))
				set=HIGHG;
			else if (strstr(p, "midg"))
				set=MIDG;
			else if (strstr(p, "lowg"))
				set=LOWG;
			else
				printf("Unrecognized: %s\n", line);
			spec=0;
			break;
		case Vrf:
			setting[set].vrf = atof(p);
			break;
		case Slope:
			setting[set].slope = atof(p);
			break;
		case TAu:		// orthography to avoid collision with tvxcampkg.c
			setting[set].tau = atof(p);
			break;
		case Tempcoeff:
			setting[set].tcoeff = atof(p);
			break;
		case E:
			if (spec >= SPEC_MAX)
				goto specmax;
			setting[set].trim_spec[spec].e = atof(p);
			break;
		case FField:
			strcpy(setting[set].trim_spec[spec].ff, p);
			break;
		case BPmap:
			strcpy(setting[set].trim_spec[spec].bpm, p);
			break;
		case ExtraCMD:
			strcpy(setting[set].trim_spec[spec].extra, p);
			break;
		case Dir:
			if ( *(p-1+strlen(p)) != '/')
				strcat(p, "/");
			strcpy(setting[set].trim_spec[spec].dir, p);
			setting[set].trim_spec[spec].vc = read_vcmp(p);
			if (setting[set].trim_spec[spec].vc < 0.0)
				{
				fclose(ifp);
				return -1;
				}
			spec++;
			setting[set].nfile = spec;
			break;
		default:
			break;
		}
		/* end of main loop switch */
	}

fclose(ifp);

specmax:
if (spec >= SPEC_MAX)
	{
	printf("maximum number of trim files exceeded\n");
	return -1;
	}

// check for ascending order of energy in each gain setting
// also check for descending order of vcmp
set=UHIGHG;
strcpy(line2, "ultra high gain");
for(e=0, vc=1.1, spec=0; setting[set].nfile && spec<SPEC_MAX; spec++)
	{
	if (setting[set].trim_spec[spec].e == 0.0)
		break;
	if (setting[set].trim_spec[spec].e < e)
		{
		printf("Error in uhighG: energy %lf out of order\n", 
				setting[set].trim_spec[spec].e);
		return -1;
		}
	e = setting[set].trim_spec[spec].e;
	if (setting[set].trim_spec[spec].vc < vc)
		vc = setting[set].trim_spec[spec].vc;
	else
		{
		printf(" Data for %s at energy %.0lf: vcmp = %.3lf makes no sense\n", 
				line2, e, setting[set].trim_spec[spec].vc);
		return -1;
		}
	}
set=HIGHG;
strcpy(line2, "high gain");
for(e=0, vc=1.1, spec=0; setting[set].nfile && spec<SPEC_MAX; spec++)
	{
	if (setting[set].trim_spec[spec].e == 0.0)
		break;
	if (setting[set].trim_spec[spec].e < e)
		{
		printf("Error in highG: energy %lf out of order\n", 
				setting[set].trim_spec[spec].e);
		return -1;
		}
	e = setting[set].trim_spec[spec].e;
	if (setting[set].trim_spec[spec].vc < vc)
		vc = setting[set].trim_spec[spec].vc;
	else
		{
		printf(" Data for %s at energy %.0lf: vcmp = %.3lf makes no sense\n", 
				line2, e, setting[set].trim_spec[spec].vc);
		return -1;
		}
	}
set=MIDG;
strcpy(line2, "mid gain");
for(e=0, vc=1.1, spec=0; setting[set].nfile && spec<SPEC_MAX; spec++)
	{
	if (setting[set].trim_spec[spec].e == 0.0)
		break;
	if (setting[set].trim_spec[spec].e < e)
		{
		printf("Error in midG: energy %.0lf out of order\n", 
				setting[set].trim_spec[spec].e);
		return -1;
		}
	e = setting[set].trim_spec[spec].e;
	if (setting[set].trim_spec[spec].vc < vc)
		vc = setting[set].trim_spec[spec].vc;
	else
		{
		printf(" Data for %s at energy %.0lf: vcmp = %.3lf makes no sense\n", 
				line2, e, setting[set].trim_spec[spec].vc);
		return -1;
		}
	}
set=LOWG;
strcpy(line2, "low gain");
for(e=0, vc=1.1, spec=0; setting[set].nfile && spec<SPEC_MAX; spec++)
	{
	if (setting[set].trim_spec[spec].e == 0.0)
		break;
	if (setting[set].trim_spec[spec].e < e)
		{
		printf("Error in lowG: energy %lf out of order\n", 
				setting[set].trim_spec[spec].e);
		return -1;
		}
	e = setting[set].trim_spec[spec].e;
	if (setting[set].trim_spec[spec].vc < vc)
		vc = setting[set].trim_spec[spec].vc;
	else
		{
		printf(" Data for %s at energy %.0lf: vcmp = %.3lf makes no sense\n", 
				line2, e, setting[set].trim_spec[spec].vc);
		return -1;
		}
	}

#if 0
// Verify ==============================

set=LOWG;
printf("settings = lowG\n\
vrf = %lf\n\
slope = %lf\n\
tau = %g\n\
tempcoeff = %lf\n", 
setting[set].vrf, setting[set].slope, setting[set].tau,setting[set].tcoeff);
spec=0;
while(setting[set].trim_spec[spec].e>0)
	{
	printf("e = %.0lf, vcmp = %.3lf\ndir = %s\n", setting[set].trim_spec[spec].e,
			setting[set].trim_spec[spec].vc, setting[set].trim_spec[spec].dir);
	spec++;
	}

set=MIDG;
printf("settings = midG\n\
vrf = %lf\n\
slope = %lf\n\
tau = %g\n\
tempcoeff = %lf\n", 
setting[set].vrf, setting[set].slope, setting[set].tau,setting[set].tcoeff);
spec=0;
while(setting[set].trim_spec[spec].e>0)
	{
	printf("e = %.0lf, vcmp = %.3lf\ndir = %s\n", setting[set].trim_spec[spec].e,
			setting[set].trim_spec[spec].vc, setting[set].trim_spec[spec].dir);
	spec++;
	}

set=HIGHG;
printf("settings = highG\n\
vrf = %lf\n\
slope = %lf\n\
tau = %g\n\
tempcoeff = %lf\n", 
setting[set].vrf, setting[set].slope, setting[set].tau,setting[set].tcoeff);
spec=0;
while(setting[set].trim_spec[spec].e>0)
	{
	printf("e = %.0lf, vcmp = %.3lf\ndir = %s\n", setting[set].trim_spec[spec].e,
			setting[set].trim_spec[spec].vc, setting[set].trim_spec[spec].dir);
	spec++;
	}

set=UHIGHG;
printf("settings = uhighG\n\
vrf = %lf\n\
slope = %lf\n\
tau = %g\n\
tempcoeff = %lf\n", 
setting[set].vrf, setting[set].slope, setting[set].tau,setting[set].tcoeff);
spec=0;
while(setting[set].trim_spec[spec].e>0)
	{
	printf("e = %.0lf, vcmp = %.3lf\ndir = %s\n", setting[set].trim_spec[spec].e,
			setting[set].trim_spec[spec].vc, setting[set].trim_spec[spec].dir);
	spec++;
	}
// =============================================
#endif


// pick up optional gain setting from typed line
q=ptr;
while(*q)
	{
	*q=tolower(*q);
	q++;
	}

if (isalpha(*ptr))
	{
	if (strstr(ptr, "uhighg"))
		new_set=UHIGHG;
	else if (strstr(ptr, "highg"))
		new_set=HIGHG;
	else if (strstr(ptr, "midg"))
		new_set=MIDG;
	else if (strstr(ptr, "lowg"))
		new_set=LOWG;
	else
		{
		sprintf(msg, "ERROR: unknown gain setting: %s", ptr);
		return -1;
		}
	}
else
	new_set=current_set;

if (!setting[new_set].nfile)
	{
	switch (new_set)
		{
		case LOWG:
			strcpy(line2, "low gain");
			break;
		case MIDG:
			strcpy(line2, "mid gain");
			break;
		case HIGHG:
			strcpy(line2, "high gain");
			break;
		case UHIGHG:
			strcpy(line2, "ultra high gain");
			break;
		default:
			strcpy(line2, "unknown gain");
			break;
		}
	sprintf(msg, "*** ERROR: no data available for %s", line2);
	return -1;
	}

// pick up requested threshold
while (*ptr && (isspace(*ptr) || isalpha(*ptr)) )
	ptr++;
if (isdigit(*ptr))
	new_th = atof(ptr);
else
	{
	printf("Threshold value not found\n");
	return -1;
	}

if (new_th < 1.9e3 || new_th > 3.0e4)
	{
	printf("Requested threshold (%.2lf eV) is out of range\n", new_th);
	return -1;
	}

if (new_set==current_set && new_th==current_th)
	return 1;				// nothing to do

current_set = new_set;
current_th = new_th;

switch (current_set)
	{
	case LOWG:
		strcpy(line2, "low gain");
		break;
	case MIDG:
		strcpy(line2, "mid gain");
		break;
	case HIGHG:
		strcpy(line2, "high gain");
		break;
	case UHIGHG:
		strcpy(line2, "ultra high gain");
		break;
	default:
		strcpy(line2, "unknown gain");
		break;
	}

printf("Requested setting: %s; threshold: %.0lf eV\n", line2, current_th);

// find the closest (in energy) available trim directory
ediff=1.0e9;
for(i=0; i<setting[current_set].nfile; i++)
	{
	e = fabs(current_th - setting[current_set].trim_spec[i].e);
	if (e < ediff)
		{
		current_tfile = i;
		ediff = e;
		}
	}

strcpy(trimpath, setting[current_set].trim_spec[current_tfile].dir);
printf("Trim directory:\n  %s\n", trimpath);

// get the vcmp reference value loaded from autotrim_b01_m01.dat
vcref = setting[current_set].trim_spec[current_tfile].vc;
if (vcref==0.0)
	{
	sprintf(msg, "*** ERROR: cannot get the reference value of vcmp");
	return -1;
	}

// find emax and emin for this energy setting; adjust current_th if needed
i=0;
vcmp = setting[current_set].trim_spec[i].vc;
emin = setting[current_set].trim_spec[i].e - (vcmp-1.0)/setting[current_set].slope;
if (current_th < emin)
	{
	current_th = emin;
	printf("Adjusting threshold to %.0lf (the minimum)\n", current_th);
	}
i=setting[current_set].nfile-1;
vcmp = setting[current_set].trim_spec[i].vc;
emax = setting[current_set].trim_spec[i].e - vcmp/setting[current_set].slope;
if (current_th > emax)
	{
	current_th = emax;
	printf("Adjusting threshold to %.0lf (the maximum)\n", current_th);
	}

// calculate vcmp to use
vcmp = setting[current_set].trim_spec[current_tfile].vc;
vcmp_set = vcmp + (current_th - setting[current_set].trim_spec[current_tfile].e) *
		setting[current_set].slope;

strcpy(line, "/tmp/setthreshold.cmd");
if ( (ofp = fopen(line, "w")) == NULL )
	{
	sprintf(msg, "*** ERROR: could not open %s for writing", line);
	return -1;
	}

strcpy(trim_directory, trimpath);				// system copy
camera_record_variable(Cam_trim_directory, trim_directory);
fprintf(ofp, "# set detector gain and threshold\n");
fprintf(ofp, "# %s  -  %s\n", DETECTOR, timestamp());
fprintf(ofp, "# Settings: %s; threshold: %.0lf eV; vcmp: %.3lf V\n", 
		line2, current_th, vcmp_set);
fprintf(ofp, "# Directory: %s\n", trimpath);
fprintf(ofp, "#\n");
fprintf(ofp, "dacoffset 0\n");		// turn off offset compensation
fprintf(ofp, "trim_all 63\n");		// safety measure
fprintf(ofp, "#\n");

// set the dacs
fprintf(ofp, "LdCmndFile %ssetdacs_b01_m01.dat\n", trimpath);
fprintf(ofp, "#\n");

// load the trim files
for (chip=0; chip<NCHIP; chip++)
	{
	fprintf(ofp, "trimfromfile %sautotrim_b01_m01_c%.2d.dat\n", trimpath, chip);
	fprintf(ofp, "prog b15_m07_chsel 0x0\n");
	}
fprintf(ofp, "#\n");

// set vcmp & vcca
vcsum = 0;
for (chip=0; chip<NCHIP; chip++)
	{
	sprintf(line3, "VCMP%d", chip);
	sprintf(line, "%sautotrim_b01_m01_c%.2d.dat", trimpath, chip);
	if ( (ifp=fopen(line, "r")) == NULL)
		{
		sprintf(msg, "*** ERROR: cannot open for reading:\n  %s", line);
		fclose(ofp);
		return -1;
		}
	while ( (fgets(line, sizeof(line), ifp) != NULL) )
		{
		if (line[0] == '#')
			continue;
		if ( (p=strstr(line, line3)) )
			{
			vc = atof(p+strlen(line3));
			break;
			}
		}
	fclose(ifp);
	if (vc==0.0)
		{
		sprintf(msg, "*** ERROR: cannot get b01_m01_%s", line3);
		fclose(ofp);
		return -1;
		}
	fprintf(ofp, "set B01_M01_VCMP%d %.4lf\n", chip, vcmp_set-vcref+vc);
	vcsum += vcmp_set-vcref+vc;
	}
vcca_set[1][1] = vcsum/NCHIP;
fprintf(ofp, "#\n");

// repeat for all other modules
for (bank=1; bank<=NBANK; bank++)
	for (mod=1; mod<=NMOD_BANK; mod++)
		{
		if (bank==1 && mod ==1)		// already done
			continue;

		// set the dacs
		fprintf(ofp, "LdCmndFile %ssetdacs_b%.2d_m%.2d.dat\n", trimpath, bank, mod);
		fprintf(ofp, "#\n");

		// load the trim files
		for (chip=0; chip<NCHIP; chip++)
			{
			fprintf(ofp, "trimfromfile %sautotrim_b%.2d_m%.2d_c%.2d.dat\n",
					trimpath, bank, mod, chip);
			fprintf(ofp, "prog b15_m07_chsel 0x0\n");
			}
		fprintf(ofp, "#\n");

		// set vcmp & vcca
		vcsum = 0;
		for (chip=0; chip<NCHIP; chip++)
			{
			sprintf(line3, "VCMP%d", chip);
			sprintf(line, "%sautotrim_b%.2d_m%.2d_c%.2d.dat", trimpath,
					bank, mod, chip);
			if ( (ifp=fopen(line, "r")) == NULL)
				{
				sprintf(msg, "*** ERROR: cannot open for reading:\n  %s", line);
				fclose(ofp);
				return -1;
				}
			while ( (fgets(line, sizeof(line), ifp) != NULL) )
				{
				if (line[0] == '#')
					continue;
				if ( (p=strstr(line, line3)) )
					{
					vc = atof(p+strlen(line3));
					break;
					}
				}
			fclose(ifp);
			if (vc==0.0)
				{
				sprintf(msg, "*** ERROR: cannot get b%.2d_m%.2d_%s", bank, mod, line3);
				fclose(ofp);
				return -1;
				}
			fprintf(ofp, "set B%.2d_M%.2d_VCMP%d %.4lf\n", bank, mod, chip,
					vcmp_set-vcref+vc); 
			vcsum += vcmp_set-vcref+vc;
			}
		vcca_set[bank][mod] = vcsum/NCHIP;
		fprintf(ofp, "#\n");
		}

switch (current_set)
	{
	case LOWG:
		strcpy(line, "LOWG");
		break;
	case MIDG:
		strcpy(line, "MIDG");
		break;
	case HIGHG:
		strcpy(line, "HIGHG");
		break;
	case UHIGHG:
		strcpy(line, "UHIGHG");
		break;
	default:
		line[0]='\0';
		break;
	}

for (bank=1; bank<=NBANK; bank++)
	for (mod=1; mod<=NMOD_BANK; mod++)
		fprintf(ofp, "set B%.2d_M%.2d_VCCA %.4lf\n", bank, mod, vcca_set[bank][mod]);
fprintf(ofp, "#\n");
fprintf(ofp, "prog b15_m07_chsel 0xffff\n");
fprintf(ofp, "imgmode p\n");
fprintf(ofp, "imgonly /tmp/trim_values_img.tif\n");
if (setting[current_set].trim_spec[current_tfile].ff[0])
	{
	fprintf(ofp, "LdFlatField %s\n", setting[current_set].trim_spec[current_tfile].ff);
	fprintf(ofp, "LdFlatField\n");		// print filename
	}
else
	fprintf(ofp, "LdFlatField 0\n");
if (setting[current_set].trim_spec[current_tfile].bpm[0])
	{
	fprintf(ofp, "LdBadPixMap %s\n", setting[current_set].trim_spec[current_tfile].bpm);
	fprintf(ofp, "LdBadPixMap\n");		// print filename
	}
else
	fprintf(ofp, "LdBadPixMap 0\n");
if (setting[current_set].trim_spec[current_tfile].extra[0])
	fprintf(ofp, "LdCmndFile %s\n", setting[current_set].trim_spec[current_tfile].extra);
fprintf(ofp, "#\n");
fprintf(ofp, "tau %.1lfe-9\n", 1.0e9*setting[current_set].tau);
fprintf(ofp, "imgmode x\n");
fprintf(ofp, "# Settings: %s: threshold: %.0lf eV; vcmp (b01_m01_c00): %.3lf V\n", 
		line, current_th, vcmp_set);
fprintf(ofp, "#\n");

fclose(ofp);

i=0;
i+=sprintf(msg+i, "# Settings: %s: threshold: %.0lf eV; vcmp (b01_m01_c00): %.3lf V\n", 
		line, current_th, vcmp_set);
i+=sprintf(msg+i, " /tmp/trim_values_img.tif was written\n");

camera_record_variable(Cam_trim_directory, trimpath);

sprintf(line+strlen(line), " %.0lf %.3lf %d",
		current_th, vcmp_set, current_tfile);
camera_record_variable(Cam_threshold_setting, line);

// tau will be updated when the macro runs

return 0;
}



/*****************************************************************************\
**                                                                           **
** set_calibration_state() from stored variables at camserver instantiation  **
**                                                                           **
\*****************************************************************************/

void set_calibration_state(char *p)
{
char *q;

q=p;
while(*q)
	{
	*q = tolower(*q);
	q++;
	}
if (strstr(p, "lowg"))
	current_set=LOWG;
else if (strstr(p, "midg"))
	current_set=MIDG;
else if (strstr(p, "highg"))
	current_set=HIGHG;
else if (strstr(p, "uhighg"))
	current_set=UHIGHG;
else
	{
	printf("Unrecognized: %s\n", p);
	return;			// give up
	}

q=p;
while(*q && !isspace(*q))
	q++;
sscanf(q, "%lf%lf%d", &current_th, &vcmp_set, &current_tfile);

if (trim_directory[0])
	strcpy(setting[current_set].trim_spec[current_tfile].dir, trim_directory);
else
	printf("set_calibration_state() did not get a valid trim directory\n");

return;
}



static CALIBS_CMND find_calibs_command(char **pP)
{
int i, j, n, idx=0;
char *p=*pP;

while(*p && (isspace(*p)))		// space to command
	p++;
if (*p == '\0')			// end of line?
	return 0;
for (n=0; *(p+n); n++)	// count characters in command
	if (!isalnum(*(p+n)) && *(p+n)!='_')	// allows '_'
		break;
j = 0;			// count matching names
for (i = 0; i < calibs_count; i++)
	if ( strncasecmp(p, calibs_list[i].name, n) == 0 )
		{
		idx = i;		// permit unambiguous abbreviations
		j++;
		if ( n == strlen(calibs_list[i].name) )
			{
			j = 1;		// to skip next block
			break;		// exact match is exempt from ambiguity check
			}
		}
if (j != 1)
	{
	for (i=0; *(p+i) && !isspace(*(p+i)); i++)
		;
	*(p+i) = '\0';		// isolate command
	if (j == 0)
		printf("Command not found: %s\n", p);
	else
		printf("Command '%s' is ambiguous\n", p);
	return 0;
	}
*pP += n;		// skip over command name
return calibs_list[idx].cmnd;
}


// get the reference vcmp for the current setting as the average over 
// all chips in bank 1, module 1 - of course, they may all be the same
static double read_vcmp(char *dir)
{
FILE *ifp;
char line[LENFN], *q;
double v=0.0;
int i;

for(i=0; i<16; i++)
	{
	strcpy(line, dir);
	sprintf(line+strlen(line), "autotrim_b01_m01_c%.2d.dat", i);
	if ( (ifp = fopen(line, "r")) == NULL)
		{
		printf("*** ERROR: could not open:\n  %s\n", line);
		return -1.0;
		}
	while (fgets(line, sizeof(line), ifp))
		if ( (q=strstr(line, "VCMP")) )
			{
			q = strchr(q, ' ');
			v += atof(q);
			break;
			}
	fclose (ifp);
	}

return v/16.0;
}



/*****************************************************************************\
**                                                                           **
**   get_current_gain - return gain setting in provided string               **
**                                                                           **
\*****************************************************************************/

char *get_current_gain(void)
{
// dummy for now
return current_gain;
}



/*****************************************************************************\
**                                                                           **
**   get_current_vrf - return vrf setting even if threshold not set          **
**                                                                           **
\*****************************************************************************/

double get_current_vrf()
{
// dummy for now

return 9.9;
}






