#include <stdio.h>
#include <math.h>
#include <unistd.h>
#include <stdlib.h>
#include <getopt.h>

#include <predict/predict.h>
#include "gs232.h"
#include "tle.h"

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

char *portname = "/dev/ttyACM0";

struct antenna_t {
  int azimuth;
  int elevation;
};
struct antenna_t antenna;
struct antenna_t antenna_previous;

predict_orbital_elements_t *iss_tle;
struct predict_orbit iss_orbit;

void _print_usage(void)
{
  printf(
    "\n"
    "Usage: track [options] INPUT\n"
    "\n"
    "  -p, --port <path>      Set the rotator serial port. Default: /dev/ttyACM0\n"
    "  -t, --tle-file <path>  Set the path of the TLE file. Default: iss.txt\n"
    "  -u  --tle-url <path>   Set the URL to update the TLE file.\n"
    "                             Default: https://hamtv.batc.tv/iss.txt\n"
    "  -n  --no-tle-update    Don't attempt to update TLE from web.\n"
    "  -d  --dummy            Run GS232 in Dummy mode (no rotator connection required).\n"
    "\n"
  );
}

int main(int argc, char **argv)
{
  int opt, c;

  printf("** ISS Tracker **\n\n");

  tle.url = "https://hamtv.batc.tv/iss.txt";
  tle.file = "iss.txt";
  tle.tmp_file = "/tmp/iss.txt";
  tle.update_enabled = 1;

  gs232.port = "/dev/ttyACM0";
  gs232.dummy = 0;

  static const struct option long_options[] = {
    { "port",         required_argument, 0, 'p' },
    { "tle-file",     required_argument, 0, 't' },
    { "tle-url",      required_argument, 0, 'u' },
    { "no-tle-update",no_argument,       0, 'n' },
    { "dummy",        no_argument,       0, 'd' },
    { 0,              0,                 0, 0 }
  };

  while((c = getopt_long(argc, argv, "p:t:u:nd", long_options, &opt)) != -1)
  {
    switch(c)
    {
    
    case 'p': /* --port <path> */
      gs232.port = optarg;
      break;
    
    case 't': /* --tle-file <path> */
      tle.file = optarg;
      break;
    
    case 'u': /* --tle-url <path> */
      tle.url = optarg;
      break;
      
    case 'n':
      tle.update_enabled = 0;
      break;
      
    case 'd':
     gs232.dummy = 1;
      break;
    
    case '?':
      _print_usage();
      return(0);
    }
  }

  printf("Updating TLE file from web.. ");

  if(!tle.update_enabled)
  {
    printf(TXT_YEL"Disabled"TXT_NORM"\n");
  }
  else if(tle_update() < 0)
  {
    printf(TXT_YEL"Error (non-fatal)"TXT_NORM"\n");
  }
  else
  {
    printf(TXT_GRN"OK"TXT_NORM"\n");
  }

  printf("Parsing TLE from file..      ");

  if(tle_load() < 0)
  {
    printf(TXT_RED"Error loading TLE from file!"TXT_NORM"\n");
    exit(1);
  }

  iss_tle = predict_parse_tle(tle.elements);
  if(iss_tle == NULL)
  {
    printf(TXT_RED"Error parsing TLE!"TXT_NORM"\n");
    exit(1);
  }

  printf(TXT_GRN"OK"TXT_NORM" (Epoch: 20%01d-%2.2f)\n",
    iss_tle->epoch_year,
    iss_tle->epoch_day
  );

  printf("Setting up orbital objects.. ");

  if(predict_orbit(iss_tle, &iss_orbit, 0) < 0)
  {
    printf(TXT_RED"Failed to initialize orbit from tle!"TXT_NORM"\n");
    exit(1);
  }

  // Create observer object
  predict_observer_t *obs = predict_create_observer("Self", 51.9*M_PI/180.0, -1.39*M_PI/180.0, 0);
  if(!obs)
  {
    printf(TXT_RED"Failed to initialize observer!"TXT_NORM"\n");
    exit(1);
  }

  printf(TXT_GRN"OK"TXT_NORM"\n");
  
  double current_julian;
  time_t current_time;

  struct tm next_aos_local, next_los_local;
  char next_aos_string[32], next_los_string[32];
  double next_aos_julian, next_los_julian;
  time_t next_aos, next_los;

  struct predict_observation iss_observation;

  printf("Connecting to rotator..      ");

  if(gs232_open_port() < 0)
  {
    printf(TXT_RED"Error opening rotator serial port!"TXT_NORM"\n");
    exit(1);
  }

  if(gs232_get_position(&antenna.azimuth, &antenna.elevation) < 0)
  {
    printf(TXT_RED"Error querying rotator position!"TXT_NORM"\n");
    exit(1);
  }

  printf(TXT_GRN"OK"TXT_NORM" (AZ: %03d, EL: %03d)\n",
    antenna.azimuth,
    antenna.elevation
  );

  antenna_previous.azimuth = antenna.azimuth;
  antenna_previous.elevation = antenna.elevation;

  printf("\n");

  while(1)
  {
    current_time = time(NULL);
    current_julian = predict_to_julian(current_time);
  
    if(predict_orbit(iss_tle, &iss_orbit, current_julian) < 0)
    {
      fprintf(stderr, "Failed to calculate orbit!");
      exit(1);
    }

    predict_observe_orbit(obs, &iss_orbit, &iss_observation);

    printf("Current ISS Position: AZ: %03.1f, EL: %03.1f\n",
      (iss_observation.azimuth*180.0/M_PI),
      (iss_observation.elevation*180.0/M_PI)
    );

    if(iss_observation.elevation > 0)
    {
      /* Pass in progress */

      /* Azimuth */
      if(((iss_observation.azimuth*180.0/M_PI) - antenna.azimuth) > TRACK_THRESHOLD)
      {
        if(((iss_observation.azimuth*180.0/M_PI) - antenna.azimuth) <= 2 * TRACK_THRESHOLD)
        {
          antenna.azimuth += 2 * TRACK_THRESHOLD;
        }
        else
        {
          antenna.azimuth = (iss_observation.azimuth*180.0/M_PI) + TRACK_THRESHOLD;
        }

      }
      else if((antenna.azimuth - (iss_observation.azimuth*180.0/M_PI)) > TRACK_THRESHOLD)
      {
        if((antenna.azimuth - (iss_observation.azimuth*180.0/M_PI)) <= 2 * TRACK_THRESHOLD)
        {
          antenna.azimuth -= 2 * TRACK_THRESHOLD;
        }
        else
        {
          antenna.azimuth = (iss_observation.azimuth*180.0/M_PI) - TRACK_THRESHOLD;
        }
      }

      /* Elevation */
      if(((iss_observation.elevation*180.0/M_PI) - antenna.elevation) > TRACK_THRESHOLD)
      {
        if(((iss_observation.elevation*180.0/M_PI) - antenna.elevation) <= 2 * TRACK_THRESHOLD)
        {
          antenna.elevation += 2 * TRACK_THRESHOLD;
        }
        else
        {
          antenna.elevation = (iss_observation.elevation*180.0/M_PI) + TRACK_THRESHOLD;
        }
      }
      else if((antenna.elevation - (iss_observation.elevation*180.0/M_PI)) > TRACK_THRESHOLD)
      {
        if(antenna.elevation >= 2 * TRACK_THRESHOLD)
        {
          if((antenna.elevation - (iss_observation.elevation*180.0/M_PI)) <= 2 * TRACK_THRESHOLD)
          {
            antenna.elevation -= 2 * TRACK_THRESHOLD;
          }
          else
          {
            antenna.elevation = (iss_observation.elevation*180.0/M_PI) - TRACK_THRESHOLD;
          }
        }
        else
        {
          antenna.elevation = 0;
        }
      }

      printf("Pass in progress, pointing AZ: %d, EL: %d\n",
        antenna.azimuth,
        antenna.elevation
      );
    }
    else
    {
      /* Check time to next AoS */
      next_aos_julian = predict_next_aos(obs, iss_tle, current_julian);
      next_aos = predict_from_julian(next_aos_julian);

      if((next_aos - current_time) < 60)
      {
        /* Pass coming soon, align dish! */
        localtime_r(&next_aos, &next_aos_local);
        strftime(next_aos_string,32,"%Y-%m-%d %H:%M:%SZ",&next_aos_local);

        predict_orbit(iss_tle, &iss_orbit, next_aos_julian);
        predict_observe_orbit(obs, &iss_orbit, &iss_observation);

        antenna.azimuth = iss_observation.azimuth*180.0/M_PI;
        antenna.elevation = 0;

        printf("Aligning antenna to %d for upcoming AoS: %s\n",
          antenna.azimuth,
          next_aos_string
        );
      }
      else
      {
	      /* Find following LoS */
	      next_los_julian = predict_next_los(obs, iss_tle, next_aos_julian);
	      next_los = predict_from_julian(next_los_julian);

	      /* Calculate maximum elevation of upcoming pass */
	      predict_orbit(iss_tle, &iss_orbit, next_aos_julian + ((next_los_julian - next_aos_julian) / 2));
	      predict_observe_orbit(obs, &iss_orbit, &iss_observation);

        /* Park Dish */
        localtime_r(&next_aos, &next_aos_local);
        strftime(next_aos_string,32,"%Y-%m-%d %H:%M:%S",&next_aos_local);

        localtime_r(&next_los, &next_los_local);
        strftime(next_los_string,32,"%H:%M:%S",&next_los_local);

        printf("Antenna parked. Next AoS: %s - %s (max elevation: %.1f)\n",
          next_aos_string,
          next_los_string,
          (iss_observation.elevation*180.0/M_PI)
        );

        antenna.azimuth = ANTENNA_PARK_AZIMUTH;
        antenna.elevation = ANTENNA_PARK_ELEVATION;
      }

    }

    int az, el;
    if(gs232_get_position(&az, &el) < 0)
    {
      fprintf(stderr, "Error querying rotator position!");
      exit(1);
    }

    printf("Current antenna position AZ: %d, EL: %d\n", az, el);

    if(antenna.azimuth != antenna_previous.azimuth || antenna.elevation != antenna_previous.elevation)
    {
      printf("Commanding antenna to AZ: %d, EL: %d\n",
        antenna.azimuth,
        antenna.elevation
      );

      gs232_set_position(antenna.azimuth, antenna.elevation);

      antenna_previous.azimuth = antenna.azimuth;
      antenna_previous.elevation = antenna.elevation;
    }

    //Sleep
    usleep(TRACK_INTERVAL_MS * 1000);
  }

  // Free memory
  predict_destroy_orbital_elements(iss_tle);
  predict_destroy_observer(obs);

  return 0;
}
