#include "icmp.h"
#include "ip.h"
#include "rtable.h"
#include "arp.h"
#include "base.h"
#include "log.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// send icmp packet
void icmp_send_packet(iface_info_t *iface, char *in_pkt, int len, u8 type, u8 code)
{
	// fprintf(stderr, "TODO: malloc and send icmp packet.\n");
	#ifdef MYDEBUG
		fprintf(stderr, "send icmp.\n");
	#endif
	struct ether_header *eh = (struct ether_header *)in_pkt;
	struct iphdr *ihr = packet_to_ip_hdr(in_pkt);
	int pkt_len = type==ICMP_ECHOREPLY ? len + IP_BASE_HDR_SIZE - IP_HDR_SIZE(ihr)
									   : ETHER_HDR_SIZE + IP_BASE_HDR_SIZE + ICMP_HDR_SIZE + IP_HDR_SIZE(ihr) + 8;
	char *packet = malloc(pkt_len);

	struct ether_header *n_eh = (struct ether_header *)packet;
	memcpy(n_eh->ether_dhost,eh->ether_shost,ETH_ALEN);
	memcpy(n_eh->ether_shost,iface->mac,ETH_ALEN);
	n_eh->ether_type = htons(ETH_P_IP);

	struct icmphdr *n_ichr = (struct icmphdr*)(packet + ETHER_HDR_SIZE + IP_BASE_HDR_SIZE);
	n_ichr->code = code;
	n_ichr->type = type;
	if(type == ICMP_ECHOREPLY) {
		int start = ETHER_HDR_SIZE+IP_HDR_SIZE(ihr)+4; // 4 is offsetof u of icmphdr
		memcpy(&n_ichr->u, in_pkt+start, pkt_len-start);
	}
	else {
		memset(&n_ichr->u,0,sizeof(n_ichr->u));
		memcpy((char*)n_ichr + ICMP_HDR_SIZE, in_pkt+ETHER_HDR_SIZE, IP_HDR_SIZE(ihr)+8);
	}
	n_ichr->checksum = icmp_checksum(n_ichr,pkt_len-(ETHER_HDR_SIZE + IP_BASE_HDR_SIZE));

	struct iphdr *n_ihr = packet_to_ip_hdr(packet);
	ip_init_hdr(n_ihr,iface->ip,ntohl(ihr->saddr),pkt_len-ETHER_HDR_SIZE,IPPROTO_ICMP);

	iface_send_packet(iface, packet, pkt_len);
}
