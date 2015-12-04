#include "logger.h"
#include "stdio.h"
#include "stdarg.h"

static int current_log_level = -1;

int set_log_level(int lvl) {
	if (lvl <= 0 || lvl > 5)
		return -1;

	current_log_level = lvl;
	log_debug("Set log level to %d", lvl);

	return 0;
}

void log_impl(int lvl, const char *fmt, ...) {
	assert(current_log_level != -1);

	if (current_log_level < lvl)
		return;

	va_list ap;
	va_start(ap, fmt);

	vprintf(fmt, ap);

	va_end(ap);
}
