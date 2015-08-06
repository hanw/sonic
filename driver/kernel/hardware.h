#ifndef __SONIC_HARDWARE__
#define __SONIC_HARDWARE__

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

/* hardware.c */
#if SONIC_SFP
#define sonic_fpga_reset(port)	sonic_send_cmd(port, SONIC_CMD_RESET, 0, 0, 0)
#define sonic_fpga_start_sfp(port) sonic_send_cmd(port, SONIC_CMD_START_SFP1, 0, 0, 0)
#define sonic_fpga_stop_sfp(port) sonic_send_cmd(port, SONIC_CMD_STOP_SFP1, 0, 0, 0)
#endif

struct sonic_priv;
struct sonic_port;
struct sonic_pcs;

void sonic_fpga_exit(struct sonic_priv *);
int sonic_fpga_init(struct sonic_priv *);
void sonic_scan_bars(struct sonic_priv *);

void sonic_dma_rx(struct sonic_pcs *);
void sonic_dma_tx(struct sonic_pcs *);
uint64_t sonic_send_cmd(struct sonic_port *, int, int, int, int);
#endif /* __SONIC_HARDWARE__ */
