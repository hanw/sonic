#if SONIC_KERNEL
#include <linux/inet.h>
#include <linux/pci.h>
#else /* SONIC_KERNEL */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#endif /* SONIC_KERNEL */

#include "sonic.h"
#include "fifo.h"
#include "mode.h"

#define DMA_BUF_ORDER		0	// power of 2
#define DMA_NUM_DESC		1

void sonic_free_pcs(struct sonic_pcs *pcs)
{
#if SONIC_PCIE
//	pci_free_consistent(pcs->port->sonic->pdev, sizeof(struct sonic_dma_table),
//			pcs->dt, pcs->dt_bus);
	FREE(pcs->dt);
#endif
	free_fifo(pcs->fifo);	

	FREE_PAGES(pcs->dma_buf, pcs->dma_buf_order + 1);
	FREE(pcs);
}

//FIXME: error handling
struct sonic_pcs * sonic_alloc_pcs(struct sonic_port *port, int dma_buf_order, int dma_num_desc, int fifo_fnum, int fifo_porder, int dir, int cpu)
{
	struct sonic_pcs *pcs;
	SONIC_DDPRINT("cpuid = %d\n", cpu);

	pcs = ALLOC(sizeof(struct sonic_pcs));
	if (!pcs) {
		SONIC_ERROR("pcs alloc failed\n");
		return NULL;
	}
	SONIC_MPRINT(pcs, "[%d]" , port->port_id);

	pcs->fifo = alloc_fifo(fifo_fnum, fifo_porder, &port->stopper);
	if (!pcs->fifo) {
		FREE(pcs);
		return NULL;
	}

	pcs->cpu = cpu;
	pcs->port = port;
	pcs->pcs_fifo = NULL;

	pcs->dic = 0;

	// setting dma buffer
	pcs->dma_buf = ALLOC_PAGES(dma_buf_order + 1);
	if (!pcs->dma_buf) {
		SONIC_ERROR("dma alloc failed\n");
		return NULL;
	}
	SONIC_MPRINT(pcs->dma_buf, "[%d]", port->port_id);

	memset(pcs->dma_buf, 0, order2bytes(dma_buf_order+1));

	pcs->dma_buf_order = dma_buf_order;
	pcs->dma_buf_size = order2bytes(dma_buf_order);
	pcs->dma_buf_pages = power_of_two(dma_buf_order);

	pcs->dma_cur_buf = (uint64_t *) pcs->dma_buf;
	pcs->dma_cur_buf_idx = 0;

	// FIXME
	pcs->dma_page = pcs->dma_buf;
	pcs->dma_page_index = 0;

	SONIC_DDPRINT("dma_buf_order = %d, dma_buf_size = %x (%d), \n", dma_buf_order, 
			pcs->dma_buf_size, pcs->dma_buf_size);

	pcs->debug = 0;

#if SONIC_PCIE
	// setting dma descriptor table
	pcs->dma_bus = virt_to_bus(pcs->dma_buf);

//	pcs->dt = pci_alloc_consistent(sonic->pdev, sizeof(struct sonic_dma_table),
//			&pcs->dt_bus);
	pcs->dt = ALLOC(sizeof(struct sonic_dma_table));
	pcs->dt_bus = virt_to_bus(pcs->dt);
	if(pcs->dt == NULL) {
		SONIC_ERROR("dt alloc failed\n");
		FREE_PAGES(pcs->dma_buf, dma_buf_order + 1);
		FREE(pcs);
		return NULL;
	}
	SONIC_MPRINT(pcs->dt, "[%p]", pcs->port_id);

	pcs->dt->w3 = 0;
	pcs->dma_num_desc = dma_num_desc;
#endif /* SONIC_PCIE */

	if (dir == SONIC_DIR_TX) {
//		pcs->dma_header = &port->ctrl->tx_header;
//		pcs->ep_offset = port->tx_offset;
//		pcs->ep_size = port->rx_size << 4;	// FIXME rx_size is # of OWORDS
		pcs->dma_idx = 0;
//		pcs->ptr = &port->irq_data->tx_ptr2;
	} else {
//		pcs->dma_header = &port->ctrl->rx_header;
//		pcs->ep_offset = port->rx_offset;
//		pcs->ep_size = port->rx_size << 4;
		pcs->dma_idx = pcs->dma_buf_size >> 3;
//		pcs->ptr = &port->irq_data->rx_ptr;
	}

	return pcs;
}

void sonic_free_mac(struct sonic_mac *mac)
{
	free_fifo(mac->fifo);
	FREE(mac);
}

struct sonic_mac * sonic_alloc_mac(struct sonic_port *port, int fifo_fnum, int fifo_porder, int cpu) 
{
	struct sonic_mac * mac;

	mac = ALLOC(sizeof(struct sonic_mac));
	if (!mac) {
		SONIC_ERROR("mac alloc failed\n");
		return NULL;
	}

	mac->port = port;
	mac->port_id = port->port_id;
	mac->cpu = cpu;

	mac->fifo = alloc_fifo(fifo_fnum, fifo_porder, &port->stopper);
	if (!mac->fifo) {
		FREE(mac);
		return NULL;
	}

	mac->data = NULL;

	mac->in_fifo = NULL;
	mac->out_fifo = NULL;

	return mac;
}

void sonic_free_logger(struct sonic_logger *logger)
{
    sonic_free_logs(logger);
    sonic_free_ins(logger);

	free_fifo(logger->fifo);
	FREE(logger);
}

struct sonic_logger * sonic_alloc_logger(struct sonic_port *port, int fifo_fnum, int fifo_porder, int cpu,
		int log_porder)
{
	struct sonic_logger *logger;

	logger = ALLOC(sizeof(struct sonic_logger));
	if (!logger) {
		SONIC_ERROR("logger alloc failed\n");
		return NULL;
	}

	logger->port = port;
	logger->port_id = port->port_id;
	logger->cpu = cpu;

	logger->fifo = alloc_fifo(fifo_fnum, fifo_porder, &port->stopper);
	if (!logger->fifo) {
		FREE(logger);
		return NULL;
	}

	logger->log_porder = log_porder;
	INIT_LIST_HEAD(&logger->log_head);
	INIT_LIST_HEAD(&logger->ins_head);

#if SONIC_KERNEL
	spin_lock_init(&logger->lock);
	atomic_set(&logger->refcnt, 0);
#endif

	logger->in_fifo = NULL;
	logger->out_fifo = NULL;

	return logger;
}

static void sonic_free_port(struct sonic_port *port)
{
	if (port->tx_pcs)
		sonic_free_pcs(port->tx_pcs);
	if (port->rx_pcs)
		sonic_free_pcs(port->rx_pcs);
	if (port->tx_mac)
		sonic_free_mac(port->tx_mac);
	if (port->rx_mac)
		sonic_free_mac(port->rx_mac);
	if (port->log)
		sonic_free_logger(port->log);
    if (port->gen)
        sonic_free_pkt_gen(port);

#if SONIC_SFP
	sonic_fpga_port_exit(port);
#endif

	FREE(port);
}

void sonic_free_ports(struct sonic_priv *sonic)
{
	int i;
	for ( i = 0 ; i < SONIC_PORT_SIZE ; i ++ ) {
		if (sonic->ports[i]) {
			sonic_free_port(sonic->ports[i]);
			sonic->ports[i] = NULL;
		}
	}
}

static int sonic_alloc_tx(struct sonic_port *port, int port_id)
{
	struct tester_args *args = &port->sonic->tester_args;

	sonic_set_cpu(SONIC_SOCKET(args, tx, port_id));

	port->tx_pcs = sonic_alloc_pcs(port, args->tx_dma_porder, args->tx_dma_num_desc, 
			args->tx_pcs_fnum, args->tx_pcs_porder, SONIC_DIR_TX, SONIC_CPU(args, tx, port_id, tx_pcs));
	if (!port->tx_pcs) 
		return -1;

	port->tx_pcs->port_id = port_id;

	port->tx_mac = sonic_alloc_mac(port, args->tx_mac_fnum, args->tx_mac_porder, 
			SONIC_CPU(args, tx, port_id, tx_mac));

	if (!port->tx_mac) {
		sonic_free_pcs(port->tx_pcs);
		return -1;
	}

    // TODO
    sonic_alloc_pkt_gen(port);

	return 0;
}

static int sonic_alloc_rx(struct sonic_port *port, int port_id)
{
	struct tester_args *args = &port->sonic->tester_args;

	sonic_set_cpu(SONIC_SOCKET(args, rx, port_id));

	port->rx_pcs = sonic_alloc_pcs(port, args->rx_dma_porder, args->rx_dma_num_desc, 
			args->rx_pcs_fnum, args->rx_pcs_porder, SONIC_DIR_RX, SONIC_CPU(args, rx, port_id, rx_pcs));
	if (!port->rx_pcs) 
		return -1;

	port->rx_pcs->port_id = port_id;

	port->rx_mac = sonic_alloc_mac(port, args->rx_mac_fnum, args->rx_mac_porder, 
			SONIC_CPU(args, rx, port_id, rx_mac));

	if (!port->rx_mac) {
		sonic_free_pcs(port->rx_pcs);
		return -1;
	}

	port->log = sonic_alloc_logger(port, args->rx_mac_fnum, args->rx_mac_porder,
			SONIC_CPU(args, rx, port_id, log), args->log_porder);
	if (!port->log) {
		sonic_free_pcs(port->rx_pcs);
		sonic_free_mac(port->rx_mac);
		return -1;
	}


	return 0;
}

static struct sonic_port * sonic_alloc_port(struct sonic_priv *sonic, int port_id)
{
	struct sonic_port *port;
	struct tester_args *args = &sonic->tester_args;

	SONIC_DDPRINT("allocating port [%d] in cpu %d\n", port_id, sonic_get_cpu());

	port = ALLOC(sizeof(struct sonic_port));
	if (!port) {
		SONIC_ERROR("allocation failed\n");
		return NULL;
	}

	memset(port, 0, sizeof(struct sonic_port));

	port->sonic = sonic;
	port->port_id = port_id;

	// FIXME
	sonic_set_port_info_id(&port->info, args, port_id);

	if (sonic_alloc_tx(port, port_id))
		goto out;

	if (sonic_alloc_rx(port, port_id))
		goto out;

	// FIXME: for NUMA allocation
	sonic_set_cpu(SONIC_SOCKET(args, port, port_id));

#if SONIC_KERNEL
	spin_lock_init(&port->lock);
#endif /* SONIC_KERNEL */

#if SONIC_SFP
	if (sonic_fpga_port_init(port))
		goto out;
#endif /* SONIC_SFP */

	return port;

out:
	sonic_free_port(port);
	return NULL;
}

int sonic_alloc_ports(struct sonic_priv *sonic)
{
	int i;

	for ( i = 0 ; i < SONIC_PORT_SIZE ; i ++ ) {
		if (!(sonic->ports[i] = sonic_alloc_port(sonic, i))) {
			sonic_free_ports(sonic);
			return -1;
		}
	}

	return 0;
}
