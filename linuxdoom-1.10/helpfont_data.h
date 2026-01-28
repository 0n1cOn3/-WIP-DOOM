#ifndef HELPFONT_DATA_H
#define HELPFONT_DATA_H

#include <stdint.h>

#define HELPFONT_ATLAS_W 176
#define HELPFONT_ATLAS_H 40
#define HELPFONT_BG_W 320
#define HELPFONT_BG_H 200

typedef struct helpfont_glyph_s {
    uint8_t x;
    uint8_t y;
    uint8_t w;
    uint8_t h;
    uint8_t advance;
    int8_t offset_x;
    int8_t offset_y;
} helpfont_glyph_t;

extern const uint8_t helpfont_atlas_red[HELPFONT_ATLAS_W * HELPFONT_ATLAS_H * 4];
extern const uint8_t helpfont_atlas_green[HELPFONT_ATLAS_W * HELPFONT_ATLAS_H * 4];
extern const uint8_t helpfont_atlas_gold[HELPFONT_ATLAS_W * HELPFONT_ATLAS_H * 4];
extern const uint8_t helpfont_atlas_white[HELPFONT_ATLAS_W * HELPFONT_ATLAS_H * 4];
extern const helpfont_glyph_t helpfont_glyphs[128];
extern const uint8_t helpfont_bg_rgb[HELPFONT_BG_W * HELPFONT_BG_H * 3];

#endif
