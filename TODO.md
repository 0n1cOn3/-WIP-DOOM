# Modernization Completion Checklist

**Last Updated:** January 28, 2026
**Current Branch:** master
**Status Document:** See STATUS.md for detailed progress tracking

The following tasks must be completed before the SDL2-focused adaptation is considered "done." They build on the initial video, CMake, and networking work already started.

**Note:** Items marked (✅ DONE) are completed. Items marked (⚠️ PARTIAL) are in progress. Unmarked items are pending.

## Build and platform hygiene
- ✅ **DONE:** Collapse duplicate build targets in the root `CMakeLists.txt` and rely on a single `linuxdoom` target.
- ✅ **DONE:** Keep `linuxdoom-1.10/CMakeLists.txt` centered on one `DOOM_SOURCES` list and the SDL2-only platform backends (SDL2 video/audio/input, SDL2_net networking with stub fallback).
- ✅ **DONE:** Remove unconditional X11 and legacy SDL1 dependencies from the build graph (SDL2 is the only video/input path).
- Add sensible feature toggles for optional components (e.g., MIDI, IPv6) and document them in `BUILDING.md`.

## Video and input
- ✅ **DONE:** Finalize the SDL2-only video path: retire `i_video_x11.c`/`i_video_sdl.c`, keep `i_video_sdl2.c`, and ensure no X11/SDL1 conditionals remain. (Legacy backends removed from repository; SDL2 is now the only video implementation)
- ✅ **DONE:** Handle SDL2 window events (resize/fullscreen/focus) by using logical sizing for aspect ratio preservation instead of stretching to the current window size. (SDL_RenderSetLogicalSize implements automatic letterboxing for 4:3 aspect ratio)
- ✅ **DONE:** Ensure window resizing, fullscreen toggles (Alt+Enter), and input focus changes are handled uniformly via SDL2 events.

## Networking
- ✅ **DONE:** Chose SDL_net as the single maintained backend and removed all BSD sockets code path from i_net.c and d_net.c (~350 lines removed).
- ✅ **DONE:** Removed `NETWORK_BACKEND` CMake option; SDL_net is now the only networking backend with automatic stub fallback.
- ✅ **DONE:** Aligned `d_net.c` with SDL_net API (simplified `ReadNetUint32()` to use `SDLNet_Read32()` exclusively, removed BSD byte-order functions).
- ✅ **DONE:** Retained `sdl_net_stub/` for systems without SDL2_net package (superior to legacy BSD code with modern `getaddrinfo()` and full IPv6 support).
- ✅ **DONE:** Network latency/packet-loss handling hooks integrated for testing and field deployment.
- ✅ **DONE:** Updated CMakeLists.txt to remove conditional network backend selection; build now always uses SDL2_net with stub fallback.
- ✅ **DONE:** **Secure transport:** All packets now carry a BLAKE2s MAC keyed by a per-session 128-bit key; lobby/session key exchange implemented for `-host`/`-connect` and the in-game lobby.
- ✅ **DONE:** **Lobby UI & discovery:** In-game Multiplayer menu (Host/Join LAN/Join by address) with LAN discovery list, direct-connect entry, and user-friendly status/errors (with optional rich details).
- ✅ **DONE:** **Content integrity:** Exchange IWAD/PWAD hash set before START; host “vanilla only” toggle available (gates content and joins).
- 🔲 **WAN traversal:** Optional introducer-based UDP hole punching (no UPnP) with clear failure messaging; keep direct connect as fallback.
- 🔲 **Rate limits & logging:** Expose `-netlog`, throttle join/command bursts, and surface kick/mismatch reasons in UI.

## Audio
- ✅ **DONE:** Unify audio on SDL2 and drop the external `sndserver`: refactor `i_sound.c`/`i_sound.h` for SDL2 audio exclusively and simplify command-line flags that referenced the helper process.
- ✅ **DONE:** Clean up mixing in `s_sound.c` and `sounds.c`: removed legacy 8-bit assumptions, documented 16-bit/stereo format (11025 Hz), made sample rate easily configurable via #define.
- ✅ **DONE:** Confirm the build only links SDL2 audio libraries (no OSS/ALSA-specific flags) and that startup failures surface clear diagnostics.
- ✅ **DONE:** Remove legacy DMX audio device selection code (snd_MusicDevice, etc.)

## Game data and content gates
- ✅ **DONE:** Streamlined `gamemode`/`gameversion` conditionals: Created centralized game version capability system (g_version.c/h) with data-driven metadata tables and capability query functions to replace scattered version checks.
- ✅ **DONE:** Consolidated duplicate sky texture selection logic in `g_game.c` (was duplicated in G_DoLoadLevel and G_InitNew, now uses single G_GetSkyTexture helper).
- 🔲 Normalize WAD lump fallbacks in `w_wad.c` and `p_setup.c` to degrade gracefully; refresh UI strings in `dstrings.c`/`dstrings.h` to remove version-locked messaging (deferred - existing fallback logic already acceptable).

## Rendering, HUD, and resolutions
- Fix aspect handling in `r_main.c`, `r_draw.c`, and HUD rendering (`st_*`, `hu_*`) so 4:3 and widescreen modes display without stretching.
- Re-evaluate BLOCKMAP/REJECT reliance in `p_map.c`, `p_sight.c`, and `p_inter.c`; prototype BSP-friendly alternatives and document any required map-format expectations.
- Audit sprite/patch scaling, automap overlays, and intermission screens for SDL2 resolution independence.

## Advanced graphics, mods, and content (Future)
- [ ] Add Vulkan renderer with OpenGL fallback.
- [ ] Enhance settings menu with shader selection under Graphics:
  - Classic DOOM shaders.
  - Modern DOOM shaders + HD graphics (assets in `/home/hx/Downloads/GZDoom_HD_Texture_pack.6.rar` and `/home/hx/Downloads/HDHQDoom.rar`).
- [ ] Add HQ music from `HDHQDoom.rar`.
- [ ] Add toggle for enabling/disabling blood (from `HDHQDoom.rar`).
- [ ] Allow GZDoom mods and shaders to be used; provide an API for it.
- [ ] Implement 2.5D raymarching inside a GLES 3.0 shader.
- [ ] Preload all sprites into shader VRAM; pass 2D sprite positions + camera to geometry shader.

## Save/load, demos, and deterministic behavior
- Verify savegame and demo playback determinism after SDL2, networking, and audio changes; update any timing assumptions in `p_tick.c` and input aggregation code.
- Add minimal regression checks (even manual scripts) to cover save/load and demo playback across common WADs.

## Multiplayer
- ✅ **DONE:** Multiplayer Host/Join flows implemented (in-game menu + lobby + LAN discovery + direct connect).

## Packaging, docs, and polish
- ✅ **DONE:** Update `BUILDING.md` with SDL2-only setup steps, dependency lists, and configuration examples (audio config, network backend, build options).
- ✅ **DONE:** Update `README.TXT` with SDL2-only capabilities and setup instructions (added comprehensive SDL2 MODERNIZATION section documenting video, audio, networking, and removed backends).
- ✅ **DONE:** Updated `STATUS.md` documenting modernization progress and completion status.
- Provide a concise changelog entry summarizing removed backends (X11/SDL1, `sndserver`, BSD sockets) and new expectations for users.
