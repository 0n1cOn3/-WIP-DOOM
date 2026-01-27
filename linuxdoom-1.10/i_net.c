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
// $Log:$
//
// DESCRIPTION:
//
//-----------------------------------------------------------------------------

static const char
rcsid[] = "$Id: m_bbox.c,v 1.1 1997/02/03 22:45:10 b1 Exp $";

#include <errno.h>
#include <stddef.h>
#include <stdint.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <time.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <SDL.h>
#include <SDL_net.h>
#include "blake2s.h"
#include "d_main.h"   // wadfiles[]

#include "i_system.h"
#include "d_event.h"
#include "d_net.h"
#include "m_argv.h"

#include "doomstat.h"

#ifdef __GNUG__
#pragma implementation "i_net.h"
#endif
#include "i_net.h"

// Standard DOOM networking port (originally IPPORT_USERRESERVED + 0x1d = 5000 + 29)
#define DOOM_DEFAULT_PORT 5029
#define DISCOVERY_PORT 5030
#define DISCOVERY_MAGIC SDL_SwapBE32(0x44495343) /* 'DISC' */
#define JOIN_MAGIC SDL_SwapBE32(0x444A4F49)      /* 'DJOI' */
#define START_MAGIC SDL_SwapBE32(0x53544152)     /* 'STAR' */

// Discovery and lobby protocol message types.
// Discovery:
//   PROBE:    [magic][type=1]
//   ANNOUNCE: [magic][type=2][doomport_be16]
// Lobby:
//   JOIN_REQ: [magic][type=1][...]
//   JOIN_ACK: [magic][type=2][...]
//   START:    [magic][type=3][...]
//   JOIN_NACK:[magic][type=4][...]
static const Uint8 TYPE_JOIN_REQ = 1;
static const Uint8 TYPE_JOIN_ACK = 2;
static const Uint8 TYPE_START = 3;
static const Uint8 TYPE_JOIN_NACK = 4;
static const Uint8 TYPE_DISC_PROBE = 1;
static const Uint8 TYPE_DISC_ANNOUNCE = 2;

void    NetSend (void);
boolean NetListen (void);

static void NetListenThunk(void);

void    (*netget) (void);
void    (*netsend) (void);

typedef uint16_t doom_port_t;

// Network simulation variables for testing/debugging.
// -netdelay <ms>: Add latency to outgoing packets (max 2000ms)
// -packetloss <percent>: Randomly drop packets (0-99%)
int net_latency_ms = 0;
int net_packet_loss = 0;
// Thread-safe RNG seed for network simulation
static unsigned int net_rng_seed = 0;

// UDP socket and packet buffers
static UDPsocket             udpsocket;
static UDPpacket            *recvpacket;
static UDPpacket            *sendpacket;
static IPaddress             sendaddress[MAXNETNODES];
static boolean               sdl_net_inited = false;

// Network configuration and state
static doom_port_t doomport = DOOM_DEFAULT_PORT;
static IPaddress connect_target;                // target address for -connect
static boolean use_host_flow = false;
static boolean use_client_flow = false;
static int desired_players = 0;
static int assigned_node = -1;
static boolean net_vanilla_only = false;
static uint8_t session_key[16];                 // 128-bit session key for MAC
static uint8_t content_hash[16];                // hash over IWAD/PWAD set
static IPaddress discovered[16];
static int discovered_count = 0;
static int net_total_players = 0;
static int net_connected_players = 0;
static char net_roster[MAXPLAYERS][16];
static int net_start_skill = 2;
static int net_start_episode = 1;
static int net_start_map = 1;

typedef Uint16 netorder_16;
typedef Uint32 netorder_32;

// Async network init state (Phase 3).
static int net_init_state = NET_STATUS_INIT;
static Uint32 net_init_start_ticks = 0;
static Uint32 net_init_last_send_ticks = 0;
static int net_init_is_host = 0;
static int net_init_total_players = 0;
static int net_init_have_clients = 0;
static int net_init_reject_reason = 0;
static int net_init_content_mismatch = 0;

static UDPsocket discovery_socket;
static UDPpacket *discovery_packet;

/*
 * Parse a positive integer from a command-line argument string.
 *
 * text:       String to parse
 * upperBound: Maximum allowed value (if > 0). Values <= 0 mean no upper limit.
 * fallback:   Value to return if parsing fails or result is non-positive
 *
 * Returns the parsed positive integer, clamped to upperBound if specified,
 * or fallback if the input is invalid.
 */
static int ParsePositiveIntArg(const char *text, int upperBound, int fallback)
{
    char *end = NULL;
    long value;

    if (!text || !*text)
        return fallback;

    value = strtol(text, &end, 10);
    if (end == text || value <= 0)
        return fallback;

    if (upperBound > 0 && value > upperBound)
        value = upperBound;

    return (int)value;
}

static int ClampInt(int value, int min_value, int max_value)
{
    if (value < min_value)
        return min_value;
    if (value > max_value)
        return max_value;
    return value;
}

int I_GetNetLatencyMs(void)
{
    return net_latency_ms;
}

int I_GetNetPacketLoss(void)
{
    return net_packet_loss;
}

void I_SetNetLatencyMs(int ms)
{
    net_latency_ms = ClampInt(ms, 0, 2000);
}

void I_SetNetPacketLoss(int percent)
{
    net_packet_loss = ClampInt(percent, 0, 99);
}

Uint16 I_GetNetDefaultPort(void)
{
    return (Uint16)doomport;
}

void I_SetVanillaOnly(int on)
{
    net_vanilla_only = on ? true : false;
}

void I_SetNetStartSettings(int skill, int episode, int map)
{
    // Keep within DOOM's expected ranges.
    net_start_skill = ClampInt(skill, 1, 5);
    net_start_episode = ClampInt(episode, 1, 4);
    net_start_map = ClampInt(map, 1, 32);
}

void I_GetNetStartSettings(int *skill, int *episode, int *map)
{
    if (skill) *skill = net_start_skill;
    if (episode) *episode = net_start_episode;
    if (map) *map = net_start_map;
}

void I_GetSessionInfo(uint8_t key16[16], uint8_t hash16[16], int *vanilla_only)
{
    if (key16)
        memcpy(key16, session_key, 16);
    if (hash16)
        memcpy(hash16, content_hash, 16);
    if (vanilla_only)
        *vanilla_only = net_vanilla_only ? 1 : 0;
}

int I_GetLobbyPlayerCount(void)
{
    return net_connected_players;
}

int I_GetTotalPlayers(void)
{
    return net_total_players;
}

void I_GetLobbyRoster(char names[][16], int max)
{
    int n = (net_total_players < max) ? net_total_players : max;
    for (int i = 0; i < n; ++i)
    {
        memcpy(names[i], net_roster[i], 16);
    }
}

int I_WasNetContentMismatch(void)
{
    return net_init_content_mismatch ? 1 : 0;
}

int I_GetDiscoveredServers(IPaddress *out, int max)
{
    int n = discovered_count < max ? discovered_count : max;
    for (int i = 0; i < n; ++i)
        out[i] = discovered[i];
    return n;
}

// Broadcast probe; collect announces for ~0.5s
int I_RunLanDiscovery(IPaddress *out, int max)
{
    discovered_count = 0;
    UDPsocket dsock = SDLNet_UDP_Open(0);
    if (!dsock)
        return 0;
    UDPpacket *pkt = SDLNet_AllocPacket(64);
    if (!pkt)
    {
        SDLNet_UDP_Close(dsock);
        return 0;
    }

    IPaddress bcast;
    bcast.host = 0xFFFFFFFF;
    bcast.port = SDL_SwapBE16(DISCOVERY_PORT);

    uint8_t probe[5] = {0};
    SDLNet_Write32(DISCOVERY_MAGIC, probe);
    probe[4] = TYPE_DISC_PROBE;
    memcpy(pkt->data, probe, sizeof(probe));
    pkt->len = 5;
    pkt->address = bcast;
    SDLNet_UDP_Send(dsock, -1, pkt);

    Uint32 start = SDL_GetTicks();
    while ((SDL_GetTicks() - start) < 500 && discovered_count < (int)(sizeof(discovered)/sizeof(discovered[0])))
    {
        if (SDLNet_UDP_Recv(dsock, pkt) > 0)
        {
            if (pkt->len >= 5 && SDLNet_Read32(pkt->data) == DISCOVERY_MAGIC && pkt->data[4] == TYPE_DISC_ANNOUNCE)
            {
                IPaddress a = pkt->address;
                // Older announces didn't include a port. Newer ones include [doomport_be16].
                if (pkt->len >= 7)
                    memcpy(&a.port, pkt->data + 5, sizeof(a.port));
                else
                    a.port = SDL_SwapBE16(doomport);

                discovered[discovered_count++] = a;
                if (out && discovered_count <= max)
                    out[discovered_count-1] = a;
            }
        }
    }

    SDLNet_FreePacket(pkt);
    SDLNet_UDP_Close(dsock);
    return discovered_count;
}

// Initialize network simulation parameters from command-line arguments.
// Note: This function is called during single-threaded startup from I_InitNetwork().
// The RNG seed initialization is not protected by a mutex as DOOM's initialization
// is single-threaded. However, ShouldDropPacket() uses rand_r() which is thread-safe.
static void InitNetworkSimulation(void)
{
    static boolean seeded = false;
    int p;

    if (!seeded)
    {
        net_rng_seed = (unsigned int)time(NULL);
        seeded = true;
    }

    p = M_CheckParm("-netdelay");
    if (p && p < myargc - 1)
        I_SetNetLatencyMs(ParsePositiveIntArg(myargv[p + 1], 2000, 0));

    p = M_CheckParm("-packetloss");
    if (p && p < myargc - 1)
        I_SetNetPacketLoss(ParsePositiveIntArg(myargv[p + 1], 99, 0));
}

// Thread-safe packet drop simulation using rand_r().
// Note: This is for testing/debugging only. The random number
// generation is not cryptographically secure. The modulo operation
// introduces a slight bias in the distribution, which is acceptable
// for network simulation testing purposes.
static boolean ShouldDropPacket(void)
{
    if (net_packet_loss <= 0)
        return false;

    return (rand_r(&net_rng_seed) % 100) < net_packet_loss;
}

static void ApplyNetworkLatency(void)
{
    if (net_latency_ms <= 0)
        return;

    usleep((unsigned long)net_latency_ms * 1000UL);
}

static netorder_32 NetWrite32(uint32_t value)
{
    netorder_32 encoded;
    SDLNet_Write32(value, &encoded);
    return encoded;
}

static netorder_16 NetWrite16(uint16_t value)
{
    netorder_16 encoded;
    SDLNet_Write16(value, &encoded);
    return encoded;
}

static uint32_t NetRead32(const netorder_32 *value)
{
    return SDLNet_Read32(value);
}

static uint16_t NetRead16(const netorder_16 *value)
{
    return SDLNet_Read16(value);
}

// MAC utility using BLAKE2s (16-byte output)
static void Net_MakeMac(const uint8_t *data, int len, uint8_t out[16])
{
    blake2s_mac16(session_key, sizeof(session_key), data, (size_t)len, out);
}

static void Net_GenerateSessionKey(void)
{
    uint64_t ticks = (uint64_t)SDL_GetPerformanceCounter() ^ (uint64_t)time(NULL);
    for (int i = 0; i < 16; ++i)
    {
        ticks ^= ticks << 13;
        ticks ^= ticks >> 7;
        ticks ^= ticks << 17;
        session_key[i] = (uint8_t)(ticks & 0xFF);
    }
}

static int HashFile(const char *path, uint8_t out[16])
{
    int fd = open(path, O_RDONLY);
    if (fd < 0)
        return -1;
    blake2s_state S;
    blake2s_init(&S, 16);
    uint8_t buf[4096];
    ssize_t r;
    while ((r = read(fd, buf, sizeof(buf))) > 0)
        blake2s_update(&S, buf, (size_t)r);
    close(fd);
    if (r < 0)
        return -1;
    blake2s_final(&S, out, 16);
    return 0;
}

static void ComputeContentHash(uint8_t out[16])
{
    blake2s_state S;
    blake2s_init(&S, 16);
    for (int i = 0; i < MAXWADFILES && wadfiles[i]; ++i)
    {
        uint8_t fh[16];
        if (HashFile(wadfiles[i], fh) == 0)
        {
            blake2s_update(&S, fh, sizeof(fh));
            blake2s_update(&S, wadfiles[i], strlen(wadfiles[i]));
        }
    }
    blake2s_final(&S, out, 16);
}

void I_NetPackBuffer(const doomdata_t *src, doomdata_t *dest)
{
    int c;

    *dest = *src;
    dest->checksum = NetWrite32(src->checksum);

    for (c = 0; c < src->numtics; ++c)
    {
        dest->cmds[c].angleturn = NetWrite16(src->cmds[c].angleturn);
        dest->cmds[c].consistancy = NetWrite16(src->cmds[c].consistancy);
    }
}

void I_NetUnpackBuffer(const doomdata_t *src, doomdata_t *dest)
{
    int c;

    *dest = *src;
    dest->checksum = NetRead32(&src->checksum);

    for (c = 0; c < dest->numtics; ++c)
    {
        dest->cmds[c].angleturn = NetRead16(&src->cmds[c].angleturn);
        dest->cmds[c].consistancy = NetRead16(&src->cmds[c].consistancy);
    }
}

static boolean NetAddressesEqual(const IPaddress *a, const IPaddress *b)
{
    if (!a || !b)
        return false;

    return a->host == b->host && a->port == b->port;
}

static doom_port_t ParsePort(const char *text, doom_port_t fallback)
{
    char *end = NULL;
    long value;

    if (!text || !*text)
        return fallback;

    value = strtol(text, &end, 10);
    if (end == text || value <= 0 || value > 65535)
        return fallback;

    return (Uint16)value;
}

static boolean ResolveAddressSpec(const char *spec,
                                  Uint16 defaultPort,
                                  IPaddress *out)
{
    Uint16 port = defaultPort;
    char address[256];
    const char *portText = NULL;
    const char *endBracket;
    const char *colon;

    memset(address, 0, sizeof(address));

    if (!spec || !*spec)
        return false;

    if (spec[0] == '[')
    {
        size_t len;
        endBracket = strchr(spec, ']');
        if (!endBracket)
            return false;
        len = endBracket - spec - 1;
        if (len >= sizeof(address))
            len = sizeof(address) - 1;
        strncpy(address, spec + 1, len);
        address[len] = '\0';
        if (endBracket[1] == ':' && endBracket[2] != '\0')
            portText = endBracket + 2;
    }
    else
    {
        size_t len;
        colon = strrchr(spec, ':');
        if (colon && strchr(colon + 1, ':') == NULL)
        {
            portText = colon + 1;
            len = colon - spec;
            if (len >= sizeof(address))
                len = sizeof(address) - 1;
            strncpy(address, spec, len);
            address[len] = '\0';
        }
        else
        {
            strncpy(address, spec, sizeof(address) - 1);
            address[sizeof(address) - 1] = '\0';
        }
    }

    if (portText)
        port = ParsePort(portText, defaultPort);

    if (SDLNet_ResolveHost(out, address[0] ? address : spec, port) == -1)
        return false;

    return true;
}

int I_ResolveNetAddress(const char *spec, IPaddress *out)
{
    return ResolveAddressSpec(spec, doomport, out) ? 1 : 0;
}

static void EnsurePacketCapacity(int length)
{
    if (!recvpacket || recvpacket->maxlen < length)
    {
        if (recvpacket)
            SDLNet_FreePacket(recvpacket);
        recvpacket = SDLNet_AllocPacket(length);
    }

    if (!sendpacket || sendpacket->maxlen < length)
    {
        if (sendpacket)
            SDLNet_FreePacket(sendpacket);
        sendpacket = SDLNet_AllocPacket(length);
    }

    if (!recvpacket || !sendpacket)
        I_Error("Failed to allocate UDP packets");
}

void NetSend (void)
{
    doomdata_t wire;
    int length = doomcom->datalength;
    int total = length + 16;

    if (ShouldDropPacket())
        return;

    ApplyNetworkLatency();

    EnsurePacketCapacity(total);
    I_NetPackBuffer(netbuffer, &wire);

    uint8_t mac[16];
    Net_MakeMac((uint8_t *)&wire, length, mac);

    memcpy(sendpacket->data, mac, 16);
    memcpy(sendpacket->data + 16, &wire, length);
    sendpacket->len = total;
    sendpacket->address = sendaddress[doomcom->remotenode];

    if (SDLNet_UDP_Send(udpsocket, -1, sendpacket) == 0)
        I_Error("SendPacket error: %s", SDLNet_GetError());
}

boolean NetListen (void)
{
    doomdata_t wire;
    int i;

    doomcom->remotenode = -1;

    if (!recvpacket)
        return false;

    if (SDLNet_UDP_Recv(udpsocket, recvpacket) <= 0)
        return false;

    if (ShouldDropPacket())
        return false;

    ApplyNetworkLatency();

    for (i = 0; i < doomcom->numnodes; ++i)
    {
        if (NetAddressesEqual(&recvpacket->address, &sendaddress[i]))
        {
            doomcom->remotenode = i;
            break;
        }
    }

    if (doomcom->remotenode == -1)
        return false;

    if (recvpacket->len < 16)
        return false;

    doomcom->datalength = recvpacket->len - 16;
    if (doomcom->datalength <= 0 || doomcom->datalength > (int)sizeof(wire))
        return false;

    uint8_t mac[16], mac_check[16];
    memcpy(mac, recvpacket->data, 16);
    memcpy(&wire, recvpacket->data + 16, doomcom->datalength);
    Net_MakeMac((uint8_t *)&wire, doomcom->datalength, mac_check);
    if (memcmp(mac, mac_check, 16) != 0)
        return false;

    I_NetUnpackBuffer(&wire, netbuffer);

    return true;
}

static void NetListenThunk(void)
{
    if (!NetListen())
        doomcom->remotenode = -1;
}

static void CloseDiscoverySocket(void)
{
    if (discovery_packet)
    {
        SDLNet_FreePacket(discovery_packet);
        discovery_packet = NULL;
    }
    if (discovery_socket)
    {
        SDLNet_UDP_Close(discovery_socket);
        discovery_socket = NULL;
    }
}

static void ResetAsyncInitState(void)
{
    net_init_state = NET_STATUS_INIT;
    net_init_start_ticks = 0;
    net_init_last_send_ticks = 0;
    net_init_is_host = 0;
    net_init_total_players = 0;
    net_init_have_clients = 0;
    net_init_reject_reason = 0;
    net_init_content_mismatch = 0;
    assigned_node = -1;
}

void I_SetConnectTarget(IPaddress addr)
{
    connect_target = addr;
}

void I_CancelNetworkInit(void)
{
    // Only cancel pre-game lobby init. If netgame is already active, leave it alone.
    if (netgame)
        return;

    CloseDiscoverySocket();

    if (udpsocket)
    {
        SDLNet_UDP_Close(udpsocket);
        udpsocket = NULL;
    }

    // Keep recvpacket/sendpacket allocated (safe), but reset lobby-visible state.
    net_total_players = 0;
    net_connected_players = 0;
    memset(net_roster, 0, sizeof(net_roster));
    memset(sendaddress, 0, sizeof(sendaddress));
    memset(session_key, 0, sizeof(session_key));
    memset(content_hash, 0, sizeof(content_hash));

    ResetAsyncInitState();
}

void I_InitNetworkAsync(int is_host, int player_count)
{
    // Cancel any in-flight init first.
    if (net_init_state != NET_STATUS_INIT && net_init_state != NET_STATUS_READY)
        I_CancelNetworkInit();

    // SDLNet is initialized during startup in I_InitNetwork(), but keep this robust.
    if (!sdl_net_inited)
    {
        if (SDLNet_Init() == -1)
        {
            net_init_state = NET_STATUS_ERROR;
            return;
        }
        sdl_net_inited = true;
    }

    InitNetworkSimulation();

    // Bind game port.
    if (!udpsocket)
    {
        udpsocket = SDLNet_UDP_Open(doomport);
        if (!udpsocket)
        {
            net_init_state = NET_STATUS_ERROR;
            return;
        }
    }

    // Packet buffers large enough for both doomdata_t and lobby control messages.
    EnsurePacketCapacity(sizeof(doomdata_t));

    // Compute local content hash up front for both host and client.
    ComputeContentHash(content_hash);

    net_init_is_host = is_host ? 1 : 0;
    // DOOM netplay is limited to MAXPLAYERS (typically 4).
    net_init_total_players = net_init_is_host ? ClampInt(player_count, 2, MAXPLAYERS) : 0;
    net_init_have_clients = 0;
    net_init_reject_reason = 0;
    net_init_content_mismatch = 0;
    net_init_start_ticks = SDL_GetTicks();
    net_init_last_send_ticks = 0;
    net_init_state = NET_STATUS_WAITING;

    // Lobby-visible state.
    memset(net_roster, 0, sizeof(net_roster));
    strncpy(net_roster[0], playername, 15);
    net_total_players = net_init_is_host ? net_init_total_players : 0;
    net_connected_players = net_init_is_host ? 1 : 0;

    if (net_init_is_host)
    {
        // Enforce "vanilla-only" as "host must not load PWADs".
        if (net_vanilla_only && wadfiles[1] != NULL)
        {
            net_init_state = NET_STATUS_ERROR;
            return;
        }

        Net_GenerateSessionKey();
        I_SetNetStartSettings(net_start_skill, net_start_episode, net_start_map);

        // Listen/respond for LAN discovery.
        if (!discovery_socket)
        {
            discovery_socket = SDLNet_UDP_Open(DISCOVERY_PORT);
            if (discovery_socket)
                discovery_packet = SDLNet_AllocPacket(64);
        }
    }
    else
    {
        // Client side: pre-fill roster slot with our own name (helps BASIC lobby).
        memset(net_roster, 0, sizeof(net_roster));
        if (assigned_node >= 1 && assigned_node < MAXPLAYERS)
            strncpy(net_roster[assigned_node], playername, 15);
    }
}

static void SendDiscoveryAnnounce(void)
{
    if (!discovery_socket || !discovery_packet)
        return;

    // [magic][type=ANNOUNCE][doomport_be16]
    SDLNet_Write32(DISCOVERY_MAGIC, discovery_packet->data);
    discovery_packet->data[4] = TYPE_DISC_ANNOUNCE;
    SDLNet_Write16(doomport, discovery_packet->data + 5);
    discovery_packet->len = 7;
    // Reply directly to last probe sender; address set by caller. If unset, broadcast.
    SDLNet_UDP_Send(discovery_socket, -1, discovery_packet);
}

static void PollDiscoverySocket(void)
{
    if (!discovery_socket || !discovery_packet)
        return;

    // Respond to probes.
    while (SDLNet_UDP_Recv(discovery_socket, discovery_packet) > 0)
    {
        if (discovery_packet->len >= 5 &&
            SDLNet_Read32(discovery_packet->data) == DISCOVERY_MAGIC &&
            discovery_packet->data[4] == TYPE_DISC_PROBE)
        {
            // Reply with ANNOUNCE back to the requester.
            SendDiscoveryAnnounce();
        }
    }
}

static void Host_ProcessJoinReq(void)
{
    if (recvpacket->len < 38)
        return;
    if (SDLNet_Read32(recvpacket->data) != JOIN_MAGIC)
        return;
    if (recvpacket->data[4] != TYPE_JOIN_REQ)
        return;

    // Always require exact content match for determinism and mod gating.
    if (memcmp(recvpacket->data + 6, content_hash, 16) != 0)
    {
        net_init_content_mismatch = 1;
        uint8_t nack[8] = {0};
        SDLNet_Write32(JOIN_MAGIC, nack);
        nack[4] = TYPE_JOIN_NACK;
        nack[5] = 1; // content mismatch
        memcpy(sendpacket->data, nack, sizeof(nack));
        sendpacket->len = (int)sizeof(nack);
        sendpacket->address = recvpacket->address;
        SDLNet_UDP_Send(udpsocket, -1, sendpacket);
        return;
    }

    // Ignore duplicates.
    boolean known = false;
    for (int i = 1; i <= net_init_have_clients; ++i)
    {
        if (NetAddressesEqual(&recvpacket->address, &sendaddress[i]))
        {
            known = true;
            break;
        }
    }
    if (known)
        return;

    if (net_init_have_clients >= (net_init_total_players - 1))
        return;

    net_init_have_clients++;
    sendaddress[net_init_have_clients] = recvpacket->address;

    // Store client name (16 bytes at offset 22)
    memset(net_roster[net_init_have_clients], 0, 16);
    memcpy(net_roster[net_init_have_clients], recvpacket->data + 22, 16);

    net_connected_players = net_init_have_clients + 1;
    net_total_players = net_init_total_players;

    // JOIN_ACK: [magic][type][node][total][vanilla][session_key16]
    uint8_t ack[24] = {0};
    SDLNet_Write32(JOIN_MAGIC, ack);
    ack[4] = TYPE_JOIN_ACK;
    ack[5] = (uint8_t)net_init_have_clients;
    ack[6] = (uint8_t)net_init_total_players;
    ack[7] = net_vanilla_only ? 1 : 0;
    memcpy(ack + 8, session_key, 16);
    memcpy(sendpacket->data, ack, sizeof(ack));
    sendpacket->len = (int)sizeof(ack);
    sendpacket->address = recvpacket->address;
    SDLNet_UDP_Send(udpsocket, -1, sendpacket);
}

static void Host_SendStart(void)
{
    int have_clients = net_init_have_clients;

    // START: [magic][type][total][client_count][flags][skill][episode][map][pad]
    //        [client_addrs...]
    //        [roster[MAXPLAYERS][16]]
    //        [content_hash16]
    uint8_t startbuf[12 + sizeof(IPaddress) * (MAXNETNODES - 1) + (MAXPLAYERS * 16) + 16] = {0};
    SDLNet_Write32(START_MAGIC, startbuf);
    startbuf[4] = TYPE_START;
    startbuf[5] = (uint8_t)net_init_total_players;
    startbuf[6] = (uint8_t)have_clients;
    startbuf[7] = 0; // flags (reserved)
    startbuf[8] = (uint8_t)ClampInt(net_start_skill, 1, 5);
    startbuf[9] = (uint8_t)ClampInt(net_start_episode, 1, 4);
    startbuf[10] = (uint8_t)ClampInt(net_start_map, 1, 32);
    startbuf[11] = 0;

    memcpy(startbuf + 12, &sendaddress[1], sizeof(IPaddress) * have_clients);
    memcpy(startbuf + 12 + sizeof(IPaddress) * have_clients, net_roster, MAXPLAYERS * 16);
    memcpy(startbuf + 12 + sizeof(IPaddress) * have_clients + (MAXPLAYERS * 16), content_hash, 16);

    int total_len = 12 + (int)(sizeof(IPaddress) * have_clients) + (MAXPLAYERS * 16) + 16;
    for (int i = 1; i <= have_clients; ++i)
    {
        memcpy(sendpacket->data, startbuf, (size_t)total_len);
        sendpacket->len = total_len;
        sendpacket->address = sendaddress[i];
        SDLNet_UDP_Send(udpsocket, -1, sendpacket);
    }
}

static void Client_SendJoinReq(void)
{
    // JOIN_REQ: [magic][type][pad][content_hash16][playername16]
    uint8_t joinreq[38] = {0};
    SDLNet_Write32(JOIN_MAGIC, joinreq);
    joinreq[4] = TYPE_JOIN_REQ;
    memcpy(joinreq + 6, content_hash, 16);
    memset(joinreq + 22, 0, 16);
    strncpy((char *)(joinreq + 22), playername, 15);

    memcpy(sendpacket->data, joinreq, sizeof(joinreq));
    sendpacket->len = (int)sizeof(joinreq);
    sendpacket->address = connect_target;
    SDLNet_UDP_Send(udpsocket, -1, sendpacket);
}

static void Client_ProcessLobbyPackets(void)
{
    Uint32 magic;
    Uint8 type;

    if (recvpacket->len < 5)
        return;

    magic = SDLNet_Read32(recvpacket->data);
    type = recvpacket->data[4];

    if (magic == JOIN_MAGIC && type == TYPE_JOIN_ACK && recvpacket->len >= 24)
    {
        assigned_node = recvpacket->data[5];
        net_init_total_players = recvpacket->data[6];
        memcpy(session_key, recvpacket->data + 8, 16);
        net_total_players = net_init_total_players;
        // Client doesn't know connected count yet.
        if (net_connected_players <= 0)
            net_connected_players = 1;
        return;
    }

    if (magic == START_MAGIC && type == TYPE_START && recvpacket->len >= 8)
    {
        int total;
        int client_count;
        int need;

        if (recvpacket->len < 12)
            return;

        total = recvpacket->data[5];
        client_count = recvpacket->data[6];
        need = 12 + client_count * (int)sizeof(IPaddress) + (MAXPLAYERS * 16) + 16;
        if (recvpacket->len < need)
            return;

        if (assigned_node < 1)
        {
            // START without an assigned node isn't usable (no way to know our player index).
            net_init_state = NET_STATUS_ERROR;
            return;
        }

        net_start_skill = recvpacket->data[8];
        net_start_episode = recvpacket->data[9];
        net_start_map = recvpacket->data[10];

        net_init_total_players = total;
        net_total_players = total;
        net_connected_players = total;

        sendaddress[0] = connect_target;
        memcpy(&sendaddress[1], recvpacket->data + 12, sizeof(IPaddress) * client_count);
        memcpy(net_roster, recvpacket->data + 12 + sizeof(IPaddress) * client_count, MAXPLAYERS * 16);

        if (memcmp(content_hash,
                   recvpacket->data + 12 + sizeof(IPaddress) * client_count + (MAXPLAYERS * 16),
                   16) != 0)
        {
            net_init_state = NET_STATUS_ERROR;
            return;
        }

        net_init_state = NET_STATUS_READY;
        return;
    }

    if (magic == JOIN_MAGIC && type == TYPE_JOIN_NACK && recvpacket->len >= 6)
    {
        net_init_reject_reason = recvpacket->data[5];
        if (net_init_reject_reason == 1)
            net_init_content_mismatch = 1;
        net_init_state = NET_STATUS_REJECTED;
        return;
    }
}

int I_PollNetworkInit(void)
{
    if (net_init_state == NET_STATUS_INIT || net_init_state == NET_STATUS_READY ||
        net_init_state == NET_STATUS_REJECTED || net_init_state == NET_STATUS_TIMEOUT ||
        net_init_state == NET_STATUS_ERROR)
        return net_init_state;

    // Timeout budget: 30s.
    if ((SDL_GetTicks() - net_init_start_ticks) > 30000)
    {
        net_init_state = NET_STATUS_TIMEOUT;
        return net_init_state;
    }

    if (!udpsocket || !recvpacket || !sendpacket)
    {
        net_init_state = NET_STATUS_ERROR;
        return net_init_state;
    }

    if (net_init_is_host)
    {
        PollDiscoverySocket();

        while (SDLNet_UDP_Recv(udpsocket, recvpacket) > 0)
            Host_ProcessJoinReq();
    }
    else
    {
        // Retry JOIN_REQ every second until we get START or reject.
        if (net_init_last_send_ticks == 0 || (SDL_GetTicks() - net_init_last_send_ticks) > 1000)
        {
            Client_SendJoinReq();
            net_init_last_send_ticks = SDL_GetTicks();
        }

        while (SDLNet_UDP_Recv(udpsocket, recvpacket) > 0)
            Client_ProcessLobbyPackets();
    }

    return net_init_state;
}

int I_LobbyStartGame(void)
{
    if (!net_init_is_host)
        return 0;
    if (net_init_state != NET_STATUS_WAITING)
        return 0;
    if (!udpsocket || !sendpacket)
        return 0;

    // Host can start once at least 2 players are present.
    if (net_connected_players < 2)
        return 0;

    // Start with currently connected players (host + joined clients).
    int start_total = net_connected_players;
    int have_clients = start_total - 1;

    net_init_total_players = start_total;
    net_total_players = start_total;
    net_init_have_clients = have_clients;

    Host_SendStart();

    net_init_state = NET_STATUS_READY;
    return 1;
}

void I_FinishNetworkInit(void)
{
    if (net_init_state != NET_STATUS_READY)
        return;

    netsend = NetSend;
    netget = NetListenThunk;
    netgame = true;

    doomcom->id = DOOMCOM_ID;
    doomcom->numnodes = net_total_players;
    doomcom->numplayers = net_total_players;
    doomcom->consoleplayer = (assigned_node >= 0) ? assigned_node : 0;
    netbuffer = &doomcom->data;

    // Lobby init complete; discovery is not needed once gameplay starts.
    CloseDiscoverySocket();
}

void I_InitNetwork (void)
{
    int                 i;
    int                 p;
    Uint32              start_ticks;

    doomcom = malloc (sizeof (*doomcom) );
    memset (doomcom, 0, sizeof(*doomcom) );

    if (SDLNet_Init() == -1)
        I_Error("SDLNet_Init failed: %s", SDLNet_GetError());
    sdl_net_inited = true;

    InitNetworkSimulation();

    i = M_CheckParm ("-dup");
    if (i && i< myargc-1)
    {
        doomcom->ticdup = myargv[i+1][0]-'0';
        if (doomcom->ticdup < 1)
            doomcom->ticdup = 1;
        if (doomcom->ticdup > 9)
            doomcom->ticdup = 9;
    }
    else
        doomcom-> ticdup = 1;

    doomcom-> extratics = M_CheckParm ("-extratic") ? 1 : 0;

    p = M_CheckParm ("-port");
    if (p && p<myargc-1)
    {
        doomport = ParsePort(myargv[p+1], doomport);
        printf ("using alternate port %u\n", doomport);
    }

    // parse host/connect
    p = M_CheckParm("-host");
    if (p && p < myargc-1)
    {
        // DOOM netplay is limited to MAXPLAYERS (typically 4).
        desired_players = ParsePositiveIntArg(myargv[p+1], MAXPLAYERS, 2);
        use_host_flow = true;
    }
    p = M_CheckParm("-connect");
    if (p && p < myargc-1)
    {
        if (!ResolveAddressSpec(myargv[p+1], doomport, &connect_target))
            I_Error("Couldn't resolve %s", myargv[p+1]);
        use_client_flow = true;
    }

    i = M_CheckParm ("-net");
    if (!i && !use_host_flow && !use_client_flow)
    {
        netgame = false;
        doomcom->id = DOOMCOM_ID;
        doomcom->numplayers = doomcom->numnodes = 1;
        doomcom->deathmatch = false;
        doomcom->consoleplayer = 0;
        netbuffer = &doomcom->data;
        return;
    }

    netsend = NetSend;
    netget = NetListenThunk;
    netgame = true;

    // host/client secure lobby
    if (use_host_flow || use_client_flow)
    {
        // Phase 3: keep CLI behavior but drive it via the async state machine.
        I_InitNetworkAsync(use_host_flow ? 1 : 0, desired_players);

        start_ticks = SDL_GetTicks();
        while (1)
        {
            int st = I_PollNetworkInit();
            // CLI compatibility: when running with -host N, auto-start once N players are present.
            if (use_host_flow && st == NET_STATUS_WAITING && I_GetLobbyPlayerCount() >= desired_players)
                I_LobbyStartGame();
            if (st == NET_STATUS_READY)
                break;
            if (st == NET_STATUS_REJECTED)
                I_Error("Join rejected (reason %d)", net_init_reject_reason);
            if (st == NET_STATUS_TIMEOUT)
                I_Error("Network init timed out");
            if (st == NET_STATUS_ERROR)
                I_Error("Network init failed");

            if ((SDL_GetTicks() - start_ticks) > 60000)
                I_Error("Network init stalled");
            SDL_Delay(10);
        }

        I_FinishNetworkInit();
        return;
    }

    // legacy -net path (manual addressing)
    doomcom->consoleplayer = myargv[i+1][0]-'1';
    doomcom->numnodes = 1;
    i++;
    while (++i < myargc && myargv[i][0] != '-')
    {
        if (!ResolveAddressSpec(myargv[i], doomport, &sendaddress[doomcom->numnodes]))
            I_Error("Couldn't resolve %s", myargv[i]);
        doomcom->numnodes++;
    }
    doomcom->id = DOOMCOM_ID;
    doomcom->numplayers = doomcom->numnodes;
    udpsocket = SDLNet_UDP_Open(doomport);
    if (!udpsocket)
        I_Error("BindToPort: %s", SDLNet_GetError());
    EnsurePacketCapacity(sizeof(doomdata_t));
    netbuffer = &doomcom->data;
}

void I_ShutdownNetwork(void)
{
    if (recvpacket)
    {
        SDLNet_FreePacket(recvpacket);
        recvpacket = NULL;
    }

    if (sendpacket)
    {
        SDLNet_FreePacket(sendpacket);
        sendpacket = NULL;
    }

    if (udpsocket)
    {
        SDLNet_UDP_Close(udpsocket);
        udpsocket = NULL;
    }

    SDLNet_Quit();
    sdl_net_inited = false;

    if (doomcom)
    {
        free(doomcom);
        doomcom = NULL;
    }
    netbuffer = NULL;
}

void I_NetCmd (void)
{
    if (doomcom->command == CMD_SEND)
    {
        netsend ();
    }
    else if (doomcom->command == CMD_GET)
    {
        netget ();
    }
    else
        I_Error ("Bad net cmd: %i\n",doomcom->command);
}

int I_RunNetworkHarness(int argc, char **argv)
{
    // not used in this SDL2_net build
    return 0;
}
