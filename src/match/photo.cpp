#include "photo.h"

#include "app/config.h"
#include "media/media.h"
#include "media_proto.h"

namespace photo {

namespace {

uint32_t hashOf(const char* player) { return rc::photoHash(player, static_cast<uint16_t>(cfg::PHOTO_PX)); }

}

void request(const char* player) {
  if (!player || !player[0]) return;
  media::wantPhoto(hashOf(player), player);
}

const lv_image_dsc_t* get(const char* player) {
  if (!player || !player[0]) return nullptr;
  return media::photo(hashOf(player));
}

}
