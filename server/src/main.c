#include "config.h"

#include "command_line_options_parser.h"

static struct cmd_line_opt_t cmd_line_opts_list[] = { // list of options
	{ .name = { "-l", "--log-level" }, .descr = "log level", .opt_type = OPT_TYPE_INT, },
	{ .name = { "-c", "--config" }, .descr = "config fine path", .opt_type = OPT_TYPE_STR, },
	{ .name = { "-h", "--help" }, .descr = "show this help", .opt_type = OPT_TYPE_HELP, },
};

int main (int argc, const char **argv) {
	if (parse_command_line_arguments(V_VSIZE(cmd_line_opts_list), argc, argv) != 0) {
		return -1;
	}

	return 0;
}
