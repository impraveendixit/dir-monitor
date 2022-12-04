#include <stdio.h>
#include <stdlib.h>
#include <libgen.h>
#include <pthread.h>
#include <json-c/json.h>
#include <mosquitto.h>

#include "dm-manager.h"
#include "dir-monitor.h"
#include "internals.h"
#include "debug.h"


struct dm_manager {
	/* Maintain list of directory monitoring agent */
	struct dir_monitor_list *dm_list;
	/* Mosquitto subscriber to receive commands */
	struct mosquitto *mosq;
};


/**
 * This function fetches directories from the json list object and modifies the
 * directory monitor agent list depending on the flag passed.
 *
 * @param: obj		json_object array type.
 * @param: do_remove	Flag to control modification of list. 0-add,1-remove.
 * @param: dm_list	List containing monitoring agents.
 * @return: No return.
 */
static void __modify_dir_monitor_list(json_object *obj, unsigned int do_remove,
				      struct dir_monitor_list *dm_list)
{
	int len = json_object_array_length(obj);
	register int i;

	for (i = 0; i < len; i++) {
		json_object *tmp = json_object_array_get_idx(obj, i);
		if (do_remove)
			dir_monitor_list_remove(dm_list,
						json_object_get_string(tmp));
		else
			dir_monitor_list_add(dm_list,
					     json_object_get_string(tmp));
	}
}

/**
 * This function disconnect from the subscriber to kill
 * the infinite subscriber loop.
 *
 * @param: dmm A valid dm manager object.
 * @return: No return.
 */
static void __kill_dm_manager(struct dm_manager *dmm)
{
	/* Kill the subscriber loop */
	mosquitto_disconnect(dmm->mosq);
}

/**
 * This function parses the MQTT message command and control the
 * directory monitoring manager behavior based on that.
 *
 * @param: user_data	Userdata to pass to the function.
 * @param: msg		MQTT message.
 * @return: 0 on success. -1 on failure.
 */
static int parse_message(void *user_data, const char *msg)
{
	const char *cmd_code = NULL;
	json_object *tmp, *root = NULL;
	struct dm_manager *dmm = (struct dm_manager *)user_data;

	root = json_tokener_parse(msg);
	if (!root) {
		DEBUG("Failed to parse message");
		return -1;
	}

	if (json_object_object_get_ex(root, "cmd_code", &tmp))
		cmd_code = json_object_get_string(tmp);

	if (!strcmp(cmd_code, "start_dir_monitoring")) {
		if (!json_pointer_get(root, "/msg/directories", &tmp))
			__modify_dir_monitor_list(tmp, 0, dmm->dm_list);

	} else if (!strcmp(cmd_code, "stop_dir_monitoring")) {
		if (!json_pointer_get(root, "/msg/directories", &tmp))
			__modify_dir_monitor_list(tmp, 1, dmm->dm_list);

	} else if (!strcmp(cmd_code, "kill_dir_monitoring")) {
		__kill_dm_manager(dmm);

	} else {
		DEBUG("Unknown message format");
	}

	json_object_put(root);
	return 0;
}

static void on_message_callback(struct mosquitto *mosq, void *obj,
				const struct mosquitto_message *msg)
{
	DEBUG("%s", (const char *)msg->payload);
	parse_message(obj, msg->payload);
}

int dm_manager_start(struct dm_manager **out, int argc, char **argv)
{
	register int i;
	char topic[64] = "";
	struct dm_manager *dmm = NULL;

	if ((dmm = malloc(sizeof(struct dm_manager))) == NULL) {
		SYSERR("Memory allocation failure");
		goto exit;
	}

	/* Create List for directory monitor agents */
	dmm->dm_list = dir_monitor_list_create();
	if (dmm->dm_list == NULL) {
		DEBUG("Failed to create dir monitor list");
		goto exit_free;
	}

	/* Initiate a mosquitto subscriber for manager */
	dmm->mosq = mosquitto_new("Directory Monitoring System", true, dmm);
	if (dmm->mosq == NULL) {
		ERROR("mosquitto_new() failed.");
		goto exit_destroy_list;
	}

	/* Add message receive callback */
	mosquitto_message_callback_set(dmm->mosq, on_message_callback);

	if (mosquitto_connect(dmm->mosq, HOST_ADDRESS, MQTT_PORT,
			      MQTT_CONNECTION_TIMEOUT) != MOSQ_ERR_SUCCESS) {
		ERROR("Failed to connect to broker");
		goto exit_destroy_mosq;
	}

	/* Subscribe to topic for config messages */
	snprintf(topic, 64, "%s%s", TOPIC_PREFIX, "config");
	mosquitto_subscribe(dmm->mosq, NULL, topic, 0);

	/* Create all monitor threads and add to the list */
	for (i = 0; i < argc - 1; i++) {
		char *dir = argv[i + 1];
		if (!dir_monitor_list_add(dmm->dm_list, dir))
			INFO("Started monitoring %s directory.", dir);
		else
			WARN("Failed to monitor %s directory.", dir);
	}

	*out = dmm;
	return 0;

 exit_destroy_mosq:
	mosquitto_destroy(dmm->mosq);
 exit_destroy_list:
	dir_monitor_list_destroy(dmm->dm_list);
 exit_free:
	free(dmm);
 exit:
	return -1;
}

void dm_manager_run(struct dm_manager *dmm)
{
	/* Subscriber loop */
	mosquitto_loop_forever(dmm->mosq, 10, 1);
}

void dm_manager_stop(struct dm_manager *dmm)
{
	if (dmm) {
		/* Must not call from callback function ..!! */
		mosquitto_destroy(dmm->mosq);
		dir_monitor_list_destroy(dmm->dm_list);
		free(dmm);
	}
}
