#ifndef DEBUG_H_INCLUDED
#define DEBUG_H_INCLUDED

#include <stdio.h>
#include <errno.h>
#include <string.h>

#define LOG_LEVEL_OFF	0
#define LOG_LEVEL_FATAL	1
#define LOG_LEVEL_ERROR	2
#define LOG_LEVEL_WARN	3
#define LOG_LEVEL_INFO	4
#define LOG_LEVEL_DEBUG	5

/* Bash color codes */
#define COLOR_BLACK		"\e[0;30m"
#define COLOR_DARK_GREY		"\e[1;30m"
#define COLOR_RED		"\e[0;31m"
#define COLOR_LIGHT_RED		"\e[1;31m"
#define COLOR_GREEN		"\e[0;32m"
#define COLOR_LIGHT_GREEN	"\e[1;32m"
#define COLOR_ORANGE		"\e[0;33m"
#define COLOR_YELLOW		"\e[1;33m"
#define COLOR_BLUE		"\e[0;34m"
#define COLOR_LIGHT_BLUE	"\e[1;34m"
#define COLOR_PURPLE		"\e[0;35m"
#define COLOR_LIGHT_PURPLE	"\e[1;35m"
#define COLOR_CYAN		"\e[0;36m"
#define COLOR_LIGHT_CYAN	"\e[1;36m"
#define COLOR_LIGHT_GREY	"\e[0;37m"
#define COLOR_WHITE		"\e[1;37m"
#define COLOR_NONE		"\e[0;0m"

#if defined(__STDC_VERSION__) && (__STDC_VERSION__ >= 199901L)
	#define FUNC	__func__
#else
	#define FUNC	__FUNCTION__
#endif

#ifndef CONFIG_LOG_LEVEL
#define CONFIG_LOG_LEVEL	LOG_LEVEL_DEBUG
#endif

#define __log(level, tag, M, ...) \
do { \
	if (level <= CONFIG_LOG_LEVEL) { \
		if (level == LOG_LEVEL_INFO) \
			fprintf(stderr, tag" "M"\n", ##__VA_ARGS__); \
		else \
			fprintf(stderr, tag "[%s:%d:%s] " M "\n", \
				__FILE__, __LINE__, FUNC, ##__VA_ARGS__); \
		fflush(stderr); \
	} \
} while (0)

#define fatal_tag	COLOR_LIGHT_RED"[FATAL]"COLOR_NONE
#define error_tag	COLOR_RED"[ERROR]"COLOR_NONE
#define warn_tag	COLOR_YELLOW"[WARN]"COLOR_NONE
#define info_tag	COLOR_CYAN"[INFO]"COLOR_NONE
#define debug_tag	COLOR_LIGHT_BLUE"[DEBUG]"COLOR_NONE

#define FATAL(M, ...)	__log(LOG_LEVEL_FATAL, fatal_tag, M, ##__VA_ARGS__)
#define ERROR(M, ...)	__log(LOG_LEVEL_ERROR, error_tag, M, ##__VA_ARGS__)
#define WARN(M, ...)	__log(LOG_LEVEL_WARN, warn_tag, M, ##__VA_ARGS__)
#define INFO(M, ...)	__log(LOG_LEVEL_INFO, info_tag, M, ##__VA_ARGS__)
#define DEBUG(M, ...)	__log(LOG_LEVEL_DEBUG, debug_tag, M, ##__VA_ARGS__)

#define SYSERR(M, ...) \
	ERROR(COLOR_RED"[%s] "COLOR_NONE M, \
	      ((!errno) ? "UNKNOWN_ERROR" : strerror(errno)), ##__VA_ARGS__)

#endif	/* DEBUG_H_INCLUDED */
