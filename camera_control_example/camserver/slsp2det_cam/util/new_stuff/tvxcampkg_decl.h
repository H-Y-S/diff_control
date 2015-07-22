// tvxcampkg_decl.h - supplemental variable declarations for the SLS cameras
// these allocate memory in camserver.c, near line 164

#ifndef TVXCAMPKG_DECL_H
#define TVXCAMPKG_DECL_H

#include "detsys.h"

int j, k, n, value, mod, bank, col, row, devno, chan, flag;
int ixl, iyl, ixo, iyo;
unsigned int ui;
char line2[150], *cP;
double vcal, val1, v, tlo, thi, hlo, hhi;
struct SETTINGS *sp;
FILE *ofp;
struct TRIMBIN trims;

#ifdef USE_GPIB
int ofd;
#endif


#endif
