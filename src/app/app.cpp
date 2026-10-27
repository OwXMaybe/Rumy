#include "app.h"

#include <Arduino.h>
#include <WiFi.h>

#include "diag/diag.h"
#include "settings/settings.h"

namespace app {

namespace {

volatile Action s_action = ACT_NONE;
volatile uint32_t s_due = 0;

}

void request(Action a, uint32_t delayMs) {
  if (a == ACT_NONE) return;
  s_due = millis() + delayMs;
  s_action = a;
  diag::note("app", "eylem %u, %u ms sonra", static_cast<unsigned>(a), static_cast<unsigned>(delayMs));
}

Action pending() { return s_action; }

void poll() {
  Action a = s_action;
  if (a == ACT_NONE || static_cast<int32_t>(millis() - s_due) < 0) return;
  s_action = ACT_NONE;
  if (a == ACT_WIFI_RESET) {
    settings::setWifiConfigured(false);
    WiFi.disconnect(true, true);
  } else if (a == ACT_FACTORY_RESET) {
    settings::factoryReset();
    WiFi.disconnect(true, true);
  }
  diag::note("app", "yeniden baslatiliyor");
  Serial.flush();
  vTaskDelay(pdMS_TO_TICKS(150));
  ESP.restart();
}

const char* resetReasonText() { return diag::resetReason(); }

uint32_t bootCount() { return settings::bootCount(); }

}
