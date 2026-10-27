#include <Arduino.h>
#include <fcntl.h>
#include <lvgl.h>
#include <signal.h>
#include <stdio.h>
#include <sys/file.h>
#include <unistd.h>

#include "agents/agents.h"
#include "app/app.h"
#include "app/config.h"
#include "app/mem.h"
#include "app/store.h"
#include "board/display.h"
#include "board/sdcard.h"
#include "diag/diag.h"
#include "events/events.h"
#include "health_pi.h"
#include "media/camera.h"
#include "media/fwupdate.h"
#include "media/media.h"
#include "net/net.h"
#include "net/relay.h"
#include "paths.h"
#include "quake/quake.h"
#include "settings/settings.h"
#include "ui/ui.h"
#include "weather/weather.h"
#include "wiz/wiz.h"

namespace {

constexpr int BOOT_STEPS = 9;
const char* s_stepName[BOOT_STEPS];
uint32_t s_stepMs[BOOT_STEPS];
int s_stepN = 0;
volatile sig_atomic_t s_stop = 0;

void onSignal(int) { s_stop = 1; }

void step(const char* name) {
  if (s_stepN >= BOOT_STEPS) return;
  s_stepName[s_stepN] = name;
  s_stepMs[s_stepN] = millis();
  s_stepN++;
}

void reportSteps() {
  char buf[200];
  size_t k = 0;
  for (int i = 0; i < s_stepN && k < sizeof(buf); i++) {
    int n = snprintf(buf + k, sizeof(buf) - k, "%s%s %u ms", i ? " · " : "", s_stepName[i],
                     static_cast<unsigned>(s_stepMs[i]));
    if (n < 0) break;
    k += static_cast<size_t>(n);
  }
  diag::note("boot", "adimlar: %s", buf);
}

void signals() {
  signal(SIGPIPE, SIG_IGN);
  struct sigaction sa = {};
  sa.sa_handler = onSignal;
  sigaction(SIGTERM, &sa, nullptr);
  sigaction(SIGINT, &sa, nullptr);
}

bool singleInstance() {
  pi::ensureDir(pi::dataDir());
  char path[512];
  snprintf(path, sizeof(path), "%s/rumy.kilit", pi::dataDir());
  int fd = open(path, O_RDWR | O_CREAT | O_CLOEXEC, 0644);
  if (fd < 0) return true;
  if (flock(fd, LOCK_EX | LOCK_NB) == 0) return true;
  close(fd);
  return false;
}

}

int main() {
  signals();
  setvbuf(stdout, nullptr, _IOLBF, 0);
  if (!singleInstance()) {
    fprintf(stdout, "[rumy] baska bir Rumy zaten calisiyor, cikiliyor\n");
    return 75;
  }
  pi::bootInit();
  store::init();
  settings::load();
  settings::bumpBootCount();
  if (!settings::get().wifiConfigured) settings::setWifiConfigured(true);
  diag::begin();
  Settings st = settings::get();
  step("basla");
  if (!mem::init()) {
    diag::note("boot", "HATA: bellek ayrilamadi");
    return 1;
  }
  step("tampon");
  display::begin(st.touchRot);
  lv_init();
  display::startLvgl(mem::drawBuf1(), mem::drawBuf2(), cfg::DRAW_BUF_BYTES);
  step("ekran");
  if (!sdcard::begin()) diag::note("boot", "veri klasoru olusturulamadi");
  step("sd");
  weather::loadCache();
  quake::loadCache();
  events::begin();
  step("veri");
  media::begin();
  ui::begin();
  step("arayuz");
  net::begin();
  step("ag");
  wiz::begin();
  step("wiz");
  mem::report(Serial);
  reportSteps();
  pi::healthBegin();
  pi::watchdogBegin();
  diag::note("boot", "hazir");
  while (!s_stop) {
    pi::watchdogKick();
    ui::loop();
    diag::loop();
    media::loop();
    agents::loop();
    fwupdate::loop();
    relay::service();
    cam::loop();
    app::poll();
  }
  diag::note("boot", "durduruluyor");
  pi::markCleanExit();
  fflush(stdout);
  return 0;
}
