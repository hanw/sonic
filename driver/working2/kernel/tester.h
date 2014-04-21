#ifndef __SONIC_TESTER__
#define __SONIC_TESTER__

#if !SONIC_KERNEL
#include <stdint.h>
#else
#include <linux/types.h>
#endif

#if !SONIC_KERNEL
#define	SONIC_OUTPUT(msg, args...) do {\
	SONIC_PRINT("thru= %f " msg, 	\
            (double)_tbits / _time, ##args);	\
	} while(0)
#else /* SONIC_KERNEL */
#define	SONIC_OUTPUT(msg, args...) do {\
	SONIC_PRINT("_time= %llu _tbits= %llu " msg, _time, _tbits, ##args);	\
	} while(0)
#endif /* SONIC_KERNEL */

#define SONIC_CPU(args, path, port_id, x)	((args-> path ## _socket[port_id]) * 6 + args->x ## _cpu[port_id])
#define SONIC_SOCKET(args, path, port_id)	((args-> path ## _socket[port_id]) * 6)

struct sonic_priv;
struct sonic_pcs_block;

/* tester.c */
void sonic_init_tester_args(struct tester_args*);
#if SONIC_KERNEL
int sonic_test_starter(struct sonic_priv*, char *);
int sonic_get_option(struct tester_args *, char *);
#else /* SONIC_KERNEL */
int sonic_tester(struct sonic_priv *);
int sonic_get_option(struct tester_args *, char **);
#endif /* SONIC_KERNEL */

/* tester_helper.c */
int sonic_encode_helper(uint64_t *, uint8_t *, int , int , unsigned , int , 
		struct sonic_pcs_block *, int );
int sonic_gearbox_helper(uint64_t *, int, struct sonic_pcs_block *, int);
#if !SONIC_KERNEL
struct sonic_app;
int sonic_ins_helper(struct sonic_app *, char *);
#endif

#define sonic_set_port_info_id(info, args, port_id)	\
	sonic_set_port_info (info, args->mac_src[port_id], args->mac_dst[port_id],\
		args->ip_src[port_id], args->ip_dst[port_id], \
		args->port_src[port_id], args->port_dst[port_id])
		

#endif /* __SONIC_TESTER__ */
