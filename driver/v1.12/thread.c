#if SONIC_KERNEL
#include <linux/init.h>
#include <linux/sched.h>
#else /* SONIC_KERNEL */
#define _GNU_SOURCE
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sched.h>
#include <unistd.h>
#include <sys/syscall.h>
#endif /* SONIC_KERNEL */
#include "sonic.h"
#include "fifo.h"
#include "covert.h"
#include "mode.h"

extern unsigned long long int descrambler_debug;
extern unsigned long long int scrambler_debug;

int sonic_set_port_args(struct sonic_priv *sonic);

#if !SONIC_KERNEL 
int sonic_get_cpu()
{
	int i;
	pid_t tid;
	cpu_set_t cpu_set;
	CPU_ZERO(&cpu_set);

	tid = syscall(SYS_gettid);

	if (sched_getaffinity(tid, sizeof(cpu_set), &cpu_set) < 0) {
		SONIC_ERROR("get_cpu error\n");
		return -1;
	}

	for ( i = 0 ; i < CPU_SETSIZE ; i ++ ) {
		if (CPU_ISSET(i, &cpu_set)) {
			SONIC_DDPRINT("this thread %d is running on cpu #%d\n", tid, i);
			break;
		}
	}

	return i;
}

int sonic_set_cpu(int cpun)
{
	pid_t tid;
	cpu_set_t cpu_set;
	CPU_ZERO(&cpu_set);
	CPU_SET(cpun, &cpu_set);

	SONIC_DDPRINT("cpun = %d\n");

	tid = syscall(SYS_gettid);
	if(sched_setaffinity(tid, sizeof(cpu_set), &cpu_set) < 0) {
		SONIC_ERROR("set_cpu error\n");
		return -1;
	}

	return 0;
}
#else /* SONIC_KERNEL */
int sonic_get_cpu()
{
	return task_cpu(current);
}

int sonic_set_cpu(int cpun)
{
	set_cpus_allowed(current, cpumask_of_cpu(cpun));
	return 0;
}
#endif /* SONIC_KERNEL */

#if SONIC_KERNEL
static int sonic_run_thread(void *args)
#else
static void * sonic_run_thread(void *args)
#endif
{
	struct sonic_thread *thread = (struct sonic_thread *) args;
	SONIC_DDPRINT("\n");
#if SONIC_KERNEL
	daemonize("sonic");
	atomic_inc(&thread->port->threads);
#endif

	sonic_set_cpu(thread->cpu);

	SONIC_DDPRINT("Thread cpu %d is running on %d\n",
		thread->cpu, sonic_get_cpu());
	
	thread->func(thread);

#if SONIC_KERNEL
	atomic_dec(&thread->port->threads);
#endif

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

inline void sonic_reset_pcs(struct sonic_pcs *pcs)
{
	if(pcs->fifo)  
		reset_fifo(pcs->fifo);

	pcs->dma_idx = 0;
	pcs->dic = 0;
	pcs->debug =0 ;

#if SONIC_PCIE
	pcs->dt->w3 = pcs->dma_num_desc - 1;
#endif
}

inline void sonic_reset_mac(struct sonic_mac *mac)
{
	if(mac->fifo)
		reset_fifo(mac->fifo);
}

inline void sonic_reset_logger(struct sonic_logger *logger)
{
	if (logger->fifo)
		reset_fifo(logger->fifo);
#if SONIC_KERNEL
	spin_lock_bh(&logger->lock);
#endif
	sonic_free_logs(logger);        //TODO
#if SONIC_KERNEL
	spin_unlock_bh(&logger->lock);
#endif /* SONIC_KERNEL */
}

inline int sonic_reset_port(struct sonic_port *port)
{
#if SONIC_SFP
	int ret;
	ret = sonic_fpga_reset(port);
	if (ret == -1) 
		return -1;
#endif
	SONIC_DDPRINT("\n");

#define SONIC_THREADS(x,y,z)		\
	if (port->x)			\
		sonic_reset_##y (port->x);
	SONIC_THREADS_ARGS
#undef SONIC_THREADS
#if SONIC_KERNEL
	atomic_set(&port->threads, 0);
#endif /* SONIC_THREADS */

	sonic_reset_port_stat(port);

	port->stopper = 0;

	return 0;
}

int sonic_reset_sonic(struct sonic_priv *sonic)
{
	int i;

	for ( i = 0 ; i < SONIC_PORT_SIZE ; i ++ ) 
		sonic_reset_port(sonic->ports[i]);

	return 0;
}

int sonic_start_port(struct sonic_port *port, struct sonic_port_args *args)
{
	unsigned long int ret;

	if (sonic_reset_port(port) == -1) 
		return -1;	

#if !SONIC_KERNEL
    if (port->port_id == 0 && (port->sonic->tester_args.mode == MODE_PKT_RPT 
                || port->sonic->tester_args.mode == MODE_PKT_CRC_RPT
                || port->sonic->tester_args.mode == MODE_PKT_RPT_CAP))
//        sonic_ins_helper(port->log, "ins_1518.tmp");
        sonic_ins_helper(port->log, "res/multi/6500/1518_170_1.tmp.info");
#endif
	
	// FIXME: if create_thread returns error
#define SONIC_THREADS(x, y, z)	\
	if (args-> x ) {	\
		SONIC_DDPRINT("%s\n", #x);	\
		port-> x ->func = args-> x ;	\
		ret = sonic_create_thread(port-> x ); \
		SONIC_DDPRINT("pthread_t %u created\n", ret);\
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
	SONIC_DDPRINT("START\n");
	if (sonic_set_port_args(sonic))
		return -1;

	if (sonic->port_args[0] && sonic_start_port(sonic->ports[0], sonic->port_args[0]) == -1)
			return -1;
	if (sonic->port_args[1] && sonic_start_port(sonic->ports[1], sonic->port_args[1]) == -1) 
			return -1;

	return 0;
}

inline void sonic_stop_port(struct sonic_port *port)
{
	SONIC_DDPRINT("%p\n", &port->stopper);
	port->stopper = 1;
}

#if !SONIC_KERNEL
int sonic_wait_port(struct sonic_port *port)
{
#define SONIC_THREADS(x, y, z)	\
	pthread_join(port->pid_##x, NULL);
	SONIC_THREADS_ARGS
#undef SONIC_THREADS
	return 0;
}
#else
int sonic_wait_port(struct sonic_port *port)
{
	int ret =0;
	while(1) {
//		__set_current_state(TASK_INTERRUPTIBLE);
		if (atomic_read(&port->threads) == 0)
			break;

		sonic_stop_port(port);
		// FIXME
		schedule();
	}
	__set_current_state(TASK_RUNNING);

#if SONIC_SFP
	if (sonic_fpga_stop_sfp(port) == -1)
		ret=-1;
#endif
	return ret;
}
#endif

int sonic_wait_sonic(struct sonic_priv *sonic)
{
	SONIC_DDPRINT("wait\n");

	if (sonic->port_args[0] && sonic_wait_port(sonic->ports[0]) == -1)
			return -1;
	if (sonic->port_args[1] && sonic_wait_port(sonic->ports[1]) == -1)
			return -1;

	return 0;
}

void sonic_stop_sonic(struct sonic_priv *sonic)
{
	SONIC_DDPRINT("stop\n");

	if (sonic->port_args[0])
		sonic_stop_port(sonic->ports[0]);

	if (sonic->port_args[1])
		sonic_stop_port(sonic->ports[1]);

	sonic_wait_sonic(sonic);

	sonic->port_args[0] = NULL;
	sonic->port_args[1] = NULL;
}

int sonic_run_port(struct sonic_port *port, struct sonic_port_args *args, int duration)
{
	SONIC_DDPRINT("\n");

	SONIC_DDPRINT("START port\n");

	if (sonic_start_port(port, args) == -1)
		return -1;

	SLEEP(duration);

	SONIC_DDPRINT("STOP port\n");

	sonic_stop_port(port);

	if (sonic_wait_port(port) == -1)
		return -1;

	SONIC_DDPRINT("DONE\n");

	return 0;
}

int sonic_run_sonic(struct sonic_priv *sonic, int duration)
{
	SONIC_DPRINT("\n");

	if (sonic_start_sonic(sonic) == -1)
		return -1;

	SLEEP(duration);

	sonic_stop_sonic(sonic);

	SONIC_DDPRINT("DONE\n");

#if SONIC_KERNEL
    spin_lock_bh(&sonic->lock);
    sonic->status = 0;
    spin_unlock_bh(&sonic->lock);
#endif

	return 0;
}
