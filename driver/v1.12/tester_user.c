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
#include "tester.h"

#define NNLOOP	10

static char * program;

void usage() 
{
    fprintf(stderr, "Usage %s [-n loops] [-b packet size] [-e packet size]"
            " [-t step]\n" , program);
    exit(EXIT_FAILURE);
}

static struct sonic_priv * sonic_alloc_sonic()
{
    struct sonic_priv *sonic = calloc(1, sizeof (struct sonic_priv));
    if (!sonic)
        return NULL;

    return sonic;
}

int main(int argc, char *argv[])
{
    char **p = argv;
    struct tester_args args;
    struct sonic_priv *sonic = sonic_alloc_sonic();
    if(!sonic) {
    SONIC_ERROR("sonic alloc failed\n");
    exit(EXIT_FAILURE);
    }

    program = argv[0];

    setvbuf(stdout, NULL, _IONBF, 0);

    sonic_init_tester_args(&args);

    sonic_get_option(&args, ++p);

    crc32init_le();

    memcpy(&sonic->tester_args, &args, sizeof(struct tester_args));

    if(sonic_alloc_ports (sonic)) {
    SONIC_ERROR("sonic_alloc_ports failed\n");
    exit(EXIT_FAILURE);
    };

    sonic_tester(sonic);

    sonic_free_ports(sonic);

    free(sonic);

    exit(EXIT_SUCCESS);    
}
