#pragma once

#include <stdint.h>

struct StaticSemaphore_t {
  int dummy;
};
typedef StaticSemaphore_t* SemaphoreHandle_t;

inline SemaphoreHandle_t xSemaphoreCreateMutexStatic(StaticSemaphore_t* b) { return b; }
inline SemaphoreHandle_t xSemaphoreCreateRecursiveMutexStatic(StaticSemaphore_t* b) { return b; }
inline int xSemaphoreTake(SemaphoreHandle_t, uint32_t) { return 1; }
inline int xSemaphoreGive(SemaphoreHandle_t) { return 1; }
inline int xSemaphoreTakeRecursive(SemaphoreHandle_t, uint32_t) { return 1; }
inline int xSemaphoreGiveRecursive(SemaphoreHandle_t) { return 1; }
