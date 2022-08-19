#include "arp.h"
#include "base.h"
#include "types.h"
#include "ether.h"
#include "arpcache.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// #include "log.h"

// send an arp request: encapsulate an arp request packet, send it out through
// iface_send_packet
void arp_send_request(iface_info_t *iface, u32 dst_ip)
{
	//fprintf(stderr, "TODO: send arp request when lookup failed in arpcache.\n");
	//fprintf(stderr, "arp req ip is %d.%d.%d.%d\n",*(char*)&dst_ip,*((char*)&dst_ip+1),*((char*)&dst_ip+2),*((char*)&dst_ip+3));
	char *packet = (char *)malloc(ETHER_HDR_SIZE + sizeof(struct ether_arp));
	struct ether_header *eh = (struct ether_header *)packet;
	memcpy(eh->ether_shost, iface->mac, ETH_ALEN);
	memset(eh->ether_dhost,0xff,ETH_ALEN);
	eh->ether_type = htons(ETH_P_ARP);
	struct ether_arp *arp_pkt = packet_to_arp_hdr(packet);
	
	arp_pkt->arp_hrd = htons(ARPHRD_ETHER);
	arp_pkt->arp_pro = htons(ETH_P_IP);
	arp_pkt->arp_hln = (u8)ETH_ALEN;
	arp_pkt->arp_pln = (u8)4;
	arp_pkt->arp_op = htons(ARPOP_REQUEST);
	memcpy(arp_pkt->arp_sha, iface->mac, ETH_ALEN);
	arp_pkt->arp_spa = htonl(iface->ip);
	memset(arp_pkt->arp_tha,0,ETH_ALEN);
	arp_pkt->arp_tpa = htonl(dst_ip);
	iface_send_packet(iface, packet, ETHER_HDR_SIZE + sizeof(struct ether_arp));
}

// send an arp reply packet: encapsulate an arp reply packet, send it out
// through iface_send_packet
void arp_send_reply(iface_info_t *iface, struct ether_arp *req_hdr)
{
	// fprintf(stderr, "TODO: send arp reply when receiving arp request.\n");
	char *packet = (char *)malloc(ETHER_HDR_SIZE + sizeof(struct ether_arp));
	struct ether_header *eh = (struct ether_header *)packet;
	memcpy(eh->ether_shost, iface->mac, ETH_ALEN);
	memcpy(eh->ether_dhost, req_hdr->arp_sha, ETH_ALEN);
	eh->ether_type = htons(ETH_P_ARP);
	struct ether_arp *arp_pkt = packet_to_arp_hdr(packet);
	
	arp_pkt->arp_hrd = htons(ARPHRD_ETHER);
	arp_pkt->arp_pro = htons(ETH_P_IP);
	arp_pkt->arp_hln = (u8)ETH_ALEN;
	arp_pkt->arp_pln = (u8)4;
	arp_pkt->arp_op = htons(ARPOP_REPLY);
	memcpy(arp_pkt->arp_sha, iface->mac, ETH_ALEN);
	arp_pkt->arp_spa = htonl(iface->ip);
	memcpy(arp_pkt->arp_tha, req_hdr->arp_sha, ETH_ALEN);
	arp_pkt->arp_tpa = req_hdr->arp_spa;
	iface_send_packet(iface, packet, ETHER_HDR_SIZE + sizeof(struct ether_arp));
}

void handle_arp_packet(iface_info_t *iface, char *packet, int len)
{
	// fprintf(stderr, "TODO: process arp packet: arp request & arp reply.\n");
	struct ether_arp *arp_pkt = packet_to_arp_hdr(packet);
	//u32 dst_ip = ntohl(arp_pkt->arp_tpa);
	//fprintf(stderr, "arp req ip is %d.%d.%d.%d\n",*(char*)&dst_ip,*((char*)&dst_ip+1),*((char*)&dst_ip+2),*((char*)&dst_ip+3));
	
	if(ntohs(arp_pkt->arp_op) == ARPOP_REPLY){
		arpcache_insert(ntohl(arp_pkt->arp_spa), arp_pkt->arp_sha);
	}
	else if(ntohs(arp_pkt->arp_op) == ARPOP_REQUEST \
		 	&& ntohl(arp_pkt->arp_tpa) == iface->ip){
		arp_send_reply(iface, arp_pkt);
	}
	free(packet);
}

// send (IP) packet through arpcache lookup 
//
// Lookup the mac address of dst_ip in arpcache. If it is found, fill the
// ethernet header and emit the packet by iface_send_packet, otherwise, pending 
// this packet into arpcache, and send arp request.
void iface_send_packet_by_arp(iface_info_t *iface, u32 dst_ip, char *packet, int len)
{
	struct ether_header *eh = (struct ether_header *)packet;
	//memcpy(eh->ether_shost, iface->mac, ETH_ALEN);
	eh->ether_type = htons(ETH_P_IP);

	u8 dst_mac[ETH_ALEN];
	int found = arpcache_lookup(dst_ip, dst_mac);
	if (found) {
		// log(DEBUG, "found the mac of %x, send this packet", dst_ip);
		memcpy(eh->ether_dhost, dst_mac, ETH_ALEN);
		memcpy(eh->ether_shost, iface->mac, ETH_ALEN);
		iface_send_packet(iface, packet, len);
	}
	else {
		// log(DEBUG, "lookup %x failed, pend this packet", dst_ip);
		arpcache_append_packet(iface, dst_ip, packet, len);
	}
}
