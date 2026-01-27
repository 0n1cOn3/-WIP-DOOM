// Emacs style mode select   -*- C++ -*-
//-----------------------------------------------------------------------------
//
// $Id:$
//
// Copyright (C) 1993-1996 by id Software, Inc.
//
// This source is available for distribution and/or modification
// only under the terms of the DOOM Source Code License as
// published by id Software. All rights reserved.
//
// The source is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the DOOM Source Code License
// for more details.
//
//
// DESCRIPTION:
//  Game version capability system. Centralizes version-specific metadata
//  to provide a cleaner API than scattered gamemode/gamemission checks.
//
//  Instead of scattered 'if (gamemode == commercial)' checks throughout
//  the codebase, code can use capability queries like G_MaxEpisodes(),
//  G_AllowPWADs(), etc. This provides:
//
//  - Single source of truth for version metadata
//  - Cleaner, more maintainable code
//  - Easy to extend with new capabilities
//  - Self-documenting intent (checking capabilities, not versions)
//
//-----------------------------------------------------------------------------

#ifndef __G_VERSION__
#define __G_VERSION__

#include "doomtype.h"
#include "doomdef.h"

// Game version capability descriptor
// Contains all version-specific metadata in one place
typedef struct
{
    GameMode_t mode;                // Game mode (shareware, registered, etc.)
    const char *name;               // Human-readable version name
    int max_episodes;               // Maximum accessible episodes (1-4)
    int max_maps_per_episode;       // Maps per episode for episode-based games (0 for map-based)
    int total_maps;                 // Total playable maps
    int sky_episode_base;           // Base episode for sky texture selection
    const char *help_lump;          // Help screen lump name ("HELP" or "HELP1")
    boolean has_help2;              // Whether HELP2 lump exists for this version
    boolean allow_pwads;            // Whether -file option is allowed
    boolean fast_finale_skip;       // Whether finale allows fast skipping (commercial style)
} gameversion_t;

// Global version data and current version pointer
extern const gameversion_t game_versions[5];
extern const gameversion_t *current_version;

// Version system initialization
// Call this after gamemode has been detected in D_Main
void G_InitVersion(GameMode_t mode);

// Capability query functions - use these instead of scattered gamemode checks
int G_MaxEpisodes(void);
int G_MaxMaps(int episode);
int G_TotalMaps(void);
boolean G_AllowPWADs(void);
boolean G_FastFinaleSkip(void);
const char *G_HelpLump(void);
boolean G_HasHelp2(void);

#endif

//-----------------------------------------------------------------------------
//
// $Log:$
//
//-----------------------------------------------------------------------------
