#pragma once

#include <pthread.h>
#include <stdint.h>

#define portMAX_DELAY 0xFFFFFFFFu
#define pdPASS 1
#define pdFAIL 0

typedef pthread_mutex_t portMUX_TYPE;
#define portMUX_INITIALIZER_UNLOCKED PTHREAD_MUTEX_INITIALIZER
#define portENTER_CRITICAL(m) pthread_mutex_lock(m)
#define portEXIT_CRITICAL(m) pthread_mutex_unlock(m)
