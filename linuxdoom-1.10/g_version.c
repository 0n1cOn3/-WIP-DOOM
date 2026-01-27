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
// DESCRIPTION:
//  Game version capability system implementation.
//  Provides centralized version metadata and capability queries.
//
//-----------------------------------------------------------------------------

#include "g_version.h"

// Game version capability table
// Defines all version-specific metadata in a single, centralized location
// This eliminates the need for scattered gamemode checks throughout the code
const gameversion_t game_versions[5] =
{
    // DOOM Shareware (Episode 1 only)
    {
        .mode = shareware,
        .name = "DOOM Shareware",
        .max_episodes = 1,
        .max_maps_per_episode = 9,
        .total_maps = 9,
        .sky_episode_base = 1,
        .help_lump = "HELP1",
        .has_help2 = true,
        .allow_pwads = false,           // Shareware disallows PWADs
        .fast_finale_skip = false       // Uses slow text finale
    },

    // DOOM Registered (Episodes 1-3)
    {
        .mode = registered,
        .name = "DOOM Registered",
        .max_episodes = 3,
        .max_maps_per_episode = 9,
        .total_maps = 27,
        .sky_episode_base = 1,
        .help_lump = "HELP1",
        .has_help2 = true,
        .allow_pwads = true,
        .fast_finale_skip = false       // Uses slow text finale
    },

    // Ultimate DOOM (Episodes 1-4, retail)
    {
        .mode = retail,
        .name = "Ultimate DOOM",
        .max_episodes = 4,
        .max_maps_per_episode = 9,
        .total_maps = 36,
        .sky_episode_base = 1,
        .help_lump = "HELP1",
        .has_help2 = true,
        .allow_pwads = true,
        .fast_finale_skip = false       // Uses slow text finale
    },

    // DOOM II (Map-based, no episodes)
    // Note: Also covers TNT: Evilution and The Plutonia Experiment
    {
        .mode = commercial,
        .name = "DOOM II",
        .max_episodes = 1,              // Map-based, not episode-based
        .max_maps_per_episode = 0,      // 0 indicates map-based, not episode-based
        .total_maps = 34,
        .sky_episode_base = 0,
        .help_lump = "HELP",            // Single help screen for commercial
        .has_help2 = false,
        .allow_pwads = true,
        .fast_finale_skip = true        // Allows fast finale skipping
    },

    // Unknown/Indetermined (Default fallback)
    {
        .mode = indetermined,
        .name = "Unknown",
        .max_episodes = 1,
        .max_maps_per_episode = 9,
        .total_maps = 9,
        .sky_episode_base = 1,
        .help_lump = "HELP1",
        .has_help2 = true,
        .allow_pwads = false,
        .fast_finale_skip = false
    }
};

// Current version pointer - set by G_InitVersion()
const gameversion_t *current_version = NULL;

// Initialize the version system
// Must be called after gamemode has been detected
void G_InitVersion(GameMode_t mode)
{
    int i;

    for (i = 0; i < 5; i++)
    {
        if (game_versions[i].mode == mode)
        {
            current_version = &game_versions[i];
            return;
        }
    }

    // Default to indetermined if not found
    current_version = &game_versions[4];
}

// Capability query: Get maximum accessible episodes for current version
int G_MaxEpisodes(void)
{
    if (!current_version)
        return 1;   // Safe fallback

    return current_version->max_episodes;
}

// Capability query: Get maximum maps for episode
// For map-based games (commercial), returns total maps
// For episode-based games, returns maps per episode
int G_MaxMaps(int episode)
{
    if (!current_version)
        return 9;   // Safe fallback

    // If max_maps_per_episode is 0, this is a map-based game
    if (current_version->max_maps_per_episode > 0)
        return current_version->max_maps_per_episode;
    else
        return current_version->total_maps;  // Map-based: return total
}

// Capability query: Get total playable maps for current version
int G_TotalMaps(void)
{
    if (!current_version)
        return 9;   // Safe fallback

    return current_version->total_maps;
}

// Capability query: Whether PWADs are allowed in this version
boolean G_AllowPWADs(void)
{
    if (!current_version)
        return false;   // Safe fallback

    return current_version->allow_pwads;
}

// Capability query: Whether finale allows fast skipping (commercial-style)
boolean G_FastFinaleSkip(void)
{
    if (!current_version)
        return false;   // Safe fallback

    return current_version->fast_finale_skip;
}

// Capability query: Get help screen lump name for current version
// Returns "HELP" for commercial, "HELP1" for others
const char* G_HelpLump(void)
{
    if (!current_version)
        return "HELP1";   // Safe fallback

    return current_version->help_lump;
}

// Capability query: Whether HELP2 lump exists for current version
boolean G_HasHelp2(void)
{
    if (!current_version)
        return true;    // Safe fallback

    return current_version->has_help2;
}

//-----------------------------------------------------------------------------
//
// $Log:$
//
//-----------------------------------------------------------------------------
