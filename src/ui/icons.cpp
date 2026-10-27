#include "icons.h"

#include "app/config.h"
#include "icons_embedded.h"

namespace icons {

namespace {

#if RUMY_SCREEN_H >= 320
const lv_image_dsc_t* const* const kSets[SIZE_COUNT] = {ICONS_85, ICONS_64, ICONS_37, ICONS_32};
constexpr int32_t kPx[SIZE_COUNT] = {85, 64, 37, 32};
#else
const lv_image_dsc_t* const* const kSets[SIZE_COUNT] = {ICONS_64, ICONS_48, ICONS_28, ICONS_24};
constexpr int32_t kPx[SIZE_COUNT] = {64, 48, 28, 24};
#endif

}

const lv_image_dsc_t* get(rc::IconId id, Size s) {
  if (id >= rc::IC_COUNT) id = rc::IC_OVERCAST;
  if (s >= SIZE_COUNT) s = S48;
  return kSets[s][id];
}

int32_t pixels(Size s) { return kPx[s < SIZE_COUNT ? s : S48]; }

}
