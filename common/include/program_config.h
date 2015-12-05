#ifndef __PROGRAM_CONFIG_H__
#define __PROGRAM_CONFIG_H__

#include "common.h"

/*
 * Usage:
 *  implement list of options in the following format (in config header file):
 *	#define CONFIG_SPEC(_) \
 *		_(user, STR) \
 *		_(group, STR) \
 *		...etc
 *  and call SET_CONFIG_SPEC(CONFIG_SPEC) in the same header.
 *  This will initialize configuriation
 *
 *  In main file call MK_CONFIG_GETTERS(CONFIG_SPEC).
 *  This will create implementations of methods
 *	get_opt_user(), get_opt_group() ... etc
 *
 *  In main() function call DEF_CONFIG(CONFIG_SPEC) and after this read_config(filename).
 *  This will initialize config with values from the given config file.
 *
 *  Function will return -1 on error and 0 on read success
 *
 *  To add new option type add option name to TYPES() macro and set _NEW_TYPE macro in the format below
 */

// use following types to declare config option type
#define TYPES(_) \
	_(INT) \
	_(STR) \
	_(FLOAT) \
	_(BOOL)

// following callbacks are defined in .c module
//			_(callback, varname, real_type)
#define _INT(_)		_(lookup_int, i_val, int)
#define _STR(_)		_(lookup_string, s_val, const char *)
#define _FLOAT(_)	_(lookup_float, f_val, double)
#define _BOOL(_)	_(lookup_bool, b_val, int)

#define TO_TYPE(type) _##type

#define MK_OPT_TYPE(T) CONFIG_OPT_TYPE_## T
#define MK_OPT_TYPE_WITH_COMMA(T) MK_OPT_TYPE(T),
#define MK_OPT_VAR(T) TO_TYPE(T)(GETTER_TYPE) TO_TYPE(T)(GETTER_VAR);

#define MK_GETTER_NAME(name) __get_config_opt_## name
#define MK_OPT_GETTER(T) TO_TYPE(T)(GETTER_TYPE) MK_GETTER_NAME(T)(enum config_option_type_t opt);

#define SINGLE_OPT(name, type) [OPT_INDEX_## name] = { .opt_name = #name, .opt_type = MK_OPT_TYPE_WITH_COMMA(type) },
#define JUST_NAME(name, type) OPT_INDEX_## name,

#define GETTER_TYPE(cb, var, type) type
#define GETTER_VAR(cb, var, type) var
#define GETTER_CB(cb, var, type) cb
#define GETTER_NAME(name, type) TO_TYPE(type)(GETTER_TYPE) get_opt_## name()
#define GETTER(name, type) GETTER_NAME(name, type){ return MK_GETTER_NAME(type)(OPT_INDEX_## name); }
#define GETTER_INIT(name, type) GETTER_NAME(name, type);

#define INIT_GETTERS(OPTS_LIST) OPTS_LIST(GETTER_INIT)
#define MK_CONFIG_GETTERS(OPTS_LIST) OPTS_LIST(GETTER)

#define SET_CONFIG_SPEC(OPTS_LIST) \
	enum opt_index_t { \
		OPTS_LIST(JUST_NAME) \
		OPT_INDEX_OPT_MAX \
	}; \
	INIT_GETTERS(OPTS_LIST)

enum config_option_type_t {
	CONFIG_OPT_TYPE_MIN_OPT_INDEX = 0,
	TYPES(MK_OPT_TYPE_WITH_COMMA)
	CONFIG_OPT_TYPE_MAX_OPT_INDEX,
};
struct option_t {
	const char *opt_name;
	enum config_option_type_t opt_type;
	union {
		TYPES(MK_OPT_VAR)
	};
};

#define DEF_CONFIG(OPTS_LIST) \
	struct option_t __just_a_config[] = { \
		OPTS_LIST(SINGLE_OPT) \
	}

TYPES(MK_OPT_GETTER)

// read_config should be called on program initialize
#define read_config(path) __read_config(V_VSIZE(__just_a_config), path)
int __read_config(struct option_t *config, unsigned options_count, const char *path);
void deinitialize_config();

#endif // __PROGRAM_CONFIG_H__
