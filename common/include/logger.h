#ifndef __LOGGER_H__
#define __LOGGER_H__

#include "common.h"
#include <unistd.h>

#define __LOG_ERR	'E'
#define __LOG_WARN	'W'
#define __LOG_INFO	'I'
#define __LOG_DEBUG	'D'
#define __LOG_TRACE	'T'

enum {
	LOG_ERROR = 1,
	LOG_WARN,
	LOG_INFO,
	LOG_DEBUG,
	LOG_TRACE,
	LOG_MAX
};

#ifdef LOG_PATH
#	define _log_impl(lvl, sym, fmt, ...) log_impl(lvl, "[ %c ] " fmt " at %s:%d", sym, ##__VA_ARGS__, __FILE__, __LINE__)
#else
#	define _log_impl(lvl, sym, fmt, ...) log_impl(lvl, "[ %c ] " fmt, sym, ##__VA_ARGS__)
#endif

#define log_error(fmt, ...)	_log_impl(LOG_ERROR, __LOG_ERR, fmt, ##__VA_ARGS__)
#define log_warn(fmt, ...)	_log_impl(LOG_WARN, __LOG_WARN, fmt, ##__VA_ARGS__)
#define log_info(fmt, ...)	_log_impl(LOG_INFO, __LOG_INFO, fmt, ##__VA_ARGS__)
#define log_debug(fmt, ...)	_log_impl(LOG_DEBUG, __LOG_DEBUG, fmt, ##__VA_ARGS__)
#define log_trace(fmt, ...)	_log_impl(LOG_TRACE, __LOG_TRACE, fmt, ##__VA_ARGS__)

int set_log_level(int lvl);
void log_impl(int lvl, const char *fmt, ...) __ATTR_FORMAT__(printf, 2, 3);

int reinit_logger(int index);
int init_logger(int n_processes, const char *logfile, const char *user, const char *group);
pid_t logger_pid();
int logger_sock();

void deinitialize_logger();

#endif // __LOGGER_H__
