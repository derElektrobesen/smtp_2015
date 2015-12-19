#include "worker.h"
#include "common.h"
#include "config.h"
#include "logger.h"
#include "proto.h"

#include <stdlib.h>
#include <assert.h>
#include <sys/resource.h>
#include <unistd.h>
#include <errno.h>

struct worker_t {
	pid_t pid;
	int sock;
	uint8_t busy;
	int index;
};

static struct worker_t *workers = NULL;

static void init_workers() {
	if (workers)
		return;

	log_trace("Trying to initialize workers list");

	// number of workers was checked in main()
	workers = (struct worker_t *)malloc((unsigned)get_opt_n_workers() * sizeof(struct worker_t));
	assert(workers);

	memset(workers, 0, (unsigned)get_opt_n_workers() * sizeof(struct worker_t));

	int i = 0;
	for (; i < get_opt_n_workers(); ++i) {
		workers[i].index = i;
	}
}

static __attribute__((destructor))
void deinit_workers() {
	log_trace("Trying to deinitialize workers");

	if (workers)
		free(workers);
}

static void close_opened_descriptors(int *except, int except_count) {
	static rlim_t n_fds = 0;
	if (!n_fds) {
		struct rlimit rl;
		int ret = getrlimit(RLIMIT_NOFILE, &rl);

		if (ret < 0) {
			n_fds = 1024;
			log_error("Can't get rlimit: %s, set n_fds to %lld", strerror(errno), n_fds);
		} else {
			n_fds = rl.rlim_cur;
			log_info("Number of file descriptors was set to %lld", n_fds);
		}
	}

	int i = 0;
	for (; i < n_fds; ++i) {
		int j = 0;
		uint8_t skip = 0;

		for (; !skip && j < except_count; ++j) {
			if (except[j] == i)
				skip = 1;
		}

		if (!skip)
			close(i);
	}
}

static struct worker_t *get_worker(int sock) {
	int i = 0;
	struct worker_t *worker = NULL;
	for (; !worker && i < get_opt_n_workers(); ++i) {
		if (!workers[i].busy)
			worker = workers + i;
	}

	if (worker) {
		worker->busy = 1;
		worker->sock = sock;
	}

	return worker;
}

static int init_worker(struct worker_t *worker) {
	pid_t pid = getpid();

	log_info("New worker created, pid = %d", pid);
	worker->pid = pid;

	if (reinit_logger(worker->index) != 0) {
		log_error("Can't reinit logger in child");
		return -1;
	}

	int except[] = { worker->sock, logger_sock() };
	close_opened_descriptors(except, sizeof(except) / sizeof(*except));

	return 0;
}

static void run_worker(struct worker_t *worker) {
	smtp_communicate_with_client(worker->sock);
}

static int destroy_worker_impl(struct worker_t *worker) {
	memset(worker, 0, sizeof(*worker));
	return 0;
}

static int mk_worker_impl(struct worker_t *worker) {
	pid_t pid = fork();
	srand((unsigned)rand());

	if (pid == 0) {
		// child created
		if (init_worker(worker) == 0) {
			run_worker(worker);

			log_info("Worker finish its work");

			// TODO: don't destroy process at end.
			exit(0);
			return 0; // just in case
		}

		log_error("Can't init worker");
	} else if (pid) {
		worker->pid = pid;
		log_info("New worker created, pid = %d, close connection to %d", pid, worker->sock);

		close(worker->sock);

		return 0;
	} else
		log_error("Can't fork: %s", strerror(errno));

	destroy_worker_impl(worker);
	return -1;
}

int mk_worker(int client_sock) {
	if (!workers)
		init_workers();

	log_info("Trying to create a worker for connection %d", client_sock);

	struct worker_t *worker = get_worker(client_sock);
	if (!worker) {
		log_error("Too many clients accepted. Decline client %d", client_sock);
		return -1;
	}

	return mk_worker_impl(worker);
}

int destroy_worker(int pid) {
	assert(workers);

	int i = 0;
	for (; i < get_opt_n_workers(); ++i) {
		if (workers[i].pid == pid && workers[i].busy == 1) {
			log_trace("Found worker #%d with pid %d to destroy", i, pid);
			return destroy_worker_impl(workers + i);
		}
	}

	log_error("Can't find worker with pid %d", pid);
	return -1;
}
