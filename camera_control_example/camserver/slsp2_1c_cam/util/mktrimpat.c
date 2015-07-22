// mktrimpat.c - make up a VPG pattern to trim a chip using stored data

#include <stdio.h>
#include <string.h>

#define NCOL 44		// values for 1 chip
#define NROW 78

#define PATCOMP "vpg517bn"		// the pattern compiler

int main (int argc, char *argv[])
{
int ix, iy, trim, trimv[NROW][NCOL], n;
char dpath[120], tpath[120], line[120], *p;
FILE *dfp, *tfp, *ofp;

if (argc <= 2)
	{
	printf("Usage:  mktrimpat data_path pattern_path\n");
	return 1;
	}
if ( (dfp=fopen(argv[1], "r")) == NULL )
	{
	printf("Cannot open data file: %s\n", argv[1]);
	return 1;
	}
strcpy(tpath, argv[1]);
if ( (p=strrchr(tpath, '/')) == NULL )
	{
	printf("Usage:  mktrimpat data_path pattern_path\n");
	return 1;
	}
*(p+1)='\0';		// isolate path

// find the template
strcpy(tpath, argv[2]);
if (strstr(tpath, "loadtrim.src"))		// full name given
	;
else if (strstr(tpath, "loadtrim"))		// extension missing
	strcat(tpath, ".src");
else
	strcat(tpath, "/loadtrim.src");

if ( (tfp=fopen(tpath, "r")) == NULL )
	{
	printf("Cannot find template: %s\n", tpath);
	return 1;
	}

*strrchr(tpath, '/') = '\0';
strcat(tpath, "/trimchip.src");
if ( (ofp=fopen(tpath, "w")) ==  NULL )
	{
	printf("Cannot open output file: %s\n", tpath);
	return 1;
	}

while(fgets(line, sizeof(line), tfp))
	{
	fprintf(ofp, "%s", line);	// copy template to output
	if (strstr(line, "START OF MAIN PROGRAM"))
		goto start1;
	}
printf("Start not found in template\n");
return 1;

start1:

while(fgets(line, sizeof(line), tfp))
	{
	fprintf(ofp, "%s", line);	// copy template to output
	if (strstr(line, "MAIN:"))
		goto start2;
	}
printf("MAIN: not found in template\n");
return 1;

// make up pattern
start2:
while (fgets(line, sizeof(line), dfp))
	{
	if ( (p=strstr(line, "trim")) == NULL )
		continue;
	sscanf(line+4, "%d%d%x", &ix, &iy, &trim);
	trimv[iy][ix]=trim;
	}

// output the trim commands in order
for (iy=0; iy<NROW; iy++)
	{
	for (ix=0; ix<NCOL; ix++)
	if (trimv[iy][ix] <= 0xf)
		fprintf(ofp, "      1100 0000 0010 0010 ; jump PR0%X ! %3d %3d\n",
				trimv[iy][ix], ix, iy);
	else
		fprintf(ofp, "      1100 0000 0010 0010 ; jump PR%X ! %3d %3d\n",
				trimv[iy][ix], ix, iy);
	fprintf(ofp, "!\n      1100 0000 0010 0010 ; jump NXTR !\n!\n");
	}
	
// finish up
while(fgets(line, sizeof(line), tfp))
	{
	if (strstr(line, "END OF MAIN PROGRAM"))
		goto end1;
	}
printf("End not found in template\n");
return 1;

end1:					// copy rest of template
while(fgets(line, sizeof(line), tfp))
	fprintf(ofp, "%s", line);	// copy template to output

fclose(dfp);
fclose(tfp);
fclose(ofp);

// compile the pattern
*strrchr(tpath, '/') = '\0';
strcpy(line, "cd ");
strcat(line, tpath);
strcat(line, ";");
strcat(line, PATCOMP);
strcat(line, " trimchip.src");
n=system(line);
printf("trimchip was compiled, return code = %d\n", n);

return 0;
}
