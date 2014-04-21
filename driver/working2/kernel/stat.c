#if !SONIC_KERNEL
#include <string.h>
#endif
#include "sonic.h"

/*
void sonic_merge_port_stat(struct sonic_port *port)
{
#define SONIC_THREADS(x,y,z)                \
    memcpy(&port->stat.x, &port->x->stat, sizeof(struct sonic_##y ##_stat));
    SONIC_THREADS_ARGS
#undef SONIC_THREADS
}
*/

void sonic_merge_stats(struct sonic_priv *sonic)
{
    int i;
    for ( i = 0 ; i < SONIC_PORT_SIZE ; i ++) {
        struct sonic_port *port = sonic->ports[i];
#define SONIC_THREADS(x,y,z)                \
        memcpy(&sonic->stat.ports[i].x, &port->x->stat, sizeof(struct sonic_##y ##_stat));
        SONIC_THREADS_ARGS
#undef SONIC_THREADS
    }
}

static void sonic_print_pcs_stat(struct sonic_pcs_stat *stat, char *head, int id)
{
#if SONIC_KERNEL
    SONIC_PRINT("[%s%d] time= %llu pkts= %llu blks= %llu err_states= %llu err_blks= %llu fifo= %llu dma= %llu\n", 
            head, id, stat->total_time, stat->total_packets, stat->total_blocks, 
            stat->total_error_states, stat->total_error_blocks, 
            stat->total_fifo_cnt, stat->total_dma_cnt);
#else /* SONIC_KERNEL */
    SONIC_PRINT("[%s%d] thru= %f pkts= %llu blks= %llu err_states= %llu err_blks= %llu fifo= %llu dma= %llu\n", 
            head, id, (double) stat->total_blocks * 66 / stat->total_time, 
            stat->total_packets, stat->total_blocks, stat->total_error_states, 
            stat->total_error_blocks, stat->total_fifo_cnt, stat->total_dma_cnt);
#endif /* SONIC_KERNEL */
}

static void sonic_print_mac_stat(struct sonic_mac_stat *stat, char *head, int id)
{
#if SONIC_KERNEL 
    SONIC_PRINT("[%s%d] time= %llu pkts= %llu err_crc= %llu err_len= %llu fifo= %llu\n", 
            head, id, stat->total_time, stat->total_packets, stat->total_error_crc,
            stat->total_error_len, stat->total_fifo_cnt);
#else /* SONIC_KERNEL */
    SONIC_PRINT("[%s%d] thru= %f pkts= %llu bytes= %llu err_crc= %llu err_len= %llu fifo= %llu\n", 
            head, id, (double) stat->total_bytes * 8 / stat->total_time, 
            stat->total_packets, stat->total_bytes, stat->total_error_crc, 
            stat->total_error_len, stat->total_fifo_cnt);
#endif /* SONIC_KERNEL */
}

static void sonic_print_app_stat(struct sonic_app_stat *stat, char *head, int id)
{
#if SONIC_KERNEL
    SONIC_PRINT("[%s%d] time= %llu pkts= %llu bytes= %llu fifo= %llu\n", 
            head, id, stat->total_time, stat->total_packets, 
            stat->total_bytes, stat->total_fifo_cnt);
#else /* SONIC_KERNEL */
    SONIC_PRINT("[%s%d] thru=%f time= %llu pkts= %llu bytes= %llu fifo= %llu\n", 
            head, id, (double) stat->total_bytes * 8 / stat->total_time, 
            stat->total_time, stat->total_packets, stat->total_bytes, 
            stat->total_fifo_cnt);
#endif /* SONIC_KERNEL */
}

void sonic_print_port_stat(struct sonic_port_stat *stat, int port_id)
{
    SONIC_PRINT("---Port [%d] stats---\n", port_id);
#define SONIC_THREADS(x,y,z) \
    sonic_print_##y ##_stat(&stat->x, #x, port_id);
    SONIC_THREADS_ARGS
#undef SONIC_THREADS
}

void sonic_print_stats(struct sonic_sonic_stat *stat)
{
    int i;
    for ( i = 0 ; i < SONIC_PORT_SIZE ; i ++) 
        sonic_print_port_stat(&stat->ports[i], i);
}

void sonic_reset_stats(struct sonic_priv *sonic)
{
//    int i;
    memset(&sonic->stat, 0, sizeof(struct sonic_sonic_stat));
}
