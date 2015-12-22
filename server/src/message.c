#include "common.h"
#include "message.h"
#include "logger.h"
#include "config.h"

#include <stdlib.h>
#include <stdio.h>
#include <pcre.h>
#include <errno.h>
#include <time.h>
#include <ctype.h>

#define MAX_HEADERS 64

#define HEADERS(_) \
	_(FROM) \
	_(TO) \
	_(SUBJECT) \
	_(DATE) \

#define MK_ENUM(name) HEADER_## name,

enum header_type_t {
	HEADERS(MK_ENUM)
	HEADER_OTHER,
};

struct header_info_t {
	const char *header_name;
	size_t header_name_len;
	enum header_type_t header_type;
};

#define MK_INFO(name) { .header_name = #name, .header_type = MK_ENUM(name) },

struct header_info_t supported_headers[] = {
	HEADERS(MK_INFO)
};

#undef MK_ENUM
#undef MK_INFO

static enum header_type_t get_header_type(const char *header_name) {
	int i = 0;
	for (; i < VSIZE(supported_headers); ++i) {
		size_t len = supported_headers[i].header_name_len;
		if (len == 0)
			supported_headers[i].header_name_len = len = strlen(supported_headers[i].header_name);

		if (strlen(header_name) == len && strncasecmp(header_name, supported_headers[i].header_name, len) == 0) {
			log_trace("Header %s recognized as %s", header_name, supported_headers[i].header_name);
			return supported_headers[i].header_type;
		}
	}

	return HEADER_OTHER;
}

struct message_header_t {
	enum header_type_t header_type;

	char header_name[128];
	char header_value[1024];
};

struct added_header_t {
	const char *header_name;
	const char *header_value;
};

static pcre *header_re = NULL;

__attribute__((destructor))
void clean_re() {
	if (header_re)
		pcre_free(header_re);
}

const char *parse_headers(const char *data, size_t data_len, struct message_header_t *headers, size_t max_headers, int *n_headers) {
	const char delim[] = "\r\n\r\n";
	const char *data_ptr = memmem(data, data_len, delim, sizeof(delim) - 1);
	if (data_ptr)
		data_ptr += sizeof(delim) - 1;

	if (!header_re) {
		const char *err;
		int err_off;
		header_re = pcre_compile("([\\w-]+):\\s*(.*)", PCRE_NEWLINE_CRLF, &err, &err_off, NULL);
		if (!header_re) {
			log_error("Can't compile re: %s (offset %d)", err, err_off);
			return data_ptr;
		}
	}

	int off = 0;
	int len = (int)data_len;
	int rc = 0;

	int ovector[24];
	if (data_ptr)
		len = (int)(data_ptr - data);

	*n_headers = 0;
	while (off < len && (rc = pcre_exec(header_re, NULL, data, len, off, 0, ovector, sizeof(ovector))) >= 0) {
		if (*n_headers >= max_headers) {
			log_error("Too many headers in message. Reject some");
			break;
		}

		struct message_header_t *hdr = headers + *n_headers;
		int name_len = snprintf(hdr->header_name, sizeof(hdr->header_name), "%.*s", ovector[3] - ovector[2], data + ovector[2]);
		snprintf(hdr->header_value, sizeof(hdr->header_value), "%.*s", ovector[5] - ovector[4], data + ovector[4]);
		hdr->header_type = get_header_type(hdr->header_name);

		log_trace("Header #%d: %s: %s", *n_headers, hdr->header_name, hdr->header_value);

		int i = 1;
		hdr->header_name[0] = (char)toupper(hdr->header_name[0]);
		for (; i < name_len; ++i) {
			hdr->header_name[i] = (char)tolower(hdr->header_name[i]);
		}

		off = ovector[1];
		++*n_headers;
	}

	return data_ptr;
}

static int store_message(const struct added_header_t *headers, int n_headers, const char *data, size_t data_len) {
	char message_name[128];
	snprintf(message_name, sizeof(message_name), "%s/%ld-%s-%d-%d", get_opt_tmp_dir(), time(NULL), get_opt_hostname(), getpid(), rand());

	log_info("Saving message to %s/%s", get_opt_root_dir(), message_name);

	FILE *f = fopen(message_name, "w");
	if (!f) {
		log_error("Can't open message %s: %s", message_name, strerror(errno));
		return -1;
	}

	int i = 0;
	for (; i < n_headers; ++i)
		fprintf(f, "%s: %s\r\n", headers[i].header_name, headers[i].header_value);

	fprintf(f, "\r\n%.*s", (int)data_len, data);
	fclose(f);

	return 0;
}

int mk_message(const char *data, size_t data_len, const char *mail_from, const char *rcpt_to, char *uidl, size_t uidl_len) {
	log_trace("Saving message from '%s'", mail_from);
	log_trace("Recipients are: %s", rcpt_to);

	char timestamp[255];
	time_t t = time(NULL);
	struct tm *tmp = localtime(&t);
	if (tmp == NULL) {
		log_error("Can't get localtime: %s", strerror(errno));
		return -1;
	}

	if (strftime(timestamp, sizeof(timestamp), "%a, %e %b %Y %T %z", tmp) == 0) {
		log_error("Strftime failed: %s", strerror(errno));
		return -1;
	}

	char from_header_value[1024];
	snprintf(from_header_value, sizeof(from_header_value), "%s <%s>", mail_from, mail_from);

	char msgid[255];
	snprintf(msgid, sizeof(msgid), "%ld-%d-%ld", time(NULL) | rand(), rand(), time(NULL));

	const char *at_sym = strchr(mail_from, '@');
	snprintf(uidl, uidl_len, "<%s@%s>", msgid, at_sym + 1);

	int n_default_headers = 4;
	struct added_header_t headers_to_add[MAX_HEADERS + 4] = {
		{ .header_name = "From", .header_value = from_header_value, },
		{ .header_name = "Date", .header_value = timestamp, },
		{ .header_name = "Message-ID", .header_value = uidl, },
		{ .header_name = "To", .header_value = rcpt_to, },
	};

	struct message_header_t headers[MAX_HEADERS];
	int n_headers = 0;

	const char *data_ptr = parse_headers(data, data_len, headers, VSIZE(headers), &n_headers);
	if (!data_ptr)
		log_info("Message without body came");

	int i = 0;
	const struct message_header_t *subject = NULL;
	for (; i < n_headers; ++i) {
		if (headers[i].header_type == HEADER_SUBJECT) {
			subject = headers + i;
		}
	}

	if (subject) {
		headers_to_add[n_default_headers++] = (struct added_header_t){
			.header_name = subject->header_name,
			.header_value = subject->header_value,
		};
	} else {
		headers_to_add[n_default_headers++] = (struct added_header_t){
			.header_name = "Subject",
			.header_value = "<No subject>",
		};
	}

	for (i = 0; i < n_headers; ++i) {
		if (headers[i].header_type == HEADER_OTHER) {
			headers_to_add[n_default_headers++] = (struct added_header_t){
				.header_name = headers[i].header_name,
				.header_value = headers[i].header_value,
			};
		}
	}

	return store_message(headers_to_add, n_default_headers, data_ptr, (size_t)(data_ptr ? data_len - (size_t)(data_ptr - data) : 0));
}
