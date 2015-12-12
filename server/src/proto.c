#include "proto.h"
#include "logger.h"

#include <stdio.h>
#include <unistd.h>

int smtp_send_error(int sock, int status, const char *msg) {
	char real_msg[4096] = "";
	int printed = snprintf(real_msg, sizeof(real_msg), "%d %s\r\n", status, msg);

	if (printed >= sizeof(real_msg))
		log_error("Too long error message: %s, maximum %zu chars are expected", msg, sizeof(real_msg));
	else if (printed < 0) {
		log_error("Can't send error message: snprintf failed");
		return -1;
	}

	write(sock, real_msg, (size_t)printed);
	return 0;
}
