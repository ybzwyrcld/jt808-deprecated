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

#ifndef JT808_UNIX_SOCKET_UNIX_SOCKET_H_
#define JT808_UNIX_SOCKET_UNIX_SOCKET_H_

#include <sys/types.h>

#define QLEN        10
#define STALE       30  // client's name can't be older than this (sec).
#define CLI_PATH    "/var/tmp/"  // +5 fro pid = 14 chars.
#define CLI_PERM    S_IRWXU  // rwx for user only.

int ServerListen(const char *name);
int ServerAccept(const int &listenfd, uid_t *uidptr);
int ClientConnect(const char *name);

#endif  // JT808_UNIX_SOCKET_UNIX_SOCKET_H_
