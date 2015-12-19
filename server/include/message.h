#ifndef __MESSAGE_H__
#define __MESSAGE_H__

int mk_message(const char *data, size_t data_len, const char *mail_from, const char *rcpt_to, char *uidl, size_t uidl_len);

#endif // __MESSAGE_H__
