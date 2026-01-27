# Modernization Completion Checklist

**Last Updated:** January 27, 2026
**Current Branch:** master
**Status Document:** See STATUS.md for detailed progress tracking

The following tasks must be completed before the SDL2-focused adaptation is considered "done." They build on the initial video, CMake, and networking work already started.

**Note:** Items marked (✅ DONE) are completed. Items marked (⚠️ PARTIAL) are in progress. Unmarked items are pending.

## Build and platform hygiene
- ✅ **DONE:** Collapse duplicate build targets in the root `CMakeLists.txt` and rely on one `add_executable` definition. Ensure options like `NETWORK_BACKEND` and `DOOM_USE_SDL2` are consistent and documented.
- ✅ **DONE:** Keep `linuxdoom-1.10/CMakeLists.txt` centered on one `DOOM_SOURCES` list and the selected backends: SDL2 video, SDL_net when present, or legacy BSD sockets when explicitly chosen. Continue exercising both `NETWORK_BACKEND` variants and document the backend-dependent compile flags in `BUILDING.md`.
- ✅ **DONE:** Remove unconditional X11 and legacy SDL1 dependencies from the build graph. Build with SDL2 video and `-DNETWORK_BACKEND=SDL_NET` succeeds without X11 headers.
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
- [ ] Multiplayer mode: allow any player to host a server and others to join (UI flow + matchmaking/host discovery + connection UX).

## Packaging, docs, and polish
- ✅ **DONE:** Update `BUILDING.md` with SDL2-only setup steps, dependency lists, and configuration examples (audio config, network backend, build options).
- ✅ **DONE:** Update `README.TXT` with SDL2-only capabilities and setup instructions (added comprehensive SDL2 MODERNIZATION section documenting video, audio, networking, and removed backends).
- ✅ **DONE:** Updated `STATUS.md` documenting modernization progress and completion status.
- Provide a concise changelog entry summarizing removed backends (X11/SDL1, `sndserver`, BSD sockets) and new expectations for users.
