# Network Layer (net/)

The network layer provides UDP/TCP/HTTP networking with reliable messaging, delta compression, and Quake II protocol compatibility.

## Location

`src/net/` - Network subsystem used by both client and server

## Core Purpose

- Low-level UDP/TCP socket management
- Reliable message delivery over UDP (netchan)
- Message encoding/decoding with bit-level precision
- HTTP client for master server communication and updates
- Delta compression for entity states

## Key Files

### net.c / net.h
Core networking initialization and utilities:
- `Net_Init()` / `Net_Shutdown()` - Initialize/cleanup network subsystem
- `Net_Socket()` - Create UDP or TCP socket
- `Net_NetaddrToString()` / `Net_StringToNetaddr()` - Address conversion
- `Net_CompareNetaddr()` - Compare network addresses
- Platform-specific socket setup (Windows vs Unix)

### net_udp.c / net_udp.h
UDP datagram handling:
- `Net_SendDatagram()` - Send unreliable datagram
- `Net_ReceiveDatagram()` - Receive datagram (non-blocking)
- Used by netchan for underlying transport

### net_tcp.c / net_tcp.h
TCP stream handling:
- `Net_Accept()` - Accept incoming TCP connection
- `Net_Connect()` - Connect to TCP server
- `Net_Send()` / `Net_Receive()` - Stream I/O
- Used primarily for reliable admin/rcon connections

### net_chan.c / net_chan.h
**Reliable messaging over UDP** (most important network component):

The netchan (network channel) provides:
- **Reliable delivery** - Packets are acknowledged, resent if lost
- **Sequencing** - Out-of-order packets are detected and reordered
- **Fragmentation** - Large messages split across multiple packets
- **Flow control** - Prevents overwhelming slow connections

**Key functions**:
- `Netchan_Setup()` - Initialize channel between client/server
- `Netchan_Transmit()` - Send reliable message
- `Netchan_Process()` - Process received packet, return true if valid message
- `Netchan_CanReliable()` - Can we send a reliable message now?

**Netchan pattern**:
```c
// Sending
if (Netchan_CanReliable(&client->netchan)) {
    // Write message
    Net_WriteData(&client->netchan.message, data, len);
    
    // Flush on next frame
    Netchan_Transmit(&client->netchan, data, len);
}

// Receiving
if (Netchan_Process(&client->netchan, &msg)) {
    // Valid message received, parse it
    const uint8_t cmd = Net_ReadByte(&msg);
    // ...
}
```

### net_message.c / net_message.h
**Bit-level message encoding/decoding**:

Provides efficient binary serialization with bit precision:
- `Net_WriteByte()`, `Net_WriteShort()`, `Net_WriteLong()` - Write integers
- `Net_WriteString()` - Write null-terminated string
- `Net_WritePosition()` - Write compressed 3D position (16 bits per axis)
- `Net_WriteAngle()` - Write compressed angle (8 bits, 256 values)
- `Net_WriteDeltaEntity()` - Write delta-compressed entity state
- Corresponding `Net_Read*()` functions

**Message buffer** (`mem_buf_t`):
- Automatically grows as data is written
- Tracks read/write position
- Can be "rewound" to re-read

**Delta compression**:
Entity states are sent as deltas from previous frame:
```c
// Only changed fields are sent
Net_WriteDeltaEntity(&msg, &old_state, &new_state, false);
```

Huge bandwidth savings - typical entity update is 10-20 bytes instead of 100+.

### net_http.c / net_http.h
HTTP client implementation:
- `Net_GetHTTP()` - Synchronous HTTP GET request
- Used for:
  - Master server queries (get server list)
  - Update checking
  - Downloading missing files (maps, models)
- Returns response body and HTTP status code

## Protocol Overview

### Connection Handshake

**Client → Server**:
1. Client sends `"connect"` command with protocol version
2. Server responds with `"client_connect"` + challenge number
3. Client sends challenge response
4. Server responds with `"cmd"` (connection accepted)

**Both directions**:
- Netchan is established with sequence numbers
- Client and server now exchange reliable messages

### Message Types

**Client → Server**:
- `CL_CMD_USER` - Player input (movement, buttons, angles)
- `CL_CMD_PING` - Ping measurement
- `CL_CMD_STRING` - Console command (say, kill, etc.)

**Server → Client**:
- `SV_CMD_BASELINE` - Initial entity states
- `SV_CMD_FRAME` - Entity state snapshot for this frame
- `SV_CMD_SOUND` - Play positioned sound
- `SV_CMD_PRINT` - Print message to console
- `SV_CMD_STUFFTEXT` - Execute command on client

See `src/shared/protocol.h` for complete protocol definitions.

### Frame Updates

Server sends frame snapshots at `sv_hz` rate (default 60 Hz):

```c
// Server frame message
Net_WriteByte(&msg, SV_CMD_FRAME);
Net_WriteLong(&msg, frame_num);
Net_WriteLong(&msg, delta_frame_num); // Frame to delta from
Net_WriteByte(&msg, num_entities);

// Write each entity as delta from client's last acknowledged frame
for (each entity) {
    Net_WriteDeltaEntity(&msg, &old_state, &new_state, false);
}
```

Client receives frame:
1. Read frame number and delta frame
2. Apply deltas to entity states
3. Store frame for future deltas
4. Render entities from frame

### Delta Compression Details

Entity state fields are delta-compressed individually:

```c
typedef struct {
    vec3_t origin;           // 3x16 bits (compressed)
    vec3_t angles;           // 3x8 bits (compressed)
    uint16_t animation;      // Animation frame
    uint16_t model;          // Model index
    uint32_t effects;        // Visual effects flags
    uint8_t bounds;          // Bounding box index
    // ... ~30 fields total
} entity_state_t;
```

Only changed fields are transmitted. Typical entity update:
- Stationary item: 1-2 bytes (just entity number)
- Moving player: 10-15 bytes (origin, angles, animation)
- Full entity: 80-100 bytes (rare - only on spawn)

## Network Address Types

```c
typedef enum {
    NA_DATAGRAM,  // UDP (default for game traffic)
    NA_STREAM     // TCP (admin/rcon)
} net_addr_type_t;

typedef struct {
    net_addr_type_t type;
    in_addr_t ip;           // IPv4 address
    in_port_t port;
} net_addr_t;
```

Currently IPv4 only. IPv6 support is a TODO.

## Performance Characteristics

### Bandwidth Usage

Typical server → client bandwidth per connected player:
- **Quiet scene**: 5-10 KB/s (few entities, little movement)
- **Active combat**: 15-30 KB/s (many entities, lots of movement)
- **Peak**: 40-60 KB/s (all entities changing, complex scene)

Client → server is much lower:
- **User commands**: 2-5 KB/s (movement, buttons, angles)

### Latency Handling

- **Client prediction**: Client simulates movement locally, server corrects
- **Interpolation**: Client renders entities between frames (smooth motion)
- **Extrapolation**: Client can predict entity position if frame is late
- **Lag compensation**: Server uses client's view time for hit detection

### Packet Loss Tolerance

- Netchan handles packet loss automatically
- Reliable messages are resent until acknowledged
- Entity deltas are cumulative (missing a frame is recoverable)
- Typical playable packet loss: up to 10-15%

## Console Variables

### Server
```
sv_hz 60              # Server frame rate (default 60)
sv_max_clients 16     # Maximum client connections
sv_timeout 15         # Client timeout in seconds
net_max_rate 0        # Max bytes/sec per client (0=unlimited)
```

### Client
```
cl_max_rate 0         # Max bytes/sec from server (0=unlimited)
net_show_packets 0    # Debug: show packet traffic (1=basic, 2=verbose)
```

## Common Patterns

### Server: Broadcasting to All Clients
```c
for (int i = 0; i < sv_max_clients->integer; i++) {
    sv_client_t *cl = &svs.clients[i];
    if (cl->state != SV_CLIENT_ACTIVE)
        continue;
    
    // Write to this client's message buffer
    Net_WriteByte(&cl->netchan.message, SV_CMD_PRINT);
    Net_WriteString(&cl->netchan.message, "Hello world\n");
}
```

### Client: Sending User Command
```c
// Client movement is sent every frame
Net_WriteByte(&cls.netchan.message, CL_CMD_USER);
Net_WriteDeltaUserCmd(&cls.netchan.message, &old_cmd, &cmd);
```

### Reading Messages
```c
mem_buf_t msg;
// ... receive into msg ...

const uint8_t cmd = Net_ReadByte(&msg);
switch (cmd) {
    case SV_CMD_FRAME:
        const int32_t frame = Net_ReadLong(&msg);
        // ... parse frame ...
        break;
    case SV_CMD_PRINT:
        const char *text = Net_ReadString(&msg);
        Com_Print("%s", text);
        break;
}
```

## Master Server Protocol

Master server (http://quetoo.org/servers) tracks active game servers:

**Server → Master** (heartbeat every 5 minutes):
```
POST /servers
hostname=My Server&max_clients=16&current_clients=4&...
```

**Client → Master** (get server list):
```
GET /servers
```

Returns JSON list of active servers:
```json
[
  {"hostname": "Server 1", "address": "1.2.3.4:1996", "clients": "4/16"},
  ...
]
```

See `src/server/sv_master.c` and `src/client/cl_main.c` for implementation.

## Debugging Network Issues

### Enable Packet Debugging
```
net_show_packets 2    # Show all packet I/O
developer 1           # Enable debug output
```

### Check Connection State
```c
Com_Print("Netchan: out=%d, in=%d, dropped=%d\n",
          chan->outgoing_sequence,
          chan->incoming_sequence, 
          chan->dropped);
```

High `dropped` count indicates packet loss.

### Test Latency
```
ping                  # Shows current ping to server
```

Ping is measured by:
1. Client sends `CL_CMD_PING` with timestamp
2. Server echoes it back
3. Client calculates round-trip time

## Common Issues

### Client Timeouts
- Netchan requires regular messages (heartbeat)
- If no messages sent for `sv_timeout` seconds, client is dropped
- Solution: Send periodic keep-alive if no other traffic

### Entity Overflow
- Too many entities in PVS can overflow message buffer (MAX_MSG_SIZE)
- Server will drop entities beyond limit
- Console warning: "overflow: too many entities"
- Solution: Reduce entity count or increase MAX_MSG_SIZE (protocol change)

### Sequence Number Wraparound
- Sequence numbers are 32-bit, wrap after ~4 billion packets
- At 60 Hz, takes ~2 years of continuous connection
- Handled automatically by netchan (uses modulo arithmetic)

## Security Notes

- **No encryption**: All traffic is plaintext (intentional - low latency)
- **Challenge/response**: Prevents IP spoofing during connection
- **Rate limiting**: Server rate-limits commands to prevent spam
- **Admin password**: Rcon commands require password authentication

## Integration Points

### Server (src/server/)
- `SV_SendClientMessages()` - Flush netchan messages to all clients
- `SV_ReadPackets()` - Process incoming client packets
- `SV_Frame()` - Main server frame, sends entity snapshots

### Client (src/client/)
- `CL_SendCommand()` - Send user input to server
- `CL_ReadPackets()` - Process incoming server packets
- `CL_ParseServerMessage()` - Parse server commands

### Game Module
Game module doesn't directly use network layer - server/client handle all network I/O. Game only sees high-level entity states and events.
