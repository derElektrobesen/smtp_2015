#include "logger.h"
#include "server.h"
#include "config.h"

#include <fcntl.h>
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
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
		return 0;
	}

	log_error("Can't resolve %s to ip", hostname);
	return -1;
}

static int mk_server() {
	char ip_addr[32] = "";
	if (hostname_to_ip(get_opt_listen_host(), ip_addr) != 0)
		return -1;

	struct sockaddr_in addr;
	memset(&addr, 0, sizeof(addr));

	uint16_t port = 0;
	if (get_opt_listen_port() <= 0 || get_opt_listen_port() >= (1u << sizeof(port))) {
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
	sa.sa_handler = SIG_IGN;
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = 0;
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

	return sock;
}

// Struct describes a workers queue
struct queue_t {
	
};

void run_server() {
	int server_socket = mk_server();
	if (server_socket < 0) {
		log_error("Can't create server. Stop");
		return;
	}

	int server_sock = mk_server();
	if (server_sock <= 0)
		return; // error already printed
}