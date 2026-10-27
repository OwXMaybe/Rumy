#pragma once

#define LGFX_USE_V1
#include <LovyanGFX.hpp>

#include "app/config.h"

class LgfxCyd : public lgfx::LGFX_Device {
#if defined(RUMY_PANEL_ST7789)
  lgfx::Panel_ST7789 panel_;
#else
  lgfx::Panel_ILI9341 panel_;
#endif
  lgfx::Bus_SPI bus_;
  lgfx::Light_PWM light_;
  lgfx::Touch_XPT2046 touch_;

 public:
  LgfxCyd() {
    {
      auto c = bus_.config();
      c.spi_host = HSPI_HOST;
      c.spi_mode = 0;
      c.freq_write = cfg::TFT_WRITE_HZ;
      c.freq_read = cfg::TFT_READ_HZ;
      c.spi_3wire = false;
      c.use_lock = true;
      c.dma_channel = SPI_DMA_CH_AUTO;
      c.pin_sclk = cfg::PIN_TFT_SCLK;
      c.pin_mosi = cfg::PIN_TFT_MOSI;
      c.pin_miso = cfg::PIN_TFT_MISO;
      c.pin_dc = cfg::PIN_TFT_DC;
      bus_.config(c);
      panel_.setBus(&bus_);
    }
    {
      auto c = panel_.config();
      c.pin_cs = cfg::PIN_TFT_CS;
      c.pin_rst = -1;
      c.pin_busy = -1;
      c.panel_width = 240;
      c.panel_height = 320;
      c.offset_x = 0;
      c.offset_y = 0;
      c.offset_rotation = 0;
      c.readable = true;
      c.bus_shared = false;
#if defined(RUMY_PANEL_ST7789)
      c.invert = true;
      c.rgb_order = false;
#else
      c.invert = false;
      c.rgb_order = false;
#endif
      panel_.config(c);
    }
    {
      auto c = light_.config();
      c.pin_bl = cfg::PIN_TFT_BL;
      c.invert = false;
      c.freq = 12000;
      c.pwm_channel = 7;
      light_.config(c);
      panel_.setLight(&light_);
    }
    {
      auto c = touch_.config();
      c.x_min = cfg::TP_X_MIN;
      c.x_max = cfg::TP_X_MAX;
      c.y_min = cfg::TP_Y_MIN;
      c.y_max = cfg::TP_Y_MAX;
      c.pin_int = cfg::PIN_TP_IRQ;
      c.bus_shared = false;
      c.offset_rotation = cfg::TP_DEFAULT_ROTATION;
      c.spi_host = cfg::TP_SOFT_SPI_HOST;
      c.freq = cfg::TP_SPI_HZ;
      c.pin_sclk = cfg::PIN_TP_SCLK;
      c.pin_mosi = cfg::PIN_TP_MOSI;
      c.pin_miso = cfg::PIN_TP_MISO;
      c.pin_cs = cfg::PIN_TP_CS;
      touch_.config(c);
      panel_.setTouch(&touch_);
    }
    setPanel(&panel_);
  }

  void setWriteHz(uint32_t hz) {
    auto c = bus_.config();
    c.freq_write = hz;
    bus_.config(c);
  }

  void setTouchRotation(uint8_t rot) {
    auto c = touch_.config();
    c.offset_rotation = rot & 7;
    touch_.config(c);
    uint16_t cal[8] = {static_cast<uint16_t>(c.x_min), static_cast<uint16_t>(c.y_min),
                       static_cast<uint16_t>(c.x_min), static_cast<uint16_t>(c.y_max),
                       static_cast<uint16_t>(c.x_max), static_cast<uint16_t>(c.y_min),
                       static_cast<uint16_t>(c.x_max), static_cast<uint16_t>(c.y_max)};
    setTouchCalibrate(cal);
  }
};
