// tvxcampkg_decl.h - supplemental variable declarations for the SLS cameras

#ifndef TVXCAMPKG_DECL_H
#define TVXCAMPKG_DECL_H

int i, j, n, mod, bank;
char *p, line2[150], *cP;
int div, ofd, value, row, col;
double vcal, val1;
struct SETTINGS *sp;
unsigned short vstat;
unsigned int addr, data;
long long dataL;
FILE *ofp;
//time_t t;

#ifdef SP8_BL38B1
double val2;		// SPring8 BL38b1
#endif


#endif
