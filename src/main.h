#ifndef __MAIN_H__
#define __MAIN_H__

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <string.h>
#include <errno.h>
#include <inttypes.h>
#include <sys/types.h>
#include <sys/time.h>

#include <rte_common.h>
#include <rte_byteorder.h>
#include <rte_cycles.h>
#include <rte_eal.h>
#include <rte_launch.h>
#include <rte_lcore.h>
#include <rte_log.h>
#include <rte_malloc.h>
#include <rte_mbuf.h>
#include <rte_memcpy.h>
#include <rte_memory.h>
#include <rte_mempool.h>
#include <rte_ring.h>
#include <rte_ethdev.h>
#include <rte_ether.h>
#include <rte_ip.h>
#include <rte_tcp.h>
#include <rte_udp.h>

#include <libwebsockets.h>

#include "flowtable.h"



/** flowatcher app **/

#define RTE_LOGTYPE_FLOWATCHER RTE_LOGTYPE_USER1
#define APP_MAX_LCORE 8

extern volatile uint8_t app_quit_signal;

enum app_core_type {
	APP_CORE_NONE = 0, /* skelton */
	APP_CORE_RXFL, /* RX & update flow_table */
	APP_CORE_MONX, /* monitor & export flow_table */
};

struct lcore_conf {
	uint32_t core_id;
	enum app_core_type core_type;
	uint32_t queue_id; /* make sense only when RX */
} __rte_cache_aligned;

struct app_params {
	/* ports */
	uint32_t n_ports;

	/* cores */
	uint32_t n_rx_cores;
	struct lcore_conf lcore_conf[APP_MAX_LCORE];

	/* mempool */
	struct rte_mempool *pool;

	/* flow table */
	struct flow_table *flow_table_v4[APP_MAX_LCORE];
	struct flow_table *flow_table_v6[APP_MAX_LCORE];
} __rte_cache_aligned;
extern struct app_params app;

/* app status */
struct app_rx_flow_stats {
	uint64_t rx_count;
	uint64_t updated_tbl_v4_count;
	uint64_t miss_updated_tbl_v4_count;
	uint64_t updated_tbl_v6_count;
	uint64_t miss_updated_tbl_v6_count;
	uint64_t unknown_pkt_count;
} __rte_cache_aligned;
extern struct app_rx_flow_stats app_stat[APP_MAX_LCORE];

struct app_rx_exp_stats {
	uint32_t flow_cnt;
	float kpps;
	float mbps;
};
extern struct app_rx_exp_stats exp_stat;



/** for export **/

#define RTE_LOGTYPE_WEBSOCKETS RTE_LOGTYPE_USER1

enum export_states {
	EXPORT_START_V4,
	EXPORT_PAYLOAD_V4,
	EXPORT_PAYLOAD_V4_FIN,
	EXPORT_START_V6,
	EXPORT_PAYLOAD_V6,
	EXPORT_PAYLOAD_V6_FIN,
};

enum wss_protocol {
	PROTOCOL_HTTP = 0,
	PROTOCOL_FLOWEXP,
	COUNT_PROTOCOL
};

struct per_session_data__flowexp {
	uint32_t serial;
	uint32_t left;
	enum export_states state;
};

extern struct lws_context *wss_context;
extern struct lws_protocols wss_protocols[COUNT_PROTOCOL + 1];



/** functions **/

void app_init(void);

/* main_loop */
void app_main_loop_rx_flow(void);
void app_main_loop_monitor(void);
void app_main_loop_skelton(void);

/* lws_callback */
int callback_http(struct lws *wsi, enum lws_callback_reasons reason, void *user, void *in, size_t len);
int callback_flowexp(struct lws *wsi, enum lws_callback_reasons reason, void *user, void *in, size_t len);

/* get_status */
void get_app_status_init(void);
void get_app_status(void);
void get_app_status_1sec(void);

#endif /* __MAIN_H__ */
