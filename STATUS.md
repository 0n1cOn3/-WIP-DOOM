# DOOM Modernization Project - Current Status

**Last Updated:** January 27, 2026
**Repository:** 0n1cOn3/-WIP-DOOM
**Branch:** master

## Executive Summary

This repository contains the legacy Linux Doom sources undergoing modernization to use modern SDL2 APIs instead of legacy X11 and SDL1 backends. The project is **actively in progress** with foundational work completed and significant modernization tasks remaining.

## What Has Been Completed ✅

### Build System
- ✅ **CMake Build System**: Root `CMakeLists.txt` established with subdirectory structure
- ✅ **Single Target Build**: CMake now builds one `linuxdoom` target (no duplicate executables)
- ✅ **Network Backend Selection**: `NETWORK_BACKEND` option supports `SDL_NET` (default) or `BSD`
- ✅ **SDL_net Stub System**: Auto-injects SDL_net stub headers when system library is absent
- ✅ **Build Artifacts Organization**: Outputs to `build/bin` directory

### Video Backend
- ✅ **SDL2 Video Backend Created**: `i_video_sdl2.c` exists and is functional
- ✅ **Backend Selection Logic**: CMake selects SDL2 backend by default
- ✅ **Legacy Backends Removed**: X11 (`i_video_x11.c`) and SDL1 (`i_video_sdl.c`) deleted from repository

### Networking
- ✅ **SDL_net Integration**: Default networking through `i_net.c` with SDL_net
- ✅ **BSD Sockets Fallback**: Legacy BSD networking available via `DOOM_USE_LEGACY_NETWORKING` flag
- ✅ **Network Harness**: `net_harness` utility honors selected `NETWORK_BACKEND`
- ✅ **Latency Handling**: Basic latency/packet-loss handling hooks added

### Documentation
- ✅ **BUILDING.md**: Clear build instructions with SDL2 and networking backend examples
- ✅ **AGENTS.md**: Repository guidance for AI agents and future editors
- ✅ **TODO.md**: Comprehensive modernization checklist with 40+ tasks
- ✅ **README.TXT**: Original documentation preserved

## What Is In Progress / Partially Complete ⚠️

### Video System
- ✅ **Legacy Code Removal**: X11 and SDL1 video code removed from repository
- ✅ **Conditional Compilation**: No X11/SDL1 conditionals remain in codebase
- ⚠️ **Window Event Handling**: SDL2 resize/fullscreen/focus events need proper handling
- ⚠️ **Aspect Ratio**: Resolution-independent rendering and aspect ratio preservation incomplete

### Build Configuration
- ✅ **X11 Dependencies**: Removed from build; only SDL2 is linked
- ⚠️ **Feature Toggles**: Optional components (MIDI, IPv6) lack clear toggles
- ⚠️ **Documentation**: Backend expectations for packagers not fully documented

### Networking
- ⚠️ **Backend Consolidation**: Both SDL_net and BSD paths exist; need to choose one primary
- ⚠️ **API Alignment**: `d_net.c`/`d_net.h` needs alignment with chosen backend
- ⚠️ **Stub Directory**: `sdl_net_stub/` may be removed if SDL_net becomes mandatory

## What Remains To Be Done 🔲

### High Priority

#### 1. Finalize SDL2-Only Video Path
- ✅ Remove `i_video_x11.c` and `i_video_sdl.c` from repository
- ✅ Purge X11/SDL1 conditionals from `v_video.c`, `i_video.h`
- ✅ No legacy conditionals found in HUD modules (`st_*.c`, `hu_*.c`)
- ✅ No legacy conditionals in `g_game.c` input handling
- ✅ Verify CMake no longer requires X11 headers

#### 2. Audio Modernization
- 🔲 Refactor `i_sound.c`/`i_sound.h` for SDL2 audio exclusively
- 🔲 Remove external `sndserver` dependency
- 🔲 Clean mixing in `s_sound.c` and `sounds.c` for 16/32-bit output
- 🔲 Remove OSS/ALSA/X11-specific audio flags
- 🔲 Add clear audio diagnostics for startup failures

#### 3. Build System Cleanup
- 🔲 Remove duplicate/legacy build branches from CMakeLists.txt
- 🔲 Document `NETWORK_BACKEND` and other options in BUILDING.md
- 🔲 Add feature toggles for optional components
- 🔲 Ensure SDL2-only builds succeed without X11

#### 4. Rendering & Resolution Support
- 🔲 Fix aspect handling in `r_main.c`, `r_draw.c`
- 🔲 Support 4:3 and widescreen without stretching
- 🔲 Handle SDL2 window resize/fullscreen events
- 🔲 Audit sprite/patch scaling for resolution independence
- 🔲 Update HUD rendering for modern displays

### Medium Priority

#### 5. Networking Backend Consolidation
- 🔲 Choose primary backend (SDL_net recommended)
- 🔲 Remove unused networking code path
- 🔲 Align `d_net.c`/`d_net.h` with chosen API
- 🔲 Consider removing `sdl_net_stub/` if SDL_net is mandatory

#### 6. Game Data & Content Gates
- 🔲 Streamline `gamemode`/`gameversion` conditionals
- 🔲 Normalize WAD lump fallbacks in `w_wad.c`, `p_setup.c`
- 🔲 Update UI strings in `dstrings.c`/`dstrings.h`
- 🔲 Make capability flags explicit vs. version-based

#### 7. Save/Load & Determinism
- 🔲 Verify savegame determinism after SDL2 changes
- 🔲 Verify demo playback works correctly
- 🔲 Update timing assumptions in `p_tick.c`
- 🔲 Add regression checks for save/load and demos

### Lower Priority

#### 8. Map Data Optimization
- 🔲 Re-evaluate BLOCKMAP/REJECT in `p_map.c`, `p_sight.c`, `p_inter.c`
- 🔲 Prototype BSP-friendly alternatives
- 🔲 Document map format expectations

#### 9. Polish & Packaging
- 🔲 Update README.TXT with SDL2-only setup
- 🔲 Create changelog for removed backends
- 🔲 Document migration path for users
- 🔲 Final packaging and release preparation

## Key Files & Directories

### Build Configuration
- `/CMakeLists.txt` - Root build file (delegates to subdirectory)
- `/linuxdoom-1.10/CMakeLists.txt` - Main build logic with backend selection
- `/BUILDING.md` - User-facing build instructions

### Video Backends
- `/linuxdoom-1.10/i_video_sdl2.c` - **Active SDL2 backend** ✅
- `/linuxdoom-1.10/i_video_sdl.c` - Legacy SDL1 backend (to be removed)
- `/linuxdoom-1.10/i_video_x11.c` - Legacy X11 backend (to be removed)
- `/linuxdoom-1.10/i_video.h` - Video interface header
- `/linuxdoom-1.10/v_video.c` - Video system implementation

### Networking
- `/linuxdoom-1.10/i_net.c` - Network implementation (SDL_net + BSD sockets)
- `/linuxdoom-1.10/i_net.h` - Network interface
- `/linuxdoom-1.10/d_net.c` - DOOM network layer
- `/linuxdoom-1.10/net_harness.c` - Network testing utility
- `/linuxdoom-1.10/sdl_net_stub/` - Stub headers for SDL_net

### Audio
- `/linuxdoom-1.10/i_sound.c` - Sound implementation (needs SDL2 migration)
- `/linuxdoom-1.10/s_sound.c` - Sound system
- `/sndserv/` - External sound server (to be removed)

### Documentation
- `/TODO.md` - Complete modernization checklist
- `/AGENTS.md` - Repository guidance and current status
- `/README.TXT` - Original Doom documentation
- `/STATUS.md` - **This file**

## Build Instructions

### Current Working Build

```bash
# Note: Requires SDL2 development libraries
# On Debian/Ubuntu: sudo apt-get install libsdl2-dev libsdl2-net-dev

# Configure with SDL_net (default)
cmake -S . -B build -DNETWORK_BACKEND=SDL_NET

# Build
cmake --build build

# Output: build/bin/linuxdoom
```

### Alternative: BSD Networking

```bash
cmake -S . -B build-bsd -DNETWORK_BACKEND=BSD
cmake --build build-bsd
```

## Current Challenges

1. **X11 Dependencies**: The build configuration may still have lingering X11 dependencies that need to be removed

2. **Multiple Video Backends**: Three video backend implementations exist, but only SDL2 should remain

3. **Audio System**: Still uses legacy paths; needs complete SDL2 migration

4. **Resolution Handling**: Aspect ratio and resolution independence not fully implemented

5. **Testing**: No automated test infrastructure exists; manual testing required

## Recommended Next Steps

### Completed ✅
1. **Remove Legacy Video Backends**: Deleted `i_video_x11.c` and `i_video_sdl.c`
2. **Clean Build Dependencies**: Removed X11 from CMake requirements
3. **Update Video Code**: No X11/SDL1 conditionals in codebase

### Immediate (Next PR)
4. **Audio Migration**: Refactor `i_sound.c` for SDL2 audio only
5. **Resolution Support**: Implement proper aspect ratio handling
6. **Window Events**: Add SDL2 resize/fullscreen event handling

### Short Term (Following PRs)
7. **Networking Consolidation**: Choose and document primary backend
8. **Documentation**: Update all docs for SDL2-only approach
9. **Testing**: Verify save/load and demo playback

### Medium Term
7. **Networking Consolidation**: Choose and document primary backend
8. **Documentation**: Update all docs for SDL2-only approach
9. **Testing**: Verify save/load and demo playback

## Success Metrics

The modernization will be considered complete when:

- ✅ Single SDL2 video backend (X11/SDL1 removed)
- ✅ Single SDL2 audio backend (sndserver removed)
- ✅ One primary networking backend (documented)
- ✅ Builds without X11 dependencies
- ✅ Supports modern resolutions (4:3, 16:9, 21:9)
- ✅ Window resize/fullscreen works correctly
- ✅ Wayland compatibility confirmed
- ✅ All documentation updated
- ✅ Savegames and demos work correctly

## Resources

- **Original Linux Doom**: id Software (1993-1997)
- **SDL2 Documentation**: https://wiki.libsdl.org/
- **CMake Documentation**: https://cmake.org/documentation/
- **Repository Issues**: https://github.com/0n1cOn3/DOOM/issues
- **Pull Requests**: https://github.com/0n1cOn3/DOOM/pulls

---

## Summary

**Current State**: Foundational modernization work is complete. The codebase has a working SDL2 video backend, CMake build system, and dual networking backend support.

**Next Phase**: Focus on removing legacy code (X11, SDL1, sndserver) and completing the SDL2-only migration for video and audio.

**Timeline Estimate**: With focused effort, core modernization (items 1-6 above) could be completed in 4-6 PRs. Polish and optimization would follow.

**Risk Level**: Low - The existing SDL2 backend is functional; remaining work is primarily removal and cleanup of legacy code.
