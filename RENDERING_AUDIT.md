# Rendering & HUD Audit (Resolution Independence)

This file tracks the ongoing audit for sprite/patch scaling and HUD rendering on modern displays.

## Scope
- Sprite/patch scaling for resolution independence (current logical base: 320x200).
- HUD/status bar rendering readability on modern resolutions.
- Aspect interactions with UI elements and status bar (`-aspect`, `-widescreen`, `-stretch`).

## Initial Findings
- Status bar positions in `st_stuff.c` are hard-coded to 320x200 coordinates (e.g., `ST_*` constants).
- HUD/widget draws in `st_lib.c` assume 320x200 base with `V_CopyRect` and `V_DrawPatch*`.
- 4:3 mode uses a 320x240 logical height (pixel-aspect correction), 16:9 mode keeps 320x200 logical size, and stretch mode fills the window.

## Next Steps
- Decide target behavior for HUD in widescreen:
  - Preserve original size (letterbox) vs. scale HUD to window height.
- Identify patch draws that should use a scaled coordinate transform.
- Evaluate automap and intermission screens for scaling consistency.
