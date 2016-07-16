#include "main.h"
#include "flowtable.h"
#include "config.h"

volatile uint8_t app_quit_signal = 0;

struct app_params app = {
	/* cores */
	.lcore_conf = {
		{0, APP_CORE_MONX, 0},
		{1, APP_CORE_RXFL, 0},
		{2, APP_CORE_RXFL, 0},
		{3, APP_CORE_RXFL, 0},
	},
};

static const struct rte_eth_conf port_conf = {
	.rxmode = {
		.mq_mode = ETH_MQ_RX_RSS,
		.max_rx_pkt_len = ETHER_MAX_LEN,
	},
	.rx_adv_conf = {
		.rss_conf= {
			.rss_key = NULL,
			.rss_hf = ETH_RSS_IP,
		},
	},
	.txmode = {
		.mq_mode = ETH_MQ_TX_NONE,
	},
};
static const struct rte_eth_rxconf rx_conf = {
	.rx_thresh = {
		.pthresh = 8,
		.hthresh = 8,
		.wthresh = 4,
	},
	.rx_free_thresh = 64,
	.rx_drop_en = 0,
};

struct app_rx_flow_stats app_stat[APP_MAX_LCORE];
struct app_rx_exp_stats exp_stat;

struct lws_context *wss_context = NULL;
struct lws_protocols wss_protocols[] = {
	{
		"http-only",
		callback_http,
		0,
		0, 0, 0
	},
	{
		"flow-export-protocol",
		callback_flowexp,
		sizeof(struct per_session_data__flowexp),
		10, 0, 0,
	},
	{ 0 }
};



static void app_init_lcore_conf(void)
{
	unsigned lcore_id;
	uint32_t i, enmonx = 0, count = 0;

	for (lcore_id = 0; lcore_id < APP_MAX_LCORE; lcore_id++) {
		if (rte_lcore_is_enabled(lcore_id) == 0)
			continue;

		for (i = 0; i < APP_MAX_LCORE; i++) {
			struct lcore_conf *lconf = &app.lcore_conf[i];

			if (lconf->core_id != lcore_id)
				continue;

			if (lconf->core_type == APP_CORE_RXFL) {
				lconf->queue_id = count++;
			} else if (lconf->core_type == APP_CORE_MONX) {
				if (enmonx)
					lconf->core_type = APP_CORE_NONE;
				enmonx = 1;
			}
		}
	}

	RTE_LOG(INFO, FLOWATCHER, "n_rx_cores = %u\n", count);
	app.n_rx_cores = count;
}

static void app_init_mbuf_pool(void)
{
	RTE_LOG(INFO, FLOWATCHER, "Creating mbuf_pool ...\n");

	app.pool = rte_mempool_create("mbuf_pool",
			POOL_SIZE * app.n_rx_cores, POOL_MBUF_SIZE,
			POOL_CACHE_SIZE,
			sizeof(struct rte_pktmbuf_pool_private),
			rte_pktmbuf_pool_init, NULL,
			rte_pktmbuf_init, NULL,
			(int) rte_socket_id(), 0);

	if (app.pool == NULL)
		rte_exit(EXIT_FAILURE, "Cannot create mbuf_pool\n");
}

static void app_init_flowtables(void)
{
	uint32_t i;
	char s[32];

	RTE_LOG(INFO, FLOWATCHER, "Creating flow_table ...\n");

	for (i = 0; i < app.n_rx_cores; i++) {
		snprintf(s, sizeof(s), "flow_table_v4_%u", i);
		app.flow_table_v4[i] = create_flow_table(s,
				FLOW_TABLE_SIZE, PKT_IP_TYPE_IPV4);

		if (app.flow_table_v4[i] == NULL)
			rte_exit(EXIT_FAILURE, "Cannot create %s\n", s);
		else
			RTE_LOG(INFO, FLOWATCHER, "Created %s\n", s);
	}

	for (i = 0; i < app.n_rx_cores; i++) {
		snprintf(s, sizeof(s), "flow_table_v6_%u", i);
		app.flow_table_v6[i] = create_flow_table(s,
				FLOW_TABLE_SIZE, PKT_IP_TYPE_IPV6);

		if (app.flow_table_v6[i] == NULL)
			rte_exit(EXIT_FAILURE, "Cannot create %s\n", s);
		else
			RTE_LOG(INFO, FLOWATCHER, "Created %s\n", s);
	}
}

#define CHECK_INTERVAL 100 /* 100ms */
#define MAX_CHECK_TIME 50  /* 5s (50 * 100ms) in total */
static void app_ports_check_link_status(void)
{
	uint32_t port;
	uint8_t count, all_ports_up, print_flag = 0;
	struct rte_eth_link link;

	RTE_LOG(INFO, FLOWATCHER, "Checking link status ");

	for (count = 0; count <= MAX_CHECK_TIME; count++) {
		all_ports_up = 1;

		for (port = 0; port < app.n_ports; port++) {
			memset(&link, 0, sizeof(link));
			rte_eth_link_get_nowait(port, &link);
			if (print_flag) {
				RTE_LOG(INFO, FLOWATCHER,
						"[Port %u] Link %s - speed %u Mbps - %s\n",
						port,
						link.link_status ? "UP" : "DOWN",
						link.link_speed,
						(link.link_duplex == ETH_LINK_FULL_DUPLEX) ?
						"full-duplex" : "half-duplex");
				continue;
			}
			if (link.link_status == 0) {
				all_ports_up = 0;
				break;
			}
		}
		if (print_flag == 1)
			break;

		if (all_ports_up == 0) {
			printf(".");
			fflush(stdout);
			rte_delay_ms(CHECK_INTERVAL);
		}

		if (all_ports_up == 1 || count == (MAX_CHECK_TIME - 1)) {
			print_flag = 1;
			printf("done\n");
		}
	}
}

static void app_init_ports(void)
{
	unsigned lcore_id;
	uint32_t i, port;
	uint16_t n_rx_queue = app.n_rx_cores, n_tx_queue = 1, q;
	int ret;

	for (port = 0; port < app.n_ports; port++) {
		RTE_LOG(INFO, FLOWATCHER, "Initializing port %u ...\n", port);

		/* init port */
		ret = rte_eth_dev_configure(port, n_rx_queue, n_tx_queue, &port_conf);
		if (ret < 0)
			rte_exit(EXIT_FAILURE, "Cannot initialize port %u (%d)\n", port, ret);

		/* init RX queues */
		q = 0;
		for (lcore_id = 0; lcore_id < APP_MAX_LCORE; lcore_id++) {
			if (rte_lcore_is_enabled(lcore_id) == 0)
				continue;

			for (i = 0; i < APP_MAX_LCORE; i++) {
				struct lcore_conf *lconf = &app.lcore_conf[i];

				if (lconf->core_id != lcore_id)
					continue;

				if (lconf->core_type == APP_CORE_RXFL) {
					ret = rte_eth_rx_queue_setup(port, q, RX_RING_SIZE,
							rte_eth_dev_socket_id(port), &rx_conf, app.pool);
					if (ret < 0)
						rte_exit(EXIT_FAILURE, "Cannot initialize RX for port %u (%d)\n", port, ret);
					RTE_LOG(INFO, FLOWATCHER, "Setup RX queue %u for port %u (lcore:%u)\n",
							q, port, lcore_id);
					q++;
				}
			}
		}

		/* init TX queues */
		for (q = 0; q < n_tx_queue; q++) {
			ret = rte_eth_tx_queue_setup(port, q, TX_RING_SIZE,
					rte_eth_dev_socket_id(port), NULL);
			if (ret < 0)
				rte_exit(EXIT_FAILURE, "Cannot initialize TX for port %u (%d)\n", port, ret);
			RTE_LOG(INFO, FLOWATCHER, "Setup TX queue %u for port %u\n", q, port);
		}

		/* start port */
		ret = rte_eth_dev_start(port);
		if (ret < 0)
			rte_exit(EXIT_FAILURE, "Cannot start port %u (%d)\n", port, ret);

		/* display port MAC address */
		struct ether_addr addr;
		char buf[ETHER_ADDR_FMT_SIZE];
		rte_eth_macaddr_get(port, &addr);
		ether_format_addr(buf, ETHER_ADDR_FMT_SIZE, &addr);
		RTE_LOG(INFO, FLOWATCHER, "[Port %u] MAC address %s\n", port, buf);

		rte_eth_promiscuous_enable(port);
		rte_eth_stats_reset(port);
	}

	app_ports_check_link_status();
}

static void app_init_lws_context(void)
{
	struct lws_context_creation_info info;

	memset(&info, 0, sizeof(info));
	info.port = FLOWEXP_PORT;
	info.iface = NULL;
	info.protocols = wss_protocols;
	info.ssl_cert_filepath = NULL;
	info.ssl_private_key_filepath = NULL;
	info.gid = -1;
	info.uid = -1;
	info.max_http_header_pool = 1;
	info.options = 0;

	wss_context = lws_create_context(&info);
	if (wss_context == NULL)
		rte_exit(EXIT_FAILURE, "Cannot create lws_context\n");
	else
		RTE_LOG(INFO, FLOWATCHER, "Created lws_context\n");
}

static void app_init_status(void)
{
	get_app_status_init();
}

void app_init(void)
{
	/* check number of lcores */
	uint32_t n_cores = rte_lcore_count();
	if (n_cores > APP_MAX_LCORE)
		n_cores = APP_MAX_LCORE;
	RTE_LOG(INFO, FLOWATCHER, "Find %u logical cores\n", n_cores);

	/* check number of ports */
	app.n_ports = rte_eth_dev_count();
	if (app.n_ports == 0)
		rte_exit(EXIT_FAILURE, "No ethernet port\n");
	if (app.n_ports > RTE_MAX_ETHPORTS)
		app.n_ports = RTE_MAX_ETHPORTS;
	RTE_LOG(INFO, FLOWATCHER, "Find %u ethdev ports\n", app.n_ports);

	/* init app */
	app_init_lcore_conf();
	app_init_mbuf_pool();
	app_init_flowtables();
	app_init_ports();
	app_init_lws_context();
	app_init_status();

	get_app_status();
	RTE_LOG(INFO, FLOWATCHER, "Initialization completed\n\n");
}
