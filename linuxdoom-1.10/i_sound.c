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
#include <string.h>

#include <SDL2/SDL.h>

#include "z_zone.h"
#include "i_system.h"
#include "i_sound.h"
#include "w_wad.h"
#include "sounds.h"
#include "doomstat.h"

// Audio parameters
#define NUM_CHANNELS		8
#define SAMPLECOUNT		512
#define SAMPLERATE		11025	// Hz
#define SAMPLESIZE		2   	// 16bit
#define BUFMUL                  4
#define MIXBUFFERSIZE		(SAMPLECOUNT*BUFMUL)

// The actual lengths of all sound effects.
int lengths[NUMSFX];

// The global mixing buffer.
signed short mixbuffer[MIXBUFFERSIZE];

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

    // Remove the cached lump.
    Z_Free( sfx );

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
    channelleftvol_lookup[channel] =
	&vol_lookup[( 127 - seperation ) * 256];
    channelrightvol_lookup[channel] =
	&vol_lookup[seperation * 256];

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

    return rc;
}


//
// Stops a sound channel.
//
void I_StopSound(int handle)
{
    int i;
    for (i=0 ; i<NUM_CHANNELS ; i++)
    {
	if (channelhandles[i] == handle)
	{
	    channels[i] = 0;
	}
    }
}


//
// Called by S_*() functions
//  to see if a channel is still playing.
// Returns 0 if no longer playing, 1 if playing.
//
int I_SoundIsPlaying(int handle)
{
    int i;
    for (i=0 ; i<NUM_CHANNELS ; i++)
    {
	if (channelhandles[i] == handle && channels[i])
	{
	    return 1;
	}
    }
    return 0;
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

    for ( i = 0; i < NUM_CHANNELS; i++ )
    {
	if (channelhandles[i] == handle)
	{
	    // Update volume.
	    channelleftvol_lookup[i] =
		&vol_lookup[( 127 - sep ) * 256 + vol * 256*128];
	    channelrightvol_lookup[i] =
		&vol_lookup[sep * 256 + vol * 256*128];

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


//
// SDL audio callback function.
// Called by SDL when it needs more audio data.
//
static void audio_callback(void *userdata, Uint8 *stream, int len)
{
    // Update the mixing buffer
    I_UpdateSound();

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
    fprintf(stderr, "I_InitSound: user sound control not allowed\n");

    // Set up audio device
    SDL_zero(audio_spec);
    audio_spec.freq = SAMPLERATE;
    audio_spec.format = AUDIO_S16;
    audio_spec.channels = 2;
    audio_spec.samples = SAMPLECOUNT;
    audio_spec.callback = audio_callback;
    audio_spec.userdata = NULL;

    audio_device = SDL_OpenAudioDevice(NULL, 0, &audio_spec, NULL, 0);
    if (audio_device == 0)
    {
        I_Error("Could not open audio device: %s", SDL_GetError());
    }

    // Start playback
    SDL_PauseAudioDevice(audio_device, 0);

    fprintf(stderr, "I_InitSound: sound initialization complete\n");

    // Initialize volume lookup table
    // with gambits
    for (i=0 ; i<128 ; i++)
	for (j=0 ; j<256 ; j++)
	    vol_lookup[i*256+j] = (i*j*256)/(127*127);

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

    for (i=0; i<NUM_CHANNELS; i++)
    {
	channels[i] = 0;
	channelhandles[i] = -1;
	channelstep[i] = 0;
	channelstepremainder[i] = 0;
    }
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


//
// Dummy music functions
//
void I_InitMusic(void)
{
}

void I_ShutdownMusic(void)
{
}

void I_PlaySong(int handle, int looping)
{
}

void I_PauseSong (int handle)
{
}

void I_ResumeSong (int handle)
{
}

void I_StopSong(int handle)
{
}

void I_UnRegisterSong(int handle)
{
}

int I_RegisterSong(void* data)
{
    return 1;
}

int I_QrySongPlaying(int handle)
{
    return 0;
}

void I_SetMusicVolume(int volume)
{
}

//-----------------------------------------------------------------------------
//
// $Log:$
//
//-----------------------------------------------------------------------------
