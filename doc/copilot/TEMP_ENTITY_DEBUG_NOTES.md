# Temp Entity Bug - Investigation Summary

**Date:** November 29, 2025  
**Status:** 🔍 Under investigation - needs debugging

---

## Symptom

Temp entities not working:
- No explosion sounds/effects (rockets, grenades)
- No muzzle flash sounds
- Missing all temp entity effects

---

## What We Know

### Temp Entity Flow:

1. **Server creates temp entity:**
   ```c
   // Example: g_ballistics.c line 370
   gi.WriteByte(SV_CMD_TEMP_ENTITY);
   gi.WriteByte(TE_EXPLOSION);
   gi.WritePosition(ent->s.origin);
   gi.WriteDir(Vec3_Up());
   gi.Multicast(ent->s.origin, MULTICAST_PHS);
   ```

2. **`gi.WriteByte` → `Sv_WriteByte` → writes to `sv.multicast` buffer**

3. **`gi.Multicast` → `Sv_Multicast()` in sv_send.c:**
   - Loops through all clients
   - Checks if client should receive message
   - Writes `sv.multicast` to client's message buffer
   - Clears `sv.multicast`

4. **Client receives:**
   - `Cg_ParseMessage()` dispatches `SV_CMD_TEMP_ENTITY`
   - `Cg_ParseTempEntity()` reads and creates effects

### Changes in Buggy Commits:

**sv_send.c (`Sv_Multicast`):**
- Changed from `cl->entity->client->ai` to `svs.game->clients[j]->ai`
- Removed third `filter` parameter (unused, not the problem)
- `svs.game->clients[]` is pre-allocated, guaranteed non-NULL

**sv_client.c:**
- Removed sending `client_num` during connection setup
- Client no longer reads or stores its client number

**Muzzle flash sending (g_weapon.c):**
- Changed from `(ptrdiff_t)(ent - g_game.entities)` to `ent->s.number`
- Removed extra byte for `MZ_BLASTER` client color
- Client parsing updated to match (no protocol mismatch)

---

## What We've Ruled Out

❌ **NULL pointer in `svs.game->clients[j]`** - They're pre-allocated  
❌ **Protocol mismatch** - Server/client in same process, same definitions  
❌ **Muzzle flash byte mismatch** - Client parsing matches server sending  
❌ **Client array indexing** - `j` correctly indexes both `svs.clients` and `svs.game->clients`

---

## Debugging Strategy

### Key Questions:

1. **Is `Sv_Multicast()` being called at all?**
   - Set breakpoint in `Sv_Multicast()` when firing weapon
   - Check if `sv.multicast.size > 0`

2. **Is the loop iterating correctly?**
   - Check `sv_max_clients->integer` value
   - Check `cl->state` for client 0
   - Check `svs.game->clients[0]->ai` value

3. **Is data being written to client buffers?**
   - Set breakpoint at line 227/229 (write to buffer)
   - Check if `sv.multicast.data` contains valid data
   - Check if it reaches `Mem_WriteBuffer()` or `Sv_ClientDatagramMessage()`

4. **Are messages reaching the client?**
   - Set breakpoint in `Cg_ParseMessage()` on client side
   - Check if `SV_CMD_TEMP_ENTITY` or `SV_CMD_MUZZLE_FLASH` ever arrive

### Suspected Issues:

**Most Likely:**
- `svs.game->clients[j]->ai` might be true when it shouldn't be
- Or accessing wrong memory/uninitialized data
- Loop might be skipping all clients for some reason

**Possible:**
- `sv.multicast` buffer not being populated correctly
- Messages being written but not transmitted
- Client parsing broken in a way we haven't seen

---

## Files to Focus On

**Server Side:**
- `src/server/sv_send.c` - Multicast loop (line 207-234)
- `src/server/sv_game.c` - Game interface setup
- `src/game/default/g_main.c` - Client array initialization

**Client Side:**
- `src/client/cl_parse.c` - Message parsing
- `src/cgame/default/cg_main.c` - CGa message dispatch
- `src/cgame/default/cg_temp_entity.c` - Temp entity creation
- `src/cgame/default/cg_muzzle_flash.c` - Muzzle flash creation

---

## Breakpoint Suggestions

### Server:
```
(lldb) b sv_send.c:189  // Sv_Multicast entry
(lldb) b sv_send.c:218  // AI check
(lldb) b sv_send.c:227  // Reliable write
(lldb) b sv_send.c:229  // Unreliable write
```

### Client:
```
(lldb) b cg_main.c:346  // Cg_ParseMuzzleFlash
(lldb) b cg_temp_entity.c:1319  // TE_EXPLOSION case
```

### Inspect:
```
(lldb) p sv.multicast.size
(lldb) p sv.multicast.data[0]@10
(lldb) p cl->state
(lldb) p svs.game->clients[0]->ai
(lldb) p svs.game->clients[0]->ps.client
```

---

## Next Steps

1. Set breakpoints in `Sv_Multicast()` 
2. Fire weapon in-game
3. Check if function is called
4. If yes: trace through loop to see why clients aren't receiving
5. If no: check if temp entity authoring code is being called

Good luck with the debugging! 🔍

---

**Investigation Status:** Waiting for debugger insights to identify root cause.
