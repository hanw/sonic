#ifndef __SONIC_HEADER__
#define __SONIC_HEADER__

#include <linux/ip.h>	// struct iphdr
#include <linux/udp.h>	// struct udphdr
#include <linux/tcp.h>	// struct tcphdr
//#include "fifo.h"

#if SONIC_TESTER
#include "tester.h"
#endif

#if SONIC_KERNEL
#include <linux/types.h>
#include <linux/cdev.h>
#include <linux/if_ether.h>
#define ALLOC(x)	kmalloc(x, GFP_KERNEL)
#define FREE(x)		kfree(x)
#define ALLOC_PAGE()	((uint64_t *) __get_free_page(GFP_KERNEL))
#define ALLOC_PAGES(o)	((uint64_t *) __get_free_pages(GFP_KERNEL, o))
#define ALLOC_PAGES2(o)	((uint64_t *) __get_free_pages(GFP_KERNEL, o))
#define FREE_PAGE(p)	free_page((unsigned long )p)
#define FREE_PAGES(p,o) free_pages((unsigned long)p, o)
#define CLOCK(x)	getrawmonotonic(&x)
#define SLEEP(x)	do { \
	__set_current_state(TASK_INTERRUPTIBLE); \
	schedule_timeout(HZ * x);	\
	__set_current_state(TASK_RUNNING); }while(0)

#define __SONIC_PRINT(level, msg, args...) do {\
    if (level <= sonic_verbose) \
        printk(KERN_INFO "SONIC: [%s] " msg, __func__, ##args);	\
	} while (0)

#define SONIC_ERROR(msg, args...) do {\
	printk(KERN_ERR	"SONIC: [%s] " msg, __func__, ##args);	\
	} while (0)

#if SONIC_DMEMORY
#define	SONIC_MPRINT(pointer, msg, args...) do {\
	printk(KERN_INFO "SONIC:MEMORY: [%s] name: %s address: %p %.16llx " msg, __func__, #pointer, pointer, __pa(pointer), ##args); \
	} while (0)
#else
#define SONIC_MPRINT(pointer, msg, args...)
#endif

#else /* SONIC_KERNEL */
#include <stdio.h>
#include <stdint.h>
#include <pthread.h>
#include <net/ethernet.h>
#include "list.h"

#define ALLOC(x)	malloc(x)
#define FREE(x)		free(x)
#define ALLOC_PAGE()	malloc(4096)
#define ALLOC_PAGES(o) 	malloc(order2bytes(o))
#define ALLOC_PAGES2(o)	malloc(order2bytes(o))
#define FREE_PAGE(p)	free(p)
#define FREE_PAGES(p,o) free(p)
#define CLOCK(x)	clock_gettime(CLOCK_MONOTONIC, &x);
#define SLEEP(x)	sleep(x);

#define likely(x)	__builtin_expect((x), 1)
#define unlikely(x)	__builtin_expect((x), 0)

#define __SONIC_PRINT(level, msg, args...) do {\
    if (level <= sonic_verbose) \
        printf("SONIC: [%s] " msg, __func__, ##args);\
    } while (0)

#define SONIC_ERROR(msg, args...) do {\
	fprintf(stderr, "SONIC: [%s] " msg, __func__, ##args);	\
	} while (0)

#if SONIC_DMEMORY
#define	SONIC_MPRINT(pointer, msg, args...) do {\
	printf("SONIC:MEMORY: [%s] name: %s address: %p " msg, __func__, #pointer, pointer, ##args); \
	} while (0)
#else
#define SONIC_MPRINT(pointer, msg, args...)
#endif

#endif /* SONIC_KERNEL */

extern int sonic_verbose;

#define SONIC_OPT_SCRAMBLER	0
#define SONIC_BITWISE_SCRAMBLER	1

// SONIC_CRC
#define SONIC_FASTCRC		0
#define SONIC_LOOKUPCRC		1
#define SONIC_BITCRC		2
#define SONIC_FASTCRC_O		3

#define SONIC_PRINT(...) __SONIC_PRINT(0, __VA_ARGS__)

#if SONIC_DEBUG
#define SONIC_DPRINT(...) __SONIC_PRINT(1, __VA_ARGS__)
#else /*SONIC_DEBUG*/
#define SONIC_DPRINT(msg, args...)
#endif /*SONIC_DEBUG*/

#if SONIC_DDEBUG
#define SONIC_DDPRINT(...) __SONIC_PRINT(2, __VA_ARGS__)
#else /*SONIC_DDEBUG*/
#define SONIC_DDPRINT(msg, args...)
#endif /*SONIC_DDEBUG*/

#define SONIC_MDPRINT(x)

#define SONIC_NAME		"sonic"
#define SONIC_CHR_DEV_NAME	"sonic_dev"

#define SONIC_PCI_VENDOR_ID	0x1172
#define SONIC_PCI_DEVICE_ID	0xe001

#if SONIC_TWO_PORTS		
#define SONIC_PORT_SIZE		2
#else
#define SONIC_PORT_SIZE		1
#endif

#define SONIC_BAR_NUM		6
#define SONIC_BAR_BASE		2

#define PREAMBLE_LEN		8
#define IP_HLEN			sizeof(struct iphdr)
#define UDP_HLEN		sizeof(struct udphdr)
#define PAYLOAD_OFFSET		(ETH_HLEN + IP_HLEN + UDP_HLEN)
#define CRC_OFFSET(p, len)	(p + len - 4)

#define IDLE_FRAME	0x1e
#define PCS_INITIAL_STATE	0xffffffffffffffff

#define pci_dma_h(addr) ((addr >>16) >> 16)
#define pci_dma_l(addr) (addr & 0xffffffffUL)

#ifndef PAGE_SIZE	
#define PAGE_SIZE	4096
#endif

#define SONIC_DIR_TX		0x1
#define SONIC_DIR_RX		0x2

#define SONIC_ALIGN(type, x, a)	((type *)(((unsigned long long )(x + a - 1)) & ~(a - 1)))
#define ISALIGNED(x, a)	(((unsigned long long)x) % a == 0)
#define NEXT_PKT(p) (struct sonic_packet_entry *) (((uint8_t *)p) + p->next)

#if SONIC_KERNEL

/* Commands */
#define SONIC_CMD_NONE 	 		0
#define SONIC_CMD_RESET 	 	0x01
#define SONIC_CMD_SET_ADDR_IRQ 		0x02	 // TODO
#define SONIC_CMD_CONFIG_IRQ		0x03
#define SONIC_CMD_GET_RX_OFFSET 	0x10
#define SONIC_CMD_GET_RX_OFFSET2 	0x11
#define SONIC_CMD_GET_RX_SIZE 	 	0x12
#define SONIC_CMD_GET_RX_BLOCK_SIZE 	0x13
#define SONIC_CMD_SET_RX_BLOCK_SIZE 	0x14
#define SONIC_CMD_GET_TX_OFFSET 	0x15
#define SONIC_CMD_GET_TX_OFFSET2	0x16
#define SONIC_CMD_GET_TX_SIZE 		0x17
#define SONIC_CMD_STOP_SFP1 	 	0x20
#define SONIC_CMD_START_SFP1 	 	0x21
#define SONIC_CMD_STOP_SFP2 	 	0x22
#define SONIC_CMD_START_SFP2 	 	0x23
#define SONIC_CMD_MODE 	 		0x24

#define SONIC_CMD_MAX			0x25

/* Error Code */
#define SONIC_CMD_OK 			0
#define SONIC_CMD_ERROR 		1

struct pci_dev;
struct timespec;

struct sonic_dma_header {
	uint32_t w0;
	uint32_t bdt_addr_h;
	uint32_t bdt_addr_l;
	uint32_t w3;
} __attribute__ ((packed));

struct sonic_dma_status {
	uint32_t write_h;
	uint32_t write_l;
	uint32_t read_h;
	uint32_t read_l;
	uint32_t error;
} __attribute__ ((packed));

/* Descriptor table */
struct sonic_dma_desc {
	uint32_t w0;
	uint32_t ep_addr;
	uint32_t rc_addr_h;
	uint32_t rc_addr_l;
} __attribute__ ((packed));

struct sonic_dma_table {
	uint32_t reserved[3];
	uint32_t w3;
	struct sonic_dma_desc desc[255];
} __attribute__ ((packed));

struct sonic_irq_header {
	uint32_t w0;
	uint32_t irq_data_addr_h;
	uint32_t irq_data_addr_l;
} __attribute__ ((packed));

struct sonic_cmd {
	uint32_t cmd;
	uint32_t data0;
	uint32_t data1;
	uint32_t data2;
	uint32_t rt_addr_h;
	uint32_t rt_addr_l;
} __attribute__ ((packed));

struct sonic_cmd_response {
	uint64_t error;
	uint64_t data;
};

/* sonic control registers */
struct sonic_ctrl_register {
	struct sonic_dma_header		rx_header;
	struct sonic_dma_header		tx_header;
	struct sonic_dma_status		status;
	uint32_t reserved[3];
	struct sonic_irq_header		irq_header;
	uint32_t w19;
	struct sonic_cmd		cmd;
} __attribute__ ((packed));

/* IRQ data */
struct sonic_irq_data {
	uint8_t status;
	uint32_t ready_bytes:24;
	uint32_t reserved;
	uint32_t rx_ptr;
	uint32_t tx_ptr;
	uint32_t reserved2[12];
	uint32_t tx_ptr2;
} __attribute__ ((packed));
#endif /* SONIC_KERNEL */

struct sonic_priv;
struct sonic_fifo;

struct sonic_pcs_block {
	uint8_t syncheader;
	uint64_t payload;
};

struct sonic_udp_port_info {
	char mac_src[ETH_ALEN];
	char mac_dst[ETH_ALEN];
	uint32_t ip_src;
	uint32_t ip_dst;
	uint16_t port_src;
	uint16_t port_dst;
};

struct sonic_pkt_generator {
	int buf_batching;

	int mode;
	long long int pkt_num;
	int pkt_len;
	int idle;
	int idle_chain;
	int wait;
	int chain_num;
	int multi_queue;
	int port_base;		// FIXME

    int bw_gap;
    int bw_num;

	// timing channel
	uint8_t *msg;
	int msg_len;
	int delta;
    struct sonic_fifo *fifo;

	struct sonic_udp_port_info info;
};

// sonic packet type
struct sonic_packet_entry {
//	struct sonic_packet_entry *next;
	uint32_t next;
//	struct sonic_packet_entry *next;
	uint32_t reserved[3];
	short type;
	short len;
	uint32_t idle;
	uint32_t idle_bits;
	short error;
	short packet_bits;	
//	short len;
//	short idle;
//	int bits;
	uint8_t buf[0];
};

// data structure for logging
struct sonic_log_entry {
	uint8_t error;
	uint8_t type;
	short len;
	uint32_t idle;
	uint32_t idle_bits;
	uint32_t packet_bits;

	uint8_t buf[48];
};

struct sonic_mac_fifo_entry {
	int pkt_cnt;
	int reserved;
	struct sonic_packet_entry packets[0];
};

struct sonic_log {
	struct list_head head;
	int log_cnt;
	struct sonic_log_entry logs[0];
};

struct sonic_ins_entry {
    short type;
    short len;
    uint32_t idle;
    uint32_t id;
};

struct sonic_ins {
    struct list_head head;
    int ins_cnt;
    struct sonic_ins_entry inss[0];
};

#define NUM_BLOCKS_IN_PAGE		496
struct sonic_dma_page {
	uint32_t reserved;
	uint32_t syncheaders[31];
	uint64_t payloads[496];
}; 

#define SONIC_THREADS_ARGS	\
	SONIC_THREADS(tx_pcs, pcs, TX)	\
	SONIC_THREADS(tx_mac, mac, TX)	\
	SONIC_THREADS(rx_pcs, pcs, RX)	\
	SONIC_THREADS(rx_mac, mac, RX)  \
	SONIC_THREADS(log, logger, RX)	

struct sonic_port_args {
#define SONIC_THREADS(x, y, z)  int (* x ) (void *); 
	SONIC_THREADS_ARGS
#undef SONIC_THREADS
};

struct sonic_thread {
	struct sonic_port * port;
	int cpu;
	int (*func) (void *);
};

struct sonic_logger {
	struct sonic_port * port;
	int cpu;
	int (*func) (void *);

	int port_id;
	struct sonic_fifo * fifo;

	struct sonic_fifo * in_fifo;
	struct sonic_fifo * out_fifo;

#if SONIC_KERNEL
	spinlock_t lock;
	atomic_t refcnt;
#endif /* SONIC_KERNEL */

	struct list_head log_head;
	int log_porder;
    struct list_head ins_head;

};

struct sonic_pcs_stats {
	uint64_t total_pkts;
	uint64_t total_blocks;
	uint64_t total_idles;
	uint64_t total_errors;	// RX_E in rx path
};

// pcs is responsible for PCS layer and DMA
struct sonic_pcs {
	struct sonic_port * port;
	int cpu;
	int (*func) (void *);

	int mode;
	int port_id;
#if SONIC_PCIE
	struct sonic_dma_header *dma_header;

	struct sonic_dma_table *dt;
	dma_addr_t dt_bus;

	int dma_num_desc;

	int ep_offset;
	int ep_size;
#endif

	void * dma_buf;
#if SONIC_KERNEL
	dma_addr_t dma_bus;
#endif

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

	struct sonic_fifo * fifo;
	struct sonic_fifo * pcs_fifo;	// pcs forwarding

	int dic;			// DIC controlling idles for TX

	uint64_t debug;

	struct sonic_pcs_stats stat;
};

struct sonic_mac_stats {
	uint64_t total_pkts;
	uint64_t total_bytes;
	uint64_t total_crcerrors;
	uint64_t total_lenerrors;
};

struct sonic_mac {
	struct sonic_port * port;
	int cpu;
	int (*func) (void *);

	int port_id;
	struct sonic_fifo * fifo;

	struct sonic_fifo * in_fifo;
	struct sonic_fifo * out_fifo;

	void * data;

	struct sonic_mac_stats stat;
};

struct sonic_port {
	struct sonic_priv *sonic;
	struct sonic_udp_port_info info;
	int stopper;
	int port_id;
    struct sonic_pkt_generator *gen;

#define SONIC_THREADS(x, y, z) \
	struct sonic_##y * x; \
	unsigned long int pid_##x;
	SONIC_THREADS_ARGS
#undef SONIC_THREADS

#if SONIC_KERNEL
	atomic_t threads;
	wait_queue_head_t wait;
	spinlock_t lock;
#endif /* SONIC_KERNEL */

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
};

struct sonic_priv {
#if SONIC_KERNEL
#if SONIC_PCIE
	/* pci */
	struct pci_dev * pdev;
//	void * __iomem bar[SONIC_BAR_NUM];

	unsigned long long irq_count;
	int msi_enabled;
#endif /*SONIC_PCIE*/
#if SONIC_FS
	/* fs */
	dev_t sonic_dev;
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

	int status;		// 1 running
	int mode;		// OP mode
	struct sonic_port_args *port_args[SONIC_PORT_SIZE];

	/* port */
	struct sonic_port *ports[SONIC_PORT_SIZE];

#if SONIC_TESTER
	struct tester_args tester_args;
#endif /*SONIC_TESTER*/
};

extern struct sonic_priv *sonic;

#if SONIC_KERNEL
/* hardware.c */
#if SONIC_SFP
#define sonic_fpga_reset(port)	sonic_send_cmd(port, SONIC_CMD_RESET, 0, 0, 0)
#define sonic_fpga_start_sfp(port) sonic_send_cmd(port, SONIC_CMD_START_SFP1, 0, 0, 0)
#define sonic_fpga_stop_sfp(port) sonic_send_cmd(port, SONIC_CMD_STOP_SFP1, 0, 0, 0)
#endif
void sonic_fpga_exit(struct sonic_priv *);
int sonic_fpga_init(struct sonic_priv *);
void sonic_fpga_port_exit(struct sonic_port *);
int sonic_fpga_port_init(struct sonic_port *);
void __devinit sonic_scan_bars(struct sonic_priv *);
void sonic_scan_bar(struct sonic_priv *);
int sonic_map_bar(struct sonic_port *);
void sonic_unmap_bar(struct sonic_port *);

void sonic_dma_rx(struct sonic_pcs *);
void sonic_dma_tx(struct sonic_pcs *);
uint64_t sonic_send_cmd(struct sonic_port *, int, int, int, int);

/* fs.c */
int sonic_fs_init(struct sonic_priv *, int );
void sonic_fs_exit(struct sonic_priv *);

/* proc.c */
int sonic_proc_init(struct sonic_priv *);
void sonic_proc_exit(struct sonic_priv *);
#else 
struct sonic_priv;
#endif /* SONIC_KERNEL */

/* port.c */
void sonic_free_ports(struct sonic_priv *);
int sonic_alloc_ports(struct sonic_priv *);

//void sonic_init_tx_path(struct sonic_port *);
//void sonic_init_rx_path(struct sonic_port *);
void sonic_init_ports(struct sonic_priv *, int);
int sonic_init_sonic(struct sonic_priv *, int);
void sonic_release_sonic(struct sonic_priv *);

/* crc.c */
inline uint32_t fast_crc(unsigned char *, int);
inline uint32_t fast_crc_nobr(unsigned char *, int);
uint32_t crc32_bitwise(uint32_t crc, unsigned char *bytes, int len);

/* tx_pcs.c */
inline void sonic_gearbox(struct sonic_pcs *, uint64_t, uint64_t);
//uint64_t sonic_encode(struct sonic_pcs *, uint64_t *, uint8_t *, int, int, unsigned);
int sonic_tx_pcs_loop(void *);
int sonic_tx_pcs_idle_loop(void *);
int sonic_tx_pcs_tester(void *);
int sonic_tx_pcs_scrambler_loop(void *);
#if SONIC_SFP
int sonic_tx_dma_tester(void *);
#endif

/* rx_pcs.c */
enum R_STATE {
	RX_INIT=0x1, RX_C=0x10, RX_D=0x100, RX_T=0x1000, RX_E=0x10000
};

int sonic_rx_pcs_loop(void *);
int sonic_decode_no_fifo(uint64_t *, void *, int , enum R_STATE *, struct sonic_packet_entry *, int);
int sonic_rx_pcs_tester(void *args);
int sonic_rx_pcs_idle_loop(void *);
int sonic_rx_pcs_descrambler_loop(void *);
#if SONIC_SFP
int sonic_rx_dma_tester(void *);
#endif

struct tester_args;

/* mac.c */
void sonic_prepare_pkt_gen (struct sonic_pkt_generator *, struct sonic_fifo *, int);
//void sonic_free_pkt_gen (struct sonic_mac *);
void sonic_free_pkt_gen (struct sonic_port *);
//int sonic_alloc_pkt_gen (struct sonic_mac *);
int sonic_alloc_pkt_gen (struct sonic_port *);
int sonic_pkt_gen_set_len(struct sonic_mac *, int);
int sonic_pkt_gen_set_mode(struct sonic_mac *, int);
int sonic_pkt_gen_set_pkt_num(struct sonic_mac *, int);
int sonic_pkt_gen_set(struct sonic_pkt_generator *, int, int, struct tester_args *);
int sonic_pkt_generator_loop(void *);
int sonic_pkt_generator_loop2(void *);
int sonic_pkt_generator_covert_loop(void *);
int sonic_pkt_receiver_loop(void *);
int sonic_pkt_receiver_loop2(void *);
int sonic_tx_mac_tester(void *);
int sonic_tx_mac_loop(void *);

/* util.c */
inline uint64_t power_of_two(int);
inline uint64_t order2bytes(int);
inline uint64_t timediff2u64 (struct timespec *, struct timespec *);
char * print_binary_64(uint64_t);
void sonic_print_hex(uint8_t *, int, int);
void sonic_print_eth_frame(uint8_t *, int);
int sonic_str_to_mac(char *, char *);
void sonic_fill_frame(struct sonic_udp_port_info *, uint8_t *, int);
void sonic_fill_packet(uint8_t *, int );
int sonic_fill_buf_batching (struct sonic_udp_port_info *, uint8_t *, int, int, int, int );
inline void sonic_set_pkt_id(uint8_t *, int);
int sonic_update_buf_batching (uint8_t *, int, int, int, int);
int sonic_update_buf_batching2 (uint8_t *, int, int, struct sonic_pkt_generator *,  int *);
int sonic_update_buf_batching_covert (uint8_t *, int, int, struct sonic_pkt_generator *);
void sonic_print_port_info(struct sonic_udp_port_info *);
int sonic_set_port_info(struct sonic_udp_port_info *, char *, char *, char *, 
		char * , int , int );
void print_page(struct sonic_dma_page *);
int sonic_update_csum_id(uint8_t *, int, int, int);

/* thread.c */
int sonic_get_cpu(void );
int sonic_set_cpu(int);
int sonic_start_sonic(struct sonic_priv *);
void sonic_stop_sonic(struct sonic_priv *);
int sonic_run_sonic(struct sonic_priv *, int);
int sonic_reset_sonic(struct sonic_priv *);

/* stat.c */
void sonic_reset_port_stat(struct sonic_port *);
void sonic_reset_stat(struct sonic_priv *);
int sonic_print_port_stat(struct sonic_port *, char *);
void sonic_print_stat(struct sonic_priv *, char *);
void sonic_get_stat(struct sonic_priv *, uint64_t *);

/* log.c */
int sonic_log_loop(void *);
int sonic_log_tester(void *);
void sonic_free_logs(struct sonic_logger *);
void sonic_free_ins(struct sonic_logger *);
int sonic_ins_loop(void *);

/* availbw.c */
int sonic_availbw_loop(void *);

#endif /*__SONIC_HEADER__*/
