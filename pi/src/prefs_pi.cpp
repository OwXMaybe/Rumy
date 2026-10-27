#include <ArduinoJson.h>
#include <Preferences.h>
#include <stdio.h>
#include <string.h>

#include "paths.h"

namespace {

constexpr size_t FILE_MAX = 16384;

void filePath(const char* ns, char* out, size_t cap) {
  const char* name = !strcmp(ns, "rumy") ? "ayarlar" : ns;
  snprintf(out, cap, "%s/%s.json", pi::configDir(), name);
}

JsonDocument& doc(void* p) { return *static_cast<JsonDocument*>(p); }

}

Preferences::~Preferences() { end(); }

bool Preferences::begin(const char* ns, bool readOnly) {
  end();
  snprintf(ns_, sizeof(ns_), "%s", ns ? ns : "rumy");
  readOnly_ = readOnly;
  dirty_ = false;
  JsonDocument* d = new JsonDocument;
  char path[512];
  filePath(ns_, path, sizeof(path));
  static uint8_t buf[FILE_MAX];
  size_t len = 0;
  if (pi::readAll(path, buf, sizeof(buf), len) && len) {
    if (deserializeJson(*d, reinterpret_cast<const char*>(buf), len) != DeserializationError::Ok || !d->is<JsonObject>())
      d->to<JsonObject>();
  } else {
    d->to<JsonObject>();
  }
  doc_ = d;
  open_ = true;
  return true;
}

void Preferences::save() {
  if (!open_ || readOnly_ || !dirty_) return;
  static char out[FILE_MAX];
  size_t n = serializeJsonPretty(doc(doc_), out, sizeof(out) - 1);
  if (!n || n >= sizeof(out) - 1) return;
  out[n++] = '\n';
  char path[512];
  filePath(ns_, path, sizeof(path));
  if (pi::writeAtomic(path, out, n, false)) dirty_ = false;
}

void Preferences::end() {
  if (!open_) return;
  save();
  delete static_cast<JsonDocument*>(doc_);
  doc_ = nullptr;
  open_ = false;
}

bool Preferences::clear() {
  if (!open_ || readOnly_) return false;
  doc(doc_).to<JsonObject>();
  dirty_ = true;
  return true;
}

bool Preferences::isKey(const char* key) { return open_ && key && !doc(doc_)[key].isNull(); }

size_t Preferences::getString(const char* key, char* out, size_t cap) {
  if (!open_ || !key || !out || !cap) return 0;
  JsonVariantConst v = doc(doc_)[key];
  if (!v.is<const char*>()) return 0;
  const char* s = v.as<const char*>();
  size_t n = strlen(s);
  if (n + 1 > cap) return 0;
  memcpy(out, s, n + 1);
  return n + 1;
}

size_t Preferences::putString(const char* key, const char* v) {
  if (!open_ || readOnly_ || !key || !v) return 0;
  JsonVariantConst cur = doc(doc_)[key];
  if (!cur.is<const char*>() || strcmp(cur.as<const char*>(), v) != 0) {
    doc(doc_)[key] = v;
    dirty_ = true;
  }
  return strlen(v);
}

bool Preferences::number(const char* key, double& out) {
  if (!open_ || !key) return false;
  JsonVariantConst v = doc(doc_)[key];
  if (v.is<float>() || v.is<long>() || v.is<unsigned long>()) {
    out = v.as<float>();
    if (v.is<long>()) out = static_cast<double>(v.as<long>());
    else if (v.is<unsigned long>()) out = static_cast<double>(v.as<unsigned long>());
    return true;
  }
  return false;
}

size_t Preferences::putNumber(const char* key, double v, size_t width) {
  if (!open_ || readOnly_ || !key) return 0;
  double cur = 0;
  if (!number(key, cur) || cur != v) {
    if (v == static_cast<double>(static_cast<long>(v))) doc(doc_)[key] = static_cast<long>(v);
    else doc(doc_)[key] = static_cast<float>(v);
    dirty_ = true;
  }
  return width;
}

float Preferences::getFloat(const char* key, float d) {
  double v;
  return number(key, v) ? static_cast<float>(v) : d;
}

size_t Preferences::putFloat(const char* key, float v) {
  if (!open_ || readOnly_ || !key) return 0;
  double cur = 0;
  if (!number(key, cur) || static_cast<float>(cur) != v) {
    doc(doc_)[key] = v;
    dirty_ = true;
  }
  return 4;
}

uint8_t Preferences::getUChar(const char* key, uint8_t d) {
  double v;
  return number(key, v) ? static_cast<uint8_t>(v) : d;
}

size_t Preferences::putUChar(const char* key, uint8_t v) { return putNumber(key, v, 1); }

uint16_t Preferences::getUShort(const char* key, uint16_t d) {
  double v;
  return number(key, v) ? static_cast<uint16_t>(v) : d;
}

size_t Preferences::putUShort(const char* key, uint16_t v) { return putNumber(key, v, 2); }

uint32_t Preferences::getUInt(const char* key, uint32_t d) {
  double v;
  return number(key, v) ? static_cast<uint32_t>(v) : d;
}

size_t Preferences::putUInt(const char* key, uint32_t v) { return putNumber(key, v, 4); }
