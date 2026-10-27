#pragma once

#include <Print.h>
#include <lvgl.h>
#include <stdint.h>

#include "media_proto.h"

namespace media {

enum Cmd : uint8_t {
  CMD_PLAY_PAUSE = 0,
  CMD_NEXT,
  CMD_PREV,
  CMD_VOL_UP,
  CMD_VOL_DOWN,
};

struct State {
  bool link;
  bool active;
  bool playing;
  uint32_t durMs;
  uint32_t accent;
  uint32_t track;
  int8_t vol;
  char app[20];
  char title[96];
  char artist[64];
  char album[64];
};

struct PlaylistItem {
  char name[40];
  uint16_t total;
  uint8_t sheet;
  uint8_t cell;
};

struct Playlists {
  uint32_t ver;
  uint8_t status;
  uint8_t n;
  bool shuffle;
  int8_t playing;
  int8_t pick;
  uint8_t pickResult;
  uint32_t pickAt;
  uint32_t started;
  char msg[48];
  char pickMsg[48];
  uint32_t sheet[rc::PL_SHEETS];
  PlaylistItem item[rc::PL_MAX];
};

void begin();
void loop();
void lineBegin();
void lineChar(char c);
void lineEnd();
bool receiving();
bool receive();
void discardUntilQuiet();
bool discarding();
void discard();

const State& state();
uint32_t version();
uint32_t positionMs();
const lv_image_dsc_t* art();
bool tempo(uint32_t& periodMs, uint32_t& beatMs);
void send(Cmd c);
void seek(uint32_t ms);
void report(Print& out);

const Playlists& playlists();
void playlistsOpen(bool open);
void playPlaylist(int index);
void setShuffle(bool on);
const lv_image_dsc_t* playlistSheet(int index, int32_t& dx, int32_t& dy);
void wantPhoto(uint32_t hash, const char* name);
const lv_image_dsc_t* photo(uint32_t hash);

}
