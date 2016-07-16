#ifndef __FLOWTABLE_H__
#define __FLOWTABLE_H__

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include <sys/time.h>

#include <rte_common.h>
#include <rte_errno.h>
#include <rte_hash.h>
#include <rte_jhash.h>
#include <rte_lcore.h>
#include <rte_log.h>
#include <rte_malloc.h>
#include <rte_memcpy.h>
#include <rte_memory.h>

#include "fivetuple.h"

#define RTE_LOGTYPE_FLOWTABLE RTE_LOGTYPE_USER1

enum pkt_ip_type {
	PKT_IP_TYPE_IPV4,
	PKT_IP_TYPE_IPV6,
};

/* pkt-info */
struct pkt_info {
	uint64_t timestamp;
	uint32_t pktlen;
	enum pkt_ip_type type;
	union fivetuple key;
} __rte_cache_aligned;

/* flow-entry */
struct flow_entry {
	uint64_t start_tsc;
	uint64_t last_tsc;
	uint64_t pkt_cnt;
	uint64_t byte_cnt;
	union fivetuple key;
} __rte_cache_aligned;

/* flow-table */
struct flow_table {
	struct rte_hash *handler;
	uint64_t flow_cnt;
	struct flow_entry entries[0];
};

static inline uint32_t roundup32(uint32_t x)
{
	x--;
	x |= x >> 1;
	x |= x >> 2;
	x |= x >> 4;
	x |= x >> 8;
	x |= x >> 16;
	x++;
	return x;
}

/* functions */
struct flow_table *create_flow_table(const char *name, uint32_t _size, enum pkt_ip_type type);
void destroy_flow_table(struct flow_table *table);
int update_flow_entry(struct flow_table *table, struct pkt_info *info);
int remove_flow_entry(struct flow_table *table, union fivetuple *_key);

#endif /* __FLOWTABLE_H__ */
