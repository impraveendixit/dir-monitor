#ifndef DM_MANAGER_H_INCLUDED
#define DM_MANAGER_H_INCLUDED

struct dm_manager;

/**
 * This function creates and starts lists of directory monitoring agents.
 * Agents are spawned on separate thread. Directories can be passed via
 * command line or through MQTT message in specific format on the given
 * topic which this function subscribes to.
 *
 * @param: out	Storage location to keep allocated dm_manager object.
 * @param: argc	Number of command line arguments.
 * @param: argv	List of directories passed through command line.
 *
 * @return: 0 on success or -1 on failure.
 */
int dm_manager_start(struct dm_manager **out, int argc, char **argv);

/**
 * This function runs directory monitoring manager in infinite
 * loop which waits for MQTT messages.
 *
 * @param: dmm A valid directory monitoring manager object.
 * @return: No return.
 */
void dm_manager_run(struct dm_manager *dmm);

/**
 * This function stops the services of directory monitoring
 * manager and clear resources claimed by it.
 *
 * @param: dmm A valid directory monitoring manager object.
 * @return: No return.
 */
void dm_manager_stop(struct dm_manager *dmm);

#endif /* DM_MANAGER_H_INCLUDED */
