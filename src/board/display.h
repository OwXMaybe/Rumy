#pragma once

#include <stddef.h>
#include <stdint.h>

class LgfxCyd;

namespace display {

struct FlushPerf {
  uint32_t flushes;
  uint32_t pixels;
  uint32_t flushUs;
  uint32_t touchUs;
};

bool begin(uint8_t touchRot);
void setFlip(bool on);
FlushPerf takeFlushPerf();
void setScroll(uint16_t line);
bool hardwareScroll();
void ignoreUntilRelease();
void setWriteHz(uint32_t hz);
uint32_t spiTest(uint32_t hz);
void ledsOff();
LgfxCyd& gfx();
void startLvgl(uint8_t* buf1, uint8_t* buf2, uint32_t bytes);
void setBrightnessPct(uint8_t pct);
uint8_t brightnessPct();
void setTouchRotation(uint8_t rot);
uint32_t lastTouchMs();
bool touching();
void setTouchLog(bool on);
uint32_t touchReads();
bool touchCalSupported();
bool rawTouch(int32_t& x, int32_t& y);
bool calibrateTouch(const int32_t* tx, const int32_t* ty, const int32_t* rx, const int32_t* ry);
bool mapRawTouch(int32_t rx, int32_t ry, int32_t& x, int32_t& y);
void keepTouchCal(bool save);
using JpegReader = uint32_t (*)(void* ctx, uint8_t* buf, uint32_t len);
bool drawJpeg(JpegReader read, void* ctx, int32_t x, int32_t y, uint8_t* pool, size_t poolBytes);
void dumpScreen();

}
