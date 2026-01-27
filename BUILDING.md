# Building linuxdoom

This project uses SDL2 for all platform interfaces: video, input, audio, and networking.
The build system is designed to work out-of-the-box with minimal configuration.

## Prerequisites

Ensure the following packages (names from Debian/Ubuntu) are installed for the
selected backends:

- Common: `build-essential`, `cmake`
- SDL2 video/input (default): `libsdl2-dev`
- SDL_net networking (default): `libsdl2-net-dev`
- BSD networking: typically provided by libc

## Example build

```bash
cmake -S . -B build
cmake --build build
```

Build artifacts are written to `build/bin` directory.

## Network Configuration

The project uses SDL2_net for all multiplayer networking:

- **System SDL2_net Library**: If available on your system, the build will use the system SDL2_net library
- **Automatic Fallback**: If SDL2_net is not installed, an internal compatibility stub is automatically used
- **No Configuration Needed**: The CMake build system automatically detects and configures networking

## Network Backend Details

The internal SDL_net stub uses modern socket APIs (`getaddrinfo`) and supports both IPv4 and IPv6.
This provides excellent compatibility for systems without SDL2_net installed.

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
