// mx_parms.h - declarations for parsing MX paramters

#ifndef MX_PARAMS_H
#define MX_PARAMS_H

// prototypes of functions
int parse_mx_param_string(char *);
int format_mx_params(char *, int);
int format_non_mx_params(char *, int);
int format_mx_single(char *, char *);
void inc_mx_start_angle(int);


/* start of MX_PARAMS enum - please do not change this comment - lines below will change */
typedef enum {
		Wavelength = 1,
		Energy_range,
		Detector_distance,
		Detector_Voffset,
		Beam_xy,
		Flux,
		Filter_transmission,
		Start_angle,
		Angle_increment,
		Detector_2theta,
		Polarization,
		Alpha,
		Kappa,
		Phi,
		Chi,
		Oscillation_axis,
		N_oscillations,
		} MX_PARAMS_CMND ;

typedef struct
		{
		MX_PARAMS_CMND cmnd;
		char *name;
		} MX_PARAMS_LIST ;

#ifdef MX_PARAMS_MAIN

static MX_PARAMS_LIST mx_params_list[] =
		{
		{ Wavelength, "Wavelength" },
		{ Energy_range, "Energy_range" },
		{ Detector_distance, "Detector_distance" },
		{ Detector_Voffset, "Detector_Voffset" },
		{ Beam_xy, "Beam_xy" },
		{ Flux, "Flux" },
		{ Filter_transmission, "Filter_transmission" },
		{ Start_angle, "Start_angle" },
		{ Angle_increment, "Angle_increment" },
		{ Detector_2theta, "Detector_2theta" },
		{ Polarization, "Polarization" },
		{ Alpha, "Alpha" },
		{ Kappa, "Kappa" },
		{ Phi, "Phi" },
		{ Chi, "Chi" },
		{ Oscillation_axis, "Oscillation_axis" },
		{ N_oscillations, "N_oscillations" },
		} ;

static int mx_params_count = sizeof(mx_params_list)/sizeof(MX_PARAMS_LIST);

/* end of variable data - please do not change this comment */

#endif

#endif
