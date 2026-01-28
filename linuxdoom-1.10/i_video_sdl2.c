// Emacs style mode select   -*- C++ -*-
//-----------------------------------------------------------------------------
//
// Simplified SDL2 video backend for linuxdoom.
//
//-----------------------------------------------------------------------------

#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <SDL2/SDL.h>

#include "doomdef.h"
#include "doomstat.h"
#include "d_event.h"
#include "d_main.h"
#include "i_system.h"
#include "i_video.h"
#include "m_menu.h"
#include "m_argv.h"
#include "v_video.h"

static SDL_Window *window = NULL;
static SDL_Renderer *renderer = NULL;
static SDL_Texture *texture = NULL;
static uint32_t *video_buffer = NULL;
static uint32_t palette_table[256];

static int ScreenWidth = SCREENWIDTH;
static int ScreenHeight = SCREENHEIGHT;

static int mouse_button_state = 0;
extern boolean menuactive;

int vid_window_width = 0;     // 0 = detect desktop resolution and scale appropriately
int vid_window_height = 0;
int vid_fullscreen = 0;
int vid_aspect = 0;        // 0 = 4:3, 1 = 16:9, 2 = stretch
int vid_integer_scale = 1;

static void I_UpdateRendererLogicalSize(void)
{
    if (!renderer)
        return;

    // Stretch mode: disable logical sizing so the texture fills the window.
    if (vid_aspect == 2)
    {
        SDL_RenderSetLogicalSize(renderer, 0, 0);
        SDL_RenderSetIntegerScale(renderer, SDL_FALSE);
        return;
    }

    // 4:3 mode applies a 1.2x vertical stretch (320x200 -> 320x240) to emulate
    // DOOM's original pixel aspect. 16:9 uses a 320x180 logical space.
    {
        int logical_w = ScreenWidth;
        int logical_h = ScreenHeight;

        if (vid_aspect == 1)
            logical_h = (int)lroundf((float)ScreenWidth * 9.0f / 16.0f);
        else
            logical_h = (int)lroundf((float)ScreenHeight * 6.0f / 5.0f);

        if (logical_h <= 0)
            logical_h = ScreenHeight;

        SDL_RenderSetLogicalSize(renderer, logical_w, logical_h);
        SDL_RenderSetIntegerScale(renderer, vid_integer_scale ? SDL_TRUE : SDL_FALSE);
    }
}

void I_GetDesktopResolution(int *width, int *height)
{
    SDL_DisplayMode mode;

    if (width)
        *width = ScreenWidth * 2;
    if (height)
        *height = ScreenHeight * 2;

    if (SDL_GetCurrentDisplayMode(0, &mode) == 0 && mode.w > 0 && mode.h > 0)
    {
        if (width)
            *width = mode.w;
        if (height)
            *height = mode.h;
    }
}

static void I_GetRenderDestRect(SDL_Rect *dest)
{
    int output_width = 0;
    int output_height = 0;
    if (SDL_GetRendererOutputSize(renderer, &output_width, &output_height) != 0 ||
        output_width <= 0 || output_height <= 0)
    {
        dest->x = 0;
        dest->y = 0;
        dest->w = ScreenWidth;
        dest->h = ScreenHeight;
        return;
    }

    if (vid_aspect == 2)
    {
        dest->x = 0;
        dest->y = 0;
        dest->w = output_width;
        dest->h = output_height;
        return;
    }

    const float base_width = (float)ScreenWidth;
    const float base_height = (vid_aspect == 1)
        ? (base_width * 9.0f / 16.0f)
        : ((float)ScreenHeight * 6.0f / 5.0f);
    float scale = fminf(output_width / base_width, output_height / base_height);
    if (vid_integer_scale)
    {
        int integer_scale = (int)scale;
        if (integer_scale >= 1)
        {
            scale = (float)integer_scale;
        }
    }

    dest->w = (int)lroundf(base_width * scale);
    dest->h = (int)lroundf(base_height * scale);
    dest->x = (output_width - dest->w) / 2;
    dest->y = (output_height - dest->h) / 2;
}

static uint8_t scale_palette_value(uint8_t value)
{
    return value;
}

static int sdl_translate_key(SDL_Keycode key)
{
    switch (key)
    {
        case SDLK_LEFT: return KEY_LEFTARROW;
        case SDLK_RIGHT: return KEY_RIGHTARROW;
        case SDLK_DOWN: return KEY_DOWNARROW;
        case SDLK_UP: return KEY_UPARROW;
        case SDLK_ESCAPE: return KEY_ESCAPE;
        case SDLK_RETURN: return KEY_ENTER;
        case SDLK_TAB: return KEY_TAB;
        case SDLK_F1: return KEY_F1;
        case SDLK_F2: return KEY_F2;
        case SDLK_F3: return KEY_F3;
        case SDLK_F4: return KEY_F4;
        case SDLK_F5: return KEY_F5;
        case SDLK_F6: return KEY_F6;
        case SDLK_F7: return KEY_F7;
        case SDLK_F8: return KEY_F8;
        case SDLK_F9: return KEY_F9;
        case SDLK_F10: return KEY_F10;
        case SDLK_F11: return KEY_F11;
        case SDLK_F12: return KEY_F12;
        case SDLK_BACKSPACE: return KEY_BACKSPACE;
        case SDLK_PAUSE: return KEY_PAUSE;
        case SDLK_EQUALS: return KEY_EQUALS;
        case SDLK_KP_EQUALS: return KEY_EQUALS;
        case SDLK_MINUS: return KEY_MINUS;
        case SDLK_LSHIFT:
        case SDLK_RSHIFT: return KEY_RSHIFT;
        case SDLK_LCTRL:
        case SDLK_RCTRL: return KEY_RCTRL;
        case SDLK_LALT:
        case SDLK_RALT: return KEY_RALT;
        default:
            if (key >= SDLK_SPACE && key <= SDLK_z)
            {
                return (int)key;
            }
            return 0;
    }
}

void I_ShutdownGraphics(void)
{
    if (video_buffer)
    {
        free(video_buffer);
        video_buffer = NULL;
    }

    if (texture)
    {
        SDL_DestroyTexture(texture);
        texture = NULL;
    }

    if (renderer)
    {
        SDL_DestroyRenderer(renderer);
        renderer = NULL;
    }

    if (window)
    {
        SDL_DestroyWindow(window);
        window = NULL;
    }

    SDL_QuitSubSystem(SDL_INIT_VIDEO);
}

void I_InitGraphics(void)
{
    if (M_CheckParm("-widescreen"))
        vid_aspect = 1;
    if (M_CheckParm("-stretch"))
        vid_aspect = 2;
    if (M_CheckParm("-fullscreen"))
        vid_fullscreen = 1;
    if (M_CheckParm("-windowed"))
        vid_fullscreen = 0;

    if (SDL_InitSubSystem(SDL_INIT_VIDEO) < 0)
    {
        I_Error("SDL video init failed: %s", SDL_GetError());
    }

    if (vid_window_width <= 0 || vid_window_height <= 0)
    {
        int desktop_width = 0, desktop_height = 0;
        I_GetDesktopResolution(&desktop_width, &desktop_height);

        if (desktop_width > 0 && desktop_height > 0)
        {
            // For fullscreen, use native desktop resolution
            if (vid_fullscreen)
            {
                vid_window_width = desktop_width;
                vid_window_height = desktop_height;
            }
            else
            {
                // For windowed mode, use 75% of desktop resolution
                int scaled_width = (desktop_width * 75) / 100;
                int scaled_height = (desktop_height * 75) / 100;

                // Apply aspect ratio constraint (320x240 for 4:3)
                float base_width = (float)ScreenWidth;
                float base_height = (vid_aspect == 1)
                    ? (base_width * 9.0f / 16.0f)
                    : ((float)ScreenHeight * 6.0f / 5.0f);
                float scale = fminf(scaled_width / base_width, scaled_height / base_height);

                // Use integer scaling if enabled
                if (vid_integer_scale)
                {
                    int integer_scale = (int)scale;
                    if (integer_scale >= 1)
                        scale = (float)integer_scale;
                }

                // Calculate final window size
                vid_window_width = (int)lroundf(base_width * scale);
                vid_window_height = (int)lroundf(base_height * scale);

                // Ensure minimum size
                if (vid_window_width < ScreenWidth)
                    vid_window_width = ScreenWidth;
                if (vid_window_height < ScreenHeight)
                    vid_window_height = ScreenHeight;
            }
        }
        else
        {
            // Fallback if desktop detection fails
            vid_window_width = ScreenWidth * 4;
            vid_window_height = ScreenHeight * 4;
        }
    }

    Uint32 window_flags = SDL_WINDOW_RESIZABLE;
    if (vid_fullscreen)
        window_flags |= SDL_WINDOW_FULLSCREEN_DESKTOP;

    window = SDL_CreateWindow(
        "Doom",
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        vid_window_width,
        vid_window_height,
        window_flags);

    if (!window)
    {
        I_Error("SDL window creation failed: %s", SDL_GetError());
    }

    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, vid_integer_scale ? "nearest" : "linear");

    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!renderer)
    {
        I_Error("SDL renderer creation failed: %s", SDL_GetError());
    }

    // Set logical rendering size (handles aspect + integer scaling).
    I_UpdateRendererLogicalSize();

    texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, ScreenWidth, ScreenHeight);
    if (!texture)
    {
        I_Error("SDL texture creation failed: %s", SDL_GetError());
    }

    SDL_SetTextureScaleMode(texture, vid_integer_scale ? SDL_ScaleModeNearest : SDL_ScaleModeLinear);

    video_buffer = (uint32_t *)malloc(ScreenWidth * ScreenHeight * sizeof(uint32_t));
    if (!video_buffer)
    {
        I_Error("Failed to allocate video buffer");
    }

    SDL_ShowCursor(SDL_DISABLE);
    SDL_SetRelativeMouseMode(SDL_TRUE);
}

void I_ApplyVideoSettings(void)
{
    if (!window || !renderer)
        return;

    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, vid_integer_scale ? "nearest" : "linear");
    if (texture)
        SDL_SetTextureScaleMode(texture, vid_integer_scale ? SDL_ScaleModeNearest : SDL_ScaleModeLinear);

    if (vid_fullscreen)
        SDL_SetWindowFullscreen(window, SDL_WINDOW_FULLSCREEN_DESKTOP);
    else
        SDL_SetWindowFullscreen(window, 0);

    if (!vid_fullscreen && vid_window_width > 0 && vid_window_height > 0)
    {
        // Clamp windowed sizes so the window doesn't spill off-screen due to
        // decorations / unusable areas.
        SDL_Rect usable;
        int w = vid_window_width;
        int h = vid_window_height;

        if (SDL_GetDisplayUsableBounds(0, &usable) == 0)
        {
            if (w > usable.w) w = usable.w;
            if (h > usable.h) h = usable.h;
        }
        SDL_SetWindowSize(window, w, h);
    }

    SDL_GetWindowSize(window, &vid_window_width, &vid_window_height);
    I_UpdateRendererLogicalSize();
}

void I_UpdateNoBlit(void)
{
    /* SDL2 backend blits directly during I_FinishUpdate. */
}

void I_FinishUpdate(void)
{
    if (!screens[0])
    {
        return;
    }

    for (int y = 0; y < ScreenHeight; ++y)
    {
        for (int x = 0; x < ScreenWidth; ++x)
        {
            uint8_t index = screens[0][y * ScreenWidth + x];
            video_buffer[y * ScreenWidth + x] = palette_table[index];
        }
    }

    SDL_UpdateTexture(texture, NULL, video_buffer, ScreenWidth * (int)sizeof(uint32_t));
    SDL_RenderClear(renderer);
    // SDL_RenderSetLogicalSize automatically scales to fill window while maintaining aspect ratio
    SDL_RenderCopy(renderer, texture, NULL, NULL);
    SDL_RenderPresent(renderer);
}

void I_ReadScreen(byte *scr)
{
    memcpy(scr, screens[0], ScreenWidth * ScreenHeight);
}

void I_SetPalette(byte *palette)
{
    for (int i = 0; i < 256; ++i)
    {
        uint8_t r = gammatable[usegamma][scale_palette_value(palette[3 * i])];
        uint8_t g = gammatable[usegamma][scale_palette_value(palette[3 * i + 1])];
        uint8_t b = gammatable[usegamma][scale_palette_value(palette[3 * i + 2])];

        palette_table[i] = (0xFFu << 24) | (r << 16) | (g << 8) | b;
    }
}

void I_StartFrame(void)
{
    // SDL_PumpEvents() is not needed here because SDL_PollEvent() in I_StartTic() already pumps events.
}

void I_StartTic(void)
{
    SDL_Event event;
    while (SDL_PollEvent(&event))
    {
        switch (event.type)
        {
            case SDL_QUIT:
                I_Quit();
                break;
            case SDL_WINDOWEVENT:
                switch (event.window.event)
                {
                    case SDL_WINDOWEVENT_FOCUS_LOST:
                        // Window lost focus - game continues but user may want to pause
                        break;
                    case SDL_WINDOWEVENT_FOCUS_GAINED:
                        // Window regained focus
                        break;
                    case SDL_WINDOWEVENT_SIZE_CHANGED:
                        if (!vid_fullscreen)
                        {
                            vid_window_width = event.window.data1;
                            vid_window_height = event.window.data2;
                        }
                        I_UpdateRendererLogicalSize();
                        break;
                    // Note: SDL_WINDOWEVENT_SIZE_CHANGED not needed because
                    // SDL_RenderSetLogicalSize handles resize automatically
                }
                break;
            case SDL_KEYDOWN:
            case SDL_KEYUP:
            {
                // Handle Alt+Enter for fullscreen toggle (only on KEYDOWN)
                if (event.type == SDL_KEYDOWN &&
                    event.key.keysym.sym == SDLK_RETURN &&
                    (event.key.keysym.mod & KMOD_ALT))
                {
                    vid_fullscreen = !vid_fullscreen;
                    I_ApplyVideoSettings();
                    break;
                }

                int key = sdl_translate_key(event.key.keysym.sym);
                if (key)
                {
                    event_t doom_event;
                    doom_event.type = (event.type == SDL_KEYDOWN) ? ev_keydown : ev_keyup;
                    doom_event.data1 = key;
                    doom_event.data2 = 0;
                    doom_event.data3 = 0;
                    D_PostEvent(&doom_event);
                }
                break;
            }
            case SDL_MOUSEBUTTONDOWN:
            {
                if (menuactive)
                {
                    // Still post mouse events for menu, but don't affect game state
                    event_t doom_event;
                    doom_event.type = ev_mouse;

                    if (event.button.button == SDL_BUTTON_LEFT)
                        doom_event.data1 = 1;
                    else if (event.button.button == SDL_BUTTON_RIGHT)
                        doom_event.data1 = 2;
                    else
                        doom_event.data1 = 0;

                    doom_event.data2 = 0;
                    doom_event.data3 = 0;
                    doom_event.data4 = event.button.x;
                    doom_event.data5 = event.button.y;
                    D_PostEvent(&doom_event);
                    break;
                }
                event_t doom_event;
                doom_event.type = ev_mouse;

                // Update button state - set the bit for this button
                if (event.button.button == SDL_BUTTON_LEFT)
                    mouse_button_state |= 1;
                else if (event.button.button == SDL_BUTTON_MIDDLE)
                    mouse_button_state |= 2;
                else if (event.button.button == SDL_BUTTON_RIGHT)
                    mouse_button_state |= 4;

                doom_event.data1 = mouse_button_state;
                doom_event.data2 = 0;
                doom_event.data3 = 0;
                doom_event.data4 = event.button.x;
                doom_event.data5 = event.button.y;
                D_PostEvent(&doom_event);
                break;
            }
            case SDL_MOUSEBUTTONUP:
            {
                if (menuactive)
                {
                    // Still post mouse events for menu
                    event_t doom_event;
                    doom_event.type = ev_mouse;
                    doom_event.data1 = 0;  // No button held
                    doom_event.data2 = 0;
                    doom_event.data3 = 0;
                    doom_event.data4 = event.button.x;
                    doom_event.data5 = event.button.y;
                    D_PostEvent(&doom_event);
                    break;
                }
                event_t doom_event;
                doom_event.type = ev_mouse;

                // Update button state - clear the bit for this button
                if (event.button.button == SDL_BUTTON_LEFT)
                    mouse_button_state &= ~1;
                else if (event.button.button == SDL_BUTTON_MIDDLE)
                    mouse_button_state &= ~2;
                else if (event.button.button == SDL_BUTTON_RIGHT)
                    mouse_button_state &= ~4;

                doom_event.data1 = mouse_button_state;
                doom_event.data2 = 0;
                doom_event.data3 = 0;
                doom_event.data4 = event.button.x;
                doom_event.data5 = event.button.y;
                D_PostEvent(&doom_event);
                break;
            }
            case SDL_MOUSEMOTION:
            {
                event_t doom_event;
                doom_event.type = ev_mouse;
                doom_event.data1 = menuactive ? 0 : mouse_button_state;  // No buttons in menu mode
                // Scale mouse movement like X11 backend (shift left by 2)
                doom_event.data2 = event.motion.xrel << 2;
                doom_event.data3 = -event.motion.yrel << 2;  // Invert Y axis
                doom_event.data4 = event.motion.x;
                doom_event.data5 = event.motion.y;
                D_PostEvent(&doom_event);
                break;
            }
            default:
                break;
        }
    }
}
