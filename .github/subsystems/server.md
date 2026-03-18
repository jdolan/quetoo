# Server System (server/)

The server hosts the game simulation, manages clients, runs game logic, and sends world state updates.

## Location

`src/server/` - Server subsystem (dedicated or integrated with client)

## Core Purpose

- Host multiplayer game sessions (dedicated server or listen server)
- Manage client connections and authentication
- Run game physics simulation at fixed tick rate (default 60 Hz)
- Broadcast world state to clients (entity snapshots)
- Interface with game module for gameplay logic
- Anti-cheat and admin controls

## Key Files

### sv_main.c / sv_main.h
Main server loop and initialization:
- `SV_Init()` / `SV_Shutdown()` - Server lifecycle
- `SV_Frame()` - Main server tick, runs at `sv_hz` rate (default 60 Hz)
- `SV_InitServer()` - Start new game (load map, spawn entities)
- `SV_ShutdownServer()` - End current game

**Server frame sequence** (60 times per second):
1. Process client input packets
2. Run game physics (`ge->Frame()`)
3. Build entity snapshots for each client
4. Send frame messages to clients
5. Update master server heartbeat

### sv_init.c / sv_init.h
Map loading and server initialization:
- `SV_InitServer()` - Load BSP, initialize collision, spawn entities
- `SV_SpawnServer()` - Full server restart (new map)
- Validates BSP, checks protocol version, sets up game state

### sv_world.c / sv_world.h
Entity management and spatial queries:
- `SV_LinkEntity()` / `SV_UnlinkEntity()` - Add/remove entity from world
- `SV_AreaEntities()` - Find all entities in an area (spatial query)
- `SV_Trace()` - Wrapper around `Cm_BoxTrace()` that also tests entities
- `SV_PointContents()` - Check contents at point (BSP + entities)

**Spatial partitioning**: Entities are organized into areas (BSP leafs) for fast queries.

### sv_send.c / sv_send.h
Network message transmission:
- `SV_Multicast()` - Send message to multiple clients (PVS-based)
- `SV_Unicast()` - Send message to single client
- `SV_WriteFrameToClient()` - Build entity snapshot for client (delta-compressed)
- `SV_SendClientMessages()` - Flush network buffers to all clients

**Multicast types**:
- `MULTICAST_ALL` - Send to all clients
- `MULTICAST_PVS` - Send to clients in PVS (potentially visible set)
- `MULTICAST_PHS` - Send to clients in PHS (potentially hearable set)

### sv_client.c / sv_client.h
Client connection management:
- `SV_ClientConnect()` - New client connecting
- `SV_ClientBegin()` - Client ready to receive game state
- `SV_ClientDisconnect()` - Client leaving
- `SV_ExecuteClientMessage()` - Process client input (movement, commands)
- `SV_DropClient()` - Forcibly disconnect client

**Client states**:
- `SV_CLIENT_FREE` - Slot not in use
- `SV_CLIENT_CONNECTED` - TCP connected, authenticating
- `SV_CLIENT_PRIMED` - Loading map/resources
- `SV_CLIENT_ACTIVE` - In game, receiving frames

### sv_game.c / sv_game.h
Game module interface (loads game.so/.dll):
- `SV_InitGameProgs()` - Load and initialize game module
- `SV_ShutdownGameProgs()` - Unload game module
- Exports API to game module (`gi` import struct)
- Calls game module functions (`ge->Init()`, `ge->Frame()`, etc.)

**Game module exports** (`game_export_t`):
- `ge->Init()` - Game initialization
- `ge->Shutdown()` - Game cleanup
- `ge->SpawnEntity()` - Spawn entity from BSP entity definition
- `ge->Frame()` - Run game logic for one tick
- `ge->ClientConnect()` / `ge->ClientDisconnect()` - Client events
- `ge->ClientCommand()` - Handle client console commands

### sv_entity.c / sv_entity.h
Entity state management:
- `SV_BuildClientFrame()` - Build entity snapshot for client (visibility culling)
- `SV_FatPVS()` - Calculate potentially visible set for client
- Manages baseline entity states (initial spawns)
- Handles entity overflow (too many entities visible)

### sv_console.c / sv_console.h
Server console commands:
- `sv_status` - Show connected clients
- `sv_map <mapname>` - Load new map
- `sv_kick <client>` - Kick player
- `sv_kill` - Shutdown server
- Integrated with main console system

### sv_admin.c / sv_admin.h
Administrative controls:
- Mute/unmute players
- Kick/ban players  
- Change map
- Password-protected rcon (remote console)

### sv_master.c / sv_master.h
Master server communication:
- Heartbeat to master server (announces server availability)
- Server info updates (hostname, player count, map, etc.)
- HTTP-based protocol with quetoo.org master server

### sv_editor.c / sv_editor.h
In-game map editor support:
- Place/move entities in real-time
- Used by level designers for rapid iteration
- Network protocol for editor commands

## Server Architecture

### Server Types

**Dedicated server**:
- Standalone process, no graphics/sound
- Optimized for hosting
- Run with `quetoo-server` or `quetoo +dedicated 1`

**Listen server**:
- Integrated with client
- One player is "host", others connect
- Started with `map <mapname>` in client

Both use same server code, only difference is initialization.

### Entity Snapshot System

Server maintains circular buffer of world state snapshots:
```c
typedef struct {
    int32_t frame_num;              // Unique frame number
    entity_state_t entities[MAX];   // All entity states this frame
    int32_t num_entities;
} sv_frame_t;

sv_frame_t frames[UPDATE_BACKUP];   // Circular buffer
```

When sending to client:
1. Client acknowledges frame N (last frame it received)
2. Server sends frame M with delta from frame N
3. Only changed entity fields are sent (huge bandwidth savings)

### PVS (Potentially Visible Set)

Server only sends entities client can potentially see:
1. Calculate PVS for client's camera position
2. Include all entities in visible BSP leafs
3. Always include client's own entity
4. Delta-compress against client's last acknowledged frame

This prevents clients from "seeing through walls" via network traffic.

### Client Input Processing

Clients send movement commands at their framerate:
```c
// Client sends this structure
typedef struct {
    int32_t msec;        // Frame time
    vec3_t angles;       // View angles
    int16_t forward;     // Forward/back input (-400 to +400)
    int16_t right;       // Left/right strafe
    int16_t up;          // Jump/crouch
    uint16_t buttons;    // Fire, use, etc.
} user_cmd_t;
```

Server accumulates commands and runs physics at fixed 60 Hz:
1. Client sends input at 60-144 Hz (varies by client framerate)
2. Server buffers commands
3. Every 16.67ms (60 Hz), server processes all buffered commands
4. Runs game physics for that tick
5. Sends updated world state

## Client State Management

Each connected client has:
```c
typedef struct {
    sv_client_state_t state;        // Connection state
    netchan_t netchan;              // Network channel
    
    user_cmd_t last_cmd;            // Last movement command
    int32_t last_frame;             // Last frame client acknowledged
    
    int32_t message_size;           // Bandwidth tracking
    int32_t rate;                   // Rate limiting
    
    int32_t ping;                   // Latency
    
    bool download;                  // Currently downloading file
    
    char userinfo[MAX_USER_INFO];   // Name, skin, team, etc.
    
    g_client_t *client;             // Game module client data
} sv_client_t;
```

## Console Variables

### Performance
```
sv_hz 60              # Server tickrate (60 Hz standard)
sv_max_clients 16     # Maximum player count
sv_timeout 15         # Disconnect timeout (seconds)
net_max_rate 0        # Per-client bandwidth cap (0=unlimited)
```

### Gameplay  
```
g_gameplay 0          # Game mode (0=deathmatch, 1=instagib, etc.)
g_teams 0             # Enable team play
g_match 0             # Match mode (warmup, ready-up)
g_frag_limit 30       # Score limit
g_time_limit 20       # Time limit (minutes)
g_cheats 0            # Enable cheat commands
```

### Server Info
```
sv_hostname "My Server"   # Server name
sv_max_clients 16         # Player slots
sv_public 1               # Advertise to master server
```

## Common Patterns

### Broadcasting Events
```c
// Send event to all clients that can see this position
gi.PositionedSound(origin, entity, entity->s.event, ATTEN_NORM);

// Implemented as:
Net_WriteByte(&sv.multicast, SV_CMD_SOUND);
Net_WritePosition(&sv.multicast, origin);
Net_WriteShort(&sv.multicast, sound_index);
SV_Multicast(origin, MULTICAST_PHS);  // Send to hearable set
```

### Spawning Entities
```c
// Game module spawns entity
g_entity_t *ent = G_Spawn();
ent->classname = "item_health";
Vec3_Copy(origin, ent->s.origin);
ent->solid = SOLID_TRIGGER;

// Link into world (server spatial database)
gi.LinkEntity(ent);
```

### Kicking Players
```c
sv_client_t *cl = &svs.clients[client_num];
SV_DropClient(cl, "Kicked by admin");
SV_BroadcastPrint("%s was kicked\n", cl->name);
```

## Performance Characteristics

### CPU Usage
- **Physics**: Most expensive (collision traces, entity updates)
- **PVS/snapshot building**: Second most expensive
- **Network I/O**: Relatively cheap (mostly copying memory)

Typical dedicated server:
- **Empty**: <1% CPU (idle)
- **16 players, active combat**: 10-20% CPU (single core)
- **16 bots, active combat**: 30-50% CPU (AI is expensive)

### Memory Usage
- **Base**: ~50 MB (BSP, textures, sounds)
- **Per client**: ~1 MB (frame history, netchan buffers)
- **16 clients**: ~70 MB total

### Network Bandwidth
- **Per client**: 5-60 KB/s outbound (varies by activity)
- **16 clients, peak**: ~1 Mbps outbound
- **Inbound**: Very low (~50 KB/s total for all clients)

## Debugging

### Server Console Commands
```
sv_status          # Show all connected clients (ping, rate, etc.)
developer 1        # Enable debug output
sv_no_vis 1        # Disable PVS culling (send all entities)
```

### Network Debugging
```
net_show_packets 2 # Show all network traffic
sv_rcon_password "debug"  # Set rcon password
```

### Entity Debugging
```c
// In game module
gi.DebugPrint("Entity %d at %s\n", ent->s.number, VectorToString(ent->s.origin));
```

## Integration with Game Module

Server is a "dumb host" - all gameplay logic is in game module (`src/game/default/`):

**Server provides** (via `gi` import struct):
- Entity linking (`gi.LinkEntity()`)
- Collision traces (`gi.Trace()`)
- Sound/temp entity broadcasting (`gi.Sound()`, `gi.WriteByte()`, etc.)
- Console variables (`gi.Cvar()`)
- Entity queries (`gi.InPVS()`, `gi.AreaEntities()`)

**Game provides** (via `ge` export struct):
- Gameplay rules (scoring, teams, weapons)
- Entity behavior (items, triggers, projectiles)
- Player physics (movement, jumping, swimming)
- AI (bots)

This separation allows different game modes/mods without changing server code.

## Common Issues

### Client Overflow
**Symptom**: "Too many entities in frame" warning
**Cause**: More entities visible than MAX_PACKET_ENTITIES (256)
**Solution**: Reduce entity count or increase limit (protocol change)

### High CPU Usage
**Symptom**: Server lag, frame skipping
**Causes**: 
- Too many entities (especially projectiles)
- Expensive collision traces
- AI pathfinding with many bots
**Solution**: Profile and optimize hot paths

### Clients Timing Out
**Symptom**: Players randomly disconnect
**Causes**:
- Network packet loss
- Server frame rate dropping below minimum
- Firewall blocking packets
**Solution**: Check `sv_hz` stays at target, reduce server load

### Invisible Entities
**Symptom**: Players report missing entities
**Cause**: PVS culling incorrectly hiding visible entities
**Debug**: Set `sv_no_vis 1` to disable PVS, check if entities appear
**Solution**: Fix PVS calculation or entity origin

## Security Considerations

### Anti-Cheat
- Server is authoritative for all gameplay
- Clients only send input, not positions/actions
- Server validates all moves (can't teleport, fly without power-up, etc.)
- PVS prevents wallhacks via network data

### Rate Limiting
- Command flood protection (max commands/second)
- Bandwidth rate limiting per client
- Connection attempt rate limiting

### Exploits to Watch For
- **Speed hacks**: Server validates movement distances
- **Aim bots**: Hard to prevent (client-side)
- **Denial of service**: Rate limiting, connection caps
- **Name spoofing**: Validate userinfo strings

## Map Loading Flow

1. Server receives `map <mapname>` command
2. `SV_InitServer()` called
3. Load BSP with `Cm_LoadBSP()`
4. Initialize game module: `ge->Init()`
5. Parse BSP entities: `Cm_Entities()`
6. Spawn each entity: `ge->SpawnEntity()` for each
7. Server is now ready for client connections

## File Downloads

Server can send files to clients (maps, models, sounds):
1. Client requests missing file
2. Server sends file in chunks via netchan
3. Client assembles chunks, verifies checksum
4. Client loads file and continues connecting

Enabled with `sv_allow_download 1` (default).
