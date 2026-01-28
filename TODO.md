# Modernization Completion Checklist

**Last Updated:** January 28, 2026
**Current Branch:** enhanced
**Status Document:** See STATUS.md for detailed progress tracking

The following tasks must be completed before the SDL2-focused adaptation is considered "done." They build on the initial video, CMake, and networking work already completed.

**Note:** Items marked (✅ DONE) are completed. Items marked (⚠️ PARTIAL) are in progress. Unmarked items are pending.

## Build and platform hygiene
- ✅ **DONE:** Collapse duplicate build targets in the root `CMakeLists.txt` and rely on a single `linuxdoom` target.
- ✅ **DONE:** Keep `linuxdoom-1.10/CMakeLists.txt` centered on one `DOOM_SOURCES` list and the SDL2-only platform backends (SDL2 video/audio/input, SDL2_net networking with stub fallback).
- ✅ **DONE:** Remove unconditional X11 and legacy SDL1 dependencies from the build graph (SDL2 is the only video/input path).
- 🔲 Document worktree-friendly builds (separate build directories per worktree).

## Build optimizations (Future)
- [ ] Code optimization for x86/x64 instruction sets
- [ ] Investigate CPU/GPU split for rendering work
- [ ] Clarify static linking expectations given licensing constraints

## Video and input
- ✅ **DONE:** Finalize the SDL2-only video path: retire `i_video_x11.c`/`i_video_sdl.c`, keep `i_video_sdl2.c`, and ensure no X11/SDL1 conditionals remain.
- ✅ **DONE:** Handle SDL2 window events (resize/fullscreen/focus) with logical sizing for aspect ratio preservation.
- ✅ **DONE:** Implement aspect modes via CLI: `-aspect`, `-widescreen`, `-stretch`.
- 🔲 Add an in-menu toggle for aspect ratio modes.
- 🔲 Audit HUD/sprite scaling for resolution independence.

## Networking
- ✅ **DONE:** Chose SDL_net as the single maintained backend and removed all BSD sockets code path from i_net.c and d_net.c.
- ✅ **DONE:** Removed `NETWORK_BACKEND` CMake option; SDL_net is now the only networking backend with automatic stub fallback.
- ✅ **DONE:** Aligned `d_net.c` with SDL_net API (simplified `ReadNetUint32()` to use `SDLNet_Read32()` exclusively, removed BSD byte-order functions).
- ✅ **DONE:** Retained `sdl_net_stub/` for systems without SDL2_net package (modern `getaddrinfo()` and IPv4/IPv6 support).
- ✅ **DONE:** Network latency/packet-loss handling hooks integrated for testing and field deployment.
- ✅ **DONE:** **Secure transport:** All packets carry a BLAKE2s MAC keyed by a per-session 128-bit key; lobby/session key exchange implemented.
- ✅ **DONE:** **Lobby UI & discovery:** Multiplayer menu (Host/Join LAN/Join by address) with LAN discovery list and direct connect.
- ✅ **DONE:** **Content integrity:** Exchange IWAD/PWAD hash set before START; host “vanilla only” toggle available.
- 🔲 **Rate limits & logging:** Expose `-netlog`, throttle join/command bursts, and surface kick/mismatch reasons in UI.

## Audio
- ✅ **DONE:** Unify audio on SDL2 and drop the external `sndserver`.
- ✅ **DONE:** Clean up mixing in `s_sound.c` and `sounds.c`: 16-bit stereo @ 11025 Hz.
- ✅ **DONE:** Remove legacy DMX audio device selection code.
- ✅ **DONE:** Restore music playback via ADLMIDI/OPNMIDI with ALSA sequencer fallback.

## Game data and content gates
- ✅ **DONE:** Centralized game version capability system (g_version.c/h) with capability query helpers.
- ✅ **DONE:** Consolidated duplicate sky texture selection logic in `g_game.c`.
- 🔲 Normalize WAD lump fallbacks in `w_wad.c` and `p_setup.c` (deferred; existing fallback logic acceptable).
- 🔲 Update UI strings in `dstrings.c`/`dstrings.h` (deferred).

## Rendering, HUD, and resolutions
- Fix aspect handling in `r_main.c`, `r_draw.c`, and HUD rendering (`st_*`, `hu_*`) so 4:3 and widescreen modes display without stretching.
- Audit sprite/patch scaling, automap overlays, and intermission screens for SDL2 resolution independence.

## Advanced graphics, mods, and content (Future)
- [ ] Add Vulkan renderer with OpenGL fallback.
- [ ] Enhance settings menu with shader selection under Graphics.
- [ ] Add HQ music from `HDHQDoom.rar`.
- [ ] Add toggle for enabling/disabling blood (from `HDHQDoom.rar`).
- [ ] Allow GZDoom mods and shaders to be used; provide an API for it.
- [ ] Implement 2.5D raymarching inside a GLES 3.0 shader.
- [ ] Preload all sprites into shader VRAM; pass 2D sprite positions + camera to geometry shader.

## Save/load, demos, and deterministic behavior
- Verify savegame and demo playback determinism after SDL2, networking, and audio changes; update any timing assumptions in `p_tick.c` and input aggregation code.
- Add minimal regression checks (even manual scripts) to cover save/load and demo playback across common WADs.

## Packaging, docs, and polish
- ✅ **DONE:** Update `BUILDING.md` with SDL2-only setup steps, dependency lists, and configuration examples.
- ✅ **DONE:** Update `README.TXT` with SDL2-only capabilities and setup instructions.
- ✅ **DONE:** Updated `STATUS.md` documenting modernization progress and completion status.
- 🔲 Provide a concise changelog entry summarizing removed backends (X11/SDL1, `sndserver`, BSD sockets) and new expectations for users.
