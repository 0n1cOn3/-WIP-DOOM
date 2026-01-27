# DOOM SDL2 (GPL-2.0-only)

Modernized, single-target build of the original Linux DOOM 1.10 sources. Video, audio, input, and networking run on SDL2 and SDL2_net; music playback ships with bundled libADLMIDI/libOPNMIDI via CMake FetchContent. The top-level CMakeLists.txt is the source of truth.

## Features
- SDL2 renderer with automatic 4:3 logical size, optional `-widescreen` or `-stretch` scaling, Alt+Enter fullscreen toggle.
- SDL2 audio backend (11025 Hz, 16-bit stereo) with in-process mixer.
- SDL2_net multiplayer with IPv6 support and an auto-injected compatibility stub when SDL2_net is missing.
- Built-in BLAKE2s MAC on game packets: host generates a 128-bit session key; all net traffic is authenticated.
- ADLMIDI (OPL3) and OPNMIDI (OPN2) music backends fetched at configure time.
- Single `linuxdoom` target built by CMake; no legacy sndserver or X11 backends.

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
- `-widescreen` (16:9 logical size), `-stretch` (fill without preserving aspect)
- `-fullscreen` / `-windowed`
- Alt+Enter toggles fullscreen at runtime.
- Multiplayer bootstrap:
  - Host: `-host N` (optionally `-port P`)
  - Join: `-connect host[:port]`
  - All packets are MACed with BLAKE2s using a session key sent during JOIN_ACK.

Saves/config: by default `~/.doomrc` and `.dsg` save files in the working directory.

## Project status
Core SDL2 modernization is complete. See `STATUS.md` for current state and `TODO.md` for the roadmap. `AGENTS.md` documents repo-specific guidance.

## License
- Core source: GNU GPL-2.0-only (see `LICENSE.TXT`).
- Third-party components and their licenses are listed in `THIRD_PARTY_NOTICES.md` (SDL2/SDL2_net under zlib, libADLMIDI under LGPL/GPL/MIT, libOPNMIDI under LGPL/GPL/MIT).
- Included lightweight BLAKE2s implementation is public domain/CC0.

## Support & contributions
Issues and patches are welcome. Please keep the SDL2-only build contract intact and adhere to the licensing notes above when redistributing binaries. Plain-text debug artifacts (e.g., personal logs) should not be committed for releases.
