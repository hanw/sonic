#ifndef __SONIC_CONFIG__
#define __SONIC_CONFIG__

#if !SONIC_KERNEL
#include <stdint.h>
#else
#include <linux/types.h>
#endif

#define SONIC_PKT_GEN

#define SONIC_CONFIG_RUNTIME_ARGS                       \
    SONIC_CONFIG_RUNTIME(nloops, 1)                     \
    SONIC_CONFIG_RUNTIME(verbose, 1)                    \
    /*SONIC_CONFIG_RUNTIME(mode, 1) */                      \
    SONIC_CONFIG_RUNTIME(duration, 10)                   

#define SONIC_CONFIG_RUNTIME_ARR_ARGS                   \
    SONIC_CONFIG_RUNTIME(mode, "null_port", "null_port", 16) 

// packet gen modes
#define SONIC_PKT_GEN_HOMOGENEOUS   1
#define SONIC_PKT_GEN_CHAIN         2
#define SONIC_PKT_GEN_COVERT        3

// Packet gen related
#define SONIC_CONFIG_PKT_GEN_INT_ARGS                   \
    SONIC_CONFIG_RUNTIME(gen_mode, 1, 1)                \
    SONIC_CONFIG_RUNTIME(pkt_len, 1518, 1518)           \
    SONIC_CONFIG_RUNTIME(batching, -1, -1)              \
    SONIC_CONFIG_RUNTIME(idle, 13738, 13738)                  \
    SONIC_CONFIG_RUNTIME(pkt_cnt, 1000000, 1000000)           \
    SONIC_CONFIG_RUNTIME(chain_idle, 1200, 1200)            \
    SONIC_CONFIG_RUNTIME(chain_num, 20, 20)         \
    SONIC_CONFIG_RUNTIME(wait, 5, 5)                    \
    SONIC_CONFIG_RUNTIME(multi_queue, 1, 1)             \
    SONIC_CONFIG_RUNTIME(delta, 1018, 1018)             \
    SONIC_CONFIG_RUNTIME(delay, 0, 248)             

#define SONIC_CONFIG_PORT_INT_ARGS                      \
    SONIC_CONFIG_RUNTIME(port_src, 5000, 5000)          \
    SONIC_CONFIG_RUNTIME(port_dst, 5008, 5008)          \
    SONIC_CONFIG_RUNTIME(vlan_id, 2202, 2202)

#define SONIC_CONFIG_RUNTIME_INT_ARR_ARGS       \
    SONIC_CONFIG_PKT_GEN_INT_ARGS               \
    SONIC_CONFIG_PORT_INT_ARGS

// Packet Gen Related
#define SONIC_CONFIG_PORT_STR_ARGS                                   \
    SONIC_CONFIG_RUNTIME(mac_src, "00:60:dd:45:39:0d", "02:80:c2:00:10:23", 18) \
    SONIC_CONFIG_RUNTIME(mac_dst, "00:07:43:12:5a:e0", "00:60:dd:45:39:5a", 18) \
    SONIC_CONFIG_RUNTIME(ip_src, "192.168.2.23", "192.168.3.63", 16)          \
    SONIC_CONFIG_RUNTIME(ip_dst, "192.168.2.10", "192.168.3.43", 16)          \
    SONIC_CONFIG_RUNTIME(covert_msg, "Hello World", "Hello World", 12)

#define SONIC_CONFIG_RUNTIME_STR_ARR_ARGS       \
    SONIC_CONFIG_RUNTIME_ARR_ARGS               \
    SONIC_CONFIG_PORT_STR_ARGS

struct sonic_config_runtime_port_args {
#define SONIC_CONFIG_RUNTIME(x,y,z) int x;
    SONIC_CONFIG_RUNTIME_INT_ARR_ARGS
#undef SONIC_CONFIG_RUNTIME
#define SONIC_CONFIG_RUNTIME(x,y,z,w) char x[w];
    SONIC_CONFIG_RUNTIME_STR_ARR_ARGS
#undef SONIC_CONFIG_RUNTIME
};

struct sonic_config_runtime_args {
#define SONIC_CONFIG_RUNTIME(x,y) int x;
    SONIC_CONFIG_RUNTIME_ARGS
#undef SONIC_CONFIG_RUNTIME
    struct sonic_config_runtime_port_args ports[2];
    /*
#define SONIC_CONFIG_RUNTIME(x,y,z) int x[2];
    SONIC_CONFIG_RUNTIME_INT_ARR_ARGS
#undef SONIC_CONFIG_RUNTIME
#define SONIC_CONFIG_RUNTIME(x,y,z,w) char x[2][w];
    SONIC_CONFIG_RUNTIME_STR_ARR_ARGS
#undef SONIC_CONFIG_RUNTIME
    */
};

/* Do not modify these values if you do not know what you are modifying */
#define SONIC_CONFIG_SYSTEM_ARGS                        \
    SONIC_CONFIG_SYSTEM(tx_dma_exp, 4)                  \
    SONIC_CONFIG_SYSTEM(tx_dma_num_desc, 1)             \
    SONIC_CONFIG_SYSTEM(rx_dma_exp, 3)                  \
    SONIC_CONFIG_SYSTEM(rx_dma_num_desc, 1)             \
    SONIC_CONFIG_SYSTEM(fifo_exp, 4)                    \
    SONIC_CONFIG_SYSTEM(fifo_num_ent, 5)                \
    SONIC_CONFIG_SYSTEM(ctrl_cpu, 0)

/* Do not modify these values if you do not know what you are doing */
#define SONIC_CONFIG_SYSTEM_ARR_ARGS                   \
    SONIC_CONFIG_SYSTEM(port_socket, 1, 0)

struct sonic_config_system_args {
#define SONIC_CONFIG_SYSTEM(x,y) int x;
    SONIC_CONFIG_SYSTEM_ARGS
#undef SONIC_CONFIG_SYSTEM
#define SONIC_CONFIG_SYSTEM(x,y,z) int x[2];
    SONIC_CONFIG_SYSTEM_ARR_ARGS
#undef SONIC_CONFIG_SYSTEM
};
#endif /* __SONIC_CONFIG__ */
