#pragma once

#include <pthread.h>
#include <stdint.h>

#include "FreeRTOS.h"

struct StaticSemaphore_t {
  pthread_mutex_t m;
};
typedef StaticSemaphore_t* SemaphoreHandle_t;

inline SemaphoreHandle_t xSemaphoreCreateMutexStatic(StaticSemaphore_t* b) {
  pthread_mutex_init(&b->m, nullptr);
  return b;
}

inline SemaphoreHandle_t xSemaphoreCreateRecursiveMutexStatic(StaticSemaphore_t* b) {
  pthread_mutexattr_t a;
  pthread_mutexattr_init(&a);
  pthread_mutexattr_settype(&a, PTHREAD_MUTEX_RECURSIVE);
  pthread_mutex_init(&b->m, &a);
  pthread_mutexattr_destroy(&a);
  return b;
}

inline int xSemaphoreTake(SemaphoreHandle_t h, uint32_t) { return pthread_mutex_lock(&h->m) == 0 ? 1 : 0; }
inline int xSemaphoreGive(SemaphoreHandle_t h) { return pthread_mutex_unlock(&h->m) == 0 ? 1 : 0; }
inline int xSemaphoreTakeRecursive(SemaphoreHandle_t h, uint32_t t) { return xSemaphoreTake(h, t); }
inline int xSemaphoreGiveRecursive(SemaphoreHandle_t h) { return xSemaphoreGive(h); }
