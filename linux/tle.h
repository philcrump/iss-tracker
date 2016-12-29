#ifndef TLE_H_
#define _TLE_H_

struct tle_t {
	char *elements[2];

	char *file;

	char *tmp_file;

	char *url;

	int update_enabled;
};

struct tle_t tle;

int tle_update(void);

int tle_load(void);

#endif