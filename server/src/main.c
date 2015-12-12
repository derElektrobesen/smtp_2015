#include "config.h"
#include "logger.h"
#include "server.h"

#include "command_line_options_parser.h"

enum {
	OPT_LOG_LVL = 0,
	OPT_CONFIG,
	OPT_HELP,
	OPT_OUTPUT,
};

static struct cmd_line_opt_t cmd_line_opts_list[] = { // list of options
	[OPT_LOG_LVL] =	{ .name = { "-l", "--log-level" },	.descr = "log level",		.opt_type = OPT_TYPE_INT, },
	[OPT_CONFIG] =	{ .name = { "-c", "--config" },		.descr = "config fine path",	.opt_type = OPT_TYPE_STR, },
	[OPT_HELP] =	{ .name = { "-h", "--help" },		.descr = "show this help",	.opt_type = OPT_TYPE_HELP, },
	[OPT_OUTPUT] =	{ .name = { "-o", "--output" },		.descr = "log file path",	.opt_type = OPT_TYPE_STR, },
};

MK_CONFIG_GETTERS(CONFIG_SPEC)

int main (int argc, const char **argv) {
	void destruct(void *arg __attribute__((unused))) {
		deinitialize_config();
		deinitialize_logger();
	}

	int guard __attribute__((cleanup(destruct))) = 0;

	set_log_level(LOG_ERROR);

	if (parse_command_line_arguments(V_VSIZE(cmd_line_opts_list), argc, argv) != 0) {
		return -1;
	}

	if (!cmd_line_opts_list[OPT_CONFIG].s_val) {
		log_error("-c option is required. See -h for more info");
		return -1;
	}

	if (cmd_line_opts_list[OPT_LOG_LVL].i_val <= 0) {
		cmd_line_opts_list[OPT_LOG_LVL].i_val = 2;
	}

	if (cmd_line_opts_list[OPT_LOG_LVL].i_val >= LOG_MAX) {
		cmd_line_opts_list[OPT_LOG_LVL].i_val = LOG_MAX - 1;
	}

	set_log_level(cmd_line_opts_list[OPT_LOG_LVL].i_val);

	DEF_CONFIG(CONFIG_SPEC);
	if (read_config(cmd_line_opts_list[OPT_CONFIG].s_val) != 0)
		return -1;

	if (get_opt_n_workers() <= 0) {
		log_error("n_workers parametr should be greater then zero");
		return -1;
	}

	if (init_logger(get_opt_n_workers(), cmd_line_opts_list[OPT_OUTPUT].s_val, get_opt_user(), get_opt_group()) < 0)
		return -1;

	run_server();

	return 0;
}
