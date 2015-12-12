#include "logger.h"
#include "server.h"
#include "config.h"
#include "worker.h"
#include "proto.h"
#include "fsm.h"

#include <fcntl.h>
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <signal.h>

static int hostname_to_ip(const char *hostname, char ip[32])
{
	struct hostent *he = NULL;
	struct in_addr **addr_list = NULL;
	int i = 0;

	if ((he = gethostbyname(hostname)) == NULL) {
		// get the host info
		log_error("gethostbyname failed for %s (%s)", hostname, strerror(errno));
		return -1;
	}

	addr_list = (struct in_addr **)he->h_addr_list;
	for (i = 0; addr_list[i] != NULL; i++) {
		//Return the first one;
		strcpy(ip , inet_ntoa(*addr_list[i]) );
		log_trace("Address %s was resolved to %s", hostname, ip);
		return 0;
	}

	log_error("Can't resolve %s to ip", hostname);
	return -1;
}

static void handle_sigchld(int sig) {
	pid_t child_pid = 0;
	int status = 0;
	while ((child_pid = waitpid(-1, &status, WNOHANG))) {
		if (WIFEXITED(status))
			log_warn("Child exited normally, status = %d", WEXITSTATUS(status));
		else {
			char stat_str[128] = "";
			if (WIFSIGNALED(status))
				snprintf(stat_str, sizeof(stat_str), "process was killed, signal = %d", WTERMSIG(status));
			else
				snprintf(stat_str, sizeof(stat_str), "status = 0x%x", status);

			log_error("Child exited abnormally, %s", stat_str);
		}

		destroy_worker(child_pid);
	}
}

static int mk_server() {
	char ip_addr[32] = "";
	if (hostname_to_ip(get_opt_listen_host(), ip_addr) != 0)
		return -1;

	struct sockaddr_in addr;
	memset(&addr, 0, sizeof(addr));

	uint16_t port = 0;
	if (get_opt_listen_port() <= 0 || get_opt_listen_port() >= (1u << (sizeof(port) * 8))) {
		log_error("Invalid port value: %d", get_opt_listen_port());
		return -1;
	}

	port = (uint16_t)get_opt_listen_port();

	// TODO: IPv6
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = inet_addr(ip_addr);
	addr.sin_port = htons(port);

	int sock = socket(AF_INET, SOCK_STREAM, 0);
	if (sock < 0) {
		log_error("Can't create server socket: %s", strerror(errno));
		return -1;
	}

	int val = 1;
	if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &val, sizeof(val)) != 0) {
		log_error("Can't setsockopt: %s", strerror(errno));
		return -1;
	}

	if (bind(sock, (struct sockaddr *)&addr, sizeof(struct sockaddr)) != 0) {
		log_error("Can't bind on %s:%d: %s", get_opt_listen_host(), get_opt_listen_port(), strerror(errno));
		close(sock);
		return -1;
	}

	drop_privileges(get_opt_user(), get_opt_group(), get_opt_root_dir());

	// XXX: backlog is hardcoded
	if (listen(sock, 3) != 0) {
		log_error("Can't start listen on %s:%d: %s", get_opt_listen_host(), get_opt_listen_port(), strerror(errno));
		close(sock);
		return -1;
	}

	struct sigaction sa;
	sa.sa_handler = &handle_sigchld;
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = SA_RESTART | SA_NOCLDSTOP;
	if (sigaction(SIGCHLD, &sa, NULL) != 0) {
		log_error("Can't set sigaction: %s", strerror(errno));
		close(sock);
		return -1;
	}

	// where socketfd is the socket you want to make non-blocking
	int status = fcntl(sock, F_SETFL, fcntl(sock, F_GETFL, 0) | O_NONBLOCK);

	if (status == -1) {
		log_error("Can't make server socket non-block: %s", strerror(errno));
		close(sock);
		return -1;
	}

	log_info("Listening on %s:%d", get_opt_listen_host(), get_opt_listen_port());

	return sock;
}

#define STATES_LIST(ARG, _) \
	_(ARG, WAIT_CONN, FSM_INIT_STATE) \
	_(ARG, ERROR) \
	_(ARG, PROCESS_FDS) \
	_(ARG, PROCESS_ERROR_FD) \
	_(ARG, PROCESS_SERVER_FD) \
	_(ARG, STOP_SERVER, FSM_LAST_STATE)

struct server_status_t;
FSM(server, STATES_LIST, struct server_status_t *);

struct fds_process_status_t {
	uint8_t error_fds_processed;

	int current_socket;
};

struct server_error_info_t {
	int error_socket;
	int error_code;
	char err_msg[512];
	FSM_STATE_TYPE(server) next_state;
};

struct server_status_t {
	int server_socket;

	fd_set active_fd_set;
	fd_set error_fd_set;

	struct server_error_info_t error_info;
	struct fds_process_status_t fds_process_status;
};

FSM_CB(server, WAIT_CONN, server_status) {
	if (select(FD_SETSIZE, &server_status->active_fd_set, NULL, &server_status->error_fd_set, NULL) < 0) {
		log_error("Select failed: %s", strerror(errno));
		return ERROR;
	}

	memset(&server_status->fds_process_status, 0, sizeof(server_status->fds_process_status));
	return PROCESS_FDS;
}

FSM_CB(server, PROCESS_FDS, server_status) {
	struct fds_process_status_t *status = &server_status->fds_process_status;

	if (!status->error_fds_processed && ++status->current_socket >= FD_SETSIZE) {
		log_trace("Error fds processed");
		status->error_fds_processed = 1;
		return PROCESS_SERVER_FD;
	} else if (status->error_fds_processed)
		return WAIT_CONN;

	return PROCESS_ERROR_FD;
}

FSM_CB(server, PROCESS_ERROR_FD, server_status) {
	if (FD_ISSET(server_status->fds_process_status.current_socket, &server_status->error_fd_set)) {
		close(server_status->fds_process_status.current_socket);
		FD_CLR(server_status->fds_process_status.current_socket, &server_status->error_fd_set);
	}

	return PROCESS_FDS;
}

FSM_CB(server, PROCESS_SERVER_FD, server_status) {
	if (!FD_ISSET(server_status->server_socket, &server_status->active_fd_set))
		return PROCESS_FDS;

	// Establish connection with client
	struct sockaddr_in clientname;
	socklen_t size = sizeof(clientname);

	int new = accept(server_status->server_socket, (struct sockaddr *) &clientname, &size);
	if (new < 0) {
		log_error("Can't accept new client: %s", strerror(errno));
		return PROCESS_FDS;
	}

	log_info("Connection with %s:%d was established", inet_ntoa(clientname.sin_addr), ntohs(clientname.sin_port));
	if (mk_worker(new) != 0) {
		struct server_error_info_t *error_info = &server_status->error_info;
		error_info->error_socket = new;
		error_info->next_state = PROCESS_FDS;
		snprintf(error_info->err_msg, sizeof(error_info->err_msg), "no more workers");

		log_error("Can't start worker for %s:%d, send error message and destroy connection",
				inet_ntoa(clientname.sin_addr), ntohs(clientname.sin_port));

		return ERROR;
	}

	return PROCESS_FDS;
}

FSM_CB(server, ERROR, server_status) {
	void reset_err_info(struct server_error_info_t **err_info) {
		memset(*err_info, 0, sizeof(**err_info));
	}

	struct server_error_info_t *error_info __attribute__((cleanup(reset_err_info))) = &server_status->error_info;
	if (error_info->error_socket) {
		if (error_info->err_msg[0] == '\0')
			snprintf(error_info->err_msg, sizeof(error_info->err_msg), "unknown error");
		if (error_info->error_code == 0)
			error_info->error_code = 500;

		log_error("Sending error message '%s' to socket %d", error_info->err_msg, error_info->error_socket);

		smtp_send_error(error_info->error_socket, error_info->error_code, error_info->err_msg);

		shutdown(error_info->error_socket, SHUT_WR);
		FD_SET(error_info->error_socket, &server_status->error_fd_set);

		if (error_info->next_state)
			return error_info->next_state;

		return WAIT_CONN;
	}

	log_error("Error happen in server loop. Shutdown server");
	return STOP_SERVER;
}

static void run_loop(int server_socket) {
	struct server_status_t server_status;

	memset(&server_status, 0, sizeof(server_status));
	server_status.server_socket = server_socket;

	FD_ZERO(&server_status.active_fd_set);
	FD_ZERO(&server_status.error_fd_set);

	FD_SET(server_socket, &server_status.active_fd_set);

	FSM_RUN(server, &server_status);
}

void run_server() {
	int server_socket = mk_server();
	if (server_socket < 0) {
		log_error("Can't create server. Stop");
		return;
	}

	run_loop(server_socket);
}
