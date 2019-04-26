#ifndef UNIX_SOCKET_H
#define UNIX_SOCKET_H

#include <sys/types.h>

#define QLEN        10
#define STALE       30             // client's name can't be older than this (sec).
#define CLI_PATH    "/var/tmp/"    // +5 fro pid = 14 chars.
#define CLI_PERM    S_IRWXU        // rwx for user only.

int ServerListen(const char *name);
int ServerAccept(int listenfd, uid_t *uidptr);
int ClientConnect(const char *name) ;

#endif // UNIX_SOCKET_H
