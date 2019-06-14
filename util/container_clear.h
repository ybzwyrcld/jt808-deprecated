#ifndef JT808_UTIL_CONTAINER_CLEAR_H_
#define JT808_UTIL_CONTAINER_CLEAR_H_

#include <iostream>
#include <list>
#include <vector>


template <class T>
static void ClearContainerElement(std::vector<T *> *vect) {
  if (vect == nullptr || vect->empty()) {
    return ;
  }

  while (!vect->empty()) {
    delete vect->back();
    vect->pop_back();
  }
}

template <class T>
static void ClearContainerElement(std::vector<T *> &vect) {
  if (vect.empty()) {
    return ;
  }

  while (!vect.empty()) {
    delete vect.back();
    vect.pop_back();
  }
}

template <class T>
static void ClearContainerElement(std::list<T *> *list) {
  if (list == nullptr || list->empty()) {
    return ;
  }

  while (!list->empty()) {
    delete list->back();
    list->pop_back();
  }
}

template <class T>
static void ClearContainerElement(std::list<T *> &list) {
  if (list.empty()) {
    return ;
  }

  while (!list.empty()) {
    delete list.back();
    list.pop_back();
  }
}

#endif  // JT808_UTIL_CONTAINER_CLEAR_H_
