# Repository guidance

This codebase contains the legacy Linux Doom sources with a partially modernized build. Use this file for any future edits in the repo.

## Coding and build expectations
- The codebase is now SDL2-only: video, audio, input, and networking all use SDL2 (or SDL2_net for networking).
- The top-level CMake build is the source of truth. Configure with `cmake -S . -B build` and build with `cmake --build build`.
  - Network backend defaults to SDL2_net with automatic stub fallback if system library is unavailable.
  - No build options needed to select backends; SDL2 is the only video/audio implementation.
- If you change code (not just docs), run the build after configuration. There are no automated tests.

## Current status of the port to modern APIs
- ✅ **SDL2 Video**: `linuxdoom-1.10/i_video_sdl2.c` is the only video backend. Legacy X11/SDL1 code removed.
- ✅ **SDL2 Audio**: `linuxdoom-1.10/i_sound.c` uses SDL2 audio exclusively. Legacy sndserver, OSS/ALSA removed.
- ✅ **SDL2_net Networking**: `linuxdoom-1.10/i_net.c` uses SDL2_net exclusively. BSD sockets code removed. Auto-injects SDL_net stub when system library unavailable.
- ✅ **Build System**: CMake builds a single `linuxdoom` target. Automatic 4:3 aspect ratio preservation via SDL_RenderSetLogicalSize().

## Suggested next steps (Core SDL2 Modernization Complete)
The foundational SDL2 modernization is complete. Future work should focus on:
- **Testing**: Verify multiplayer networking, save/load/demo playback, and gameplay across different WADs
- **Polish**: Add optional aspect ratio mode selection (4:3, widescreen, stretched) via command-line flags
- **Content Consolidation**: Streamline gamemode/gameversion conditionals in `g_game.c`, `p_setup.c`, `info.c`
- **Advanced Graphics**: Consider optional widescreen support, HUD scaling for modern resolutions (future enhancement)

## Outstanding work items
See `TODO.md` for the complete modernization checklist. Below are the remaining engineering themes:

### Completed ✅
- ✅ SDL2 video backend (X11/SDL1 removed)
- ✅ SDL2 audio backend (sndserver removed, 16-bit stereo @ 11025 Hz)
- ✅ SDL2_net networking (BSD sockets removed, IPv6 support via modern APIs)
- ✅ CMake simplified (single target, no NETWORK_BACKEND option)
- ✅ 4:3 aspect ratio preservation via SDL_RenderSetLogicalSize()
- ✅ Documentation updated (BUILDING.md, README.TXT, STATUS.md, TODO.md)

### Remaining (Medium Priority)
- Streamline game-version conditionals and content gates: replace `gamemode`/`gameversion` gating in `g_game.c`, `p_setup.c`, and `info.c` with explicit capability flags or sane defaults.
- Normalize WAD lump fallbacks in `w_wad.c`/`p_setup.c`; update version-locked UI strings in `dstrings.c`/`dstrings.h`.
- Testing: Verify save/load and demo playback work correctly with SDL2 backends.

### Future Enhancements (Lower Priority)
- Add optional aspect ratio modes (4:3, widescreen, stretched) via command-line flags
- Evaluate resolution independence for sprites, patches, HUD (currently fixed at 320x200 logical resolution)
- Consider advanced graphics features (widescreen rendering, HUD scaling for modern displays)
