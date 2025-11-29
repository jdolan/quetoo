# Entity State Management Bug Analysis

## Symptom
Random models (ammo boxes, armor) appearing attached to unrelated entities (blaster trails, particles).

## Root Cause Hypothesis

The issue is entity slot reuse with stale data. Here's the scenario:

1. Entity slot #5 is used by an ammo box with `model1 = MODEL_AMMO`
2. Ammo box is removed/freed
3. Entity slot #5 is immediately reused for a blaster projectile
4. Blaster projectile has `model1 = 0` (no model) and `trail = TRAIL_BLASTER`
5. **BUG:** On the client, `cl_entity_t.current` still contains stale `model1 = MODEL_AMMO` from the ammo box
6. When `Cg_AddEntity()` runs, it doesn't early-return because `model1 != 0`, so it renders the ammo box model
7. The trail renders correctly because it uses the fresh `entity_state_t` from the packet

## Why This Happens

The problem is likely in the entity state initialization when an entity slot is reused. Let me check the spawn_id handling...

Actually, I think the issue is in `Cl_ValidDeltaEntity()` line 61:

```c
if (ent->current.spawn_id != to->spawn_id) {
  return false;  // Delta invalid - entity was respawned
}
```

When this triggers (spawn_id changed), line 103 does:
```c
ent->prev = *to;
// ... reset animations, trails, etc
```

But then line 117 does:
```c
ent->current = *to;  // This SHOULD fix it
```

So `ent->current` SHOULD be updated... unless there's a race condition or the entity hasn't been parsed yet when `Cg_AddEntities()` runs?

## Testing Theory

Add debug logging to see:
1. What is `ent->current.model1` when `Cg_AddEntity()` is called for a blaster
2. What is `ent->current.spawn_id` vs the fresh `entity_state_t.spawn_id`
3. Whether `ent->frame_num` matches `cl.frame.frame_num`

## Potential Fix

The safest fix: In `Cg_AddEntities()`, don't trust `ent->current`, use the fresh `entity_state_t` instead:

```c
for (int32_t i = 0; i < frame->num_entities; i++) {
  const uint32_t snum = (frame->entity_state + i) & ENTITY_STATE_MASK;
  const entity_state_t *s = &cgi.client->entity_states[snum];
  cl_entity_t *ent = &cgi.client->entities[s->number];
  
  // VERIFY: Does ent->current match s?
  if (ent->current.number != s->number || ent->current.spawn_id != s->spawn_id) {
    // Stale data detected! Force update:
    ent->current = *s;
  }
  
  Cg_EntityTrail(ent);
  Cg_AddEntity(ent);
}
```

Or simpler: Just always use the fresh state for model checks:

```c
// In Cg_AddEntity(), use the fresh state from the frame:
if (!ent->current.model1) {  // ← This might be stale!
  return;
}
```

Should be:

```c
const entity_state_t *fresh_state = &cgi.client->entity_states[...]; // Get from frame
if (!fresh_state->model1) {  // ← Use fresh state
  return;
}
```

But we need to pass the fresh state to Cg_AddEntity()...
