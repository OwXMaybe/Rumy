#pragma once

#include <stddef.h>
#include <stdint.h>

#ifndef RUMY_VERSION
#define RUMY_VERSION "dev"
#endif

#ifndef RUMY_ENDURANCE
#define RUMY_ENDURANCE 0
#endif

#ifndef RUMY_PI
#define RUMY_PI 0
#endif

#ifndef RUMY_LVGL_POOL_KB
#define RUMY_LVGL_POOL_KB 24
#endif

#ifndef RUMY_WEB_PORT
#define RUMY_WEB_PORT 80
#endif

#ifndef RUMY_SCREEN_W
#define RUMY_SCREEN_W 320
#endif

#ifndef RUMY_SCREEN_H
#define RUMY_SCREEN_H 240
#endif

#if RUMY_PI
#define RUMY_PANEL_NAME "ILI9486 Pi"
#elif defined(RUMY_PANEL_ST7789)
#define RUMY_PANEL_NAME "ST7789"
#else
#define RUMY_PANEL_ILI9341_SELECTED 1
#define RUMY_PANEL_NAME "ILI9341"
#endif

namespace cfg {

constexpr int SCREEN_W = RUMY_SCREEN_W;
constexpr int SCREEN_H = RUMY_SCREEN_H;
constexpr bool BIG_UI = SCREEN_H >= 320;

constexpr int PIN_TFT_SCLK = 14;
constexpr int PIN_TFT_MOSI = 13;
constexpr int PIN_TFT_MISO = 12;
constexpr int PIN_TFT_DC = 2;
constexpr int PIN_TFT_CS = 15;
constexpr int PIN_TFT_BL = 21;
constexpr uint32_t TFT_WRITE_HZ = 40000000;
constexpr uint32_t TFT_READ_HZ = 16000000;

constexpr int PIN_TP_SCLK = 25;
constexpr int PIN_TP_MOSI = 32;
constexpr int PIN_TP_MISO = 39;
constexpr int PIN_TP_CS = 33;
constexpr int PIN_TP_IRQ = 36;
constexpr int TP_SOFT_SPI_HOST = -1;
constexpr uint32_t TP_SPI_HZ = 1000000;
constexpr int TP_X_MIN = 300;
constexpr int TP_X_MAX = 3900;
constexpr int TP_Y_MIN = 200;
constexpr int TP_Y_MAX = 3700;
#ifndef RUMY_TOUCH_ROT
#if defined(RUMY_PANEL_ST7789)
#define RUMY_TOUCH_ROT 0
#else
#define RUMY_TOUCH_ROT 6
#endif
#endif
constexpr uint8_t TP_DEFAULT_ROTATION = RUMY_TOUCH_ROT;

constexpr int PIN_SD_SCK = 18;
constexpr int PIN_SD_MISO = 19;
constexpr int PIN_SD_MOSI = 23;
constexpr int PIN_SD_CS = 5;
constexpr uint32_t SD_HZ = 20000000;
constexpr uint32_t SD_HZ_SLOW = 4000000;
constexpr uint8_t SD_MAX_FILES = 1;

constexpr int PIN_LED_R = 4;
constexpr int PIN_LED_G = 16;
constexpr int PIN_LED_B = 17;

constexpr uint32_t LVGL_POOL_BYTES = RUMY_LVGL_POOL_KB * 1024U;
constexpr uint32_t LVGL_MIN_BLOCK = 2048;
constexpr uint32_t LVGL_TOAST_NEED = 1536;
constexpr uint32_t LVGL_REFUSE_NOTE_MS = 5000;
constexpr int DRAW_BUF_LINES = 32;
constexpr uint32_t SLIDE_MS = 220;
constexpr uint8_t EYE_SLEEP_DEFAULT_MIN = 0;
constexpr uint16_t NIGHT_START_DEFAULT_MIN = 2 * 60;
constexpr int32_t SLIDE_MIN_STEP = 6;
constexpr size_t DRAW_BUF_BYTES = SCREEN_W * DRAW_BUF_LINES * 2;
constexpr size_t JSON_ARENA_BYTES = 8U * 1024U;
constexpr size_t WIZ_JSON_ARENA_BYTES = 1536;

constexpr uint32_t NET_STACK = 10 * 1024;
constexpr uint32_t WIZ_STACK = 4 * 1024;
constexpr int NET_CORE = 0;
constexpr int WIZ_CORE = 0;
constexpr int NET_PRIO = 3;
constexpr int WIZ_PRIO = 4;

constexpr uint32_t TLS_MIN_LARGEST_BLOCK = 34U * 1024U;
constexpr uint32_t TLS_MIN_FREE_HEAP = 64U * 1024U;
constexpr uint32_t HTTP_CONNECT_MS = 6000;
constexpr uint32_t HTTP_TIMEOUT_MS = 10000;
constexpr uint32_t TLS_HANDSHAKE_S = 12;
constexpr uint32_t RELAY_TIMEOUT_MS = 12000;

#if RUMY_ENDURANCE
constexpr uint32_t PERIOD_FORECAST_S = 30;
constexpr uint32_t PERIOD_HOURLY_S = 30;
constexpr uint32_t PERIOD_QUAKE_S = 30;
constexpr uint32_t PERIOD_HOLIDAY_S = 60;
constexpr uint32_t DIAG_LOG_S = 60;
constexpr uint32_t ENDURANCE_SCREEN_MS = 5000;
#else
constexpr uint32_t PERIOD_FORECAST_S = 15 * 60;
constexpr uint32_t PERIOD_HOURLY_S = 60 * 60;
constexpr uint32_t PERIOD_QUAKE_S = 10 * 60;
constexpr uint32_t PERIOD_HOLIDAY_S = 24UL * 3600UL;
constexpr uint32_t DIAG_LOG_S = 5 * 60;
constexpr uint32_t ENDURANCE_SCREEN_MS = 0;
#endif
constexpr uint32_t RETRY_MIN_S = 30;
constexpr uint32_t MATCH_SCHED_S = 6UL * 3600UL;
constexpr uint32_t MATCH_LEAD_S = 15UL * 60UL;
constexpr uint32_t MATCH_LIVE_S = 10;
constexpr uint32_t MATCH_PRE_POLL_S = 60;
constexpr uint32_t MATCH_AFTER_S = 3UL * 3600UL;
constexpr uint32_t MATCH_MAX_S = 4UL * 3600UL;
constexpr uint32_t MATCH_DEMO_MS = 10UL * 60UL * 1000UL;
constexpr int MATCH_PAGES_PER_RUN = 3;
constexpr int MATCH_NAMES_PER_RUN = 2;
constexpr uint32_t MATCH_GOAL_MS = 11000;
constexpr uint32_t MATCH_TOAST_MS = 6000;
constexpr uint32_t MATCH_REACT_GOAL_MS = 180000;
constexpr uint32_t MATCH_REACT_OPP_MS = 120000;
constexpr uint32_t MATCH_REACT_WIN_MS = 1800000;
constexpr uint32_t MATCH_REACT_LOSS_MS = 600000;
constexpr uint32_t MATCH_CANCEL_CONFIRM_MS = 25000;
constexpr uint32_t MATCH_CARD_DAYS = 3;
constexpr uint32_t MATCH_PIN_S = 3UL * 3600UL;
constexpr const char* BOARD_LEAGUES[] = {"tur.1", "uefa.champions"};
constexpr uint32_t BOARD_LIVE_S = 20;
constexpr uint32_t FAST_FRESH_MS = 90000;
constexpr uint32_t FAST_BRIDGE_MS = 45000;
constexpr uint32_t LS_RETRY_S = 120;
constexpr uint32_t BOARD_LEAD_S = 10UL * 60UL;
constexpr uint32_t BOARD_IDLE_S = 6UL * 3600UL;
constexpr int LEAGUE_SKIP_DAYS = 14;
constexpr uint32_t LEAGUE_IDLE_S = 3600;
constexpr uint32_t LEAGUE_DAY_S = 300;
constexpr uint32_t LEAGUE_TABLE_S = 600;
constexpr uint32_t RETRY_MAX_S = 15 * 60;

constexpr uint32_t QUAKE_WINDOW_S = 2UL * 24UL * 3600UL;
constexpr float QUAKE_MIN_MAG = 2.0f;
constexpr int16_t QUAKE_ALERT_MAG10 = 40;
constexpr uint32_t QUAKE_ALERT_PIN_S = 3600;

constexpr uint32_t WIZ_HEARTBEAT_MS = 5000;
constexpr uint32_t WIZ_SEND_MIN_MS = 80;
constexpr uint32_t WIZ_PARTY_MS = 8000;
constexpr uint32_t WIZ_PARTY_STEP_MS = 500;
constexpr uint32_t WIZ_ALERT_STEP_MS = 400;
constexpr uint8_t WIZ_ALERT_STEPS = 4;
constexpr uint32_t WIZ_SYNC_QUIET_MS = 1000;
constexpr uint32_t WIZ_REPLY_TIMEOUT_MS = 1500;
constexpr uint32_t WIZ_DISCOVERY_MS = 1000;
constexpr uint32_t WIZ_REDISCOVER_MS = 15000;

constexpr uint32_t WEB_ACTIVE_MS = 20000;
constexpr size_t WEB_BODY_MAX = 3072;
constexpr uint16_t WEB_PORT = RUMY_WEB_PORT;
constexpr uint32_t WIFI_RETRY_MS = 20000;
constexpr uint32_t NET_STATUS_MS = 5000;
constexpr uint32_t UI_DATA_POLL_MS = 250;
constexpr uint32_t NIGHT_WAKE_MS = 30000;
constexpr uint16_t HOME_TIMEOUT_DEFAULT_S = 60;
constexpr uint8_t CARD_SECONDS_DEFAULT = 5;

constexpr uint32_t SERIAL_BAUD = 460800;
constexpr size_t SERIAL_RX_BYTES = 2304;
constexpr int32_t MEDIA_ART = BIG_UI ? 192 : 144;
constexpr int32_t PHOTO_PX = BIG_UI ? 112 : 84;
constexpr uint32_t PHOTO_RETRY_MS = 4000;
constexpr uint8_t PHOTO_TRIES = 3;
constexpr size_t MEDIA_SLOT_BYTES = BIG_UI ? 0x13000 : 0xB000;
constexpr size_t MEDIA_CHUNK = 2048;
constexpr size_t MEDIA_LINE = 320;
constexpr uint32_t MEDIA_LINK_MS = 6000;
constexpr uint32_t MEDIA_FOOTBALL_MS = 60000;
constexpr uint32_t MEDIA_RX_TIMEOUT_MS = 2000;
constexpr uint32_t MEDIA_QUIET_MS = 300;
constexpr uint32_t MEDIA_QUIET_MAX_MS = 2000;
constexpr uint32_t MEDIA_PAUSED_HOME_MS = 60000;
constexpr uint32_t MEDIA_TEMPO_MS = 5000;
constexpr uint32_t MEDIA_VOL_SHOW_MS = 2000;
constexpr int MEDIA_VOL_STEP = 5;
constexpr uint32_t MEDIA_VOL_HOLD_MS = 400;
constexpr uint32_t MEDIA_VOL_REPEAT_MS = 150;
constexpr uint32_t MEDIA_VOL_SETTLE_MS = 2000;
constexpr int32_t MEDIA_PL_CELL = MEDIA_ART / 3;
constexpr int32_t MEDIA_PL_COVER = MEDIA_ART * 5 / 18;
constexpr uint32_t MEDIA_PL_START_MS = 25000;
constexpr uint32_t WX_FORCE_MS = 120000;

constexpr uint32_t PRINTER_CONNECT_MS = 3000;
constexpr uint32_t PRINTER_RETRY_MIN_MS = 5000;
constexpr uint32_t PRINTER_RETRY_MAX_MS = 60000;
constexpr uint32_t PRINTER_REQUEST_MS = 30000;
constexpr uint32_t PRINTER_SILENCE_MS = 45000;
constexpr uint32_t PRINTER_READ_MS = 400;
constexpr uint32_t PRINTER_PUBLISH_MS = 1000;
constexpr int PRINTER_MSGS_PER_SERVICE = 4;
constexpr uint32_t PRINTER_ERROR_WAKE_MS = 60000;
constexpr int32_t CAM_W = BIG_UI ? 213 : 160;
constexpr int32_t CAM_H = BIG_UI ? 160 : 120;
constexpr size_t CAM_POOL_BYTES = 4000;
constexpr size_t CAM_JPEG_MAX = BIG_UI ? 16384 : 8192;
constexpr uint32_t CAM_BYTE_TIMEOUT_MS = 300;
constexpr uint32_t CAM_BYTE_TIMEOUT_WIFI_MS = 800;
constexpr uint16_t BRIDGE_PORT = 7777;
constexpr uint32_t BRIDGE_AUTH_MS = 3000;
constexpr uint32_t BRIDGE_IDLE_MS = 15000;
constexpr uint32_t BRIDGE_SEND_MS = 4000;
constexpr uint32_t BRIDGE_FAIL_PAUSE_MS = 2000;
constexpr uint32_t BRIDGE_PACE_HEAP = 40U * 1024U;
constexpr uint32_t BRIDGE_PACE_MS = 100;
constexpr uint32_t BRIDGE_PACE_LOW_HEAP = 24U * 1024U;
constexpr uint32_t BRIDGE_PACE_LOW_MS = 1500;
constexpr size_t FW_CHUNK = 2048;
constexpr uint32_t FW_RX_TIMEOUT_MS = 6000;
constexpr uint32_t FW_REBOOT_MS = 1500;
constexpr uint32_t FW_FAIL_SHOW_MS = 6000;
constexpr uint64_t FW_SD_SLACK = 256U * 1024U;
constexpr uint32_t CAM_KEEPALIVE_MS = 5000;
constexpr uint32_t CAM_STALE_MS = 8000;
constexpr uint32_t PRINTER_DONE_CARD_S = 30U * 60U;
constexpr uint32_t AGENT_LINK_MS = 30000;
constexpr uint32_t AGENT_DONE_MIN_S = 120;
constexpr uint32_t AGENT_WAIT_TOAST_MS = 10000;
constexpr uint32_t AGENT_DONE_TOAST_MS = 6000;
constexpr uint32_t AGENT_REACT_MS = 4000;
constexpr uint32_t AGENT_FRAME_MS = 50;

constexpr float DEFAULT_LAT = 41.008f;
constexpr float DEFAULT_LON = 28.978f;
constexpr const char* DEFAULT_CITY = "İstanbul";
constexpr int DEFAULT_PROVINCE = 34;

constexpr const char* PORTAL_AP = "Rumy-Kurulum";
constexpr const char* SD_ROOT = "/rumy";
constexpr const char* SD_IMG_DIR = "/rumy/img";
constexpr const char* SD_CACHE_DIR = "/rumy/cache";
constexpr const char* SD_HOLIDAY_DIR = "/rumy/holidays";
constexpr const char* SD_EVENTS_FILE = "/rumy/events.json";

}
