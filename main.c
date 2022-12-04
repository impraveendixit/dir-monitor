#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <mosquitto.h>

#include "dir-monitor.h"
#include "dm-manager.h"
#include "debug.h"


int main(int argc, char **argv)
{
	struct dm_manager *dmm = NULL;

	/* Mosquitto lib initialization; Not thread safe..!! */
	mosquitto_lib_init();

	INFO("Starting Directory monitoring system...");

	/* Create manager thread */
	if (dm_manager_start(&dmm, argc, argv) != 0) {
		WARN("Failed to start Directory monitoring system..!!");
		mosquitto_lib_cleanup();
		exit(EXIT_FAILURE);
	}

	/* Manager waits to receive commands */
	dm_manager_run(dmm);

	/* free up resources */
	dm_manager_stop(dmm);

	INFO("Directory monitoring system stopped...!!");

	mosquitto_lib_cleanup();
	exit(EXIT_SUCCESS);
}
