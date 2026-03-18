# Entity Events Bug Fix

**Date:** November 29, 2025  
**File:** `src/cgame/default/cg_entity.c`  
**Bug:** Entity events (muzzle flashes, pickup sounds, jump/land sounds) not firing

---

## Symptom

Single-frame entity events were not being processed after the entity state management refactor. Events like:
- Muzzle flashes
- Item pickup sounds
- Jump and landing sounds
- Footstep sounds
- Teleport effects
- Item respawn effects

---

## Root Cause

The bug was introduced by our previous fix for the entity state management issue. Here's what happened:

### The Problem Chain:

1. **Original Intent:** `Cg_EntityEvent()` clears events after processing:
   ```c
   void Cg_EntityEvent(cl_entity_t *ent) {
     entity_state_t *s = &ent->current;
     // ... process event (muzzle flash, sounds, etc.) ...
     s->event = 0;  // Clear so it doesn't repeat
   }
   ```

2. **Call Order:**
   - `Cg_Interpolate()` is called first → processes events → clears `ent->current.event = 0`
   - `Cg_AddEntities()` is called later → renders entities

3. **Our Previous Bug Fix:** To fix stale entity data, we added:
   ```c
   // In Cg_AddEntities()
   if (ent->current.number != s->number || ent->current.spawn_id != s->spawn_id) {
     ent->current = *s;  // Copy fresh state from circular buffer
   }
   ```

4. **The New Bug:**
   - The circular buffer `entity_states[snum]` still contains the **original event value**
   - When we detect a spawn_id mismatch and do `ent->current = *s`, we **restore the event**!
   - The event that was already processed and cleared in `Cg_Interpolate()` comes back
   - This prevents events from being processed again in the next frame (they should only fire once)

### Why Events Were Lost:

Events are **single-frame fields** - they should fire once and be cleared. But our fix was inadvertently restoring them from the circular buffer, causing them to:
- Either fire multiple times (if the restoration happened on the same frame)
- Or never fire at all (if they were cleared and then immediately restored before rendering)

---

## The Fix

Clear the event in **both** locations:
1. In `ent->current` (done by `Cg_EntityEvent()`)
2. In the circular buffer `entity_states[]` (new fix)

### Code Change:

```c
void Cg_Interpolate(const cl_frame_t *frame) {

  cgi.client->entity = Cg_Self();

  for (int32_t i = 0; i < frame->num_entities; i++) {

    const uint32_t snum = (frame->entity_state + i) & ENTITY_STATE_MASK;
    entity_state_t *s = &cgi.client->entity_states[snum];  // Changed from const!

    cl_entity_t *ent = &cgi.client->entities[s->number];

    Cg_EntitySound(ent);

    Cg_EntityEvent(ent);  // Clears ent->current.event
    
    // NEW: Also clear the event in the circular buffer to prevent it from being restored
    // by the spawn_id validation in Cg_AddEntities()
    s->event = 0;
  }
}
```

### What Changed:

1. **Made `s` non-const** - Changed `const entity_state_t *s` to `entity_state_t *s`
2. **Added event clearing** - Added `s->event = 0;` after `Cg_EntityEvent()`
3. **Added comment** - Explained why we're clearing the circular buffer too

---

## Why This Works

### Event Lifecycle (Corrected):

1. **Server sends event** → Event is in network packet
2. **Client parses packet** → Event goes into `entity_states[]` circular buffer
3. **`Cl_ReadDeltaEntity()`** → Copies to `ent->current`
4. **`Cg_Interpolate()`** → 
   - Processes event (plays sound, shows effect)
   - Clears `ent->current.event = 0`
   - **NEW:** Also clears `entity_states[snum].event = 0`
5. **`Cg_AddEntities()`** →
   - Validates spawn_id and may copy from circular buffer
   - **But event is already cleared in both places now!**
   - No accidental event restoration

### Why Both Clears Are Needed:

- **Clear `ent->current.event`:** Prevents re-processing if entity isn't overwritten
- **Clear `entity_states[snum].event`:** Prevents restoration when our spawn_id fix copies `*s`

---

## Similar Pattern: Entity Sounds

Entity sounds work correctly because they use the same pattern:

```c
static void Cg_EntitySound(cl_entity_t *ent) {
  entity_state_t *s = &ent->current;
  
  if (s->sound) {
    Cg_AddSample(...);
  }
  
  s->sound = 0;  // Clear after processing
}
```

But sounds don't need the circular buffer clear because they're processed differently. Events are special because they're **truly single-frame** - they must fire exactly once.

---

## Testing

### What Should Work Now:

- ✅ Muzzle flashes when firing weapons
- ✅ Item pickup sounds and effects
- ✅ Jump sounds (player jumping)
- ✅ Landing sounds (player landing after fall)
- ✅ Footstep sounds while walking
- ✅ Teleport effects
- ✅ Item respawn effects
- ✅ Player drowning/gurp/sizzle sounds
- ✅ Fall damage sounds

### How to Test:

1. **Muzzle flashes:** Fire weapons, should see flash every shot
2. **Pickup sounds:** Pick up items, should hear sound each time
3. **Jump/land:** Jump around, should hear both jump and landing sounds
4. **Footsteps:** Walk/run, should hear footsteps
5. **Teleport:** Use teleporter, should see/hear effect

---

## Code Location

**File:** `src/cgame/default/cg_entity.c`  
**Function:** `Cg_Interpolate()`  
**Lines Changed:** ~3 lines (changed const, added clear, added comment)

---

## Related Code

### Entity Event Processing:

**File:** `src/cgame/default/cg_entity_event.c`  
**Function:** `Cg_EntityEvent()`

Handles all entity events:
- `EV_CLIENT_JUMP` - Jump sound
- `EV_CLIENT_LAND` - Landing sound
- `EV_CLIENT_FALL` - Fall damage sound
- `EV_CLIENT_FOOTSTEP` - Footstep sound
- `EV_ITEM_PICKUP` - Item pickup effect
- `EV_ITEM_RESPAWN` - Item respawn effect
- etc.

Each event plays sounds and/or shows visual effects.

---

## Why This Bug Was Subtle

1. **Indirect interaction:** The bug was caused by the interaction between TWO fixes:
   - The spawn_id validation (our previous bug fix)
   - The event clearing (existing code)

2. **Timing dependent:** Events would only be lost if:
   - Entity was in the frame
   - Spawn_id validation triggered a copy
   - Copy happened after event processing

3. **Not always reproducible:** Depending on timing and which entities were in the frame, events might work sometimes and fail other times

4. **Similar existing code:** The `s->sound = 0` clearing already existed, but events needed the circular buffer clear too

---

## Summary

Fixed entity events by ensuring they're cleared in both locations:
- ✅ `ent->current.event` (existing)
- ✅ `entity_states[snum].event` (new)

This prevents our spawn_id validation fix from accidentally restoring cleared events from the circular buffer.

**Status:** All entity events (muzzle flashes, sounds, effects) should now work correctly!
