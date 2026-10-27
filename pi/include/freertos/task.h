#pragma once

#include <stdint.h>

#include "Arduino.h"
#include "FreeRTOS.h"

typedef void (*TaskFunction_t)(void*);

int xTaskCreatePinnedToCore(TaskFunction_t fn, const char* name, uint32_t stackBytes, void* arg, int prio,
                            TaskHandle_t* handle, int core);
