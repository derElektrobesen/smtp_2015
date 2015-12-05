#include "program_config.h"
#include "logger.h"
#include "common.h"

#include <libconfig.h>
#include <stdlib.h>

static struct option_t program_config[1024]; // max 1024 options are expected
static int options_count;

#define MK_OPT_GETTER_FUNC(T) TO_TYPE(T)(GETTER_TYPE) MK_GETTER_NAME(T)(enum config_option_type_t opt) { return program_config[opt].TO_TYPE(T)(GETTER_VAR); }
TYPES(MK_OPT_GETTER_FUNC)

struct config_iter_t {
	struct option_t *it;
	int n_options;
	int offset;
};

static struct config_iter_t new_iter(struct option_t *options, int options_count) {
	struct config_iter_t it;
	memset(&it, 0, sizeof(it));
	it.it = options;
	it.n_options = options_count;
	return it;
}

static struct option_t *next_iter(struct config_iter_t *iter) {
	assert(iter);

	if (iter->n_options == 0 || iter->offset == iter->n_options)
		return NULL;

	struct option_t *opt = iter->it + iter->offset;
	++iter->offset;

	return opt;
}

static int lookup_int(struct config_t *config, struct option_t *opt) {
	return 0;
}

static int lookup_string(struct config_t *config, struct option_t *opt) {
	return 0;
}

static int lookup_float(struct config_t *config, struct option_t *opt) {
	return 0;
}

static int lookup_bool(struct config_t *config, struct option_t *opt) {
	return 0;
}

typedef int (*option_parser_t)(struct config_t *config, struct option_t *opt);

#define MK_LOCAL_PARSER(T) TO_TYPE(T)(GETTER_CB),
static option_parser_t options_parsers[] = {
	[CONFIG_OPT_TYPE_MIN_OPT_INDEX] = NULL,
	TYPES(MK_LOCAL_PARSER)
};

int __read_config(struct option_t *options, int opts_count, const char *path) {
	assert(opts_count < VSIZE(program_config));

	struct config_t config;
	config_init(&config);

	int ret = config_read_file(&config, path);
	if (ret != CONFIG_TRUE) {
		log_error("Can't parse config %s:%d (%s)", path, config_error_line(&config), config_error_text(&config));
		return -1;
	}

	struct option_t *opt;
	struct config_iter_t it = new_iter(options, opts_count);
	while ((opt = next_iter(&it))) {
		assert(opt->opt_type < CONFIG_OPT_TYPE_MAX_OPT_INDEX && opt->opt_type > CONFIG_OPT_TYPE_MIN_OPT_INDEX);
		if (options_parsers[opt->opt_type](&config, opt) != 0)
			return -1;
	}

	memcpy(program_config, options, sizeof(*options) * opts_count);
	options_count = opts_count;

	return 0;
}

void deinitialize_config() {
	struct config_iter_t it = new_iter(program_config, options_count);
	struct option_t *opt = NULL;

	while ((opt = next_iter(&it))) {
		if (opt->opt_type == MK_OPT_TYPE(STR) && opt->s_val != NULL)
			free((char *)opt->s_val);
	}
}

