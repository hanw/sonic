#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <time.h>
#include <getopt.h>

#include "sonic.h"
#include "crc32.h"
//#include "config.h"

#define NNLOOP	10

static char * program;
struct sonic_priv * sonic;

void usage() 
{
    fprintf(stderr, "Usage %s [-n loops] [-b packet size] [-e packet size]"
            " [-t step]\n" , program);
    exit(EXIT_FAILURE);
}

int main(int argc, char *argv[])
{
    char **p = argv, ret=0;
    struct sonic_priv *sonic = NULL;

    program = argv[0];

    setvbuf(stdout, NULL, _IONBF, 0);

    crc32init_le();

    if ( __sonic_init(&sonic) != 0) 
        exit(EXIT_FAILURE); 

    sonic_get_config_runtime_args(&sonic->runtime_args, ++p);

#if SONIC_DDEBUG
    sonic_print_config_runtime_args(&sonic->runtime_args);
#endif

    ret = __sonic_run(sonic);

    __sonic_exit(sonic);

    return ret;
}
