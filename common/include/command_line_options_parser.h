#ifndef __COMMANDS_LINE_OPTIONS_PARSER_H__
#define __COMMANDS_LINE_OPTIONS_PARSER_H__

#include "common.h"

enum cmd_line_opt_type_t {
	OPT_TYPE_INT,
	OPT_TYPE_STR,
	OPT_TYPE_HELP,
};

struct cmd_line_opt_t {
	const char *name[2];
	const char *descr;

	enum cmd_line_opt_type_t opt_type;
	union {
		const char *s_val;
		int i_val;
	};
};

// will return 0 on success and -1 on error
int parse_command_line_arguments(struct cmd_line_opt_t *options, int options_count, int argc, const char **argv);

#endif //__COMMANDS_LINE_OPTIONS_PARSER_H__
