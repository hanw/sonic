#if SONIC_KERNEL
#include <linux/crc32.h>        // ether_crc
#include <linux/if_ether.h>     //struct ethhdr
#include <linux/slab.h>
#include <linux/types.h>
#include <linux/time.h>
#include <linux/version.h>
#else /* !SONIC_KERNEL */
#define _GNU_SOURCE
#include <arpa/inet.h>
#include <linux/socket.h>
#include <net/ethernet.h>
#include <sched.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>
#endif /* SONIC_KERNEL */
#include "sonic.h"

#define SEC2NANO        1000000000L
#define ISDECIMAL(c)    (c>=0x30 && c <= 0x39)

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
#if LINUX_VERSION_CODE <= KERNEL_VERSION(2,6,32)
    struct cpumask cpumask = cpumask_of_cpu(cpun);
    set_cpus_allowed_ptr(current, &cpumask);
#else
    set_cpus_allowed(current, cpumask_of_cpu(cpun));
#endif
    return 0;
}
#endif /* SONIC_KERNEL */

int sonic_random()
{
#if SONIC_KERNEL
    int i;
    get_random_bytes(&i, sizeof(i));
    return i;
#else /* SONIC_KERNEL */
    unsigned int seed = (unsigned int)time(NULL);
    srand(seed);
    return rand();
#endif /* SONIC_KERNEL */
}

inline uint64_t power_of_two(int n)
{
    return ( 1<< n);
}

inline uint64_t order2bytes(int order)
{
    return power_of_two(order) * PAGE_SIZE;
}

inline uint64_t timediff2u64 (struct timespec *t2, struct timespec *t1)
{
    uint64_t l = 0;
    l = (t1->tv_sec - t2->tv_sec) * SEC2NANO + t1->tv_nsec - t2->tv_nsec;
    return l;
}

char * print_binary_64(uint64_t x)
{
    int i;
    static char buf[128];
    char *p = buf;
    for ( i = 0 ; i < 64 ; i ++ ) {
        *p++ = (x >> (63-i)) &0x1	? '1' : '0';
        if ( i % 8 == 7)
            *p++ = ' ';
    }
    *p = '\0';

    return buf;
}

void sonic_print_hex(uint8_t * str, int len, int b)
{
    int i =0, p=0;
    char *buf;

    if (b < 16)
        b = 16;

    buf= ALLOC( ((len / 5) + 1) * (40 + 3 * b));

    SONIC_PRINT("starting address = %p\n", str);
    p += sprintf(buf, "%p byte %.4d~%.4d  : ", str, 1, 1+b);

    for ( ; i < len ;i ++) {
        p += sprintf(buf + p, "%.2x", str[i]);

        if (i %4  == 3)
            p += sprintf(buf + p, " ");

        if (i%b == b-1) {
            SONIC_PRINT("%s\n", buf);	
            p = 0;
            if ( i + 1 <= len)
            p += sprintf(buf, "%p byte %.4d~%.4d  : ", str +i +1, i+2, 
                len > i+2+b ? i+2+b : len);
        }
    }
    SONIC_PRINT("%s\n", buf);
    SONIC_PRINT("\n");
    FREE(buf);
}

void sonic_print_eth_frame(uint8_t * data, int len)
{
#if SONIC_DDEBUG
    struct ethhdr *eth = (struct ethhdr *) (data + PREAMBLE_LEN);	// preamble
    struct iphdr *ip = (struct iphdr *) (((uint8_t *) eth) + ETH_HLEN);
    struct udphdr *udp = (struct udphdr *) (((uint8_t *) ip) + IP_HLEN);

    unsigned char * mac_src = eth->h_source;
    unsigned char * mac_dst = eth->h_dest;
#endif /*SONIC_DDEBUG*/
    SONIC_DDPRINT("----Frame length = %u----\n", len);

    SONIC_DDPRINT("-eth_src = %.2x%.2x%.2x%.2x%.2x%.2x\n", 
            mac_src[0], mac_src[1], mac_src[2], 
            mac_src[3], mac_src[4], mac_src[5]);
    SONIC_DDPRINT("-eth_dst = %.2x%.2x%.2x%.2x%.2x%.2x\n", 
            mac_dst[0], mac_dst[1], mac_dst[2], 
            mac_dst[3], mac_dst[4], mac_dst[5]);

    SONIC_DDPRINT("-ip_src = %x--ip_dst = %x\n", ntohl(ip->saddr), 
            ntohl(ip->daddr));
    SONIC_DDPRINT("-port_src = %d--port_dst = %d\n", ntohs(udp->source),
            ntohs(udp->dest));

    sonic_print_hex (data, len, 32);
}

#define ISHEX(c)        ((ISDECIMAL(c)) || (c>=0x41 && c<=0x46) || \
                        (c >=0x61 && c <= 0x66))
int sonic_str_to_mac( char * mac, unsigned char * ret)
{
    int i = 0 , n = 0 , len = strlen((char *)mac);

    if (len != 12 && len != 17)
        return -1;

    for ( n = 0 ; n  < 6 ; n ++) {
        char h;
        char tmp[3];

        if( !ISHEX(mac[i]) || !ISHEX(mac[i+1]))
            return -1;

        memset(tmp, 0, 3);
        memcpy(tmp, mac+i, 2);

        sscanf(tmp, "%hhx", &h);
        h &= 0xff;

        ret[n] = h;
        if ( n!= 5 && !ISHEX(mac[i+2]))
            i += 3;
        else 
            i += 2;
    }
    return 0;
}

#if !SONIC_KERNEL
static uint16_t csum(void *buf, int len)
{
    uint32_t sum = 0;
    unsigned char *cbuf = buf;

    while(len > 1)
    {
        sum += (cbuf[0] << 8) | cbuf[1];
        cbuf += 2;
        len -= 2;
    }
    if (len >0)
        sum += (cbuf[0] << 8);

    while(sum >> 16)
        sum = (sum & 0xffff) + (sum >> 16);

    return ((uint16_t) (~sum));
}

#endif /*!SONIC_KERNEL*/

uint16_t udp_csum(struct udphdr *uh, struct iphdr *iph)
{
    uint16_t len = ntohs(uh->len);
    unsigned char *cbuf = (void *) uh;
    uint64_t sum = 0;

    /* the pseudoheader */
    sum += ntohl(iph->saddr);
    sum += ntohl(iph->daddr);
    sum += (IPPROTO_UDP << 16) | len;

    while (len > 1) {
        sum += (cbuf[0] << 8) | cbuf[1];
        cbuf += 2;
        len -= 2;
    }
    if (len > 0) /* left-over byte, zero padded */
        sum += (cbuf[0] << 8);

    /* fold */
    while (sum >> 32)
        sum = (sum & 0xffffffff) + (sum >> 32);
    while (sum >> 16)
        sum = (sum & 0xffff) + (sum >> 16);
    sum = ((~sum) & 0xffff);
    if (sum == 0)
        sum = 0xffff;
    return (uint16_t) sum;
}

//static int csum_debug=0;

/* Adapted from Tudor's code */
static inline void sonic_fill_udp_payload(uint8_t *data, int len)
{
    unsigned char cycle[] = "ERLUOERLUO";
    int i, len_cycle = strlen((char *)cycle);

    *(uint32_t *) data = 0;

    for ( i = 4 ; i < len-4 ; i ++) {
        data[i] = cycle[i % len_cycle];
    }

    for ( ; i < len ; i ++) {
        data[i] = 0;
    }
}

static inline void sonic_fill_udp(struct sonic_port_info *info, uint8_t *data, int len,struct iphdr *iph)
{
    struct udphdr *udp = (struct udphdr *) data;

    udp->source = htons(info->port_src);
    udp->dest = htons(info->port_dst);
    //	udp->dest = htons(info->port_dst + csum_debug++);
    udp->len = htons(len-4);
        
    sonic_fill_udp_payload(data + UDP_HLEN , len - UDP_HLEN);
    udp->check = 0;
    udp->check = htons(udp_csum(udp,iph));
    // FIXME: WHY here?
#if SONIC_KERNEL
    iph->check = ip_fast_csum(iph, iph->ihl);	// ip checksum */
#else
    iph->check = csum(iph, iph->ihl*4);

#if 0
//	SONIC_DPRINT("network: dport = %x (%x), udp->check = %x (%x), csum_debug = %x (%x)\n",
//			ntohs(udp->dest), ~ntohs(udp->dest), ntohs(udp->check), ~ntohs(iph->check), 
//			csum_debug, htons(csum_debug));
	SONIC_DPRINT("host:    dport = %x (%x), udp->check = %x (%x), csum_debug = %x (%x)\n",
			udp->dest, ~udp->dest, udp->check, ~udp->check, 
			csum_debug, htons(csum_debug));

	uint64_t tmp, tmp2;

	tmp2 = (~udp->check) + htons(csum_debug);
	while(tmp2 >>16)
		tmp2 = (tmp2 &0xffff) + (tmp2 >> 16);
	
	SONIC_DPRINT("new csum = %x (%x)\n", ~tmp2, tmp2);

	csum_debug ++;

	if (csum_debug == 16)
		exit (1);
#endif

//	sonic_print_hex(data-2, len-4, 16);
//	exit(1);
#endif

}

static inline void sonic_fill_ip(struct sonic_port_info *info, uint8_t *data, int len)
{
    struct iphdr *ip = (struct iphdr *) data;
    ip->ihl = IP_HLEN / 4;
    ip->version = IPVERSION;
    ip->tos = 0;
    ip->tot_len = htons(len-4);
    ip->id = 0;
    ip->frag_off = 0x40;
    ip->ttl = 64;
    ip->check = 0;
    ip->protocol = IPPROTO_UDP;
    ip->saddr = info->ip_src;
    ip->daddr = info->ip_dst;
/*#if SONIC_KERNEL
	ip->check = ip_fast_csum(ip, ip->ihl);	// ip checksum 
#else
	ip->check = csum(ip, ip->ihl*4);
#endif
*/

	sonic_fill_udp(info, data + IP_HLEN , len - IP_HLEN,ip);
}

static inline void sonic_fill_eth(struct sonic_port_info *info, uint8_t *data, int len)
{

#if !SONIC_VLAN
    struct ethhdr *eth = (struct ethhdr *) data;
    //CHANGE PACKET TYPE
    eth->h_proto = htons(ETH_P_IP);
#else /* SONIC_VLAN */
    struct sonic_vlan_hdr *eth = (struct sonic_vlan_hdr *) data;
    eth->h_vlan_proto = htons(ETH_P_8021Q);

    eth->h_vlan_TCI=htons(info->vlan_id);
    eth->h_vlan_encapsulated_proto = htons(ETH_P_IP);
#endif /* SONIC_VLAN */

    memcpy(eth->h_source, info->mac_src, ETH_ALEN);
    memcpy(eth->h_dest, info->mac_dst, ETH_ALEN);

    sonic_fill_ip(info, data + ETH_HLEN + SONIC_VLAN_ADD, len - SONIC_VLAN_ADD - ETH_HLEN);
}

void sonic_fill_packet(uint8_t *data, int len)
{
    sonic_fill_udp_payload(data, len);
}

/* the address of data must be aligned to 16k + 8 */
void sonic_fill_frame(struct sonic_port_info *info, uint8_t *data, int len)
{
    /* preemble + SFD */
    *(uint64_t *) data = 0xd555555555555500UL;

    sonic_fill_eth(info, data + 8, len);

#if SONIC_DDEBUG
//    sonic_print_eth_frame(data, len + 8);
#endif
}

int sonic_fill_buf_batching(struct sonic_port_info *info, \
        struct sonic_packets *packets, int bufsize)
{
    int i=0, pkt_len = info->pkt_len, consumed;
    struct sonic_packet * packet = packets->packets, * next_packet;
    uint8_t * p; 

    consumed = sizeof(struct sonic_packets);

    while(1) {
        p = packet->buf;

        sonic_fill_frame(info, p, pkt_len);

        packet->idle = info->idle;
        packet->len = pkt_len + PREAMBLE_LEN;

        i ++;
        if (i == info->batching) {
            packet->next = 0;
            break;
        }

        next_packet = (struct sonic_packet *) 
                (SONIC_ALIGN(uint8_t, p + pkt_len + PREAMBLE_LEN + 8, 16) - 8);

        packet->next = (uint8_t *)next_packet - (uint8_t *) packet;

        consumed += packet->next;

        if (consumed + sizeof(struct sonic_packet) + pkt_len > bufsize) {
            packet->next = 0;
            break;
        }

        packet = next_packet;
    }

    packets->pkt_cnt = i;

#if SONIC_DDEBUG
//    sonic_print_hex((uint8_t*) packets, bufsize, 32);
#endif /* SONIC_DDEBUG */
    return i;
}

int sonic_prepare_pkt_gen_fifo(struct sonic_fifo *fifo, struct sonic_port_info *info)
{
    int i = 0, ret=0;
    struct sonic_packets *packets;

    for ( i = 0 ; i < fifo->size ; i ++ ) {
        packets = (struct sonic_packets *) fifo->entry[i].data;
        ret = sonic_fill_buf_batching(info, packets, fifo->entry_size);
    }

    return ret;
}

inline void sonic_set_pkt_id(uint8_t * pkt, int id)
{
	uint8_t * pid = pkt + PAYLOAD_OFFSET ;
	*(uint32_t *) pid = htonl(id);
//	* pid = id;
}

void print_dma_page(struct sonic_dma_page *page)
{
    int i;
    uint32_t cur_sh;
    uint8_t sh;
    uint64_t payload;
    SONIC_PRINT("size of page = %lu %p %d\n", sizeof(struct sonic_dma_page), 
            page, page->reserved);

    cur_sh = page->syncheaders[0];
    for ( i = 0 ; i < NUM_BLOCKS_IN_PAGE ; i ++) {
        if ( i % 16 == 0) 
            cur_sh = page->syncheaders[i/16];

        sh = (cur_sh >> ((i % 16) * 2)) & 0x3;
        payload = page->payloads[i];

        SONIC_PRINT("block %d = %x, %.16llx\n", i, sh, (unsigned long long)payload);
    }
}

int sonic_gen_idles(struct sonic_port *port, struct sonic_fifo *fifo, int nsec)
{
    const volatile int * const stopper = &port->stopper;
    uint64_t beginning_idles = 8 * 0x9502f90ULL; // # of idles per second
    uint64_t default_idle = power_of_two(fifo->exp) * 496; 
    struct sonic_packets *packets;
    struct sonic_packet *packet;

    beginning_idles *= nsec;

    while(beginning_idles >= default_idle) {
        packets = (struct sonic_packets*) get_write_entry(fifo);

        packets->pkt_cnt = 1;
        packet = packets->packets;
        packet->len = 0;
        packet->idle = default_idle;

        beginning_idles -= default_idle;

        put_write_entry(fifo, packets);
        
        if (*stopper != 0)
            return -1;     
    }

    return 0;
}
