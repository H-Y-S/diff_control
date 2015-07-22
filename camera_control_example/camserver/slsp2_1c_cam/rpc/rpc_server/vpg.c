/*
** vpg.c
**
** routines for the VPG517 pattern generator
** loadpattern based on Daneks loadpattern routine
**
** from B. Schmitt, modified by EFE
*/
#include <stdio.h>
#include <string.h>
#include "ioc.h"
#include "vpg.h"

#define PRINT 0      /* 1 - print data, 0 - less printing */
#define PRINT_2 0
//#define DEBUG_CODE		// use debugging code

#define OK 0
#define FAIL -1


/* bits for status register */
#define vme_clk_enable   0x00000001
#define tristate_counter 0x00000002
#define output_disable   0x00000004
/* #define int_clk_enable   0x00000049 */ /* this includes vme_clk_enable */
#define int_clk_enable   0x00000008

/* bits for vme jump command */
#define vme_jump_command 0x00000080
#define vme_jump_mask    0xFFFF807F

#define divmask          0xFFFF0F

/*address offsets */
#define out_register      0x1FFE4
#define in_register       0x1FFE8
#define delay_register    0x1FFEC
#define jump_register     0x1FFF0
#define pc_reset_register 0x1FFF4
#define vme_clk_register  0x1FFF8
#define status_register   0x1FFFC
#define FORCE_ADR         0x1FE00
 

/* function declarations */
#include "ioc.h"

int sysBusToLocalAdrs(int,int,unsigned int*);
int vxMemProbe(char *, int, int, char *);			// from vxWorks
void load_configuration(void);

/* global varables */
unsigned int cpu_base_1;		/* reference for 1st VPG */
unsigned int cpu_base_2; 		/* reference for 2nd VPG */
unsigned int cpu_base_d;		/* reference for default VPG */
unsigned int vme_base_1 = 0x4000000; /* assume 1st VPG at $4000000 */
unsigned int vme_base_2 = 0x6000000; /* assume 2nd VPG at $6000000 */
unsigned int *ptr1;
int vpg_is_initialized=0;
unsigned int two_vpgs_found=0;
char image_path[120] = "/images/";
char config_path[120] = "/config/";

int vpgini(void) {
  /* 
   * maps the VME memory into the address space of the CPU 
   * must be called once before any of the other routines 
   * work
   */

   unsigned int status, bits, res;
   int i, rval, flag;

	if (vpg_is_initialized)
		{
// 		load_configuration();		// reload the configuration file
		return OK;
		}

/* Initilize vme for vpg1*/
   cpu_base_1=0;

   printf("initial value of cpu_base %x\n", cpu_base_1);

   status=sysBusToLocalAdrs(0xd, vme_base_1, &cpu_base_1);

   printf("VPG1 mapping result %#x\n",status);
   printf("pointing to %#x\n",cpu_base_1);

   if(status != 0)
     {
     printf(" VPG1 cannot be initilized\n");
     return FAIL;
	 }

  /* start the vme clock & reset bits 1 & 2
  */ 

	cpu_base_d = cpu_base_1;
	if(vxMemProbe( (char *)(cpu_base_d + status_register), VX_READ, 4,
				(char *)&status) == ERROR)
		{
		printf("\n---***---VME bus error initializing VPG1\n");
		return FAIL;
		}

  status |= vme_clk_enable;	
  status &= ~tristate_counter;
  status &= ~output_disable;

  rval = write_status(status);
  set_clk_divider(10);			// it's helpful to define this

  /* memory check */
  bits = 0xa5a5a5a5;
  flag = 0;
  for (i = 0; i<0x1ffc0; i+=4)
  	{
	vme_write_a32(i, bits);
	if (bits != (res = vme_read_a32(i)))
		{
		printf("*** VPG1 memory test failed at address %x: %x\n", i, res);
		flag = 1;
		break;
		}
	}
  if (!flag)
  	printf(" VPG1 memory test successfully completed\n");
  printf(" VPG1 is initialized\n");

#ifdef TWO_VPGS
/* Initilize vme for vpg2*/
   cpu_base_2=0;

   printf("initial value of cpu_base %x\n", cpu_base_2);

   status=sysBusToLocalAdrs(0xd, vme_base_2, &cpu_base_2);

   printf("VPG2 mapping result %#x\n",status);
   printf("pointing to %#x\n",cpu_base_2);

   if(status != 0)
     {
     printf(" VPG2 cannot be initilized\n");
     goto skipVPG2;
	 }

  /* start the vme clock & reset bits 1 & 2
  */ 

	cpu_base_d = cpu_base_2;
	if(vxMemProbe( (char *)(cpu_base_d + status_register), VX_READ, 4,
				(char *)&status) == ERROR)
		{
		printf("\n---***---VME bus error initializing VPG2\n");
		goto skipVPG2;
		}

  status |= vme_clk_enable;	
  status &= ~tristate_counter;
  status &= ~output_disable;
  two_vpgs_found=1;

  rval = write_status(status);
  set_clk_divider(10);			// it's helpful to define this

  /* memory check */
  bits = 0xa5a5a5a5;
  flag = 0;
  for (i = 0; i<0x1ffc0; i+=4)
  	{
	vme_write_a32(i, bits);
	if (bits != (res = vme_read_a32(i)))
		{
		printf("*** VPG2 memory test failed at address %x: %x\n", i, res);
		flag = 1;
		break;
		}
	}
  if (!flag)
  	printf(" VPG2 memory test successfully completed\n");
   printf(" VPG2 is initialized\n");
skipVPG2:
#endif

  cpu_base_d = cpu_base_1;		// default setting

  load_configuration();

  vpg_is_initialized = 1;		// we need not repeat this
  return rval;
}


void load_configuration(void)
{
char line[80], *p;
FILE *ifp;

if ( (ifp=fopen("/iocrc", "r")) == NULL)
	printf("*** Could not open configuration file '/iocrc'\n");
else
	{
	while (fgets(line, sizeof(line), ifp))
		{
		if (line[0]=='#')
			continue;
		if (strstr(line, "image_path"))
			{
			p = line+strlen("image_path");
			while (*p==' ' || *p=='\t')		// skip leading blanks
				p++;
			strcpy(image_path, p);
			*strrchr(image_path, '\n') ='\0';
			printf("Setting image path: %s\n", image_path);
			}
		else if (strstr(line, "config_path"))
			{
			p = line+strlen("config_path");
			while (*p==' ' || *p=='\t')		// skip leading blanks
				p++;
			strcpy(config_path, p);
			*strrchr(config_path, '\n') ='\0';
			printf("Setting config path: %s\n", config_path);
			}
		}
	fclose(ifp);
	}

return;
}

int loadpattern(char *fname) {
  /* 
   * loads the pattern
   * vpgini must be called first
   */

   unsigned int data, adr, line;
   int status, i;
   const int MAX_LINE = 32639;
   FILE *fp1;

   unsigned int rval,val;
 

   /* stop clock */
   if ((rval=clk_stop())==0) {
     printf("stopping clock ok\n");
   } else {
     printf("stopping clock error !!!!!\n");
   }

   /* Open the data file */
   fp1 = fopen(fname,"r");
   if( fp1 == NULL ) {
     printf(" file %s does not exist\n",fname);
     return FAIL;
   }
   printf(" Downloading file %s \n",fname);

   i = 0;
   /*    read a line from the data file until EOF */    
   while ( (fscanf(fp1,"%x %x",&line,&data)) != EOF )    /* HEX input !!! */
	{
       i++;                                               /* count lines */
       if( i > MAX_LINE )     /* check */
	    	  {
           printf(" Too many program lines %d %d\n",i,MAX_LINE);
	   	   return FAIL;
		   }
     
     if(PRINT)
       printf(" %d line = %d data = %x\n",i,line,data);

     
     /* load line number into VPG memory */
     adr=4*line;
     status = vme_write_a32(adr,data);
     if( status != 0 )
       printf(" No response from VME\n");

     if(PRINT)
       printf(" set counter %d %08x, %08x, %08x\n",
	      i,line,adr,data);
   }  /* end while */ 

   fclose(fp1); /* close file */
   printf("data loaded ok\n");


   /* Set the forced goto address to 0 */
   val=0;
   if ((rval=set_force(val))==0) {
     printf("setting force ok\n");
   } else {
     printf("setting force error !!!!!\n");
   }


   /* Read the STATUS word */
   val=read_status();   
   printf(" status = %08x\n",val);
   printf(" reading stastus word ok\n");


   /* Reset the PC (program counter) to 0 */
   if ((rval=pc_reset())==0) {
     printf("resetting ok\n");
   } else {
     printf("pc resetting error !!!!!\n");
   }

   return OK;

}

int vme_write_a32(unsigned int addr,unsigned int data)
{
  /* this function writes data to the VME memory address addr
     addr is the offset to the base address of the VME module 
     in the VME address space. addr is added to the corresponding
     offset in the CPU address space
  */

    ptr1=(unsigned int*) (cpu_base_d + addr);

#ifdef DEBUG_CODE
	if(vxMemProbe((char *)ptr1, VX_WRITE, 4, (char *)&data) == ERROR)
		printf("VME write error addr:data %p: %#8x\n", ptr1, data);
	else
	    if (PRINT) printf("writing 0x%08x to address %p\n", data, ptr1);
#else
    *ptr1=data;
    if (PRINT) printf("writing 0x%08x to address %p\n", data, ptr1);
#endif

    return OK;
}

int vme_read_a32(unsigned int addr)
{
  /* this function reads data from the VME memory address addr.
     addr is the offset to the base address of the VME module 
     in the VME address space. addr is added to the corresponding
     offset in the CPU address space.
  */
	unsigned int dat;

    ptr1=(unsigned int*) (cpu_base_d + addr);

#ifdef DEBUG_CODE
	if(vxMemProbe( (char *)ptr1, VX_READ, 4, (char *)&dat) == ERROR)
		{
		printf("VME read error addr:data %p: %#8x\n", ptr1, dat);
		return NULL;
		}
	else
	    if (PRINT) printf("reading addr %p : %0x\n", ptr1, dat);
#else
	dat = *ptr1;
    if (PRINT)printf("reading addr %p : %0x\n", ptr1, dat);
#endif

    return dat;
}


int set_clk_divider(unsigned int divider)
{
/* set the clock divider bits D4(LSB)..D7(MSB) in status register
   the VPG clock is the internal quartz clock divided by (n+1) where
   n is the values writen in these 4 bits 
 */
  unsigned int status;
  int rval;

  printf(" setting clock divider to %d\n", divider);
  if ( (divider<16) || (divider>0) ) {
    divider = divider << 4;
    status = read_status();
    status = status & divmask;
    status = status | divider;
    rval =  write_status(status);
  return rval;
  }

  return FAIL;
}

int set_clk_delay(int delay15, int delay16)
{
  /* set the delay in ns of channel 15 (D0 .. D7) and 16 (D8 .. D15) 
   */
  unsigned int delay;
  int rval;

  if ( (delay15 > 255) || (delay15 < 0) ||  (delay16 > 255) || (delay16 < 0) ) {
    return FAIL;
  }
  printf(" setting clock delay15 to  %d delay16 to %d\n", delay15, delay16);

  delay = (delay16 << 8) + delay15;
  rval = vme_write_a32(delay_register,delay);

  return rval;
}

int write_output(unsigned int data)
{
  /* writes data to the output port (only lower 8 bits) does not interrupt
     pattern generator 
  */

  int rval;

  printf(" writing %d to output\n", data);
  rval = vme_write_a32(out_register,data);

  return rval;
}

int read_input(void)
{
  /* reads data from the input port (only lower 8 bits) does not interrupt
     pattern generator 
  */
  int rval;

  rval = vme_read_a32(in_register);

  return rval;
}


int vme_jump(unsigned int adr)
{
  /* write the address adr to the jump register. check that the address is in
     the correct form, i.e. only bits 14 to 7 contain a value. all other bits
     must be 0, then the address need to be shifted to the left
  */

  int rval;

  if ( (adr & vme_jump_mask)!=0 ) return FAIL;
  printf(" jumping to address %#x\n", adr);
  adr = adr << 1;

  adr = adr | vme_jump_command;
  rval = vme_write_a32(jump_register,adr);

  return rval;
}

int vme_clk(void)
{
  /* the VPG can be clocked by this internal VME command. this allows a
     CPU controlled single step modus, bit 0 in status register (vme_clk_enable)
     has to be set before
   */
  int rval;

  rval = vme_write_a32(vme_clk_register,0);

  return rval;
}

int write_status(unsigned int data)
{
  /* writes data to status register 
   */ 
  int rval;
  unsigned int adr;

  if (PRINT_2) printf(" writing %#x to status register\n", data);
  adr=status_register;
  rval = vme_write_a32(adr,data);

  return rval;
}

int read_status(void)
{
  /* reads data from status register 
   */ 
  int rval;

  rval = vme_read_a32(status_register);

  return rval;
}

int set_force(unsigned int adr)
{
  /* writes a goto adr in the memory for the force input
   */ 
  int rval;
  unsigned int force_adr=FORCE_ADR;

  if ( (adr & vme_jump_mask)!=0 ) return FAIL;

  if (PRINT_2) printf(" setting force to %d \n", adr);
  adr = adr < 1;

  adr = adr | vme_jump_command;
  rval = vme_write_a32(force_adr,adr);

  return rval;
}

int clk_start(void)
{
  /* starts the internal clock
   */ 
  int rval;
  unsigned int status;

  status = read_status();
  status |= vme_clk_enable;		// this should not change anything
  status |= int_clk_enable;

		// empirical fix for a bug on the board
  status &= 0x0000ffff;			// upper bits get set for reasons unknown

  rval = write_status(status);

  return rval;
}


int clk_stop(void)
{
  /* stops the internal clk
   */ 
  int rval;
  unsigned int status;

  status = read_status();
//  status = status & (~vme_clk_enable);	// leave vme clk on
  status = status & (~int_clk_enable);

  rval = write_status(status);

  return rval;
}

int pc_reset(void)
{
  /* reset program counter to 0
   */ 
  int rval;
  unsigned int adr=pc_reset_register, val=0;

  rval = vme_write_a32(adr,val);

  return rval;
}

int clr_mem(void)
{
  /* clears all memory
   */ 

  unsigned int adr,val=0;

  for (adr=0;adr<32639;adr++) {
    ptr1=(unsigned int*) (cpu_base_d + adr*4);
    *ptr1=val;
    /*3    printf(" %x\n",ptr1); */
  }

  return OK;
}

int set_default_vpg(int n)
{
cpu_base_d = cpu_base_1;

if ( PRINT_2 && two_vpgs_found ) printf("Request default vpg: %d\n", n);
if (n>=2){
	if ( two_vpgs_found )
		{cpu_base_d = cpu_base_2;
		return OK;
		}
	else
		return FAIL;
	}

return OK;
}
