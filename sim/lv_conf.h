#ifndef RUMY_SIM_LV_CONF_H
#define RUMY_SIM_LV_CONF_H

#include "../include/lv_conf.h"
#include "sim_paths.h"

#undef LV_USE_FS_ARDUINO_SD
#define LV_USE_FS_ARDUINO_SD 0
#undef LV_USE_FS_STDIO
#define LV_USE_FS_STDIO 1
#define LV_FS_STDIO_LETTER 'S'
#define LV_FS_STDIO_PATH RUMY_SIM_SD
#define LV_FS_STDIO_CACHE_SIZE 0
#undef LV_USE_LOG
#define LV_USE_LOG 1
#define LV_LOG_LEVEL LV_LOG_LEVEL_WARN
#define LV_LOG_PRINTF 1
#undef LV_MEM_SIZE
#define LV_MEM_SIZE (RUMY_SIM_POOL_KB * 1024U)

#endif
