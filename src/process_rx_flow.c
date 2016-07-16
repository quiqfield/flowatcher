#include "main.h"
#include "flowtable.h"
#include "config.h"

void app_main_loop_rx_flow(void)
{
	const unsigned lcore_id = rte_lcore_id();
	struct rte_mbuf *bufs[RX_BURST_SIZE];
	struct rte_mbuf *buf;
	struct ether_hdr *eth_hdr;
	struct ipv4_hdr *ipv4_hdr;
	struct ipv6_hdr *ipv6_hdr;
	struct tcp_hdr *tcp_hdr;
	struct udp_hdr *udp_hdr;
	struct pkt_info pktinfo;
	int32_t ret;
	uint16_t i, n_rx, queueid;
	uint8_t port;

	port = 0;
	queueid = (uint16_t) app.lcore_conf[lcore_id].queue_id;
	RTE_LOG(INFO, FLOWATCHER, "[core %u] packet RX & update flow_table Ready\n", lcore_id);

	while (!app_quit_signal) {

		n_rx = rte_eth_rx_burst(port, queueid, bufs, RX_BURST_SIZE);
		if (unlikely(n_rx == 0)) {
			port++;
			if (port >= app.n_ports)
				port = 0;
			continue;
		}
		app_stat[queueid].rx_count += n_rx;

		for (i = 0; i < n_rx; i++) {
			buf = bufs[i];

			pktinfo.timestamp = rte_rdtsc();
			pktinfo.pktlen = rte_pktmbuf_pkt_len(buf);

			eth_hdr = rte_pktmbuf_mtod(buf, struct ether_hdr *);

			/* strip vlan_hdr */
			if (eth_hdr->ether_type == rte_cpu_to_be_16(ETHER_TYPE_VLAN)) {
				/* struct vlan_hdr *vh = (struct vlan_hdr *) &eth_hdr[1]; */
				/* buf->ol_flags |= PKT_RX_VLAN_PKT; */
				/* buf->vlan_tci = rte_be_to_cpu_16(vh->vlan_tci); */
				/* memmove(rte_pktmbuf_adj(buf, sizeof(struct vlan_hdr)), */
				/* 		eth_hdr, 2 * ETHER_ADDR_LEN); */
				/* eth_hdr = rte_pktmbuf_mtod(buf, struct ether_hdr *); */
				eth_hdr = (struct ether_hdr *) rte_pktmbuf_adj(buf, sizeof(struct vlan_hdr));
			}

			if (eth_hdr->ether_type == rte_cpu_to_be_16(ETHER_TYPE_IPv4)) {
				/* IPv4 */
				pktinfo.type = PKT_IP_TYPE_IPV4;
				ipv4_hdr = (struct ipv4_hdr *) &eth_hdr[1];

				pktinfo.key.v4.src_ip = rte_be_to_cpu_32(ipv4_hdr->src_addr);
				pktinfo.key.v4.dst_ip = rte_be_to_cpu_32(ipv4_hdr->dst_addr);
				pktinfo.key.v4.proto = ipv4_hdr->next_proto_id;

				switch (ipv4_hdr->next_proto_id) {
					case IPPROTO_TCP:
						tcp_hdr = (struct tcp_hdr *) &ipv4_hdr[1];
						pktinfo.key.v4.src_port = rte_be_to_cpu_16(tcp_hdr->src_port);
						pktinfo.key.v4.dst_port = rte_be_to_cpu_16(tcp_hdr->dst_port);
						break;
					case IPPROTO_UDP:
						udp_hdr = (struct udp_hdr *) &ipv4_hdr[1];
						pktinfo.key.v4.src_port = rte_be_to_cpu_16(udp_hdr->src_port);
						pktinfo.key.v4.dst_port = rte_be_to_cpu_16(udp_hdr->dst_port);
						break;
					default:
						pktinfo.key.v4.src_port = 0;
						pktinfo.key.v4.dst_port = 0;
						break;
				}

				rte_pktmbuf_free(buf);

				/* update flow_table_v4 */
				ret = update_flow_entry(app.flow_table_v4[queueid], &pktinfo);
				if (ret == 0)
					app_stat[queueid].updated_tbl_v4_count++;
				else
					app_stat[queueid].miss_updated_tbl_v4_count++;

			} else if (eth_hdr->ether_type == rte_cpu_to_be_16(ETHER_TYPE_IPv6)) {
				/* IPv6 */
				pktinfo.type = PKT_IP_TYPE_IPV6;
				ipv6_hdr = (struct ipv6_hdr *) &eth_hdr[1];

				rte_memcpy(pktinfo.key.v6.src_ip, ipv6_hdr->src_addr, 16);
				rte_memcpy(pktinfo.key.v6.dst_ip, ipv6_hdr->dst_addr, 16);
				pktinfo.key.v6.proto = ipv6_hdr->proto;

				switch (ipv6_hdr->proto) {
					case IPPROTO_TCP:
						tcp_hdr = (struct tcp_hdr *) &ipv6_hdr[1];
						pktinfo.key.v6.src_port = rte_be_to_cpu_16(tcp_hdr->src_port);
						pktinfo.key.v6.dst_port = rte_be_to_cpu_16(tcp_hdr->dst_port);
						break;
					case IPPROTO_UDP:
						udp_hdr = (struct udp_hdr *) &ipv6_hdr[1];
						pktinfo.key.v6.src_port = rte_be_to_cpu_16(udp_hdr->src_port);
						pktinfo.key.v6.dst_port = rte_be_to_cpu_16(udp_hdr->dst_port);
						break;
					default:
						pktinfo.key.v6.src_port = 0;
						pktinfo.key.v6.dst_port = 0;
						break;
				}

				rte_pktmbuf_free(buf);

				/* update flow_table_v6 */
				ret = update_flow_entry(app.flow_table_v6[queueid], &pktinfo);
				if (ret == 0)
					app_stat[queueid].updated_tbl_v6_count++;
				else
					app_stat[queueid].miss_updated_tbl_v6_count++;

			} else {
				/* others */
				app_stat[queueid].unknown_pkt_count++;
				rte_pktmbuf_free(buf);
				continue;
			}
		}

		port++;
		if (port >= app.n_ports)
			port = 0;
	}

	RTE_LOG(INFO, FLOWATCHER, "[core %u] packet RX & update flow_table finished\n", lcore_id);
}
