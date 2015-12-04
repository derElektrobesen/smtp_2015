#ifndef __LOGGER_H__
#define __LOGGER_H__

#include "common.h"

#define LOG_ERR		'E'
#define LOG_WARN	'W'
#define LOG_INFO	'I'
#define LOG_DEBUG	'D'
#define LOG_TRACE	'T'

#ifdef LOG_PATH
#	define _log_impl(lvl, sym, fmt, args...) log_impl(lvl, "[%c] " fmt, sym, args)
#else
#	define _log_impl(lvl, sym, fmt, args...) log_impl(lvl, "[%c] %s:%d " fmt, sym, __FILE__, __LINE__, args)
#endif

#define log_error(fmt, args)	_log_impl(1, LOG_ERR, fmt, args)
#define log_warn(fmt, args)	_log_impl(2, LOG_WARN, fmt, args)
#define log_info(fmt, args)	_log_impl(3, LOG_INFO, fmt, args)
#define log_debug(fmt, args)	_log_impl(4, LOG_DEBUG, fmt, args)
#define log_trace(fmt, args)	_log_impl(5, LOG_TRACE, fmt, args)

int set_log_level(int lvl);
void log_impl(int lvl, const char *fmt, ...) __ATTR_FORMAT__(printf, 2, 3);

#endif // __LOGGER_H__
