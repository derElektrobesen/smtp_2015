#ifndef __CONFIG_H__
#define __CONFIG_H__

#include "program_config.h"

#define CONFIG_SPEC(_) \
	_(user, STR) \
	_(group, STR) \
	_(listen_host, STR) \
	_(listen_port, INT) \
	_(root_dir, STR) \
	_(queue_dir, STR) \
	_(n_workers, INT) \
	_(hostname, STR) \

SET_CONFIG_SPEC(CONFIG_SPEC)

#endif // __CONFIG_H__
