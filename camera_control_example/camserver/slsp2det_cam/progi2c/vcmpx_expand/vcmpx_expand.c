// vcmpx_expand.c - expand single vcmpx entries to the required 16 values

// gcc -g -Wall -o vcmpx_expand vcmpx_expand.c

/* Usage:  vcmpx_expand offset_file
**   where 'offset_file' is usually, e.g., 'p6m.off'
** Action: where an entry of the form:
**		nnn VCMPX -0.123
** is found, it will be repalced by 
**		nnn VCMP0 -0.123
**		nnn VCMP1 -0.123
**		...
**		nnn VCMP15 -0.123
**
** The result is written to, e.g., 'p6m.off.exp', which can be checked and
** renamed as needed.
*/	


#include <stdio.h>
#include <string.h>
#include <ctype.h>


int main (int argc, char *argv[])
{
int i, n;
FILE *ifp, *ofp;
char line[150], line2[150], *p, *q;

if (argc != 2)
	{
	printf("Usage:  vcmpx_expand offset_file\n");
	return 1;
	}

strcpy(line, argv[1]);

if (  (ifp=fopen(line, "r")) == NULL)
	{
	printf("Could not open %s for reading\n", line);
	return 1;
	}

strcat(line, ".exp");
if ( (ofp=fopen(line, "w")) == NULL)
	{
	printf("Could not open %s for writing\n", line);
	fclose(ifp);
	return 1;
	}

while ( (fgets(line, sizeof(line), ifp)) != NULL)
	{
	if ( !(p=strstr(line, "VCMPX")) )
		{
		fprintf(ofp, "%s", line);
		continue;
		}
	p += strlen("VCMP");		// expansion point
	strncpy(line2, line, p-line);
	line2[p-line] = '\0';		// terminate string
	p++;						// skip the 'X'
	n = 0;
	while (*p!='\0' && isspace(*(p+n)))
		{
		*(p+n) = ' ';			// line up columns better
		n++;
		}
	q = line2+strlen(line2);	// point under construction
	for (i=0; i<16; i++)
		{
		n = sprintf(q, "%d", i);
		if (i<10)
			strcat(line2+n, p);
		else if (*p==' ')
			strcat(line2+n, p+1);
		else
			strcat(line2+n, p);
		fprintf(ofp, "%s", line2);
		}
	}

return 0;
}

