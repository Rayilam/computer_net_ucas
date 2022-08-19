#include "ip.h"
#include "icmp.h"
#include "log.h"

#include <stdio.h>
#include <stdlib.h>

// handle ip packet
//
// If the packet is ICMP echo request and the destination IP address is equal to
// the IP address of the iface, send ICMP echo reply; otherwise, forward the
// packet.
void handle_ip_packet(iface_info_t *iface, char *packet, int len)
{
	// fprintf(stderr, "TODO: handle ip packet.\n");
	struct iphdr *iph = packet_to_ip_hdr(packet);
	if(ntohl(iph->daddr) == iface->ip){
		if(iph->protocol == IPPROTO_ICMP) {
			struct icmphdr *icph = (struct icmphdr*)(IP_DATA(iph));
			if(icph->type == ICMP_ECHOREQUEST)
				icmp_send_packet(iface,packet,len,ICMP_ECHOREPLY,0);
		}
		free(packet);
	}
	else {
		#ifdef MYDEBUG
		fprintf(stderr, "forward.\n");
		#endif
		ip_forward_packet(iface, packet, len);
	}
}
