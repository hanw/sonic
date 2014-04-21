#if! SONIC_KERNEL
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#endif /*SONIC_KERNEL*/
#include "sonic.h"
#include "mode.h"
//#include "covert.h"

int sonic_verbose=2;

#define SONIC_CONFIG_SYSTEM(x,y) \
static int sonic_ ##x = y;
SONIC_CONFIG_SYSTEM_ARGS
#undef SONIC_CONFIG_SYSTEM

#define SONIC_CONFIG_SYSTEM(x,y,z)\
static int sonic_ ##x[2] = {y, z}; 
SONIC_CONFIG_SYSTEM_ARR_ARGS
#undef SONIC_CONFIG_SYSTEM

#if SONIC_KERNEL
// Module Parameters
#define SONIC_CONFIG_SYSTEM(x,y) \
module_param(sonic_ ##x, int, 0444); 
SONIC_CONFIG_SYSTEM_ARGS
#undef SONIC_CONFIG_SYSTEM

#define SONIC_CONFIG_SYSTEM(x,y,z) \
module_param_array(sonic_ ##x, int, NULL, 0444);
SONIC_CONFIG_SYSTEM_ARR_ARGS
#undef SONIC_CONFIG_SYSTEM
#endif /* SONIC_KERNEL */

void sonic_print_config_system_args(struct sonic_config_system_args *args)
{

    SONIC_DPRINT("SONIC_KERNEL : %d\n", SONIC_KERNEL);
#if SONIC_KERNEL
    SONIC_DPRINT("SONIC_PCIE : %d\n", SONIC_PCIE);
    SONIC_DPRINT("SONIC_PROC : %d\n", SONIC_PROC);
    SONIC_DPRINT("SONIC_FS : %d\n", SONIC_FS);
    SONIC_DPRINT("SONIC_SFP : %d\n", SONIC_SFP);
    SONIC_DPRINT("SONIC_DISABLE_IRQ : %d\n", SONIC_DISABLE_IRQ);
#endif
    SONIC_DPRINT("SONIC_TWO_PORTS : %d\n", SONIC_TWO_PORTS);

#define SONIC_CONFIG_SYSTEM(x,y)   SONIC_DPRINT( "%-20s = %d\n", #x, args->x);
    SONIC_CONFIG_SYSTEM_ARGS
#undef SONIC_CONFIG_SYSTEM
#define SONIC_CONFIG_SYSTEM(x,y,z) \
    SONIC_DPRINT( "%-20s[0] = %d [1] = %d\n", #x, args->x [0], args->x [1]);
    SONIC_CONFIG_SYSTEM_ARR_ARGS
#undef SONIC_CONFIG_SYSTEM
}

void sonic_print_config_runtime_args(struct sonic_config_runtime_args *args)
{
#define SONIC_CONFIG_RUNTIME(x,y)   SONIC_DPRINT( "%-20s = %d\n", #x, args->x);
    SONIC_CONFIG_RUNTIME_ARGS
#undef SONIC_CONFIG_RUNTIME
#define SONIC_CONFIG_RUNTIME(x,y,z) \
    SONIC_DPRINT( "%-20s[0] = %d [1] = %d\n", #x, args->ports[0].x, args->ports[1].x);
    SONIC_CONFIG_RUNTIME_INT_ARR_ARGS
#undef SONIC_CONFIG_RUNTIME
#define SONIC_CONFIG_RUNTIME(x,y,z,w)	\
    SONIC_DPRINT( "%-20s[0] = %s [1] = %s\n", #x, args->ports[0].x, args->ports[1].x);
    SONIC_CONFIG_RUNTIME_STR_ARR_ARGS
#undef SONIC_CONFIG_RUNTIME
}

void sonic_check_config_system_args(struct sonic_config_system_args *args)
{
    // TODO
}

void sonic_check_config_runtime_args(struct sonic_config_runtime_args *args)
{
    int i=0;
    struct sonic_config_runtime_port_args *pargs;

    if (args->nloops <= 0) {
        SONIC_ERROR("nloops must be a positive number\n");
        args->nloops = 1;
    }
    if (args->verbose < 0 || args->verbose > 2) {
        args->verbose = 0;
    }
    if (args->mode > MODE_MAX || args->mode < 0) {
        SONIC_ERROR("Invalid test mode. Set to 10\n");
        args->mode=1;
    }
#if SONIC_KERNEL
    if (args->mode < 10)  {
        SONIC_ERROR("Invalid test mode. Set to 10\n");
        args->mode=10;
    }
#endif
    if (args->duration <= 0) {
        SONIC_ERROR("Duration must be a positive number. Set to 30\n");
        args->duration = 30;
    }

    // TODO FIFO_ARGS and SYSTEM_ARR_ARGS

    for ( i = 0 ; i < SONIC_PORT_SIZE ; i ++ ) {
        pargs = &args->ports[i];
        // TODO: packet generator modes
//        if (args->gen_mode[i] > MODE_MAX || args->gen_mode[i] < 0) {
//            SONIC_ERROR("Invalid test mode\n");
//            args->gen_mode[i] = 1;
//        }
        if (pargs->pkt_len < 64 || pargs->pkt_len > 1518) {
            SONIC_ERROR("Invalid packet length\n");
            pargs->pkt_len = 1518;
        }
        if (pargs->idle <= 0) {
            SONIC_ERROR("Idle must be a positive integer\n");
            pargs->idle = 12;
        }
        if (pargs->pkt_cnt <= 0) {
            SONIC_ERROR("Packet number must be a positive integer\n");
            pargs->pkt_cnt = 1000;
        }
        if (pargs->chain_idle <= 0) {
            SONIC_ERROR("Chain idle must be a positive integer\n");
            pargs->chain_idle = 12;
        }
        if (pargs->chain_num <= 0) {
            SONIC_ERROR("Chain number must be a positive integer\n");
            pargs->chain_idle = 1000;
        }
        if (pargs->wait >= args->duration || pargs->wait < 0) {
            SONIC_ERROR("Wait time must be a positive integer and less than duration\n");
            pargs->wait = 1;
        }
        if (pargs->multi_queue <= 0) {
            SONIC_ERROR("The number of Queue must be a positive integer\n");
            pargs->multi_queue = 1;
        }
        if (args->duration < pargs->wait) {
            pargs->wait = args->duration - 1;
        }

        /* TODO: udp info */
    }

    sonic_verbose = args->verbose;

}

static int sonic_simple_parse_int_args(char *str, int target[2])
{
    int i=0;
    char delim[]=",", *token, *tstr = str;

    while ((token = strsep(&tstr, delim)) != NULL) {
        if (*token != '\0') 
            sscanf(token, "%d", &target[i]);

        i++;
        if (i == 2)
            break;
    }

    return i;
}

static int sonic_simple_parse_str_args(char *str, int len, char target[2][len])
{
    int i=0;
    char delim[]=",", *token, *tstr = str;

    while ((token = strsep(&tstr, delim)) != NULL) {
        if (*token != '\0')
            sscanf(token, "%s", target[i]);

        i ++;
        if (i == 2)
            break;
    }

    return i;
}

static int sonic_simple_parse_config_runtime_args(
        struct sonic_config_runtime_args * args, const char * str)
{
    int ret;
    char *p = strchr(str, '=');
    if ( p == NULL ) {
        SONIC_ERROR("Wrong arguments\n");
        return -1;
    }
    *p = '\0';

// FIXME
#define SONIC_CONFIG_RUNTIME(x,y) \
    if (!strcmp(str, #x) ) sscanf(p+1, "%d", &args-> x ); else
    SONIC_CONFIG_RUNTIME_ARGS
#undef SONIC_CONFIG_RUNTIME
#define SONIC_CONFIG_RUNTIME(x,y,z)         \
    if (!strcmp(str, #x) ) {                \
        int target[2];                      \
        ret = sonic_simple_parse_int_args(p+1, target); \
        if (ret >= 1)                       \
            args->ports[0].x = target[0];   \
        if (ret == 2)                       \
            args->ports[1].x = target[1];   \
    } else
    SONIC_CONFIG_RUNTIME_INT_ARR_ARGS
#undef SONIC_CONFIG_RUNTIME
#define SONIC_CONFIG_RUNTIME(x,y,z,w)       \
    if (!strcmp(str, #x) ) {                \
        char target[2][w];                  \
        ret = sonic_simple_parse_str_args(p+1, w, target);    \
        if (ret >= 1)                       \
            strncpy(args->ports[0].x, target[0], w);    \
        if (ret == 2)                       \
            strncpy(args->ports[1].x, target[1], w);    \
    } else
    SONIC_CONFIG_RUNTIME_STR_ARR_ARGS
#undef SONIC_CONFIG_RUNTIME
        SONIC_ERROR("Unknown args %s\n", str);

    *p='=';

    return 0;
}

#if !SONIC_KERNEL
int sonic_get_config_runtime_args(struct sonic_config_runtime_args *args, char **argv)
{
    char ** p = argv;
    int ret;

    SONIC_DPRINT("\n");

    sonic_init_config_runtime_args(args);

    while (*p) {
        ret = sonic_simple_parse_config_runtime_args(args, *p);
        p++;
        if (ret == -1) 
            continue;
    }

//    sonic_check_config_runtime_args(args);

    return 0;
}
#else /* SONIC_KERNEL */
int sonic_get_config_runtime_args(struct sonic_config_runtime_args *args, char *str)
{
    int i =0, ret;
    char delim[] = " \t\n", *token, *tstr = str;

    SONIC_DPRINT("%s\n", str);

    sonic_init_config_runtime_args(args);

    while ((token = strsep(&tstr, delim)) != NULL) {
        if (*token == '\0')
            continue;

        if (i == 0) {
            if(strcmp(token, "tester")) 
                return -1;
        } else {
            ret = sonic_simple_parse_config_runtime_args(args, token);
            if (ret == -1) 
                continue;
        }
        i++;
    }
//    sonic_check_config_runtime_args(args);

    return 0;
}
#endif /* SONIC_KERNEL */

void sonic_init_config_system_args(struct sonic_config_system_args * args)
{
#define SONIC_CONFIG_SYSTEM(x,y) args->x = sonic_ ##x;
    SONIC_CONFIG_SYSTEM_ARGS
#undef SONIC_CONFIG_SYSTEM
#define SONIC_CONFIG_SYSTEM(x,y,z) \
    args->x [0] = sonic_ ##x[0]; \
    args->x [1] = sonic_ ##x[1];
    SONIC_CONFIG_SYSTEM_ARR_ARGS
#undef SONIC_CONFIG_SYSTEM

    sonic_check_config_system_args(args);
}

void sonic_init_config_runtime_args(struct sonic_config_runtime_args *args)
{
#define SONIC_CONFIG_RUNTIME(x,y) args->x = y;
    SONIC_CONFIG_RUNTIME_ARGS
#undef SONIC_CONFIG_RUNTIME
#define SONIC_CONFIG_RUNTIME(x,y,z) \
    args->ports[0].x = y; \
    args->ports[1].x = z;
    SONIC_CONFIG_RUNTIME_INT_ARR_ARGS
#undef SONIC_CONFIG_RUNTIME
#define SONIC_CONFIG_RUNTIME(x,y,z,w) \
    strncpy(args->ports[0].x, y, w);\
    strncpy(args->ports[1].x, z, w);
    SONIC_CONFIG_RUNTIME_STR_ARR_ARGS
#undef SONIC_CONFIG_RUNTIME

    sonic_check_config_runtime_args(args);
}
