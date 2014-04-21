#include "sonic.h"
#include "mode.h"
#include "covert.h"

static struct sonic_port_args tx_pcs_tester = {
    .tx_pcs = sonic_tx_pcs_tester,
};

#if 0
static struct sonic_port_args tx_covert_pcs_tester = {
    .tx_pcs = sonic_covert_tx_pcs_tester,
};
#endif

static struct sonic_port_args tx_mac_tester = {
    .tx_mac = sonic_tx_mac_tester,
};

static struct sonic_port_args rx_pcs_tester = {
    .rx_pcs = sonic_rx_pcs_tester,
};

static struct sonic_port_args rx_covert_pcs_tester = {
    .rx_pcs = sonic_covert_rx_pcs_tester,
};

#if 0
static struct sonic_port_args tx_pcs = {
    .tx_pcs = sonic_tx_pcs_loop,
#if SONIC_SFP
    .rx_pcs = sonic_rx_pcs_idle_loop,
#endif
};

static struct sonic_port_args rx_pcs = {
    .rx_pcs = sonic_rx_pcs_loop,
#if SONIC_SFP
    .tx_pcs = sonic_tx_pcs_idle_loop,
#endif
};
#endif

static struct sonic_port_args txrx_pcs = {
	.tx_pcs = sonic_tx_pcs_loop,
	.rx_pcs = sonic_rx_pcs_loop,
};

static struct sonic_port_args txrx_pcs2 = {
	.tx_pcs = sonic_tx_pcs_scrambler_loop,
	.rx_pcs = sonic_rx_pcs_descrambler_loop,
};

static struct sonic_port_args txrx_pcs_cap = {
    .tx_pcs = sonic_tx_pcs_loop,
    .rx_pcs = sonic_rx_pcs_loop,
    .log = sonic_log_loop,
};

static struct sonic_port_args txrx_pcs_app = {
    .tx_pcs = sonic_tx_pcs_loop,
    .rx_pcs = sonic_rx_pcs_loop,
    .log = sonic_availbw_loop,  // FIXME
};

static struct sonic_port_args txrx_mac = {
    .tx_pcs = sonic_tx_pcs_loop,
    .tx_mac = sonic_tx_mac_loop,
    .rx_pcs = sonic_rx_pcs_loop,
};

static struct sonic_port_args txrx_mac_cap = {
    .tx_pcs = sonic_tx_pcs_loop,
    .tx_mac = sonic_tx_mac_loop,
    .rx_pcs = sonic_rx_pcs_loop,
    .log = sonic_log_loop,
};

static struct sonic_port_args txrx_mac_app = {
    .tx_pcs = sonic_tx_pcs_loop,
    .tx_mac = sonic_tx_mac_loop,
    .rx_pcs = sonic_rx_pcs_loop,
    .log = sonic_log_loop,  // FIXME
};

static struct sonic_port_args pkt_gen = {
	.tx_pcs = sonic_tx_pcs_loop,
	.tx_mac = sonic_pkt_generator_loop,
#if SONIC_SFP
//	.rx_pcs = sonic_rx_pcs_idle_loop,
#endif
};

static struct sonic_port_args pkt_gen2 = {
	.tx_pcs = sonic_tx_pcs_loop,
	.tx_mac = sonic_pkt_generator_loop2,
#if SONIC_SFP
//	.rx_pcs = sonic_rx_pcs_idle_loop,
#endif
};

static struct sonic_port_args pkt_gen_covert= {
	.tx_pcs = sonic_tx_pcs_loop,
	.tx_mac = sonic_pkt_generator_covert_loop,
#if SONIC_SFP
//	.rx_pcs = sonic_rx_pcs_idle_loop,
#endif
};

static struct sonic_port_args pkt_recv = {
	.rx_pcs = sonic_rx_pcs_loop,
	.rx_mac = sonic_pkt_receiver_loop,
#if SONIC_SFP
	.tx_pcs = sonic_tx_pcs_idle_loop,
#endif
};

static struct sonic_port_args pkt_cap = {
	.rx_pcs = sonic_rx_pcs_loop,
	.log = sonic_log_loop,
#if SONIC_SFP
	.tx_pcs = sonic_tx_pcs_idle_loop,
#endif
};

static struct sonic_port_args pkt_crccap = {
	.rx_pcs = sonic_rx_pcs_loop,
	.rx_mac = sonic_pkt_receiver_loop,
	.log = sonic_log_loop,
#if SONIC_SFP
	.tx_pcs = sonic_tx_pcs_idle_loop,
#endif
};

static struct sonic_port_args pkt_gencap = {
	.tx_pcs = sonic_tx_pcs_loop,
	.tx_mac = sonic_pkt_generator_loop,
	.rx_pcs = sonic_rx_pcs_loop,
	.log = sonic_log_loop,
};

#if 0
static struct sonic_port_args pcs_cap = {
	.tx_pcs = sonic_tx_pcs_loop,
	.rx_pcs = sonic_rx_pcs_loop,
	.log = sonic_log_loop,
};

static struct sonic_port_args pcs_reccap = {
	.tx_pcs = sonic_tx_pcs_loop,
	.rx_pcs = sonic_rx_pcs_loop,
	.rx_mac = sonic_pkt_receiver_loop,
	.log = sonic_log_loop,
};

static struct sonic_port_args mac_cap = {
	.tx_pcs = sonic_tx_pcs_loop,
	.tx_mac = sonic_pkt_receiver_loop, 
	.rx_pcs = sonic_rx_pcs_loop,
	.rx_mac = sonic_pkt_receiver_loop,
	.log = sonic_log_loop,
};
#endif

static struct sonic_port_args pkt_genrcv = {
	.tx_pcs = sonic_tx_pcs_loop,
	.tx_mac = sonic_pkt_generator_loop,
	.rx_pcs = sonic_rx_pcs_loop,
	.rx_mac = sonic_pkt_receiver_loop
};

static struct sonic_port_args pkt_repeater = {
    .tx_pcs = sonic_tx_pcs_loop,
//    .tx_mac = sonic_tx_mac_loop,
    .log = sonic_ins_loop,
#if SONIC_SFP
//    .rx_pcs = sonic_rx_pcs_idle_loop,
#endif
};

static struct sonic_port_args pkt_crc_repeater = {
    .tx_pcs = sonic_tx_pcs_loop,
    .tx_mac = sonic_tx_mac_loop,
    .log = sonic_ins_loop,
};

#if SONIC_SFP
static struct sonic_port_args tx_dma= {
    .tx_pcs = sonic_tx_dma_tester,
};

static struct sonic_port_args rx_dma= {
    .rx_pcs = sonic_rx_dma_tester,
};

static struct sonic_port_args rxtx_dma= {
    .tx_pcs = sonic_tx_dma_tester,
    .rx_pcs = sonic_rx_dma_tester,
};

static struct sonic_port_args tx_idle= {
    .tx_pcs = sonic_tx_pcs_idle_loop,
};

static struct sonic_port_args rx_idle= {
    .rx_pcs = sonic_rx_pcs_idle_loop,
};

static struct sonic_port_args rxtx_idle= {
    .tx_pcs = sonic_tx_pcs_idle_loop,
    .rx_pcs = sonic_rx_pcs_idle_loop,
};
#endif /* SONIC_SFP*/

static struct sonic_port_args null_tester={};

int sonic_set_port_args(struct sonic_priv * sonic)
{
    int ret = -1;

    switch(sonic->mode) {
#define SONIC_MODE(x,y,z,w,v)\
    case (x) :\
        sonic->port_args[0] = &z;\
        sonic->port_args[1] = &w;\
        ret = 0;\
        break;
    SONIC_MODE_ARGS;
#undef SONIC_MODE
    }

    return ret;
}

void sonic_init_rx_path(struct sonic_port *port)
{
    struct sonic_fifo *fifo = port->rx_pcs->fifo;
    port->rx_pcs->pcs_fifo = fifo;
    port->rx_mac->in_fifo = fifo;
}

void sonic_init_rx_path_cap(struct sonic_port *port)
{
    struct sonic_fifo *fifo = port->rx_pcs->fifo;
    port->rx_pcs->pcs_fifo = fifo;
    port->log->in_fifo = fifo;
}

void sonic_init_rx_path_crc_cap(struct sonic_port *port)
{
    struct sonic_fifo *fifo = port->rx_pcs->fifo;
    struct sonic_fifo *fifo2 = port->rx_mac->fifo;
    port->rx_pcs->pcs_fifo = fifo;
    port->rx_mac->in_fifo = fifo;
    port->rx_mac->out_fifo = fifo2;
    port->log->in_fifo = fifo2;
}

/*
void sonic_init_pcs_port_forward(struct sonic_port *port)
{
	struct sonic_fifo *fifo = port->rx_pcs->fifo;	

	port->rx_pcs->pcs_fifo = fifo;
	port->tx_pcs->pcs_fifo = fifo;
}
*/

void sonic_init_tx_path(struct sonic_port *port)
{
    struct sonic_fifo *fifo = port->tx_pcs->fifo;
    port->tx_pcs->pcs_fifo = fifo;
    port->tx_mac->out_fifo = fifo;
}

void sonic_init_tx_path_rep(struct sonic_port *port)
{
    struct sonic_fifo *fifo = port->tx_pcs->fifo;
    port->tx_pcs->pcs_fifo = fifo;
    port->log->out_fifo = fifo;
    
}

void sonic_init_tx_path_crc_rep(struct sonic_port *port)
{
    struct sonic_fifo *fifo = port->tx_mac->fifo;
    struct sonic_fifo *fifo2 = port->tx_pcs->fifo;
    port->tx_pcs->pcs_fifo = fifo2;
    port->tx_mac->out_fifo = fifo2;
    port->tx_mac->in_fifo = fifo;
    port->log->out_fifo = fifo;
}

void sonic_init_null(struct sonic_priv *sonic)
{
    return;
}

void sonic_init_port (struct sonic_priv *sonic)
{
    int i;
    for ( i = 0 ; i < SONIC_PORT_SIZE ; i ++ ) {
        sonic_init_tx_path(sonic->ports[i]);
        sonic_init_rx_path(sonic->ports[i]);
    }
}

// FIXME
void sonic_init_pcs_forward(struct sonic_priv *sonic)
{
    struct sonic_port *port0 = sonic->ports[0];
    struct sonic_port *port1 = sonic->ports[1];
    struct sonic_fifo *fifo0 = port0->tx_pcs->fifo;
    struct sonic_fifo *fifo1 = port1->tx_pcs->fifo;

    port0->rx_pcs->pcs_fifo = fifo1;
    port1->tx_pcs->pcs_fifo = fifo1;

    port1->rx_pcs->pcs_fifo = fifo0;
    port0->tx_pcs->pcs_fifo = fifo0;
}

void sonic_init_pcs_app_forward(struct sonic_priv *sonic)
{
    int i;
    for ( i = 0 ; i < SONIC_PORT_SIZE ; i++ ) {
        struct sonic_port *rx_port = sonic->ports[i];
        struct sonic_port *tx_port = sonic->ports[i==0?1:0];
        struct sonic_fifo *fifo = rx_port->rx_pcs->fifo;
        struct sonic_fifo *fifo2 = rx_port->log->fifo;

        rx_port->rx_pcs->pcs_fifo = fifo;
        rx_port->log->in_fifo = fifo;
        rx_port->log->out_fifo = fifo2;
        tx_port->tx_pcs->pcs_fifo = fifo2;
    }
}

void sonic_init_pcs_app_cap_forward(struct sonic_priv *sonic)
{
    struct sonic_port *port0 = sonic->ports[0];
    struct sonic_port *port1 = sonic->ports[1];
    struct sonic_fifo *fifo0 = port0->tx_pcs->fifo;

    struct sonic_fifo *fifo1 = port0->log->fifo;
    struct sonic_fifo *fifo2 = port1->log->fifo;
    struct sonic_fifo *fifo3 = port1->tx_pcs->fifo;

    port1->rx_pcs->pcs_fifo = fifo0;
    port0->tx_pcs->pcs_fifo = fifo0;

    port0->rx_pcs->pcs_fifo = fifo1;
    port0->log->in_fifo = fifo1;
    port0->log->out_fifo = fifo2;
    port1->log->in_fifo = fifo2;
    port1->log->out_fifo = fifo3;
    port1->tx_pcs->pcs_fifo = fifo3;
}

void sonic_init_mac_forward(struct sonic_priv *sonic)
{
    int i;
    for ( i = 0 ; i < SONIC_PORT_SIZE ; i++ ) {
        struct sonic_port *rx_port = sonic->ports[i];
        struct sonic_port *tx_port = sonic->ports[i==0?1:0];
        struct sonic_fifo *fifo = rx_port->rx_pcs->fifo;
        struct sonic_fifo *fifo2 = tx_port->tx_mac->fifo;

        rx_port->rx_pcs->pcs_fifo = fifo;
        tx_port->tx_mac->in_fifo = fifo;
        tx_port->tx_mac->out_fifo = fifo2;
        tx_port->tx_pcs->pcs_fifo = fifo2;
    }
}

void sonic_init_mac_app_forward(struct sonic_priv *sonic)
{
    int i;
    for ( i = 0 ; i < SONIC_PORT_SIZE ; i++ ) {
        struct sonic_port *rx_port = sonic->ports[i];
        struct sonic_port *tx_port = sonic->ports[i==0?1:0];
        struct sonic_fifo *fifo = rx_port->rx_pcs->fifo;
        struct sonic_fifo *fifo2 = rx_port->log->fifo;
        struct sonic_fifo *fifo3 = tx_port->tx_mac->fifo;

        rx_port->rx_pcs->pcs_fifo = fifo;
        rx_port->log->in_fifo = fifo;
        rx_port->log->out_fifo = fifo2;
        tx_port->tx_mac->in_fifo = fifo2;
        tx_port->tx_mac->out_fifo = fifo3;
        tx_port->tx_pcs->pcs_fifo = fifo3;
    }
}

#if 0
void sonic_init_pcs_rec_cap_forward(struct sonic_priv *sonic)
{
	int i;
	for ( i = 0 ; i < SONIC_PORT_SIZE ; i++ ) {
		struct sonic_port *port = sonic->ports[i];
		struct sonic_fifo *fifo = port->rx_pcs->fifo;	
		struct sonic_fifo *fifo2 = port->rx_mac->fifo;
		struct sonic_fifo *fifo3 = port->log->fifo;

		port->rx_pcs->pcs_fifo = fifo;
		port->rx_mac->in_fifo = fifo;

		port->rx_mac->out_fifo = fifo2;
		port->log->in_fifo = fifo2;

		port->log->out_fifo = fifo3;
		port->tx_pcs->pcs_fifo = fifo3;
	}
}

void sonic_init_pcs_cross_forward(struct sonic_priv *sonic)
{
	int i;
	for ( i = 0 ; i < SONIC_PORT_SIZE ; i ++ ) {
		struct sonic_port *rx_port = sonic->ports[i];
		struct sonic_port *tx_port = sonic->ports[i==0?1:0];
		struct sonic_fifo *fifo = sonic->ports[i]->rx_pcs->fifo;

		rx_port->rx_pcs->pcs_fifo = fifo;
		tx_port->tx_pcs->pcs_fifo = fifo;
	}
}

void sonic_init_mac_cap_forward(struct sonic_priv *sonic)
{
	int i;
	for ( i = 0 ; i < SONIC_PORT_SIZE ; i ++ ) {
		struct sonic_port *port = sonic->ports[i];
		struct sonic_fifo *fifo = port->rx_pcs->fifo;	
		struct sonic_fifo *fifo2 = port->rx_mac->fifo;
		struct sonic_fifo *fifo3 = port->log->fifo;
		struct sonic_fifo *fifo4 = port->tx_mac->fifo;

		port->rx_pcs->pcs_fifo = fifo;
		port->rx_mac->in_fifo = fifo;

		port->rx_mac->out_fifo = fifo2;
		port->log->in_fifo = fifo2;

		port->log->out_fifo = fifo3;
		port->tx_mac->in_fifo = fifo3;

		port->tx_mac->out_fifo = fifo4;
		port->tx_pcs->pcs_fifo = fifo4;
	}
}
#endif

void sonic_init_port_cap (struct sonic_priv *sonic)
{
    int i;
    for ( i = 0 ; i < SONIC_PORT_SIZE ; i ++ ) {
        sonic_init_tx_path(sonic->ports[i]);
        sonic_init_rx_path_cap(sonic->ports[i]);
    }
}

void sonic_init_port_crc_cap (struct sonic_priv *sonic)
{
    int i;
    for ( i = 0 ; i < SONIC_PORT_SIZE ; i ++ ) {
        sonic_init_tx_path(sonic->ports[i]);
        sonic_init_rx_path_crc_cap(sonic->ports[i]);
    }
}

void sonic_init_port_rep (struct sonic_priv *sonic)
{
    int i;
    for ( i = 0 ; i < SONIC_PORT_SIZE; i ++) {
        sonic_init_tx_path_rep(sonic->ports[i]);
        sonic_init_rx_path(sonic->ports[i]);
    }
}

void sonic_init_port_crc_rep (struct sonic_priv *sonic)
{
    int i;
    for ( i = 0 ; i < SONIC_PORT_SIZE ; i ++ ) {
        sonic_init_tx_path_crc_rep(sonic->ports[i]);
        sonic_init_rx_path(sonic->ports[i]);
    }
}

/*
void sonic_init_custom(struct sonic_priv *sonic)
{
	sonic_init_tx_path(sonic->ports[0]);
	sonic_init_rx_path_cap(sonic->ports[0]);
	sonic_init_pcs_port_forward(sonic->ports[1]);
}

void sonic_init_custom(struct sonic_priv *sonic)
{
//	sonic_init_port(sonic);
	sonic_init_pcs_cap_forward(sonic);
}
*/

void sonic_init_ports(struct sonic_priv *sonic, int mode)
{
#if 0
	switch(mode) {
	case MODE_PCS_FWRD:
		sonic_init_pcs_forward(sonic);
		break;
	case MODE_PKT_GEN_RCV:
		sonic_init_port(sonic);
		break;
	case MODE_PCS_CAP_FWRD:
		sonic_init_pcs_cap_forward(sonic);
		break;
	case MODE_PCS_REC_CAP_FWRD:
		sonic_init_pcs_rec_cap_forward(sonic);
		break;
	case MODE_MAC_CAP_FWRD:
		sonic_init_mac_cap_forward(sonic);
		break;
#if SONIC_TWO_PORTS
	case MODE_PCS2_FWRD:
	case MODE_PCS_FWRD2:
		sonic_init_pcs_forward(sonic);
		break;
	case MODE_PCS_CROSS:
//	case MODE_PCS_CROSS2:
		sonic_init_pcs_cross_forward(sonic);
		break;
//	case MODE_MAC_CROSS2:
		break;
	case MODE_PKT_GEN_RCV2:
	case MODE_PKT_CROSS:
	case MODE_PKT_CROSS2:
		sonic_init_port(sonic);
		break;
	case MODE_PKT_CAP_CROSS:
	case MODE_PKT_CAP_CROSS2:
	case MODE_PKT_GEN_CAP:
	case MODE_PKT_COVERT_CAP:
		sonic_init_port_cap(sonic);
		break;
    case MODE_PKT_RPT_CAP:
        sonic_init_port_rep(sonic);
        break;
//	case MODE_CUSTOM:
//		sonic_init_custom(sonic);
//		break;
#endif
	}
#endif
    switch(mode) {
#define SONIC_MODE(x,y,z,w,v) \
    case (x):\
        sonic_init_##v (sonic);\
        break;
    SONIC_MODE_ARGS;
#undef SONIC_MODE
    }
}

static void sonic_reset_fifo_path(struct sonic_priv *sonic)
{
    int i;
    struct sonic_port *port;
    for ( i = 0 ; i < SONIC_PORT_SIZE ; i ++ ) {
        port = sonic->ports[i];
        port->tx_pcs->pcs_fifo = NULL;
        port->rx_pcs->pcs_fifo = NULL;
        port->tx_mac->in_fifo = NULL;
        port->tx_mac->out_fifo = NULL;
        port->rx_mac->in_fifo = NULL;
        port->rx_mac->out_fifo = NULL;
        port->log->in_fifo = NULL;
        port->log->out_fifo = NULL;
    }
}

int sonic_init_sonic(struct sonic_priv *sonic, int mode)
{
    int ret=0, i;

#if SONIC_KERNEL
    spin_lock_bh(&sonic->lock);
    if (sonic->status)
        ret = -1;
    else
        sonic->status = 1;
    spin_unlock_bh(&sonic->lock);
#endif
    if (ret)
        return ret;

    sonic_reset_fifo_path(sonic);

#if 0
    if (mode == MODE_CRC_CMP) 
        ;
    else if (mode == MODE_PKT_RPT)
        sonic_init_port_rep(sonic);
    else if (mode == MODE_PKT_CRC_RPT)
        sonic_init_port_crc_rep(sonic);
    else if (mode < MODE_PKT_CAP) 
        sonic_init_port(sonic);
    else if (mode == MODE_PKT_CAP)
        sonic_init_port_cap(sonic);
    else if (mode == MODE_PKT_CRC_CAP)
        sonic_init_port_crc_cap(sonic);
    else //if (mode < MODE_PORT_MAX)
#endif
        sonic_init_ports(sonic, mode);
    //	else if (mode < MODE_MAX)
    //		;
    //	else {
    //		SONIC_ERROR("Unknown mode\n");
    //		ret = -1;
    //	}

    // application specifics
    for ( i = 0 ; i < SONIC_PORT_SIZE ; i ++ ) {
        if ((mode >= MODE_CRC && mode <= MODE_TX_MAX) ||
                (mode >= MODE_PKT_GEN_RCV && mode < MODE_PKT_COVERT_CAP)) {

            if (mode != MODE_PKT_RPT)
                sonic->ports[i]->tx_mac->data = sonic->ports[i]->gen;
//            else 
 //               sonic_ins_helper(sonic->ports[i]->log, "ins.tmp");

//            if (sonic_alloc_pkt_gen(sonic->ports[i]->tx_mac))
 //               return -1;
        }
    }

    sonic->mode = mode;
    SONIC_DPRINT("MODE= %d\n", mode);

    return ret;
}

void sonic_release_sonic(struct sonic_priv *sonic)
{
    int i;
    for ( i = 0 ; i < SONIC_PORT_SIZE ; i ++ ) {
        if ((sonic->mode >= MODE_CRC && sonic->mode <= MODE_TX_MAX) ||
                (sonic->mode >= MODE_PKT_GEN_RCV && sonic->mode < MODE_PKT_COVERT_CAP)) {
            sonic->ports[i]->tx_mac->data = NULL;
    //			sonic_free_pkt_gen(sonic->ports[i]->tx_mac);
        }
    }
}
