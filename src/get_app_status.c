#include "main.h"

static struct rte_eth_stats prev_stat[RTE_MAX_ETHPORTS];

void get_app_status_init(void)
{
	uint32_t port, i;
	for (port = 0; port < app.n_ports; port++)
		rte_eth_stats_get(port, &prev_stat[port]);
	for (i = 0; i < app.n_rx_cores; i++) {
		app_stat[i].rx_count = 0;
		app_stat[i].updated_tbl_v4_count = 0;
		app_stat[i].miss_updated_tbl_v4_count = 0;
		app_stat[i].updated_tbl_v6_count = 0;
		app_stat[i].miss_updated_tbl_v6_count = 0;
		app_stat[i].unknown_pkt_count = 0;
	}
	exp_stat.flow_cnt = 0;
	exp_stat.kpps = 0;
	exp_stat.mbps = 0;
}

void get_app_status(void)
{
	struct rte_eth_stats stat[app.n_ports];
	struct app_rx_flow_stats _app_stat[app.n_rx_cores];
	struct app_rx_flow_stats total;
	uint32_t port, i;
	uint32_t cnt_v4, cnt_v6;

	/* get status */
	cnt_v4 = cnt_v6 = 0;
	for (i = 0; i < app.n_rx_cores; i++) {
		cnt_v4 += app.flow_table_v4[i]->flow_cnt;
		cnt_v6 += app.flow_table_v6[i]->flow_cnt;
	}

	for (port = 0; port < app.n_ports; port++)
		rte_eth_stats_get(port, &stat[port]);
	rte_memcpy(_app_stat, app_stat, sizeof(struct app_rx_flow_stats) * app.n_rx_cores);

	memset(&total, 0, sizeof(struct app_rx_flow_stats));
	for (i = 0; i < app.n_rx_cores; i++) {
		total.rx_count += _app_stat[i].rx_count;
		total.updated_tbl_v4_count += _app_stat[i].updated_tbl_v4_count;
		total.miss_updated_tbl_v4_count += _app_stat[i].miss_updated_tbl_v4_count;
		total.updated_tbl_v6_count += _app_stat[i].updated_tbl_v6_count;
		total.miss_updated_tbl_v6_count += _app_stat[i].miss_updated_tbl_v6_count;
		total.unknown_pkt_count += _app_stat[i].unknown_pkt_count;
	}

	/* print status */
	printf("\n===== Ports status =====\n");
	for (port = 0; port < app.n_ports; port++) {
		printf(" [Port %u]\n RX: %"PRIu64" pkts, %"PRIu64" bytes\n"
				" Drop: %"PRIu64" pkts\n RX Total: %"PRIu64" pkts\n"
				" TX: %"PRIu64" pkts, %"PRIu64" bytes\n",
				port, stat[port].ipackets, stat[port].ibytes, stat[port].imissed,
				(stat[port].ipackets + stat[port].imissed),
				stat[port].opackets, stat[port].obytes);
	}

	printf("=====  APP status  =====\n");
	printf(" RX: %"PRIu64" pkts\n", total.rx_count);
	printf(" tbl_v4: %"PRIu64" pkts\n", total.updated_tbl_v4_count);
	printf(" miss_tbl_v4: %"PRIu64" pkts\n", total.miss_updated_tbl_v4_count);
	printf(" tbl_v6: %"PRIu64" pkts\n", total.updated_tbl_v6_count);
	printf(" miss_tbl_v6: %"PRIu64" pkts\n", total.miss_updated_tbl_v6_count);
	printf(" unknown_pkt: %"PRIu64" pkts\n", total.unknown_pkt_count);

	printf("===== Flow count =====\n");
	printf(" cnt_v4: %"PRIu32", cnt_v6: %"PRIu32"\n", cnt_v4, cnt_v6);
}

void get_app_status_1sec(void)
{
	struct rte_eth_stats stat[app.n_ports];
	struct app_rx_flow_stats _app_stat[app.n_rx_cores];
	struct app_rx_flow_stats total;
	uint64_t exp_pps, exp_bps;
	uint32_t port, i;
	uint32_t cnt_v4, cnt_v6;

	/* get status */
	cnt_v4 = cnt_v6 = 0;
	for (i = 0; i < app.n_rx_cores; i++) {
		cnt_v4 += app.flow_table_v4[i]->flow_cnt;
		cnt_v6 += app.flow_table_v6[i]->flow_cnt;
	}

	for (port = 0; port < app.n_ports; port++)
		rte_eth_stats_get(port, &stat[port]);
	rte_memcpy(_app_stat, app_stat, sizeof(struct app_rx_flow_stats) * app.n_rx_cores);

	memset(&total, 0, sizeof(struct app_rx_flow_stats));
	for (i = 0; i < app.n_rx_cores; i++) {
		total.rx_count += _app_stat[i].rx_count;
		total.updated_tbl_v4_count += _app_stat[i].updated_tbl_v4_count;
		total.miss_updated_tbl_v4_count += _app_stat[i].miss_updated_tbl_v4_count;
		total.updated_tbl_v6_count += _app_stat[i].updated_tbl_v6_count;
		total.miss_updated_tbl_v6_count += _app_stat[i].miss_updated_tbl_v6_count;
		total.unknown_pkt_count += _app_stat[i].unknown_pkt_count;
	}

	/* print status */
	exp_pps = exp_bps = 0;
	for (port = 0; port < app.n_ports; port++) {
		printf("[Port %u] RX Total: %"PRIu64" pkts(%.4lfMpps),"
				" RX: %"PRIu64" pkts(%.4lfMpps)"
				" %"PRIu64" bytes(%.4lfGbps),"
				" Drop: %"PRIu64" pkts(%.4lfMpps)\n",
				port, (stat[port].ipackets + stat[port].imissed),
				(double) ((stat[port].ipackets + stat[port].imissed
				 - prev_stat[port].ipackets - prev_stat[port].imissed) / 1000000.0),
				stat[port].ipackets,
				(double) ((stat[port].ipackets - prev_stat[port].ipackets) / 1000000.0),
				stat[port].ibytes,
				(double) ((stat[port].ibytes - prev_stat[port].ibytes) * 8 / 1000000000.0),
				stat[port].imissed,
				(double) ((stat[port].imissed - prev_stat[port].imissed) / 1000000.0));
		exp_pps += (stat[port].ipackets - prev_stat[port].ipackets);
		exp_bps += (stat[port].ibytes - prev_stat[port].ibytes) * 8;
	}
	exp_stat.kpps = (float) (exp_pps / 1000.0);
	exp_stat.mbps = (float) (exp_bps / 1000000.0);

	printf("[APP] RX: %"PRIu64" pkts, tbl_v4: %"PRIu64" pkts, miss_tbl_v4: %"PRIu64" pkts"
			" tbl_v6: %"PRIu64" pkts, miss_tbl_v6: %"PRIu64" pkts, unknown: %"PRIu64" pkts\n",
			total.rx_count, total.updated_tbl_v4_count, total.miss_updated_tbl_v4_count,
			total.updated_tbl_v6_count, total.miss_updated_tbl_v6_count, total.unknown_pkt_count);

	printf("[Flowtable] cnt_v4: %"PRIu32", cnt_v6: %"PRIu32"\n", cnt_v4, cnt_v6);
	exp_stat.flow_cnt = cnt_v4 + cnt_v6;

	/* update prev_stat */
	rte_memcpy(prev_stat, stat, sizeof(struct rte_eth_stats) * app.n_ports);
}
