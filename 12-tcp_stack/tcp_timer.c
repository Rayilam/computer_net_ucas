#include "tcp.h"
#include "tcp_timer.h"
#include "tcp_sock.h"

#include <stdio.h>
#include <unistd.h>

static struct list_head timer_list;

// scan the timer_list, find the tcp sock which stays for at 2*MSL, release it
void tcp_scan_timer_list()
{
	// fprintf(stdout, "TODO: implement %s please.\n", __FUNCTION__);

	struct tcp_sock *s;
	struct tcp_timer *t;
	int i = 0;
	list_for_each_entry(t, &timer_list, list)
	{
		if (t->type == 0)
		{
			t->timeout -= TCP_TIMER_SCAN_INTERVAL;
			if (t->timeout <= 0)
			{
				list_delete_entry(&t->list);
				s = timewait_to_tcp_sock(t);
				if (s->parent == NULL)
				{
					tcp_bind_unhash(s);
				}
				tcp_set_state(s, TCP_CLOSED);
				free_tcp_sock(s);
			}
		}
		else if (t->type == 1)
		{
			sock = retranstimer_to_tcp_sock(t);
			pthread_mutex_lock(&s->time_lock);
			t->timeout -= TCP_TIMER_SCAN_INTERVAL;
			if (t->timeout <= 0)
			{
				pthread_mutex_lock(&s->sb_lock);
				struct send_buffer *temp;
				list_for_each_entry(temp, &(s->send_buf), list)
				{
					if (temp->number >= 3)
					{
						tcp_send_control_packet(s, TCP_RST);
						exit(1);
					}
					else
					{
						char *packet_temp = (char *)malloc(temp->total_len);
						memcpy(packet_temp, temp->packet, temp->total_len);
						ip_send_packet(packet_temp, temp->total_len);

						t->timeout = TCP_RETRANS_INTERVAL_INITIAL;
						temp->number += 1;
						for (i = 0; i < temp->number; i++)
						{
							t->timeout = 2 * t->timeout;
						}
					}
					break;
				}
				pthread_mutex_unlock(&s->sb_lock);
			}
			pthread_mutex_unlock(&s->time_lock);
		}
	}
}

// set the timewait timer of a tcp sock, by adding the timer into timer_list
void tcp_set_timewait_timer(struct tcp_sock *tsk)
{
	// fprintf(stdout, "TODO: implement %s please.\n", __FUNCTION__);
	// tsk->timewait.enable = 1;
	tsk->timewait.type = 0;
	tsk->timewait.timeout = TCP_TIMEWAIT_TIMEOUT;
	list_add_tail(&tsk->timewait.list, &timer_list);
	tsk->ref_cnt++;
}

// scan the timer_list periodically by calling tcp_scan_timer_list
void *tcp_timer_thread(void *arg)
{
	init_list_head(&timer_list);
	while (1)
	{
		usleep(TCP_TIMER_SCAN_INTERVAL);
		tcp_scan_timer_list();
	}

	return NULL;
}
