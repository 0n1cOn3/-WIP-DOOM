# DOOM SDL2 (GPL-2.0-only) — Modernized & Enhanced

Modernized, single-target build of the original Linux DOOM 1.10 sources, plus ongoing enhancements. Video, audio, input, and networking run on SDL2 and SDL2_net; music playback ships with bundled libADLMIDI/libOPNMIDI via CMake FetchContent. The top-level CMakeLists.txt is the source of truth.

If you remember the 90s Linux port: this keeps the gameplay, but drops the period-correct plumbing. No X11 renderer fork, no external `sndserver`, no BSD sockets sprawl. It is DOOM, but built and run like a modern Linux game.

## Features
- SDL2 renderer with logical sizing for aspect preservation (4:3 default, `-widescreen`, `-stretch`, or `-aspect 4:3|16:9|stretch`) and Alt+Enter fullscreen toggle.
- SDL2 audio backend (11025 Hz, 16-bit stereo) with in-process mixer.
- Music backends: ADLMIDI (OPL3) + OPNMIDI (OPN2), fetched at configure time; ALSA sequencer fallback when available.
- Multiplayer: in-game Host/Join menu with lobby roster, LAN discovery, and direct connect (host[:port] + IPv6 `[addr]:port`).
- Secure net transport: all game packets carry a BLAKE2s MAC keyed by a per-session 128-bit key; content hashing gates mismatched WAD sets.
- Single `linuxdoom` target built by CMake; no legacy sndserver, SDL1, or X11 backends.

## What Changed From The Original Linux 1.10 Port
- Rendering: SDL2 + logical sizing for clean aspect preservation instead of fixed X11-era assumptions.
- Audio: no helper process; SDL2 audio mixer runs in-process.
- Networking: SDL2_net (with a stub fallback); lobby bootstrap, LAN discovery, and basic mismatch defenses.
- Build: one CMake build that fetches optional music deps and produces a single modern binary.
- Enhancements: gameplay/UI/network features beyond the original port (this is no longer “just a port”).

## Recommended Repo Layout (Vanilla vs Enhanced)
If you want to keep a strict “original port” tree alongside the enhanced one, use one of these workflows:
- **Two branches + worktrees (recommended):** keep an unmodified `vanilla` branch and develop on `enhanced`. Use `git worktree` so both trees are checked out at once.
- **Two directories in one repo:** keep an archived `vanilla/` source snapshot (not built), and keep the enhanced tree as the CMake build target.

See `UPSTREAM.md` for a concrete suggested workflow and naming.

## Requirements
- CMake ≥ 3.16
- C compiler (C11) and C++ compiler (C++14) for the music backends
- SDL2 development files
- SDL2_net development files (optional; a stub is built if absent)
- ALSA headers (optional; enables sequencer fallback for music)

## Build
```sh
cmake -S . -B build
cmake --build build
```
Artifacts land in `build/bin/` (`linuxdoom`, `net_harness`).

## Run
```sh
./run-linuxdoom.sh /path/to/DOOM.WAD [extra DOOM args]
# or
DOOMWADDIR=/path/to/wads ./build/bin/linuxdoom
```
Convenience env vars: `IWAD_PATH`, `PWAD_PATH` for the wrapper.

Common flags:
- `-aspect 4:3|16:9|stretch` (explicit aspect selection)
- `-widescreen` (alias for 16:9), `-stretch` (fill without preserving aspect)
- `-fullscreen` / `-windowed`
- Alt+Enter toggles fullscreen at runtime.
- Music backend selection:
  - `-music_backend adlmidi|opnmidi|alsa|off`
  - `-midi adlmidi|opnmidi|alsa|off` (alias)
  - `-nomusic` (disable music)
- Multiplayer:
  - In-game: Main Menu -> Multiplayer (Host/Join/LAN/Direct Connect)
  - CLI bootstrap:
    - Host: `-host N` (optionally `-port P`)
    - Join: `-connect host[:port]`
  - Optional: `-player <name>` to set your lobby roster name.

Saves/config: by default `~/.doomrc` and `.dsg` save files in the working directory.

## Development workflow
- Use the `enhanced` branch for active work; keep `vanilla` as a clean baseline.
- Prefer worktrees (`git worktree`) for parallel checkouts. Use separate build dirs per worktree to avoid CMake cache conflicts (e.g. `build-enhanced/`).
- After code changes, re-run the build (`cmake -S . -B build` then `cmake --build build`).
- There are no automated tests; sanity-check with `build/bin/net_harness` and a quick run of `linuxdoom`.

## Project status
Core SDL2 modernization is complete. See `STATUS.md` for current state and `TODO.md` for the roadmap. `AGENTS.md` documents repo-specific guidance.

## License
- Core source: GNU GPL-2.0-only (see `LICENSE.TXT`).
- Third-party components and their licenses are listed in `THIRD_PARTY_NOTICES.md` (SDL2/SDL2_net under zlib, libADLMIDI under LGPL/GPL/MIT, libOPNMIDI under LGPL/GPL/MIT).
- Included lightweight BLAKE2s implementation is public domain/CC0.

## Support & contributions
Issues and patches are welcome. Please keep the SDL2-only build contract intact and adhere to the licensing notes above when redistributing binaries. Plain-text debug artifacts (e.g., personal logs) should not be committed for releases.
