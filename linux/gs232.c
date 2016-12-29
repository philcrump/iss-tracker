#include <stdio.h>
#include <errno.h>
#include <fcntl.h> 
#include <string.h>
#include <termios.h>
#include <unistd.h>

#include "gs232.h"

static int set_interface_attribs (int fd, int speed, int parity)
{
        struct termios tty;
        memset (&tty, 0, sizeof tty);
        if (tcgetattr (fd, &tty) != 0)
        {
                fprintf(stderr,"error %d from tcgetattr", errno);
                return -1;
        }

        cfsetospeed (&tty, speed);
        cfsetispeed (&tty, speed);

        tty.c_cflag = (tty.c_cflag & ~CSIZE) | CS8;     // 8-bit chars
        // disable IGNBRK for mismatched speed tests; otherwise receive break
        // as \000 chars
        tty.c_iflag &= ~IGNBRK;         // disable break processing
        tty.c_lflag = 0;                // no signaling chars, no echo,
                                        // no canonical processing
        tty.c_oflag = 0;                // no remapping, no delays
        tty.c_cc[VMIN]  = 0;            // read doesn't block
        tty.c_cc[VTIME] = 5;            // 0.5 seconds read timeout

        tty.c_iflag &= ~(IXON | IXOFF | IXANY); // shut off xon/xoff ctrl

        tty.c_cflag |= (CLOCAL | CREAD);// ignore modem controls,
                                        // enable reading
        tty.c_cflag &= ~(PARENB | PARODD);      // shut off parity
        tty.c_cflag |= parity;
        tty.c_cflag &= ~CSTOPB;
        tty.c_cflag &= ~CRTSCTS;

        if (tcsetattr (fd, TCSANOW, &tty) != 0)
        {
                fprintf(stderr,"error %d from tcsetattr", errno);
                return -1;
        }
        return 0;
}

static void set_blocking (int fd, int should_block)
{
        struct termios tty;
        memset (&tty, 0, sizeof tty);
        if (tcgetattr (fd, &tty) != 0)
        {
                fprintf(stderr,"error %d from tggetattr", errno);
                return;
        }

        tty.c_cc[VMIN]  = should_block ? 1 : 0;
        tty.c_cc[VTIME] = 5;            // 0.5 seconds read timeout

        if (tcsetattr (fd, TCSANOW, &tty) != 0)
                fprintf(stderr,"error %d setting term attributes", errno);
}

int gs232_open_port(void)
{
	if(gs232.dummy)
	{
		return 0;
	}

	gs232.fd = open(gs232.port, O_RDWR | O_NOCTTY | O_SYNC);
	if (gs232.fd < 0)
	{
	        fprintf(stderr, "Error opening %s: %s\n", gs232.port, strerror (errno));
	        return -1;
	}

	set_interface_attribs(gs232.fd, B9600, 0);  // set speed to 9600 bps, 8n1 (no parity)
	set_blocking(gs232.fd, 0);                // set no blocking

	return 0;
}

int gs232_get_position(int *az, int *el)
{
	char buf[100];
	int n;
	char *azStr, *elStr;

	if(gs232.dummy)
	{
		*az = 0;
		*el = 0;
		return 0;
	}

	/* Clear serial rx buffer */
	n = write(gs232.fd, "\r\n", 2);
	n = read(gs232.fd, buf, sizeof buf);
	/* Clear function rx buffer */
	memset(buf,0,sizeof buf);

	/* Send query command */
	n = write(gs232.fd, "C2\r\n", 4);
	usleep((20) * 1000);

	/* Read response */
	// eg. AZ=355EL=000 (length: 13)
	n = 0;
	while(n<12)
	{
		n += read(gs232.fd, &buf[n], sizeof buf);
	}

	azStr = strstr(buf, "AZ");
	if(azStr == NULL)
	{
		fprintf(stderr, "AZ String not found in return string!\n");
		return -1;
	}

	sscanf(azStr, "AZ=%03d", az);

	elStr = strstr(buf, "EL");
	if(elStr == NULL)
	{
		fprintf(stderr, "EL String not found in return string!\n");
		return -1;
	}

	sscanf(elStr, "EL=%03d", el);

	return 0;
}

int gs232_set_position(int az, int el)
{
	int n;
	char buf[100];

	if(gs232.dummy)
	{
		printf("GS232: Dummy position set: AZ=%03d, EL=%03d\n",
			az, el
		);
		return 0;
	}


	/* Clear serial rx buffer */
	n = write(gs232.fd, "\r\n", 2);
	n = read(gs232.fd, buf, sizeof buf);
	/* Clear function rx buffer */
	memset(buf,0,sizeof buf);

	/* Assemble command */
	sprintf(buf,"W%03d %03d\r\n", az, el);

	/* Send query command */
	n = write(gs232.fd, buf, 10);
	usleep((20) * 1000);

	(void) n;
	return 0;
}