#include "command_line_options_parser.h"
#include "logger.h"
#include "state_machine.h"

#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <stdio.h>

struct cmd_line_opts_t {
	struct cmd_line_opt_t *cur_opt;
	int done;

	int argc;
	int cur_arg_index;
	const char **argv;

	struct cmd_line_opt_t *options_list;
	int options_count;
};

void init_cmd_line_opts(struct cmd_line_opts_t *opts, int argc, const char **argv, struct cmd_line_opt_t *opts_list, int options_count) {
	memset(opts, 0, sizeof(*opts));

	opts->argc = argc;
	opts->cur_arg_index = 1; // skip program name
	opts->argv = argv;

	opts->options_list = opts_list;
	opts->options_count = options_count;
}

#define STATE_MACHINE_STATES_LIST(ARG, _) \
	_(ARG, NO_OPT, next_opt, initial_state) \
	_(ARG, PARSE_OPT_TYPE_INT, parse_int_opt) \
	_(ARG, PARSE_OPT_TYPE_STR, parse_str_opt) \
	_(ARG, PARSE_OPT_TYPE_HELP, print_help) \
	_(ARG, OPTS_PARSING_DONE)

STATE_MACHINE(cmd_line_opts, STATE_MACHINE_STATES_LIST, struct cmd_line_opts_t *);

STATE_MACHINE_CB(cmd_line_opts, parse_int_opt, options) {
	if (options->cur_arg_index >= options->argc) {
		log_error("Option '%s' should have an integer value", options->cur_opt->name[1]);
		return PARSE_OPT_TYPE_HELP;
	}

	const char *opt_val = options->argv[options->cur_arg_index++];

	int i = 0;
	int valid = 1;
	for (; i < strlen(opt_val); ++i) {
		if (!isdigit(opt_val[i])) {
			valid = 0;
			break;
		}
	}

	if (!valid) {
		log_error("Option '%s' should have an integer value", options->cur_opt->name[1]);
		return PARSE_OPT_TYPE_HELP;
	}

	options->cur_opt->i_val = atoi(opt_val);
	return NO_OPT;
}

STATE_MACHINE_CB(cmd_line_opts, parse_str_opt, options) {
	if (options->cur_arg_index >= options->argc) {
		log_error("Option '%s' should have a string value", options->cur_opt->name[1]);
		return OPTS_PARSING_DONE;
	}

	const char *opt_val = options->argv[options->cur_arg_index++];

	options->cur_opt->s_val = opt_val;
	return NO_OPT;
}

static struct {
	enum cmd_line_opt_type_t opt_type;
	STATE_MACHINE_STATE_TYPE(cmd_line_opts) state;
} type_vs_state[] = {
	{ .opt_type = OPT_TYPE_INT, .state = PARSE_OPT_TYPE_INT, },
	{ .opt_type = OPT_TYPE_STR, .state = PARSE_OPT_TYPE_STR, },
	{ .opt_type = OPT_TYPE_HELP, .state = PARSE_OPT_TYPE_HELP, },
};

STATE_MACHINE_CB(cmd_line_opts, next_opt, options) {
	if (options->cur_arg_index >= options->argc) {
		log_trace("Iteration over options done");
		options->done = 1;
		return OPTS_PARSING_DONE;
	}

	const char *opt_name = options->argv[options->cur_arg_index++];

	int i = 0;
	for (; i < options->options_count; ++i) {
		struct cmd_line_opt_t *cur_opt = options->options_list + i;
		if (strcmp(cur_opt->name[0], opt_name) == 0 || strcmp(cur_opt->name[1], opt_name) == 0) {
			// state found
			options->cur_opt = cur_opt;
			int j = 0;
			for (; j < VSIZE(type_vs_state); ++j) {
				if (type_vs_state[j].opt_type == cur_opt->opt_type)
					return type_vs_state[j].state;
			}

			assert(!"Unknown option type");
		}
	}

	log_error("Option with name '%s' not found", opt_name);
	return PARSE_OPT_TYPE_HELP;
}

STATE_MACHINE_CB(cmd_line_opts, print_help, options) {
	printf("Usage: %s [options]\n", options->argv[0]);

	int i = 0;
	for (; i < options->options_count; ++i) {
		struct cmd_line_opt_t *cur_opt = options->options_list + i;
		printf("\t%s (%s)\t-- %s\n", cur_opt->name[0], cur_opt->name[1], cur_opt->descr);
	}

	return OPTS_PARSING_DONE;
}

int parse_command_line_arguments(struct cmd_line_opt_t *opts, int opts_count, int argc, const char **argv) {
	struct cmd_line_opts_t options;
	init_cmd_line_opts(&options, argc, argv, opts, opts_count);

	STATE_MACHINE_RUN(cmd_line_opts, &options);

	if (options.done != 1) {
		return -1;
	}

	return 0;
}
