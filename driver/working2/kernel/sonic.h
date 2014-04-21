#ifndef __SONIC_HEADER__
#define __SONIC_HEADER__

#include <linux/ip.h>	// struct iphdr
#include <linux/udp.h>	// struct udphdr
#include <linux/tcp.h>	// struct tcphdr
#include "config.h"
#include "fifo.h"
#include "ioctl.h"
#include "mode.h"
#include "stat.h"

#if SONIC_KERNEL
#include <linux/types.h>
#include <linux/cdev.h>
#include <linux/if_ether.h>
#include "hardware.h"
#define ALLOC(x)        kmalloc(x, GFP_KERNEL)
#define ZALLOC(x)       kzalloc(x, GFP_KERNEL);
#define FREE(x)         kfree(x)
#define ALLOC_PAGE()    ((uint64_t *) __get_free_page(GFP_KERNEL))
#define ALLOC_PAGES(o)  ((uint64_t *) __get_free_pages(GFP_KERNEL, o))
#define FREE_PAGE(p)    free_page((unsigned long )p)
#define FREE_PAGES(p,o) free_pages((unsigned long)p, o)
#define CLOCK(x)        getrawmonotonic(&x)
#define SLEEP(x)                                    \
    do {                                            \
        __set_current_state(TASK_INTERRUPTIBLE);    \
        schedule_timeout(HZ * x);                   \
        __set_current_state(TASK_RUNNING);          \
    }while(0)

#define __SONIC_PRINT(level, msg, args...)                      \
    do {                                                        \
        if (level <= sonic_verbose)                             \
        printk(KERN_INFO "SONIC: [%s] " msg, __func__, ##args); \
    } while (0)

#define SONIC_ERROR(msg, args...)                               \
    do {                                                        \
        printk(KERN_ERR	"SONIC: [%s] " msg, __func__, ##args);  \
    } while (0)
#define DISABLE_INTERRUPT(flags)                                \
    local_irq_save(flags);                                      \
    local_irq_disable()
#define ENABLE_INTERRUPT(flags)                                 \
    local_irq_restore(flags);                                   \
    local_irq_enable()

#if SONIC_DMEMORY
#define SONIC_MPRINT(pointer, msg, args...)                     \
    do {\
        printk(KERN_INFO "SONIC:MEMORY: [%s] name: %s address: %p %.16llx " msg, __func__, #pointer, pointer, __pa(pointer), ##args); \
    } while (0)
#else /* !SONIC_DMEMORY */
#define SONIC_MPRINT(pointer, msg, args...)
#endif /* SONIC_DMEMORY */

#else /* !SONIC_KERNEL */
#include <stdio.h>
#include <stdint.h>
#include <pthread.h>
#include <net/ethernet.h>
#include "list.h"

#define ALLOC(x)        malloc(x)
#define ZALLOC(x)       calloc(x, 1);
#define FREE(x)         free(x)
#define ALLOC_PAGE()    malloc(4096)
#define ALLOC_PAGES(o)  malloc(order2bytes(o))
#define FREE_PAGE(p)    free(p)
#define FREE_PAGES(p,o) free(p)
#define CLOCK(x)        clock_gettime(CLOCK_MONOTONIC, &x);
#define SLEEP(x)        sleep(x);
#define likely(x)       __builtin_expect((x), 1)
#define unlikely(x)     __builtin_expect((x), 0)

#define __SONIC_PRINT(level, msg, args...)              \
    do {                                                \
        if (level <= sonic_verbose)                     \
            printf("SONIC: [%s] " msg,__func__, ##args);\
    } while (0)

#define SONIC_ERROR(msg, args...)                               \
    do {                                                        \
        fprintf(stderr, "SONIC: [%s] " msg, __func__, ##args);  \
    } while (0)

#if SONIC_DMEMORY
#define SONIC_MPRINT(pointer, msg, args...)                     \
    do {                                                        \\
    printf("SONIC:MEMORY: [%s] name: %s address: %p " msg, __func__, #pointer, pointer, ##args); \
    } while (0)
#else /* !SONIC_DMEMORY */
#define SONIC_MPRINT(pointer, msg, args...)
#endif /* SONIC_DMEMORY */
#define DISABLE_INTERRUPT(flags) flags=1
#define ENABLE_INTERRUPT(flags) flags=0
#endif /* SONIC_KERNEL */

extern int sonic_verbose;

// SONIC_SCRAMBLER
#define SONIC_OPT_SCRAMBLER     0
#define SONIC_BITWISE_SCRAMBLER 1

#define SONIC_PRINT(...) __SONIC_PRINT(0, __VA_ARGS__)

#if SONIC_DEBUG
#define SONIC_DPRINT(...) __SONIC_PRINT(1, __VA_ARGS__)
#else /* !SONIC_DEBUG */
#define SONIC_DPRINT(msg, args...)
#endif /* SONIC_DEBUG */

#if SONIC_DDEBUG
#define SONIC_DDPRINT(...) __SONIC_PRINT(2, __VA_ARGS__)
#else /* !SONIC_DDEBUG */
#define SONIC_DDPRINT(msg, args...)
#endif /* SONIC_DDEBUG */

#define SONIC_NAME          "sonic"
#define SONIC_CHR_DEV_NAME  "sonic_dev"

/* Do not modify these */
#define SONIC_PCI_VENDOR_ID 0x1172
#define SONIC_PCI_DEVICE_ID 0xe001
#define SONIC_BAR_NUM       6
#define SONIC_BAR_BASE      2

#if SONIC_TWO_PORTS
#define SONIC_PORT_SIZE     2
#else /* !SONIC_TWO_PORTS */
#define SONIC_PORT_SIZE     1
#endif /* SONIC_TWO_PORTS */

#define PREAMBLE_LEN        8
#define IP_HLEN             sizeof(struct iphdr)
#define UDP_HLEN            sizeof(struct udphdr)
#define PAYLOAD_OFFSET      (ETH_HLEN + IP_HLEN + UDP_HLEN)
#define CRC_OFFSET(p, len)  (((char * )p) + len - 4)

#define IDLE_FRAME          0x1e
#define PCS_INITIAL_STATE   0xffffffffffffffff

#define pci_dma_h(addr) ((addr >>16) >> 16)
#define pci_dma_l(addr) (addr & 0xffffffffUL)

#ifndef PAGE_SIZE
#define PAGE_SIZE   4096
#endif /* PAGE_SIZE */

#define SONIC_DIR_TX        0x1
#define SONIC_DIR_RX        0x2

#define SONIC_ALIGN(type, x, a) ((type *)(((unsigned long long )(x + a - 1)) & ~(a - 1)))
#define ISALIGNED(x, a)         (((unsigned long long)x) % a == 0)
#define NEXT_PKT(p) (struct sonic_packet_entry *) (((uint8_t *)p) + p->next)
#define SONIC_NEXT_PACKET(p) (struct sonic_packet*) (((uint8_t *)p) + p->next)
#define SONIC_COMPUTE_NEXT(p, len)  (struct sonic_packet *) \
    (SONIC_ALIGN(uint8_t, ((uint8_t *)p) + len + PREAMBLE_LEN + 8, 16) - 8)
#define FOR_SONIC_PACKETS(i, packets, packet)           \
    for (i = 0, packet = packets->packets;              \
            i < packets->pkt_cnt ;                      \
            i++, packet = SONIC_NEXT_PACKET(packet) )

#define SONIC_CPU(args, port_id, cpu)  ((args->port_socket[port_id]) * 6 + cpu)
//#define SONIC_CPU(args, path, port_id, x)	((args-> path ## _socket[port_id]) * 6 + args->x ## _cpu[port_id])
#define SONIC_SOCKET(args, path, port_id)	((args-> path ## _socket[port_id]) * 6)

#define START_CLOCK()   CLOCK(_t1); DISABLE_INTERRUPT(flags)
#define START_CLOCK_INTR()  CLOCK(_t1)
#define STOP_CLOCK(stat)                \
    ENABLE_INTERRUPT(flags);            \
    CLOCK(_t2);                         \
    (stat)->total_time = timediff2u64(&_t1, &_t2); 
#define STOP_CLOCK_INTR(stat)           \
    flags=0;                            \
    CLOCK(_t2);                         \
    (stat)->total_time = timediff2u64(&_t1, &_t2); 

#define SONIC_PPRINT(thread, msg, args...) \
    SONIC_PRINT("[p%d] " msg, thread->port_id, ##args)

#define SONIC_THREAD_COMMON_VARIABLES(x,y)                                  \
    struct sonic_##x * x = (struct sonic_##x *) y;                          \
    const volatile int * const stopper = &x->port->stopper;                 \
    struct timespec _t1, _t2;                                               \
    unsigned long flags=0

struct sonic_priv;
struct sonic_fifo;

struct sonic_pcs_block {
    uint8_t syncheader;
    uint64_t payload;
};

// sonic packet type
struct sonic_packet {
    uint32_t next;
    uint32_t reserved[3];
    short type;
    short len;
    uint32_t idle;
    uint64_t idle_bits;
    uint8_t buf[0];
};

struct sonic_packets {
    int pkt_cnt;
    int reserved;
    struct sonic_packet packets[0];
};

// data structure for logging
struct sonic_cap_entry {
    short type;
    short len;
    uint32_t idle;
    uint64_t idle_bits;

    uint8_t buf[48];
};

struct sonic_rpt_entry {
    short type;
    short len;
    uint32_t idle;
    uint32_t id;
    uint32_t reserved;
};

struct sonic_app_list{
    struct list_head head;
    int cnt;
    void * entry[0];
};

#define NUM_BLOCKS_IN_PAGE		496
struct sonic_dma_page {
    uint32_t reserved;
    uint32_t syncheaders[31];
    uint64_t payloads[496];
}; 

#define SONIC_THREADS_ARGS          \
    SONIC_THREADS(tx_pcs, pcs, TX)  \
    SONIC_THREADS(tx_mac, mac, TX)  \
    SONIC_THREADS(rx_pcs, pcs, RX)  \
    SONIC_THREADS(rx_mac, mac, RX)  \
    SONIC_THREADS(app, app, RX)

struct sonic_port;
struct sonic_runtime_thread_funcs {
    int mode;       // FIXME
#define SONIC_THREADS(x, y, z)  int (* x ) (void *); 
    SONIC_THREADS_ARGS
#undef SONIC_THREADS
    void (*set_fifo) (struct sonic_port *);
};

#define SONIC_THREAD_MEMBERS    \
    struct sonic_port * port;   \
    int port_id;                \
    int cpu;                    \
    char name[8];               \
    int (*func) (void *);       \
    struct sonic_fifo *in_fifo; \
    struct sonic_fifo *out_fifo;

struct sonic_thread {
    SONIC_THREAD_MEMBERS
};

struct sonic_app {
    SONIC_THREAD_MEMBERS

#if SONIC_KERNEL
    spinlock_t lock;
    atomic_t refcnt;
#endif /* SONIC_KERNEL */

    int app_exp;
    struct list_head in_head;       // From User to Kernel
    struct list_head out_head;      // From Kernel to User

    struct sonic_app_stat stat;
};

enum R_STATE {
    RX_INIT=0x1, RX_C=0x10, RX_D=0x100, RX_T=0x1000, RX_E=0x10000
};

// pcs is responsible for PCS layer and DMA
struct sonic_pcs {
    SONIC_THREAD_MEMBERS

    int mode;
#if SONIC_KERNEL
#if SONIC_PCIE
    struct sonic_dma_header *dma_header;
    struct sonic_dma_table *dt;
    dma_addr_t dt_bus;
    int dma_num_desc;
    int ep_offset;
    int ep_size;
#endif /* SONIC_PCIE */
    dma_addr_t dma_bus;
#endif /* SONIC_KERNEL */
    void * dma_buf;

    uint64_t * dma_cur_buf;
    int dma_cur_buf_idx;
    int dma_buf_size;
    int dma_buf_order;
    int dma_buf_pages;

    int dma_idx;

    // FIXME
    struct sonic_dma_page *dma_page;
    int dma_page_index;

    volatile int * ptr;

    int idle;

    int dic;			// DIC controlling idles for TX

    uint64_t debug;

    uint64_t state;
    int beginning_idles;
    enum R_STATE r_state;

    struct sonic_pcs_stat stat;
};

struct sonic_mac {
    SONIC_THREAD_MEMBERS

    struct sonic_mac_stat stat;
};

struct sonic_port_info {
#define SONIC_CONFIG_RUNTIME(x,y,z) int x;
    SONIC_CONFIG_PKT_GEN_INT_ARGS
#undef SONIC_CONFIG_RUNTIME
    unsigned char mac_src[ETH_ALEN];
    unsigned char mac_dst[ETH_ALEN];
    uint32_t ip_src;
    uint32_t ip_dst;
    uint16_t port_src;
    uint16_t port_dst;
    uint16_t vlan_id;

    char covert_msg[16];        // FIXME
};

struct sonic_runtime_thread_funcs;

struct sonic_port {
    struct sonic_priv *sonic;
//    struct sonic_pkt_gen_info gen;
    struct sonic_port_info info;
    struct sonic_runtime_thread_funcs *runtime_funcs;
    int stopper;
    int port_id;
    int mode;

    struct sonic_fifo *fifo[4];

#define SONIC_THREADS(x, y, z) \
    struct sonic_##y * x; \
    unsigned long int pid_##x;
    SONIC_THREADS_ARGS
#undef SONIC_THREADS

#if SONIC_KERNEL
    atomic_t threads;
    wait_queue_head_t wait;
    spinlock_t lock;

#if SONIC_PCIE
    /* control register */
    struct sonic_ctrl_register * __iomem ctrl;	// control register

    /* irq */
    struct sonic_irq_data * irq_data;	// irq_data
    dma_addr_t irq_bus;
    int msi_enabled;

    /* command response */
    struct sonic_cmd_response * cmd_rsp;
    dma_addr_t cmd_rsp_bus;

    int rx_offset;
    int rx_size;
    int rx_blk_size;
    int tx_offset;
    int tx_size;
#endif /* SONIC_PCIE */
#endif /* SONIC_KERNEL */

//    struct sonic_port_stat stat;
};

struct sonic_priv {
#if SONIC_KERNEL
#if SONIC_PCIE
    /* pci */
    struct pci_dev * pdev;
    void * __iomem bar[SONIC_BAR_NUM];
    unsigned long long irq_count;
    int msi_enabled;
#endif /*SONIC_PCIE*/

#if SONIC_FS
    /* fs */
    dev_t sonic_dev;
    struct class * sonic_class;
    struct cdev sonic_cdev;
    int cdev_minor;
    atomic_t refcnt;
#endif /*SONIC_FS*/
#if SONIC_PROC
    /* proc */
    char * procfile;
    struct proc_dir_entry *procentry;
    void * procpage;
#endif /*SONIC_PROC*/
    spinlock_t lock;
#endif /* SONIC_KERNEL */

    int status;     // 1 running

    /* port */
    struct sonic_port *ports[SONIC_PORT_SIZE];
    struct sonic_sonic_stat stat;

    /* system parameters */
    struct sonic_config_system_args system_args;
    struct sonic_config_runtime_args runtime_args;
};

extern struct sonic_priv *sonic;

#if SONIC_VLAN
struct sonic_vlan_hdr {
    unsigned char   h_dest[ETH_ALEN];
    unsigned char   h_source[ETH_ALEN];
    __be16          h_vlan_proto;
    __be16          h_vlan_TCI;
    __be16          h_vlan_encapsulated_proto;
};
#define SONIC_VLAN_ADD  4
#else
#define SONIC_VLAN_ADD  0
#endif


#if SONIC_KERNEL
#if SONIC_FS
/* fs.c */
int sonic_fs_init(struct sonic_priv *, int );
void sonic_fs_exit(struct sonic_priv *);
#endif /* SONIC_FS */
#if SONIC_PROC
/* proc.c */
int sonic_proc_init(struct sonic_priv *);
void sonic_proc_exit(struct sonic_priv *);
#endif /* SONIC_PROC */
#endif /* SONIC_KERNEL */

/* port.c */
void sonic_print_port_infos(struct sonic_priv *);
void sonic_set_port_infos(struct sonic_priv *);
void sonic_free_app_in_lists(struct sonic_app *);
void sonic_free_app_out_lists(struct sonic_app *);
void sonic_free_ports(struct sonic_priv *);
int sonic_alloc_ports(struct sonic_priv *);
int __sonic_reset(struct sonic_priv *);
int __sonic_init(struct sonic_priv **);
void __sonic_exit(struct sonic_priv *);

/* config.c */
void sonic_check_config_system_args(struct sonic_config_system_args *);
void sonic_check_config_runtime_args(struct sonic_config_runtime_args *);
#if SONIC_KERNEL
int sonic_get_config_runtime_args(struct sonic_config_runtime_args *, char *);
#else /* !SONIC_KERNEL */
int sonic_get_config_runtime_args(struct sonic_config_runtime_args *, char **);
#endif /* SONIC_KERNEL */
void sonic_print_config_runtime_args(struct sonic_config_runtime_args *);
void sonic_print_config_system_args(struct sonic_config_system_args *);
void sonic_init_config_runtime_args(struct sonic_config_runtime_args *);
void sonic_init_config_system_args(struct sonic_config_system_args *);

/* mode.c */
void sonic_set_runtime_threads(struct sonic_priv *);

/* run.c */
int sonic_run_sonic(struct sonic_priv *);
int sonic_start_sonic(struct sonic_priv *);
void sonic_stop_sonic(struct sonic_priv *);
int __sonic_run(struct sonic_priv *);
#if SONIC_KERNEL
int sonic_run(struct sonic_priv *, char *);
#endif /* SONIC_KERNEL */

#if !SONIC_KERNEL
/* tester.c */
void sonic_fill_dma_buffer_for_rx_test(struct sonic_pcs*, struct sonic_fifo*);
int sonic_test_crc_value(struct sonic_priv *);
int sonic_test_crc_performance(void *);
int sonic_test_encoder(void *);
int sonic_test_decoder(void *);
int sonic_rpt_helper(struct sonic_app *, char *, int);
#endif /* SONIC_KERNEL */

/* crc.c */
uint32_t fast_crc(uint32_t, unsigned char *, int);
uint32_t fast_crc_nobr(uint32_t, unsigned char *, int);
uint32_t crc32_bitwise(uint32_t , unsigned char *, int );
extern uint32_t (* sonic_crc) (uint32_t, unsigned char *, int);
#define SONIC_CRC(packet) sonic_crc(0xffffffff, packet->buf + PREAMBLE_LEN, packet->len - PREAMBLE_LEN - 4)

/* mac.c */
inline int sonic_update_csum_dport_id(uint8_t *, int, int, int);
int sonic_mac_pkt_generator_loop(void *);
int sonic_mac_rx_loop(void *);
int sonic_mac_tx_loop(void *);

/* pcs.c */
void sonic_encode(struct sonic_pcs *, struct sonic_packet *);
//inline int sonic_decode(struct sonic_pcs *, int, struct sonic_packet *);
int sonic_pcs_tx_loop(void *);
int sonic_pcs_rx_loop(void *);
#if SONIC_SFP
int sonic_pcs_tx_idle_loop(void *);
int sonic_pcs_rx_idle_loop(void *);
int sonic_pcs_tx_dma_loop(void *);
int sonic_pcs_rx_dma_loop(void *);
#endif /* !SONIC_SFP */

/* app.c */
int sonic_app_cap_loop(void *);
int sonic_app_rpt_loop(void *);
int sonic_app_vrpt_loop(void *);
int sonic_app_crpt_loop(void *);

/* util.c */
struct timespec;
inline uint64_t power_of_two(int);
inline uint64_t order2bytes(int);
inline uint64_t timediff2u64 (struct timespec *, struct timespec *);
int sonic_random(void);
char * print_binary_64(uint64_t);
void sonic_print_hex(uint8_t *, int, int);
void sonic_print_eth_frame(uint8_t *, int);
int sonic_str_to_mac(char *, unsigned char *);
void sonic_fill_frame(struct sonic_port_info *, uint8_t *, int);
int sonic_prepare_pkt_gen_fifo(struct sonic_fifo *, struct sonic_port_info *);
int sonic_get_cpu(void );
int sonic_set_cpu(int);
int sonic_gen_idles(struct sonic_port*, struct sonic_fifo *, int);
uint16_t udp_csum(struct udphdr *, struct iphdr *);

#endif /*__SONIC_HEADER__*/
