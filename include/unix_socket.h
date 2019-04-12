#ifndef __UNIX_SOCKET_H__
#define __UNIX_SOCKET_H__

#define QLEN        10
#define STALE       30             /* client's name can't be older than this (sec) */
#define CLI_PATH    "/var/tmp/"    /* +5 fro pid = 14 chars */
#define CLI_PERM    S_IRWXU        /* rwx for user only */

int serv_listen(const char *name);
int serv_accept(int listenfd, uid_t *uidptr);
int cli_conn(const char *name) ;

#endif // #ifndef __UNIX_SOCKET_H__
