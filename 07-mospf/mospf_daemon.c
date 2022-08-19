#include "mospf_daemon.h"
#include "mospf_proto.h"
#include "mospf_nbr.h"
#include "mospf_database.h"

#include "ip.h"

#include "list.h"
#include "log.h"
#include "rtable.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>

extern ustack_t *instance;

u8 mc_mac[ETH_ALEN] = {0x01, 0x00, 0x5e, 0x00, 0x00, 0x05};
pthread_mutex_t db_lock;

pthread_mutex_t mospf_lock;

void mospf_init()
{
	pthread_mutex_init(&mospf_lock, NULL);

	instance->area_id = 0;
	// get the ip address of the first interface
	iface_info_t *iface = list_entry(instance->iface_list.next, iface_info_t, list);
	instance->router_id = iface->ip;
	instance->sequence_num = 0;
	instance->lsuint = MOSPF_DEFAULT_LSUINT;

	iface = NULL;
	list_for_each_entry(iface, &instance->iface_list, list)
	{
		iface->helloint = MOSPF_DEFAULT_HELLOINT;
		init_list_head(&iface->nbr_list);
	}

	init_mospf_db();
}

void *sending_mospf_hello_thread(void *param);
void *sending_mospf_lsu_thread(void *param);
void *checking_nbr_thread(void *param);
void *checking_database_thread(void *param);

void mospf_run()
{
	pthread_t hello, lsu, nbr, db;
	pthread_create(&hello, NULL, sending_mospf_hello_thread, NULL);
	pthread_create(&lsu, NULL, sending_mospf_lsu_thread, NULL);
	pthread_create(&nbr, NULL, checking_nbr_thread, NULL);
	pthread_create(&db, NULL, checking_database_thread, NULL);
}

void *sending_mospf_hello_thread(void *param)
{
	// fprintf(stdout, "TODO: send mOSPF Hello message periodically.\n");
	while (1)
	{
		sleep(MOSPF_DEFAULT_HELLOINT);
		int len = ETHER_HDR_SIZE + IP_BASE_HDR_SIZE + MOSPF_HDR_SIZE + MOSPF_HELLO_SIZE;
		iface_info_t *iface = NULL;
		list_for_each_entry(iface, &instance->iface_list, list)
		{
			char *packet = malloc(len);
			struct ether_header *eh = (struct ether_header *)packet;
			struct iphdr *ihr = packet_to_ip_hdr(packet);
			memcpy(eh->ether_shost, iface->mac, ETH_ALEN);
			memcpy(eh->ether_dhost, mc_mac, ETH_ALEN);
			eh->ether_type = htons(ETH_P_IP);

			struct mospf_hdr *mhr = (struct mospf_hdr *)((char *)ihr + IP_BASE_HDR_SIZE);
			struct mospf_hello *hello = (struct mospf_hello *)((char *)mhr + MOSPF_HDR_SIZE);
			mospf_init_hdr(mhr, MOSPF_TYPE_HELLO, MOSPF_HDR_SIZE + MOSPF_HELLO_SIZE, instance->router_id, instance->area_id);
			mospf_init_hello(hello, iface->mask);
			mhr->checksum = mospf_checksum(mhr);

			ip_init_hdr(ihr, iface->ip, MOSPF_ALLSPFRouters, len - ETHER_HDR_SIZE, IPPROTO_MOSPF);
			iface_send_packet(iface, packet, len);
		}
	}
	return NULL;
}

// broadcast lsu:
static void broadcast_lsu(const char *pkt, int len, u32 ext_rid)
{
	iface_info_t *iface = NULL;
	mospf_nbr_t *nbr = NULL;
	list_for_each_entry(iface, &instance->iface_list, list)
	{
		list_for_each_entry(nbr, &iface->nbr_list, list)
		{
			if (nbr->nbr_id == ext_rid)
				continue;
			char *packet = malloc(len);
			memcpy(packet, pkt, len);
			struct iphdr *ihr = packet_to_ip_hdr(packet);
			ip_init_hdr(ihr, iface->ip, nbr->nbr_ip, len - ETHER_HDR_SIZE, IPPROTO_MOSPF);
			ip_send_packet(packet, len);
		}
	}
}
// send mospf lsu:
static void send_mospf_lsu()
{
	pthread_mutex_lock(&mospf_lock);
	int num = 0;
	iface_info_t *iface = NULL;
	mospf_nbr_t *nbr = NULL;
	list_for_each_entry(iface, &instance->iface_list, list)
	{
		num += iface->num_nbr ? iface->num_nbr : 1;
	}
	struct mospf_lsa array[num];
	int i = 0;
	list_for_each_entry(iface, &instance->iface_list, list)
	{
		if (!iface->num_nbr)
		{
			array[i].rid = 0;
			array[i].network = htonl(iface->ip & iface->mask);
			array[i].mask = htonl(iface->mask);
			i++;
		}
		else
		{
			list_for_each_entry(nbr, &iface->nbr_list, list)
			{
				array[i].rid = htonl(nbr->nbr_id);
				array[i].network = htonl(nbr->nbr_ip & nbr->nbr_mask);
				array[i].mask = htonl(nbr->nbr_mask);
				i++;
			}
		}
	}

	int len = ETHER_HDR_SIZE + IP_BASE_HDR_SIZE + MOSPF_HDR_SIZE + MOSPF_LSU_SIZE + sizeof(array);
	char *packet = malloc(len);
	struct iphdr *ihr = packet_to_ip_hdr(packet);
	struct mospf_hdr *mhr = (struct mospf_hdr *)((char *)ihr + IP_BASE_HDR_SIZE);
	struct mospf_lsu *lsu = (struct mospf_lsu *)((char *)mhr + MOSPF_HDR_SIZE);
	mospf_init_hdr(mhr, MOSPF_TYPE_LSU, len - ETHER_HDR_SIZE - IP_BASE_HDR_SIZE, instance->router_id, instance->area_id);
	mospf_init_lsu(lsu, num);
	memcpy(lsu + 1, array, sizeof(array));
	mhr->checksum = mospf_checksum(mhr);
	instance->sequence_num++;

	pthread_mutex_unlock(&mospf_lock);
	broadcast_lsu(packet, len, 0);
}

void *checking_nbr_thread(void *param)
{
	// fprintf(stdout, "TODO: neighbor list timeout operation.\n");
	while (1)
	{
		sleep(1);
		pthread_mutex_lock(&mospf_lock);
		int mod = 0;
		iface_info_t *iface = NULL;
		list_for_each_entry(iface, &instance->iface_list, list)
		{
			mospf_nbr_t *nbr = NULL, *q;
			list_for_each_entry_safe(nbr, q, &iface->nbr_list, list)
			{
				if (nbr->alive++ >= 3 * MOSPF_DEFAULT_HELLOINT)
				{
					list_delete_entry(&nbr->list);
					free(nbr);
					iface->num_nbr--;
					mod = 1;
				}
			}
		}
		pthread_mutex_unlock(&mospf_lock);
		if (mod)
			send_mospf_lsu();
	}
	return NULL;
}

void *checking_database_thread(void *param)
{
	// fprintf(stdout, "TODO: link state database timeout operation.\n");
	while (1)
	{
		sleep(1);
		pthread_mutex_lock(&db_lock);
		mospf_db_entry_t *entry = NULL, *q;
		list_for_each_entry_safe(entry, q, &mospf_db, list)
		{
			if (entry->alive++ >= MOSPF_DATABASE_TIMEOUT)
			{
				list_delete_entry(&entry->list);
				free(entry);
			}
		}
		pthread_mutex_unlock(&db_lock);
	}
	return NULL;
}

void printnbr();
void handle_mospf_hello(iface_info_t *iface, const char *packet, int len)
{
	// fprintf(stdout, "TODO: handle mOSPF Hello message.\n");
	struct iphdr *ihr = packet_to_ip_hdr(packet);
	struct mospf_hdr *mhr = (struct mospf_hdr *)((char *)ihr + IP_BASE_HDR_SIZE);
	struct mospf_hello *hello = (struct mospf_hello *)((char *)mhr + MOSPF_HDR_SIZE);
	u32 rid = ntohl(mhr->rid);

	pthread_mutex_lock(&mospf_lock);
	mospf_nbr_t *nbr = NULL;
	list_for_each_entry(nbr, &iface->nbr_list, list)
	{
		if (nbr->nbr_id == rid)
		{
			nbr->alive = 0;
			pthread_mutex_unlock(&mospf_lock);
			return;
		}
	}
	nbr = malloc(sizeof(mospf_nbr_t));
	nbr->nbr_id = rid;
	nbr->nbr_ip = ntohl(ihr->saddr);
	nbr->alive = 0;
	nbr->nbr_mask = ntohl(hello->mask);
	list_add_tail(&nbr->list, &iface->nbr_list);
	iface->num_nbr++;

	printnbr();
	pthread_mutex_unlock(&mospf_lock);
	send_mospf_lsu();
}

void *sending_mospf_lsu_thread(void *param)
{
	// fprintf(stdout, "TODO: send mOSPF LSU message periodically.\n");
	while (1)
	{
		sleep(MOSPF_DEFAULT_LSUINT);
		send_mospf_lsu();
	}
	return NULL;
	return NULL;
}

//路由器计算路由表项：
#define MN 25
#define MNBR 5
typedef struct
{
	u32 rid;
	u32 gw;
	u32 mask;
	u32 network;
	int nadv;
	iface_info_t *iface;
} rnode_t;
static void update_rtable()
{

	rnode_t rnode[MN];
	char visit[MN] = {0};
	int graph[MN][MNBR];
	int queue[MN];
	int i1 = 0, i2 = 0;
	int k = 0;
	i1 = i2 = 0;

	rt_entry_t *rt_entry, *q;
	list_for_each_entry_safe(rt_entry, q, &rtable, list)
	{
		if (rt_entry->gw)
			remove_rt_entry(rt_entry);
	}

	pthread_mutex_lock(&db_lock);
	mospf_db_entry_t *entry = NULL;
	list_for_each_entry(entry, &mospf_db, list)
	{
		rnode[k].nadv = entry->nadv;
		rnode[k++].rid = entry->rid;
	}

	int found;
	int i = 0;
	list_for_each_entry(entry, &mospf_db, list)
	{
		for (int j = 0; j < entry->nadv; j++)
		{
			found = 0;
			for (int l = 0; l < k; l++)
			{
				if (rnode[l].rid == entry->array[j].rid)
				{
					graph[i][j] = l;
					rnode[l].mask = entry->array[j].mask;
					rnode[l].network = entry->array[j].network;
					found = 1;
				}
			}
			if (!found)
			{
				rnode[k].rid = entry->array[j].network;
				graph[i][j] = k;
				rnode[k].mask = entry->array[j].mask;
				rnode[k].nadv = 0;
				rnode[k++].network = entry->array[j].network;
			}
		}
		i++;
	}
	pthread_mutex_unlock(&db_lock);
	pthread_mutex_lock(&mospf_lock);

	iface_info_t *iface = NULL;
	mospf_nbr_t *nbr = NULL;
	list_for_each_entry(iface, &instance->iface_list, list)
	{
		list_for_each_entry(nbr, &iface->nbr_list, list)
		{
			for (int i = 0; i < k; i++)
			{
				if (rnode[i].rid == nbr->nbr_id)
				{
					rnode[i].iface = iface;
					rnode[i].gw = nbr->nbr_ip;
					visit[i] = 1;

					queue[i2++] = i;
				}
			}
		}
	}
	pthread_mutex_unlock(&mospf_lock);

	while (i2 > i1)
	{
		int th = queue[i1++];
		for (int i = 0; i < rnode[th].nadv; i++)
		{
			int next = graph[th][i];
			if (visit[next])
				continue;
			rnode[next].iface = rnode[th].iface;
			rnode[next].gw = rnode[th].gw;
			rt_entry_t *rt_entry = new_rt_entry(rnode[next].network, rnode[next].mask, rnode[th].gw, rnode[th].iface);
			add_rt_entry(rt_entry);
			visit[next] = 1;
			fprintf(stdout, "visit " IP_FMT "\n", HOST_IP_FMT_STR(rnode[next].rid));
			queue[i2++] = next;
		}
	}
	print_rtable();
}

void printdb();
void handle_mospf_lsu(iface_info_t *iface, char *packet, int len)
{
	// fprintf(stdout, "TODO: handle mOSPF LSU message.\n");
	struct iphdr *ihr = packet_to_ip_hdr(packet);
	struct mospf_hdr *mhr = (struct mospf_hdr *)((char *)ihr + IP_BASE_HDR_SIZE);
	struct mospf_lsu *lsu = (struct mospf_lsu *)((char *)mhr + MOSPF_HDR_SIZE);
	if (lsu->ttl < 1)
		return;

	u32 rid = ntohl(mhr->rid);
	u16 seq = ntohs(lsu->seq);
	u32 nadv = ntohl(lsu->nadv);
	pthread_mutex_lock(&db_lock);
	int found = 0;
	mospf_db_entry_t *entry = NULL, *q;
	list_for_each_entry_safe(entry, q, &mospf_db, list)
	{
		if (entry->rid == rid)
		{
			if (entry->seq >= seq)
			{
				pthread_mutex_unlock(&db_lock);
				return;
			}
			found = 1;
			free(entry->array);
			break;
		}
	}
	if (!found)
	{
		entry = malloc(sizeof(mospf_db_entry_t));
		list_add_tail(&entry->list, &mospf_db);
	}
	entry->rid = rid;
	entry->seq = seq;
	entry->nadv = nadv;
	entry->alive = 0;
	entry->array = malloc(nadv * sizeof(struct mospf_lsa));
	struct mospf_lsa *lsa = (struct mospf_lsa *)(lsu + 1);
	for (int i = 0; i < nadv; i++)
	{
		entry->array[i].rid = ntohl(lsa[i].rid);
		entry->array[i].network = ntohl(lsa[i].network);
		entry->array[i].mask = ntohl(lsa[i].mask);
	}
	printdb();
	pthread_mutex_unlock(&db_lock);
	update_rtable();

	if (lsu->ttl-- > 1)
	{
		mhr->checksum = mospf_checksum(mhr);
		broadcast_lsu(packet, len, rid);
	}
}

void handle_mospf_packet(iface_info_t *iface, char *packet, int len)
{
	struct iphdr *ip = (struct iphdr *)(packet + ETHER_HDR_SIZE);
	struct mospf_hdr *mospf = (struct mospf_hdr *)((char *)ip + IP_HDR_SIZE(ip));

	if (mospf->version != MOSPF_VERSION)
	{
		log(ERROR, "received mospf packet with incorrect version (%d)", mospf->version);
		return;
	}
	if (mospf->checksum != mospf_checksum(mospf))
	{
		log(ERROR, "received mospf packet with incorrect checksum");
		return;
	}
	if (ntohl(mospf->aid) != instance->area_id)
	{
		log(ERROR, "received mospf packet with incorrect area id");
		return;
	}

	switch (mospf->type)
	{
	case MOSPF_TYPE_HELLO:
		handle_mospf_hello(iface, packet, len);
		break;
	case MOSPF_TYPE_LSU:
		handle_mospf_lsu(iface, packet, len);
		break;
	default:
		log(ERROR, "received mospf packet with unknown type (%d).", mospf->type);
		break;
	}
}

// print tdb,tnbr:
void printdb()
{
	mospf_db_entry_t *entry = NULL;
	fprintf(stdout, "RID\t\tNetwork\t\tMask\t\tNbr\n");
	list_for_each_entry(entry, &mospf_db, list)
	{
		for (int i = 0; i < entry->nadv; i++)
		{
			fprintf(stdout, IP_FMT "\t" IP_FMT "\t" IP_FMT "\t" IP_FMT "\n",
					HOST_IP_FMT_STR(entry->rid),
					HOST_IP_FMT_STR(entry->array[i].network),
					HOST_IP_FMT_STR(entry->array[i].mask),
					HOST_IP_FMT_STR(entry->array[i].rid));
		}
	}
}

void printnbr()
{
	iface_info_t *iface = NULL;
	mospf_nbr_t *nbr = NULL;
	fprintf(stdout, "RID\t\tRIP\t\tMask\n");
	list_for_each_entry(iface, &instance->iface_list, list)
	{
		list_for_each_entry(nbr, &iface->nbr_list, list)
		{
			fprintf(stdout, IP_FMT "\t" IP_FMT "\t" IP_FMT "\n",
					HOST_IP_FMT_STR(nbr->nbr_id),
					HOST_IP_FMT_STR(nbr->nbr_ip),
					HOST_IP_FMT_STR(nbr->nbr_mask));
		}
	}
}