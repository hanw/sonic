#ifndef __SONIC_MODE__
#define __SONIC_MODE__

#if !SONIC_KERNEL
#include <stdint.h>
#else
#include <linux/types.h>
#endif

/* sonic operating mode */
#define SONIC_MODE_ARGS_TESTER \
    /* Basic Tester */                                                      \
    SONIC_MODE( 0, null_port)                       \
    SONIC_MODE( 1, crc_tester)                            \
    SONIC_MODE( 2, encode_tester)                      \
    SONIC_MODE( 3, decode_tester)

#define SONIC_MODE_ARGS_BASIC   \
    SONIC_MODE( 10, pkt_gen)                         \
    SONIC_MODE( 11, pkt_rpt)                         \
    SONIC_MODE( 12, pkt_vrpt)                       \
    SONIC_MODE( 20, pkt_rcv)                         \
    SONIC_MODE( 21, pkt_cap)                         \
    SONIC_MODE( 30, pkt_gencap)                   \
    SONIC_MODE( 31, arp)                            \
    SONIC_MODE( 32, pcs_cross)                      \
    SONIC_MODE( 33, pcs_app_cross)

#if SONIC_SFP
#define SONIC_MODE_ARGS_SFP                                      \
    /* DMA TESTER */                                             \
    SONIC_MODE( 50, rx_dma)                \
    SONIC_MODE( 51, tx_dma)                \
    SONIC_MODE( 52, rxtx_dma)            \
    /* IDLE TESTER */                                            \
    SONIC_MODE( 60, rx_idle)              \
    SONIC_MODE( 61, tx_idle)              \
    SONIC_MODE( 62, rxtx_idle)          
#endif

#if !SONIC_KERNEL 
#define SONIC_MODE_ARGS SONIC_MODE_ARGS_TESTER SONIC_MODE_ARGS_BASIC        \

//    SONIC_MODE( 50, MAX, null_tester, null_tester)              
#elif SONIC_SFP 
#define SONIC_MODE_ARGS SONIC_MODE_ARGS_BASIC SONIC_MODE_ARGS_SFP           \

//    SONIC_MODE( 70, MAX, null_tester, null_tester)              
#else 
#define SONIC_MODE_ARGS SONIC_MODE_ARGS_BASIC                               \

//    SONIC_MODE( 50, MAX, null_tester, null_tester)
#endif 

#if 0
enum sonic_mode{
#define SONIC_MODE(x,y) MODE_##y = x,
    SONIC_MODE_ARGS
#undef SONIC_MODE
};
#endif


#endif /*__SONIC_MODE__*/
