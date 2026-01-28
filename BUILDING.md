# Building linuxdoom

This project uses SDL2 for all platform interfaces: video, input, audio, and networking.
Music playback is provided by libADLMIDI (OPL3) and libOPNMIDI (OPN2), with ALSA sequencer as an optional fallback.
The build system is designed to work out-of-the-box with minimal configuration.

## Prerequisites

Ensure the following packages (names from Debian/Ubuntu) are installed:

- Common: `build-essential`, `cmake`
- SDL2 video/input: `libsdl2-dev`
- SDL2_net networking (optional; stub fallback if missing): `libsdl2-net-dev`
- ALSA sequencer fallback (optional): `libasound2-dev`

## Example build

```bash
cmake -S . -B build
cmake --build build
```

Build artifacts are written to `build/bin`.

### Worktrees and multiple builds

If you use `git worktree` (e.g. `enhanced` + `vanilla` checked out at once), use a different build directory per worktree to avoid CMake cache conflicts:

```bash
cmake -S . -B build-enhanced
cmake --build build-enhanced
```

## Network configuration

The project uses SDL2_net for all multiplayer networking:

- **System SDL2_net library**: If available on your system, the build will link to it.
- **Automatic fallback**: If SDL2_net is not installed, an internal compatibility stub is used.
- **No configuration needed**: CMake detects and configures networking automatically.

The internal SDL_net stub uses modern socket APIs (`getaddrinfo`) and supports both IPv4 and IPv6.

## Audio configuration

The audio system uses SDL2 with the following defaults:

- **Sample Rate**: 11025 Hz (legacy DOOM standard)
- **Channels**: 2 (stereo)
- **Bit Depth**: 16-bit signed integers

To change the sample rate, edit the `SAMPLERATE` macro in `linuxdoom-1.10/i_sound.c`
and rebuild. This requires no other changes as the mixing algorithm is format-agnostic.

## Testing / sanity checks

There are no automated tests. After building, sanity-check the build by running:

```bash
./build/bin/net_harness
```

And a quick game run:

```bash
export DOOMWADDIR=/path/to/iwad
./build/bin/linuxdoom
```

Alternatively, use the wrapper script:

```bash
./run-linuxdoom.sh /path/to/DOOM.WAD
```

Press `Alt+Enter` to toggle fullscreen mode. The game should display with the selected aspect ratio (4:3 default, `-widescreen`, `-stretch`, or `-aspect ...`).

## Music backends

The build pulls libADLMIDI and libOPNMIDI via `FetchContent` (GPL-compatible). ALSA is used
only when available to provide a sequencer fallback. Music backend selection is available
in the Sound menu, or via `-music_backend` / `-midi` on the command line.
