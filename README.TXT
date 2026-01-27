
Here it is, at long last.  The DOOM source code is released for your
non-profit use.  You still need real DOOM data to work with this code.
If you don't actually own a real copy of one of the DOOMs, you should
still be able to find them at software stores.

Many thanks to Bernd Kreimeier for taking the time to clean up the
project and make sure that it actually works.  Projects tends to rot if
you leave it alone for a few years, and it takes effort for someone to
deal with it again.

The bad news:  this code only compiles and runs on linux.  We couldn't
release the dos code because of a copyrighted sound library we used
(wow, was that a mistake -- I write my own sound code now), and I
honestly don't even know what happened to the port that microsoft did
to windows.

Still, the code is quite portable, and it should be straightforward to
bring it up on just about any platform.

I wrote this code a long, long time ago, and there are plenty of things
that seem downright silly in retrospect (using polar coordinates for
clipping comes to mind), but overall it should still be a usefull base
to experiment and build on.

The basic rendering concept -- horizontal and vertical lines of constant
Z with fixed light shading per band was dead-on, but the implementation
could be improved dramatically from the original code if it were
revisited.  The way the rendering proceded from walls to floors to
sprites could be collapsed into a single front-to-back walk of the bsp
tree to collect information, then draw all the contents of a subsector
on the way back up the tree.  It requires treating floors and ceilings
as polygons, rather than just the gaps between walls, and it requires
clipping sprite billboards into subsector fragments, but it would be
The Right Thing.

The movement and line of sight checking against the lines is one of the
bigger misses that I look back on.  It is messy code that had some
failure cases, and there was a vastly simpler (and faster) solution
sitting in front of my face.  I used the BSP tree for rendering things,
but I didn't realize at the time that it could also be used for
environment testing.  Replacing the line of sight test with a bsp line
clip would be pretty easy.  Sweeping volumes for movement gets a bit
tougher, and touches on many of the challenges faced in quake / quake2
with edge bevels on polyhedrons.

Some project ideas:

Port it to your favorite operating system.

Add some rendering features -- transparency, look up / down, slopes,
etc.

Add some game features -- weapons, jumping, ducking, flying, etc.

Create a packet server based internet game.

Create a client / server based internet game.

Do a 3D accelerated version.  On modern hardware (fast pentium + 3DFX)
you probably wouldn't even need to be clever -- you could just draw the
entire level and get reasonable speed.  With a touch of effort, it should
easily lock at 60 fps (well, there are some issues with DOOM's 35 hz
timebase...).  The biggest issues would probably be the non-power of two
texture sizes and the walls composed of multiple textures.


I don't have a real good guess at how many people are going to be
playing with this, but if significant projects are undertaken, it would
be cool to see a level of community cooperation.  I know that most early
projects are going to be rough hacks done in isolation, but I would be
very pleased to see a coordinated 'net release of an improved, backwards
compatable version of DOOM on multiple platforms next year.

Have fun.

John Carmack
12-23-97

Copyright (c) ZeniMax Media Inc.
Licensed under the GNU General Public License 2.0.

Modern build
============
A CMake-based build is available for modern toolchains and package managers.
This project has been fully modernized to use SDL2 for all platform interfaces.

BUILD REQUIREMENTS
------------------
- CMake 3.16 or newer
- SDL2 development headers (libsdl2-dev on Debian/Ubuntu)
- SDL2_net development headers (libsdl2-net-dev on Debian/Ubuntu) - optional,
  an internal compatibility stub is used if unavailable

BUILDING
--------
    cmake -S . -B build
    cmake --build build

The resulting binary is written to `build/bin/linuxdoom`.

RUNNING
-------
You must supply an IWAD to run the game:

    DOOMWADDIR=/path/to/iwads ./build/bin/linuxdoom

If you have a PWAD, supply it alongside the IWAD:

    DOOMWADDIR=/path/to/iwads ./build/bin/linuxdoom -file /path/to/MOD.WAD

Convenience wrapper:

    ./run-linuxdoom.sh /path/to/DOOM.WAD
    IWAD_PATH=/path/to/DOOM.WAD PWAD_PATH=/path/to/MOD.WAD ./run-linuxdoom.sh

Note: `net_harness` is a networking test tool, not a game launcher.

Display options:
- `-widescreen` scales output to a 16:9 target aspect ratio (stretched).
- `-stretch` stretches to fill the window without preserving aspect.

SDL2 MODERNIZATION
-------------------
This version of DOOM has been modernized to use SDL2 for all platform interfaces:

* Video/Graphics: SDL2 video backend with automatic 4:3 aspect ratio preservation
  and fullscreen support (toggle with Alt+Enter). Works on X11, Wayland, and
  other SDL2-supported display servers.

* Audio: SDL2 audio backend providing 11025 Hz, 16-bit stereo output. No external
  sound server process required.

* Networking: SDL2_net for multiplayer networking with full IPv6 support. An
  internal compatibility stub is automatically used if SDL2_net is not available
  on the system.

* Input: SDL2 event handling for keyboard and mouse input with proper event
  queuing and window focus management.

LEGACY SUPPORT REMOVED
----------------------
The following legacy platform-specific code has been removed:
- X11 video backend (replaced by SDL2)
- SDL1 video backend (replaced by SDL2)
- OSS/ALSA audio drivers (replaced by SDL2)
- sndserver external sound process dependency
- BSD sockets networking (replaced by SDL2_net)

The X11 pathway and legacy sound server are no longer available. Use SDL2-capable
systems for this version of DOOM.

For further build details, see BUILDING.md
