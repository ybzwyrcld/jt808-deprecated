// Copyright 2019 Yuming Meng. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "unix_socket/unix_socket.h"

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


// Create a server endpoint of a connection.
// Return fd if all ok, <0 on error.
int ServerListen(const char *name) {
  int fd;
  int len;
  int error;
  int retval;
  struct sockaddr_un sock_un;
  char cmd[128] = {0};

  // create a UNIX domain stream socket.
  if ((fd = socket(AF_UNIX, SOCK_STREAM, 0)) < 0)
    return(-1);

  // in case it already exists.
  unlink(name);

  // fill in socket address structure.
  memset(&sock_un, 0, sizeof(sock_un));
  sock_un.sun_family = AF_UNIX;
  snprintf(sock_un.sun_path, sizeof (sock_un.sun_path), "%s", name);
  len = offsetof(struct sockaddr_un, sun_path) + strlen(name);

  // bind the name to the descriptor.
  if (bind(fd, (struct sockaddr *)&sock_un, len) < 0) {
    retval = -2;
    goto errout;
  }

  // add all user write permission.
  snprintf(cmd, sizeof (cmd), "chmod a+w %s", name);
  retval = system(cmd);

  // tell kernel we're a server.
  if (listen(fd, QLEN) < 0) {
    retval = -3;
    goto errout;
  }

  return fd;

errout:
  error = errno;
  close(fd);
  errno = error;
  return retval;
}

// Wait for a client connection  to arrive, and accept it.
// We also obtain the client's usr ID from the pathname
// that it must bind before calling us.
// Returns new fd if all ok, < 0 on error
int ServerAccept(const int &listenfd, uid_t *uidptr) {
  int clientfd;
  int len;
  int error;
  int retval;
  time_t staletime;
  struct sockaddr_un sock_un;
  struct stat statbuf;

  len = sizeof(sock_un);
  if ((clientfd = accept(listenfd,
                         reinterpret_cast<struct sockaddr *>(&sock_un),
                         reinterpret_cast<socklen_t*>(&len))) < 0) {
    return -1;
  }
  // often errno=EINTR, if signal caught.

  // obtain the client's uid from its calling address.
  // len of pathname.
  len -= offsetof(struct sockaddr_un, sun_path);
  // null terminate.
  sock_un.sun_path[len] = 0;

  if (stat(sock_un.sun_path, &statbuf) < 0) {
    retval = -2;
    goto errout;
  }

#ifdef S_ISSOCK  // not defined fro SVR4.
  if (S_ISSOCK(statbuf.st_mode) == 0) {
    retval = -3;  // not a socket.
    goto errout;
  }
#endif
  if ((statbuf.st_mode & (S_IRWXG | S_IRWXO)) ||
       (statbuf.st_mode & S_IRWXU) != S_IRWXU) {
    retval = -4;  // is not rwx------.
    goto errout;
  }

  staletime = time(NULL) - STALE;
  if (statbuf.st_atime < staletime ||
      statbuf.st_ctime < staletime ||
      statbuf.st_mtime < staletime) {
    retval = -5;  // i-node is too old.
    goto errout;
  }

  if (uidptr != NULL)
    *uidptr = statbuf.st_uid;  // return uid of caller.

  unlink(sock_un.sun_path);  // we're done with pathname now.
  return clientfd;

errout:
  error = errno;
  close(clientfd);
  errno = error;
  return retval;
}

// Create a client endpoint and connect to a server.
// Returns fd if all ok, <0 on error.
int ClientConnect(const char *name) {
  int fd;
  int len;
  int error;
  int retval;
  struct sockaddr_un sock_un;

  // create a UNIX domain stream socket.
  if ((fd = socket(AF_UNIX, SOCK_STREAM, 0)) < 0) {
    return -1;
  }

  memset(&sock_un, 0, sizeof(sock_un));
  sock_un.sun_family = AF_UNIX;
  snprintf(sock_un.sun_path, sizeof (sock_un.sun_path), "%s%05d",
           CLI_PATH, getpid());
  len = offsetof(struct sockaddr_un, sun_path) + strlen(sock_un.sun_path);

  unlink(sock_un.sun_path);  // in case it already exits.
  if (bind(fd, reinterpret_cast<struct sockaddr *>(&sock_un), len) < 0) {
    retval = -2;
    goto errout;
  }

  if (chmod(sock_un.sun_path, CLI_PERM) < 0) {
    retval = -3;
    goto errout;
  }

  // fill socket address structure with server's address.
  memset(&sock_un, 0, sizeof(sock_un));
  sock_un.sun_family = AF_UNIX;
  snprintf(sock_un.sun_path, sizeof (sock_un.sun_path), "%s", name);
  len = offsetof(struct sockaddr_un, sun_path) + strlen(name);

  if (connect(fd, reinterpret_cast<struct sockaddr *>(&sock_un), len) < 0) {
    retval = -4;
    goto errout;
  }

  return fd;

errout:
  error = errno;
  close(fd);
  errno = error;
  return retval;
}

