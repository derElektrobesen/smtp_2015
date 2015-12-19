#include "common.h"
#include "message.h"

#include <stdio.h>

int mk_message(const char *data, size_t data_len, char *uidl, size_t uidl_len) {
	snprintf(uidl, uidl_len, "1234567890");
	return 0;
}
