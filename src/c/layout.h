#pragma once
#include <pebble.h>

#define SCALE_W(bounds, val) ((int16_t)((val) * (bounds).size.w / 144))
#define SCALE_H(bounds, val) ((int16_t)((val) * (bounds).size.h / 168))

static inline const char *font_for_height(const GRect bounds, const char *base, const char *emery, const char *gabbro) {
    if (bounds.size.h >= 228) return gabbro;
    if (bounds.size.h >= 200) return emery;
    return base;
}
