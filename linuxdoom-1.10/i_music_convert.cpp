// Emacs style mode select   -*- C++ -*-
//-----------------------------------------------------------------------------
//
// MUS to MIDI conversion wrapper using libADLMIDI's converter.
//
//-----------------------------------------------------------------------------

#include <cstdint>
#include <cstdlib>

#include "cvt_mus2mid.hpp"

extern "C" int I_MusToMidi(const uint8_t *mus, uint32_t mus_len, uint8_t **out, uint32_t *out_len)
{
    if (!mus || !out || !out_len)
        return 0;

    uint8_t *midi_data = NULL;
    uint32_t midi_len = 0;
    if (Convert_mus2midi(const_cast<uint8_t *>(mus), mus_len, &midi_data, &midi_len, 0) < 0)
        return 0;

    *out = midi_data;
    *out_len = midi_len;
    return 1;
}

extern "C" void I_MusToMidiFree(uint8_t *data)
{
    free(data);
}
