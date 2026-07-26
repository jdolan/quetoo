# How to add a mode-specific item

This guide adds an item that exists only while its owning mode or modifier is
active. It assumes you can compile Quetoo, know basic C, and have already read
[how-to-add-game-mode.md](how-to-add-game-mode.md).

The example is a Tag Beacon. Picking it up makes that player the single “it”
player stored in the Tag mode's mode-wide state.

## Why use a mode-owned item?

Use a mode-owned item when it does not make sense outside its owner:

- an objective token;
- a Tag beacon;
- a mode-specific key;
- a round-only powerup;
- custom ammunition; or
- a weapon used by only one mode.

The item descriptor, callbacks, map classname, and runtime tag all belong to
the active mode instance. Removing the module removes the item registration.

Do not add such an item permanently to `g_item_tag_t` or `bg_item_defs`.

## Built-in alias or dynamic item?

A mode item descriptor supports two forms.

### Alias

An alias exposes an existing catalog item under a mode-owned classname or
registration:

```c
static const g_item_t *G_TagResolveQuad(g_mode_t *mode) {
  const g_mode_context_t *context = G_ModeContext(mode);
  return context && context->items ?
      &context->items[POWERUP_QUAD] : NULL;
}

static const g_mode_item_def_t g_tag_items[] = {
  {
    .classname = "item_tag_quad",
    .Resolve = G_TagResolveQuad,
  },
};
```

`dynamic` defaults to false. The item keeps its built-in tag and callbacks.

### Dynamic item

A dynamic descriptor clones a complete `g_item_t` prototype and assigns a
runtime tag. Use it for a distinct name, behavior, presentation, or item
category.

The rest of this guide uses a dynamic item.

## Keep shared Tag state in a private header

If the Tag rules and Tag item are in separate C files, put only the
cross-file private declarations in `g_mode_tag.h`:

For one short item, it is also fine to keep this code in `g_mode_tag.c` and
skip the private header. Split it when the item implementation is large or
when the mode has many small item implementations.

```c
#pragma once

#include "g_local.h"

typedef struct {
  g_client_t *it;
  uint32_t changes;
} g_tag_state_t;

static inline g_tag_state_t *G_TagState(g_mode_t *mode) {
  return G_ModeState(mode);
}

#define G_TAG_ITEM_COUNT 1

extern const g_mode_item_def_t g_tag_items[G_TAG_ITEM_COUNT];
```

This is a private mode header, not a common game interface. Other modes should
not include it.

The Tag descriptor requests one shared record:

```c
.state_size = sizeof(g_tag_state_t),
```

There is one `g_tag_state_t` for the active Tag instance, so there can be only
one authoritative `it` pointer.

## Implement the pickup callback

Create `g_mode_tag_item.c`:

```c
#include "g_mode_tag.h"

static bool G_TagBeaconPickup(g_client_t *cl, g_entity_t *ent) {
  if (!cl || !ent || !ent->item) {
    return false;
  }

  g_mode_t *mode = G_ModeForItem(ent->item);
  g_tag_state_t *state = G_TagState(mode);
  if (!mode || !state || state->it == cl) {
    return false;
  }

  state->it = cl;
  state->changes++;

  gi.BroadcastPrint(PRINT_HIGH, "%s is it!\n",
                    cl->persistent.net_name);

  if (!(ent->spawn_flags & SF_ITEM_DROPPED)) {
    G_SetItemRespawn(ent, SECONDS_TO_MILLIS(15.f));
  }

  return true;
}
```

Item callbacks receive the client and item entity, not a `g_mode_t *`.
`G_ModeForItem` finds whichever active primary mode or modifier owns the
dynamic item. The callback can then access its owner's mode-wide, client, or
entity data without assuming composition order.

Returning true tells `G_TouchItem` to:

- show the pickup name and icon;
- play the pickup sound;
- emit the pickup event; and
- free a dropped item.

The callback owns the actual rule change. Generic item code does not know what
“it” means.

## Define the complete item prototype

Continue in `g_mode_tag_item.c`:

```c
static const g_item_t g_tag_beacon_prototype = {
  .def = {
    .classname = "item_tag_beacon",
    .pickup_sound = "powerups/quad/pickup.wav",
    .model = "models/powerups/quad/tris.obj",
    .effects = EF_ROTATE | EF_BOB | EF_LIGHT | EF_LIGHT_PULSE,
    .icon = "pics/i_quad",
    .name = "Tag Beacon",
    .quantity = 1,
    .type = ITEM_TYPE_POWERUP,
    .priority = 0.9f,
    .precaches = "",
    .light_color = { 1.f, 0.3f, 0.1f },
    .light_radius = 120.f,
    .effect_color = { { 1.f, 0.3f, 0.1f, 1.f } },
  },
  .Pickup = G_TagBeaconPickup,
};

static const g_item_t *G_TagBeaconPrototype(g_mode_t *mode) {
  (void) mode;
  return &g_tag_beacon_prototype;
}

const g_mode_item_def_t g_tag_items[G_TAG_ITEM_COUNT] = {
  {
    .classname = "item_tag_beacon",
    .Resolve = G_TagBeaconPrototype,
    .dynamic = true,
  },
};
```

A dynamic prototype must contain all callbacks it needs. The built-in
classname initializer is deliberately not rerun over a completed mode
prototype because doing so would erase custom callbacks.

The example reuses existing assets. Replace them with mode assets when those
assets are installed and precached.

## Register the item with the mode

Include the private header in the Tag mode file and add:

```c
static const g_mode_def_t g_tag_mode = {
  .name = "tag",
  .kind = G_MODE_PRIMARY,
  .state_size = sizeof(g_tag_state_t),
  .items = g_tag_items,
  .num_items = G_TAG_ITEM_COUNT,
  .ops = &g_tag_ops,
};
```

Add `g_mode_tag_item.c` to `game_la_SOURCES` in `Makefile.am`.

The descriptor owns the registration. Do not also add
`item_tag_beacon` to the common entity-class or built-in item tables.

## Place or spawn the item

### Place it in a map

A mapper can use the registered classname:

```text
{
  "classname" "item_tag_beacon"
  "origin" "256 64 48"
}
```

When Tag is active, the generic map entity path resolves the mode item and
calls `G_SpawnItem`. When Tag is not active, that classname has no item
registration.

### Spawn it from mode code

For procedural placement:

```c
static g_entity_t *G_TagSpawnBeacon(const vec3_t origin) {
  const g_item_t *item = G_FindItemByClassName("item_tag_beacon");
  if (!item) {
    return NULL;
  }

  g_entity_t *ent = G_AllocEntity(item->def.classname);
  ent->s.origin = origin;
  G_SpawnItem(ent, item);
  return ent;
}
```

Call this after the placement information it needs is ready. Worldspawn
activates modes before the rest of the map entity list has finished loading,
so a `Frame` or `ResetItems` hook is often a better procedural spawn point
than `LevelBegin`.

Do not cache the returned `g_item_t *` or entity pointer across map changes.

## Inventory-backed mode items

The Tag Beacon changes mode state immediately and does not need to remain in
inventory. If an item should be carried:

```c
cl->inventory[ent->item->def.tag]++;
```

Use `ent->item->def.tag`, not a hard-coded number. Dynamic tags are allocated
for the active composition and may change on the next map.

Put persistent mode behavior in mode client data even if an inventory count
is also useful for HUD or selection. For example:

```c
typedef struct {
  g_mode_client_t base;
  uint32_t beacon_expires_at;
} g_tag_client_t;
```

The inventory answers “does the client carry this runtime item?” The
mode-client record answers “what does that mean for Tag?”

## Droppable items

Set the prototype's `Drop` callback when players may drop it:

```c
.Drop = G_DropItem,
```

The callback that invokes `G_DropItem` must also remove or update the client's
inventory. `G_DropItem` creates the world entity but intentionally does not
change inventory counts.

Dropped items use a different construction path from map items. Test both.
Any team color, semantic owner, or special entity metadata must be initialized
for both paths.

## Mode-owned ammunition

A dynamic weapon and dynamic ammunition are assigned tags during activation.
The weapon cannot hard-code the ammunition tag.

Register both descriptors and provide `ResolveAmmo` for the weapon:

```c
static const g_item_t *G_TagResolveRuntimeAmmo(g_mode_t *mode) {
  for (size_t i = 0; i < mode->def->num_items; i++) {
    if (!q_strcmp(mode->def->items[i].classname, "ammo_tag_cells") &&
        mode->item_data[i].def.tag > ITEM_NONE) {
      return &G_ModeContext(mode)->items[mode->item_data[i].def.tag];
    }
  }
  return NULL;
}

static const g_mode_item_def_t g_tag_items[] = {
  {
    .classname = "ammo_tag_cells",
    .Resolve = G_TagAmmoPrototype,
    .dynamic = true,
  },
  {
    .classname = "weapon_tag_launcher",
    .Resolve = G_TagLauncherPrototype,
    .ResolveAmmo = G_TagResolveRuntimeAmmo,
    .dynamic = true,
  },
};
```

`ResolveAmmo` runs after all dynamic items have tags. The runtime writes the
resolved tag into the cloned weapon definition before publishing the catalog.

For a larger item collection, add a small private lookup helper instead of
repeating descriptor scans.

## Per-entity item data

If every spawned item entity needs private data, request a mode entity AoS
record:

```c
typedef struct {
  g_mode_entity_t base;
  uint32_t activated_at;
  g_client_t *last_owner;
} g_tag_entity_t;
```

Set `.entity_data_size`, put `g_mode_entity_t` first, and validate its entity
pointer and spawn ID when reading it. This is safer than adding Tag-only
fields to `g_entity_t`.

Remember that item entity slots are reused. Item identity belongs in the
mode's entity record or item definition, not in assumptions about entity
numbers or runtime item tags.

## Client presentation

For every dynamic item, the server publishes:

- display name;
- classname;
- icon;
- model;
- item type;
- quantity;
- ammunition tag; and
- effect color.

Cgame stores these records by runtime tag. Pickup messages, icons, item
effects, inventory, and weapon presentation can therefore use dynamic items
without a permanent shared enum entry.

A new kind of presentation that is not described by these fields still needs
a versioned extension or coordinated cgame code. Do not overload the runtime
tag with semantic meaning.

## Bots

Dynamic items appear in the generic item catalog. Use:

- `priority` for their general desirability;
- `BotCanPickup` for a final eligibility decision;
- `BotTarget` for mode-specific target priority; and
- per-client mode data for persistent bot roles.

For the Tag Beacon, a bot that is already “it” should not pursue it:

```c
static bool G_TagBotCanPickup(g_mode_t *mode, const g_client_t *cl,
                              const g_entity_t *item, bool *can_pickup) {
  const g_tag_state_t *state = G_TagState(mode);
  if (!state || !cl || !item || !item->item ||
      q_strcmp(item->item->def.classname, "item_tag_beacon")) {
    return false;
  }

  *can_pickup = state->it != cl;
  return true;
}
```

Returning true means the mode made the final decision for that item.

## Removal checklist

To remove the item:

1. remove its descriptor from the mode;
2. remove its prototype and callbacks;
3. remove its source file from `Makefile.am`;
4. remove map placements or procedural spawn code;
5. remove assets used only by that item; and
6. build and test the mode without it.

There is no permanent item enum to renumber and no common spawn registration
to clean up.

## Testing checklist

1. Start a map with the owner disabled; the item should not be registered.
2. Start a map with the owner enabled.
3. Test map-placed and programmatically spawned copies.
4. Pick it up as an eligible and ineligible client.
5. Verify pickup sound, icon, model, effect color, and respawn.
6. Test dropped copies if supported.
7. Test bots' target and pickup policy.
8. Disconnect the client referenced by mode-wide state.
9. Change maps repeatedly and watch for stale item, entity, or client pointers.
10. Combine the owner with supported modifiers.

## Common mistakes

### The callback cannot find its mode

The item was not registered by the active descriptor, the callback passed the
wrong `g_item_t *`, or it tried `G_ModeActive` even though the owner is a
modifier. Use `G_ModeForItem(ent->item)`.

### The item appears but cannot be picked up

The dynamic prototype omitted `Pickup`, or the callback returned false.

### The pickup repeats immediately

A map item callback returned true without calling `G_SetItemRespawn`.

### The inventory index works on one map and fails on another

The code hard-coded or cached a dynamic runtime tag. Always read the tag from
the active item definition.

### Several clients appear to be “it”

The unique relationship was duplicated into per-client booleans. Keep the
single authoritative pointer in `G_ModeState` and derive per-client
presentation from it.

## Related documentation

- [How to add a game mode or modifier](how-to-add-game-mode.md)
- [How to add a weapon](how-to-add-weapon.md)
- [Game-mode refactor PR overview](game-modes-refactor-pr.md)
