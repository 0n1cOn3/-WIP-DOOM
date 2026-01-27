# Third-Party Licenses

This project links against the following upstream libraries. Respect their licenses when redistributing binaries.

| Component | Purpose | License(s) |
| --- | --- | --- |
| SDL2 | Core video/audio/input | zlib license citeturn0search4turn0search6 |
| SDL2_net | Networking transport | zlib license citeturn0search3 |
| libADLMIDI | OPL3 (FM) music synthesis | Mixed: LGPL-2.1+/GPL-2.0+/GPL-3.0+/MIT depending on modules citeturn0search1turn1search8 |
| libOPNMIDI | OPN2/OPNA music synthesis | LGPL-2.1-or-later / GPL-2.0-or-later / GPL-3.0-or-later / MIT citeturn1search0 |
| ALSA (optional) | Sequencer fallback for MIDI | LGPL-2.1 citeturn2search1 |

Notes:
- SDL2_net is optional; a stub builds automatically when the system library is absent.
- ADLMIDI/OPNMIDI are fetched via CMake FetchContent; review their licensing if you statically link or redistribute binaries.
- When shipping binaries, include this notice and the full `LICENSE.TXT` (GPL-2.0-only) to satisfy GPL requirements.
