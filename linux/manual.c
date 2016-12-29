#include <stdio.h>
#include <math.h>
#include <unistd.h>
#include <stdlib.h>

#include "gs232.h"

#define TXT_NORM  "\x1B[0m"
#define TXT_RED  "\x1B[31m"
#define TXT_GRN  "\x1B[32m"
#define TXT_YEL  "\x1B[33m"
#define TXT_BLU  "\x1B[34m"
#define TXT_MAG  "\x1B[35m"
#define TXT_CYN  "\x1B[36m"
#define TXT_WHT  "\x1B[37m"

#define TRACK_THRESHOLD   1.00
#define TRACK_INTERVAL_MS 800

#define ANTENNA_PARK_ELEVATION    90;
#define ANTENNA_PARK_AZIMUTH    180;

struct antenna_t {
  int azimuth;
  int elevation;
};
struct antenna_t antenna;
struct antenna_t antenna_previous;

int main(int argc, char **argv)
{
  printf("** Manual Rotator Control **\n\n");

  gs232.port = "/dev/ttyACM0";
  gs232.dummy = 0;

  if(argc !=3)
  {
    printf("Usage: ./manual <AZ> <EL>\n");
    exit(1);
  }

  printf("Connecting to rotator..      ");

  if(gs232_open_port() < 0)
  {
    fprintf(stderr, "\nError opening rotator serial port!");
    exit(1);
  }

  if(gs232_get_position(&antenna.azimuth, &antenna.elevation) < 0)
  {
    fprintf(stderr, "\nError querying rotator position!");
    exit(1);
  }

  printf("%sOK%s (AZ: %03d, EL: %03d)\n",
    TXT_GRN,TXT_NORM,
    antenna.azimuth,
    antenna.elevation
  );

  antenna.azimuth = atoi(argv[1]);
  antenna.elevation = atoi(argv[2]);

  printf("\nCommanding antenna to AZ: %d, EL: %d\n",
    antenna.azimuth,
    antenna.elevation
  );

  gs232_set_position(antenna.azimuth, antenna.elevation);

  printf("Done. exiting..\n");
  return 0;
}
