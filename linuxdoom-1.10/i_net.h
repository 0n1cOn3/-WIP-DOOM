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
// FITNESS FOR A PARTICULAR PURPOSE. See the DOOM Source Code License
// for more details.
//
// DESCRIPTION:
//	System specific network interface stuff.
//
//-----------------------------------------------------------------------------


#ifndef __I_NET__
#define __I_NET__


#ifdef __GNUG__
#pragma interface
#endif

#include <SDL_net.h>
#include "d_net.h"

// Called by D_DoomMain.


void I_InitNetwork (void);
void I_ShutdownNetwork(void);
void I_NetCmd (void);

// Shared helpers for packing and unpacking network payloads.
void I_NetPackBuffer(const doomdata_t *src, doomdata_t *dest);
void I_NetUnpackBuffer(const doomdata_t *src, doomdata_t *dest);

// Simple regression harness entry point.
int I_RunNetworkHarness(int argc, char **argv);

int I_GetNetLatencyMs(void);
int I_GetNetPacketLoss(void);
void I_SetNetLatencyMs(int ms);
void I_SetNetPacketLoss(int percent);
Uint16 I_GetNetDefaultPort(void);
int I_ResolveNetAddress(const char *spec, IPaddress *out);
void I_SetVanillaOnly(int on);
void I_SetNetStartSettings(int skill, int episode, int map);
void I_GetNetStartSettings(int *skill, int *episode, int *map);
int I_RunLanDiscovery(IPaddress *out, int max);
int I_GetDiscoveredServers(IPaddress *out, int max);
void I_GetSessionInfo(uint8_t key16[16], uint8_t hash16[16], int *vanilla_only);
int I_GetLobbyPlayerCount(void);
int I_GetTotalPlayers(void);
void I_GetLobbyRoster(char names[][16], int max);
int I_WasNetContentMismatch(void);

// Async network init for lobby UI (Phase 3).
enum
{
    NET_STATUS_INIT = 0,
    NET_STATUS_WAITING,
    NET_STATUS_READY,
    NET_STATUS_REJECTED,
    NET_STATUS_TIMEOUT,
    NET_STATUS_ERROR
};

void I_InitNetworkAsync(int is_host, int player_count);
int I_PollNetworkInit(void);        // returns NET_STATUS_*
void I_FinishNetworkInit(void);     // completes setup when NET_STATUS_READY
void I_CancelNetworkInit(void);     // cancel/cleanup
void I_SetConnectTarget(IPaddress addr); // for menu-driven connects
int I_LobbyStartGame(void);         // host-only: broadcast START and transition to READY

#endif
//-----------------------------------------------------------------------------
//
// $Log:$
//
//-----------------------------------------------------------------------------
