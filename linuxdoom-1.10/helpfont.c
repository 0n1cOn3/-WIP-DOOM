#include "helpfont.h"

#include <limits.h>

#include "doomtype.h"
#include "v_video.h"
#include "w_wad.h"
#include "z_zone.h"
#include "helpfont_data.h"

typedef enum
{
    HELPFONT_RED = 0,
    HELPFONT_GREEN,
    HELPFONT_GOLD,
    HELPFONT_WHITE
} helpfont_color_t;

typedef struct
{
    int x;
    int y;
    helpfont_color_t color;
    const char *text;
} helpfont_line_t;

static byte help_bg_pal[HELPFONT_BG_W * HELPFONT_BG_H];
static boolean help_bg_cached = false;

static const uint8_t *HelpFont_GetAtlas(helpfont_color_t color)
{
    switch (color)
    {
        case HELPFONT_RED:
            return helpfont_atlas_red;
        case HELPFONT_GREEN:
            return helpfont_atlas_green;
        case HELPFONT_GOLD:
            return helpfont_atlas_gold;
        case HELPFONT_WHITE:
            return helpfont_atlas_white;
        default:
            return helpfont_atlas_white;
    }
}

static byte HelpFont_ClosestPaletteIndex(const byte *palette, int r, int g, int b)
{
    int best = 0;
    int bestdist = INT_MAX;

    for (int i = 0; i < 256; ++i)
    {
        int dr = r - palette[i * 3];
        int dg = g - palette[i * 3 + 1];
        int db = b - palette[i * 3 + 2];
        int dist = dr * dr + dg * dg + db * db;
        if (dist < bestdist)
        {
            bestdist = dist;
            best = i;
            if (dist == 0)
                break;
        }
    }

    return (byte)best;
}

static void HelpFont_BuildBackground(void)
{
    const byte *palette = (const byte *)W_CacheLumpName("PLAYPAL", PU_CACHE);
    const uint8_t *src = helpfont_bg_rgb;
    byte *dst = help_bg_pal;
    const int pixels = HELPFONT_BG_W * HELPFONT_BG_H;

    for (int i = 0; i < pixels; ++i)
    {
        int r = src[i * 3];
        int g = src[i * 3 + 1];
        int b = src[i * 3 + 2];
        dst[i] = HelpFont_ClosestPaletteIndex(palette, r, g, b);
    }

    help_bg_cached = true;
}

static int HelpFont_TextWidth(const char *text)
{
    int width = 0;
    const unsigned char *p = (const unsigned char *)text;

    while (*p)
    {
        unsigned char ch = *p++;
        if (ch < 128)
        {
            const helpfont_glyph_t *glyph = &helpfont_glyphs[ch];
            if (glyph->advance)
                width += glyph->advance;
            else if (glyph->w)
                width += glyph->w;
            else
                width += 4;
        }
        else
        {
            width += 4;
        }
    }

    return width;
}

static void HelpFont_DrawText(int x, int y, helpfont_color_t color, const char *text)
{
    const uint8_t *atlas = HelpFont_GetAtlas(color);
    const byte *palette = (const byte *)W_CacheLumpName("PLAYPAL", PU_CACHE);
    const unsigned char *p = (const unsigned char *)text;
    int cursor_x = x;
    int cursor_y = y;

    while (*p)
    {
        unsigned char ch = *p++;
        if (ch >= 128)
            continue;

        const helpfont_glyph_t *glyph = &helpfont_glyphs[ch];
        if (glyph->advance == 0 && glyph->w == 0)
        {
            cursor_x += 4;
            continue;
        }

        int draw_x = cursor_x + glyph->offset_x;
        int draw_y = cursor_y + glyph->offset_y;
        int w = glyph->w;
        int h = glyph->h;

        for (int row = 0; row < h; ++row)
        {
            int dst_y = draw_y + row;
            if (dst_y < 0 || dst_y >= SCREENHEIGHT)
                continue;

            for (int col = 0; col < w; ++col)
            {
                int dst_x = draw_x + col;
                if (dst_x < 0 || dst_x >= SCREENWIDTH)
                    continue;

                int atlas_x = glyph->x + col;
                int atlas_y = glyph->y + row;
                int atlas_idx = (atlas_y * HELPFONT_ATLAS_W + atlas_x) * 4;
                uint8_t a = atlas[atlas_idx + 3];
                if (!a)
                    continue;

                int r = atlas[atlas_idx];
                int g = atlas[atlas_idx + 1];
                int b = atlas[atlas_idx + 2];
                byte color_index = HelpFont_ClosestPaletteIndex(palette, r, g, b);
                screens[0][dst_y * SCREENWIDTH + dst_x] = color_index;
            }
        }

        cursor_x += glyph->advance ? glyph->advance : glyph->w;
    }
}

static void HelpFont_DrawLabelValue(int x, int y, const char *label, const char *value)
{
    HelpFont_DrawText(x, y, HELPFONT_GOLD, label);
    HelpFont_DrawText(x + HelpFont_TextWidth(label), y, HELPFONT_GREEN, value);
}

static void HelpFont_DrawHelpPage1(void)
{
    const int line = 10;
    const int col1 = 12;
    const int col2 = 116;
    const int col3 = 200;

    HelpFont_DrawText(12, 6, HELPFONT_RED, "HELP");
    HelpFont_DrawLabelValue(86, 6, "PAUSE KEY = ", "PAUSE");
    HelpFont_DrawLabelValue(200, 6, "TAB = ", "AUTOMAP");
    HelpFont_DrawLabelValue(86, 16, "ESC KEY = ", "MENU");

    int y = 30;
    HelpFont_DrawText(12, y, HELPFONT_WHITE, "FUNCTION KEYS");
    y += line;
    HelpFont_DrawLabelValue(col1, y, "F1 = ", "HELP");
    HelpFont_DrawLabelValue(col2, y, "F3 = ", "LOAD");
    HelpFont_DrawLabelValue(col3, y, "F4 = ", "SOUND VOL");
    y += line;
    HelpFont_DrawLabelValue(col1, y, "F2 = ", "SAVE");
    HelpFont_DrawLabelValue(col2, y, "F6 = ", "QUICK SAVE");
    HelpFont_DrawLabelValue(col3, y, "F7 = ", "END GAME");
    y += line;
    HelpFont_DrawLabelValue(col1, y, "F5 = ", "DETAIL");
    HelpFont_DrawLabelValue(col2, y, "F9 = ", "QUICK LOAD");
    HelpFont_DrawLabelValue(col3, y, "F10 = ", "QUIT");
    y += line;
    HelpFont_DrawLabelValue(col1, y, "F8 = ", "MESSAGES");
    HelpFont_DrawLabelValue(col2, y, "+ = ", "VIEW UP");
    HelpFont_DrawLabelValue(col3, y, "F11 = ", "GAMMA");
    y += line;
    HelpFont_DrawLabelValue(col1, y, "- = ", "VIEW DOWN");

    y += line + 2;
    HelpFont_DrawText(12, y, HELPFONT_WHITE, "AUTOMAP FUNCTIONS");
    y += line;
    HelpFont_DrawLabelValue(col1, y, "F = ", "FOLLOW MODE");
    HelpFont_DrawLabelValue(col2, y, "M = ", "MARK PLACE");
    HelpFont_DrawLabelValue(col3, y, "C = ", "CLEAR MARKS");
    y += line;
    HelpFont_DrawLabelValue(col1, y, "+ = ", "ZOOM IN");
    HelpFont_DrawLabelValue(col2, y, "- = ", "ZOOM OUT");
    HelpFont_DrawLabelValue(col3, y, "0 = ", "FULL ZOOM");

    y += line + 2;
    HelpFont_DrawText(12, y, HELPFONT_WHITE, "WEAPONS");
    y += line;
    HelpFont_DrawLabelValue(col1, y, "1 = ", "FIST");
    HelpFont_DrawLabelValue(col2, y, "2 = ", "PISTOL");
    HelpFont_DrawLabelValue(col3, y, "3 = ", "SHOTGUN");
    y += line;
    HelpFont_DrawLabelValue(col1, y, "4 = ", "CHAINGUN");
    HelpFont_DrawLabelValue(col2, y, "5 = ", "ROCKET");
    HelpFont_DrawLabelValue(col3, y, "6 = ", "PLASMA");
    y += line;
    HelpFont_DrawLabelValue(col1, y, "7 = ", "BFG 9000");
}

static void HelpFont_DrawHelpPage2(void)
{
    const int line = 10;
    const int col1 = 12;

    HelpFont_DrawText(12, 6, HELPFONT_RED, "HELP");
    HelpFont_DrawText(70, 6, HELPFONT_WHITE, "PAGE 2/2");
    HelpFont_DrawLabelValue(172, 6, "TAB = ", "AUTOMAP");

    int y = 30;
    HelpFont_DrawText(12, y, HELPFONT_WHITE, "MOVEMENT");
    y += line;
    HelpFont_DrawLabelValue(col1, y, "MOVE FORWARD = ", "UP ARROW");
    y += line;
    HelpFont_DrawLabelValue(col1, y, "MOVE BACKWARD = ", "DOWN ARROW");
    y += line;
    HelpFont_DrawLabelValue(col1, y, "TURN LEFT = ", "LEFT ARROW");
    y += line;
    HelpFont_DrawLabelValue(col1, y, "TURN RIGHT = ", "RIGHT ARROW");
    y += line;
    HelpFont_DrawLabelValue(col1, y, "RUN = ", "SHIFT + ARROW");
    y += line;
    HelpFont_DrawLabelValue(col1, y, "STRAFE = ", "ALT + ARROW");

    y += line + 4;
    HelpFont_DrawText(12, y, HELPFONT_WHITE, "ACTIONS");
    y += line;
    HelpFont_DrawLabelValue(col1, y, "FIRE = ", "CTRL, MOUSE1, JOY1");
    y += line;
    HelpFont_DrawLabelValue(col1, y, "USE/OPEN = ", "MOUSE2, JOY2");
    y += line;
    HelpFont_DrawLabelValue(col1, y, "JUMP = ", "SPACE");
}

void HelpScreen_Draw(int page)
{
    if (!help_bg_cached)
        HelpFont_BuildBackground();

    V_DrawBlock(0, 0, 0, HELPFONT_BG_W, HELPFONT_BG_H, help_bg_pal);

    if (page == 2)
        HelpFont_DrawHelpPage2();
    else
        HelpFont_DrawHelpPage1();
}
