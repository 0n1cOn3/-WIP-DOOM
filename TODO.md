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
- ⚠️ **PARTIAL:** Legacy BSD networking now builds without SDL_net; choose a single maintained backend—SDL_net (default via `i_net.c`) or BSD sockets (legacy path guarded by `DOOM_USE_LEGACY_NETWORKING`)—and remove the unused code path from `CMakeLists.txt`. (Both backends exist; consolidation pending)
- ✅ **DONE:** Make `net_harness` honor the selected `NETWORK_BACKEND` instead of always linking the SDL2_net stub; ensure SDL_net is optional when BSD sockets are selected.
- Align `d_net.c`/`d_net.h` and `net_harness.c` with the chosen API (packet structs, init/teardown), and delete `sdl_net_stub/` if SDL_net becomes mandatory. Continue streamlining the shared packet handling once the preferred backend is locked in.
- ✅ **DONE:** Add basic latency/packet-loss handling hooks so multiplayer remains stable on modern networks. Provide tunables for field testing and integrate them with the networking init path.

## Audio
- ✅ **DONE:** Unify audio on SDL2 and drop the external `sndserver`: refactor `i_sound.c`/`i_sound.h` for SDL2 audio exclusively and simplify command-line flags that referenced the helper process.
- ✅ **DONE:** Clean up mixing in `s_sound.c` and `sounds.c`: removed legacy 8-bit assumptions, documented 16-bit/stereo format (11025 Hz), made sample rate easily configurable via #define.
- ✅ **DONE:** Confirm the build only links SDL2 audio libraries (no OSS/ALSA-specific flags) and that startup failures surface clear diagnostics.
- ✅ **DONE:** Remove legacy DMX audio device selection code (snd_MusicDevice, etc.)

## Game data and content gates
- Streamline `gamemode`/`gameversion` conditionals in `g_game.c`, `p_setup.c`, and `info.c` so capabilities are explicit and defaults are sensible regardless of WAD flavor.
- Normalize WAD lump fallbacks in `w_wad.c` and `p_setup.c` to degrade gracefully; refresh UI strings in `dstrings.c`/`dstrings.h` to remove version-locked messaging.

## Rendering, HUD, and resolutions
- Fix aspect handling in `r_main.c`, `r_draw.c`, and HUD rendering (`st_*`, `hu_*`) so 4:3 and widescreen modes display without stretching.
- Re-evaluate BLOCKMAP/REJECT reliance in `p_map.c`, `p_sight.c`, and `p_inter.c`; prototype BSP-friendly alternatives and document any required map-format expectations.
- Audit sprite/patch scaling, automap overlays, and intermission screens for SDL2 resolution independence.

## Save/load, demos, and deterministic behavior
- Verify savegame and demo playback determinism after SDL2, networking, and audio changes; update any timing assumptions in `p_tick.c` and input aggregation code.
- Add minimal regression checks (even manual scripts) to cover save/load and demo playback across common WADs.

## Packaging, docs, and polish
- ✅ **DONE:** Update `BUILDING.md` with SDL2-only setup steps, dependency lists, and configuration examples (audio config, network backend, build options).
- ⚠️ **PARTIAL:** Update `README.TXT` with SDL2-only capabilities and setup instructions (documentation partial).
- Provide a concise changelog entry summarizing removed backends (X11/SDL1, `sndserver`) and new expectations for users.
