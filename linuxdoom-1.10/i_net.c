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

typedef Uint16 netorder_16;
typedef Uint32 netorder_32;

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

void I_SetVanillaOnly(int on)
{
    net_vanilla_only = on ? true : false;
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
    probe[4] = 1; // PROBE
    pkt->data = probe;
    pkt->len = 5;
    pkt->address = bcast;
    SDLNet_UDP_Send(dsock, -1, pkt);

    Uint32 start = SDL_GetTicks();
    while ((SDL_GetTicks() - start) < 500 && discovered_count < (int)(sizeof(discovered)/sizeof(discovered[0])))
    {
        if (SDLNet_UDP_Recv(dsock, pkt) > 0)
        {
            if (pkt->len >= 5 && SDLNet_Read32(pkt->data) == DISCOVERY_MAGIC && pkt->data[4] == 2)
            {
                discovered[discovered_count++] = pkt->address;
                if (out && discovered_count <= max)
                    out[discovered_count-1] = pkt->address;
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

void I_InitNetwork (void)
{
    int                 i;
    int                 p;
    Uint32              start_ticks;

    doomcom = malloc (sizeof (*doomcom) );
    memset (doomcom, 0, sizeof(*doomcom) );

    if (SDLNet_Init() == -1)
        I_Error("SDLNet_Init failed: %s", SDLNet_GetError());

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
        desired_players = ParsePositiveIntArg(myargv[p+1], MAXNETNODES, 2);
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
        udpsocket = SDLNet_UDP_Open(doomport);
        if (!udpsocket)
            I_Error("BindToPort: %s", SDLNet_GetError());

        EnsurePacketCapacity(sizeof(doomdata_t));
        Net_GenerateSessionKey();
        ComputeContentHash(content_hash);

        const Uint8 TYPE_JOIN_REQ = 1;
        const Uint8 TYPE_JOIN_ACK = 2;
        const Uint8 TYPE_START = 3;
        const Uint8 TYPE_JOIN_NACK = 4;
        const Uint8 TYPE_DISC_PROBE = 1;
        const Uint8 TYPE_DISC_ANNOUNCE = 2;

        if (use_host_flow)
        {
            int have_clients = 0;
            start_ticks = SDL_GetTicks();
            Uint32 wait_ms = 10000;

            // one-shot announce
            UDPsocket dsock = SDLNet_UDP_Open(0);
            UDPpacket *dpkt = SDLNet_AllocPacket(32);
            if (dsock && dpkt)
            {
                IPaddress b;
                b.host = 0xFFFFFFFF;
                b.port = SDL_SwapBE16(DISCOVERY_PORT);
                SDLNet_Write32(DISCOVERY_MAGIC, dpkt->data);
                dpkt->data[4] = TYPE_DISC_ANNOUNCE;
                dpkt->len = 5;
                dpkt->address = b;
                SDLNet_UDP_Send(dsock, -1, dpkt);
            }

            while ((SDL_GetTicks() - start_ticks) < wait_ms && have_clients < desired_players - 1)
            {
                if (SDLNet_UDP_Recv(udpsocket, recvpacket) > 0)
                {
                    if (recvpacket->len >= 22 &&
                        SDLNet_Read32(recvpacket->data) == JOIN_MAGIC &&
                        recvpacket->data[4] == TYPE_JOIN_REQ)
                    {
                        if (net_vanilla_only && memcmp(recvpacket->data + 6, content_hash, 16) != 0)
                        {
                            uint8_t nack[8] = {0};
                            SDLNet_Write32(JOIN_MAGIC, nack);
                            nack[4] = TYPE_JOIN_NACK;
                            nack[5] = 1;
                            sendpacket->data = nack;
                            sendpacket->len = sizeof(nack);
                            sendpacket->address = recvpacket->address;
                            SDLNet_UDP_Send(udpsocket, -1, sendpacket);
                            continue;
                        }
                        boolean known = false;
                        for (i = 1; i <= have_clients; ++i)
                            if (NetAddressesEqual(&recvpacket->address, &sendaddress[i])) known = true;
                        if (!known && have_clients < desired_players - 1)
                        {
                            have_clients++;
                            sendaddress[have_clients] = recvpacket->address;
                            uint8_t ack[24] = {0};
                            SDLNet_Write32(JOIN_MAGIC, ack);
                            ack[4] = TYPE_JOIN_ACK;
                            ack[5] = (uint8_t)have_clients;
                            ack[6] = (uint8_t)desired_players;
                            ack[7] = net_vanilla_only ? 1 : 0;
                            memcpy(ack + 8, session_key, 16);
                            sendpacket->data = ack;
                            sendpacket->len = sizeof(ack);
                            sendpacket->address = recvpacket->address;
                            SDLNet_UDP_Send(udpsocket, -1, sendpacket);
                        }
                    }
                }
                SDL_Delay(10);
            }
            if (have_clients < desired_players - 1)
                I_Error("Not enough players joined");

            uint8_t startbuf[8 + sizeof(IPaddress) * (MAXNETNODES - 1) + 16] = {0};
            SDLNet_Write32(START_MAGIC, startbuf);
            startbuf[4] = TYPE_START;
            startbuf[5] = (uint8_t)desired_players;
            startbuf[6] = (uint8_t)have_clients;
            memcpy(startbuf + 8, &sendaddress[1], sizeof(IPaddress) * have_clients);
            memcpy(startbuf + 8 + sizeof(IPaddress) * have_clients, content_hash, 16);
            for (i = 1; i <= have_clients; ++i)
            {
                sendpacket->data = startbuf;
                sendpacket->len = 8 + sizeof(IPaddress) * have_clients + 16;
                sendpacket->address = sendaddress[i];
                SDLNet_UDP_Send(udpsocket, -1, sendpacket);
            }

            doomcom->consoleplayer = 0;
            doomcom->numnodes = desired_players;
            doomcom->numplayers = desired_players;
        }
        else // client
        {
            uint8_t joinreq[22] = {0};
            SDLNet_Write32(JOIN_MAGIC, joinreq);
            joinreq[4] = TYPE_JOIN_REQ;
            memcpy(joinreq + 6, content_hash, 16);
            sendpacket->data = joinreq;
            sendpacket->len = sizeof(joinreq);
            sendpacket->address = connect_target;
            SDLNet_UDP_Send(udpsocket, -1, sendpacket);

            boolean got_ack = false, got_start = false;
            int total = 0, client_count = 0;
            start_ticks = SDL_GetTicks();
            Uint32 wait_ms = 10000;
            while ((SDL_GetTicks() - start_ticks) < wait_ms && !got_start)
            {
                if (SDLNet_UDP_Recv(udpsocket, recvpacket) > 0)
                {
                    Uint32 magic = SDLNet_Read32(recvpacket->data);
                    Uint8 type = recvpacket->data[4];
                    if (magic == JOIN_MAGIC && type == TYPE_JOIN_ACK && recvpacket->len >= 24)
                    {
                        assigned_node = recvpacket->data[5];
                        total = recvpacket->data[6];
                        memcpy(session_key, recvpacket->data + 8, 16);
                        got_ack = true;
                    }
                    else if (magic == START_MAGIC && type == TYPE_START && recvpacket->len >= 8)
                    {
                        total = recvpacket->data[5];
                        client_count = recvpacket->data[6];
                        int need = 8 + client_count * (int)sizeof(IPaddress) + 16;
                        if (recvpacket->len >= need)
                        {
                            sendaddress[0] = connect_target;
                            memcpy(&sendaddress[1], recvpacket->data + 8, sizeof(IPaddress) * client_count);
                            if (memcmp(content_hash, recvpacket->data + 8 + sizeof(IPaddress) * client_count, 16) != 0)
                                I_Error("Content hash mismatch with host");
                            got_start = true;
                        }
                    }
                    else if (magic == JOIN_MAGIC && type == TYPE_JOIN_NACK)
                    {
                        I_Error("Join rejected (reason %d)", recvpacket->data[5]);
                    }
                }
                SDL_Delay(10);
            }
            if (!got_ack || !got_start || assigned_node < 1)
                I_Error("Failed to join host");
            doomcom->consoleplayer = assigned_node;
            doomcom->numnodes = total;
            doomcom->numplayers = total;
        }

        doomcom->id = DOOMCOM_ID;
        netbuffer = &doomcom->data;
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
