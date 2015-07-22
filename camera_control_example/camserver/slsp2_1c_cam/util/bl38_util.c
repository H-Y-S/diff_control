// bl38_util.c - interface from camserver to R-axis4 controller at SPring8 BL38


#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
//#include <sys/types.h>
#include <sys/socket.h>
//#include <unistd.h>
//#include <netinet/in.h>
//#include <arpa/inet.h>
#include <netdb.h>

#include "debug.h"
#include "tvxcampkg.h"

#ifdef SP8_BL38B1

static struct resp_char ra4_resp[] = {
				  	{"EOFPLACE\n",	RES_EOF,	0},
				  	{"Accept\n",	RES_ACCEPT,	1},
				  	{"Refuse\n",	RES_REFUSE,	1},
					{"Success\n",	RES_SUCCESS,1},
					{"Error\n",		RES_ERROR,	2},
					{NULL,		-1,		0},
				    };

static char default_host[] = "bl38b.spring8.or.jp";
static char default_port[] = "9999";
static char default_positions[] = "/home/det/sls8x2/ccd_kappa_positions";

struct cm_mdef {
			char *cn_name;	/* the name of the motor */
			int	cn_unit;	/* the CN0170 unit number */
			char cn_motor;	/* the CN0170 axis (X or Y) */
			int	cn_base;	/* base speed, normal operation, this axis */
			int	cn_top;		/* top speed, normal operation, this axis */
			int	cn_accel;	/* accelleration, normal operation, this axis */
			int	cn_stdeg;	/* number of steps per degree, this axis */
			int	cn_mode;	/* 0 if axis not init, 1 if normal, 2 if data collection */
			int	cn_noman;	/* 1 if no manual mode */
			int	cn_punit;	/* position unit */
			int	cn_pchan;	/* position channel */
		   };

static struct cm_mdef kappa_motors[] = {
	{"omega",  0,  'Y', 1000, 16000, 16000,  5000,  0, 1,    1,1},
	{"phi",    1,  'Y',    0,    10,    10,  25000, 0, 0,    1,1},
	{"kappa",  0,  'Y',  250,  1000,  1000,     50, 0, 1,    1,2},
	{"2theta", 0,  'Y',  250,  1000,  1000,    200, 0, 1,    0,2},
	{"dist",   2,  'X',    0,    10,    10,  12500, 0, 0,    0,0},
	{NULL,     0, '\0',    0,     0,     0,      0, 0, 0,    0,0},
	};

static char progname[] = "bl38_util.c";
static int		in_manual = 0;
static int		mind_motion;			/* motor in motion */
static char	    gl_line[2048];
static char     en_hostname[512];
static int      en_port;
static int      en_fd = -1;
static float	stat_phi;
static float	stat_dist;
//static float	dist_at_gstand_x_0 = 150;
//static float	dist_dir = -1;
//static float	min_velocity = .0999;

float	stat_omega;
float	stat_kappa;
float	stat_2theta;

// internal prototypes
static void set_enetra4_host(char *c);
static void set_enetra4_port(int p);
static void enetra4_shutdown(void);
static int rep_write(int, char*, int);
static int read_until_mult(int, char*, int, struct resp_char[]);
static int check_port_raw(int);
static int enetra4_init(void);
static int connect_to_host(int *, char *, int, char *);



// --------------------------------------------------------------
int connect_ra4 (void)
{
if (enetra4_init())
	printf("(bl38_util.c): error initializing connection to RA4\n");

return 0;
}


// --------------------------------------------------------------
int disconnect_ra4(void)
{
enetra4_shutdown();
return 0;
}


// --------------------------------------------------------------
static int enetra4_init(void)
{
char	line[256];
float	new_phi;
//float   new_dist;
char	*cp;

	if(NULL == (cp = getenv("RA4_HOST")))
		cp = default_host;
	set_enetra4_host(cp);
	
	DBG(3, "(bl38_util.c) Using host: %s\n", cp);
	
	if(NULL == (cp = (char *) getenv("RA4_PORT")))
		cp = default_port;
	set_enetra4_port(atoi(cp));
	
	DBG(3, "(bl38_util.c) Using port: %s\n", cp);

printf("bl38_util.c - checkpoint 0\n");
    if(-1 == (en_fd = connect_to_host(&en_fd,en_hostname,en_port,NULL)))
      {
//            fprintf(stderr,"(bl38_util.c): cannot establish connection with mar345 controller\n");
            printf("(bl38_util.c): cannot establish connection with mar345 controller\n");
            perror("ccd_bl_enetra4: connecting for command");
            return -1;
      }
printf("bl38_util.c - checkpoint 1\n");
	cm_input(gl_line);

	if(NULL == (char *) strstr(gl_line, "Accept"))
        	return(1);

	printf("EXEC_TCL_COMMAND TclSrv_Gonio:Phi read_pos\n");
	sprintf(line,"EXEC_TCL_COMMAND TclSrv_Gonio:Phi read_pos");
	cm_output(line);
printf("bl38_util.c - checkpoint 2\n");
	cm_input(gl_line);
printf("bl38_util.c - checkpoint 3\n");
	if(NULL != (cp = (char *) strstr(gl_line, "gonio")))
	  {
	    cp += strlen("gonio ");
	    sscanf(cp, "%f", &new_phi);
	    stat_phi = new_phi;
	  }
	    
#if 0
#ifdef	BL38B1
	sprintf(line,"EXEC_TCL_COMMAND TclSrv_GStand read_pos");
	cm_output(line);
	cm_input(gl_line);
	if(NULL != (cp = (char *) strstr(gl_line, "{gstand")))
	  {
	    cp += strlen("gstand ");
	    sscanf(cp, "%f", &new_dist);
	    new_dist *= dist_dir;
	    new_dist += dist_at_gstand_x_0;
	    stat_dist = new_dist;
	  }
#endif	/* BL38B1 */
#endif


//	send_status();
	return(0);
  }


// --------------------------------------------------------------
int check_port(void)
{
return check_port_raw(en_fd);
}


// --------------------------------------------------------------
static void	set_enetra4_host(char *c)
  {
        strcpy(en_hostname,c);
  }


// --------------------------------------------------------------
static void	set_enetra4_port(int p)
  {
        en_port = p;
  }


// --------------------------------------------------------------
static void	enetra4_shutdown()
  {

        if(en_fd != -1)
          {
		cm_output("PRO_CMD_CLOSE 0");
//                shutdown(en_fd,2);
                close(en_fd);
          }
  }


// --------------------------------------------------------------
int	cm_input(line)
char	*line;
  {
	int	i;

	i = read_until_mult(en_fd,line,2048,ra4_resp);
//	fprintf(stdout,"cm_input: status %d from read_until_mult, data:\n%s\n",i,line);
	printf("cm_input: status %d from read_until_mult, data:\n%s\n",i,line);

	return(0);
  }


// --------------------------------------------------------------
int	cm_output(line)
char	*line;
 {
	int	i;
	char	obuf[2049];

	if(line[0] == '\0')
	  return(0);
	strcpy(obuf,line);
	i = strlen(obuf);
	if(obuf[i-1] != '\n')
	  {
		obuf[i] = '\n';
		obuf[i+1] = '\0';
		i++;
	  }
	rep_write(en_fd, obuf, i);

	return(0);
  }


// --------------------------------------------------------------
/*
 *      Function to do a write, with possible multiple chunks.
 *      We need this because of unknown buffering over the network.
 *
 *      The write blocks.
 *
 *      Returns the number of characters written, or -1 if an error.
 */

static int     rep_write(fd,buf,count)
int     fd,count;
char    *buf;
  {
        char    *pos;
        int     remcount,i;

        if(count == 0)
                return(0);

        pos = buf;
        remcount = count;

        while(remcount > 0)
          {
                i = write(fd,pos,remcount);
                if(i < 0)
                  {
//                    fprintf(stderr,"rep_write: Error (%d) on file descriptor %d\n",errno,fd);
                    printf("rep_write: Error (%d) on file descriptor %d\n",errno,fd);
                    perror("rep_write");
                    return(-1);
                  }
                remcount -= i;
                pos += i;
          }
        return(count);
  }


// --------------------------------------------------------------
/*
 *	read_until_mult:
 *
 *		Reads a maximum of maxchar from the file descriptor
 *		fd into buf.  Reading terminates when a string
 *		contained in looking_for structure is found.  Additional
 *		characters read will be read until the number of newlines
 *		in the buffer is greater than or equal to the resp_nnl
 *		structure member.
 *
 *		The read blocks.
 *
 *		For convienence, since we usually use this routine with
 *		string handling routines, scanf's, and such, the program
 *		will append a "null" to the end of the buffer.  The number
 *		of chars gets incremented by 1.
 *
 *	  Returns:
 *
 *		-1 	on an error.
 *		 0 	on EOF.
 *		>0 	OK; in this case it is the resp_type entry number found.
 *
 *	Note:
 *
 *		This routine should not be used when it is expected that
 *		there may be more than one kind of message present on
 *		the socket, as the subsequent messages or parts thereof
 *		will be included in the buffer passed with the FIRST message
 *		in the buffer, and will probably be thrown away by the
 *		calling code.
 */

static int	read_until_mult(fd,buf,maxchar,looking_for)
int fd;
char *buf;
int maxchar;
struct resp_char looking_for[];
  {
	int		i,k,nnl,ret,utindex;
//	int		j,eobuf,looklen;
	fd_set		readmask;
	struct timeval	timeout;

	utindex = 0;

	while(1)
	  {
		FD_ZERO(&readmask);
		FD_SET(fd,&readmask);
		timeout.tv_usec = 0;
		timeout.tv_sec = 1;
		ret = select(FD_SETSIZE, &readmask, (fd_set *) 0, (fd_set *) 0, NULL);
		if(ret == 0)
			continue;
		if(ret == -1)
		  {
		    if(errno == EINTR)
			continue;	/* interrupted system calls are OK. */
		    return(-1);
		  }
		if(0 == FD_ISSET(fd,&readmask))
			continue;
		ret = read(fd,&buf[utindex],maxchar - utindex);
		if(ret == -1)
			return(-1);
		if(ret == 0)
			return(0);

		utindex += ret;

		if(utindex == maxchar)
			return(-1);

		buf[utindex] = '\0';
		for(k = 0; NULL != looking_for[k].resp_msg; k++)
		  if(NULL != (char *) strstr(buf, looking_for[k].resp_msg))
		  	break;
		if(looking_for[k].resp_type == -1)
			continue;
		nnl = 0;
		for(i = 0; buf[i] != '\0'; i++)
		  if(buf[i] == '\n')
		  	nnl++;
		if(nnl >= looking_for[k].resp_nnl)
			return(looking_for[k].resp_type);
	  }
  }


// --------------------------------------------------------------
/*
 *	Check if any input is presetnt with a 2 second timeout.
 */

static int check_port_raw(int fd)
  {
        fd_set readmask;
        int ret;
        struct timeval  timeout;

        FD_ZERO(&readmask);
        FD_SET(fd,&readmask);
        timeout.tv_usec = 0;
        timeout.tv_sec = 2;
        ret = select(FD_SETSIZE, &readmask, (fd_set *) 0, (fd_set *) 0, &timeout);
        if(ret == 0)
             return(0);
        if(ret == -1)
          {
          if(errno == EINTR)
              return(0);          /* Ignore interrupted system calls */
          else
              return(-1);
          }
        return(1);
  }


// --------------------------------------------------------------
static int connect_to_host(int *fd, char *remoteHost, int remotePort, char *function)
{
int status = 0, clientSocket=-1;
struct hostent *hostPtr = NULL;
struct sockaddr_in serverName = { 0 };

if (*fd)
	{close(*fd);
	fd=0;
	}

clientSocket = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
if (-1 == clientSocket)
{
	perror("socket()");
	return -1;
}

/*
* need to resolve the remote server name or IP address
*/
hostPtr = gethostbyname(remoteHost);
if (NULL == hostPtr)
{
	hostPtr = gethostbyaddr(remoteHost, strlen(remoteHost), AF_INET);
	if (NULL == hostPtr)
	{
		perror("Error resolving server address ");
		return 1;
	}
}

serverName.sin_family = AF_INET;
serverName.sin_port = htons(remotePort);
(void) memcpy(&serverName.sin_addr, hostPtr->h_addr, hostPtr->h_length);

// this blocks - can hang seemingly forever - eventually times out
DBG(3, "(bl38_util.c) Connecting to camera server %s:%d\n", 
		remoteHost, remotePort);
status = connect(clientSocket, (struct sockaddr*) &serverName,
		sizeof(serverName));
if (-1 == status)
{
	DBG(3, "(bl38_util.c) - connection failed\n");
	perror("connect()");
	return -1;
}

return clientSocket;
}


// --------------------------------------------------------------
int	cm_shutter(int val)

  {
//  	char	line[256];

	if(in_manual)
		cm_manual(0);
	if(val == 0)
		cm_output("EXEC_TCL_COMMAND TclSrv_Optics:Shutter close");
	 else
		cm_output("EXEC_TCL_COMMAND TclSrv_Optics:Shutter open");
	cm_input(gl_line);

	return(0);
  }


// --------------------------------------------------------------
void cm_getmotval()
  {
	FILE	*fpmot;
	char	*positionfile;
	char	line[132];

	if(NULL == (positionfile = (char *) getenv("CCD_KAPPA_POSITIONS")))
		positionfile = default_positions;
	if(NULL == (fpmot = fopen(positionfile,"r")))
	  {
//	    fprintf(stderr,"cm_getmotval: cannot open %s as motor position file\n",positionfile);
	    printf("cm_getmotval: cannot open %s as motor position file\n",positionfile);
	    return;
	  }
	fgets(line,sizeof line,fpmot);
	sscanf(line,"%f",&stat_omega);
	fgets(line,sizeof line,fpmot);
	sscanf(line,"%f",&stat_phi);
	fgets(line,sizeof line,fpmot);
	sscanf(line,"%f",&stat_kappa);
	fgets(line,sizeof line,fpmot);
	sscanf(line,"%f",&stat_2theta);
	fgets(line,sizeof line,fpmot);
	sscanf(line,"%f",&stat_dist);
	fclose(fpmot);
  }


// --------------------------------------------------------------
void	cm_putmotval()
  {
	FILE	*fpmot;
	char	*positionfile;

	if(NULL == (positionfile = (char *) getenv("CCD_KAPPA_POSITIONS")))
		positionfile = default_positions;
	if(NULL == (fpmot = fopen(positionfile,"w")))
	  {
//	    fprintf(stderr,"cm_putmotval: cannot create %s as motor position file\n",positionfile);
	    printf("cm_putmotval: cannot create %s as motor position file\n",positionfile);
	    return;
	  }
	fprintf(fpmot,"%10.3f\n%10.3f\n%10.3f\n%10.3f\n%10.3f\n",
		stat_omega,stat_phi,stat_kappa,stat_2theta,stat_dist);
	fclose(fpmot);
	DBG(3,"(bl38util.c) Writing file: %s\n", positionfile);
  }


// --------------------------------------------------------------
void cm_check_motion()
  {
	char	line[132];
	int	i,j;

	while(1)
	  {
	    sleep(1);
	    sprintf(line,"%cP?",kappa_motors[mind_motion].cn_motor);
	    cm_output(line);
	    if(cm_input(gl_line))
	      {
//		fprintf(stderr,"cm_check_motion: timeout occurred reading status\n");
		printf("cm_check_motion: timeout occurred reading status\n");
		return;
	      }
	      
	    cm_input(gl_line);
//	    fprintf(stderr,"cm_check_motion: received line: %s\n",line);
	    printf("cm_check_motion: received line: %s\n",line);
	    for(i = j = 0; line[i] != '\0';i++)
	      if(line[i] == '=')
		{
		  j = 1;
		  break;
		}
	    if(j == 1)
		break;
//	    send_status();
	  }
  }


// --------------------------------------------------------------

int	cm_moveto(motstr,new_value,current_value)
char	*motstr;
double	new_value;
double	current_value;
  {
//	int	mind,i_current,i_new,amt_to_move;
	int	mind;
//	double	slop,signed_slop,x1,x2,x3,xa;
	double	x1,x2,x3;
	char	line[132];

	for(mind = 0; NULL != kappa_motors[mind].cn_name; mind++) 
	  if(0 == strcmp(kappa_motors[mind].cn_name,motstr))
		break;
	if(kappa_motors[mind].cn_name == NULL)
		return(1);
	
	mind_motion = mind;

//	fprintf(stderr,"cm_moveto: motor: %s new: %f current: %f\n",motstr,new_value,current_value);
	printf("cm_moveto: motor: %s new: %f current: %f\n",motstr,new_value,current_value);
//	if(new_value == current_value)
//		return(0);

	if(in_manual)
		cm_manual(0);
        /*
         *      Check to see if the requested motor position is reasonable.
         *
         *      x1 will be a value from 0 <= x1 < 360.
         */

	if(mind == 4)	/* distance */
	  {
        	x1 = new_value;
        	x2 = current_value;
		goto skip_anglenorm;
	  }
        x1 = new_value;
        while(x1 >= 360.)
                x1 -= 360.;
        while(x1 < 0)
                x1 += 360;
        x2 = current_value;
        while(x2 >= 360.)
                x2 -= 360.;
        while(x2 < 0)
                x2 += 360;

        switch(mind)
          {
            case 0:     /* omega */
                if(x1 > 120. && x1 < 240.)
                  {
//                    fprintf(stderr,"acs_moveto: %f (renormalized from %f) is an ILLEGAL omega value\n",x1,new_value);
//                    fprintf(stderr,"            Motions are restricted to be 240 to 360, 0 to 120 (-120 < omega < 120)\n");
                    printf("acs_moveto: %f (renormalized from %f) is an ILLEGAL omega value\n",x1,new_value);
                    printf("            Motions are restricted to be 240 to 360, 0 to 120 (-120 < omega < 120)\n");
                    return(1);
                  }
                break;
	    case 1:	/* phi */
		sprintf(line,"EXEC_TCL_COMMAND TclSrv_Gonio:Phi \"move %9.3f\"",x1);
		cm_output(line);
	    	break;
            case 2:     /* kappa */
                if(x1 > 90 && x1 < 270)
                  {
//                    fprintf(stderr,"acs_moveto: %f (renormalized from %f) is an ILLEGAL kappa value\n",x1,new_value);
//                    fprintf(stderr,"            Motions are restricted to be 270 to 360, 0 to 90 (-90 < kappa < 90)\n");
                    printf("acs_moveto: %f (renormalized from %f) is an ILLEGAL kappa value\n",x1,new_value);
                    printf("            Motions are restricted to be 270 to 360, 0 to 90 (-90 < kappa < 90)\n");
                    return(1);
                  }
                break;
            case 3:     /* two theta */
                if(x1 > 45 && x1 < 315)
                  {
//                    fprintf(stderr,"acs_moveto: %f (renormalized from %f) is an ILLEGAL 2theta value\n",x1,new_value);
//                    fprintf(stderr,"            Motions are restricted to be 315 to 360, 0 to 45 (-45 < 2theta < 45)\n");
                    printf("acs_moveto: %f (renormalized from %f) is an ILLEGAL 2theta value\n",x1,new_value);
                    printf("            Motions are restricted to be 315 to 360, 0 to 45 (-45 < 2theta < 45)\n");
                    return(1);
                  }
                break;
            default:
                break;
          }

	if(x2 >= 180)
	  x2 -= 360.;
	if(x1 >= 180)
	  x1 -= 360.;


skip_anglenorm:

	x3 = x1 - x2;

	if(mind == 4)	/* distance */
	  {
#if 0
		xa = x1 - dist_at_gstand_x_0;
		xa *= dist_dir;
		sprintf(line,"EXEC_TCL_COMMAND TclSrv_GStand \"x move %9.3f\"",xa);
		cm_output(line);
#endif
	  }
	 else
	  {
		sprintf(line,"EXEC_TCL_COMMAND TclSrv_Gonio:Phi \"move %9.3f\"",x1);
		cm_output(line);
		stat_phi = x1;
	  }
	cm_input(gl_line);
	return(0);
      }

// --------------------------------------------------------------
int	cm_manual(mode)
int	mode;
  {
//	int	i,mind;
//	char	line[256];

	if(mode == 1)
	  {
		in_manual = 1;
	  }
	 else
	  {
		in_manual = 0;
	  }
	return(0);
  }


// --------------------------------------------------------------
int	cm_home()
  {
	char	line[256];
//	float	new_phi,new_dist;
	float	new_phi;
	char	*c;

	if(in_manual)
	    cm_manual(0);

	sprintf(line,"EXEC_TCL_COMMAND TclSrv_Gonio:Phi init");
	cm_output(line);
	cm_input(gl_line);
	if(NULL != (c = (char *) strstr(gl_line, "{gonio")))
	  {
	    c += strlen("{gonio ");
	    sscanf(c, "%f", &new_phi);
	    stat_phi = new_phi;
	  }
//	fprintf(stdout,"New phi value from zeroing: %fd\n",stat_phi);
	printf("New phi value from zeroing: %fd\n",stat_phi);
	    

#if 0
#ifndef	BL40B2
	sprintf(line,"EXEC_TCL_COMMAND TclSrv_GStand \"x init\"");
	cm_output(line);
	cm_input(gl_line);
	if(NULL != (c = (char *) strstr(gl_line, "gstand")))
	  {
	    c += strlen("gstand ");
	    sscanf(c, "%f", &new_dist);
	    stat_dist = new_dist;
	  }
#endif	/* Not BL40B2 */
#endif

//	send_status();
	return(0);
  }

// --------------------------------------------------------------
int	cm_dc(motstr,width,dctime)
char	*motstr;
double	width;
double	dctime;
  {
//	int	mind,i_width,n_osc;
	int	mind,n_osc;
//	double	slop,signed_slop,axis_velocity,a_start,a_end;
	double	axis_velocity,a_start,a_end, new_phi;
	char	line[132], *cp;
//	char	rline[1024];

	for(mind = 0; NULL != kappa_motors[mind].cn_name; mind++) 
	  if(0 == strcmp(kappa_motors[mind].cn_name,motstr))
		    break;
	if(kappa_motors[mind].cn_name == NULL)
		    return(1);
	    
	if(in_manual)
		    cm_manual(0);

	sprintf(line,"EXEC_TCL_COMMAND TclSrv_WaitTime %.3f",0.200);
//	fprintf(stdout,"%s : Sending %s\n",progname,line);
	printf("%s : Sending %s\n",progname,line);
	cm_output(line);
	cm_input(gl_line);

	sprintf(line,"EXEC_TCL_COMMAND TclSrv_Gonio:Phi read_pos");
	cm_output(line);
	cm_input(gl_line);
	if(NULL != (cp = (char *) strstr(gl_line, "gonio")))
	  {
	    cp += strlen("gonio ");
	    sscanf(cp, "%lf", &new_phi);
	    stat_phi = new_phi;
	  }
	    
	a_start = stat_phi;
	a_end = stat_phi + width;
//	n_osc = 2;
	n_osc = 1;
	axis_velocity = n_osc * width / dctime;

//	fprintf(stdout,"initial n_osc: %d with initial axis_velocity: %f\n",n_osc,axis_velocity);
	printf("initial n_osc: %d with initial axis_velocity: %f\n",n_osc,axis_velocity);
#if 0		// fine-slicing kludge
	if(axis_velocity > 0)
	  {
	    for(; axis_velocity < min_velocity;)
	      {
		n_osc += 2;
		axis_velocity = n_osc * width / dctime;
//		fprintf(stdout,"n_osc: %d with axis_velocity: %f\n",n_osc,axis_velocity);
		printf("n_osc: %d with axis_velocity: %f\n",n_osc,axis_velocity);
	      }
	  } 
#endif

#ifdef BL40B2
	if(axis_velocity == 0)
		sprintf(line,"EXEC_TCL_COMMAND TclSrv_Optics:Shutter \"on_with_time %.3f\"",
			dctime);
	  else

		sprintf(line,"EXEC_TCL_COMMAND TclSrv_Gonio:Phi \"osc %.3f %.3f %d %.2f %.4f\"",
			a_start,a_end,n_osc,dctime,axis_velocity);
#else /* for 38B1 */
	sprintf(line,"EXEC_TCL_COMMAND TclSrv_Gonio:Phi \"osc %.3f %.3f %d %.2f\"",
		a_start,a_end,n_osc,dctime);
#endif /* 38B1 */

//	fprintf(stdout,"%s : Sending %s\n",progname,line);
	printf("%s : Sending %s\n",progname,line);
	cm_output(line);
	cm_input(gl_line);

	sprintf(line,"EXEC_TCL_COMMAND TclSrv_WaitTime %.3f",dctime);
//	fprintf(stdout,"%s : Sending %s\n",progname,line);
	printf("%s : Sending %s\n",progname,line);
	cm_output(line);

	return(0);
  }

#endif		// SP8_BL38B1
