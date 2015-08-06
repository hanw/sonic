#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <getopt.h>

#include "sonic.h"

#define SONIC_DEV_CONTROL   "/dev/sonic_control"
#define SONIC_DEV_PORT0     "/dev/sonic_port0"
#define SONIC_DEV_PORT1     "/dev/sonic_port1"

static int sonic_verbose=0;

static void usage() 
{
    fprintf(stderr, "Usage: ...\n");
    exit(EXIT_FAILURE);
}

static void exit_fail_msg(char * msg) 
{
    fprintf(stderr, "%s:%s\n", msg, strerror(errno));
    exit(EXIT_FAILURE);
}

static void sonic_print_config_runtime_port_args(struct sonic_config_runtime_port_args *args)
{
#define SONIC_CONFIG_RUNTIME(x,y,z) \
    printf( "%-20s[0] = %d \n", #x, args->x);
    SONIC_CONFIG_RUNTIME_INT_ARR_ARGS
#undef SONIC_CONFIG_RUNTIME
#define SONIC_CONFIG_RUNTIME(x,y,z,w)	\
    printf( "%-20s[0] = %s \n", #x, args->x);
    SONIC_CONFIG_RUNTIME_STR_ARR_ARGS
#undef SONIC_CONFIG_RUNTIME
}

static struct sonic_config_runtime_port_args default_args_port0 = {
    .port_mode = 1,
    .gen_mode = 1,
    .pkt_len = 792,
    .batching = -1,
    .idle = 151074,
    .pkt_cnt = 10000,
    .wait = 5,
    .port_src = 5000,
    .port_dst = 5000,
    .mac_src = "00:60:dd:45:39:05",
    .mac_dst = "00:07:43:12:5a:e0",
    .ip_src = "192.168.2.23",
    .ip_dst = "192.168.2.10",
    .vlan_id = 2206
};

static int sonic_preprocess(char *file)
{
    
    return 0;
}

static int sonic_postprocess(char *file)
{

    return 0;
}

int main(int argc, char **argv)
{
    int fd, c, ret=0;
    char *control = SONIC_DEV_CONTROL;
    char *port[2] = {SONIC_DEV_PORT0, SONIC_DEV_PORT1};
    char *input = NULL, *output = NULL;
    struct sonic_config_runtime_port_args args;
    unsigned int nloops=1;
    int duration=10, mode=30;

    memcpy(&args, &default_args, sizeof(args));

    // check args
    while (1) {
        int option_index=0;
        static struct option long_options[] = {
            {"nloop", 1, 0, 'n'},
            {"pkt_len", 1, 0, 'l'},
            {"pkt_cnt", 1, 0, 'c'},
            {"idle", 1, 0, 'i'},
            {"duration", 1, 0, 'd'},
            {"wait", 1, 0, 'w'},
            {"mode", 1, 0, 'm'},
            {"input", 1, 0, 'r'},
            {"output", 1, 0, 'p'},

            /* mac ip port */
            {0, 0, 0, 0},
        };

        c = getopt_long(argc, argv, "n:l:c:i:d:w:m:r:p:v", 
                long_options, &option_index);
        if (c == -1)
            break;

        switch (c) {
        case 'n':
            nloops = atoi(optarg);
            break;
        case 'l':
            args.pkt_len = atoi(optarg);
            break;
        case 'c':
            args.pkt_cnt = atoi(optarg);
            break;
        case 'i':
            args.idle = atoi(optarg);
            break;
        case 'd':
            duration = atoi(optarg);
            break;
        case 'w':
            args.wait = atoi(optarg);
            break;
        case 'm':
            mode = atoi(optarg);
            break;
        case 'r':
            input = optarg;
            break;
        case 'p':
            output = optarg;
            break;
        case 'v':
            sonic_verbose=1;
        default:
            usage();
        }
    }

    if (sonic_verbose == 1) {
        printf("mode = %d, duration = %d nloop = %d\n", mode, duration, nloops);
        sonic_print_config_runtime_port_args(&args);
        printf("input = %s\n", input ? input : "NONE");
        printf("output = %s\n", output ? output : "NONE");
    }
    

    if ((fd = open(control, O_RDWR)) == -1) 
        exit_fail_msg("File Opening Error");

    int i;
    struct sonic_sonic_stat stat;
    for ( i = 0; i < nloops; i ++ ) {
        if (ioctl(fd, SONIC_IOC_PORT0_INFO_SET, &args)) {
            SONIC_ERROR("ioctl %s failed\n", "SONIC_IOC_PORT0_INFO_SET");
        }

        if (ioctl(fd, SONIC_IOC_SET_MODE, mode)) {
            SONIC_ERROR("ioctl %s %d failed\n", "SONIC_IOC_SET_MODE", mode);
        }

        SONIC_PRINT("SoNIC is running for %d seconds...\n", duration);
        if ((ret = ioctl(fd, SONIC_IOC_RUN, duration))) {
            SONIC_ERROR("SoNIC failed\n");
            break;
        }

        if (ioctl(fd, SONIC_IOC_GET_STAT, &stat)) {
            SONIC_ERROR("ioctl %s failed\n", "SONIC_IOC_GET_STAT");
        }
            
        sonic_print_stats(&stat);
    }

//	while ((ret = read(fd, buf, 4096))) {
		

//	}
    close(fd);

    return ret;
}
