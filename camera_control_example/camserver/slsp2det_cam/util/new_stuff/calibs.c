// calibs.c - show or set energy and threshold calibrations

#define CALIBS_MAIN		// turn on space allocations in calibs.h


#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>
#include "tvxcampkg.h"
#include "camserver.h"
#include "debug.h"



typedef enum {		// do not change the order - used as indexes
		ULOWG,
		LOWG,
		MIDG,
		HIGHG,
		UHIGHG,
		GAIN1,
		GAIN2,
		GAIN3,
		GAIN4,
		GAIN5,
		GAIN6,
		GAIN7,
		GAIN8,
		GAIN9,
		UNKNOWNG
		} setting_type;		// gain settings


/*****************************************************************************\
**                                                                           **
**    Local functions                                                        **
**                                                                           **
\*****************************************************************************/

static CALIBS_CMND find_calibs_command(char **);
static void read_vcmps(char *dir, int bank, int module, float *vcmps);
static double read_vcmp(char *);
static int read_calibration (char *);
static int load_bin_file(char *);
static char  *setting2string(setting_type, int);
static setting_type string2setting(char *);


/*****************************************************************************\
**                                                                           **
**    Static variables                                                       **
**                                                                           **
\*****************************************************************************/

#define SPEC_MAX 10

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
			} setting[1+(int)GAIN9];		// LOWG, MIDG, etc.

static setting_type current_set=MIDG;
static char current_gain[40]="not set";
static int current_tfile=0;
static double current_th=0.0;
static double vcmp_set=0.0;
static detector_trim_dir[LENFN]="\0";
static int data_struct_initialized=0;
static char detector_name[20]="\0";


/*****************************************************************************\
**                                                                           **
**   show_calibration - show the current calibration setup                   **
**                                                                           **
\*****************************************************************************/

void show_calibration(char *buf)
{
int i=0;

if (current_th == 0.0)
	{
	strcpy(buf, "Threshold has not been set");
	return;
	}

i+=sprintf(buf+i, "Settings: %s; threshold: %.0lf eV; vcmp: %.3lf V\n", 
		get_current_gain(), current_th, vcmp_set);
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
**   get_current_gain - return gain setting in provided string               **
**                                                                           **
\*****************************************************************************/

char *get_current_gain(void)
{
if (current_th == 0.0)
	return current_gain;

strcpy(current_gain, setting2string(current_set, 0));

return current_gain;
}



/*****************************************************************************\
**                                                                           **
**   get_current_vrf - return vrf setting even if threshold not set          **
**                                                                           **
\*****************************************************************************/

double get_current_vrf()
{
char line[40];

if (current_th == 0.0)		// threshold not set - get vrf from system
	{
	get_vrfall(line, 1, 1);
	return atof(line);
	}

return setting[current_set].vrf;
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
**   read_calibration() - load info from calibration.def                     **
**                                                                           **
\*****************************************************************************/

static int read_calibration (char *msg)
{
FILE *ifp;
char *p, *q, line[80+LENFN];
int spec=0;
double e, vc=0.0;
setting_type set=MIDG;

if (data_struct_initialized)
	return 0;				// nothing needed

// read the calibration file & get parameters
if ( (ifp=fopen(detector_calibration_file, "r")) == NULL)
	{
	printf("Could not open for reading:\n  %s\n", detector_calibration_file);
	return -1;
	}

memset(setting, 0, sizeof(setting));
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
		case Detector:
			if ( (q=strchr(p, ' ') )
				*q='\0';
			strncpy(detector_name, p, -1+sizeof(detector_name));
			if (detector_name[0]!='p' || !isdigit(detector_name[1]))
				{
				strcpy(msg, "*** Invalid detector name: %s", detector_name);
				detector_name[0]='\0';
				return -1;
				}
			break;
		case Settings:
			if ( (set = string2setting(p)) == UNKNOWNG )
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
for(set=ULOWG; set<UNKNOWNG; set++)
	{
	if (!setting[set].nfile)			// if this set is unpopulated, 
		continue;
	for(e=0, vc=1.1, spec=0; setting[set].nfile && spec<SPEC_MAX; spec++)
		{
		if (setting[set].trim_spec[spec].e == 0.0)
			break;
		if (setting[set].trim_spec[spec].e < e)
			{
			printf("Error in %s: energy %lf out of order\n",
					setting2string(set, 1),
					setting[set].trim_spec[spec].e);
			return -1;
			}
		e = setting[set].trim_spec[spec].e;
		if (setting[set].trim_spec[spec].vc < vc)
			vc = setting[set].trim_spec[spec].vc;
		else
			{
			printf(" Data for %s at energy %.0lf: vcmp = %.3lf makes no sense\n", 
					setting2string(set, 0), e, setting[set].trim_spec[spec].vc);
			return -1;
			}
		}
	}

#if 1
// Verify ==============================

for(set=ULOWG; set<UNKNOWNG; set++)
	{
	if (!setting[set].nfile)			// if this set is unpopulated, 
		continue;
	printf("settings = %s\n\
	vrf = %lf\n\
	slope = %lf\n\
	tau = %g\n\
	tempcoeff = %lf\n",
			setting2string(set, 2),
			setting[set].vrf,
			setting[set].slope,
			setting[set].tau,
			setting[set].tcoeff);
	spec=0;
	while(setting[set].trim_spec[spec].e>0)
		{
		printf("e = %.0lf, vcmp = %.3lf\ndir = %s\n", setting[set].trim_spec[spec].e,
				setting[set].trim_spec[spec].vc, setting[set].trim_spec[spec].dir);
		spec++;
		}
	}

// =============================================
#endif

data_struct_initialized = 1;
return 0;
}



/*****************************************************************************\
**                                                                           **
**    load_bin_file() - trim detector from existing binary file              **
**                                                                           **
\*****************************************************************************/

static int load_bin_file(char * fn)
{
FILE *ofp;


/* FIXME */
// open binary file, read it, check that name & dimensions match
// do something about differing flat-field & bad_pix maps

// it is known that the binary trim file exists
// make up the command macro to load it

// open output file
strcpy(line, "/tmp/setthreshold.cmd");
if ( (ofp = fopen(line, "w")) == NULL )
	{
	sprintf(msg, "*** ERROR: could not open %s for writing", line);
	return -1;
	}

/* FIXME */
strcpy(trim_directory, trimpath[0]);				// system copy
fprintf(ofp, "# set detector gain and threshold\n");
fprintf(ofp, "# %s  -  %s\n", DETECTOR, timestamp());
fprintf(ofp, "# Settings: %s; threshold: %.0lf eV; vcmp: %.3lf V\n", 
		setting2string(current_set, 0), current_th, vcmp_set);
fprintf(ofp, "# Directory: %s\n", trimpath[0]);
fprintf(ofp, "#\n");
fprintf(ofp, "dacoffset 0\n");		// turn off offset compensation
fprintf(ofp, "trim_all 63\n");		// safety measure
fprintf(ofp, "#\n");

/*FIXME*/

		fprintf(ofp, "trimfrombinfile /tmp/autotrim_b%.2d_m%.2d.bin\n", bank, mod);

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
		setting2string(current_set, 2), current_th, vcmp_set);
fprintf(ofp, "#\n");

fclose(ofp);



return 0;
}



/*****************************************************************************\
**                                                                           **
**   set_calibration_interpolate - set up a detector calibration script      **
**                                                                           **
\*****************************************************************************/

int set_calibration_interpolate(char *ptr, char *msg)
{
char line[80+LENFN], *p, *q, trimpath[2][LENFN], line2[120], line3[120];
char trimbin[LENFN];
int spec=0, i, j, bank, mod, chip, nerr, fd;
FILE *ifp, *ofp, *tfp[2];
CALIBS_CMND cmnd;
setting_type set=MIDG, new_set;
double e, emin, emax, vc=0.0, new_th, vcsum;
float dac;
int cs,tf0,tf1,ir,jr,it,noBin;
double e0,e1,v0,v1;
struct TRIMBIN trims[3];
struct DETTRIM det_trim;
struct stat statbuf;

// pick up optional gain setting from typed line
if (isalpha(*ptr) && ((new_set = string2setting(ptr)) == UNKNOWNG) )
	{
	sprintf(msg, "ERROR: unknown gain setting: %s", ptr);
	return -1;
	}
else
	new_set=current_set;		// previous set

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

// round new threshold to 25 eV
new_th = 25.0*(rint((new_th+12.5)/25.0));

if (new_set==current_set && new_th==current_th)
	return 1;				// nothing to do

if (read_calibration(msg))
	return -1;

if (!detector_name[0])		// for bin file procedure, a name is required
	return -1;

if (!setting[new_set].nfile)
	{
	sprintf(msg, "*** ERROR: no data available for %s",
			setting2string(new_set, 0));
	return -1;
	}

// set detector_trim_dir from known data
strcpy(detector_trim_dir, setting[new_set].trim_spec[spec].dir);
q = strrchr(detector_trim_dir, '/');
*(q+1) = '\0';

if (!setting[new_set].nfile)
	{
	sprintf(msg, "*** ERROR: no data available for %s",
			setting2string(new_set, 0));
	return -1;
	}

// new_set & new_th have been set above

current_set = new_set;
current_th = new_th;
/* FIXME _ this gets scrolled away */
printf("Requested setting: %s; threshold: %.0lf eV\n", 
		setting2string(current_set, 0), current_th);

// bin file may already exist
// make up file name - e.g., p300KWF0103_T8225_vrf_m0p12.bin
sprintf(trimbin, "%s%s_T%d_vrf_m0p%.2d.bin", 
		detector_trim_dir, detector_name,
		(int)new_th, (int)fabs(setting[new_set].vrf));
if (!stat(trimbin, &statbuf))
	return load_bin_file(trimbin);		// file exists - load it

// must make up the binary file
// find the lower energy trim directory
current_tfile = 0;
for(i=0; i<setting[current_set].nfile; i++)
	{
	if (current_th > setting[current_set].trim_spec[i].e)
		current_tfile = i;
	}

// initialize settings and trim dirs
cs = current_set;
if (current_tfile<setting[current_set].nfile-1) 
	{
	tf0 = current_tfile;
	tf1 = current_tfile+1;
	}
else
	{
	tf0 = current_tfile-1;
	tf1 = current_tfile;
	}
e0 = setting[current_set].trim_spec[tf0].e;
e1 = setting[current_set].trim_spec[tf1].e;
strcpy(trimpath[0], setting[current_set].trim_spec[tf0].dir);
strcpy(trimpath[1], setting[current_set].trim_spec[tf1].dir);

printf("Trim directories:\n  %s\n  %s\n", trimpath[0], trimpath[1]);

// find emax and emin for this energy setting; adjust current_th if needed
i=0;
setting[cs].trim_spec[tf0].vc = read_vcmp(setting[cs].trim_spec[tf0].dir);
setting[cs].trim_spec[tf1].vc = read_vcmp(setting[cs].trim_spec[tf1].dir);
v0 = setting[cs].trim_spec[tf0].vc;
v1 = setting[cs].trim_spec[tf1].vc;
emin = e1;
if (fabs(v1-v0)>1e-10)
	emin = e1 + (0.9 - v1) * (e1 - e0) / (v1 - v0);
if (current_th < emin)
	{
	current_th = emin;
	printf("Adjusting threshold to %.0lf (the minimum)\n", current_th);
	}
emax = e1;
if (fabs(v1-v0)>1e-10)
	emax = e1 + (0 - v1) * (e1 - e0) / (v1 - v0);
if (current_th > emax)
	{
	current_th = emax;
	printf("Adjusting threshold to %.0lf (the maximum)\n", current_th);
	}
vcmp_set = v0;
if (fabs(e1-e0)>1e-10)
	vcmp_set = v0 + (v1-v0) * (current_th - e0) / (e1 - e0);

// repeat for all modules
for (bank=1; bank<=NBANK; bank++)
	{
	for (mod=1; mod<=NMOD_BANK; mod++)
		{
/* FIXME - we are here to make binary file */
		noBin = 0;
		// read binary files
		for (i=0;i<2;i++)
			{
			sprintf(line,"%sautotrim_b%.2d_m%.2d.bin",trimpath[i], bank, mod);
			if ( (tfp[i]=fopen(line, "r")) == NULL)
				{
				noBin = 1;
				break;
				}
			fread(&trims[i], sizeof(trims[i]), 1, tfp[i]);
			fclose(tfp[i]);
			}
		
		// read ascii files (old style)
		if (noBin) 
			{
			// read the dacs
			for (i=0;i<2;i++)
				{
				sprintf(line,"%ssetdacs_b%.2d_m%.2d.dat", trimpath[i], bank, mod);
				if ( (ifp=fopen(line, "r")) == NULL)
					{
					printf("Could not open autotrim file:\n  %s\n", line);
					return -1;
					}
				while ( (fgets(line, sizeof(line), ifp) != NULL) )
					{
					if (strstr(line,"set ")==NULL)
						continue;
					dac = 0;
					sscanf(line,"%s %s %f",line2,line3,&dac);
					if (strstr(line,"VTRM")!=NULL)
						trims[i].vtrm = dac;
					else if (strstr(line,"VRF ")!=NULL)
						trims[2].vrf = dac;
					else if (strstr(line,"VRFS")!=NULL)
						trims[2].vrfs = dac;
					else if (strstr(line,"VCAL")!=NULL)
						trims[2].vcal = dac;
					else if (strstr(line,"VDEL")!=NULL)
						trims[2].vdel = dac;
					else if (strstr(line,"VADJ")!=NULL)
						trims[2].vadj = dac;
					}
				fclose(ifp);
				}

			// read vcmp & vcca
			read_vcmps(setting[cs].trim_spec[tf0].dir,bank,mod,trims[0].vcmp);
			read_vcmps(setting[cs].trim_spec[tf1].dir,bank,mod,trims[1].vcmp);
		
			// read the trim files
			for (chip=0; chip<NCHIP; chip++)
				{
				for (i=0;i<2;i++)
					{
					sprintf(line,"%sautotrim_b%.2d_m%.2d_c%.2d.dat",trimpath[i], bank, mod, chip);
					if ( (tfp[i]=fopen(line, "r")) == NULL)
						{
						printf("Could not open autotrim file:\n  %s\n", line);
						return -1;
						}
					while ( (fgets(line, sizeof(line), tfp[i]) != NULL) )
						{
						if (strstr(line,"trim ")==NULL)
							continue;
						strcpy(line2,"");
						sscanf(line,"%s %d %d %x",line2,&ir,&jr,&it);
						trims[i].tb[chip][jr][ir] = it;
						}
					fclose(tfp[i]);
					}
				}
			}

		// interpolate trim values			
		for (chip=0; chip<NCHIP; chip++)
			for (i=0;i<NROW_CHIP;i++) 
				for (j=0;j<NCOL_CHIP;j++) 
					{
					trims[2].tb[chip][i][j] = trims[0].tb[chip][i][j];
					if (fabs(e1-e0)>1e-10)
						trims[2].tb[chip][i][j] = trims[0].tb[chip][i][j] +
							(trims[1].tb[chip][i][j]-trims[0].tb[chip][i][j]) * 
							(current_th - e0) / (e1 - e0);
					trims[2].tb[chip][i][j] = max(min(trims[2].tb[chip][i][j],63),0);
					}

		// interpolate dacs
		trims[2].vtrm = trims[0].vtrm;
		if (fabs(e1-e0)>1e-10)
			trims[2].vtrm = trims[0].vtrm + (trims[1].vtrm - trims[0].vtrm) * 
						(current_th - e0) / (e1 - e0);
		
		vcsum = 0;
		for (chip=0; chip<NCHIP; chip++)
			{
			trims[2].vcmp[chip] = trims[0].vcmp[chip];
			if (fabs(e1-e0)>1e-10)
				trims[2].vcmp[chip] = trims[0].vcmp[chip]+
					(trims[1].vcmp[chip]-trims[0].vcmp[chip]) * 
					(current_th - e0) / (e1 - e0);
			vcsum += trims[2].vcmp[chip];
			}
		trims[2].vcca = vcsum/NCHIP;
		if (bank==1 && mod==1)
			vcmp_set = trims[2].vcmp[0];

		// write interpolated trim file
		trims[2].bank = bank;
		trims[2].mod = mod;

		//  copy the result to the full detector structure
		memcpy(&det_trim.module_data[bank-1][mod-1], &trims[2], sizeof(trims[2]));
		}
	}

// Fill in the rest of the detector structure
strcpy(det_trim.detector_name, detector_name);
det_trim.npix_x = NCOL_DET;
det_trim.npix_y = NROW_DET;
strcpy(det_trim.flat_field_loE, setting[current_set].trim_spec[tf0].ff);
strcpy(det_trim.flat_field_hiE, setting[current_set].trim_spec[tf1].ff);
strcpy(det_trim.bad_pix_loE, setting[current_set].trim_spec[tf0].bpm);
strcpy(det_trim.bad_pix_hiE, setting[current_set].trim_spec[tf1].bpm);

if ((fd = open(trimbin, O_WRONLY | O_CREAT | O_TRUNC,
				S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH)) == -1)
	{
	sprintf(msg, "Could not open file for writing:\n  %s\n", trimbin);
	printf("*** ERROR %s\n", msg);
	return -1;
	}
write(fd, &det_trim, sizeof(det_trim));
close(fd);

// binary trim file now exists - load it
nerr = load_bin_file(trimbin);		// file exists - load it
	
i=0;
i+=sprintf(msg+i, "# Settings: %s: threshold: %.0lf eV; vcmp (b01_m01_c00): %.3lf V\n", 
		setting2string(current_set, 2), current_th, vcmp_set);
i+=sprintf(msg+i, " /tmp/trim_values_img.tif was written\n");

/*FIXME*/
camera_record_variable(Cam_trim_directory, trimpath[0]);

sprintf(line+strlen(line), " %.0lf %.3lf %d",
		current_th, vcmp_set, current_tfile);
camera_record_variable(Cam_threshold_setting, line);

// tau will be updated when the macro runs

return nerr;
}



/*****************************************************************************\
**                                                                           **
** set_calibration_state() from stored variables at camserver instantiation  **
**                                                                           **
\*****************************************************************************/

void set_calibration_state(char *p)
{
char *q;

if ( (current_set = string2setting(p)) == UNKNOWNG )
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



/*****************************************************************************\
**                                                                           **
\*****************************************************************************/

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



// get the reference vcmps of all chips for the current setting 
// in bank 1, module 1
static void read_vcmps(char *dir, int bank, int module, float *vcmps)
{
FILE *ifp;
char line[LENFN], *q;
int i;

for(i=0; i<16; i++)
	vcmps[i] = 0.0;
for(i=0; i<16; i++)
	{
	strcpy(line, dir);
	sprintf(line+strlen(line), "autotrim_b%02d_m%02d_c%.2d.dat", bank, module, i);
	if ( (ifp = fopen(line, "r")) == NULL)
		{
		printf("*** ERROR: could not open:\n  %s\n", line);
		return;
		}
	while (fgets(line, sizeof(line), ifp))
		if ( (q=strstr(line, "VCMP")) )
			{
			q = strchr(q, ' ');
			vcmps[i] += atof(q);
			break;
			}
	fclose (ifp);
	}
}


// convert a gain setting to its string equivalent
// type==0 - long form
// type==1 - short form, lower case
// type==2 - short form, upper case
static char str[20];
static char *setting2string(setting_type set, int type)
{
char *p=str;
if (type==0)							// long string form
	switch (set)
		{
		case ULOWG:
			strcpy(str, "ultra low gain");
			break;
		case LOWG:
			strcpy(str, "low gain");
			break;
		case MIDG:
			strcpy(str, "mid gain");
			break;
		case HIGHG:
			strcpy(str, "high gain");
			break;
		case UHIGHG:
			strcpy(str, "ultra high gain");
			break;
		case GAIN1:
			strcpy(str, "gain1");
			break;
		case GAIN2:
			strcpy(str, "gain2");
			break;
		case GAIN3:
			strcpy(str, "gain3");
			break;
		case GAIN4:
			strcpy(str, "gain4");
			break;
		case GAIN5:
			strcpy(str, "gain5");
			break;
		case GAIN6:
			strcpy(str, "gain6");
			break;
		case GAIN7:
			strcpy(str, "gain7");
			break;
		case GAIN8:
			strcpy(str, "gain8");
			break;
		case GAIN9:
			strcpy(str, "gain9");
			break;
		case UNKNOWNG:
		default:
			str[0]='\0';
			break;
		}
else								// short string form
	{
	switch (set)
		{
		case ULOWG:
			strcpy(str, "ulowg");
			break;
		case LOWG:
			strcpy(str, "lowg");
			break;
		case MIDG:
			strcpy(str, "midg");
			break;
		case HIGHG:
			strcpy(str, "highg");
			break;
		case UHIGHG:
			strcpy(str, "uhighg");
			break;
		case GAIN1:
			strcpy(str, "gain1");
			break;
		case GAIN2:
			strcpy(str, "gain2");
			break;
		case GAIN3:
			strcpy(str, "gain3");
			break;
		case GAIN4:
			strcpy(str, "gain4");
			break;
		case GAIN5:
			strcpy(str, "gain5");
			break;
		case GAIN6:
			strcpy(str, "gain6");
			break;
		case GAIN7:
			strcpy(str, "gain7");
			break;
		case GAIN8:
			strcpy(str, "gain8");
			break;
		case GAIN9:
			strcpy(str, "gain9");
			break;
		case UNKNOWNG:
		default:
			str[0] = '\0';
			break;
		}
	if (type==2)
		for( ; *p; p++)
			*p = toupper(*p);
	}
return str;
}




// interpret a string as a setting_type (gain designation)
static setting_type string2setting(char *str)
{
if (!strncasecmp(str, "gain", strlen("gain")))
	switch (atoi(str[strlen("gain")]))
		{
		case 1:
			return GAIN1;
		case 2:
			return GAIN2;
		case 3:
			return GAIN3;
		case 4:
			return GAIN4;
		case 5:
			return GAIN5;
		case 6:
			return GAIN6;
		case 7:
			return GAIN7;
		case 8:
			return GAIN8;
		case 9:
			return GAIN9;
		default:
			return UNKNOWNG;
		}
if (strchr(str, ' '))		// multiword string
	{
	if (strncasecmp(str, "ultra low gain", strlen("ultra low gain")))
		return ULOWG;
	if (strncasecmp(str, "low gain", strlen("low gain")))
		return LOWG;
	if (strncasecmp(str, "mid gain", strlen("mid gain")))
		return MIDG;
	if (strncasecmp(str, "high gain", strlen("high gain")))
		return HIGHG;
	if (strncasecmp(str, "ultra high gain", strlen("ultra high gain")))
		return UHIGHG;
	}
if (strncasecmp(str, "ulowg", strlen("ulowg")))
	return ULOWG;
if (strncasecmp(str, "lowg", strlen("lowg")))
	return LOWG;
if (strncasecmp(str, "midg", strlen("midg")))
	return MIDG;
if (strncasecmp(str, "highg", strlen("highg")))
	return HIGHG;
if (strncasecmp(str, "uhighg", strlen("uhighg")))
	return UHIGHG;

return UNKNOWNG;
}




