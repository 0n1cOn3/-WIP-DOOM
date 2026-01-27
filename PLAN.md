# Implementation Plan: DOOM Multiplayer Lobby UI System

## Overview

Implement a complete multiplayer lobby UI system that integrates with DOOM's existing menu system and secure networking layer. The lobby provides:
- Host configuration (player count 2-4, vanilla-only toggle)
- LAN server discovery and browsing
- Manual IP entry for direct connection
- Real-time roster display and session info
- Error handling with timeouts and user-friendly messages

## User Requirements

From the user's request:
1. **Lobby polish**: Proper lobby screen showing roster, session key/hash status, vanilla-only toggle
2. **LAN list UX**: Scrollable/selectable discovered hosts, "Refresh" action, clean error surfacing
3. **Host screen**: Set player count, toggle vanilla-only (gates PWADs), show session info
4. **Join screen**: Browse LAN servers or direct IP entry
5. **Status display**: Show who's joined, wait for START message

## Technical Constraints

Based on codebase exploration:
- **Menu System**: Fixed item lists (no built-in scrolling), text/patch rendering, UP/DOWN/ENTER navigation
- **Network Layer**: Already has LAN discovery (`I_RunLanDiscovery`), secure handshake (JOIN/ACK/START), session keys, content hashing
- **Current Limitation**: `I_InitNetwork()` blocks during connection - must refactor to async for responsive UI
- **Graphics**: Create M_MULTI lump for authentic DOOM aesthetic (requires SLADE3 or similar WAD editor)

## Architecture

### Menu Hierarchy

```
Main Menu
└── Multiplayer (NEW)
    ├── Host Game
    │   ├── Configure: Player count (2-4), Vanilla toggle (ON/OFF)
    │   └── → Waiting Lobby (shows roster, session key/hash)
    ├── Join Game
    │   ├── LAN Browser (8 fixed slots + Manual IP + Refresh)
    │   └── → Waiting Lobby (shows connection status)
    └── Back
```

## Key Design Decisions

### 1. Fixed 8-Server List (No Scrolling)
**Decision**: Display max 8 LAN servers, no scrolling
**Rationale**:
- Matches retro DOOM aesthetic (Load/Save has 6 fixed slots)
- Avoids implementing complex scrolling UI
- Sufficient for typical LAN parties (8+ concurrent hosts rare)
- Empty slots show "-"

**Trade-off**: Users with 9+ local servers must use manual IP entry

### 2. Async Network Initialization
**Decision**: Refactor `I_InitNetwork()` into 3 phases:
- `I_InitNetworkAsync()`: Start connection, return immediately
- `I_PollNetworkInit()`: Check status each frame (called from `M_Ticker()`)
- `I_FinishNetworkInit()`: Complete setup when ready

**Rationale**:
- Menu must remain responsive during 30s connection timeout
- User can cancel with ESC at any time
- Follows best practices for UI programming

**CLI Compatibility**: Keep blocking behavior for `-host`/`-connect` CLI args (sync wrapper around async functions)

### 3. Auto-Start vs Manual Start
**Decision**: Manual "START GAME" button
**Rationale**:
- More control for host (can start with fewer players or wait)
- Allows reviewing roster before starting
- Matches user preference for explicit control

**Implementation**: Add separate "START GAME" button to waiting lobby, only visible to host

### 4. Player Names
**Decision**: Implement real player names from CLI arg or config file
**Rationale**:
- User preference for better UX immediately
- Add `-player <name>` CLI argument
- Extend JOIN_REQ protocol to include 16-char name field
- Host and clients send names, displayed in roster

**Implementation**: Add name field to JOIN_REQ packet, store in lobby state

### 5. Server Metadata in Browser
**Decision**: Phase 1 shows only IP:port (no player count, vanilla status)
**Rationale**:
- Requires extending discovery protocol (ANNOUNCE includes metadata)
- Adds complexity to discovery message parsing
- Users can try connecting to see details (lobby shows roster)

**Future Enhancement**: ANNOUNCE packet includes JSON metadata

## Implementation Phases

### Phase 1: Graphics Assets & Menu Structure - Estimated 5 hours

**Goal**: Create M_MULTI lump and add multiplayer menus with static data

**Files**:
- `/home/hx/Github/DOOM/linuxdoom-1.10/m_menu.c`
- `/home/hx/Github/DOOM/linuxdoom-1.10/m_menu.h`
- Create or modify WAD file for M_MULTI lump

**Tasks**:
1. **Graphics Asset Creation** (1 hour):
   - Use SLADE3 or similar WAD editor
   - Create only "M_MULTI" lump (320x8 patch) - reuse M_NEWG font style
   - **Reuse existing assets** (no creation needed):
     - `M_LSLEFT/M_LSCNTR/M_LSRGHT` - Border boxes for server list entries
     - `M_THERMM/THERML/THERMR/THERMO` - Optional: progress bar for "X/N players"
   - Add M_MULTI to doom1.wad or create separate lobby.wad PWAD
   - Test loading with W_CacheLumpName()

2. **Menu Structure** (4 hours):
   - Add "Multiplayer" option to MainMenu enum and array
   - Create `MultiplayerDef` menu (Host/Join/Back)
   - Create `HostDef` menu (Players/Vanilla/Start/Back)
   - Create `JoinDef` menu (8 server slots/Manual/Refresh/Back)
   - Implement entry point handlers: `M_Multiplayer()`, `M_HostGame()`, `M_JoinGame()`
   - Add draw routines: `M_DrawMultiplayer()`, `M_DrawHostSetup()`, `M_DrawJoinBrowser()`
   - Test navigation with arrow keys, ENTER, ESC

**State Variables** (add to m_menu.c):
```c
typedef struct {
    int player_count;           // 2-4
    int vanilla_only;           // 0 or 1
    char session_key_str[40];   // hex string
    char content_hash_str[40];  // hex string
    int connected_players;
    char player_names[MAXPLAYERS][16];
} lobby_state_t;

static lobby_state_t lobby_state;

typedef struct {
    IPaddress servers[8];
    int server_count;
    char manual_ip[64];
    int manual_ip_cursor;
} browser_state_t;

static browser_state_t browser_state;

typedef enum {
    LOBBY_MODE_NONE,
    LOBBY_MODE_HOST_SETUP,
    LOBBY_MODE_HOST_WAITING,
    LOBBY_MODE_CLIENT_BROWSE,
    LOBBY_MODE_CLIENT_CONNECT,
    LOBBY_MODE_CLIENT_WAITING,
    LOBBY_MODE_ERROR
} lobby_mode_t;

static lobby_mode_t lobby_mode = LOBBY_MODE_NONE;
```

**Success Criteria**:
- Can navigate from Main → Multiplayer → Host/Join
- Arrow keys cycle player count (2-3-4-2)
- Vanilla toggle switches ON/OFF
- ESC returns to previous menu
- No crashes

### Phase 2: Host Setup & Player Names - Estimated 4 hours

**Goal**: Implement host configuration handlers and player name support

**Files**:
- `/home/hx/Github/DOOM/linuxdoom-1.10/m_menu.c`
- `/home/hx/Github/DOOM/linuxdoom-1.10/m_argv.c` (CLI arg parsing)
- `/home/hx/Github/DOOM/linuxdoom-1.10/doomdef.h` (player name global)

**Tasks**:
1. **Host Configuration** (2 hours):
   - Implement `M_HostPlayers()`: Left/right arrows cycle 2-4
   - Implement `M_HostVanilla()`: Toggle vanilla_only flag
   - Call `I_SetVanillaOnly()` to sync with network layer
   - Update `M_DrawHostSetup()` to show current values
   - Test all configurations

2. **Player Names** (2 hours):
   - Add global `char playername[16]` to doomdef.h/doomstat.h
   - Add `-player <name>` CLI arg parsing in m_argv.c
   - Default to system hostname if not specified
   - Store in lobby_state for display
   - Extend JOIN_REQ packet format (add 16 bytes after content hash)
   - Host sends name in ANNOUNCE, clients in JOIN_REQ

**Key Functions**:
```c
void M_HostPlayers(int choice);    // handle left/right arrows
void M_HostVanilla(int choice);    // toggle vanilla mode
void M_HostStart(int choice);      // transition to waiting lobby
void M_ParsePlayerName(void);      // parse -player CLI arg
const char* M_GetPlayerName(void); // get player name (CLI or hostname)
```

**Success Criteria**:
- Player count cycles correctly
- Vanilla toggle updates display
- `-player "Alice"` sets name correctly
- Default name fallback works (hostname)
- Values persist during menu navigation
- No crashes

### Phase 3: Network Refactoring (Critical Path) - Estimated 8 hours

**Goal**: Make network initialization non-blocking

**Files**:
- `/home/hx/Github/DOOM/linuxdoom-1.10/i_net.c`
- `/home/hx/Github/DOOM/linuxdoom-1.10/i_net.h`

**Tasks**:
1. Add status enum and state tracking variables to i_net.c
2. Extract host lobby logic from I_InitNetwork() into async functions
3. Extract client connection logic into async functions
4. Implement `I_InitNetworkAsync(is_host, player_count)`
5. Implement `I_PollNetworkInit()` - non-blocking state check
6. Implement `I_FinishNetworkInit()` - complete setup when ready
7. Implement `I_CancelNetworkInit()` - cleanup on ESC
8. Add lobby state queries: `I_GetLobbyPlayerCount()`, `I_GetTotalPlayers()`, `I_GetLobbyRoster()`
9. Wrap async functions with sync version for CLI backward compat
10. Test with `-host 2` and `-connect 127.0.0.1` (must still work)

**New Functions** (add to i_net.h):
```c
#define NET_STATUS_INIT       0
#define NET_STATUS_WAITING    1
#define NET_STATUS_READY      2
#define NET_STATUS_REJECTED   3
#define NET_STATUS_TIMEOUT    4
#define NET_STATUS_ERROR      5

void I_InitNetworkAsync(int is_host, int player_count);
int I_PollNetworkInit(void);        // returns NET_STATUS_*
void I_FinishNetworkInit(void);
void I_CancelNetworkInit(void);
int I_GetLobbyPlayerCount(void);
int I_GetTotalPlayers(void);
void I_GetLobbyRoster(char names[][16], int max);
void I_GetSessionInfo(uint8_t *key, uint8_t *hash);
```

**State Variables** (add to i_net.c):
```c
static int net_init_state = NET_STATUS_INIT;
static Uint32 net_init_start_ticks;
static int net_init_is_host;
static int net_init_total_players;
static int net_init_connected = 0;
```

**Success Criteria**:
- `I_InitNetworkAsync()` returns immediately
- `I_PollNetworkInit()` called from menu returns status without blocking
- CLI args `-host`/`-connect` still work (blocking wrapper)
- No regressions in existing network code

### Phase 4: Waiting Lobby with Manual Start - Estimated 5 hours

**Goal**: Display connection status, roster, session info with manual START button

**Files**:
- `/home/hx/Github/DOOM/linuxdoom-1.10/m_menu.c`

**Tasks**:
1. Create `WaitingLobbyDef` menu with selectable items:
   - For host: "START GAME" button (only enabled when enough players)
   - For client: no selectable items (status display only)
2. Implement `M_TickHostLobby()` - poll network state each frame
3. Implement `M_TickClientLobby()` - check for START/NACK
4. Integrate ticker functions into `M_Ticker()`
5. Implement `M_DrawWaitingLobby()` - show status based on mode
6. Add manual "START GAME" button handler for host
7. Add `M_FormatSessionInfo()` - convert keys to hex strings
8. Implement 30-second timeout check
9. Add ESC cancel handling in `M_Responder()`
10. Implement error message display with `M_StartMessage()`
11. Add `M_LaunchMultiplayerGame()` - close menu and start game
12. Show player names in roster (from protocol)

**Key Functions**:
```c
void M_TickHostLobby(void);         // poll host lobby state
void M_TickClientLobby(void);       // poll client connection
void M_DrawWaitingLobby(void);      // draw status screen
void M_FormatSessionInfo(void);     // format keys as hex
void M_LaunchMultiplayerGame(void); // start game when ready
```

**Display Elements**:
- **Host Waiting**:
  - "WAITING FOR PLAYERS" (title)
  - Session key: [hex] (first 8 bytes)
  - Content hash: [hex] (first 8 bytes)
  - Vanilla only: YES/NO
  - Players: X / N
  - Roster list with real names: "Alice", "Bob", etc.
  - "START GAME" button (enabled when >= 2 players, can start with fewer than max)
  - "ESC TO CANCEL"

- **Client Waiting**:
  - "CONNECTING TO HOST" (title)
  - "WAITING FOR RESPONSE..." or "Players: X / N"
  - Roster with real names (if received)
  - "ESC TO CANCEL"

**Success Criteria**:
- Lobby updates roster dynamically
- Session key/hash displayed correctly
- ESC cancels and returns to previous menu
- Timeout triggers error message after 30s
- Game launches when all players ready

### Phase 5: LAN Discovery - Estimated 3 hours

**Goal**: Implement server browser with refresh

**Files**:
- `/home/hx/Github/DOOM/linuxdoom-1.10/m_menu.c`

**Tasks**:
1. Implement `M_RefreshLanServers()` - call `I_RunLanDiscovery()`
2. Update `M_DrawJoinBrowser()` to show discovered servers
3. Implement `M_JoinSelect()` - handle server selection
4. Call `M_RefreshLanServers()` automatically on menu open
5. Handle "No servers found" case
6. Implement `M_ConnectToServer()` - transition to waiting lobby
7. Test with real network (2 machines)

**Key Functions**:
```c
void M_RefreshLanServers(void);     // trigger LAN discovery
void M_JoinSelect(int choice);      // select server from list
void M_ConnectToServer(IPaddress*); // start connection
```

**Display**:
- 8 fixed slots showing "IP:PORT" or "-" (use M_LSLEFT/LSCNTR/LSRGHT border boxes like save game)
- Optional: M_THERMO progress bar showing "X servers found"
- "ENTER IP MANUALLY"
- "REFRESH"
- "BACK"
- "NO SERVERS FOUND" message if empty

**Success Criteria**:
- Discovered servers appear in list
- Refresh button updates list
- Can select and connect to server
- Empty list shows helpful message

### Phase 6: Manual IP Entry - Estimated 3 hours

**Goal**: Allow direct IP entry for WAN or non-discoverable hosts

**Files**:
- `/home/hx/Github/DOOM/linuxdoom-1.10/m_menu.c`

**Tasks**:
1. Add text input handling to `M_Responder()` (reuse save game pattern)
2. Implement `M_JoinManual()` - enter text input mode
3. Implement `M_DrawManualConnect()` - overlay with text entry
4. Implement `M_ConnectToManualIp()` - validate and connect
5. Add `M_ResolveIpString()` - wrapper around ResolveAddressSpec
6. Handle invalid address errors
7. Test valid formats: "192.168.1.100", "10.0.0.5:5029"
8. Test invalid inputs: "999.999.999.999", ""

**Key Functions**:
```c
void M_JoinManual(int choice);      // enter IP entry mode
void M_DrawManualConnect(void);     // draw text entry overlay
void M_ConnectToManualIp(void);     // validate and connect
boolean M_ResolveIpString(const char*, IPaddress*);
```

**Input Handling**:
- Accept: 0-9, '.', ':', '[', ']'
- BACKSPACE: delete char
- ENTER: connect
- ESC: cancel

**Display**:
- "ENTER SERVER ADDRESS"
- Text box with cursor
- "EXAMPLES: 192.168.1.100 OR 10.0.0.5:5029"
- "ENTER TO CONNECT, ESC TO CANCEL"

**Success Criteria**:
- Can type IP addresses
- Valid addresses connect
- Invalid addresses show error
- ESC cancels cleanly

### Phase 7: Polish & Error Handling - Estimated 4 hours

**Goal**: Add timeouts, error messages, sound effects

**Files**:
- `/home/hx/Github/DOOM/linuxdoom-1.10/m_menu.c`
- `/home/hx/Github/DOOM/linuxdoom-1.10/i_net.c`

**Tasks**:
1. Add 30-second timeout to `I_PollNetworkInit()`
2. Implement all error cases with `M_StartMessage()`:
   - Connection timeout
   - Connection refused (NACK)
   - Content hash mismatch
   - Invalid address
3. Add sound effects to all menu actions
4. Add back navigation handlers
5. Implement `M_BackToMultiplayerMenu()`, `M_BackToJoinMenu()`
6. Test all error paths
7. Verify memory cleanup on errors

**Error Messages**:
- "CONNECTION TIMEOUT\n\nPRESS A KEY"
- "CONNECTION REFUSED\n\nPRESS A KEY"
- "MOD MISMATCH\n\nHOST IS VANILLA ONLY\n\nPRESS A KEY"
- "INVALID ADDRESS\n\nPRESS A KEY"

**Success Criteria**:
- All errors handled gracefully
- Timeouts return to menu
- No memory leaks on error paths
- Sound effects match DOOM conventions

### Phase 8: Testing & Debugging - Estimated 8 hours

**Goal**: Comprehensive end-to-end testing

**Test Scenarios**:

1. **Menu Navigation**:
   - Navigate all menus with keyboard
   - Test ESC back navigation
   - Test arrow key cycling
   - No crashes

2. **Host Flow**:
   - Configure 2/3/4 players
   - Toggle vanilla on/off
   - Start hosting
   - Wait for clients (test with real client)
   - Verify roster updates
   - Cancel with ESC
   - Timeout test (30s with no clients)

3. **Join Flow**:
   - Refresh server list
   - Select server
   - Wait for START
   - Cancel with ESC

4. **Manual IP**:
   - Valid IP: "192.168.1.100"
   - Valid IP:port: "192.168.1.100:5029"
   - Invalid IP: "999.999.999.999"
   - Empty string

5. **Error Cases**:
   - Connection timeout (disable network)
   - Connection refused (wrong hash)
   - Invalid address

6. **CLI Regression**:
   - `./linuxdoom -host 2` waits for 1 client
   - `./linuxdoom -connect 127.0.0.1` connects
   - Both start game without showing menu

7. **Memory Leaks**:
   - Valgrind/sanitizer run
   - Check socket cleanup
   - Check packet deallocation

**Success Criteria**:
- All test scenarios pass
- No crashes
- No memory leaks
- CLI behavior unchanged

## Critical Files to Modify

### New Files
1. **M_MULTI lump in WAD file** - Graphics asset for "MULTIPLAYER" menu title (320x8 patch)

### Modified Files

1. **`/home/hx/Github/DOOM/linuxdoom-1.10/m_menu.c`** (85% of work)
   - Add lobby state structures (~50 lines)
   - Add 3 new menu definitions (~70 lines, includes START button)
   - Add ~18 new handler functions (~350 lines, includes player names + manual start)
   - Add 4 draw routines (~170 lines, displays player names)
   - Add ticker integration (~50 lines)
   - Update M_Responder for text input (~30 lines)
   - **Total**: ~720 lines added

2. **`/home/hx/Github/DOOM/linuxdoom-1.10/m_menu.h`**
   - May need to expose debug functions
   - **Total**: ~5 lines added

3. **`/home/hx/Github/DOOM/linuxdoom-1.10/i_net.c`**
   - Add state tracking variables (~20 lines)
   - Refactor I_InitNetwork to async (~200 lines modified)
   - Add async init functions (~150 lines)
   - Add lobby state queries (~50 lines)
   - **Total**: ~420 lines added/modified

4. **`/home/hx/Github/DOOM/linuxdoom-1.10/i_net.h`**
   - Add status enum (~8 lines)
   - Add async function declarations (~10 lines)
   - Add lobby query functions (~5 lines)
   - **Total**: ~23 lines added

5. **`/home/hx/Github/DOOM/linuxdoom-1.10/d_main.c`**
   - Add player name CLI arg parsing (~10 lines)
   - Verify M_Ticker integration (read-only review)

6. **`/home/hx/Github/DOOM/linuxdoom-1.10/doomstat.h`**
   - Add `extern char playername[16]` global declaration (~1 line)

7. **`/home/hx/Github/DOOM/linuxdoom-1.10/doomdef.h`** or create new player name source file
   - Define `char playername[16]` storage (~1 line)

## Risks & Mitigations

### High Risk: Network Refactoring
**Risk**: Breaking existing `-host`/`-connect` CLI behavior
**Impact**: Critical - CLI users can't connect
**Mitigation**:
- Test CLI extensively before/after
- Keep sync wrapper for CLI
- Test with real network (2 machines)

### Medium Risk: Race Conditions
**Risk**: Polling network state while packets arriving
**Impact**: Could miss packets or double-process
**Mitigation**:
- Keep state machine simple
- Document assumptions
- SDL_net recv/send are thread-safe

### Medium Risk: Memory Leaks
**Risk**: Not freeing packets/sockets on error paths
**Impact**: Long-running game could crash
**Mitigation**:
- Add cleanup in I_CancelNetworkInit()
- Test all error paths
- Run valgrind

### Low Risk: Draw Routine Bugs
**Risk**: Text misalignment, overlapping elements
**Impact**: Cosmetic only
**Mitigation**: Visual testing, easy to fix

## Verification Plan

### Build Verification
```bash
cd /home/hx/Github/DOOM/linuxdoom-1.10
cmake --build build
```
**Expected**: Build succeeds, no warnings

### Unit Testing
```bash
# Host on port 5029, 2 players
./build/bin/linuxdoom -host 2

# Connect from another terminal/machine
./build/bin/linuxdoom -connect 127.0.0.1
```
**Expected**: Both start game, multiplayer works

### Menu Testing
1. Launch game without CLI args
2. Navigate: Main → Multiplayer → Host
3. Set player count to 3, toggle vanilla ON
4. Start hosting
5. Verify waiting lobby shows session info
6. Cancel with ESC
7. Navigate: Main → Multiplayer → Join
8. Refresh server list
9. Enter manual IP
10. Cancel and return to main menu

**Expected**: No crashes, responsive UI

## Alternative Approaches Considered

### Alternative 1: Scrolling Server List
**Approach**: Implement scrollable list (20+ servers)
**Pros**: More servers visible, modern UX
**Cons**: Complex pagination logic, not retro DOOM style
**Decision**: Rejected - fixed 8-slot list matches DOOM aesthetic

### Alternative 2: Separate IP Entry Menu
**Approach**: New menu screen for IP entry instead of overlay
**Pros**: Cleaner separation
**Cons**: Extra menu navigation, more code
**Decision**: Rejected - overlay matches save game pattern

### Alternative 3: Manual Start Button
**Approach**: Host manually clicks "START" instead of auto-start
**Pros**: More control, can start with fewer players
**Cons**: Extra complexity, less common multiplayer pattern
**Decision**: Rejected - auto-start is simpler

### Alternative 4: Keep Blocking Network Init
**Approach**: Don't refactor to async, show "Connecting..." screen
**Pros**: Less code change
**Cons**: Can't cancel, poor UX, 30s freeze
**Decision**: Rejected - responsive UI is essential

## Implementation Order

1. Phase 1: Graphics assets & menu structure (5 hours)
2. Phase 2: Host setup & player names (4 hours)
3. Phase 3: Network refactoring (async init) - CRITICAL PATH (8 hours)
4. Phase 4: Waiting lobby with manual START button (5 hours)
5. Phase 5: LAN discovery (3 hours)
6. Phase 6: Manual IP entry (3 hours)
7. Phase 7: Polish & error handling (4 hours)
8. Phase 8: Testing & debugging (8 hours)

**Total Estimated Effort**: 40 hours (5-6 working days at 7-8 hours/day)

## Future Enhancements (Out of Scope for Phase 1)

1. **Server metadata in discovery**: Show player count, vanilla flag, map in browser
2. **Chat in lobby**: Text messages while waiting
3. **Map/episode selection in lobby**: Host picks starting point
4. **Skill selection in lobby**: Collaborative difficulty choice
5. **Password-protected servers**: Require shared secret
6. **Persistent favorites**: Save preferred servers to config
7. **Kick player**: Host can remove misbehaving players from lobby

## Success Criteria

1. ✅ Menu hierarchy complete (Main → Multiplayer → Host/Join)
2. ✅ Host can configure player count (2-4) and vanilla toggle
3. ✅ Waiting lobby shows roster, session key, content hash
4. ✅ LAN discovery works with refresh
5. ✅ Manual IP entry validates and connects
6. ✅ Errors handled gracefully (timeout, refused, invalid address)
7. ✅ ESC cancels at any point
8. ✅ CLI backward compatible (`-host`/`-connect` unchanged)
9. ✅ No memory leaks
10. ✅ Build succeeds without warnings

## Notes

- **Graphics assets**: Only need to create M_MULTI lump - reuse existing M_LSLEFT/LSCNTR/LSRGHT borders (from save/load), M_THERMO progress bars, and M_NEWG font style
- **Manual start button**: Host must explicitly click "START GAME" button, allowing flexible roster management
- **Real player names**: Implement `-player <name>` CLI arg and extend JOIN_REQ protocol with 16-char name field
- **Fixed list philosophy**: Matches DOOM's retro aesthetic (Load/Save has 6 slots)
- **Async network is critical**: Without it, UI freezes for 30s during connection
- **CLI wrapper preserves behavior**: Existing workflows unaffected
- **Discovery blocking acceptable**: User explicitly triggers, 500ms pause is reasonable
- **Server metadata deferred**: IP:port only for Phase 1, metadata requires discovery protocol enhancement
