// mx_parms.c - display or adjust MX parameters


#define MX_PARAMS_MAIN		// turn on space allocations in mx_parms.h


#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <stdlib.h>
#include "tvxcampkg.h"
#include "debug.h"

/*****************************************************************************\
**                                                                           **
**    Global variables                                                       **
**                                                                           **
\*****************************************************************************/
#define TEXT_SIZE 20

static float mx_beam_x=(float)IMAGE_NCOL/2.0;
static float mx_beam_y=(float)IMAGE_NROW/2.0;
static int mx_n_oscilations=1;
static float mx_wavelength=1.54;
static float mx_det_distance=1.0;
static float mx_det_voffset=0.0;
static float mx_start_angle=0.0;
static float mx_angle_increment=0.1;
static float mx_det_2theta=0.0;
static float mx_polarization=0.99;
static float mx_alpha=0.0;
static float mx_kappa=0.0;
static float mx_phi=0.0;
static float mx_chi=0.0;
static float mx_flux=0.0;
static float mx_filter_tx=1.0;
static float mx_e_range_low=0.0;
static float mx_e_range_hi=0.0;
static char mx_oscillation_axis[TEXT_SIZE]="X, CW";

static int mx_params_activated=0;
static char msg[40];

/*****************************************************************************\
**                                                                           **
**      End of global variables                                              **
**                                                                           **
\*****************************************************************************/


/* ----- Functions defined in this module ---- */
static MX_PARAMS_CMND find_mx_param_command(char **);


/*
** There are 3 classes of parameters in the header:
**    Detector parameters (e.g., pixel size, tau)
**    General experimental paramters (e.g., detector distance)
**    Crystallography paramters (e.g., kappa)
**
** Detector parameters are not handled here, and some of them
** (e.g. pixel_size) cannot be changed by the user.
**
** There is code in rd_mx_parms.c to retrieve some of these paramters
** from image headers.  If any syntax changes are made here, they should
** also be mirrored there.
*/



/*
** Given: a string of one or more paramter settings separated by spaces
** or semicolons.  
** Returns 0 on success, n on syntax error, where n points to the error
*/

int parse_mx_param_string(char *ptr)
{
char *p=ptr;
int i;
MX_PARAMS_CMND cmnd;

while (*p)
	{
	if ( !(cmnd = find_mx_param_command(&p)) )
		{
		printf("%s\n", msg);
		return 0;
		}
	mx_params_activated=1;
	while(*p && (isspace(*p) || *p=='='))	// skip to data
		p++;
	if (*p == '\0')			// end of line?
		return 0;

	/* start of main loop switch - please preserve the format for mkheader */
	switch (cmnd)
		{
		case Wavelength:
			mx_wavelength = atof(p);
			break;
		case Energy_range:
			mx_e_range_low = atof(p);
			while(*p && (isdigit(*p) || *p=='-' || *p=='.'))	// skip number
				p++;
			while(*p && (isspace(*p) || *p==','))	// skip to next number
				p++;
			if (*p == '\0')			// end of line?
				return 0;
			mx_e_range_hi = atof(p);
			break;
		case Detector_distance:
			mx_det_distance = atof(p);
			break;
		case Detector_Voffset:
			mx_det_voffset = atof(p);
			break;
		case Beam_xy:
			mx_beam_x = atof(p);
			while(*p && (isdigit(*p) || *p=='-' || *p=='.'))	// skip number
				p++;
			while(*p && (isspace(*p) || *p==','))	// skip to next number
				p++;
			if (*p == '\0')			// end of line?
				return 0;
			mx_beam_y = atof(p);
			break;
		case Flux:
			mx_flux = atof(p);
			break;
		case Filter_transmission:
			mx_filter_tx = atof(p);
			break;
		case Start_angle:
			mx_start_angle = atof(p);
			break;
		case Angle_increment:
			mx_angle_increment = atof(p);
			break;
		case Detector_2theta:
			mx_det_2theta = atof(p);
			break;
		case Polarization:
			mx_polarization = atof(p);
			break;
		case Alpha:
			mx_alpha = atof(p);
			break;
		case Kappa:
			mx_kappa = atof(p);
			break;
		case Phi:
			mx_phi = atof(p);
			break;
		case Chi:
			mx_chi = atof(p);
			break;
		case Oscillation_axis:
			memset(mx_oscillation_axis, 0, TEXT_SIZE);
			mx_oscillation_axis[0] = *p++;
			if (*p == ' ')
				strcat(mx_oscillation_axis, ",");
			strncat(mx_oscillation_axis, p, TEXT_SIZE-2);
			for (i=0; mx_oscillation_axis[i] && i<TEXT_SIZE-1; i++)
				mx_oscillation_axis[i] = toupper(mx_oscillation_axis[i]);
			while (*p && *p!=';')
				p++;
			break;
		case N_oscillations:
			mx_n_oscilations = atoi(p);
			break;
		default:
			break;
		}
		/* end of main loop switch */
	// skip over number
	while(*p && (isdigit(*p) || isspace(*p) || *p=='-' || *p=='.' ||
			*p=='e' || *p=='E' || *p=='+'))
		p++;
	}
return 0;
}




/*
** Print the current parameter settings in a format suitable as
** a comment in CBF or other headers.   Result in buffer provided
** by the caller.
** Return value: N of characters formatted
*/

int format_mx_params(char *ptr, int size)
{
int n;

if(!mx_params_activated)		// facility not in use
	return 0;

// first pick up the non-mx params
n = format_non_mx_params(ptr, size);

{
int i;
for(i=0; i<mx_params_count; i++)
	{
	if (n>size-20)
		return n;
	switch (mx_params_list[i].cmnd)
		{
		case Start_angle:
			n += sprintf(ptr+n, "# %s ", mx_params_list[i].name);
			n += sprintf(ptr+n, "%.4f deg.\r\n", mx_start_angle);
			break;
		case Angle_increment:
			n += sprintf(ptr+n, "# %s ", mx_params_list[i].name);
			n += sprintf(ptr+n, "%.4f deg.\r\n", mx_angle_increment);
			break;
		case Detector_2theta:
			n += sprintf(ptr+n, "# %s ", mx_params_list[i].name);
			n += sprintf(ptr+n, "%.4f deg.\r\n", mx_det_2theta);
			break;
		case Alpha:
			n += sprintf(ptr+n, "# %s ", mx_params_list[i].name);
			n += sprintf(ptr+n, "%.4f deg.\r\n", mx_alpha);
			break;
		case Polarization:
			n += sprintf(ptr+n, "# %s ", mx_params_list[i].name);
			n += sprintf(ptr+n, "%.3f\r\n", mx_polarization);
			break;
		case Kappa:
			n += sprintf(ptr+n, "# %s ", mx_params_list[i].name);
			n += sprintf(ptr+n, "%.4f deg.\r\n", mx_kappa);
			break;
		case Phi:
			n += sprintf(ptr+n, "# %s ", mx_params_list[i].name);
			n += sprintf(ptr+n, "%.4f deg.\r\n", mx_phi);
			break;
		case Chi:
			n += sprintf(ptr+n, "# %s ", mx_params_list[i].name);
			n += sprintf(ptr+n, "%.4f deg.\r\n", mx_chi);
			break;
		case Oscillation_axis:
			n += sprintf(ptr+n, "# %s ", mx_params_list[i].name);
			n += sprintf(ptr+n, "%s\r\n", mx_oscillation_axis);
			break;
		case N_oscillations:
			n += sprintf(ptr+n, "# %s ", mx_params_list[i].name);
			n += sprintf(ptr+n, "%d\r\n", mx_n_oscilations);
			break;
		default:
			break;
		}
	}
}

return n;
}



/*
** Print the susbset of parameters needed for non-crystallographic applications
*/


int format_non_mx_params(char *ptr, int size)
{
int i, n=0;

for(i=0; i<mx_params_count; i++)
	{
	if (n>size-20)
		return n;
	switch (mx_params_list[i].cmnd)
		{
		case Wavelength:
			n += sprintf(ptr+n, "# %s ", mx_params_list[i].name);
			n += sprintf(ptr+n, "%.4f A\r\n", mx_wavelength);
			break;
		case Energy_range:
			n += sprintf(ptr+n, "# %s ", mx_params_list[i].name);
			n += sprintf(ptr+n, "(%.0f, %.0f) eV\r\n", mx_e_range_low, mx_e_range_hi);
			break;
		case Detector_distance:
			n += sprintf(ptr+n, "# %s ", mx_params_list[i].name);
			n += sprintf(ptr+n, "%.5f m\r\n", mx_det_distance);
			break;
		case Detector_Voffset:
			n += sprintf(ptr+n, "# %s ", mx_params_list[i].name);
			n += sprintf(ptr+n, "%.5f m\r\n", mx_det_voffset);
			break;
		case Beam_xy:
			n += sprintf(ptr+n, "# %s ", mx_params_list[i].name);
			n += sprintf(ptr+n, "(%.2f, %.2f) pixels\r\n", mx_beam_x, mx_beam_y);
			break;
		case Flux:
			n += sprintf(ptr+n, "# %s ", mx_params_list[i].name);
			n += sprintf(ptr+n, "%#.5g ph/s\r\n", mx_flux);
			break;
		case Filter_transmission:
			n += sprintf(ptr+n, "# %s ", mx_params_list[i].name);
			n += sprintf(ptr+n, "%.4f\r\n", mx_filter_tx);
			break;
		default:
			break;
		}

	}

return n;
}



/*
** Print a single parameter
*/


int format_mx_single(char *ptr, char *p)
{
int n=0;
MX_PARAMS_CMND cmnd;

if ( !(cmnd = find_mx_param_command(&p)) )
	{
	strcpy(ptr, msg);
	return 0;
	}

n += sprintf(ptr+n, "# %s ", mx_params_list[-1+(int)cmnd].name);
switch (cmnd)
	{
	case Wavelength:
		n += sprintf(ptr+n, "%.2f A", mx_wavelength);
		break;
	case Energy_range:
		n += sprintf(ptr+n, "(%.0f, %.0f) eV\r\n", mx_e_range_low, mx_e_range_hi);
		break;
	case Detector_distance:
		n += sprintf(ptr+n, "%.3f m", mx_det_distance);
		break;
	case Detector_Voffset:
		n += sprintf(ptr+n, "%.3f m", mx_det_voffset);
		break;
	case Beam_xy:
		n += sprintf(ptr+n, "(%.2f, %.2f) pixels", mx_beam_x, mx_beam_y);
		break;
	case Flux:
		n += sprintf(ptr+n, "%g ph/s", mx_flux);
		break;
	case Filter_transmission:
		n += sprintf(ptr+n, "%.4f", mx_filter_tx);
		break;
	case Start_angle:
		n += sprintf(ptr+n, "%.4f deg.", mx_start_angle);
		break;
	case Angle_increment:
		n += sprintf(ptr+n, "%.4f deg.", mx_angle_increment);
		break;
	case Detector_2theta:
		n += sprintf(ptr+n, "%.4f deg.", mx_det_2theta);
		break;
	case Polarization:
		n += sprintf(ptr+n, "%.3f", mx_polarization);
		break;
	case Alpha:
		n += sprintf(ptr+n, "%.4f deg.", mx_alpha);
		break;
	case Kappa:
		n += sprintf(ptr+n, "%.4f deg.", mx_kappa);
		break;
	case Phi:
		n += sprintf(ptr+n, "%.4f deg.", mx_phi);
		break;
	case Chi:
		n += sprintf(ptr+n, "%.4f deg.", mx_chi);
		break;
	case Oscillation_axis:
		n += sprintf(ptr+n, "%s\r\n", mx_oscillation_axis);
		break;
	case N_oscillations:
		n += sprintf(ptr+n, "%d", mx_n_oscilations);
		break;
	default:
		n += sprintf(ptr+n, "**command not in list**");
		break;
	}

return n;
}



/*
** Increment recorded mx_start_angle by given multiplicity
*/
void inc_mx_start_angle(int mult)
{
mx_start_angle += mx_angle_increment;
return;
}



static MX_PARAMS_CMND find_mx_param_command(char **pP)
{
int i, j, n, idx=0;
char *p=*pP;

msg[0]='\0';
while(*p && (isspace(*p) || *p==';' || *p==','))	// space to next command
	p++;
if (*p == '\0')			// end of line?
	return 0;
for (n=0; *(p+n); n++)	// count characters in command
	if (!isalnum(*(p+n)) && *(p+n)!='_')	// allows '_'
		break;
j = 0;			// count matching names
for (i = 0; i < mx_params_count; i++)
	if ( strncasecmp(p, mx_params_list[i].name, n) == 0 )
		{
		idx = i;		// permit unambiguous abbreviations
		j++;
		if ( n == strlen(mx_params_list[i].name) )
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
		sprintf(msg, "Command not found: %s", p);
	else
		sprintf(msg, "Command '%s' is ambiguous", p);
	return 0;
	}
*pP += n;		// skip over command name
return mx_params_list[idx].cmnd;
}
