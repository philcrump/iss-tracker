#ifndef _GS232_H_
#define _GS232_H_

struct gs232_t {
	int fd;
	char *port;
	int dummy;
};

struct gs232_t gs232;

int gs232_open_port(void);

int gs232_get_position(int *az, int *el);

int gs232_set_position(int az, int el);

#endif