#ifndef __X_CONFIG_H__
#define __X_CONFIG_H__

#include <libconfig.h>

int read_config(const char *path);
void deinitialize_config();

// use following types to declare config option type
#define TYPES(_) \
	_(INT) \
	_(STR) \
	_(FLOAT) \
	_(BOOL)

#define _INT(_)		_(i_val, int)
#define _STR(_)		_(s_val, const char *)
#define _FLOAT(_)	_(f_val, float)
#define _BOOL(_)	_(b_val, int)

#define TO_TYPE(type) _##type

#define MK_OPT_TYPE(T) CONFIG_OPT_TYPE_## T,
#define MK_OPT_VAR(T) TO_TYPE(T)(GETTER_TYPE) TO_TYPE(T)(GETTER_VAR);

#define SINGLE_OPT(name, type) [OPT_INDEX_## name] = { .opt_name = #name, .opt_type = MK_OPT_TYPE(type) },
#define JUST_NAME(name, type) OPT_INDEX_## name,

#define GETTER_TYPE(_, t) t
#define GETTER_VAR(v, _) v
#define GETTER(name, type) TO_TYPE(type)(GETTER_TYPE) get_option_## name() { return __program_config[OPT_INDEX_##name].TO_TYPE(type)(GETTER_VAR); }

#define MK_GETTERS(OPTS_LIST) OPTS_LIST(GETTER)

#define SET_CONFIG_SPEC(OPTS_LIST) \
	enum opt_index_t { \
		OPT_INDEX_FIRST_OPT = 0, \
		OPTS_LIST(JUST_NAME) \
		OPT_INDEX_OPT_MAX \
	}; \
	static struct program_config_t __program_config[] = { \
		[OPT_INDEX_FIRST_OPT] = {}, \
		OPTS_LIST(SINGLE_OPT) \
	}; \
	MK_GETTERS(OPTS_LIST)

#define MK_TYPES(TYPES_LIST) \
	enum config_option_type_t { \
		TYPES_LIST(MK_OPT_TYPE) \
	}; \
	struct program_config_t { \
		const char *opt_name; \
		enum config_option_type_t opt_type; \
		union { \
			TYPES_LIST(MK_OPT_VAR) \
		}; \
	};

MK_TYPES(TYPES)

#endif // __X_CONFIG_H__
