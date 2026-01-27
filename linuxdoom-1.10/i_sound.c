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
//	System interface for sound - SDL2 audio backend.
//
//-----------------------------------------------------------------------------

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <pthread.h>
#include <unistd.h>

#include <SDL2/SDL.h>

#ifdef HAVE_ADLMIDI
#include <adlmidi.h>
#endif

#ifdef HAVE_OPNMIDI
#include <opnmidi.h>
#endif

#ifdef HAVE_ALSA_SEQ
#include <alsa/asoundlib.h>
#endif

#include "z_zone.h"
#include "i_system.h"
#include "i_sound.h"
#include "w_wad.h"
#include "sounds.h"
#include "doomstat.h"

// Audio parameters
// ===================================================================
// Audio Configuration
// ===================================================================
// The audio system uses the following fixed format via SDL2:
//   - Sample Rate: 11025 Hz (legacy DOOM standard)
//   - Channels: 2 (stereo)
//   - Bit Depth: 16-bit signed integers (AUDIO_S16)
//   - Max Concurrent Sounds: 8 channels
//
// This format is optimized for compatibility with original DOOM sound
// effects and provides acceptable quality on modern systems. The low
// sample rate was chosen in the original to match console hardware and
// reduces CPU overhead for sound mixing.
//
// To change sample rate: modify SAMPLERATE below and rebuild.
// To change channels: modify NUM_CHANNELS and audio_spec.channels in I_InitSound().
// Note: Changing these will require recompiling sound effects in WAD files.
// ===================================================================

#define NUM_CHANNELS		8
#define SAMPLECOUNT		512
#define SAMPLERATE		11025	// Hz (original DOOM legacy standard)
#define SAMPLESIZE		2   	// 16-bit signed integer
#define BUFMUL                  4
#define MIXBUFFERSIZE		(SAMPLECOUNT*BUFMUL)

// The actual lengths of all sound effects.
int lengths[NUMSFX];

// The global mixing buffer.
signed short mixbuffer[MIXBUFFERSIZE];
static int16_t musicbuffer[MIXBUFFERSIZE];

// The channel step amount...
unsigned int channelstep[NUM_CHANNELS];
// ... and a 0.16 bit remainder of last step.
unsigned int channelstepremainder[NUM_CHANNELS];

// The channel data pointers, start and end.
unsigned char *channels[NUM_CHANNELS];
unsigned char *channelsend[NUM_CHANNELS];

// Time/gametic that the channel started playing.
int channelstart[NUM_CHANNELS];

// The sound in channel handles.
int channelhandles[NUM_CHANNELS];

// SFX id of the playing sound effect.
int channelids[NUM_CHANNELS];

// Pitch to stepping lookup.
int steptable[256];

// Volume lookups.
int vol_lookup[128*256];

// Hardware left and right channel volume lookup.
int *channelleftvol_lookup[NUM_CHANNELS];
int *channelrightvol_lookup[NUM_CHANNELS];

// SDL audio device ID
static SDL_AudioDeviceID audio_device = 0;
static SDL_AudioSpec audio_spec;

// Audio callback synchronization
static int sound_need_update = 0;

// Music backend selection:
// 0 = ADLMIDI (OPL3), 1 = OPNMIDI (OPN2), 2 = ALSA sequencer
int music_backend = 0;

static int music_backend_active = -1;
static int music_playing = 0;
static int music_paused = 0;
static int music_looping = 0;
static int music_volume = 15;
static int music_loaded = 0;
static uint8_t *music_midi_data = NULL;
static uint32_t music_midi_length = 0;

#ifdef HAVE_ADLMIDI
static struct ADL_MIDIPlayer *adl_player = NULL;
#endif

#ifdef HAVE_OPNMIDI
static struct OPN2_MIDIPlayer *opn_player = NULL;
#endif

extern int I_MusToMidi(const uint8_t *mus, uint32_t mus_len, uint8_t **out, uint32_t *out_len);
extern void I_MusToMidiFree(uint8_t *data);

#ifdef HAVE_ALSA_SEQ
static snd_seq_t *alsa_seq = NULL;
static int alsa_port = -1;
static pthread_t alsa_thread;
static int alsa_thread_running = 0;
static int alsa_thread_stop = 0;
static uint8_t *alsa_midi_data = NULL;
static uint32_t alsa_midi_length = 0;
static pthread_mutex_t alsa_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t alsa_cond = PTHREAD_COND_INITIALIZER;
#endif

static int ScaleSfxVolume(int volume)
{
    if (volume < 0)
        return 0;
    if (volume > 15)
        return (volume > 127) ? 127 : volume;
    return (volume * 127) / 15;
}

static int MusicBackendAvailable(int backend)
{
    switch (backend)
    {
        case 0:
#ifdef HAVE_ADLMIDI
            return 1;
#else
            return 0;
#endif
        case 1:
#ifdef HAVE_OPNMIDI
            return 1;
#else
            return 0;
#endif
        case 2:
#ifdef HAVE_ALSA_SEQ
            return 1;
#else
            return 0;
#endif
        default:
            return 0;
    }
}

static int MusicBackendSelect(int requested)
{
    int order[3] = { requested, 0, 1 };
    int i;

    if (requested == 0)
    {
        order[1] = 1;
        order[2] = 2;
    }
    else if (requested == 1)
    {
        order[1] = 0;
        order[2] = 2;
    }
    else
    {
        order[0] = 2;
        order[1] = 0;
        order[2] = 1;
    }

    for (i = 0; i < 3; i++)
    {
        if (MusicBackendAvailable(order[i]))
            return order[i];
    }

    return -1;
}

static void LockAudioDevice(void)
{
    if (audio_device != 0)
        SDL_LockAudioDevice(audio_device);
}

static void UnlockAudioDevice(void)
{
    if (audio_device != 0)
        SDL_UnlockAudioDevice(audio_device);
}

//
// This function loads the sound data from the WAD lump,
//  for single sound.
//
void*
getsfx
( char*         sfxname,
  int*          len )
{
    unsigned char*      sfx;
    unsigned char*      paddedsfx;
    int                 i;
    int                 size;
    int                 paddedsize;
    char                name[20];
    int                 sfxlump;

    // Get the sound data from the WAD, allocate lump
    //  in zone memory.
    sprintf(name, "ds%s", sfxname);

    if ( W_CheckNumForName(name) == -1 )
      sfxlump = W_GetNumForName("dspistol");
    else
      sfxlump = W_GetNumForName(name);

    size = W_LumpLength( sfxlump );
    sfx = (unsigned char*)W_CacheLumpNum( sfxlump, PU_STATIC );

    // Pads the sound effect out to the mixing buffer size.
    paddedsize = ((size-8 + (SAMPLECOUNT-1)) / SAMPLECOUNT) * SAMPLECOUNT;

    // Allocate from zone memory.
    paddedsfx = (unsigned char*)Z_Malloc( paddedsize+8, PU_STATIC, 0 );

    // Now copy and pad.
    memcpy(  paddedsfx, sfx, size );
    for (i=size ; i<paddedsize+8 ; i++)
        paddedsfx[i] = 128;

    // Release the cached lump (keep lumpcache consistent).
    Z_ChangeTag(sfx, PU_CACHE);

    // Preserve padded length.
    *len = paddedsize;

    // Return allocated padded data.
    return (void *) (paddedsfx + 8);
}


//
// Increments SFX handle number in limited range.
//
int
addsfx
( int		sfxid,
  int		volume,
  int		step,
  int		seperation )
{
    int		channel = 0;
    int		oldest = gametic;
    int		i;
    int		scaled_volume;

    // Find a free channel
    for (i=0 ; i<NUM_CHANNELS ; i++)
    {
	if (!channels[i])
	    channel = i;

	if (channelstart[i] < oldest)
	{
	    oldest = channelstart[i];
	    channel = i;
	}
    }

    // We found the oldest playing sound,
    //  rather use that than just overwriting a playing sound.
    if (channels[channel])
	channels[channel] = 0;

    // Okay, in the less recent channel,
    //  we will handle the new SFX.
    // Set pointers to raw data.
    channels[channel] = S_sfx[sfxid].data;
    channelsend[channel] = channels[channel] + lengths[sfxid];

    // Should be gametic, I presume.
    channelstart[channel] = gametic;

    // Pointers to left and right scalers.
    scaled_volume = ScaleSfxVolume(volume);
    channelleftvol_lookup[channel] =
	&vol_lookup[( 127 - seperation ) * 256 + scaled_volume * 256];
    channelrightvol_lookup[channel] =
	&vol_lookup[seperation * 256 + scaled_volume * 256];

    // Seperation, that is, orientation/stereo.
    //  range is, effectively, -1 to 1.
    channelstep[channel] = step;
    // Remaining is zero, to begin with.
    channelstepremainder[channel] = 0;
    // Should be gametic, I presume.
    channelhandles[channel] = sfxid;

    channelids[channel] = sfxid;

    return channel;
}


//
// Starts a sound in a particular sound channel.
//
int
I_StartSound
( int		id,
  int		vol,
  int		sep,
  int		pitch,
  int		priority )
{
    unsigned long	stepper;
    int		channel = 0;
    int		oldest = gametic;
    int		i;
    int		rc = 0;

    LockAudioDevice();

    // Relative mode, not pitch number.
    if (pitch == 128)
	stepper = steptable[128];
    else
	stepper = steptable[ (pitch + 128) % 256];

    // Return a handle.
    if ( (channel = addsfx(id, vol, stepper, sep)) != -1 )
    {
	rc = channel;
    }

    UnlockAudioDevice();
    return rc;
}


//
// Stops a sound channel.
//
void I_StopSound(int handle)
{
    int i;
    LockAudioDevice();
    for (i=0 ; i<NUM_CHANNELS ; i++)
    {
	if (channelhandles[i] == handle)
	{
	    channels[i] = 0;
	}
    }
    UnlockAudioDevice();
}


//
// Called by S_*() functions
//  to see if a channel is still playing.
// Returns 0 if no longer playing, 1 if playing.
//
int I_SoundIsPlaying(int handle)
{
    int i;
    int playing = 0;
    LockAudioDevice();
    for (i=0 ; i<NUM_CHANNELS ; i++)
    {
	if (channelhandles[i] == handle && channels[i])
	{
	    playing = 1;
            break;
	}
    }
    UnlockAudioDevice();
    return playing;
}


//
// Updates the volume, separation,
//  and pitch of a sound channel.
//
void
I_UpdateSoundParams
( int		handle,
  int		vol,
  int		sep,
  int		pitch )
{
    int i;
    int	stepper;

    LockAudioDevice();
    for ( i = 0; i < NUM_CHANNELS; i++ )
    {
	if (channelhandles[i] == handle)
	{
	    // Update volume.
	    int scaled_volume = ScaleSfxVolume(vol);
	    channelleftvol_lookup[i] =
		&vol_lookup[( 127 - sep ) * 256 + scaled_volume * 256];
	    channelrightvol_lookup[i] =
		&vol_lookup[sep * 256 + scaled_volume * 256];

	    // Seperation, that is, orientation/stereo,
	    //  range is, effectively, -1 to 1.

	    // Update pitch.
	    if (pitch == 128)
		stepper = steptable[128];
	    else
		stepper = steptable[ (pitch + 128) % 256 ];

	    channelstep[i] = stepper;
	}
    }
    UnlockAudioDevice();
}


//
// Mix current sound data.
//
void I_UpdateSound( void )
{
    register unsigned int sample;
    register int dl;
    register int dr;

    signed short *leftout;
    signed short *rightout;
    signed short *leftend;
    int step;
    int chan;

    leftout = mixbuffer;
    rightout = mixbuffer+1;
    step = 2;

    leftend = mixbuffer + SAMPLECOUNT*step;

    // Mix sounds into the mixing buffer.
    while (leftout != leftend)
    {
	dl = 0;
	dr = 0;

	for ( chan = 0; chan < NUM_CHANNELS; chan++ )
	{
	    if (channels[ chan ])
	    {
		sample = *channels[ chan ];
		dl += channelleftvol_lookup[ chan ][sample];
		dr += channelrightvol_lookup[ chan ][sample];
		channelstepremainder[ chan ] += channelstep[ chan ];
		channels[ chan ] += channelstepremainder[ chan ] >> 16;
		channelstepremainder[ chan ] &= 65536-1;

		if (channels[ chan ] >= channelsend[ chan ])
		    channels[ chan ] = 0;
	    }
	}

	if (dl > 0x7fff)
	    *leftout = 0x7fff;
	else if (dl < -0x8000)
	    *leftout = -0x8000;
	else
	    *leftout = dl;

	if (dr > 0x7fff)
	    *rightout = 0x7fff;
	else if (dr < -0x8000)
	    *rightout = -0x8000;
	else
	    *rightout = dr;

	leftout += step;
	rightout += step;
    }
}

static void I_UpdateMusic(int len)
{
    int frames;
    int samples;
    int i;
    int volume;
    int generated = 0;

    if (!music_playing || music_paused || !music_loaded)
        return;

    if (music_backend_active == 2)
        return;

    frames = len / (int)(sizeof(int16_t) * 2);
    if (frames <= 0)
        return;

    samples = frames * 2;
    if (samples > MIXBUFFERSIZE)
        samples = MIXBUFFERSIZE;

    memset(musicbuffer, 0, sizeof(int16_t) * samples);

#ifdef HAVE_ADLMIDI
    if (music_backend_active == 0 && adl_player)
        generated = adl_play(adl_player, samples, musicbuffer);
#endif

#ifdef HAVE_OPNMIDI
    if (music_backend_active == 1 && opn_player)
        generated = opn2_play(opn_player, samples, musicbuffer);
#endif

    if (generated > 0 && generated < samples)
    {
        memset(musicbuffer + generated, 0, sizeof(int16_t) * (samples - generated));
    }

    volume = music_volume;
    if (volume <= 0)
        return;
    if (volume > 127)
        volume = 127;

    for (i = 0; i < samples; i++)
    {
        int mixed = mixbuffer[i] + (musicbuffer[i] * volume) / 127;
        if (mixed > 32767)
            mixed = 32767;
        else if (mixed < -32768)
            mixed = -32768;
        mixbuffer[i] = (signed short)mixed;
    }
}


//
// SDL audio callback function.
// Called by SDL when it needs more audio data.
//
static void audio_callback(void *userdata, Uint8 *stream, int len)
{
    static int warned_len = 0;
    int max_len = MIXBUFFERSIZE * (int)sizeof(int16_t);

    if (!warned_len && len > max_len)
    {
        fprintf(stderr, "I_UpdateSound: callback len %d exceeds mix buffer %d\n", len, max_len);
        warned_len = 1;
    }

    // Update the mixing buffer
    I_UpdateSound();
    I_UpdateMusic(len);

    // Copy the mixing buffer to the audio stream
    // mixbuffer is signed 16-bit, same as what SDL expects
    memcpy(stream, (Uint8 *)mixbuffer, len);
}


//
// Shutdown sound hardware
//
void I_ShutdownSound(void)
{
    if (audio_device != 0)
    {
        SDL_CloseAudioDevice(audio_device);
        audio_device = 0;
    }

    SDL_QuitSubSystem(SDL_INIT_AUDIO);
}


//
// Initialize sound hardware
//
void I_InitSound(void)
{
    int i;
    int j;
    SDL_AudioSpec obtained;

    if (SDL_InitSubSystem(SDL_INIT_AUDIO) < 0)
    {
        I_Error("Could not initialize SDL audio: %s", SDL_GetError());
    }

    // Initialize sound effect lookup table
    for (i=0 ; i<NUMSFX ; i++)
        S_sfx[i].data = NULL;

    // Load all sound effects
    fprintf( stderr, "I_InitSound: ");

    for (i=1 ; i<NUMSFX ; i++)
    {
        // Alias? Example is the chaingun sound linked to pistol.
        if (!S_sfx[i].link)
        {
            // Load data from WAD file.
            S_sfx[i].data = getsfx(S_sfx[i].name, &lengths[i]);
        }
        else
        {
            // Previously loaded already?
            S_sfx[i].data = S_sfx[i].link->data;
            lengths[i] = lengths[(S_sfx[i].link - S_sfx) / sizeof(sfxinfo_t)];
        }
    }

    fprintf( stderr, " done\n");

    // Finished initialization.
    fprintf(stderr, "I_InitSound: using SDL2 mixer controls\n");
    fprintf(stderr, "I_InitSound: sfx volume %d (0-15)\n", snd_SfxVolume);

    // Set up audio device
    SDL_zero(audio_spec);
    audio_spec.freq = SAMPLERATE;
    audio_spec.format = AUDIO_S16;
    audio_spec.channels = 2;
    audio_spec.samples = SAMPLECOUNT;
    audio_spec.callback = audio_callback;
    audio_spec.userdata = NULL;

    SDL_zero(obtained);
    audio_device = SDL_OpenAudioDevice(NULL, 0, &audio_spec, &obtained, 0);
    if (audio_device == 0)
    {
        I_Error("Could not open audio device: %s", SDL_GetError());
    }

    fprintf(stderr, "I_InitSound: desired %d Hz, %d ch, samples %d, fmt 0x%x\n",
            audio_spec.freq, audio_spec.channels, audio_spec.samples, (unsigned)audio_spec.format);
    fprintf(stderr, "I_InitSound: obtained %d Hz, %d ch, samples %d, fmt 0x%x\n",
            obtained.freq, obtained.channels, obtained.samples, (unsigned)obtained.format);

    // Start playback
    SDL_PauseAudioDevice(audio_device, 0);

    fprintf(stderr, "I_InitSound: sound initialization complete\n");

    // Initialize volume lookup table
    // with gambits
    for (i=0 ; i<128 ; i++)
	for (j=0 ; j<256 ; j++)
	    vol_lookup[i*256+j] = ((j - 128) * i * 256) / 127;

    // Keeps compilers from complaining about uninitialized use.
    I_SetChannels();

    return;
}


//
// SDL2 handles audio submission in the callback,
// so this is a no-op.
//
void I_SubmitSound(void)
{
}


//
// Initialize all channels
//
void I_SetChannels()
{
    int i;

    LockAudioDevice();
    for (i=0; i<NUM_CHANNELS; i++)
    {
	channels[i] = 0;
	channelhandles[i] = -1;
	channelstep[i] = 0;
	channelstepremainder[i] = 0;
    }
    UnlockAudioDevice();
}


//
// Get raw data lump index for sound descriptor.
//
int I_GetSfxLumpNum(sfxinfo_t* sfxinfo)
{
    char namebuf[9];
    sprintf(namebuf, "ds%s", sfxinfo->name);
    return W_GetNumForName(namebuf);
}


#ifdef HAVE_ALSA_SEQ
static uint16_t ReadBE16(const uint8_t *data)
{
    return (uint16_t)((data[0] << 8) | data[1]);
}

static uint32_t ReadBE32(const uint8_t *data)
{
    return ((uint32_t)data[0] << 24) |
           ((uint32_t)data[1] << 16) |
           ((uint32_t)data[2] << 8) |
           ((uint32_t)data[3]);
}

static int ReadVarLen(const uint8_t *data, uint32_t len, uint32_t *pos, uint32_t *value)
{
    uint32_t result = 0;
    uint8_t byte;
    int i;

    for (i = 0; i < 4; i++)
    {
        if (*pos >= len)
            return 0;
        byte = data[(*pos)++];
        result = (result << 7) | (byte & 0x7f);
        if (!(byte & 0x80))
            break;
    }

    *value = result;
    return 1;
}

static int AlsaEnsureOpen(void)
{
    if (alsa_seq)
        return 1;

    if (snd_seq_open(&alsa_seq, "default", SND_SEQ_OPEN_OUTPUT, 0) < 0)
        return 0;

    snd_seq_set_client_name(alsa_seq, "DOOM Music");
    alsa_port = snd_seq_create_simple_port(
        alsa_seq,
        "Music",
        SND_SEQ_PORT_CAP_READ | SND_SEQ_PORT_CAP_SUBS_READ,
        SND_SEQ_PORT_TYPE_APPLICATION);

    if (alsa_port < 0)
    {
        snd_seq_close(alsa_seq);
        alsa_seq = NULL;
        return 0;
    }

    return 1;
}

static void AlsaClose(void)
{
    if (alsa_seq)
    {
        snd_seq_close(alsa_seq);
        alsa_seq = NULL;
        alsa_port = -1;
    }
}

static void AlsaSignal(void)
{
    pthread_mutex_lock(&alsa_mutex);
    pthread_cond_broadcast(&alsa_cond);
    pthread_mutex_unlock(&alsa_mutex);
}

static int AlsaSleepMicros(uint64_t micros)
{
    while (micros > 0)
    {
        pthread_mutex_lock(&alsa_mutex);
        while (!alsa_thread_stop && music_paused)
            pthread_cond_wait(&alsa_cond, &alsa_mutex);
        if (alsa_thread_stop || !music_playing)
        {
            pthread_mutex_unlock(&alsa_mutex);
            return 0;
        }
        pthread_mutex_unlock(&alsa_mutex);

        uint64_t chunk = (micros > 10000) ? 10000 : micros;
        usleep((useconds_t)chunk);
        micros -= chunk;
    }

    return 1;
}

static void AlsaSendEvent(uint8_t status, uint8_t data1, uint8_t data2, int have_data2)
{
    int type = status & 0xF0;
    int channel = status & 0x0F;
    snd_seq_event_t ev;

    if (!alsa_seq)
        return;

    snd_seq_ev_clear(&ev);
    snd_seq_ev_set_source(&ev, alsa_port);
    snd_seq_ev_set_subs(&ev);
    snd_seq_ev_set_direct(&ev);

    switch (type)
    {
        case 0x80:
            snd_seq_ev_set_noteoff(&ev, channel, data1, data2);
            break;
        case 0x90:
            if (data2 == 0)
                snd_seq_ev_set_noteoff(&ev, channel, data1, 0);
            else
                snd_seq_ev_set_noteon(&ev, channel, data1, data2);
            break;
        case 0xA0:
            snd_seq_ev_set_keypress(&ev, channel, data1, data2);
            break;
        case 0xB0:
            snd_seq_ev_set_controller(&ev, channel, data1, data2);
            break;
        case 0xC0:
            snd_seq_ev_set_pgmchange(&ev, channel, data1);
            break;
        case 0xD0:
            snd_seq_ev_set_chanpress(&ev, channel, data1);
            break;
        case 0xE0:
        {
            int value = ((data2 & 0x7f) << 7) | (data1 & 0x7f);
            value -= 8192;
            snd_seq_ev_set_pitchbend(&ev, channel, value);
            break;
        }
        default:
            return;
    }

    snd_seq_event_output_direct(alsa_seq, &ev);
}

static int AlsaPlayMidi(const uint8_t *data, uint32_t len, int looping)
{
    uint32_t pos = 0;
    uint32_t tempo_us = 500000;
    uint16_t division = 96;
    uint8_t running_status = 0;

    if (len < 14 || memcmp(data, "MThd", 4) != 0)
        return 0;

    pos += 4;
    if (ReadBE32(data + pos) < 6)
        return 0;
    pos += 4;

    if (pos + 6 > len)
        return 0;

    pos += 2; // format
    pos += 2; // tracks
    division = ReadBE16(data + pos);
    pos += 2;

    if (pos + 8 > len || memcmp(data + pos, "MTrk", 4) != 0)
        return 0;

    pos += 4;
    uint32_t track_len = ReadBE32(data + pos);
    pos += 4;

    uint32_t track_end = pos + track_len;
    if (track_end > len)
        track_end = len;

    while (pos < track_end)
    {
        uint32_t delta_ticks = 0;
        uint64_t delta_us;
        uint8_t status;
        uint8_t data1 = 0;
        uint8_t data2 = 0;
        uint32_t meta_len = 0;

        if (!ReadVarLen(data, track_end, &pos, &delta_ticks))
            break;

        delta_us = ((uint64_t)delta_ticks * tempo_us) / division;
        if (!AlsaSleepMicros(delta_us))
            return 0;

        if (pos >= track_end)
            break;

        status = data[pos++];
        if (status < 0x80)
        {
            if (!running_status)
                return 0;
            data1 = status;
            status = running_status;
        }
        else
        {
            running_status = status;
        }

        if (status == 0xFF)
        {
            if (pos >= track_end)
                break;
            uint8_t meta_type = data[pos++];
            if (!ReadVarLen(data, track_end, &pos, &meta_len))
                break;
            if (pos + meta_len > track_end)
                break;
            if (meta_type == 0x2F)
                break;
            if (meta_type == 0x51 && meta_len == 3)
            {
                tempo_us = ((uint32_t)data[pos] << 16) |
                           ((uint32_t)data[pos + 1] << 8) |
                           ((uint32_t)data[pos + 2]);
            }
            pos += meta_len;
            continue;
        }

        if (status == 0xF0 || status == 0xF7)
        {
            if (!ReadVarLen(data, track_end, &pos, &meta_len))
                break;
            if (pos + meta_len > track_end)
                break;
            pos += meta_len;
            continue;
        }

        switch (status & 0xF0)
        {
            case 0xC0:
            case 0xD0:
                if (data1 == 0 && status >= 0x80)
                    data1 = data[pos++];
                if (pos > track_end)
                    return 0;
                AlsaSendEvent(status, data1, 0, 0);
                break;
            default:
                if (data1 == 0 && status >= 0x80)
                    data1 = data[pos++];
                if (pos >= track_end)
                    return 0;
                data2 = data[pos++];
                if (pos > track_end)
                    return 0;
                AlsaSendEvent(status, data1, data2, 1);
                break;
        }
    }

    if (looping && music_playing && !alsa_thread_stop)
        return 1;

    return 0;
}

static void *AlsaThreadMain(void *arg)
{
    (void)arg;
    while (1)
    {
        pthread_mutex_lock(&alsa_mutex);
        while (!alsa_thread_stop &&
               (!music_playing || music_paused || !alsa_midi_data || alsa_midi_length == 0))
        {
            pthread_cond_wait(&alsa_cond, &alsa_mutex);
        }
        if (alsa_thread_stop)
        {
            pthread_mutex_unlock(&alsa_mutex);
            break;
        }

        uint8_t *data = alsa_midi_data;
        uint32_t len = alsa_midi_length;
        int looping = music_looping;
        pthread_mutex_unlock(&alsa_mutex);

        if (!AlsaEnsureOpen())
        {
            usleep(100000);
            continue;
        }

        if (!AlsaPlayMidi(data, len, looping))
        {
            pthread_mutex_lock(&alsa_mutex);
            if (!music_looping)
                music_playing = 0;
            pthread_mutex_unlock(&alsa_mutex);
        }
    }

    return NULL;
}

static void AlsaStartThread(void)
{
    if (!alsa_thread_running)
    {
        alsa_thread_stop = 0;
        if (pthread_create(&alsa_thread, NULL, AlsaThreadMain, NULL) == 0)
            alsa_thread_running = 1;
    }
}

static void AlsaStopThread(void)
{
    if (alsa_thread_running)
    {
        pthread_mutex_lock(&alsa_mutex);
        alsa_thread_stop = 1;
        pthread_cond_broadcast(&alsa_cond);
        pthread_mutex_unlock(&alsa_mutex);
        pthread_join(alsa_thread, NULL);
        alsa_thread_running = 0;
        alsa_thread_stop = 0;
    }
}
#endif

//
// Music functions
//
void I_InitMusic(void)
{
    LockAudioDevice();
    music_backend_active = MusicBackendSelect(music_backend);
    music_paused = 0;
    music_playing = 0;
    music_loaded = 0;

    fprintf(stderr, "I_InitMusic: requested backend %d\n", music_backend);

#ifdef HAVE_ADLMIDI
    if (music_backend_active == 0)
    {
        if (!adl_player)
            adl_player = adl_init(SAMPLERATE);
        if (adl_player)
        {
            adl_setVolumeRangeModel(adl_player, ADLMIDI_VolumeModel_DMX);
            adl_setNumChips(adl_player, 2);
            fprintf(stderr, "I_InitMusic: ADLMIDI backend active\n");
            UnlockAudioDevice();
            return;
        }
    }
#endif

#ifdef HAVE_OPNMIDI
    if (music_backend_active == 1)
    {
        if (!opn_player)
            opn_player = opn2_init(SAMPLERATE);
        if (opn_player)
        {
            opn2_setVolumeRangeModel(opn_player, OPNMIDI_VolumeModel_DMX);
            opn2_setNumChips(opn_player, 2);
            fprintf(stderr, "I_InitMusic: OPNMIDI backend active\n");
            UnlockAudioDevice();
            return;
        }
    }
#endif

#ifdef HAVE_ALSA_SEQ
    if (music_backend_active == 2)
    {
        AlsaStartThread();
        fprintf(stderr, "I_InitMusic: ALSA sequencer backend active\n");
        UnlockAudioDevice();
        return;
    }
#endif

    fprintf(stderr, "I_InitMusic: no music backend available\n");
    UnlockAudioDevice();
}

void I_ShutdownMusic(void)
{
    LockAudioDevice();
    music_playing = 0;
    music_paused = 0;
    music_loaded = 0;
    if (music_midi_data)
    {
        I_MusToMidiFree(music_midi_data);
        music_midi_data = NULL;
        music_midi_length = 0;
    }

#ifdef HAVE_ALSA_SEQ
    AlsaStopThread();
    AlsaClose();
    if (alsa_midi_data)
    {
        I_MusToMidiFree(alsa_midi_data);
        alsa_midi_data = NULL;
        alsa_midi_length = 0;
    }
#endif

#ifdef HAVE_ADLMIDI
    if (adl_player)
    {
        adl_close(adl_player);
        adl_player = NULL;
    }
#endif

#ifdef HAVE_OPNMIDI
    if (opn_player)
    {
        opn2_close(opn_player);
        opn_player = NULL;
    }
#endif
    UnlockAudioDevice();
}

void I_PlaySong(int handle, int looping)
{
    LockAudioDevice();
    (void)handle;
    if (!music_loaded || handle == 0)
    {
        music_playing = 0;
        music_paused = 0;
        UnlockAudioDevice();
        return;
    }

    music_looping = looping;
    music_playing = 1;
    music_paused = 0;

#ifdef HAVE_ADLMIDI
    if (music_backend_active == 0 && adl_player)
    {
        adl_setLoopEnabled(adl_player, looping ? 1 : 0);
        adl_setLoopCount(adl_player, looping ? -1 : 0);
        UnlockAudioDevice();
        return;
    }
#endif

#ifdef HAVE_OPNMIDI
    if (music_backend_active == 1 && opn_player)
    {
        opn2_setLoopEnabled(opn_player, looping ? 1 : 0);
        opn2_setLoopCount(opn_player, looping ? -1 : 0);
        UnlockAudioDevice();
        return;
    }
#endif

#ifdef HAVE_ALSA_SEQ
    if (music_backend_active == 2)
        AlsaSignal();
#endif
    UnlockAudioDevice();
}

void I_PauseSong (int handle)
{
    LockAudioDevice();
    (void)handle;
    music_paused = 1;
#ifdef HAVE_ALSA_SEQ
    AlsaSignal();
#endif
    UnlockAudioDevice();
}

void I_ResumeSong (int handle)
{
    LockAudioDevice();
    (void)handle;
    music_paused = 0;
#ifdef HAVE_ALSA_SEQ
    AlsaSignal();
#endif
    UnlockAudioDevice();
}

void I_StopSong(int handle)
{
    LockAudioDevice();
    (void)handle;
    music_playing = 0;
    music_paused = 0;

#ifdef HAVE_ADLMIDI
    if (adl_player)
        adl_reset(adl_player);
#endif

#ifdef HAVE_OPNMIDI
    if (opn_player)
        opn2_reset(opn_player);
#endif

#ifdef HAVE_ALSA_SEQ
    AlsaSignal();
#endif
    UnlockAudioDevice();
}

void I_UnRegisterSong(int handle)
{
    LockAudioDevice();
    (void)handle;
    music_loaded = 0;
    if (music_midi_data)
    {
        I_MusToMidiFree(music_midi_data);
        music_midi_data = NULL;
        music_midi_length = 0;
    }

#ifdef HAVE_ALSA_SEQ
    if (music_backend_active == 2 && alsa_midi_data)
    {
        I_MusToMidiFree(alsa_midi_data);
        alsa_midi_data = NULL;
        alsa_midi_length = 0;
    }
#endif
    UnlockAudioDevice();
}

int I_RegisterSong(void* data, int length)
{
    LockAudioDevice();
    if (!data || length <= 0)
    {
        fprintf(stderr, "I_RegisterSong: invalid data (%p) length %d\n", data, length);
        music_loaded = 0;
        UnlockAudioDevice();
        return 0;
    }

    music_playing = 0;
    music_loaded = 0;
    if (music_midi_data)
    {
        I_MusToMidiFree(music_midi_data);
        music_midi_data = NULL;
        music_midi_length = 0;
    }

    const uint8_t *song_data = (const uint8_t *)data;
    uint32_t song_length = (uint32_t)length;
    int is_mus = (length >= 4 && memcmp(song_data, "MUS\x1a", 4) == 0);

#ifdef HAVE_ADLMIDI
    if (music_backend_active == 0 && adl_player)
    {
        if (is_mus)
        {
            if (!I_MusToMidi(song_data, song_length, &music_midi_data, &music_midi_length))
            {
                fprintf(stderr, "I_RegisterSong: MUS->MIDI convert failed for %d bytes\n", length);
            }
            else
            {
                song_data = music_midi_data;
                song_length = music_midi_length;
                fprintf(stderr, "I_RegisterSong: MUS->MIDI %d -> %u bytes\n", length, music_midi_length);
            }
        }

        if (adl_openData(adl_player, song_data, (unsigned long)song_length) >= 0)
        {
            adl_selectSongNum(adl_player, 0);
            music_loaded = 1;
            fprintf(stderr, "I_RegisterSong: ADLMIDI loaded %u bytes\n", song_length);
            UnlockAudioDevice();
            return 1;
        }
        fprintf(stderr, "I_RegisterSong: ADLMIDI failed to load %u bytes\n", song_length);
    }
#endif

#ifdef HAVE_OPNMIDI
    if (music_backend_active == 1 && opn_player)
    {
        if (is_mus)
        {
            if (!music_midi_data)
            {
                if (!I_MusToMidi(song_data, song_length, &music_midi_data, &music_midi_length))
                {
                    fprintf(stderr, "I_RegisterSong: MUS->MIDI convert failed for %d bytes\n", length);
                }
                else
                {
                    song_data = music_midi_data;
                    song_length = music_midi_length;
                    fprintf(stderr, "I_RegisterSong: MUS->MIDI %d -> %u bytes\n", length, music_midi_length);
                }
            }
            else
            {
                song_data = music_midi_data;
                song_length = music_midi_length;
            }
        }

        if (opn2_openData(opn_player, song_data, (unsigned long)song_length) >= 0)
        {
            opn2_selectSongNum(opn_player, 0);
            music_loaded = 1;
            fprintf(stderr, "I_RegisterSong: OPNMIDI loaded %u bytes\n", song_length);
            UnlockAudioDevice();
            return 1;
        }
        fprintf(stderr, "I_RegisterSong: OPNMIDI failed to load %u bytes\n", song_length);
    }
#endif

#ifdef HAVE_ALSA_SEQ
    if (music_backend_active == 2)
    {
        if (alsa_midi_data)
        {
            I_MusToMidiFree(alsa_midi_data);
            alsa_midi_data = NULL;
            alsa_midi_length = 0;
        }

        if (I_MusToMidi((const uint8_t *)data, (uint32_t)length, &alsa_midi_data, &alsa_midi_length))
        {
            music_loaded = 1;
            fprintf(stderr, "I_RegisterSong: ALSA prepared %u bytes (from %d)\n",
                    alsa_midi_length, length);
            UnlockAudioDevice();
            return 1;
        }
        fprintf(stderr, "I_RegisterSong: ALSA convert failed for %d bytes\n", length);
    }
#endif

    UnlockAudioDevice();
    return 0;
}

int I_QrySongPlaying(int handle)
{
    (void)handle;
    return music_playing && !music_paused;
}

void I_SetMusicVolume(int volume)
{
    LockAudioDevice();
    if (volume < 0)
        volume = 0;
    if (volume <= 15)
        music_volume = (volume * 127) / 15;
    else
        music_volume = (volume > 127) ? 127 : volume;
    UnlockAudioDevice();
}

//-----------------------------------------------------------------------------
//
// $Log:$
//
//-----------------------------------------------------------------------------
