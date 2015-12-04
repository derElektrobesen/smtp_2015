#ifndef __LOGGER_H__
#define __LOGGER_H__

#include "common.h"

#define LOG_ERR		'E'
#define LOG_WARN	'W'
#define LOG_INFO	'I'
#define LOG_DEBUG	'D'
#define LOG_TRACE	'T'

#ifdef LOG_PATH
#	define _log_impl(lvl, sym, fmt, ...) log_impl(lvl, "[%c] " fmt "\n", sym, ##__VA_ARGS__)
#else
#	define _log_impl(lvl, sym, fmt, ...) log_impl(lvl, "[%c] %s:%d " fmt "\n", sym, __FILE__, __LINE__, ##__VA_ARGS__)
#endif

#define log_error(fmt, ...)	_log_impl(1, LOG_ERR, fmt, ##__VA_ARGS__)
#define log_warn(fmt, ...)	_log_impl(2, LOG_WARN, fmt, ##__VA_ARGS__)
#define log_info(fmt, ...)	_log_impl(3, LOG_INFO, fmt, ##__VA_ARGS__)
#define log_debug(fmt, ...)	_log_impl(4, LOG_DEBUG, fmt, ##__VA_ARGS__)
#define log_trace(fmt, ...)	_log_impl(5, LOG_TRACE, fmt, ##__VA_ARGS__)

int set_log_level(int lvl);
void log_impl(int lvl, const char *fmt, ...) __ATTR_FORMAT__(printf, 2, 3);

#endif // __LOGGER_H__
