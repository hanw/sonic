#if SONIC_KERNEL
#include <linux/inet.h>
#include <linux/pci.h>
#else /* SONIC_KERNEL */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <errno.h>
#endif /* SONIC_KERNEL */

#include "sonic.h"
#include "fifo.h"
#include "mode.h"

static void sonic_free_pcs(struct sonic_pcs *pcs)
{
    SONIC_DDPRINT("\n");
#if SONIC_SFP
    FREE(pcs->dt);
#endif /* SONIC_SFP */
    FREE_PAGES(pcs->dma_buf, pcs->dma_buf_order + 1);
    FREE(pcs);
}

static inline void sonic_reset_pcs(struct sonic_pcs *pcs)
{
    pcs->in_fifo = NULL;
    pcs->out_fifo = NULL;

    pcs->dma_idx = 0;
    pcs->dic = 0;
    pcs->debug =0 ;

    pcs->beginning_idles = 16;
    pcs->r_state = RX_INIT;
    pcs->state = PCS_INITIAL_STATE;

    memset(&pcs->stat, 0, sizeof(struct sonic_pcs_stat));

#if SONIC_PCIE
    pcs->dt->w3 = pcs->dma_num_desc - 1;
#endif

#if SONIC_SYNCH
    pcs->synch_counter=0;
    pcs->synch_rcounter=0;

    pcs->synch_debug=0;
    pcs->synch_debug2=0;
    pcs->synch_debug3=0;
    pcs->synch_offset=0;
    pcs->synch_roffset=0;
#endif
}

//FIXME: error handling
static struct sonic_pcs * sonic_alloc_pcs(struct sonic_port *port, int dma_buf_order, 
        int dma_num_desc, int fifo_fnum, int fifo_exp, int dir)
{
    struct sonic_pcs *pcs;
    SONIC_DDPRINT("\n");

    pcs = ALLOC(sizeof(struct sonic_pcs));
    if (!pcs) {
        SONIC_ERROR("pcs alloc failed\n");
        return NULL;
    }
    SONIC_MPRINT(pcs, "[%d]" , port->port_id);

    pcs->port = port;
    pcs->port_id = port->port_id;
    pcs->in_fifo = NULL;
    pcs->out_fifo = NULL;

    pcs->dic = 0;
    pcs->beginning_idles=0;    // FIXME

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

#if SONIC_SFP
    // setting dma descriptor table
    pcs->dma_bus = virt_to_bus(pcs->dma_buf);

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
#endif /* SONIC_SFP */

#if SONIC_SYNCH
    pcs->synch_counter=0;
    pcs->synch_rcounter=0;
    spin_lock_init(&pcs->synch_lock);
#endif /* SONIC_SYNCH */

    if (dir == SONIC_DIR_TX) 
            pcs->dma_idx = 0;
    else
            pcs->dma_idx = pcs->dma_buf_size >> 3;

    return pcs;
}

static void sonic_free_mac(struct sonic_mac *mac)
{
    SONIC_DDPRINT("\n");
    FREE(mac);
}

static inline void sonic_reset_mac(struct sonic_mac *mac)
{
    mac->in_fifo = NULL;
    mac->out_fifo = NULL;

    memset(&mac->stat, 0, sizeof(struct sonic_mac_stat));
}

static struct sonic_mac * sonic_alloc_mac(struct sonic_port *port)
{
    struct sonic_mac * mac;

    SONIC_DDPRINT("\n");

    mac = ALLOC(sizeof(struct sonic_mac));
    if (!mac) {
        SONIC_ERROR("mac alloc failed\n");
        return NULL;
    }

    mac->port = port;
    mac->port_id = port->port_id;

    mac->in_fifo = NULL;
    mac->out_fifo = NULL;

    return mac;
}

static void sonic_free_app_lists(struct list_head *head, int exp)
{
    struct sonic_app_list *list, *tmp;

    list_for_each_entry_safe(list, tmp, head, head) {
        list_del(&list->head);
        FREE_PAGES(list, exp);
    }
}

void sonic_free_app_in_lists(struct sonic_app *app)
{
    sonic_free_app_lists(&app->in_head, app->app_exp);
}

void sonic_free_app_out_lists(struct sonic_app *app)
{
    sonic_free_app_lists(&app->out_head, app->app_exp);
}

static void sonic_free_app(struct sonic_app *app)
{
    SONIC_DDPRINT("\n");
    sonic_free_app_in_lists(app);
    sonic_free_app_out_lists(app);

    FREE(app);
}

static inline void sonic_reset_app(struct sonic_app *app)
{
    app->in_fifo = NULL;
    app->out_fifo = NULL;

#if SONIC_KERNEL
    spin_lock_bh(&app->lock);
#endif /* SONIC_KERNEL */
    sonic_free_app_out_lists(app);        //TODO
#if SONIC_KERNEL
    spin_unlock_bh(&app->lock);
#endif /* SONIC_KERNEL */

    memset(&app->stat, 0, sizeof(struct sonic_app_stat));
}

static struct sonic_app * sonic_alloc_app(struct sonic_port *port, int fifo_exp) 
{
    struct sonic_app *app;

    SONIC_DDPRINT("\n");

    app = ALLOC(sizeof(struct sonic_app));
    if (!app) {
        SONIC_ERROR("app alloc failed\n");
        return NULL;
    }

    app->port = port;
    app->port_id = port->port_id;

    INIT_LIST_HEAD(&app->in_head);
    INIT_LIST_HEAD(&app->out_head);

#if SONIC_KERNEL
    spin_lock_init(&app->lock);
    atomic_set(&app->refcnt, 0);
#endif

    app->app_exp = fifo_exp;
    app->in_fifo = NULL;
    app->out_fifo = NULL;

    return app;
}

static void sonic_free_port(struct sonic_port *port)
{
    int i;
    SONIC_DDPRINT("\n");
    if (port->tx_pcs)
        sonic_free_pcs(port->tx_pcs);
    if (port->rx_pcs)
        sonic_free_pcs(port->rx_pcs);
    if (port->tx_mac)
        sonic_free_mac(port->tx_mac);
    if (port->rx_mac)
        sonic_free_mac(port->rx_mac);
    if (port->app)
        sonic_free_app(port->app);

    for ( i = 0 ; i < 4 ; i ++ ) 
        free_fifo(port->fifo[i]);

    FREE(port);
}

static void sonic_reset_port(struct sonic_port *port)
{
    int i;
#if SONIC_SFP
    int ret;
    ret = sonic_fpga_reset(port);
    if (ret == -1) 
        return;
#endif /* SONIC_SFP */

#define SONIC_THREADS(x,y,z)    \
    sonic_reset_##y (port->x);
    SONIC_THREADS_ARGS
#undef SONIC_THREADS
#if SONIC_KERNEL
    atomic_set(&port->threads, 0);
#endif /* SONIC_THREADS */

    for ( i = 0 ; i < 4 ; i ++ ) 
        reset_fifo(port->fifo[i]);

    port->runtime_funcs = NULL;

    port->stopper = 0;
}

void sonic_free_ports(struct sonic_priv *sonic)
{
    int i;
    SONIC_DDPRINT("\n");
    for ( i = 0 ; i < SONIC_PORT_SIZE ; i ++ ) {
        if (sonic->ports[i]) {
            sonic_free_port(sonic->ports[i]);
            sonic->ports[i] = NULL;
        }
    }
}

void sonic_print_port_info(struct sonic_port_info *info)
{
    SONIC_DDPRINT("-eth_src = %.2x%.2x%.2x%.2x%.2x%.2x\n", 
        info->mac_src[0], info->mac_src[1], info->mac_src[2], 
        info->mac_src[3], info->mac_src[4], info->mac_src[5]);
    SONIC_DDPRINT("-eth_dst = %.2x%.2x%.2x%.2x%.2x%.2x\n", 
        info->mac_dst[0], info->mac_dst[1], info->mac_dst[2], 
        info->mac_dst[3], info->mac_dst[4], info->mac_dst[5]);

    SONIC_DDPRINT("-ip_src = %x --ip_dst = %x\n", ntohl(info->ip_src), 
        ntohl(info->ip_dst));
    SONIC_DDPRINT("-port_src = %d --port_dst = %d\n", info->port_src,
        info->port_dst);

#define SONIC_CONFIG_RUNTIME(x,y,z)     \
    SONIC_DDPRINT("-%s = %d\n", #x, info-> x);
    SONIC_CONFIG_PKT_GEN_INT_ARGS
#undef SONIC_CONFIG_RUNTIME
}

void sonic_print_port_infos(struct sonic_priv * sonic)
{
    int i =0 ;
    for ( i =0 ; i < SONIC_PORT_SIZE ; i ++ ) {
        SONIC_DDPRINT("SoNIC Port %d info----------\n", i);
        sonic_print_port_info(&sonic->ports[i]->info);
    }
}

void sonic_set_port_info(struct sonic_port_info *info,
        struct sonic_config_runtime_args *args, int port_id)
{
    if(sonic_str_to_mac(args->ports[port_id].mac_src, info->mac_src)) {
        SONIC_ERROR("Invalid src MAC\n");
        memset(info->mac_src, 0, 6);
    }
    if(sonic_str_to_mac(args->ports[port_id].mac_dst, info->mac_dst)) {
        SONIC_ERROR("Invalid dst MAC\n");
        memset(info->mac_dst, 0, 6);
    }
#if SONIC_KERNEL
    info->ip_src = in_aton(args->ports[port_id].ip_src);
    info->ip_dst = in_aton(args->ports[port_id].ip_dst);
#else /*SONIC_KERNEL*/
    info->ip_src = inet_addr(args->ports[port_id].ip_src);
    info->ip_dst = inet_addr(args->ports[port_id].ip_dst);
#endif /*SONIC_KERNEL*/
    info->port_src = args->ports[port_id].port_src;
    info->port_dst = args->ports[port_id].port_dst;

    info->vlan_id = args->ports[port_id].vlan_id;

    strncpy(info->covert_msg, args->ports[port_id].covert_msg, 16);        // FIXME

#define SONIC_CONFIG_RUNTIME(x,y,z)     \
    info->x = args->ports[port_id].x;
    SONIC_CONFIG_PKT_GEN_INT_ARGS
#undef SONIC_CONFIG_RUNTIME
}

void sonic_set_port_infos(struct sonic_priv *sonic)
{
    int i = 0;

    sonic_check_config_runtime_args(&sonic->runtime_args);

    for ( i =0 ; i < SONIC_PORT_SIZE; i ++ ) 
        sonic_set_port_info(&sonic->ports[i]->info, &sonic->runtime_args, i);
}

static struct sonic_port * sonic_alloc_port(struct sonic_priv *sonic, int port_id)
{
    struct sonic_port *port;
    struct sonic_config_system_args *sargs = &sonic->system_args;
    int i;

    SONIC_DPRINT("Allocating port [%d]\n", port_id);

    // FIXME: for NUMA allocation
    sonic_set_cpu(SONIC_SOCKET(sargs, port, port_id));

    port = ZALLOC(sizeof(struct sonic_port));
    if (!port) {
        SONIC_ERROR("allocation failed\n");
        return NULL;
    }

    port->sonic = sonic;
    port->port_id = port_id;

    // TX Path
    port->tx_pcs = sonic_alloc_pcs(port, sargs->tx_dma_exp, 
            sargs->tx_dma_num_desc, sargs->fifo_num_ent, sargs->fifo_exp, 
            SONIC_DIR_TX);
    if (!port->tx_pcs)  {
        SONIC_ERROR("TX_PCS allocation failed\n");
        goto out;
    }
    snprintf(port->tx_pcs->name, 8, "tx_pcs%d", port_id);

    port->tx_mac = sonic_alloc_mac(port);
    if (!port->tx_mac) {
        SONIC_ERROR("TX_MAC allocation failed\n");
        goto out;
    }
    snprintf(port->tx_mac->name, 8, "tx_mac%d", port_id);

    // RX Path
    port->rx_pcs = sonic_alloc_pcs(port, sargs->rx_dma_exp, 
            sargs->rx_dma_num_desc, sargs->fifo_num_ent, sargs->fifo_exp, 
            SONIC_DIR_RX);
    if (!port->rx_pcs) {
        SONIC_ERROR("RX_PCS allocation failed\n");
        goto out;
    }
    snprintf(port->rx_pcs->name, 8, "rx_pcs%d", port_id);

    port->rx_mac = sonic_alloc_mac(port);
    if (!port->rx_mac) {
        SONIC_ERROR("RX_MAC allocation failed\n");
        goto out;
    }
    snprintf(port->rx_mac->name, 8, "rx_mac%d", port_id);

    // APP
    port->app = sonic_alloc_app(port, sargs->fifo_exp);
    if (!port->app) {
        SONIC_ERROR("APP allocation failed\n");
        goto out;
    }
    snprintf(port->app->name, 8, "app%d", port_id);

    for (i = 0 ; i < 4 ; i ++ ) 
        port->fifo[i] = alloc_fifo(sargs->fifo_num_ent, sargs->fifo_exp, 
                &port->stopper);

#if SONIC_KERNEL
    spin_lock_init(&port->lock);
#endif /* SONIC_KERNEL */

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

int __sonic_reset(struct sonic_priv *sonic)
{
    int i;

    sonic_reset_stats(sonic);

    for ( i = 0 ; i < SONIC_PORT_SIZE ; i ++ ) 
        sonic_reset_port(sonic->ports[i]);

    return 0;
}

int __sonic_init(struct sonic_priv **psonic)
{
    struct sonic_priv *s = 0;
    int ret = 0;

    SONIC_PRINT("Initializing SoNIC\n");

    s = ZALLOC(sizeof(struct sonic_priv));
    if (s == NULL) {
        SONIC_ERROR("sonic_priv allocation failed\n");
        ret = -ENOMEM; 
        goto abort;
    }

    sonic_init_config_system_args (&s->system_args);

#if SONIC_DDEBUG
    sonic_print_config_system_args (&s->system_args);
#endif /* SONIC_DDEBUG */

#if SONIC_KERNEL
    spin_lock_init(&s->lock);
#if SONIC_PROC
    if ((ret = sonic_proc_init(s)))
        goto abort_sonic;
#endif /* SONIC_PROC */
#if SONIC_FS
    if ((ret = sonic_fs_init(s, SONIC_PORT_SIZE)))
        goto abort_proc;
#endif /* SONIC_FS */
#endif /* SONIC_KERNEL */

    if ((ret = sonic_alloc_ports(s)) != 0) {
        SONIC_ERROR("sonic_alloc_ports failed\n");
        goto abort_fs;
    }

    *psonic = sonic = s;
    return 0;

abort_fs:
#if SONIC_KERNEL
#if SONIC_FS
    sonic_fs_exit(s);
abort_proc:
#endif /* SONIC_FS */
#if SONIC_PROC
    sonic_proc_exit(s);
abort_sonic:
#endif /* SONIC_PROC */
#endif /* SONIC_KERNEL */
    FREE(s);
abort:
    *psonic = sonic = NULL;
    return ret;
}

void __sonic_exit(struct sonic_priv *s)
{
    SONIC_PRINT("\n");

    sonic_free_ports(s);
#if SONIC_KERNEL
#if SONIC_FS
    sonic_fs_exit(s);
#endif /* SONIC_FS */
#if SONIC_PROC
    sonic_proc_exit(s);
#endif /* SONIC_PROC */
#endif /* SONIC_KERNEL */
    FREE(s);
    sonic = NULL;
}
