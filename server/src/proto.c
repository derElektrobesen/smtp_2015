#include "proto.h"
#include "logger.h"
#include "fsm.h"

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <assert.h>
#include <errno.h>
#include <sys/socket.h>
#include <sys/select.h>

#if !defined(VERSION) || !defined(BUILD_YEAR) || !defined(DEVELOPERS) || !defined(PROJECT)
#	error "Pass constants above via makefile"
#endif

#ifndef BLOCK_SIZE
#	define BLOCK_SIZE 4096
#endif

enum {
	ST_OK = 220,
	ST_SRV_ERR = 503,
	ST_CLOSE_AFTER_CONNECT = 554,
};

static void send_response_ex(int sock, const char *msg, size_t msg_size) {
	log_trace("Sending to client %d: '%.*s'", sock, (int)msg_size, msg);
	write(sock, msg, strlen(msg));
	write(sock, STRSZ("\r\n"));
}

static int send_response(int sock, int status, const char *msg) {
	char real_msg[4096] = "";
	int printed = snprintf(real_msg, sizeof(real_msg), "%d %s", status, msg);

	if (printed >= sizeof(real_msg))
		log_error("Too long error message: %s, maximum %zu chars are expected", msg, sizeof(real_msg));
	else if (printed < 0) {
		log_error("Can't send error message: snprintf failed");
		return -1;
	}

	send_response_ex(sock, real_msg, (size_t)printed);
	return 0;
}

struct client_error_t {
	int status;
	const char *msg;
};

struct buffer_t {
	size_t allocated;
	size_t used;
	char *buf;
};

static void init_buffer(struct buffer_t *buf) {
	buf->allocated = BLOCK_SIZE;
	buf->used = 0;
	buf->buf = (char *)malloc(buf->allocated);

	assert(buf->buf);
}

static void expand_buffer(struct buffer_t *buf) {
	buf->allocated *= 2;
	buf->buf = (char *)realloc(buf->buf, buf->allocated);
}

#define STATES(ARG, _) \
	_(ARG, WELCOME_CLIENT, FSM_INIT_STATE) \
	_(ARG, WAIT_DATA) \
	_(ARG, READ_DATA) \
	_(ARG, WAIT_COMMAND) \
	_(ARG, CLOSE_CLIENT) \
	_(ARG, FREE_BUF) \
	_(ARG, SHUTDOWN, FSM_LAST_STATE)

struct client_t;
FSM(smtp, STATES, struct client_t *);

struct client_t {
	int sock;

	const char *delimiter;

	struct buffer_t buffer;
	struct client_error_t cli_error;

	FSM_STATE_TYPE(smtp) next_state;
};

FSM_CB(smtp, WELCOME_CLIENT, cli) {
	int status = ST_OK;
	if (cli->cli_error.msg) {
		status = ST_CLOSE_AFTER_CONNECT;
	}

	send_response(cli->sock, status,  PROJECT ", v" VERSION ". Developed by " DEVELOPERS ", " BUILD_YEAR);
	if (cli->cli_error.msg) {
		send_response(cli->sock, cli->cli_error.status, cli->cli_error.msg);
		return CLOSE_CLIENT;
	}

	return WAIT_COMMAND;
}

FSM_CB(smtp, WAIT_COMMAND, cli) {
	return WAIT_DATA;
}

FSM_CB(smtp, WAIT_DATA, cli) {
	fd_set read_set, err_set;

	FD_ZERO(&read_set);
	FD_SET(cli->sock, &read_set);

	FD_ZERO(&err_set);
	FD_SET(cli->sock, &err_set);

	int ret = select(FD_SETSIZE, &read_set, NULL, &err_set, NULL);
	if (ret < 0) {
		log_error("select failed: %s", strerror(errno));
		close(cli->sock);
		return FREE_BUF;
	}

	if (FD_ISSET(cli->sock, &err_set)) {
		log_info("Client %d was gone. Close connection", cli->sock);
		close(cli->sock);
		return FREE_BUF;
	}

	if (FD_ISSET(cli->sock, &read_set)) {
		log_trace("Some data found on %d sock", cli->sock);
		return READ_DATA;
	}

	return WAIT_DATA;
}

FSM_CB(smtp, READ_DATA, cli) {
	struct buffer_t *buf = &cli->buffer;
	if (buf->allocated - buf->used < BLOCK_SIZE)
		expand_buffer(buf);

	ssize_t received = read(cli->sock, buf->buf + buf->used, buf->allocated - buf->used);
	if (received == 0) {
		log_info("Client %d was gone. Close connection", cli->sock);
		close(cli->sock);
		return FREE_BUF;
	}
	if (received < 0) {
		log_info("Error happen on read(): %s", strerror(errno));
		if (errno == EAGAIN)
			return WAIT_DATA;
		log_info("Can't process error happened. Close connection %d", cli->sock);

		cli->cli_error.msg = "can't read";
		cli->cli_error.status = ST_SRV_ERR;
		return CLOSE_CLIENT;
	}

	if (strstr(buf->buf, cli->delimiter)) {
		if (cli->next_state)
			return cli->next_state;

		log_error("State not specified to process read data. Close connection %d", cli->sock);

		cli->cli_error.msg = "server error";
		cli->cli_error.status = ST_SRV_ERR;
		return CLOSE_CLIENT;
	}

	return WAIT_DATA;
}

FSM_CB(smtp, FREE_BUF, cli) {
	log_trace("Trying to free buffer");

	if (cli->buffer.buf)
		free(cli->buffer.buf);

	return SHUTDOWN;
}

FSM_CB(smtp, CLOSE_CLIENT, cli) {
	send_response_ex(cli->sock, STRSZ("QUIT"));
	shutdown(cli->sock, SHUT_WR);
	return WAIT_DATA;
}

static void init_cli(struct client_t *cli, int sock) {
	memset(cli, 0, sizeof(*cli));

	cli->sock = sock;
	cli->delimiter = "\r\n";
	init_buffer(&cli->buffer);
}

void smtp_reject_client(int sock, const char *msg) {
	struct client_t cli;
	init_cli(&cli, sock);

	cli.cli_error.status = 504;
	cli.cli_error.msg = msg;

	FSM_RUN(smtp, &cli);
}

void smtp_communicate_with_client(int sock) {
	struct client_t cli;
	init_cli(&cli, sock);
	FSM_RUN(smtp, &cli);
}
