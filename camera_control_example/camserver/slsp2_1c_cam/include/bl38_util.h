// bl38_util.h - interface to R-axis 4 controller at SPring8 BL38

#ifndef BL38_UTIL_H
#define BL38_UTIL_H

#ifdef SP8_BL38B1

#include "bl_ext.h"

// Globals

struct resp_char {
		  char	*resp_msg;
		  int	resp_type;
		  int	resp_nnl;
		 };

enum {
	RES_EOF = 0,
	RES_ACCEPT,
	RES_REFUSE,
	RES_SUCCESS,
	RES_ERROR
     };



// function protoypes

int connect_ra4 (void);
int disconnect_ra4(void);
int	cm_input(char *);
int	cm_output(char *);
int	cm_shutter(int);
void cm_getmotval(void);
void cm_putmotval(void);
void cm_check_motion(void);
int	cm_moveto(char *, double, double);
int	cm_manual(int);
int	cm_home(void);
int	cm_dc(char *, double, double);
int check_port(void);

#endif		// SP8_BL38B1

#endif		// BL38_UTIL_H
