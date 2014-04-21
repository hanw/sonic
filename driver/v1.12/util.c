#if SONIC_KERNEL
#include <linux/types.h>
#include <linux/time.h>
#include <linux/slab.h>
#include <linux/crc32.h>	// ether_crc
#include <linux/if_ether.h>	//struct ethhdr
#else	/* SONIC_KERNEL */
#include <arpa/inet.h>
#include <net/ethernet.h>
#include <linux/socket.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>
#endif /* SONIC_KERNEL */

#include "sonic.h"
#include "fifo.h"

#define SEC2NANO	1000000000L
#define ISDECIMAL(c)	(c>=0x30 && c <= 0x39)
#define ISHEX(c)	((ISDECIMAL(c)) || (c>=0x41 && c<=0x46) || \
				(c >=0x61 && c <= 0x66))

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
	struct ethhdr *eth = (struct ethhdr *) (data + PREAMBLE_LEN);	// preamble
	struct iphdr *ip = (struct iphdr *) (((uint8_t *) eth) + ETH_HLEN);
	struct udphdr *udp = (struct udphdr *) (((uint8_t *) ip) + IP_HLEN);
//	uint8_t * payload = ((uint8_t *) udp) + UDP_HLEN;

	unsigned char * mac_src = eth->h_source;
	unsigned char * mac_dst = eth->h_dest;
	SONIC_PRINT("----Frame length = %u----\n", len);

	SONIC_PRINT("-eth_src = %.2x%.2x%.2x%.2x%.2x%.2x\n", 
			mac_src[0], mac_src[1], mac_src[2], 
			mac_src[3], mac_src[4], mac_src[5]);
	SONIC_PRINT("-eth_dst = %.2x%.2x%.2x%.2x%.2x%.2x\n", 
			mac_dst[0], mac_dst[1], mac_dst[2], 
			mac_dst[3], mac_dst[4], mac_dst[5]);

	SONIC_PRINT("-ip_src = %x--ip_dst = %x\n", ntohl(ip->saddr), 
			ntohl(ip->daddr));
	SONIC_PRINT("-port_src = %d--port_dst = %d\n", ntohs(udp->source),
			ntohs(udp->dest));

	sonic_print_hex (data, len, 32);
}

int sonic_str_to_mac( char * mac, char * ret)
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

static uint16_t udp_csum(struct udphdr *uh, struct iphdr *iph)
{
        uint16_t len = ntohs(uh->len);
        unsigned char *cbuf = (void *) uh;
        uint64_t sum = 0;

        /* the pseudoheader */
        sum += ntohl(iph->saddr);
        sum += ntohl(iph->daddr);
        sum += (IPPROTO_UDP << 16) | len;

        while (len > 1)
        {
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


static inline void sonic_fill_udp(struct sonic_udp_port_info *info, uint8_t *data, int len,struct iphdr *iph)
{
	struct udphdr *udp = (struct udphdr *) data;
	
	udp->source = htons(info->port_src);
	udp->dest = htons(info->port_dst);
//	udp->dest = htons(info->port_dst + csum_debug++);
	udp->len = htons(len-4);
		
	sonic_fill_udp_payload(data + UDP_HLEN , len - UDP_HLEN);
	udp->check = 0;
	udp->check = htons(udp_csum(udp,iph));
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

static inline void sonic_fill_ip(struct sonic_udp_port_info *info, uint8_t *data, int len)
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

static inline void sonic_fill_eth(struct sonic_udp_port_info *info, uint8_t *data, int len)
{
	struct ethhdr *eth = (struct ethhdr *) data;

	//CHANGE PACKET TYPE
	eth->h_proto = htons(ETH_P_IP);
	//eth->h_proto = htons(len-18);
	memcpy(eth->h_source, info->mac_src, ETH_ALEN);
	memcpy(eth->h_dest, info->mac_dst, ETH_ALEN);

	sonic_fill_ip(info, data + ETH_HLEN, len - ETH_HLEN);

	// TODO crc
}

void sonic_fill_packet(uint8_t *data, int len)
{
	sonic_fill_udp_payload(data, len);
}

/* the address of data must be aligned to 16k + 8 */
void sonic_fill_frame(struct sonic_udp_port_info *info, uint8_t *data, int len)
{
	/* preemble + SFD */
	*(uint64_t *) data = 0xd555555555555500UL;

	sonic_fill_eth(info, data + 8, len);

#if SONIC_DDEBUG
	sonic_print_eth_frame(data, len + 8);
#endif
}

int sonic_fill_buf_batching (struct sonic_udp_port_info *info, uint8_t *buf, 
		int bufsize, int batching, int pkt_len, int idle)
{
	int i=0, remaining;
	uint8_t *p;
	struct sonic_mac_fifo_entry *packets;
	struct sonic_packet_entry *packet, *tpacket;

	packets = (struct sonic_mac_fifo_entry *) buf;

	if (batching == -1) 
		batching = bufsize / pkt_len + 1;

	SONIC_DDPRINT("batching = %d\n", batching);

	packet = packets->packets;
	p = packet->buf;
	remaining = bufsize - sizeof(struct sonic_mac_fifo_entry);

	for ( i = 0 ; i < batching; i ++ ) {
		if ( p + pkt_len + 8 > buf + bufsize) {
			i --;
			break;
		}

		sonic_fill_frame(info, p, pkt_len); 

		tpacket = (struct sonic_packet_entry *) (SONIC_ALIGN(uint8_t, 
				p + pkt_len + PREAMBLE_LEN + 8, 16) -8);

		remaining -= (uint8_t *) tpacket - (uint8_t *) packet;

//		packet->next = tpacket;
		packet->next = (uint8_t *) tpacket - (uint8_t *) packet;
		packet->idle = idle;
		packet->len = pkt_len + PREAMBLE_LEN;
		packet = tpacket;
		
		p = packet->buf;
	}
	packets->pkt_cnt = i;
//	SONIC_PRINT("####%d\n", i);

#if SONIC_DDEBUG
	sonic_print_hex(buf, bufsize, 32);
#endif

	return i;
}

static inline int sonic_update_dport_id(struct iphdr *iph, struct udphdr *uh, int id, 
		int num_queue, int port_base)
{
//	struct udphdr * uh = (struct udphdr *) (p + PREAMBLE_LEN + ETH_HLEN + IP_HLEN);
	uint32_t *pid = (uint32_t *) (((uint8_t *) uh) + UDP_HLEN);
	int xid=*pid;
	uint64_t tmp;
	short pdest = uh->dest, pcsum = uh->check;
	
	uh->dest = htons(port_base + id % num_queue);
	*pid = htonl(id);

	uh->check = 0;
//	ncsum = htons(udp_csum(uh, iph));
//	uh->check = htons(ncsum);

	tmp = (~pcsum &0xffff)+ (~pdest&0xffff )+ htons(port_base + id % num_queue)
			+ (~(xid >> 16) &0xffff)+ (~(xid & 0xffff)&0xffff) + htons(id>>16) + htons(id &0xffff);

	while (tmp>>16)
		tmp = (tmp & 0xffff) + (tmp >> 16);
	if (tmp == 0xffff)
		tmp = 0;

	uh->check = ~tmp;

	SONIC_DDPRINT("xid = %x id = %x, num_queue = %d, port_base = %d (%x)\n", xid, id, num_queue, port_base, port_base);
	SONIC_DDPRINT("%.4x %.4x (%.4x) %.4x \n", ntohs(pcsum), ntohs(pdest), ~ntohs(pdest), port_base + id%num_queue);
	SONIC_DDPRINT("pdest = %.4x, pid = %x, pcheck = %x, dest = %x, id = %x, check = %x, tmp = %x (%x)\n",
			pdest, xid, pcsum, uh->dest, id, uh->check,  tmp, ~tmp);

#if !SONIC_KERNEL
//	if ((ncsum &0xffff )!= (uh->check & 0xffff)) {
//		SONIC_ERROR("csum error %x %x %x %x\n", xid, id, ncsum, uh->check);
//		exit(1);
//	}
//	if (id == 0xfffff)
//	if (id == 0x3)
//		exit(1);
	
#endif

	return 0;
}

int sonic_update_csum_id(uint8_t *p, int id, int num_queue, int port_base)
{
    struct iphdr *iph;
    struct udphdr *uh;
    
    iph = (struct iphdr *) (p + PREAMBLE_LEN + ETH_HLEN);
    uh = (struct udphdr *) (((uint8_t *) iph) + IP_HLEN);
    sonic_update_dport_id(iph, uh, id, num_queue, port_base);

    return 0;
}

inline void sonic_set_pkt_id(uint8_t * pkt, int id)
{
	uint8_t * pid = pkt + PAYLOAD_OFFSET ;
	*(uint32_t *) pid = htonl(id);
//	* pid = id;
}

int sonic_update_buf_batching(uint8_t *buf, int bufsize, int id, int num_queue, int port_base)
{
	int i, batching ;
	uint8_t *p;
	uint32_t * pcrc, crc;
	struct sonic_mac_fifo_entry *packets;
	struct sonic_packet_entry *packet;
	struct iphdr *iph;
	struct udphdr *uh;

//	int x=0;

	packets = (struct sonic_mac_fifo_entry *) buf;

	packet = packets->packets;
	p = packet->buf;
	batching = packets->pkt_cnt;

	for ( i = 0 ; i < batching ; i ++ ) {
		SONIC_DDPRINT("%d %d %d\n", batching, i, packet->len);
		pcrc = (uint32_t *) CRC_OFFSET(p, packet->len);
		*pcrc = 0;
		
		iph = (struct iphdr *) (p + PREAMBLE_LEN + ETH_HLEN);
		uh = (struct udphdr *) (((uint8_t *) iph) + IP_HLEN);

		sonic_update_dport_id(iph, uh, id +i, num_queue, port_base);

		//CHANGE SET PACKET ID
		//sonic_set_pkt_id(p + PREAMBLE_LEN, (id + i));

		crc = fast_crc(p + PREAMBLE_LEN, packet->len - PREAMBLE_LEN) ^0xffffffff;
//		crc = fast_crc_nobr(p + PREAMBLE_LEN, packet->len - PREAMBLE_LEN - 4) ^ 0xffffffff;
//		crc = crc32_le(0xffffffff, p + PREAMBLE_LEN, packet->len - PREAMBLE_LEN - 4) ^ 0xffffffff;
		
		*pcrc = crc;

//		uh->check = htons(udp_csum(uh, iph));

		
		
		

		packet = (struct sonic_packet_entry *) (((uint8_t *) packet)+packet->next);
		p = packet->buf;
	}


#if SONIC_DDEBUG
	sonic_print_hex(buf, bufsize, 32);
#endif

	return id + i;
}

int sonic_update_buf_batching2(uint8_t *buf, int bufsize, int id, struct sonic_pkt_generator *gen, int *offset)
{
	int i, batching, j = *offset ;
	uint8_t *p;
	uint32_t * pcrc, crc;
	struct sonic_mac_fifo_entry *packets;
	struct sonic_packet_entry *packet;
	struct iphdr *iph;
	struct udphdr *uh;

	packets = (struct sonic_mac_fifo_entry *) buf;

	packet = packets->packets;
	p = packet->buf;
	packets->pkt_cnt = gen->buf_batching;
	batching = packets->pkt_cnt;

	packet->len = gen->pkt_len + 8;

//	SONIC_DPRINT("%d %d %d\n", j, gen->idle, gen->idle_chain);
	if (gen->chain_num == -1) {
		packets->pkt_cnt = 1;
		packet->len = 0;
		packet->idle = power_of_two(4) * 496;		// FIXME
		return id;
	}

	for ( i = 0 ; i < batching ; i ++ ) {
		SONIC_DDPRINT("%d %d %d\n", batching, i, packet->len);

		//CHANGE SET PACKET ID
		//sonic_set_pkt_id(p + PREAMBLE_LEN, (id + i));
//		if (gen->chain_num >=0) {

			if (unlikely(!(j %  gen->pkt_num))) {
				packet->idle = gen->idle_chain;
				j = 0;
				gen->chain_num--;
				if (gen->chain_num == -1) {
					packets->pkt_cnt = i;
					packet->len = 0;
					break;
				}
			}
			else {
				packet->idle = gen->idle;
			}

			// FIXME UDP csum?
			iph = (struct iphdr *) (p + PREAMBLE_LEN + ETH_HLEN);
			uh = (struct udphdr *) (((uint8_t *) iph) + IP_HLEN);

			sonic_update_dport_id(iph, uh, id +i, gen->multi_queue, gen->port_base);

			pcrc = (uint32_t *) CRC_OFFSET(p, packet->len);
			*pcrc = 0;
			crc = fast_crc(p + PREAMBLE_LEN, packet->len - PREAMBLE_LEN) ^0xffffffff;
			*pcrc = crc;

			j++;
//		} 
//		else {
//			packet->idle = gen->idle_chain;
//			packet->len = 0;
//		}

//		packet = packet->next;
		packet = (struct sonic_packet_entry *) (((uint8_t *) packet)+packet->next);
		p = packet->buf;

	}

#if SONIC_DDEBUG
	sonic_print_hex(buf, bufsize, 32);
#endif

	*offset = j;
	return id + i;
}

static inline int retrieve_bit(uint8_t * msg, int msg_len, int id)
{
	int offset;
	int byte_offset;
	int bit_offset;
	int bit;

	if (id==0)
		return 0;

	offset = (id-1) % (msg_len * 8);
	byte_offset = offset >> 3;
	bit_offset = offset & 0x7;
	bit = (msg[byte_offset] >> bit_offset) & 0x1;

	SONIC_DDPRINT("id = %d offset = %d byte_offset = %d bit_offset = %d, bit = %d\n",
		id, offset, byte_offset, bit_offset, bit);

	return bit;
}

int sonic_update_buf_batching_covert(uint8_t *buf, int bufsize, int id, struct sonic_pkt_generator *gen)
{
	int i, batching;
	uint8_t *p;
	uint32_t * pcrc, crc;
	struct sonic_mac_fifo_entry *packets;
	struct sonic_packet_entry *packet;
	struct iphdr *iph;
	struct udphdr *uh;
	int covert_bit;

	packets = (struct sonic_mac_fifo_entry *) buf;

	packet = packets->packets;
	p = packet->buf;
	packets->pkt_cnt = gen->buf_batching;
	batching = packets->pkt_cnt;

	packet->len = gen->pkt_len + 8;

//	SONIC_DPRINT("%d %d %d\n", j, gen->idle, gen->idle_chain);
	if (id == gen->pkt_num) {
		packets->pkt_cnt = 1;
		packet->len = 0;
		packet->idle = power_of_two(4) * 496;		// FIXME
		return id;
	}

	for ( i = 0 ; i < batching ; i ++ ) {
		SONIC_DDPRINT("%d %d %d\n", batching, i, packet->len);

		//CHANGE SET PACKET ID
		//sonic_set_pkt_id(p + PREAMBLE_LEN, (id + i));
//		if (gen->chain_num >=0) {
			if (id+i == gen->pkt_num) {
				packets->pkt_cnt = i;
				packet->len = 0;
				packet->idle = gen->idle;
				break;
			}

			covert_bit = retrieve_bit(gen->msg, gen->msg_len, id+i);
			
			// FIXME: Overflow?
			if(id+i == 0) 
				packet->idle = 0;
			else if (covert_bit)
//				packet->idle = gen->idle;
				packet->idle = ((gen->idle - gen->delta) < 12) ? 12 : (gen->idle - gen->delta);
			else
				packet->idle = gen->idle + gen->delta;

			// FIXME UDP csum?
			iph = (struct iphdr *) (p + PREAMBLE_LEN + ETH_HLEN);
			uh = (struct udphdr *) (((uint8_t *) iph) + IP_HLEN);

			sonic_update_dport_id(iph, uh, id +i, gen->multi_queue, gen->port_base);

			pcrc = (uint32_t *) CRC_OFFSET(p, packet->len);
			*pcrc = 0;
			crc = fast_crc(p + PREAMBLE_LEN, packet->len - PREAMBLE_LEN) ^0xffffffff;
			*pcrc = crc;

//			id++;
//		} 
//		else {
//			packet->idle = gen->idle_chain;
//			packet->len = 0;
//		}

//		packet = packet->next;
		packet = (struct sonic_packet_entry *) (((uint8_t *) packet)+packet->next);
		p = packet->buf;

	}

#if SONIC_DDEBUG
	sonic_print_hex(buf, bufsize, 32);
#endif

	return id +i;
}

void sonic_print_port_info(struct sonic_udp_port_info *info)
{
	SONIC_PRINT("-eth_src = %.2x%.2x%.2x%.2x%.2x%.2x\n", 
		info->mac_src[0], info->mac_src[1], info->mac_src[2], 
		info->mac_src[3], info->mac_src[4], info->mac_src[5]);
	SONIC_PRINT("-eth_dst = %.2x%.2x%.2x%.2x%.2x%.2x\n", 
		info->mac_dst[0], info->mac_dst[1], info->mac_dst[2], 
		info->mac_dst[3], info->mac_dst[4], info->mac_dst[5]);

	SONIC_PRINT("-ip_src = %x --ip_dst = %x\n", ntohl(info->ip_src), 
		ntohl(info->ip_dst));
	SONIC_PRINT("-port_src = %d --port_dst = %d\n", info->port_src,
		info->port_dst);
}

int sonic_set_port_info(struct sonic_udp_port_info *info,
		char * mac_src, char * mac_dst, char * ip_src,
		char * ip_dst, int port_src, int port_dst)
{
	struct sonic_udp_port_info tmp;
	if(sonic_str_to_mac(mac_src, tmp.mac_src)) {
		SONIC_ERROR("Invalid src MAC\n");
		return -1;
	}
	if(sonic_str_to_mac(mac_dst, tmp.mac_dst)) {
		SONIC_ERROR("Invalid dst MAC\n");
		return -1;
	}
#if SONIC_KERNEL
	tmp.ip_src = in_aton(ip_src);
	tmp.ip_dst = in_aton(ip_dst);
#else /*SONIC_KERNEL*/
	tmp.ip_src = inet_addr(ip_src);
	tmp.ip_dst = inet_addr(ip_dst);
#endif /*SONIC_KERNEL*/
	tmp.port_src = port_src;
	tmp.port_dst = port_dst;

	memcpy(info, &tmp, sizeof(struct sonic_udp_port_info));
#if SONIC_DDEBUG
	sonic_print_port_info(info);
#endif
	return 0;
}

void print_page(struct sonic_dma_page *page)
{
	int i;
	uint32_t cur_sh;
	uint8_t sh;
	uint64_t payload;
	SONIC_PRINT("size of page = %lu %p %d\n", sizeof(struct sonic_dma_page), page, page->reserved);

	cur_sh = page->syncheaders[0];
	for ( i = 0 ; i < NUM_BLOCKS_IN_PAGE ; i ++) {
		if ( i % 16 == 0) 
			cur_sh = page->syncheaders[i/16];

		sh = (cur_sh >> ((i % 16) * 2)) & 0x3;
		payload = page->payloads[i];

		SONIC_PRINT("block %d = %x, %.16llx\n", i, sh, (unsigned long long)payload);
	}
}


