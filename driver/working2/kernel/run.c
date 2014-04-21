#if SONIC_KERNEL
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/sched.h>
#else /* !SONIC_KERNEL */
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include <unistd.h>
#include <pthread.h>
#endif /* SONIC_KERNEL */
#include "sonic.h"
#include "mode.h"

#if SONIC_KERNEL
static int sonic_run_thread(void *args)
#else /* !SONIC_KERNEL */
static void * sonic_run_thread(void *args)
#endif /* SONIC_KERNEL */
{
    struct sonic_thread *thread = (struct sonic_thread *) args;
#if !SONIC_KERNEL
    int ret=0;
#endif
    SONIC_DDPRINT("\n");
#if SONIC_KERNEL
    daemonize("sonic");
    atomic_inc(&thread->port->threads);
#endif /* SONIC_KERNEL */

    sonic_set_cpu(thread->cpu);

    SONIC_DPRINT("Thread %s is running on %d\n",
        thread->name, sonic_get_cpu());

    thread->func(thread);

#if SONIC_KERNEL
    atomic_dec(&thread->port->threads);
#else
    pthread_exit(&ret);
#endif /* SONIC_KERNEL */

    return 0;
}

static unsigned long int sonic_create_thread(void *args)
{
#if SONIC_KERNEL
    unsigned long flags = CLONE_FS | CLONE_FILES | CLONE_SIGHAND | CLONE_IO;
    return kernel_thread(sonic_run_thread, args, flags);
#else 
    int r;
    pthread_t pid;
    r = pthread_create(&pid, NULL, sonic_run_thread, args);
    if (r)
        pid = -1;

    return pid;
#endif
}

int sonic_start_port(struct sonic_port *port)
{
    unsigned long int ret=0;
    if (!port->runtime_funcs)
        return -1;

    // FIXME: if create_thread returns error
#define SONIC_THREADS(x, y, z)	\
    if (port->runtime_funcs-> x ) {	\
        SONIC_DDPRINT("%s\n", #x);	\
        port-> x ->func = port->runtime_funcs-> x ;	\
        ret = sonic_create_thread(port-> x ); \
        SONIC_DDPRINT("pthread_t %lu created\n", ret);\
        port->pid_##x = ret;	\
    }
    SONIC_THREADS_ARGS
#undef SONIC_THREADS

#if SONIC_SFP
    ret = sonic_fpga_start_sfp(port);
    if (ret == -1)
        return -1;
#endif
    return 0;
}

int sonic_start_sonic(struct sonic_priv *sonic)
{
    SONIC_DPRINT("\n");

    sonic_start_port(sonic->ports[0]);
    sonic_start_port(sonic->ports[1]);

    return 0;
}

static inline void sonic_stop_port(struct sonic_port *port)
{
    port->stopper = 1;
}

#if !SONIC_KERNEL
int sonic_wait_port(struct sonic_port *port)
{
    if (!port->runtime_funcs)
        return -1;

#define SONIC_THREADS(x, y, z)	\
    pthread_join(port->pid_##x, NULL);
    SONIC_THREADS_ARGS
#undef SONIC_THREADS
    return 0;
}
#else /* SONIC_KERNEL */
int sonic_wait_port(struct sonic_port *port)
{
    int ret =0;
    if (!port->runtime_funcs)
        return -1;

    while(1) {
        if (atomic_read(&port->threads) == 0)
            break;

        sonic_stop_port(port);
        schedule();
    }
    __set_current_state(TASK_RUNNING);

#if SONIC_SFP
    if (sonic_fpga_stop_sfp(port) == -1)
        ret=-1;
#endif
    return ret;
}
#endif /* SONIC_KERNEL */
int sonic_wait_sonic(struct sonic_priv *sonic)
{
    SONIC_DPRINT("\n");

    sonic_wait_port(sonic->ports[0]);
    sonic_wait_port(sonic->ports[1]);

    return 0;
}

void sonic_stop_sonic(struct sonic_priv *sonic)
{
    int i;
    SONIC_DPRINT("\n");

    for ( i = 0 ; i < SONIC_PORT_SIZE; i ++ ) {
        if (sonic->ports[i]->runtime_funcs)
            sonic_stop_port(sonic->ports[i]);
    }

    sonic_wait_sonic(sonic);
}


int sonic_run_sonic(struct sonic_priv *sonic)
{
    struct sonic_config_runtime_args *args = &sonic->runtime_args;
    SONIC_DPRINT("\n");

    if (sonic_start_sonic(sonic) == -1)
        return -1;

    SLEEP(args->duration);

    sonic_stop_sonic(sonic);

    sonic_merge_stats(sonic);

    SONIC_PRINT("DONE\n");

    return 0;
}

static int __sonic_acquire(struct sonic_priv *sonic)
{
    int ret=0;
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

    __sonic_reset(sonic);

    return ret;
}

static void __sonic_release(struct sonic_priv *sonic)
{
    sonic_free_app_in_lists(sonic->ports[0]->app);
    sonic_free_app_in_lists(sonic->ports[1]->app);

    sonic_print_stats(&sonic->stat);

#if SONIC_KERNEL
    spin_lock_bh(&sonic->lock);
    sonic->status = 0;
    spin_unlock_bh(&sonic->lock);
#endif
}

int __sonic_run(struct sonic_priv * sonic)
{
    int ret=0;
#if !SONIC_KERNEL
    struct sonic_config_runtime_args *args = &sonic->runtime_args;
#endif
    SONIC_DPRINT("\n");

    if (__sonic_acquire(sonic)) {
        SONIC_ERROR("SoNIC is running\n");
        return -1;
    }

    sonic_set_port_infos(sonic);
#if SONIC_DDEBUG
    sonic_print_port_infos(sonic);
#endif

    sonic_set_cpu(sonic->system_args.ctrl_cpu);

#if !SONIC_KERNEL
    if (args->mode == MODE_CRC_CMP)  {
        __sonic_release(sonic);
        return sonic_test_crc_value(sonic);
    } else if (args->mode >= MODE_PKT_RPT0 && 
            args->mode <= MODE_PKT_RPT_FULL) {
        sonic_rpt_helper(sonic->ports[0]->app, "rpt_helper.tmp", args->ports[0].pkt_cnt);
        sonic_rpt_helper(sonic->ports[1]->app, "rpt_helper.tmp", args->ports[0].pkt_cnt);
    } else if (args->mode >= MODE_PKT_VRPT0 &&
            args->mode <= MODE_PKT_VRPT_FULL ||
            args->mode >= MODE_PKT_VRPT_CAP &&
            args->mode <= MODE_PKT_CAP_VRPT) {
        sonic_rpt_helper(sonic->ports[0]->app, "vrpt_helper.tmp", args->ports[0].pkt_cnt);
        sonic_rpt_helper(sonic->ports[1]->app, "vrpt_helper.tmp", args->ports[0].pkt_cnt);
    }
#endif /* SONIC_KERNEL */
    
    sonic_set_runtime_threads(sonic);

    ret = sonic_run_sonic(sonic);

    __sonic_release(sonic);

    return ret;
}

#if SONIC_KERNEL
// entrance from kernel/proc
int sonic_run(struct sonic_priv *sonic, char *buf)
{
    SONIC_DDPRINT("\n");

    sonic_get_config_runtime_args(&sonic->runtime_args, buf);
#if SONIC_DEBUG
    sonic_print_config_runtime_args(&sonic->runtime_args);
#endif

    __sonic_run(sonic);

    return 0;
}
#endif /* SONIC_KERNEL */
