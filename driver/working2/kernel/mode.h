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
    SONIC_MODE( 0, CRC_CMP, null_tester, null_tester)                       \
    SONIC_MODE( 1, CRC, crc_tester, null_tester)                            \
    SONIC_MODE( 2, ENCODE, encode_tester, null_tester)                      \
    SONIC_MODE( 3, DECODE, decode_tester, null_tester)

#define SONIC_MODE_ARGS_BASIC   \
    SONIC_MODE( 10, PKT_GEN0, pkt_gen, null_tester)                         \
    SONIC_MODE( 11, PKT_GEN1, null_tester, pkt_gen)                         \
    SONIC_MODE( 12, PKT_GEN_FULL, pkt_gen, pkt_gen)                         \
    SONIC_MODE( 13, PKT_RPT0, pkt_rpt, null_tester)                         \
    SONIC_MODE( 14, PKT_RPT1, null_tester, pkt_rpt)                         \
    SONIC_MODE( 15, PKT_RPT_FULL, pkt_rpt, pkt_rpt)                         \
    SONIC_MODE( 16, PKT_VRPT0, pkt_vrpt, null_tester)                       \
    SONIC_MODE( 17, PKT_VRPT1, null_tester, pkt_vrpt)                       \
    SONIC_MODE( 18, PKT_VRPT_FULL, pkt_vrpt, pkt_vrpt)                      \
    SONIC_MODE( 20, PKT_RCV0, pkt_rcv, null_tester)                         \
    SONIC_MODE( 21, PKT_RCV1, null_tester, pkt_rcv)                         \
    SONIC_MODE( 22, PKT_RCV_FULL, pkt_rcv, pkt_rcv)                         \
    SONIC_MODE( 23, PKT_CAP0, pkt_cap, null_tester)                         \
    SONIC_MODE( 24, PKT_CAP1, null_tester, pkt_cap)                         \
    SONIC_MODE( 25, PKT_CAP_FULL, pkt_cap, pkt_cap)                         \
    SONIC_MODE( 30, PKT_GEN_CAP, pkt_gen, pkt_cap)                          \
    SONIC_MODE( 31, PKT_CAP_GEN, pkt_cap, pkt_gen)                          \
    SONIC_MODE( 32, PKT_RPT_CAP, pkt_rpt, pkt_cap)                          \
    SONIC_MODE( 33, PKT_CAP_RPT, pkt_cap, pkt_rpt)                          \
    SONIC_MODE( 34, PKT_VRPT_CAP, pkt_vrpt, pkt_cap)                        \
    SONIC_MODE( 35, PKT_CAP_VRPT, pkt_cap, pkt_vrpt)                        \
    SONIC_MODE( 36, PKT_VRPT_GEN, pkt_vrpt, pkt_gen)                        \
    SONIC_MODE( 37, PKT_GEN_VRPT, pkt_gen, pkt_vrpt)                        \
    SONIC_MODE( 38, PKT_GENCAP0, pkt_gencap, null_tester)                   \
    SONIC_MODE( 39, PKT_GENCAP1, null_tester, pkt_gencap)                   \
    SONIC_MODE( 40, PKT_GENCAP_FULL, pkt_gencap, pkt_gencap)                 

#if SONIC_SFP
#define SONIC_MODE_ARGS_SFP                                      \
    /* DMA TESTER */                                             \
    SONIC_MODE( 50, DMA_RX0, rx_dma, null_tester)                \
    SONIC_MODE( 51, DMA_RX1, null_tester, rx_dma)                \
    SONIC_MODE( 52, DMA_TX0, tx_dma, null_tester)                \
    SONIC_MODE( 53, DMA_TX1, null_tester, tx_dma)                \
    SONIC_MODE( 54, DMA_RXTX0, rxtx_dma, null_tester)            \
    SONIC_MODE( 55, DMA_RXTX1, null_tester, rxtx_dma)            \
    SONIC_MODE( 56, DMA_TXTX, tx_dma, tx_dma)                    \
    SONIC_MODE( 57, DMA_RXRX, rx_dma, rx_dma)                    \
    SONIC_MODE( 58, DMA_FULL, rxtx_dma, rxtx_dma)                \
    SONIC_MODE( 59, DMA_MAX, null_tester, null_tester)           \
    /* IDLE TESTER */                                            \
    SONIC_MODE( 60, IDLE_RX0, rx_idle, null_tester)              \
    SONIC_MODE( 61, IDLE_RX1, null_tester, rx_idle)              \
    SONIC_MODE( 62, IDLE_TX0, tx_idle, null_tester)              \
    SONIC_MODE( 63, IDLE_TX1, null_tester, tx_idle)              \
    SONIC_MODE( 64, IDLE_RXTX0, rxtx_idle, null_tester)          \
    SONIC_MODE( 65, IDLE_RXTX1, null_tester, rxtx_idle)          \
    SONIC_MODE( 66, IDLE_TXTX, tx_idle, tx_idle)                 \
    SONIC_MODE( 67, IDLE_RXRX, rx_idle, rx_idle)                 \
    SONIC_MODE( 68, IDLE_FULL, rxtx_idle, rxtx_idle)             \
    SONIC_MODE( 69, IDLE_RXTX, rx_idle, tx_idle)                 
#endif

#if !SONIC_KERNEL 
#define SONIC_MODE_ARGS SONIC_MODE_ARGS_TESTER SONIC_MODE_ARGS_BASIC        \
    SONIC_MODE( 50, MAX, null_tester, null_tester)              
#elif SONIC_SFP 
#define SONIC_MODE_ARGS SONIC_MODE_ARGS_BASIC SONIC_MODE_ARGS_SFP           \
    SONIC_MODE( 70, MAX, null_tester, null_tester)              
#else 
#define SONIC_MODE_ARGS SONIC_MODE_ARGS_BASIC                               \
    SONIC_MODE( 50, MAX, null_tester, null_tester)
#endif 

enum sonic_mode{
#define SONIC_MODE(x,y,z,w) MODE_##y = x,
    SONIC_MODE_ARGS
#undef SONIC_MODE
};


#endif /*__SONIC_MODE__*/
