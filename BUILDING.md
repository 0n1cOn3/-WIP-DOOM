# Building linuxdoom

This project now targets SDL2 for video, input, and audio. The build system supports
flexible configuration via CMake options.

## Build Options

- `NETWORK_BACKEND`: Network implementation to use
  - `SDL_NET` (default): Use SDL_net library for multiplayer networking
  - `BSD`: Use BSD sockets (traditional sockets API, no SDL_net dependency)

## Prerequisites

Ensure the following packages (names from Debian/Ubuntu) are installed for the
selected backends:

- Common: `build-essential`, `cmake`
- SDL2 video/input (default): `libsdl2-dev`
- SDL_net networking (default): `libsdl2-net-dev`
- BSD networking: typically provided by libc

## Example configurations

Wayland-friendly (default) SDL2 build with SDL_net networking:

```bash
cmake -S . -B build
cmake --build build
```

SDL2 video with BSD networking:

```bash
cmake -S . -B build-bsd -DNETWORK_BACKEND=BSD
cmake --build build-bsd
```

Build artifacts are written to `build/bin` (or the chosen build directory).

## Audio Configuration

The audio system uses SDL2 with the following defaults:

- **Sample Rate**: 11025 Hz (legacy DOOM standard)
- **Channels**: 2 (stereo)
- **Bit Depth**: 16-bit signed integers

To change the sample rate, edit the `SAMPLERATE` macro in `linuxdoom-1.10/i_sound.c`
and rebuild. This requires no other changes as the mixing algorithm is format-agnostic.

## Testing

After building, verify the build by running the binary:

```bash
./build/bin/linuxdoom -iwad /path/to/doom.wad
```

Press `Alt+Enter` to toggle fullscreen mode. The game should display with proper 4:3
aspect ratio (with letterboxing/pillarboxing as needed on modern displays).
