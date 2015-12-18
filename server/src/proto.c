#include "proto.h"
#include "logger.h"
#include "fsm.h"
#include "config.h"

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <assert.h>
#include <errno.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <strings.h>
#include <stdarg.h>
#include <ctype.h>
#include <pcre.h>

#if !defined(VERSION) || !defined(BUILD_YEAR) || !defined(DEVELOPERS) || !defined(PROJECT)
#	error "Pass constants above via makefile"
#endif

#ifndef BLOCK_SIZE
#	define BLOCK_SIZE 4096
#endif

#ifndef MESSAGE_MAX_SIZE
#	define MESSAGE_MAX_SIZE (unsigned long)1025*1024
#endif

#define EMAIL_RE "[a-zA-Z0-9_.+-]+@[a-zA-Z0-9-]+\\.[a-zA-Z0-9-.]+"

enum {
	ST_SERVICE_READY = 220,
	ST_BYE = 221,
	ST_MAILING_OK = 250,
	ST_LOCAL_ERR = 451,
	ST_IN_TRANSACTION = 421,
	ST_SYNTAX_ERR = 500,
	ST_INVALID_PARAMS = 501,
	ST_INVALID_CMD = 503,
	ST_TRANSACTION_FAILED = 554,
};

static void send_response_ex(int sock, const char *msg, size_t msg_size) {
	log_trace("Sending to client %d: '%.*s'", sock, (int)msg_size, msg);
	write(sock, msg, strlen(msg));
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
	write(sock, STRSZ("\r\n"));

	return 0;
}

static void send_response_f(int sock, int status, const char *fmt, ...) __attribute__((format(printf, 3, 4)));
static void send_response_f(int sock, int status, const char *fmt, ...) {
	char buf[4096];

	va_list ap;
	va_start(ap, fmt);
	int ret = vsnprintf(buf, sizeof(buf), fmt, ap);
	va_end(ap);

	if (ret < 0) {
		log_error("Can't printf: %s", strerror(errno));
		send_response(sock, ST_LOCAL_ERR, "Local error in processing");
		return;
	}

	if (ret > sizeof(buf))
		log_error("Too small buf into send_response_f");

	send_response(sock, status, buf);
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

struct cli_info_t {
	char *cli_domain;
	char *cli_from;
	char *cli_to;
	char *cli_data;
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
	_(ARG, INIT, FSM_INIT_STATE) \
	_(ARG, WELCOME_CLIENT) \
	_(ARG, WAIT_DATA) \
	_(ARG, READ_DATA) \
	_(ARG, WAIT_COMMAND) \
	_(ARG, COMMAND_CAME) \
	_(ARG, HELO_CAME) \
	_(ARG, EHLO_CAME) \
	_(ARG, MAIL_CAME) \
	_(ARG, RCPT_CAME) \
	_(ARG, DATA_CAME) \
	_(ARG, RSET_CAME) \
	_(ARG, RSET_CAME_AFTER) \
	_(ARG, NEXT_CMD) \
	_(ARG, SYNTAX_ERR) \
	_(ARG, SERVER_ERROR) \
	_(ARG, SHOW_ERROR_AND_CLOSE) \
	_(ARG, CLOSE_CLIENT) \
	_(ARG, FREE_MEM) \
	_(ARG, SHUTDOWN, FSM_LAST_STATE)

struct client_t;
FSM(smtp, STATES, struct client_t *);

struct transaction_t;
struct client_t {
	int sock;

	const char *delimiter;
	unsigned short delimiter_size;

	struct buffer_t buffer;
	struct buffer_t cli_data;
	struct client_error_t cli_error;
	struct cli_info_t cli_info;

	const struct transaction_t *cur_transaction;
	const struct command_t *cur_command;

	FSM_STATE_TYPE(smtp) next_state;
};

FSM_CB(smtp, WELCOME_CLIENT, cli) {
	int status = ST_SERVICE_READY;
	if (cli->cli_error.msg) {
		status = ST_TRANSACTION_FAILED;
	}

	log_debug("Trying to welcome the client");

	char welcome_str[4096] = "";
	if (welcome_str[0] == '\0') {
		log_trace("Creating welcome string");
		snprintf(welcome_str, sizeof(welcome_str), "%s, " PROJECT ", v" VERSION ". Developed by " DEVELOPERS ", " BUILD_YEAR ". Ready", get_opt_hostname());
		log_info("Setting welcome string to '%s'", welcome_str);
	}

	send_response(cli->sock, status, welcome_str);
	if (cli->cli_error.msg)
		return SHOW_ERROR_AND_CLOSE;

	return WAIT_COMMAND;
}

FSM_CB(smtp, SHOW_ERROR_AND_CLOSE, cli) {
	if (!cli->cli_error.msg)
		return NEXT_CMD;

	send_response(cli->sock, cli->cli_error.status, cli->cli_error.msg);
	return CLOSE_CLIENT;
}

FSM_CB(smtp, WAIT_COMMAND, cli) {
	cli->next_state = COMMAND_CAME;
	return WAIT_DATA;
}

struct command_t {
	const char cmd[64];
	size_t cmd_len;
	FSM_STATE_TYPE(smtp) state;
};

enum {
	FL_ABORT_ACTIONS = 1,
};

struct transaction_t {
	struct command_t commands[64]; // MAX COMMANDS
	size_t commands_count;
	struct command_t *cur_command;

	uint32_t flags;
};

#define CMDL(name) .cmd = name, .cmd_len = sizeof(name) - 1

static const struct transaction_t transactions[] = {
	{
		.commands = {
			{ CMDL("QUIT"), .state = CLOSE_CLIENT, },
			{ CMDL("RSET"), .state = RSET_CAME, },
		},
		.commands_count = 2,
		.flags = FL_ABORT_ACTIONS,
	}, {
		.commands = {
			{ CMDL("HELO"), .state = HELO_CAME, },
		},
		.commands_count = 1,
	}, {
		.commands = {
			{ CMDL("EHLO"), .state = EHLO_CAME, },
		},
		.commands_count = 1,
	}, {
		.commands = {
			{ CMDL("MAIL"), .state = MAIL_CAME, },
			{ CMDL("RCPT"), .state = RCPT_CAME, },
			{ CMDL("DATA"), .state = DATA_CAME, },
		},
		.commands_count = 3,
	},
};

#undef CMDL

FSM_CB(smtp, COMMAND_CAME, cli) {
	struct buffer_t *buf = &cli->cli_data;

	if (buf->used < 1) {
		log_debug("Empty command came");
		return WAIT_COMMAND;
	}

	log_debug("Trying to parse came command: %.*s", (int)buf->used, buf->buf);
	const char *delim = strnstr(buf->buf, " ", buf->used);
	uint8_t space_found = 1;

	if (!delim) {
		log_debug("No space char found in cli request. Use full string");
		delim = buf->buf + buf->used;
		space_found = 0;
	}

	const struct transaction_t *transaction = NULL;
	const struct command_t *command = NULL;

	int i = 0;
	size_t cli_cmd_len = (size_t)(delim - buf->buf);
	for (; !transaction && i < VSIZE(transactions); ++i) {
		int j = 0;
		const struct transaction_t *cur_transaction = transactions + i;
		for (; !command && j < cur_transaction->commands_count; ++j) {
			const struct command_t *cmd = cur_transaction->commands + j;
			if (cli_cmd_len == cmd->cmd_len && (strncasecmp(cmd->cmd, buf->buf, cli_cmd_len) == 0)) {
				log_trace("Command recognized as %.*s", (int)cli_cmd_len, cmd->cmd);
				command = cmd;
				transaction = cur_transaction;
			}
		}
	}

	if (!transaction || !command) {
		log_debug("Invalid command came: %.*s", (int)cli_cmd_len, buf->buf);
		send_response(cli->sock, ST_SYNTAX_ERR, "Unknown command");
		return NEXT_CMD;
	}

	if (cli->cur_transaction && cli->cur_command == &cli->cur_transaction->commands[cli->cur_transaction->commands_count - 1]) {
		log_trace("Transaction ended");
		cli->cur_transaction = NULL;
		cli->cur_command = NULL;
	}

	if (transaction->flags & FL_ABORT_ACTIONS) {
		log_debug("Abort action came");
		return command->state;
	}

	// cur transaction and cur command should be both set or not set
	assert(!cli->cur_transaction == !cli->cur_command);

	if (((cli->cur_transaction && cli->cur_command)
			&& (transaction != cli->cur_transaction || command <= cli->cur_command || command != cli->cur_command + 1))
		|| (!cli->cur_transaction && command != &transaction->commands[0])
	) {
		send_response(cli->sock, ST_IN_TRANSACTION, "Command out of sequence; try again later");
		return NEXT_CMD;
	}

	cli->cur_transaction = transaction;
	cli->cur_command = command;

	buf->used -= cli_cmd_len + space_found; // next space should be removed
	memmove(buf->buf, buf->buf + cli_cmd_len + space_found, buf->used);

	FSM_STATE_TYPE(smtp) next_state = command->state;
	assert(next_state);

	return next_state;
}

FSM_CB(smtp, SYNTAX_ERR, cli) {
	send_response(cli->sock, ST_SYNTAX_ERR, "Syntax error");
	return NEXT_CMD;
}

static int set_client_domain(struct client_t *cli) {
	struct buffer_t *buf = &cli->cli_data;
	if (buf->used == 0) {
		log_info("HELO command came without args");
		return 1;
	}

	size_t size = buf->used;
	const char *last_sym = buf->buf;
	while (size > 0 && (*last_sym == '.' || isalnum(*last_sym))) {
		++last_sym;
		--size;
	}

	if (size != 0) {
		log_info("Invalid number of args in request");
		return 1;
	}

	if (cli->cli_info.cli_domain) {
		log_info("Domain is already set");
		send_response(cli->sock, ST_SYNTAX_ERR, "Unknown command");
		return -1;
	}

	cli->cli_info.cli_domain = (char *)malloc(buf->used + 1);
	snprintf(cli->cli_info.cli_domain, buf->used + 1, "%.*s", (int)buf->used, buf->buf);

	log_info("Client domain was set to %s", cli->cli_info.cli_domain);

	return 0;
}

FSM_CB(smtp, HELO_CAME, cli) {
	log_debug("HELO command came");

	int ret = set_client_domain(cli);
	if (ret > 0)
		return SYNTAX_ERR;
	if (ret == 0)
		send_response_f(cli->sock, ST_MAILING_OK, "%s ready to serve", get_opt_hostname());

	return NEXT_CMD;
}

FSM_CB(smtp, MAIL_CAME, cli) {
	log_debug("MAIL command came");

	struct buffer_t *buf = &cli->cli_data;
	if (buf->used == 0) {
		log_info("MAIL command came without args");
		return SYNTAX_ERR;
	}

	const char *err;
	int err_off;

	void _pcre_free(pcre **re) {
		if (re && *re)
			pcre_free(*re);
	}

	log_trace("MAIL request: '%.*s'", (int)buf->used, buf->buf);

	pcre *re __attribute__((cleanup(_pcre_free))) =
		pcre_compile("^from: ?<(" EMAIL_RE ")>$", PCRE_CASELESS, &err, &err_off, NULL);

	if (!re) {
		log_error("pcre_compile failed (offset: %d), %s", err_off, err);
		return SERVER_ERROR;
	}

	int ovec[24];
	int ovecsize = VSIZE(ovec);

	int rc = pcre_exec(re, 0, buf->buf, (int)buf->used, 0, 0, ovec, ovecsize);

	int off = ovec[2];
	int len = ovec[3] - ovec[2];
	if (rc < 0 || len <= 0) {
		log_info("Invalid MAIL command came, data == '%.*s'", (int)buf->used, buf->buf);
		return SYNTAX_ERR;
	}

	cli->cli_info.cli_from = malloc((size_t)len + 1);
	snprintf(cli->cli_info.cli_from, (size_t)len + 1, "%.*s", len, buf->buf + off);

	log_debug("Trying to send message from '%s'", cli->cli_info.cli_from);
	send_response_f(cli->sock, ST_MAILING_OK, "Sender <%s> Ok", cli->cli_info.cli_from);

	return NEXT_CMD;
}

FSM_CB(smtp, RCPT_CAME, cli) {
	return NEXT_CMD;
}

FSM_CB(smtp, DATA_CAME, cli) {
	return NEXT_CMD;
}

FSM_CB(smtp, SERVER_ERROR, cli) {
	cli->cli_error.status = ST_LOCAL_ERR;
	cli->cli_error.msg = "Local error in processuing";
	return SHOW_ERROR_AND_CLOSE;
}

FSM_CB(smtp, EHLO_CAME, cli) {
	log_debug("EHLO command came");

#define add_to_resp(fmt, ...) ({ \
	int _pr = snprintf(resp + printed, sizeof(resp) - printed, fmt "\r\n", ##__VA_ARGS__); \
	if (_pr < 0 || (size_t)_pr > sizeof(resp) - printed) { \
		log_error("Can't snprintf: %s", strerror(errno)); \
		return SERVER_ERROR; \
	} \
	printed += (unsigned)_pr; \
})

	int ret = set_client_domain(cli);
	if (ret > 0)
		return SYNTAX_ERR;
	if (ret == 0) {
		char resp[4096];
		unsigned printed = 0;

		add_to_resp("%d-%s ready to serve", ST_MAILING_OK, get_opt_hostname());
		add_to_resp("%d-8BITMIME", ST_MAILING_OK);
		add_to_resp("%d SIZE %lu", ST_MAILING_OK, MESSAGE_MAX_SIZE);

		send_response_ex(cli->sock, resp, printed);
	}

#undef add_to_resp

	return NEXT_CMD;
}

FSM_CB(smtp, RSET_CAME_AFTER, cli) {
	cli->next_state = NEXT_CMD;
	send_response(cli->sock, ST_MAILING_OK, "Ok");

	return INIT;
}

FSM_CB(smtp, RSET_CAME, cli) {
	cli->next_state = RSET_CAME_AFTER;
	return FREE_MEM;
}

FSM_CB(smtp, NEXT_CMD, cli) {
	cli->cli_data.used = 0;
	return WAIT_COMMAND;
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
		return FREE_MEM;
	}

	if (FD_ISSET(cli->sock, &err_set)) {
		log_info("Client %d was gone. Close connection", cli->sock);
		close(cli->sock);
		return FREE_MEM;
	}

	if (FD_ISSET(cli->sock, &read_set)) {
		log_trace("Some data found on %d sock", cli->sock);
		return READ_DATA;
	}

	return WAIT_DATA;
}

FSM_CB(smtp, READ_DATA, cli) {
	struct buffer_t *buf = &cli->buffer;
	while (buf->allocated <= buf->used + BLOCK_SIZE)
		expand_buffer(buf);

	FSM_STATE_TYPE(smtp) next_state = cli->next_state;
	cli->next_state = NULL;

	ssize_t received = read(cli->sock, buf->buf + buf->used, buf->allocated - buf->used);
	if (received == 0) {
		log_info("Client %d was gone. Close connection", cli->sock);
		close(cli->sock);
		return FREE_MEM;
	}
	if (received < 0) {
		log_info("Error happen on read(): %s", strerror(errno));
		if (errno == EAGAIN)
			return WAIT_DATA;
		log_info("Can't process error happened. Close connection %d", cli->sock);

		cli->cli_error.msg = "can't read";
		cli->cli_error.status = ST_INVALID_CMD;
		return SHOW_ERROR_AND_CLOSE;
	}

	buf->used += (size_t)received;

	char *delimiter_ptr = (char *)memmem(buf->buf, buf->used, cli->delimiter, cli->delimiter_size);
	if (delimiter_ptr) {
		struct buffer_t *cli_buf = &cli->cli_data;
		while (delimiter_ptr - buf->buf >= cli_buf->allocated)
			expand_buffer(buf);

		cli_buf->used = (size_t)(delimiter_ptr - buf->buf);
		memcpy(cli_buf->buf, buf->buf, cli_buf->used);

		buf->used -= cli_buf->used + cli->delimiter_size;
		memmove(buf->buf, delimiter_ptr + cli->delimiter_size, buf->used);

		assert(next_state);

		return next_state;
	}

	return WAIT_DATA;
}

FSM_CB(smtp, FREE_MEM, cli) {
	log_trace("Trying to free buffer");

	safe_free(cli->buffer.buf);
	safe_free(cli->cli_data.buf);
	safe_free(cli->cli_info.cli_domain);
	safe_free(cli->cli_info.cli_from);
	safe_free(cli->cli_info.cli_to);
	safe_free(cli->cli_info.cli_data);

	FSM_STATE_TYPE(smtp) next_state = cli->next_state;
	cli->next_state = NULL;
	if (next_state)
		return next_state;

	return SHUTDOWN;
}

FSM_CB(smtp, CLOSE_CLIENT, cli) {
	send_response(cli->sock, ST_BYE, "Bye");
	shutdown(cli->sock, SHUT_WR);
	return WAIT_DATA;
}

FSM_CB(smtp, INIT, cli) {
	cli->delimiter = "\r\n";
	cli->delimiter_size = 2;
	init_buffer(&cli->buffer);
	init_buffer(&cli->cli_data);

	cli->cur_transaction = NULL;
	cli->cur_command = NULL;

	FSM_STATE_TYPE(smtp) next_state = cli->next_state;
	cli->next_state = NULL;
	if (next_state)
		return next_state;

	return WELCOME_CLIENT;
}

static void init_cli(struct client_t *cli, int sock) {
	memset(cli, 0, sizeof(*cli));

	cli->sock = sock;
}

void smtp_reject_client(int sock, const char *msg) {
	struct client_t cli;
	init_cli(&cli, sock);

	cli.cli_error.status = 504;
	cli.cli_error.msg = msg;

	log_info("Rejecting client");

	FSM_RUN(smtp, &cli);
}

void smtp_communicate_with_client(int sock) {
	struct client_t cli;
	init_cli(&cli, sock);

	log_info("Starting communication with client");

	FSM_RUN(smtp, &cli);
}
