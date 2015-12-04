#ifndef __COMMON_H__
#define __COMMON_H__

#include "assert.h"

#ifdef __GNUC__
#	define __ATTR_FORMAT__(args...) __attribute__ (( format( args ) ))
#else
#	define __ATTR_FORMAT__(...)
#endif

#define VSIZE(name) sizeof(name) / sizeof(*name)
#define V_VSIZE(name) name, VSIZE(name)

#endif // __COMMON_H__
