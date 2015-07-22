// tvxcampkg_decl.h - supplemental variable declarations for the SLS cameras
// these allocate memory in camserver.c, near line 162

#ifndef TVXCAMPKG_DECL_H
#define TVXCAMPKG_DECL_H

#include "detsys.h"

int n, value, mod, bank, col, row, devno, chan, flag;
int ixl, iyl, ixo, iyo;
unsigned int ui;
char line2[150], *cP;
double vcal, val1, v, tlo, thi, hlo, hhi;
struct SETTINGS *sp;
FILE *ofp;

#ifdef USE_GPIB
int ofd;
#endif


#endif
