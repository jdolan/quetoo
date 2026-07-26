# How to add a weapon

This guide explains how weapons fit together in Quetoo's default game module
and walks through a small mode-owned weapon. It assumes you can compile the
project and know basic C.

Read [how-to-add-game-mode.md](how-to-add-game-mode.md) first if you have not
registered a mode or modifier yet.

## Choose the weapon's ownership

There are two valid ways to add a weapon.

### Mode-owned weapon

Choose this for a weapon that only makes sense in one mode or modifier.

- It is declared in that module's `g_mode_item_def_t` array.
- It receives a runtime inventory tag.
- It is present only while its owner is active.
- Removing the mode also removes the weapon.
- It does not consume a permanent `g_item_tag_t` enum value.

This is the recommended route for mod experiments and mode-specific
loadouts.

### Built-in weapon

Choose this when the weapon is part of the default catalog and should be
available independently of any mode.

- It has a permanent tag in `bg_item.h`.
- Its shared presentation record lives in `bg_item.c`.
- It is initialized during game startup.
- Maps and every mode may refer to it.

The complete built-in checklist is later in this guide.

## How a weapon works

A server-side weapon is a `g_item_t` whose definition has
`type = ITEM_TYPE_WEAPON`.

The important parts are:

| Part | Responsibility |
|---|---|
| `g_item_def_t` | Name, classname, model, icon, ammo, flags, and AI priority. |
| `Pickup` | Add the weapon and initial ammunition to inventory. |
| `Use` | Select the weapon. |
| `Drop` | Create a dropped weapon entity, if allowed. |
| `Think` | Run the active weapon each client frame and fire when requested. |
| Projectile or trace helper | Produce the actual attack. |
| `G_FireWeapon` | Check attack input, refire time, and ammunition. |
| `G_WeaponFired` | Apply animation, timing modifiers, refire time, and ammo use. |
| `G_MuzzleFlash` | Send a standard muzzle-flash event to clients. |

The `Think` callback is the actual weapon implementation. Avoid adding a
weapon-tag switch to `G_ClientWeaponThink`; it already calls
`cl->weapon->Think`.

## Tutorial: a mode-owned Pulse Blaster

This example reuses existing Blaster assets and projectile behavior, but has
its own name, tuning, and runtime inventory tag. Reusing existing effects
keeps the first example focused on the extension mechanism.

The code can live in a small mode's main file. If the weapon grows, put it in
`g_mode_<name>_weapon.c` and expose only its item descriptor or resolver
through a private mode header.

### Declare balance cvars

Add these declarations to the owning mode's cvar array:

```c
static const g_mode_cvar_def_t g_pulse_cvars[] = {
  {
    .name = "g_pulse_damage",
    .default_value = "20",
    .flags = 0,
    .description = "Pulse Blaster damage.",
  },
  {
    .name = "g_pulse_knockback",
    .default_value = "4",
    .flags = 0,
    .description = "Pulse Blaster knockback.",
  },
  {
    .name = "g_pulse_speed",
    .default_value = "2200",
    .flags = 0,
    .description = "Pulse Blaster projectile speed.",
  },
  {
    .name = "g_pulse_refire",
    .default_value = "0.3",
    .flags = 0,
    .description = "Pulse Blaster refire interval in seconds.",
  },
};
```

The mode runtime registers these cvars. Read them when firing rather than
keeping mutable `cvar_t *` globals in the weapon file.

### Implement the fire callback

```c
static void G_FirePulseBlaster(g_client_t *cl) {
  if (!G_FireWeapon(cl)) {
    return;
  }

  const cvar_t *damage = gi.GetCvar("g_pulse_damage");
  const cvar_t *knockback = gi.GetCvar("g_pulse_knockback");
  const cvar_t *speed = gi.GetCvar("g_pulse_speed");
  const cvar_t *refire = gi.GetCvar("g_pulse_refire");
  if (!damage || !knockback || !speed || !refire) {
    return;
  }

  vec3_t forward, right, up, origin;
  G_ClientProjectile(cl, &forward, &right, &up, &origin, 1.f);

  G_BlasterProjectile(cl->entity, origin, forward,
                      speed->integer, damage->integer,
                      knockback->integer);

  G_MuzzleFlash(cl->entity, MZ_BLASTER);
  G_WeaponFired(cl, SECONDS_TO_MILLIS(refire->value),
                cl->weapon->def.quantity);
}
```

The order is important:

1. `G_FireWeapon` rejects early or invalid firing attempts.
2. The attack is created.
3. Presentation is published.
4. `G_WeaponFired` applies common bookkeeping.

Do not decrement ammunition manually if you call `G_WeaponFired`.

`G_WeaponFired` also passes the interval through active mode modifiers. A Tech
such as Haste can therefore affect this weapon without calling it directly.

### Define the weapon prototype

```c
static const g_item_t g_pulse_blaster_prototype = {
  .def = {
    .classname = "weapon_pulse_blaster",
    .pickup_sound = "weapons/common/pickup.wav",
    .model = "models/weapons/blaster/tris.md3",
    .effects = EF_ROTATE | EF_BOB,
    .icon = "pics/w_blaster",
    .name = "Pulse Blaster",
    .quantity = 0,
    .type = ITEM_TYPE_WEAPON,
    .flags = WF_PROJECTILE | WF_MED_RANGE,
    .priority = 0.35f,
    .precaches = "weapons/blaster/fire.wav",
    .effect_color = { { 0.2f, 0.6f, 1.f, 1.f } },
  },
  .Pickup = G_PickupWeapon,
  .Use = G_UseWeapon,
  .Think = G_FirePulseBlaster,
};
```

This weapon uses no ammunition, so `quantity` and `ammo` are zero and `Drop`
is omitted. For an ammunition weapon:

- set `quantity` to ammunition consumed per shot;
- set `ammo` to the ammunition item's runtime tag; and
- set `.Drop = G_DropWeapon`.

A mode-owned dynamic ammunition item must be resolved with `ResolveAmmo`
after runtime tags have been assigned. See
[how-to-add-mode-item.md](how-to-add-mode-item.md).

### Register the weapon with the mode

```c
static const g_item_t *G_PulseBlasterPrototype(g_mode_t *mode) {
  (void) mode;
  return &g_pulse_blaster_prototype;
}

static const g_mode_item_def_t g_pulse_items[] = {
  {
    .classname = "weapon_pulse_blaster",
    .Resolve = G_PulseBlasterPrototype,
    .dynamic = true,
  },
};
```

Add the arrays to the mode descriptor:

```c
.cvars = g_pulse_cvars,
.num_cvars = lengthof(g_pulse_cvars),
.items = g_pulse_items,
.num_items = lengthof(g_pulse_items),
```

Do not set `.def.tag` in the prototype. The runtime assigns a free tag between
`ITEM_TOTAL` and `MAX_INVENTORY`, copies the completed prototype into
`g_items`, and publishes the name, classname, icon, model, type, quantity,
ammo, and effect color to cgame.

### Put the weapon in the game

A mapper can place the classname directly:

```text
{
  "classname" "weapon_pulse_blaster"
  "origin" "128 64 32"
}
```

While the owning mode is active, the generic entity loader resolves the
registered item and calls `G_SpawnItem`. No separate entity-class descriptor
is required for an ordinary item entity.

To give it as starting equipment:

```c
static bool G_PulseClientInventory(g_mode_t *mode, g_client_t *cl,
                                   const g_item_t **starting_weapon) {
  (void) mode;

  G_Give(cl, "Pulse Blaster", 1);
  *starting_weapon = G_FindItem("Pulse Blaster");
  return *starting_weapon != NULL;
}
```

Returning true from `ClientInventory` means this module has supplied the
starting inventory. A modifier can do this as well as a primary mode, but the
first successful handler wins.

### Access owning mode state from the weapon

A weapon callback receives only `g_client_t *`. If it needs mode-wide or
per-client private data, recover its owner from the active weapon:

```c
g_mode_t *mode = G_ModeForItem(cl->weapon);
void *state = G_ModeState(mode);
```

`G_ModeForItem` works whether the owner is the primary mode or a modifier. It
avoids hard-coding registry position or assuming that every dynamic weapon
belongs to the primary mode.

## Adding a built-in weapon

Use this path only when the weapon is intended to remain in the common item
catalog.

### 1. Add the permanent tag

Add a value before `WEAPON_LAST` in `g_item_tag_t` in `bg_item.h`:

```c
WEAPON_PULSE_BLASTER,
WEAPON_LAST,
```

`bg_item_defs[tag]` must remain indexed by exactly that tag. Do not reorder
existing tags casually; tags are used in inventory and network state.

### 2. Add shared item metadata

Add a matching entry at the same position in `bg_item.c`. Supply at least:

- classname;
- pickup sound;
- world/view model;
- icon;
- display name;
- quantity and ammo;
- `ITEM_TYPE_WEAPON`;
- the new tag;
- weapon flags;
- AI priority; and
- precache assets.

### 3. Implement and connect its `Think`

Declare the fire function in `g_weapon.h` and implement it in `g_weapon.c`.
Use the same `G_FireWeapon` and `G_WeaponFired` sequence shown above.

The built-in initializer in `g_item.c` currently maps classnames to weapon
`Think` callbacks. Add the new classname there:

```c
} else if (!q_strcmp(it->def.classname, "weapon_pulse_blaster")) {
  it->Think = G_FirePulseBlaster;
}
```

The initializer already assigns the common pickup, use, and drop callbacks
for `ITEM_TYPE_WEAPON`.

### 4. Add balance configuration

Common built-in balance cvars are declared in `g_main.h`, defined near the top
of `g_main.c`, and registered in `G_Init`. A mode-owned weapon should instead
declare its cvars in its mode descriptor.

### 5. Review item-set mappings

If the weapon participates in the Quetoo-versus-Quake item replacement
system, update `g_quake_weapon_map` in `g_item.c`.

### 6. Review protocol-visible effects

Reusing an existing projectile, means of death, and muzzle flash needs no new
protocol enum. A genuinely new effect may require coordinated changes:

- `g_means_of_death` and obituary/score handling;
- `g_muzzle_flash_t` and `cg_muzzle_flash.c`;
- projectile entity events or trails;
- shared asset precaching; and
- any cgame effect switch that consumes the new value.

Game and cgame must agree on shared enum values.

## AI behavior

Bots use the definition rather than a weapon-name switch for most selection:

- `priority` compares the weapon with alternatives;
- `WF_HITSCAN` or `WF_PROJECTILE` describes aiming;
- range flags describe appropriate engagement distance;
- `WF_EXPLOSIVE` and `WF_TIMED` describe special risks; and
- the ammo tag and quantity determine whether it can fire.

Test bots explicitly. A weapon may compile and work for humans while having
poor priority or range metadata.

## Assets and presentation

The item model is used for both the world pickup and first-person weapon in
the current runtime catalog. The icon is used by pickup messages, inventory,
HUD, and weapon selection.

For a mode-owned weapon, the runtime publishes presentation metadata through
`CS_MODE_ITEMS`. You normally do not add its tag to a cgame switch.

New muzzle flashes, trails, sounds, or animations still need cgame support
when no existing effect describes them.

## Testing checklist

1. Build with the weapon's owner enabled and disabled.
2. Spawn the map entity and pick it up.
3. Give it through `G_Give`.
4. Select it from the weapon selector.
5. Fire with enough ammunition, insufficient ammunition, and no ammunition.
6. Verify refire timing and active modifiers such as Haste.
7. Drop it, die while holding it, and pick up the dropped copy if supported.
8. Check first-person model, world model, icon, pickup sound, and muzzle flash.
9. Test bots at short and long range.
10. Change maps and verify no runtime tag or item pointer survives teardown.

## Common mistakes

### The weapon can be picked up but cannot fire

Its `Think` callback is `NULL`. A dynamic prototype must provide it. A
built-in weapon must be connected by the initializer in `g_item.c`.

### The weapon fires without consuming ammunition

Its `ammo` tag or `quantity` is zero, or the fire callback skipped
`G_WeaponFired`.

### The weapon consumes ammunition twice

The callback decremented inventory manually and also called
`G_WeaponFired`.

### It works in game but has no HUD icon or first-person model

The prototype omitted `icon` or `model`, the asset path is wrong, or a
mode-owned item was not marked `dynamic`.

### A mode-owned weapon changes tag after a map load

That is expected. Runtime tags are per composition and level. Keep
`g_item_t *` references only for the current level and never use a dynamic tag
as a permanent semantic ID.

### Removing a mode leaves weapon-specific code behind

The weapon was added to the built-in enum or common switches even though it
was mode-specific. Prefer a dynamic descriptor and keep its callbacks in the
mode's files.

## Related documentation

- [How to add a game mode or modifier](how-to-add-game-mode.md)
- [How to add a mode-specific item](how-to-add-mode-item.md)
- [Game-mode refactor PR overview](game-modes-refactor-pr.md)
