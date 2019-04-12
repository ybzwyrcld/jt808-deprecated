#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <time.h>
#include <errno.h>

#include <unix_socket.h>

/*
* Create a server endpoint of a connection.
* Return fd if all ok, <0 on error.
*/
int serv_listen(const char *name)
{
	int fd, len, err, rval;
	struct sockaddr_un un;
	char cmd[128] = {0};

	/* create a UNIX domain stream socket */
	if ((fd = socket(AF_UNIX, SOCK_STREAM, 0)) < 0)
		return(-1);

	/* in case it already exists */
	unlink(name);

	/* fill in socket address structure */
	memset(&un, 0, sizeof(un));
	un.sun_family = AF_UNIX;
	strcpy(un.sun_path, name);
	len = offsetof(struct sockaddr_un, sun_path) + strlen(name);

	/* bind the name to the descriptor */
	if (bind(fd, (struct sockaddr *)&un, len) < 0) {
		rval = -2;
		goto errout;
	}

	/* add all user write permission. */
	snprintf(cmd, 128, "chmod a+w %s", name);
	rval = system(cmd);

	/* tell kernel we're a server */
	if (listen(fd, QLEN) < 0) {
		rval = -3;
		goto errout;
	}

	return fd;

errout:
	err = errno;
	close(fd);
	errno = err;
	return rval;
}

/*
* Wait for a client connection  to arrive, and accept it.
* We also obtain the client's usr ID from the pathname
* that it must bind before calling us.
* Returns new fd if all ok, < 0 on error
*/
int serv_accept(int listenfd, uid_t *uidptr)
{
	int clifd, len, err, rval;
	time_t staletime;
	struct sockaddr_un un;
	struct stat statbuf;

	len = sizeof(un);
	if ((clifd = accept(listenfd, (struct sockaddr *)&un, (socklen_t*)&len)) < 0)
		return(-1);	/* often errno=EINTR, if signal caught */

	/* obtain the client's uid from its calling address */
	/* len of pathname */
	len -= offsetof(struct sockaddr_un, sun_path);
	/* null terminate */
	un.sun_path[len] = 0;

	if (stat(un.sun_path, &statbuf) < 0) {
		rval = -2;
		goto errout;
	}

#ifdef S_ISSOCK	/* not defined fro SVR4 */
	if (S_ISSOCK(statbuf.st_mode) == 0) {
		rval = -3;	/* not a socket */
		goto errout;
	}
#endif
	if ((statbuf.st_mode & (S_IRWXG | S_IRWXO)) ||
		   (statbuf.st_mode & S_IRWXU) != S_IRWXU) {
		rval = -4;	/* is not rwx------ */
		goto errout;
	}

	staletime = time(NULL) - STALE;
	if (statbuf.st_atime < staletime ||
			statbuf.st_ctime < staletime ||
			statbuf.st_mtime < staletime) {
		rval = -5;	/* i-node is too old */
		goto errout;
	}

	if (uidptr != NULL)
		*uidptr = statbuf.st_uid;	/* return uid of caller */

	unlink(un.sun_path);	/* we're done with pathname now */
	return clifd;

errout:
	err = errno;
	close(clifd);
	errno = err;
	return rval;
}

/*
* Create a client endpoint and connect to a server.
* Returns fd if all ok, <0 on error.
*/
int cli_conn(const char *name)
{
	int	fd, len, err, rval;
	struct sockaddr_un un;

	/* create a UNIX domain stream socket */
	if ((fd = socket(AF_UNIX, SOCK_STREAM, 0)) < 0)
		return(-1);

	memset(&un, 0, sizeof(un));
	un.sun_family = AF_UNIX;
	sprintf(un.sun_path, "%s%05d", CLI_PATH, getpid());
	len = offsetof(struct sockaddr_un, sun_path) + strlen(un.sun_path);

	unlink(un.sun_path);	/* in case it already exits */
	if (bind(fd, (struct sockaddr *)&un, len) < 0) {
		rval = -2;
		goto errout;
	}

	if (chmod(un.sun_path, CLI_PERM) < 0) {
		rval = -3;
		goto errout;
	}

	/* fill socket address structure with server's address */
	memset(&un, 0, sizeof(un));
	un.sun_family = AF_UNIX;
	strcpy(un.sun_path, name);
	len = offsetof(struct sockaddr_un, sun_path) + strlen(name);

	if (connect(fd, (struct sockaddr *)&un, len) < 0) {
		rval = -4;
		goto errout;
	}

	return fd;

errout:
	err = errno;
	close(fd);
	errno = err;
	return rval;
}

