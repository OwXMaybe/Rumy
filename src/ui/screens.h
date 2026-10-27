#pragma once

#include <lvgl.h>
#include <stdint.h>

namespace scr_home {
lv_obj_t* create();
void update(uint32_t mask);
void setVisible(bool visible);
void onTouch();
void forceWeather(uint8_t kind, uint32_t ms);
void glance();
}

namespace scr_agents {
lv_obj_t* create();
void update(uint32_t mask);
}

namespace scr_match {
lv_obj_t* create();
void update(uint32_t mask);
void setTableStyle(int style);
}

namespace scr_football {
lv_obj_t* create();
void update(uint32_t mask);
}

namespace scr_league {
lv_obj_t* create();
void update(uint32_t mask);
}

namespace scr_playlist {
lv_obj_t* create();
void update(uint32_t mask);
}

namespace scr_weather {
lv_obj_t* create();
void update(uint32_t mask);
void enduranceStep();
bool benchStrip();
}

namespace scr_light {
lv_obj_t* create();
void update(uint32_t mask);
}

namespace scr_settings {
lv_obj_t* create();
void update(uint32_t mask);
}

namespace scr_diag {
lv_obj_t* create();
void update(uint32_t mask);
}

namespace scr_pair {
lv_obj_t* create();
void update(uint32_t mask);
}

namespace scr_location {
lv_obj_t* create();
void update(uint32_t mask);
}

namespace scr_night {
lv_obj_t* create();
void update(uint32_t mask);
}

namespace scr_info {
lv_obj_t* create();
void update(uint32_t mask);
}

namespace scr_event {
lv_obj_t* create();
void update(uint32_t mask);
}

namespace scr_display {
lv_obj_t* create();
void update(uint32_t mask);
}

namespace scr_media {
lv_obj_t* create();
void update(uint32_t mask);
}

namespace scr_nightcfg {
lv_obj_t* create();
void update(uint32_t mask);
}

namespace scr_printer {
lv_obj_t* create();
void update(uint32_t mask);
}

namespace scr_touchcal {
lv_obj_t* create();
void update(uint32_t mask);
}

namespace scr_phone {
lv_obj_t* create();
void update(uint32_t mask);
}
