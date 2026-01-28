# DOOM Modernization - Debugging Guide

This document captures practical debugging workflows for the SDL2-only codebase. It is intentionally focused on current behavior and repeatable checks.

## Quick sanity checks

- Build (no tests exist):
  ```bash
  cmake -S . -B build
  cmake --build build
  ```

- Network harness:
  ```bash
  ./build/bin/net_harness
  ```

- Minimal game run:
  ```bash
  export DOOMWADDIR=/path/to/iwad
  ./build/bin/linuxdoom
  ```

## AddressSanitizer (ASAN)

```bash
cmake -S . -B build-asan \
  -DCMAKE_C_FLAGS="-fsanitize=address -g" \
  -DCMAKE_CXX_FLAGS="-fsanitize=address -g"
cmake --build build-asan

ASAN_OPTIONS=detect_leaks=0 ./build-asan/bin/linuxdoom
```

`detect_leaks=0` avoids noise from system libraries (GTK/fontconfig/Wayland). You can remove it to investigate leaks.

## Common areas to audit

- **Save/load determinism**: `p_saveg.c`, `p_setup.c`, `p_tick.c`
- **Demo playback**: `g_game.c` demo stream parsing and timing
- **Aspect handling**: `i_video_sdl2.c`, `r_main.c`, `r_draw.c`, HUD (`st_*`, `hu_*`)
- **Networking**: `i_net.c`, `d_net.c` (payload packing, MAC validation, lobby flows)

## Manual regression checklist

- Load/save a game, quit, reload the save.
- Play a short demo and verify playback sync.
- Verify fullscreen toggle (Alt+Enter), window resize, and aspect selection.
- Host/join a LAN game using the Multiplayer menu and direct connect.

## Troubleshooting hints

- **Audio glitches**: Verify SDL audio device availability and inspect `i_sound.c` mixing.
- **Missing WADs**: Confirm `DOOMWADDIR` or use `run-linuxdoom.sh`.
- **Content mismatch in multiplayer**: Check IWAD/PWAD sets match between host and clients.
