#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include "sonic.h"

#define SONICPORT0	"/home/kslee/dev/sonic_port0"
#define SONICPORT1	"/home/kslee/dev/sonic_port1"

struct log_entry {
	short type;
	short len;
	uint32_t idle;
	uint32_t idle_bits;
	short error;
	short packet_bits;

	uint8_t buf[48];
};

static void exit_fail_msg(char * msg) 
{
	fprintf(stderr, "%s:%s\n", msg, strerror(errno));
	exit(EXIT_FAILURE);
}

int main(int argc, char **argv)
{
	int fd, ret, i, j, port;
	unsigned long long l=0;
	char *dev;
	struct sonic_log *log;
	struct log_entry *logs;
	FILE *out;

	if (argc != 3) {
		fprintf(stderr, "usage: %s <port> <output>\n", argv[0]);
		exit(EXIT_FAILURE);
	}

	port = atoi(argv[1]);	
	if (port == 0)
		dev = SONICPORT0;
	else if (port == 1)
		dev = SONICPORT1;
	else {
		fprintf(stderr, "port number must be zero or one\n");
		exit(EXIT_FAILURE);
	}

	out= fopen(argv[2], "w");

	log = malloc(4096 * 16);
	if (!log)
		exit_fail_msg("Memory Error");

	if ((fd = open(dev, O_RDONLY)) == -1)
		exit_fail_msg("File Opening Error");

	while((ret = read(fd, log, 4096 * 16))) {
//		if (l >= 1000000UL)
//			continue;
		logs = (struct log_entry *) log->logs;

//		for ( i = 1 ; i < 4096 * 16 / sizeof(struct log_entry) ; i ++ ) {
		for ( i = 0 ; i < log->log_cnt ; i ++ ) {
			fprintf(out, "%u %u %u %u ", 
				logs->type, logs->len, logs->idle, logs->idle_bits);

			for ( j = 0 ; j < 48 ; j ++ ) {
				fprintf(out, "%.2x", logs->buf[j]);

				if (j % 4 == 3)
					fprintf(out, " ");
			}
			fprintf(out, "\n");

			logs++;

		}
		l += i;
	}

	close(fd);
	free(log);
	fclose(out);

	exit(EXIT_SUCCESS);
}
