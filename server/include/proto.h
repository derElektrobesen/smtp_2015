#ifndef __PROTO_H__
#define __PROTO_H__

void smtp_communicate_with_client(int sock);
void smtp_reject_client(int sock, const char *msg);

#endif // __PROTO_H__
