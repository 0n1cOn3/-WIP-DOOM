# DOOM Modernization Project - Current Status

**Last Updated:** January 28, 2026
**Branch:** enhanced

## Executive Summary

This repository contains the legacy Linux DOOM 1.10 sources fully modernized to use SDL2 for all platform interfaces (video, audio, input, networking), **plus ongoing gameplay/UI/network enhancements**. Music playback is supported via ADLMIDI/OPNMIDI with ALSA sequencer fallback. SDL2 is the only multimedia backend. The remaining work focuses on testing, rendering polish, and content gate cleanup.

### Positioning (Not “Just a Port”)
This repo now includes behavior-changing improvements (not only platform plumbing). To keep the original port available as a clean reference, maintain a separate `vanilla` branch alongside `enhanced` (see `UPSTREAM.md`).

## What Has Been Completed ✅

### Build System
- ✅ **CMake Build System**: Root `CMakeLists.txt` established with subdirectory structure
- ✅ **Single Target Build**: CMake builds one `linuxdoom` target (no duplicates)
- ✅ **SDL2_net Only**: No `NETWORK_BACKEND` option; SDL2_net is the only networking backend
- ✅ **SDL_net Stub System**: Auto-injects SDL_net stub headers when system library is absent
- ✅ **Build Artifacts Organization**: Outputs to `build/bin`

### Video Backend
- ✅ **SDL2 Video Backend**: `i_video_sdl2.c` is the only video backend
- ✅ **Legacy Backends Removed**: X11 (`i_video_x11.c`) and SDL1 (`i_video_sdl.c`) deleted
- ✅ **Aspect Modes**: 4:3 default, `-widescreen`, `-stretch`, and `-aspect 4:3|16:9|stretch`
- ✅ **Fullscreen Toggle**: Alt+Enter implemented via SDL2 events

### Audio & Music
- ✅ **SDL2 Audio Backend**: `i_sound.c` uses SDL2 exclusively
- ✅ **Removed sndserver/OSS/ALSA**: No external audio helpers
- ✅ **Music Playback Restored**: ADLMIDI (OPL3) + OPNMIDI (OPN2) with ALSA sequencer fallback
- ✅ **Menu + CLI Selection**: Sound menu and `-music_backend`/`-midi` flags

### Networking
- ✅ **SDL2_net Backend**: Single networking implementation using SDL2_net
- ✅ **Stub Fallback**: Internal compatibility stub (IPv4/IPv6 via modern APIs)
- ✅ **Network Harness**: `net_harness` utility available
- ✅ **Authenticated Packets**: BLAKE2s MAC keyed by per-session 128-bit key
- ✅ **Multiplayer Menu**: Host/Join flows, lobby roster, LAN discovery, direct connect

### Documentation
- ✅ **BUILDING.md**: SDL2-only setup and build instructions
- ✅ **README.md / README.TXT**: Updated feature and run info
- ✅ **AGENTS.md / TODO.md / STATUS.md**: Project guidance and roadmap

## What Is In Progress / Remaining ⚠️

### Testing / Determinism
- 🔲 Verify save/load determinism after SDL2 changes
- 🔲 Verify demo playback correctness
- 🔲 Smoke-test networking across different WAD sets

### Rendering & Resolution
- 🔲 Audit aspect handling in `r_main.c`, `r_draw.c`, HUD rendering for resolution independence
- 🔲 Review sprite/patch scaling for modern resolutions (logical size is still 320x200)
- 🔲 Consider a menu toggle for aspect modes (CLI already supported)

### Content Gates (Deferred)
- 🔲 Normalize WAD lump fallbacks in `w_wad.c` / `p_setup.c`
- 🔲 Update UI strings in `dstrings.c`/`dstrings.h` (existing logic acceptable)

## Key Files & Directories

### Build Configuration
- `/CMakeLists.txt` - Root build file (delegates to subdirectory)
- `/linuxdoom-1.10/CMakeLists.txt` - Main build logic (single target, SDL2-only)
- `/BUILDING.md` - Build instructions

### Video Backends
- `/linuxdoom-1.10/i_video_sdl2.c` - **Active SDL2 backend** ✅
- `/linuxdoom-1.10/i_video.h` - Video interface header
- `/linuxdoom-1.10/v_video.c` - Video system implementation

### Networking
- `/linuxdoom-1.10/i_net.c` - Network implementation (SDL2_net + stub fallback)
- `/linuxdoom-1.10/i_net.h` - Network interface
- `/linuxdoom-1.10/d_net.c` - DOOM network layer
- `/linuxdoom-1.10/net_harness.c` - Network testing utility
- `/linuxdoom-1.10/sdl_net_stub/` - Stub headers for SDL_net
