#ifndef __SONIC_STAT__
#define __SONIC_STAT__

struct sonic_pcs_stat {
    unsigned long long total_time;
    unsigned long long total_fifo_cnt;
    unsigned long long total_dma_cnt;
    unsigned long long total_blocks;
    unsigned long long total_packets;
    unsigned long long total_error_states;
    unsigned long long total_error_blocks;
};

struct sonic_mac_stat {
    unsigned long long total_time;
    unsigned long long total_fifo_cnt;
    unsigned long long total_packets;
    unsigned long long total_bytes;
    unsigned long long total_error_crc;
    unsigned long long total_error_len;
};

struct sonic_app_stat {
    unsigned long long total_time;
    unsigned long long total_fifo_cnt;
    unsigned long long total_packets;
    unsigned long long total_bytes;
};

struct sonic_port_stat {
    struct sonic_pcs_stat tx_pcs;
    struct sonic_pcs_stat rx_pcs;
    struct sonic_mac_stat tx_mac;
    struct sonic_mac_stat rx_mac;
    struct sonic_app_stat app;
};

struct sonic_sonic_stat {
    struct sonic_port_stat ports[2];
};

struct sonic_priv;
void sonic_merge_stats(struct sonic_priv *);
void sonic_reset_stats(struct sonic_priv *);
void sonic_print_stats(struct sonic_sonic_stat *);

#endif /* __SONIC_STAT__ */
