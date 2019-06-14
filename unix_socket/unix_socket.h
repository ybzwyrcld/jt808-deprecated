#ifndef JT808_UNIX_SOCKET_UNIX_SOCKET_H_
#define JT808_UNIX_SOCKET_UNIX_SOCKET_H_

#include <sys/types.h>

#define QLEN        10
#define STALE       30             // client's name can't be older than this (sec).
#define CLI_PATH    "/var/tmp/"    // +5 fro pid = 14 chars.
#define CLI_PERM    S_IRWXU        // rwx for user only.

int ServerListen(const char *name);
int ServerAccept(const int &listenfd, uid_t *uidptr);
int ClientConnect(const char *name);

#endif // JT808_UNIX_SOCKET_UNIX_SOCKET_H_
