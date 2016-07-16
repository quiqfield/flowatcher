#include "main.h"
#include "flowtable.h"

static void sigint_handler(int sig_num)
{
	RTE_LOG(INFO, FLOWATCHER, "Exiting on signal %d\n", sig_num);
	app_quit_signal = 1;
	lws_cancel_service(wss_context);
}

static void app_finalize(void)
{
	uint32_t i;

	get_app_status();

	/* free */
	for (i = 0; i < app.n_rx_cores; i++) {
		destroy_flow_table(app.flow_table_v4[i]);
		destroy_flow_table(app.flow_table_v6[i]);
	}
	lws_context_destroy(wss_context);
}

static int app_lcore_main_loop(__attribute__((unused)) void *arg)
{
	unsigned lcore_id = rte_lcore_id();
	uint32_t i;

	for (i = 0; i < APP_MAX_LCORE; i++) {
		struct lcore_conf *lconf = &app.lcore_conf[i];

		if (lconf->core_id != lcore_id)
			continue;

		switch (lconf->core_type) {
			case APP_CORE_RXFL:
				app_main_loop_rx_flow();
				return 0;
			case APP_CORE_MONX:
				app_main_loop_monitor();
				return 0;
			default:
				app_main_loop_skelton();
				return 0;
		}
	}

	/* expected unreachable */
	return -1;
}

int main(int argc, char **argv)
{
	unsigned lcore_id;
	int ret;

	if (signal(SIGINT, sigint_handler) == SIG_ERR)
		rte_exit(EXIT_FAILURE, "Failed to set signal handler\n");

	/* init EAL */
	ret = rte_eal_init(argc, argv);
	if (ret < 0)
		rte_exit(EXIT_FAILURE, "Error with EAL initialization\n");
	argc -= ret;
	argv += ret;

	app_init();

	/* call app_lcore_main_loop() on every slave lcore */
	RTE_LCORE_FOREACH_SLAVE (lcore_id) {
		rte_eal_remote_launch(app_lcore_main_loop, NULL, lcore_id);
	}
	/* call in on master lcore too */
	(void) app_lcore_main_loop(NULL);

	rte_eal_mp_wait_lcore();
	app_finalize();

	return 0;
}
