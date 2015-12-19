#ifndef __COMMON_H__
#define __COMMON_H__

#include "assert.h"
#include "string.h"

#ifdef __GNUC__
#	define __ATTR_FORMAT__(args...) __attribute__ (( format( args ) ))
#else
#	define __ATTR_FORMAT__(...)
#endif

#define VSIZE(name) sizeof(name) / sizeof(*name)
#define V_VSIZE(name) name, VSIZE(name)

#define safe_free(ptr) ({ \
	if (ptr) \
		free(ptr); \
		ptr = NULL; \
	})

#define STRSZ(str) (str), (sizeof(str) - 1)

// dirs is a list of directories to be used in project.
// First directory will be used as root. All other dirs will bw subdirs of this dir
int drop_privileges(const char *user, const char *group, const char **dirs);

#endif // __COMMON_H__
