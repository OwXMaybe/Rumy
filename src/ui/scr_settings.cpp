#include <stdio.h>
#include <string.h>

#include "app/app.h"
#include "app/config.h"
#include "fonts/symbols.h"
#include "rows.h"
#include "screens.h"
#include "settings/settings.h"
#include "text_tr.h"
#include "theme.h"
#include "ui.h"
#include "webui/webui.h"

namespace scr_settings {

namespace {

const uint8_t kQuakeMags[] = {20, 25, 30, 35, 40};

lv_obj_t* s_scr = nullptr;
lv_obj_t* s_city = nullptr;
lv_obj_t* s_bulb = nullptr;
lv_obj_t* s_wifi = nullptr;
lv_obj_t* s_display = nullptr;
lv_obj_t* s_night = nullptr;
lv_obj_t* s_phone = nullptr;
lv_obj_t* s_quake = nullptr;
lv_obj_t* s_maint = nullptr;
lv_obj_t* s_overlay = nullptr;
app::Action s_confirm = app::ACT_NONE;

using theme::setText;

void formatSeconds(char* buf, size_t cap, uint32_t s) {
  if (s < 60) snprintf(buf, cap, "%u sn", static_cast<unsigned>(s));
  else snprintf(buf, cap, "%u dk", static_cast<unsigned>(s / 60));
}

void refreshWeb() {
  uint8_t n = webui::phones();
  char buf[24];
  if (n) snprintf(buf, sizeof(buf), "%u eşli", static_cast<unsigned>(n));
  else snprintf(buf, sizeof(buf), "Eşleştir");
  rows::setNavValue(s_phone, buf);
}

void refreshWifi() {
  const ui::Data& d = ui::data();
  char buf[40];
  if (d.net.wifi) snprintf(buf, sizeof(buf), "Bağlı · %d dBm", static_cast<int>(d.net.rssi));
  else snprintf(buf, sizeof(buf), "Bağlantı yok");
  rows::setNavValue(s_wifi, buf);
}

void refresh() {
  const Settings& st = ui::data().settings;
  char buf[48];
  char a[12];
  char b[12];
  rows::setNavValue(s_city, st.city);
  rows::setNavValue(s_bulb, st.bulbMac[0] ? (st.bulbName[0] ? st.bulbName : st.bulbMac) : "Eşleştirilmedi");
  formatSeconds(a, sizeof(a), st.homeTimeoutS);
  snprintf(buf, sizeof(buf), "%%%u · %s", static_cast<unsigned>(st.brightness), a);
  rows::setNavValue(s_display, buf);
  if (st.nightMode == NIGHT_OFF) {
    snprintf(buf, sizeof(buf), "Kapalı");
  } else if (st.nightMode == NIGHT_SUN) {
    snprintf(buf, sizeof(buf), "Güneşe göre");
  } else {
    rc::formatHm(a, sizeof(a), static_cast<uint8_t>(st.nightStart / 60), static_cast<uint8_t>(st.nightStart % 60));
    rc::formatHm(b, sizeof(b), static_cast<uint8_t>(st.nightEnd / 60), static_cast<uint8_t>(st.nightEnd % 60));
    snprintf(buf, sizeof(buf), "%s\xE2\x80\x93%s", a, b);
  }
  rows::setNavValue(s_night, buf);
  snprintf(buf, sizeof(buf), "M%u,%u", static_cast<unsigned>(st.quakeMinMag10 / 10),
           static_cast<unsigned>(st.quakeMinMag10 % 10));
  setText(s_quake, buf);
  rows::setChecked(s_maint, st.maintReboot != 0);
  refreshWeb();
  refreshWifi();
}

void closeOverlay() {
  if (s_overlay) lv_obj_delete_async(s_overlay);
  s_overlay = nullptr;
}

void confirmNo(lv_event_t* e) {
  (void)e;
  s_confirm = app::ACT_NONE;
  closeOverlay();
}

void confirmYes(lv_event_t* e) {
  (void)e;
  app::Action a = s_confirm;
  s_confirm = app::ACT_NONE;
  closeOverlay();
  if (a == app::ACT_NONE) return;
  ui::toast("Yeniden başlatılıyor…");
  app::request(a);
}

void askConfirm(const char* text, const char* yes, app::Action a) {
  closeOverlay();
  s_confirm = a;
  s_overlay = theme::makeBox(lv_layer_top());
  lv_obj_set_size(s_overlay, theme::SW, theme::SH);
  lv_obj_set_style_bg_color(s_overlay, theme::col(theme::C_BG), 0);
  lv_obj_set_style_bg_opa(s_overlay, LV_OPA_90, 0);
  lv_obj_add_flag(s_overlay, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_t* l = theme::makeLabel(s_overlay, theme::fontM(), theme::C_TEXT);
  lv_label_set_long_mode(l, LV_LABEL_LONG_MODE_WRAP);
  lv_obj_set_width(l, theme::FW(280));
  lv_obj_set_style_text_align(l, LV_TEXT_ALIGN_CENTER, 0);
  lv_label_set_text_static(l, text);
  lv_obj_align(l, LV_ALIGN_TOP_MID, 0, theme::S(26));
  lv_obj_t* no = theme::makeButton(s_overlay, "Vazgeç", theme::fontM());
  lv_obj_set_size(no, theme::S(130), theme::S(46));
  lv_obj_align(no, LV_ALIGN_BOTTOM_LEFT, theme::S(20), -theme::S(24));
  lv_obj_add_event_cb(no, confirmNo, LV_EVENT_CLICKED, nullptr);
  lv_obj_t* ok = theme::makeButton(s_overlay, yes, theme::fontM());
  lv_obj_set_size(ok, theme::S(130), theme::S(46));
  lv_obj_align(ok, LV_ALIGN_BOTTOM_RIGHT, -theme::S(20), -theme::S(24));
  lv_obj_set_style_border_color(ok, theme::col(theme::C_DANGER), 0);
  lv_obj_set_style_text_color(ok, theme::col(theme::C_DANGER), 0);
  lv_obj_add_event_cb(ok, confirmYes, LV_EVENT_CLICKED, nullptr);
}

template <typename T, size_t N>
T cycle(const T (&opts)[N], T cur, int dir) {
  size_t idx = N - 1;
  for (size_t i = 0; i < N; i++) {
    if (opts[i] >= cur) {
      idx = i;
      break;
    }
  }
  int32_t n = static_cast<int32_t>(idx) + dir;
  if (n < 0) n = 0;
  if (n >= static_cast<int32_t>(N)) n = static_cast<int32_t>(N) - 1;
  return opts[n];
}

void quakeStep(lv_event_t* e) {
  Settings st = settings::get();
  st.quakeMinMag10 = cycle(kQuakeMags, st.quakeMinMag10, rows::stepDir(e));
  settings::set(st);
}

void goPhone(lv_event_t* e) {
  (void)e;
  ui::go(ui::SCR_PHONE, 1);
}

void maintEvent(lv_event_t* e) {
  Settings st = settings::get();
  st.maintReboot = lv_obj_has_state(lv_event_get_target_obj(e), LV_STATE_CHECKED) ? 1 : 0;
  settings::set(st);
}

void goHome(lv_event_t* e) {
  (void)e;
  ui::go(ui::SCR_HOME, -1);
}

void goLocation(lv_event_t* e) {
  (void)e;
  ui::go(ui::SCR_LOCATION, 1);
}

void goPair(lv_event_t* e) {
  (void)e;
  ui::go(ui::SCR_PAIR, 1);
}

void goDisplay(lv_event_t* e) {
  (void)e;
  ui::go(ui::SCR_DISPLAY_CFG, 1);
}

void goNight(lv_event_t* e) {
  (void)e;
  ui::go(ui::SCR_NIGHT_CFG, 1);
}

void goDiag(lv_event_t* e) {
  (void)e;
  ui::go(ui::SCR_DIAG, 1);
}

void goInfo(lv_event_t* e) {
  (void)e;
  ui::go(ui::SCR_INFO, 1);
}

void wifiClick(lv_event_t* e) {
  (void)e;
  askConfirm("WiFi bilgileri silinecek ve Rumy kurulum moduna geçecek.\nTelefonundan \"Rumy-Kurulum\" ağına bağlanıp yeniden kurabilirsin.",
             "Sıfırla", app::ACT_WIFI_RESET);
}

void factoryClick(lv_event_t* e) {
  (void)e;
  askConfirm("Tüm ayarlar, konum, ampul eşleşmesi ve WiFi silinecek.\nEmin misin?", "Hepsini sil",
             app::ACT_FACTORY_RESET);
}

void deleted(lv_event_t* e) {
  (void)e;
  closeOverlay();
  s_confirm = app::ACT_NONE;
  s_scr = s_city = s_bulb = s_wifi = s_display = s_night = s_phone = s_quake = s_maint = nullptr;
}

}

lv_obj_t* create() {
  s_scr = rows::screen();
  lv_obj_add_event_cb(s_scr, deleted, LV_EVENT_DELETE, nullptr);
  rows::header(s_scr, RS_SETTINGS "  Ayarlar", true, goHome);

  rows::section(s_scr, "GENEL");
  rows::nav(s_scr, RS_PIN "  Konum", goLocation, &s_city);
  rows::nav(s_scr, RS_BULB "  Ampul", goPair, &s_bulb);
  rows::nav(s_scr, RS_WIFI "  WiFi", wifiClick, &s_wifi);

  rows::section(s_scr, "EKRAN");
  rows::nav(s_scr, RS_SUN "  Ekran", goDisplay, &s_display);
  rows::nav(s_scr, RS_MOON "  Gece modu", goNight, &s_night);

  rows::section(s_scr, "BAĞLANTI");
  rows::nav(s_scr, RS_GLOBE "  Telefon", goPhone, &s_phone);
  lv_obj_t* r = rows::row(s_scr, RS_WARNING "  Deprem eşiği", rows::H);
  s_quake = rows::stepper(r, quakeStep);

  rows::section(s_scr, "SİSTEM");
  r = rows::row(s_scr, RS_REFRESH "  04:00 yeniden başlat", rows::H);
  s_maint = rows::toggle(r, maintEvent);
  rows::nav(s_scr, RS_CHART "  Tanılama", goDiag, nullptr);
  rows::nav(s_scr, RS_INFO "  Hakkında", goInfo, nullptr);
  r = rows::nav(s_scr, RS_TRASH "  Fabrika ayarları", factoryClick, nullptr);
  lv_obj_set_style_border_color(r, theme::col(0x4A2528), 0);

  refresh();
  return s_scr;
}

void update(uint32_t mask) {
  if (!s_scr) return;
  if (mask & ui::M_SETTINGS) refresh();
  if (mask & ui::M_NET) {
    refreshWeb();
    refreshWifi();
  }
}

}
