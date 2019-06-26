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

#ifndef JT808_UTIL_CONTAINER_CLEAR_H_
#define JT808_UTIL_CONTAINER_CLEAR_H_

#include <iostream>
#include <list>
#include <vector>


template <class T>
static void ClearContainerElement(std::vector<T *> *vect) {
  if (vect == nullptr || vect->empty()) {
    return;
  }

  while (!vect->empty()) {
    delete vect->back();
    vect->pop_back();
  }
}

template <class T>
static void ClearContainerElement(std::list<T *> *list) {
  if (list == nullptr || list->empty()) {
    return;
  }

  while (!list->empty()) {
    delete list->back();
    list->pop_back();
  }
}

#endif  // JT808_UTIL_CONTAINER_CLEAR_H_
