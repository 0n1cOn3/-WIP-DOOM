# DOOM Modernization - Debugging Guide

## Critical Issues

### 1. Z_Free Memory Corruption (CRITICAL)

**Symptoms:**
- Game crashes with error: `Error: Z_Free: freed a pointer without ZONEID`
- Occurs after selecting difficulty, when loading screen should appear
- Indicates memory corruption in the zone memory allocator

**Root Cause:**
- Something is trying to free a pointer that either:
  1. Was not allocated through Z_Malloc (stack variable or static buffer)
  2. Has a corrupted ZONEID header (buffer overflow overwrote it)
  3. Has already been freed (double-free)

**Likely Culprits:**
1. **Sprite/Patch Loading** - `R_InitSprites()`, `V_DrawPatch()`
   - Complex pointer arithmetic in sprite frame handling
   - Potential buffer overflow in sprite lookup tables

2. **Level Initialization** - `P_SetupLevel()`
   - WAD lump loading and parsing
   - Map structure allocation

3. **Texture Loading** - `R_InitTextures()`, `R_InitFlats()`
   - Composite texture allocation
   - Flat lookup table initialization

4. **Stack Buffer Overflow** - Corrupting heap metadata
   - Local arrays in draw functions
   - String buffers in lump name handling

**How to Debug:**

#### Option 1: Enable AddressSanitizer (ASAN)
```bash
# Rebuild with ASAN
cmake -S . -B build \
  -DCMAKE_C_FLAGS="-fsanitize=address -g" \
  -DCMAKE_CXX_FLAGS="-fsanitize=address -g"
cmake --build build

# Run and watch for detailed error output
./build/bin/linuxdoom -iwad /path/to/DOOM.WAD 2>&1 | head -100
```

#### Option 2: Add Bounds Checking Assertions
Insert into `w_wad.c`, `r_data.c`:
```c
// In W_CacheLumpName before calling W_CacheLumpNum
if (name == NULL || strlen(name) > 8) {
    I_Error("W_CacheLumpName: invalid name parameter");
}

// In texture initialization
if (texturecount < 0 || texturecount > 32767) {
    I_Error("R_InitTextures: invalid texture count %d", texturecount);
}
```

#### Option 3: Add Debug Logging
Insert in `z_zone.c`, `Z_Free()`:
```c
fprintf(stderr, "Z_Free: ptr=%p, block->id=0x%x, expecting 0x%x\n",
        ptr, block->id, ZONEID);
```

**Suggested Fix Process:**
1. Build with ASAN to get exact location of corruption
2. If ASAN reports, fix the identified issue
3. If not ASAN-detected, add bounds checking in likely problem areas
4. Check for common buffer overflow patterns:
   - Fixed-size arrays receiving variable data (use `strncpy`, not `strcpy`)
   - Loop bounds not validated
   - Pointer arithmetic without bounds checking

---

## Feature Development: Dynamic Level Selection

### Current System
- Episode-based games (DOOM 1) have fixed 9 maps per episode
- DOOM II has fixed 32 maps
- Menu hardcodes map 1 for all game starts

### Proposed System
- Scan WAD file for available maps at startup
- Display only available maps in menu
- Support PWADs with custom map layouts
- Allow player to select any available map

### Implementation Status
✅ **Completed:**
- `M_ScanAvailableMaps()` function to detect available maps
- Supports both E#M# and MAP## formats
- Can be called from menu initialization

🔲 **TODO:**
1. Create dynamic menu for map selection
   - Allocate menu items based on available maps
   - Update menu drawing to handle variable-length lists
   - Handle map name labels (custom names from PWADs)

2. Integrate with existing menu flow
   - Add map selection between episode and difficulty
   - Update `M_ChooseSkill()` to use selected map instead of hardcoded 1
   - Handle "Random" map option

3. Test with various WADs
   - Vanilla DOOM.WAD (27 maps across 3 episodes)
   - DOOM2.WAD (32 maps)
   - Custom PWADs with non-standard layouts
   - PWADs with missing maps (E.g., only E1M1, E1M3, E1M5)

### Code References
- **Map Scanning:** `linuxdoom-1.10/m_menu.c:M_ScanAvailableMaps()`
- **Current Menu Flow:** `linuxdoom-1.10/m_menu.c:M_Episode()` -> `M_ChooseSkill()`
- **Game Initialization:** `linuxdoom-1.10/m_menu.c:M_ChooseSkill()` -> `G_DeferedInitNew()`
- **Hardcoded Map:** `linuxdoom-1.10/m_menu.c:1022` - `G_DeferedInitNew(choice,epi+1,1);`

### Example Usage
```c
int available_maps[32];
int map_count = M_ScanAvailableMaps(1, available_maps, 32);

// available_maps now contains: [1, 2, 3, 4, 5, 6, 7, 8, 9]
// map_count = 9
// (for a custom PWAD, might be: [1, 3, 5, 7, 9], map_count = 5)
```

---

## Audio Issues

### Investigation Progress (January 2026)

After detailed memory debugging with allocation tracking and lump loading logs, we've identified that the crash occurs **during Z_Malloc for the THINGS lump**, not in THINGS processing itself.

**Key Finding:** The zone memory allocator state is already corrupted before attempting to allocate THINGS. This suggests one of the map structure lumps being parsed is writing past its allocated buffer.

**Corrupted Block Signature:**
```
ptr=0x7f0271114a9c (end of allocation [19])
block->id=0x7f02 (corrupted - should be 0x1d4a11 ZONEID)
block->size=1896857812 (impossibly large - indicates heap overflow)
```

**Likely Culprits (in load order):**
1. LINEDEFS parsing - reads maplinedef_t structures, builds line_t array
2. SIDEDEFS parsing - reads mapsidedef_t structures
3. SEGS/NODES parsing - complex BSP tree data
4. REJECT matrix loading - bitfield parsing
5. BLOCKMAP loading - spatial indexing structure

**Common Overflow Patterns to Check:**
- Array bounds not validated when parsing WAD structures
- strcpy/sprintf without length limits
- Integer overflow in size calculations
- Off-by-one errors in loop bounds

### Audio Issues

#### Symptom: Audio Crackling/Pops When Switching Menus
- Occurs when transitioning between menu screens
- May be related to SDL audio buffer handling

### Investigation Needed
- Check if audio callback is being called during menu transitions
- Verify buffer sizes match SDL's expectations
- Consider adding mutex protection around audio mixing

### Likely Solution
- Lock/unlock audio device around menu transitions
- Ensure clean handoff between music backends (ADLMIDI → ALSA, etc.)

---

## Build & Test Checklist

### Pre-Release Testing
- [ ] Build with `-fsanitize=address` for memory safety
- [ ] Test with DOOM.WAD (DOOM 1)
- [ ] Test with DOOM2.WAD (DOOM II)
- [ ] Test with custom PWADs
- [ ] Test save/load functionality
- [ ] Test demo playback
- [ ] Verify audio in menus and gameplay
- [ ] Test fullscreen/windowed switching (Alt+Enter)
- [ ] Verify 4:3 aspect ratio on widescreen displays

### Performance Profiling
```bash
# With built-in profiling (if enabled):
valgrind --tool=massif ./build/bin/linuxdoom -iwad DOOM.WAD
perf record -g ./build/bin/linuxdoom -iwad DOOM.WAD
perf report
```

---

## Memory Corruption Investigation Summary (January 27, 2026)

### Progress Made

**Debugging Infrastructure Added:**
- Comprehensive Z_Free logging with corrupted block details
- Allocation tracking log showing recent 20 allocations before crash
- Lump loading diagnostics with sequence tracing
- W_ReadLump file I/O logging
- P_LoadThings and P_SetupLevel instrumentation

**Key Findings:**
1. Crash occurs during Z_Malloc for THINGS lump, NOT in THINGS data processing
2. Zone memory allocator state is already corrupted before THINGS allocation
3. One of the map data lumps being parsed writes past buffer boundaries
4. Corrupted block signature shows ZONEID header overwritten with memory pointers

**Defensive Bounds Checks Added:**
- P_LoadLineDefs: vertex index validation (prevents out-of-bounds vertexes[] access)
- P_LoadSideDefs: sector index validation (prevents out-of-bounds sectors[] access)
- W_CacheLumpName: lump name length validation
- W_ReadLump: lump size sanity check (<100MB)
- R_InitTextures: patch count and texture count bounds checking

**Remaining Suspects:**
- P_LoadSegs: Multiple complex index accesses (segs->linedef->sidenum[side]->sector)
- P_GroupLines: Complex linked list manipulation
- R_PrecacheLevel: Graphics precaching with complex lookups
- Sprite/patch loading during R_InitSprites (called before level load)

### Recommended Next Steps

**If ASAN library becomes available:**
```bash
sudo dnf install libasan-devel  # or similar for your distro
cmake -S . -B build -DCMAKE_C_FLAGS="-fsanitize=address -g -O1"
cmake --build build
./run-linuxdoom.sh /path/to/DOOM.WAD 2>&1 | head -200
```
This will pinpoint exact location of buffer overflow

**Resolution Status (January 27, 2026 - ASAN Debugging Complete):**

CRITICAL BUG FOUND AND FIXED:
==========================
**1-byte buffer overflow in IdentifyVersion() - d_main.c:681**

```c
// BEFORE (incorrect):
doomuwad = malloc(strlen(doomwaddir)+1+8+1);   // Only 8 bytes for "doomu.wad"
sprintf(doomuwad, "%s/doomu.wad", doomwaddir);  // Writes 9 chars + null = 10 bytes

// AFTER (correct):
doomuwad = malloc(strlen(doomwaddir)+1+9+1);   // Correct: 9 bytes for "doomu.wad"
sprintf(doomuwad, "%s/doomu.wad", doomwaddir);
```

**ASAN Detection:**
```
ERROR: AddressSanitizer: heap-buffer-overflow
WRITE of size 30 at 0x7bb3263e011d
0x7bb3263e011d is located 0 bytes after 29-byte region [0x7bb3263e0100,0x7bb3263e011d)
allocated by thread T0 at d_main.c:681 in IdentifyVersion
```

This overflow corrupted the zone memory allocator heap metadata, causing
the Z_Free "freed a pointer without ZONEID" crashes when loading levels.

**ADDITIONAL CRITICAL BUGS FOUND AND FIXED (January 27, 2026):**

After the sprintf overflow fix still resulted in SEGV crashes in Z_Malloc,
investigation revealed multiple secondary buffer overflows in map data loaders:

### 2. Buffer Overflow in P_GroupLines() - p_setup.c:560
```c
// BEFORE (vulnerable):
seg = &segs[ss->firstline];  // No bounds check on firstline!
ss->sector = seg->sidedef->sector;

// AFTER (fixed):
if (ss->firstline >= numsegs)
    I_Error("P_GroupLines: subsector %d firstline %d >= numsegs %d", ...);
if (!seg->sidedef)
    I_Error("P_GroupLines: seg %d has NULL sidedef", ...);
```

**Issue:** Corrupted WAD subsector data could have invalid firstline indices,
causing out-of-bounds read/write in segs[] array. This corrupts zone heap
metadata, causing SEGV in Z_Malloc on subsequent allocations.

### 3. Buffer Overflows in P_LoadLineDefs() - p_setup.c:456-474
```c
// BEFORE (vulnerable):
ld->frontsector = sides[ld->sidenum[0]].sector;  // No validation!
ld->backsector = sides[ld->sidenum[1]].sector;

// AFTER (fixed):
if (ld->sidenum[0] != -1) {
    if (ld->sidenum[0] < 0 || ld->sidenum[0] >= numsides)
        I_Error("P_LoadLineDefs: linedef %d has invalid front sidedef %d...");
    ld->frontsector = sides[ld->sidenum[0]].sector;
}
```

**Issue:** Corrupted vertex indices could cause buffer overflow into adjacent
allocations.

### 4. Buffer Overflows in P_LoadSegs() - p_setup.c:179-217
```c
// BEFORE (vulnerable):
li->v1 = &vertexes[SHORT(ml->v1)];  // No bounds check!
ldef = &lines[linedef];              // No validation!
li->sidedef = &sides[ldef->sidenum[side]];  // No validation!

// AFTER (fixed):
if (v1_idx < 0 || v1_idx >= numvertexes)
    I_Error("P_LoadSegs: seg %d has invalid v1 index...");
if (linedef < 0 || linedef >= numlines)
    I_Error("P_LoadSegs: seg %d has invalid linedef...");
if (ldef->sidenum[side] < 0 || ldef->sidenum[side] >= numsides)
    I_Error("P_LoadSegs: seg %d references invalid sidedef...");
```

**Issue:** Multiple nested index chains without validation. Any corruption
in intermediate arrays cascades into buffer overflow.

**ROOT CAUSE ANALYSIS:**
The secondary buffer overflows revealed the actual problem: WAD parsing code
assumed all data was well-formed. When any upstream buffer overflow corrupted
map structure arrays, the unchecked index access would write into heap metadata,
breaking the zone allocator for all future allocations.

**RESOLUTION:**
Added comprehensive bounds checking in all map data loader functions to catch
corrupted WAD data with clear error messages before they corrupt allocator state.

**Result:** Game now initializes without SEGV crashes. Corrupted WAD data is
detected and reported clearly instead of causing silent heap corruption.

---

## Future Enhancements

1. **Map Selection UI**
   - Display map names/descriptions from MAPINFO
   - Show par times and difficulty settings
   - Preview map in automap

2. **Content Consolidation**
   - Normalize WAD lump fallbacks for missing assets
   - Support custom HUD graphics from PWADs

3. **Multiplayer Testing**
   - Test network multiplayer with modern network backend
   - Verify packet handling for custom maps

4. **Graphics Modernization** (Future)
   - Widescreen support with proper HUD scaling
   - Optional HD sprite rendering
   - Shader-based rendering with OpenGL/Vulkan

---

## References

- **Original Linux DOOM:** id Software (1993-1997)
- **DOOM Engine Internals:** https://doomwiki.org/wiki/
- **SDL2 Documentation:** https://wiki.libsdl.org/
- **AddressSanitizer:** https://github.com/google/sanitizers
