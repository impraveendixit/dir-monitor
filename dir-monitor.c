#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <libgen.h>
#include <pthread.h>
#include <mosquitto.h>
#include <sys/time.h>
#include <sys/inotify.h>

#include "dir-monitor.h"
#include "internals.h"
#include "debug.h"


#define READ_TIMEOUT	1000
#define NAME_LEN	64
#define EVENT_LEN	(sizeof(struct inotify_event))
#define NR_EVENTS	4096
#define EVENT_BUFF_SIZE	(NR_EVENTS * (NAME_LEN + EVENT_LEN + 1))
#define BUFF_SIZE	(NR_EVENTS * NAME_LEN)

struct dir_monitor {
	/* MQTT client */
	struct mosquitto *client;
	/* Dynamically allocated string for directory name */
	char *dir_path;
	/* File descriptor associated with monitor */
	int fd;
	/* Thread ID */
	pthread_t tid;
	/* Thread alive flag */
	unsigned int is_alive : 1;
	/* Buffer to store events read from fd */
	char buff_event[EVENT_BUFF_SIZE];
	/* Buffer to construct modify mqtt message */
	char buff_modify[BUFF_SIZE];
	/* Buffer to construct delete mqtt message */
	char buff_delete[BUFF_SIZE];
	struct dir_monitor *next;
};

static inline void __event_message_create(char *buff, int size,
					  const char *tag, const char *status)
{
	snprintf(buff, size, "{\"cmd_code\":\"%s_jsonfile\","
				"\"msg\":{\"tenant\":\"tenant\","
				"\"status\":\"%s\",\"filename\":[",
		 tag, status);
}

static inline void __event_message_update(char *buff, int size,
					  int len, const char *txt)
{
	snprintf(buff + len, size - len, "\"%s\",", txt);
}

static void __event_message_finalize(char *buff, int size, int len)
{
	/* Remove last comma */
	if (buff[len - 1] == ',')
		snprintf(buff + len - 1, size - len, "%s", "]}}");
	else
		snprintf(buff + len, size - len, "%s", "]}}");
}

static void publish_message(struct mosquitto *mosq, const char *topic,
			    char *buff, int size)
{
	int rc;

	__event_message_finalize(buff, size, strlen(buff));

	/* Publish to broker */
	rc = mosquitto_publish(mosq, NULL, topic, strlen(buff),
			       buff, 0, false);
	if (rc != MOSQ_ERR_SUCCESS)
		ERROR("Failed to send to broker : %s",
		      mosquitto_strerror(rc));
}

static int event_read(int fd, int timeout, char *buff,
		      size_t size, size_t *actual)
{
	size_t nbytes = 0;
	struct timeval tve; /* The absolute target time. */
	int init = 1;
	int retval = -1;

	while (nbytes < size) {
		fd_set fds;
		struct timeval tvt;

		/* Clear file descriptor set */
		FD_ZERO(&fds);
		FD_SET(fd, &fds);

		if (timeout > 0) {
			struct timeval now;
			if (gettimeofday(&now, NULL) != 0) {
				DEBUG("gettimeofday() failed.");
				goto exit;
			}
			if (init) {
				/* Calculate the initial timeout. */
				tvt.tv_sec  = (timeout / 1000);
				tvt.tv_usec = (timeout % 1000) * 1000;
				timeradd(&now, &tvt, &tve); /* Calculate the target time. */
			} else {
				/* Calculate the remaining timeout. */
				if (timercmp(&now, &tve, <))
					timersub(&tve, &now, &tvt);
				else
					timerclear(&tvt);
			}
			init = 0;
		} else if (timeout == 0) {
			timerclear(&tvt);
		}

		/* wait for data on file descriptor */
		int rc = select(fd + 1, &fds, NULL, NULL,
				timeout >= 0 ? &tvt : NULL);
		if (rc < 0) {
			if (errno == EINTR)
				continue; /* Retry. */
			ERROR("select() error.");
			goto exit;
		} else if (rc == 0) {
			break; /* Timeout. */
		}

		ssize_t n = read(fd, buff + nbytes, size - nbytes);
		if (n < 0) {
			if (errno == EINTR || errno == EAGAIN)
				continue; /* retry reading. */
			ERROR("read() error.");
			goto exit;
		} else if (n == 0) {
			break;	/* EOF */
		}
		nbytes += n;
	}
	retval = 0;

 exit:
	if (actual)
		*actual = nbytes;
	return retval;
}

static void __handle_events(struct dir_monitor *dm)
{
	char topic[64] = "";
	/* Only directory name needed */
	char *dir_name = basename(dm->dir_path);

	snprintf(topic, 64, "%s%s", TOPIC_PREFIX, dir_name);

	/* Loop while events can be read from inotify file descriptor. */
	for (;;) {
		size_t actual = 0;
		register int i = 0;
		unsigned int buff_delete_dirty = 0;
		unsigned int buff_modify_dirty = 0;

		/* Clear the buffers */
		memset(dm->buff_event, 0, EVENT_BUFF_SIZE);
		memset(dm->buff_modify, 0, BUFF_SIZE);
		memset(dm->buff_delete, 0, BUFF_SIZE);

		/* Read some events. */
		if (event_read(dm->fd, READ_TIMEOUT, dm->buff_event,
			       EVENT_BUFF_SIZE, &actual) != 0 || actual <= 0)
			continue;

		while (i < actual) {
			const struct inotify_event *event =
			(const struct inotify_event *)&(dm->buff_event[i]);

			i += sizeof(struct inotify_event) + event->len;

			/* Not interested in directory events */
			if (!event->len || (event->mask & IN_ISDIR))
				continue;

			if (event->mask & IN_DELETE) {
				if (!buff_delete_dirty)
					__event_message_create(dm->buff_delete,
							       BUFF_SIZE,
							       dir_name,
							       "deleted");
				__event_message_update(dm->buff_delete,
						       BUFF_SIZE,
						       strlen(dm->buff_delete),
						       event->name);
				buff_delete_dirty = 1;

			} else if (event->mask & IN_MODIFY) {
				if (!buff_modify_dirty)
					__event_message_create(dm->buff_modify,
							       BUFF_SIZE,
							       dir_name,
							       "modified");
				__event_message_update(dm->buff_modify,
						       BUFF_SIZE,
						       strlen(dm->buff_modify),
						       event->name);
				buff_modify_dirty = 1;
			}
		}

		/* Publish only on update */
		if (buff_delete_dirty)
			publish_message(dm->client, topic,
					dm->buff_delete, BUFF_SIZE);
		if (buff_modify_dirty)
			publish_message(dm->client, topic,
					dm->buff_modify, BUFF_SIZE);
	}
}

static void monitor_thread_cleanup_handler(void *arg)
{
	struct dir_monitor *dm = (struct dir_monitor *)arg;
	/* Remove mqtt client */
	mosquitto_disconnect(dm->client);
	mosquitto_destroy(dm->client);
	close(dm->fd);
}

static void *monitor_thread(void *arg)
{
	struct dir_monitor *dm = (struct dir_monitor *)arg;

	/* Defer thread cancellation till pushing handler */
	pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
	pthread_setcanceltype(PTHREAD_CANCEL_DEFERRED, NULL);

	/* Push cleanup handler */
	pthread_cleanup_push(monitor_thread_cleanup_handler, dm);
	__handle_events(dm);
	pthread_cleanup_pop(1);
	pthread_exit(NULL);
}

static int check_dir_access(const char *dir_path)
{
	if (access(dir_path, F_OK) != 0) {
		SYSERR("Directory Access: ");
		return -1;
	}
	return 0;
}

int dir_monitor_start(struct dir_monitor **out, const char *dir_path)
{
	int wd;
	struct dir_monitor *dm = NULL;

	if (check_dir_access(dir_path) != 0) {
		ERROR("Failed to access directory.");
		goto exit;
	}

	dm = calloc(1, sizeof(struct dir_monitor));
	if (dm == NULL) {
		ERROR("Memory allocation failure.");
		goto exit;
	}

	dm->is_alive = 0;
	/* dir name for mqtt topic */
	dm->dir_path = strdup(dir_path);
	dm->fd = -1;
	dm->next = NULL;

	/* Create the file descriptor for accessing the inotify API
	 * 0-blocking; IN_NONBLOCK-non-blocking
	 */
	dm->fd = inotify_init1(IN_NONBLOCK);
	if (dm->fd == -1) {
		ERROR("inotify_init1() failed.");
		goto exit_free;
	}

	/* Watch for delete and modify and exclude linked.
	 * IN_CREATE not needed because we would not have to monitor
	 * only empty file created in the directory.
	 * Non-empty file created will also generate IN_MODIFY. So,
	 * IN_CREATE not needed to watch..!!
	 */
	wd = inotify_add_watch(dm->fd, dm->dir_path,
			       IN_DELETE | IN_MODIFY | IN_EXCL_UNLINK);
	if (wd == -1) {
		ERROR("Failed to setup watch on '%s'", dir_path);
		goto exit_close;
	}

	/* Initialize mqtt client */
	dm->client = mosquitto_new(basename(dm->dir_path), true, NULL);
	if (dm->client == NULL) {
		ERROR("mosquitto_new() failed.");
		goto exit_close;
	}

	/* Connect to broker */
	if (mosquitto_connect(dm->client, HOST_ADDRESS, MQTT_PORT,
			      MQTT_CONNECTION_TIMEOUT) != MOSQ_ERR_SUCCESS) {
		ERROR("Failed to connect to broker");
		goto exit_destroy;
	}

	if (pthread_create(&dm->tid, NULL, monitor_thread, dm) != 0) {
		ERROR("Failed to create thread.");
		goto exit_disconnect;
	}
	dm->is_alive = 1;

	*out = dm;
	return 0;

 exit_disconnect:
	mosquitto_disconnect(dm->client);
 exit_destroy:
	mosquitto_destroy(dm->client);
 exit_close:
	close(dm->fd);
 exit_free:
	free(dm->dir_path);
	free(dm);
 exit:
	return -1;
}

void dir_monitor_stop(struct dir_monitor *dm)
{
	if (dm == NULL)
		return;

	/* Cancel and join thread */
	if (dm->is_alive) {
		pthread_cancel(dm->tid);
		pthread_join(dm->tid, NULL);
	}
	if (dm->dir_path)
		free(dm->dir_path);
	free(dm);
	dm = NULL;
}

struct dir_monitor_list {
	struct dir_monitor *head;
};

struct dir_monitor_list *dir_monitor_list_create(void)
{
	struct dir_monitor_list *dm_list = NULL;

	if ((dm_list = malloc(sizeof(struct dir_monitor_list))) == NULL) {
		ERROR("Memory allocation failure");
		return NULL;
	}
	dm_list->head = NULL;
	return dm_list;
}

static int __if_present(struct dir_monitor_list *dm_list, const char *dir_path)
{
	struct dir_monitor **pp = &dm_list->head;

	while (*pp) {
		if (!strcmp((*pp)->dir_path, dir_path))
			return 1;
		pp = &(*pp)->next;
	}
	return 0;
}

static void __add_to_list(struct dir_monitor **head_ptr,
			  struct dir_monitor *prev,
			  struct dir_monitor *new)
{
	struct dir_monitor **pp = head_ptr;

	while (*pp && *pp != prev)
		pp = &(*pp)->next;

	new->next = *pp;
	*pp = new;
}

static inline void add_to_dir_mon_list(struct dir_monitor **head_ptr,
				       struct dir_monitor *new)
{
	/* Add to the tail */
	__add_to_list(head_ptr, NULL, new);
}

int dir_monitor_list_add(struct dir_monitor_list *dm_list,
			 const char *dir_path)
{
	struct dir_monitor *dm = NULL;

	if (__if_present(dm_list, dir_path)) {
		WARN("'%s' already in the dir monitor list. Not adding...!!",
		     dir_path);
		return -1;
	}

	if (dir_monitor_start(&dm, dir_path) != 0) {
		DEBUG("Failed to monitor %s directory.", dir_path);
		return -1;
	}

	INFO("'%s' added to the dir monitor list", dir_path);
	add_to_dir_mon_list(&dm_list->head, dm);
	return 0;
}

int dir_monitor_list_remove(struct dir_monitor_list *dm_list,
			    const char *dir_path)
{
	struct dir_monitor **pp = &dm_list->head;

	while (*pp) {
		if (!strcmp((*pp)->dir_path, dir_path)) {
			struct dir_monitor *tmp = *pp;
			INFO("Removing '%s' from dir monitor list", dir_path);
			*pp = tmp->next;
			tmp->next = NULL;
			dir_monitor_stop(tmp);
			return 0;
		}
		pp = &(*pp)->next;
	}

	WARN("'%s' not present in dir monitor list", dir_path);
	return -1;
}

void dir_monitor_list_destroy(struct dir_monitor_list *dm_list)
{
	struct dir_monitor *p = dm_list->head;

	while (p != NULL) {
		struct dir_monitor *tmp = p;
		INFO("Removing '%s' from dir monitor list", tmp->dir_path);
		p = tmp->next;
		dir_monitor_stop(tmp);
	}
	free(dm_list);
}
