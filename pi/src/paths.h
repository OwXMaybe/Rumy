#pragma once

#include <stddef.h>
#include <stdint.h>

#ifndef RUMY_DATA_DIR
#define RUMY_DATA_DIR "/tmp/rumy"
#endif

#ifndef RUMY_SD_ROOT
#define RUMY_SD_ROOT RUMY_DATA_DIR "/sd"
#endif

namespace pi {

const char* dataDir();
const char* configDir();
bool ensureDir(const char* path);
bool ensureParent(const char* path);
bool writeAtomic(const char* path, const void* data, size_t len, bool secret);
bool readAll(const char* path, uint8_t* buf, size_t cap, size_t& len);
void bootInit();
void markCleanExit();
uint32_t systemUptimeS();

}
