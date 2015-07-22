// This is not a library - incorporate into your code as needed

#include <termios.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

// function prototypes

int com0_setup(void);
void pulse_com0(void);


static int com0_fd;
static int Key=0x40;

/*****************************************************************************\
**                                                                           **
**    timing pulses on com0                                                  **
**                                                                           **
**    1) make a custom D-Sub 9 pin female connector with pin 7 connected     **
**       to pin 8 with 1 KOhm                                                **
**    2) optionally install a voltage divider between pin 3 and pin 5        **
**       to reduce +/- 12 V signal to +/- 5V                                 **
**    3) Change owner of /dev/ttyS0 to det:users                             **
**    4) Observe pulse between pin3 (as divided) and pin 5                   **
**                                                                           **
\*****************************************************************************/

int com0_setup(void)
{
char devicename[80] = "/dev/ttyS0";
long BAUD = B38400;             // baud rate
long DATABITS = CS8;			// 8 data bits
long STOPBITS = 0;
long PARITYON = 0;
long PARITY = 0;
struct termios newtio;      	 //port settings for serial port

com0_fd = open(devicename, O_RDWR | O_NOCTTY | O_NONBLOCK);
if (com0_fd < 0)
	{
	perror(devicename);
	return -1;
	}

// set new port settings for canonical input processing 
newtio.c_cflag = BAUD | CRTSCTS | DATABITS | STOPBITS | PARITYON | PARITY | CLOCAL | CREAD;
newtio.c_iflag = IGNPAR;
newtio.c_oflag = 0;
newtio.c_lflag = 0;       //ICANON;
newtio.c_cc[VMIN]=1;
newtio.c_cc[VTIME]=0;
tcflush(com0_fd, TCIFLUSH);
tcsetattr(com0_fd,TCSANOW,&newtio);

return 0;
}


void pulse_com0(void)
{
write(com0_fd, &Key,1);          //write 1 byte to the port
return;
}

