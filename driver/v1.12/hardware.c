#include <linux/pci_regs.h>
#include <linux/pci.h>
#include "sonic.h"

void sonic_print_irq(struct sonic_irq_data *irq)
{
//	SONIC_PRINT("irq = %p, status = %x, %llx\n", irq, irq->status, *(uint64_t*)irq);
	SONIC_PRINT("rx_ptr = %x, tx_ptr = %x\n", irq->rx_ptr, irq->tx_ptr);
}

/* dma */
static inline void sonic_dma_desc_set(struct sonic_dma_desc *desc, dma_addr_t addr,
		u32 ep_addr, int dwords)
{
	SONIC_DDPRINT("%p %x %x %x\n", desc, addr, ep_addr, dwords);
	desc->w0 = cpu_to_le32(dwords) ;
	desc->ep_addr = cpu_to_le32(ep_addr);
	desc->rc_addr_h = cpu_to_le32(pci_dma_h(addr));
	desc->rc_addr_l = cpu_to_le32(pci_dma_l(addr));
}

static inline void sonic_dma_setup(struct sonic_dma_header *header, int num_desc,
		dma_addr_t bus, int msi)
{
	iowrite32(num_desc, &header->w0);
//	wmb();
	iowrite32(pci_dma_h(bus), &header->bdt_addr_h);
//	wmb();
	iowrite32(pci_dma_l(bus), &header->bdt_addr_l);
	wmb();
}

static inline void sonic_dma_start(struct sonic_dma_header *header, int n)
{
	SONIC_DDPRINT("header = %p, n = %d\n", header, n);
	iowrite32(n, &header->w3);
	wmb();
}

static inline void sonic_dma_reset(struct sonic_dma_header *header)
{
	iowrite32(0x0000ffffUL, &header->w0);
	wmb();
}

void sonic_dma_tx(struct sonic_pcs *pcs) 
{
	int i=0, num = pcs->dma_num_desc, retry_cnt = 0;
	int port_id = pcs->port_id;
//	struct sonic_irq_data *irq = dma->irq;
	register uint32_t offset = pcs->ep_offset;	// in qwords
	register uint32_t bytes = pcs->dma_buf_size;
	register uint32_t nbytes = bytes / num;		// for each descriptor
	register uint32_t noffset = pcs->dma_cur_buf_idx * pcs->dma_buf_size;  // in bytes
	register uint32_t next_addr = 0;
	volatile const uint32_t * const ptr = pcs->ptr;

	SONIC_DDPRINT("header = %p, offset = %x, nbytes = %d, bytes = %d num = %d,  noffset = %d\n", 
		pcs->dma_header, offset, nbytes, bytes, num, noffset);

	/* wait for previous dma to finish */
	SONIC_DDPRINT("[q%d]\n", port_id);
	
	for ( i =0 ; i <500 ; i ++) {
		volatile u32 *p = &pcs->dt->w3;
		u32 eplast = le32_to_cpu(*p) & 0xffffUL;
                SONIC_DDPRINT( "[p%d] EPLAST = %u\n", port_id, eplast);
		if (eplast == num-1)
			break;
		udelay(1);
	}
	if ( unlikely(i == 500)) {
		SONIC_ERROR("[p%d] dma failed\n", port_id);
//		return;
	}

	sonic_dma_reset(pcs->dma_header);

#if SONIC_DMA_POINTER
retry:
	next_addr = le32_to_cpu(*ptr) << 3;
	next_addr &= 0xfffff000;
	retry_cnt++;

	if (next_addr > offset && next_addr < offset + bytes) {
		SONIC_DDPRINT("[p%d] not enough! cur ptr = %x cur addr = %x expecting %x offset = %x\n", port_id, next_addr >> 3, 
				next_addr, offset + bytes, offset);
		udelay(5);
		if (retry_cnt == 100) {
			SONIC_ERROR("[p%d] ??\n", port_id);
			goto out;
			return;
		}
		goto retry;

	} 
out:

	SONIC_DDPRINT("[p%d] offset=%x, next_addr=%x \n", port_id, offset, next_addr);
	SONIC_DDPRINT("[p%d] bytes = %x\n", port_id, bytes);
#endif

	for ( i = 0 ; i < num ; i ++) {
		sonic_dma_desc_set(&pcs->dt->desc[i] , 
			pcs->dma_bus + noffset + i*nbytes, offset + i *nbytes, nbytes >> 2);
	}
	SONIC_DDPRINT("[p%d] %.16llx\n", port_id, pcs->dma_bus);

	pcs->dt->desc[num-1].w0 |= (1<<17);	// EPLAST

	pcs->dt->w3 = cpu_to_le32(0xCAFEFADE);

	sonic_dma_setup(pcs->dma_header, num, pcs->dt_bus, 0); // FIXME
	sonic_dma_start(pcs->dma_header, num-1);

	SONIC_DDPRINT("[p%d]\n", port_id);

	/* next dma buffer */
	pcs->dma_cur_buf_idx = (pcs->dma_cur_buf_idx + 1) % 2;
	pcs->dma_cur_buf = pcs->dma_buf + pcs->dma_cur_buf_idx * pcs->dma_buf_size;

	/* next offset */
	offset += bytes;
	pcs->ep_offset = offset % pcs->ep_size;
}

void sonic_dma_rx(struct sonic_pcs *pcs) 
{
	int i=0, num = pcs->dma_num_desc, retry_cnt =0;
	int port_id = pcs->port_id;
	register uint32_t offset = pcs->ep_offset;	// in qwords
	register uint32_t bytes  = pcs->dma_buf_size;
	register uint32_t nbytes = bytes / num;
	register uint32_t next_addr = 0;
	register uint32_t noffset = pcs->dma_cur_buf_idx * pcs->dma_buf_size;  // in bytes
	volatile const uint32_t * const ptr = pcs->ptr;

	SONIC_DDPRINT("\n");

	/* wait for previous dma to finish */
	for ( i =0 ; i <500 ; i ++) {
		volatile u32 *p = &pcs->dt->w3;
		u32 eplast = le32_to_cpu(*p) & 0xffffUL;
                SONIC_DDPRINT( "[p%d] EPLAST = %u\n", port_id, eplast);
		if (eplast == num-1)
			break;
		udelay(1);
	}
	if ( unlikely(i == 500)) {
		SONIC_ERROR("[p%d] dma failed\n", port_id);
		return;
	}

	sonic_dma_reset(pcs->dma_header);

#if SONIC_DMA_POINTER
retry:
	next_addr = le32_to_cpu(*ptr) << 3;
	next_addr &= 0xfffff000;
	retry_cnt ++;

	if (next_addr >= offset && next_addr < offset + bytes) {
		SONIC_DDPRINT("[p%d] not enough! cur ptr = %x cur addr = %x expecting %x \n", port_id, next_addr >> 3, 
				next_addr, offset + bytes);
		udelay(5);
		if (unlikely(retry_cnt == 100)) {
//			SONIC_ERROR("??\n");
			goto out;
			return;
		}
		goto retry;
	} 
	SONIC_DDPRINT("[p%d] offset=%x, next_addr=%x \n", port_id, offset, next_addr);
out:
#endif

	for ( i = 0 ; i < num ; i ++ ) {
		sonic_dma_desc_set(&pcs->dt->desc[i] , 
			pcs->dma_bus + noffset + i*nbytes, offset + i * nbytes, nbytes >> 2);
	}
	SONIC_DDPRINT("[p%d] %.16llx\n", pcs->dma_bus, port_id);

	pcs->dt->desc[num-1].w0 |= (1<<17);	// EPLAST

	pcs->dt->w3 = cpu_to_le32(0xCAFEFADE);

	sonic_dma_setup(pcs->dma_header, num, pcs->dt_bus, 0);
	sonic_dma_start(pcs->dma_header, num-1);

	SONIC_DDPRINT("\n");

	pcs->dma_cur_buf_idx = (pcs->dma_cur_buf_idx + 1) % 2;
	pcs->dma_cur_buf = pcs->dma_buf + pcs->dma_cur_buf_idx * pcs->dma_buf_size;

	offset += bytes;
	pcs->ep_offset = offset % pcs->ep_size;
}

uint64_t sonic_send_cmd(struct sonic_port *port, int cmd, int p0, int p1, int p2)
{
	int i=0; 
	uint64_t ret = -1;
	uint32_t error;
	struct sonic_cmd *cmd_reg = &port->ctrl->cmd;

	if(cmd > SONIC_CMD_MAX)
		return -1;

	SONIC_DDPRINT("Sending command %d p0 = %x p1 = %x p2 = %x\n", cmd, p0, p1, p2);

	iowrite32(p0, &cmd_reg->data0);
//	wmb();
	iowrite32(p1, &cmd_reg->data1);
//	wmb();
	iowrite32(p2, &cmd_reg->data2);
//	wmb();
//	iowrite32(pci_dma_h(port->cmd_rsp_bus), &cmd_reg->rt_addr_h);
//	wmb();
//	iowrite32(pci_dma_l(port->cmd_rsp_bus), &cmd_reg->rt_addr_l);
	wmb();

	port->cmd_rsp->error = 0xffffffffffffffff;

	/* send command */
	iowrite32(cmd, &cmd_reg->cmd);
	wmb();

	while(1) {
		volatile uint64_t *p = &port->cmd_rsp->error;
		i++;
		error = le32_to_cpu(*p);
		if ( error == SONIC_CMD_OK) {
			SONIC_DDPRINT("Command 0x%.2x Okay, data = %lld\n", cmd, 
					(unsigned long long )port->cmd_rsp->data);
			break;
		}
		else if (error == SONIC_CMD_ERROR) {
			SONIC_ERROR("Command 0x%.2x failed\n", cmd);
			break;
		}
		udelay(100);
		if ( i >= 10000) {
			SONIC_ERROR("Waited too long... 0x%.2x %d\n", cmd, error);
			break;
		}
	}

	if(!error)
		ret = port->cmd_rsp->data;

	return ret;
}

void sonic_fpga_port_exit(struct sonic_port *port)
{
//	struct pci_dev *pdev = port->sonic->pdev;
	SONIC_PRINT("\n");

	/* TODO: reset?? */
	if (port->irq_data)
//		pci_free_consistent(pdev, sizeof(struct sonic_irq_data),
//				port->irq_data, port->irq_bus);
		FREE(port->irq_data);

	port->irq_data = NULL;

	if (port->cmd_rsp)
//		pci_free_consistent(pdev, sizeof(struct sonic_cmd_response),
//				port->cmd_rsp, port->cmd_rsp_bus);
		FREE(port->cmd_rsp);

	port->cmd_rsp = NULL;

	sonic_unmap_bar(port);
}

void sonic_fpga_exit(struct sonic_priv *sonic)
{
	int i;
	for ( i = 0 ; i < SONIC_PORT_SIZE ; i ++ ) 
		sonic_fpga_port_exit(sonic->ports[i]);
}

/* negotiate with board to get rx, tx, offset */
int sonic_fpga_port_init(struct sonic_port *port)
{
	int ret;
//	struct sonic_priv *sonic = port->sonic;
//	struct pci_dev *pdev = sonic->pdev;
	struct sonic_cmd *cmd_reg;

	if(sonic_map_bar(port))
		return -1;		

	cmd_reg = &port->ctrl->cmd;

	/* alloc irq_data */
//	port->irq_data = (struct sonic_irq_data *) pci_alloc_consistent(
//			pdev, sizeof(struct sonic_irq_data), &port->irq_bus);
	port->irq_data = ALLOC(sizeof(struct sonic_irq_data));
	port->irq_bus = virt_to_bus(port->irq_data);
	if (port->irq_data == NULL) {
		goto abort;
	}
//	SONIC_MPRINT(port->irq_data);
	SONIC_DPRINT("sonic->irq_data = %p\n", port->irq_data);

	/* alloc response data*/
//	port->cmd_rsp = (struct sonic_cmd_response *) pci_alloc_consistent(
//			pdev, sizeof(struct sonic_cmd_response), &port->cmd_rsp_bus);
	port->cmd_rsp = ALLOC(sizeof(struct sonic_cmd_response));
	port->cmd_rsp_bus = virt_to_bus(port->cmd_rsp);
	if (port->cmd_rsp == NULL) {
		goto abort_alloc;
	}
//	SONIC_MPRINT(port->cmd_rsp);

	iowrite32(pci_dma_h(port->cmd_rsp_bus), &cmd_reg->rt_addr_h);
	iowrite32(pci_dma_l(port->cmd_rsp_bus), &cmd_reg->rt_addr_l);
	wmb();

	/* reset the fpga */
	SONIC_DDPRINT("Issue SONIC_CMD_RESET\n");
	if (sonic_fpga_reset(port))
		goto abort_alloc;
	SONIC_DDPRINT("FPGA reset successed\n");

	/* get RX base addr */
	SONIC_DDPRINT("Issue SONIC_CMD_GET_RX_OFFSET\n");
	if ((ret = sonic_send_cmd(port, SONIC_CMD_GET_RX_OFFSET, 0, 0, 0)) == -1)
		goto abort_alloc;	
	SONIC_DDPRINT("RX_OFFSET = %d\n", ret);
	port->rx_offset = ret;

	/* get RX size */
	SONIC_DDPRINT("Issue SONIC_CMD_GET_RX_SIZE\n");
	if ((ret = sonic_send_cmd(port, SONIC_CMD_GET_RX_SIZE, 0, 0, 0)) == -1 
			|| ret == 0) 
		goto abort_alloc;	
	SONIC_DDPRINT("RX_SIZE = %d\n", ret);
	port->rx_size = ret;

#define RX_BLOCK_SIZE		(4096 >> 2 )
	/* set RX block size */
	SONIC_DDPRINT("Issue SONIC_CMD_SET_RX_BLOCK_SIZE\n");
	if ((ret = sonic_send_cmd(port, SONIC_CMD_SET_RX_BLOCK_SIZE, RX_BLOCK_SIZE , 0, 0)) == -1)
		goto abort_alloc;
	SONIC_DDPRINT("RX_BLOCK_SIZE is set to %x\n", RX_BLOCK_SIZE);
#undef RX_BLOCK_SIZE

	/* get RX block size */
	SONIC_DDPRINT("Issue SONIC_CMD_GET_RX_BLOCK_SIZE\n");
	if ((ret = sonic_send_cmd(port, SONIC_CMD_GET_RX_BLOCK_SIZE, 0, 0, 0)) == -1 
			|| ret ==0) 
		goto abort_alloc;	
	SONIC_DDPRINT("RX_BLOCK_SIZE = %d\n", ret);
	port->rx_blk_size = ret;
	
	/* get TX base addr */
	SONIC_DDPRINT("Issue SONIC_CMD_TX_OFFSET\n");
	if ((ret = sonic_send_cmd(port, SONIC_CMD_GET_TX_OFFSET, 0, 0, 0)) == -1) 
		goto abort_alloc;	
	SONIC_DDPRINT("TX_OFFSET = %d\n", ret);
	port->tx_offset = ret;

	/* set irq data */
	SONIC_DDPRINT("Issue SONIC_CMD_SET_ADDR_IRQ\n");
	if (sonic_send_cmd(port, SONIC_CMD_SET_ADDR_IRQ, 
			pci_dma_h(port->irq_bus), pci_dma_l(port->irq_bus), 0))
		goto abort_alloc;
	SONIC_DDPRINT("SoNIC IRQ data is set to 0x%.16llx\n", port->irq_bus);
	SONIC_DDPRINT("** irq addr = %x %x\n", port->ctrl->irq_header.irq_data_addr_h, port->ctrl->irq_header.irq_data_addr_l);

	SONIC_DDPRINT("rx_ptr = %x, tx_ptr = %x\n", port->irq_data->rx_ptr, 
		port->irq_data->tx_ptr);

	/* configure msi */
	SONIC_DDPRINT("Configure MSI\n");
	if (sonic_send_cmd(port, SONIC_CMD_CONFIG_IRQ, 1<<17, 0, 0))
		goto abort_alloc;
	SONIC_DDPRINT("irq msi is enabled\n");

	/* FIXME: */
	port->tx_pcs->ptr = &port->irq_data->tx_ptr;
//	SONIC_MPRINT(port->tx_pcs->ptr);
	port->rx_pcs->ptr = &port->irq_data->rx_ptr;
//	SONIC_MPRINT(port->rx_pcs->ptr);
	port->tx_pcs->dma_header = &port->ctrl->tx_header;
//	SONIC_MPRINT(port->tx_pcs->dma_header);
	port->rx_pcs->dma_header = &port->ctrl->rx_header;
//	SONIC_MPRINT(port->rx_pcs->dma_header);
	port->tx_pcs->ep_size = port->rx_size << 4;
	port->rx_pcs->ep_size = port->rx_size << 4;
	port->tx_pcs->ep_offset = port->tx_offset;
	port->rx_pcs->ep_offset = port->rx_offset;

	sonic_dma_setup(port->tx_pcs->dma_header, port->tx_pcs->dma_num_desc, 
		port->tx_pcs->dt_bus, 0);
	sonic_dma_setup(port->rx_pcs->dma_header, port->rx_pcs->dma_num_desc, 
		port->rx_pcs->dt_bus, 0);

	SONIC_PRINT("SoNIC hardware is successfully initialized\n");
	return 0;
abort_alloc:
	sonic_fpga_port_exit(port);
abort:
	SONIC_ERROR("Error\n");
	return -1;
}

int sonic_fpga_init(struct sonic_priv *sonic)
{
	int i;
	for ( i = 0 ; i < SONIC_PORT_SIZE ; i ++ ) {
		if(sonic_fpga_port_init(sonic->ports[i])){
			sonic_fpga_exit(sonic);
			return -1;
		}
	}

	return 0;
}

void __devinit sonic_scan_bars(struct sonic_priv *sonic)
{
	int i;
	struct pci_dev *pdev = sonic->pdev;
	for ( i =0 ; i < SONIC_BAR_NUM ; i ++) {
		unsigned long bar_start = pci_resource_start(pdev, i);
		if(bar_start) {
			unsigned long bar_end = pci_resource_end(pdev, i);
			unsigned long bar_flags = pci_resource_flags(pdev, i);
			SONIC_PRINT("BAR %d 0x%08lx-0x%08lx flags 0x%08lx\n",
				i, bar_start, bar_end, bar_flags);
		}
	}
}

void sonic_unmap_bar(struct sonic_port *port)
{
	struct pci_dev *pdev = port->sonic->pdev;

	if (port->ctrl) {
		pci_iounmap(pdev, port->ctrl);
		port->ctrl = NULL;
	}
}

int sonic_map_bar(struct sonic_port *port)
{
	int bar_idx = port->port_id + SONIC_BAR_BASE;
	struct pci_dev *pdev = port->sonic->pdev;
	unsigned long bar_length = pci_resource_len(pdev, bar_idx);
	unsigned long bar_end = pci_resource_len(pdev, bar_idx);

	if (bar_length == 0 && bar_end == 0)
		return -1;

	port->ctrl = pci_iomap(pdev, bar_idx, bar_length);
	if (!port->ctrl) {
		SONIC_ERROR("iomap failed bar%d, port_id %d\n", bar_idx, port->port_id);
		return -1;
	}

	SONIC_DPRINT("BAR[%d] for port %d mapped at 0x%p with length %lu\n", bar_idx,
			port->port_id, port->ctrl, bar_length);

	return 0;
}
