
CFLAGS=-Og -Wall

all:
	gcc tracker.c gs232.c tle.c $(CFLAGS) -lpredict -lm -lcurl -o track
	gcc manual.c gs232.c $(CFLAGS) -lm -o manual
	gcc read.c gs232.c $(CFLAGS) -lm -o read