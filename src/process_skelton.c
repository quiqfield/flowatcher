#include "main.h"

void app_main_loop_skelton(void)
{
	const unsigned lcore_id = rte_lcore_id();
	RTE_LOG(INFO, FLOWATCHER, "[core %u] skelton -- do nothing\n", lcore_id);
}
