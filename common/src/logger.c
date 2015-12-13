#include "logger.h"
#include "stdio.h"
#include "stdarg.h"
#include "common.h"

#include <time.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/socket.h>
#include <sys/select.h>

#ifndef LOG_FILE_FLUSH_TIME
// in seconds
#	define LOG_FILE_FLUSH_TIME 1
#endif

#ifdef __MACH__ // fucking Machintosh
#include <mach/clock.h>
#include <mach/mach.h>

void clock_gettime_impl(struct timespec *ts) {
	clock_serv_t cclock;
	mach_timespec_t mts;
	host_get_clock_service(mach_host_self(), CALENDAR_CLOCK, &cclock);
	clock_get_time(cclock, &mts);
	mach_port_deallocate(mach_task_self(), cclock);
	ts->tv_sec = mts.tv_sec;
	ts->tv_nsec = mts.tv_nsec;
}

#else

void clock_gettime_impl(struct timespec *ts) {
	clock_gettime(CLOCK_REALTIME, ts);
}

#endif

static int current_log_level = -1;
struct logger_connection_t {
	pid_t pid;

	char buf[4096]; // max log message len is 4kb
	size_t offset;

	int logger_sock;
	int remote_sock;
};

struct logger_status_t {
	size_t sockets_count;
	struct logger_connection_t *conns;
	FILE *log_f;

	struct logger_connection_t *cur_conn;

	char whoami[16];
};

static struct logger_status_t logger_status;

static void write_to_log_impl(FILE *f, const char *msg, int size) {
	fprintf(f, "%.*s", size, msg);
}

static void set_whoami(char prefix) {
	snprintf(logger_status.whoami, sizeof(logger_status.whoami), "%c-%d", prefix, getpid());
}

static void write_to_log(const struct logger_connection_t *conn, const char *msg, size_t size) {
	if (logger_status.whoami[0] == '\0')
		set_whoami('I');

	char *whoami = logger_status.whoami;
	FILE *f = logger_status.log_f;

	if (!f)
		f = stderr;

	struct timespec time;
	clock_gettime_impl(&time);

	char time_str[32];
	struct tm tm;
	localtime_r(&time.tv_sec, &tm);

	size_t ret = strftime(time_str, sizeof(time_str), "%d.%m.%Y %H:%M:%S", &tm);
	snprintf(time_str + ret, sizeof(time_str) - ret, ".%.06ld", time.tv_nsec / 1000);

	const size_t whoami_size = 9; // 5 chars for pid, 2 chars for spaces and some extra space
	size_t left = whoami_size - strlen(whoami);
	size_t right = left - left / 2;
	left -= right;

	char spaces[10];
	memset(spaces, ' ', sizeof(spaces));

	char str[8192];
	int printed = snprintf(str, sizeof(str), "%s [%.*s%s%.*s] %.*s\n", time_str, (int)left, spaces, whoami, (int)right, spaces, (int)size, msg);
	if (printed < 0) {
		log_error("Can't snprintf");
		return;
	}

	if (conn)
		write(conn->logger_sock, str, (size_t)printed);
	else
		write_to_log_impl(f, str, printed); // logger process
}

static void receive_msg(struct logger_connection_t *conn) {
	if (sizeof(conn->buf) <= conn->offset) {
		log_error("Too large amout of log data came. Flush log");
		write_to_log(conn, conn->buf, sizeof(conn->buf));
		conn->offset = 0;
	}

	ssize_t received = read(conn->remote_sock, conn->buf + conn->offset, sizeof(conn->buf) - conn->offset);
	if (received <= 0) {
		log_warn("Read() failed: %s", strerror(errno));
		return;
	}

	conn->offset += (size_t)received;

	while (1) {
		char *delim = strnstr(conn->buf, "\n", conn->offset);
		if (delim) {
			write_to_log_impl(logger_status.log_f, conn->buf, (int)(delim - conn->buf + 1));
			conn->offset -= (size_t)(delim - conn->buf + 1);
			memmove(conn->buf, delim + 1, conn->offset);
		} else
			break;
	}
}

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

	char str[4096] = "";

	va_list ap;
	va_start(ap, fmt);
	int printed = vsnprintf(str, sizeof(str), fmt, ap);
	va_end(ap);

	if (printed < 0)
		log_error("vsnprintf failed");
	else
		write_to_log(logger_status.cur_conn, str, printed);
}

static __attribute__((destructor))
void ___destruct() {
	deinitialize_logger();
}

void deinitialize_logger() {
	if (logger_status.conns) {
		int i = 0;
		for (; i < logger_status.sockets_count; ++i) {
			struct logger_connection_t *conn = logger_status.conns + i;

			if (conn->logger_sock)
				close(conn->logger_sock);
			if (conn->remote_sock)
				close(conn->remote_sock);
		}

		fclose(logger_status.log_f);

		free(logger_status.conns);
		memset(&logger_status, 0, sizeof(logger_status));
	}
}

int init_pipes(int n_processes) {
	if (n_processes <= 0) {
		log_error("Invalid number of processes");
		return -1;
	}

	deinitialize_logger();
	logger_status.sockets_count = (unsigned)n_processes + 1; // +1 for a master process

	size_t n_bytes = sizeof(struct logger_connection_t) * logger_status.sockets_count;
	logger_status.conns = (struct logger_connection_t *)malloc(n_bytes);
	assert(logger_status.conns);

	memset(logger_status.conns, 0, n_bytes);

	log_trace("Trying to init pipes");

	int i = 0;
	for (; i < logger_status.sockets_count; ++i) {
		// with master process
		int sockets[2] = { 0, 0 };
		if (pipe(sockets) < 0) {
			log_error("Can't init pipe: %s", strerror(errno));
			return -1;
		}

		struct logger_connection_t *conn = logger_status.conns + i;
		conn->remote_sock = sockets[0];
		conn->logger_sock = sockets[1];
	}

	log_trace("Socket pairs inited successfully");

	return 0;
}

static void close_logger_socks() {
	int i = 0;
	log_trace("Closing logger sockets");
	for (; i < logger_status.sockets_count; ++i) {
		close(logger_status.conns[i].logger_sock);
		logger_status.conns[i].logger_sock = -1;
	}
	log_trace("Closing done");
}

static void close_remote_socks() {
	int i = 0;
	log_trace("Closing remote sockets");
	for (; i < logger_status.sockets_count; ++i) {
		close(logger_status.conns[i].remote_sock);
		logger_status.conns[i].remote_sock = -1;
	}
	log_trace("Closing done");
}

static void run_logger_loop() {
	fd_set active;
	struct timeval timeout = {
		.tv_sec = LOG_FILE_FLUSH_TIME,
		.tv_usec = 0,
	};

	struct timeval *timeout_ptr = NULL;
	if (logger_status.log_f != stderr)
		timeout_ptr = &timeout;

	while (1) {
		FD_ZERO(&active);

		int i = 0;
		for (; i < logger_status.sockets_count; ++i) {
			FD_SET(logger_status.conns[i].remote_sock, &active);
		}

		int res = select(FD_SETSIZE, &active, NULL, NULL, timeout_ptr);
		if (res < 0) {
			log_info("Select failed: %s", strerror(errno));
			continue;
		} else if (res == 0 && timeout_ptr) {
			// timeout
			timeout.tv_sec = LOG_FILE_FLUSH_TIME;
			fflush(logger_status.log_f);
		}

		for (i = 0; i < logger_status.sockets_count; ++i) {
			struct logger_connection_t *conn = logger_status.conns + i;
			if (FD_ISSET(conn->remote_sock, &active)) {
				receive_msg(conn);
			}
		}
	}
}

static void connect_to_logger(int index) {
	assert(index < logger_status.sockets_count);

	logger_status.cur_conn = logger_status.conns + index;
}

static pid_t __logger_pid = -1;

pid_t logger_pid() {
	return __logger_pid;
}

int init_logger(int n_processes, const char *log_file, const char *user, const char *group) {
	log_trace("Trying to init logger for a process");

	if (init_pipes(n_processes) < 0)
		return -1;

	FILE *f = NULL;
	if (log_file) {
		log_trace("Trying to open log file %s", log_file);
		f = fopen(log_file, "a+");
		if (!f) {
			log_error("Can't open log file: %s", strerror(errno));
			return -1;
		}
	} else
		f = stderr;

	__logger_pid = fork();
	if (__logger_pid < 0) {
		log_error("Can't fork() logger: %s", strerror(errno));
		return -1;
	}

	if (__logger_pid == 0) {
		// child -> logger
		logger_status.log_f = f;
		set_whoami('L');

		close_logger_socks();
		drop_privileges(user, group, NULL);

		run_logger_loop();
		exit(0);
		return -1;
	}

	connect_to_logger(n_processes);
	set_whoami('M');

	// master process
	if (log_file)
		fclose(f);
	close_remote_socks();

	return 0;
}

int logger_sock() {
	return logger_status.cur_conn->logger_sock;
}

int reinit_logger(int index) {
	// Child process should write logs into another socket
	// XXX: this function should be called by child

	assert(index < logger_status.sockets_count);
	connect_to_logger(index);
	set_whoami('W');

	int i = 0;
	for (; i < logger_status.sockets_count; ++i) {
		if (i != index)
			close(logger_status.conns[i].logger_sock);
	}

	return 0;
}
