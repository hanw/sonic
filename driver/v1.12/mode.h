#ifndef __SONIC_MODE__
#define __SONIC_MODE__

#if !SONIC_KERNEL
#include <stdint.h>
#else
#include <linux/types.h>
#endif

/* sonic operating mode */
#define SONIC_MODE_ARGS_BASIC \
    /* CRC */                                                               \
    SONIC_MODE( 0, MODE_CRC_CMP, null_tester, null_tester, null)            \
    SONIC_MODE( 1, MODE_CRC, tx_mac_tester, null_tester, port)              \
    /* TX PATH */                                                           \
    SONIC_MODE( 10, MODE_ENCODE, tx_pcs_tester, null_tester, port)          \
    SONIC_MODE( 11, MODE_PKT_GEN, pkt_gen, null_tester, port)               \
    SONIC_MODE( 13, MODE_PKT_GEN2, pkt_gen2, null_tester, port)             \
    SONIC_MODE( 14, MODE_PKT_GEN_COV, pkt_gen_covert, null_tester, port)    \
    SONIC_MODE( 15, MODE_PKT_RPT, pkt_repeater, null_tester, port_rep)      \
    SONIC_MODE( 16, MODE_PKT_CRC_RPT, pkt_crc_repeater, null_tester, port_crc_rep)\
    SONIC_MODE( 19, MODE_TX_MAX, null_tester, null_tester, null)            \
    /* RX PATH */                                                           \
    SONIC_MODE( 20, MODE_DECODE, rx_pcs_tester, null_tester, port)          \
    SONIC_MODE( 21, MODE_PKT_REC, pkt_recv, null_tester, port)              \
    SONIC_MODE( 22, MODE_COVERT_DECODE, rx_covert_pcs_tester, null_tester, port)\
    SONIC_MODE( 23, MODE_PKT_CAP, pkt_cap, null_tester, port_cap)           \
    SONIC_MODE( 24, MODE_PKT_CRC_CAP, pkt_crccap, null_tester, port_crc_cap)\
    SONIC_MODE( 29, MODE_RX_MAX, null_tester, null_tester, null)            \
    /* FORWARDING , COMBINATION*/                                           \
    SONIC_MODE( 30, MODE_PCS_FWRD, txrx_pcs, txrx_pcs, pcs_forward)         \
    SONIC_MODE( 31, MODE_PCS_APP_CAP_FWRD, txrx_pcs_cap, txrx_pcs_app, pcs_app_cap_forward)\
    SONIC_MODE( 32, MODE_PCS_APP_FWRD, txrx_pcs_app, txrx_pcs_app, pcs_app_forward)\
    SONIC_MODE( 33, MODE_MAC_FWRD, txrx_mac, txrx_mac, mac_forward)         \
    SONIC_MODE( 34, MODE_MAC_CAP_FWRD, txrx_mac_cap, txrx_mac_cap, mac_app_forward)\
    SONIC_MODE( 35, MODE_MAC_APP_FWRD, txrx_mac_app, txrx_mac_app, mac_app_forward)\
    SONIC_MODE( 36, MODE_PCS2_FWRD, txrx_pcs2, txrx_pcs2, pcs_forward)      \
    SONIC_MODE( 38, MODE_PKT_GEN_RCV, pkt_genrcv, null_tester, port)        \
    SONIC_MODE( 39, MODE_PKT_GEN_RCV2, pkt_genrcv, pkt_genrcv, port)        \
    SONIC_MODE( 40, MODE_PKT_CROSS, pkt_gen, pkt_recv, port)                \
    SONIC_MODE( 41, MODE_PKT_CHIRP, pkt_gen, pkt_gen2, port)               \
    SONIC_MODE( 42, MODE_PKT_CAP_CROSS, pkt_gen, pkt_cap, port_cap)         \
    SONIC_MODE( 43, MODE_PKT_CAP_CROSS2, pkt_gen2, pkt_cap, port_cap)       \
    SONIC_MODE( 44, MODE_PKT_GEN_CAP, pkt_gencap, pkt_gencap, port_cap)     \
    SONIC_MODE( 45, MODE_PKT_COVERT_CAP, pkt_gen_covert, pkt_cap, port_cap) \
    SONIC_MODE( 47, MODE_PKT_RPT_CAP, pkt_repeater, pkt_cap, port_rep)      \
    SONIC_MODE( 49, MODE_PORT_MAX, null_tester, null_tester, null)           

#if SONIC_SFP
#define SONIC_MODE_ARGS_SFP                                                 \
    /* DMA TESTER */                                                        \
    SONIC_MODE( 50, MODE_DMA_RX0, rx_dma, null_tester, null)                \
    SONIC_MODE( 51, MODE_DMA_RX1, null_tester, rx_dma, null)                \
    SONIC_MODE( 52, MODE_DMA_TX0, tx_dma, null_tester, null)                \
    SONIC_MODE( 53, MODE_DMA_TX1, null_tester, tx_dma, null)                \
    SONIC_MODE( 54, MODE_DMA_RXTX0, rxtx_dma, null_tester, null)            \
    SONIC_MODE( 55, MODE_DMA_RXTX1, null_tester, rxtx_dma, null)            \
    SONIC_MODE( 56, MODE_DMA_TXTX, tx_dma, tx_dma, null)                    \
    SONIC_MODE( 57, MODE_DMA_RXRX, rx_dma, rx_dma, null)                    \
    SONIC_MODE( 58, MODE_DMA_FULL, rxtx_dma, rxtx_dma, null)                \
    SONIC_MODE( 59, MODE_DMA_MAX, null_tester, null_tester, null)           \
    /* IDLE TESTER */                                                       \
    SONIC_MODE( 60, MODE_IDLE_RX0, rx_idle, null_tester, null)              \
    SONIC_MODE( 61, MODE_IDLE_RX1, null_tester, rx_idle, null)              \
    SONIC_MODE( 62, MODE_IDLE_TX0, tx_idle, null_tester, null)              \
    SONIC_MODE( 63, MODE_IDLE_TX1, null_tester, tx_idle, null)              \
    SONIC_MODE( 64, MODE_IDLE_RXTX0, rxtx_idle, null_tester, null)          \
    SONIC_MODE( 65, MODE_IDLE_RXTX1, null_tester, rxtx_idle, null)          \
    SONIC_MODE( 66, MODE_IDLE_TXTX, tx_idle, tx_idle, null)                 \
    SONIC_MODE( 67, MODE_IDLE_RXRX, rx_idle, rx_idle, null)                 \
    SONIC_MODE( 68, MODE_IDLE_FULL, rxtx_idle, rxtx_idle, null)             \
    SONIC_MODE( 69, MODE_IDLE_MAX, null_tester, null_tester, null)           
#endif

#if !SONIC_SFP
#define SONIC_MODE_ARGS SONIC_MODE_ARGS_BASIC                               \
    SONIC_MODE( 50, MODE_MAX, null_tester, null_tester, null)                
#else /* SONIC_SFP */
#define SONIC_MODE_ARGS SONIC_MODE_ARGS_BASIC SONIC_MODE_ARGS_SFP           \
    SONIC_MODE( 70, MODE_MAX, null_tester, null_tester, null)                
#endif

enum sonic_mode{
#define SONIC_MODE(x,y,z,w,v) y=x,
    SONIC_MODE_ARGS
#undef SONIC_MODE
};

#endif /*__SONIC_MODE__*/
