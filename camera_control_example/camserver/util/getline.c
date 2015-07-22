// use readline to get a line and send it out through a pipe

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <readline/readline.h>
#include <readline/history.h>
#include <errno.h>


int main (int argc, char *argv[])
{
char *line, buf[4096];
int go, ifd, ofd, i, n;

ifd = atoi(argv[1]);		// input pipe for directory info
ofd = atoi(argv[2]);		// output pipe for command line
rl_already_prompted=1;		// cf. 'info readline'

if (-1 == fcntl(ifd, F_SETFL, O_NONBLOCK))
	{
	perror("getline fcntl()");
	sleep(3);
	exit(1);
	}

go=1;
while(go)
	{
	// look for directory change info
	// the directory becomes the base for filename completion
	// if this process is in readline(), this update is not
	// recorded until after the next 'enter'
	buf[0]='\0';
	usleep(1000);
	for(i=0; i<5; i++)
		{
		if ( (n=read(ifd, buf, sizeof(buf))) > 0 )
			break;
		usleep(1000);
		}
	line=buf;
	while(n>0)		// there could be more than one in the pipe
		{
		if (chdir(line))
			{
			perror(" getline chdir()");
			printf("  %s\n", line);
			fflush(stdout);
			printf("*");				// new prompt
			fflush(stdout);
			break;
			}
		n -= 1+strlen(line);
		line += 1+strlen(line);
		}

readline_loop:
	// Get a command line from the operator
	line = readline("*");			// blocking
	if (!line)						// it happened once
		continue;

	if(strlen(line) == 0)			// operator typed only 'enter'
		{
		printf("*");				// new prompt
		fflush(stdout);
		goto readline_loop;			// do not empty the pipe if  there
									// no message to to send to main
		}
	else
		{
		add_history (line);					// add to history
		write(ofd, line, 1+strlen(line));	// send to main
		}

	free(line);
	}
return 0;
}
