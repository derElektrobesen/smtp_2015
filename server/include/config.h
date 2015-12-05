#include "program_config.h"

#define CONFIG_SPEC(_) \
	_(user, STR) \
	_(group, STR) \
	_(listen_host, STR) \
	_(listen_port, INT) \
	_(root_dir, STR) \
	_(queue_dir, STR) \
	_(n_workers, INT) \

SET_CONFIG_SPEC(CONFIG_SPEC)
