#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "sonic.h"
#include "ioctl.h"

#define SONIC_DEV_PATH	"/home/kslee/dev/sonic"

static void exit_fail_msg(char * msg) 
{
	fprintf(stderr, "%s:%s\n", msg, strerror(errno));
	exit(EXIT_FAILURE);
}

void print_gen_info(struct sonic_pkt_gen_info * info)
{
	printf("mac src = %s, mac dst = %s\n", info->mac_src, info->mac_dst);
	printf("ip src = %s, ip dst = %s\n", info->ip_src, info->ip_dst);
	printf("port src = %d, port dst = %d\n", info->port_src, info->port_dst);
}

int main(int argc, char **argv)
{
	int fd;
	char *dev = SONIC_DEV_PATH;
//	char buf[4096];
	struct sonic_pkt_gen_info info = {
		.mode 		= 0,
		.pkt_num 	= 100000UL,
		.pkt_len 	= 1518,
		.mac_src	= "00:1b:21:29:b0:bf",
		.mac_dst	= "00:1b:21:29:b1:95",
		.ip_src 	= "192.168.0.1",
		.ip_dst		= "192.168.0.2",
		.port_src	= 5000,
		.port_dst	= 5000,
	};

	if ((fd = open(dev, O_RDWR)) == -1) 
		exit_fail_msg("File Opening Error");

	if (ioctl(fd, SONIC_IOC_RESET)) {
		SONIC_ERROR("ioctl %d failed\n", SONIC_IOC_RESET);
	}

	if (ioctl(fd, SONIC_IOC_PORT0_INFO_SET, &info)) {
		SONIC_ERROR("ioctl %lu failed\n", SONIC_IOC_PORT0_INFO_SET);
	}

	if (ioctl(fd, SONIC_IOC_START)) {
		SONIC_ERROR("ioctl %d failed\n", SONIC_IOC_START);
	}

	if (ioctl(fd, SONIC_IOC_STOP)) {
		SONIC_ERROR("ioctl %d failed\n", SONIC_IOC_STOP);
	}

	if (ioctl(fd, SONIC_IOC_SET_MODE, 10)) {
		SONIC_ERROR("ioctl %lu failed\n", SONIC_IOC_SET_MODE);
	}

	if (ioctl(fd, SONIC_IOC_RUN, 30)) {
		SONIC_ERROR("ioctl %lu failed\n", SONIC_IOC_RUN);
	}

//	while ((ret = read(fd, buf, 4096))) {
		

//	}

	close(fd);

	exit(EXIT_SUCCESS);
}
