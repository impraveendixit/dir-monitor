#ifndef INTERNALS_H_INCLUDED
#define INTERNALS_H_INCLUDED

/* Indirect stringification.  Doing two levels allows the parameter to be a
 * macro itself.  For example, compile with -DFOO=bar, __stringify(FOO)
 * converts to "bar".
 */
#define __stringify_1(x...)	#x
#define stringify(x...)	__stringify_1(x)

/* Host address */
#ifndef CONFIG_HAVE_HOST_ADDRESS
	#define HOST_ADDRESS	stringify(localhost)
#else
	#define HOST_ADDRESS	stringify(CONFIG_HAVE_HOST_ADDRESS)
#endif

#define MQTT_PORT	1883

#define TOPIC_PREFIX	stringify(thor/sync/)

/* Connection timeout in seconds */
#define MQTT_CONNECTION_TIMEOUT	36000

#endif /* INTERNALS_H_INCLUDED */
