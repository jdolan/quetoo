# Game-mode architecture: current state and mode-module refactor design

Status: working-tree source review (2026-07-25). This report describes the
default server game module in `src/game/default` and the mode-facing parts of
`src/cgame/default`. It is intended to guide an in-project refactor, not to
specify a clean-room replacement.

## Executive summary

Quetoo does not currently have one game-mode abstraction. A running ruleset is
the product of several independent values:

- `gameplay`: Deathmatch, Instagib, or Arena;
- `teams`: team scoring and team assignment;
- `ctf`: the capture objective (which also implies team behavior);
- `items`: Quetoo or Quake item roster;
- secondary switches such as hook, techs, friendly fire, limits, and number of
  teams.

This composability is useful and should be preserved: Instagib CTF and Arena
CTF are valid according to the implementation. The architectural problem is
that the composition has no explicit owner. Policy is expressed as repeated
tests of `g_level.gameplay`, `g_level.teams`, and `g_level.ctf` throughout
entity spawning, items, clients, combat, scoring, AI, commands, and the client
HUD.

For the refactor, prefer **removable mode modules assembled from reusable,
replaceable components**, not a larger enum containing every combination.
Each mode should expose one immutable definition and keep all mutable state in
an opaque instance owned by the mode runtime. The game core should invoke typed
lifecycle hooks and policy interfaces; it should not know that CTF, Arena, or
any other optional mode exists.

## 1. Current model

The gameplay enum has only three values
([`g_types.h`](../src/game/default/g_types.h#L361-L368)), while the effective
ruleset lives in fields on the global `g_level`
([`g_types.h`](../src/game/default/g_types.h#L763-L826)):

| Axis | Current representation | Actual responsibility |
|---|---|---|
| Combat/loadout | `g_level.gameplay` | Spawn inventory, world-item suppression, self-damage, weapon drops, rail damage |
| Team policy | `g_level.teams`, `num_teams` | Assignment, team spawn selection, friendly fire, aggregate scores |
| Objective | `g_level.ctf` | Flag availability/lifecycle, captures, capture-limit victory |
| Item family | `g_level.items` | Quetoo versus Quake inventory and pickup mapping |
| Optional mechanics | `hook`, `techs` | Derived switches, defaulting to enabled for CTF |
| Match limits | frag, capture, time | End-level conditions |

CTF overrides the standalone `teams` flag during initial map resolution, but
most consumers still test `teams || ctf`
([`g_entity.c`](../src/game/default/g_entity.c#L949-L953)). Therefore “teams”
means both a configuration input and, elsewhere, an effective capability.
This is a major source of coupling.

### Effective mode behavior

| Composition | Spawn inventory | World items | Victory metric |
|---|---|---|---|
| Deathmatch | Starter weapon | Normal | Individual frags |
| Team Deathmatch | Starter weapon | Normal | Team frags |
| CTF | Starter weapon | Normal, plus active-team flags | Team captures |
| Instagib, optionally team/CTF | Railgun and grenades | All non-flags hidden | Frags or captures |
| Arena, optionally team/CTF | All weapons and ammo from the map's active item set, plus its strongest armor | All non-flags hidden | Frags or captures |

Arena is currently a loadout/item/self-damage profile, not a round or
elimination controller. The worldspawn comment advertises `roundlimit`, but
there is no corresponding state or rule implementation
([`g_entity.c`](../src/game/default/g_entity.c#L805-L815)). A refactor should
either preserve and rename this behavior (for example, “full loadout”) or add
round semantics deliberately; it should not infer them from the current name.
The Arena modifier owns separate Quetoo and Quake loadouts and chooses between
them through the item set resolved from worldspawn.

## 2. Resolution and lifecycle

At map load, the server clears `g_level`, spawns BSP entities, builds spawn
point pools, resolves derived mechanics, resets teams/items, and loads AI
([`g_entity.c`](../src/game/default/g_entity.c#L669-L728)).

Configuration is resolved inside `G_worldspawn`, separately for each field
([`g_entity.c`](../src/game/default/g_entity.c#L841-L1015)). Precedence is not
uniform:

- gameplay: explicit `g_gameplay` cvar, then map-list metadata, then
  worldspawn, then Deathmatch;
- teams and CTF: map-list metadata, then worldspawn, then their cvars;
- item set: worldspawn only;
- limits: map-list metadata, then worldspawn, then cvars;
- hook and techs: their own cvar/map/worldspawn resolution functions, with CTF
  as the final default.

Runtime cvar changes are polled in `G_CheckRules`. Mode-affecting changes set a
restart flag; `G_RestartGame` clears player/team scores, reassigns teams,
respawns clients, and resets items/spawns
([`g_main.c`](../src/game/default/g_main.c#L410-L455),
[`g_main.c`](../src/game/default/g_main.c#L614-L685)). It is an effective
player/item reset boundary, but the active composed descriptors are not yet
rebuilt in place; a future explicit reconfiguration transaction should own
that transition. The current restart is also mixed with per-frame victory
checks and unrelated mutable cvars.

End conditions are straightforward:

- non-CTF matches use the frag limit, against team or individual scores;
- CTF suppresses frag-limit victory and uses team captures;
- the time limit applies to all compositions;
- intermission posts frag/capture statistics, freezes play, then advances the
  map after ten seconds.

## 3. CTF implementation

CTF supports two to four teams. Each static `g_team_t` contains presentation
data, class names for its flag and spawn points, scores, captures, spawn pools,
and a pointer to the base flag entity
([`g_types.h`](../src/game/default/g_types.h#L1001-L1062)).

Map initialization discovers flags and team spawns. If ordinary team spawns
are absent, it can synthesize a two-team layout from deathmatch spawns,
including red and blue flags; clients fall back to deathmatch spawns if a team
pool is unusable
([`g_entity.c`](../src/game/default/g_entity.c#L514-L617),
[`g_client.c`](../src/game/default/g_client.c#L971-L1001)).

### Implicit flag state machine

There is no flag-state object. State is distributed between the base entity,
a possible dropped entity, the carrier inventory, and carrier render fields:

```text
AT_BASE
  enemy touch -> CARRIED

CARRIED
  carrier death / disconnect / team change / drop -> DROPPED
  carrier touches own at-base flag              -> AT_BASE + capture

DROPPED
  teammate touch or 30-second timeout -> AT_BASE
  enemy touch                         -> CARRIED
```

On steal, the base flag becomes hidden/non-solid, the inventory flag slot is
set, and the carrier receives a linked model and team effect. On drop, those
carrier fields are cleared and a dropped item entity is created. On return or
capture, the base entity is restored. The transition logic, feedback, counters,
and stats recording were originally embedded in `g_item.c`; the first
implementation milestone moves that transition code to the CTF-owned module
([`g_mode_ctf.c`](../src/game/default/g_mode_ctf.c#L120-L290)) while retaining
the legacy item callback ABI.

Captures increment team and player capture counters but do not add frag score.
Team frag scores still accumulate during CTF and are shown, but do not end the
match. Death, disconnect, spectator/team changes, and explicit dropping all
route through the generic mode item-drop and reset services, which is an
important preservation invariant
([`g_client.c`](../src/game/default/g_client.c#L531-L553),
[`g_client.c`](../src/game/default/g_client.c#L1490-L1502)).

`G_ResetItem` keeps flags active in Instagib/Arena while hiding every non-flag
item, and hides flags outside CTF or beyond the active team count
([`g_item.c`](../src/game/default/g_item.c#L1025-L1060)). Bots still treat
flags as items, but their weight is now passed through the generic mode bot
directive seam; CTF supplies a private role and flag weight
([`g_mode_ctf.c`](../src/game/default/g_mode_ctf.c#L334-L358)). There is no
higher-level defend/escort objective yet.

## 4. Coupling and refactor risks

1. **No canonical effective ruleset.** Raw inputs, resolved state, and derived
   capabilities share the same globals. Invalid or redundant combinations can
   exist after runtime changes even though initial resolution normalizes some
   of them.

2. **Policy is distributed.** Mode branches occur in `g_main.c`, `g_entity.c`,
   `g_client.c`, `g_item.c`, `g_combat.c`, `g_cmd.c`, `g_client_stats.c`,
   weapon code, utility code, and AI. Adding a mode requires auditing all of
   them.

3. **CTF state has multiple sources of truth.** Base visibility/solidity,
   dropped entities, inventory, `model3`, and effect bits must agree. A failed
   transition can duplicate or lose a flag.

4. **Configuration semantics are surprising.** Precedence varies by field.
   The documented value `2` for team/CTF “auto-balance” is stored in a `bool`,
   so it only means enabled; assignment chooses the smallest team, but no
   ongoing balancing mode exists.

5. **Mode change is broad and implicit.** Cvar polling invokes a full restart,
   but ownership of validation, normalization, client notification, and reset
   is split among several functions.

6. **The client contract is fragmented.** Gameplay, CTF, team count, item set,
   and team data use separate configstrings, while scores use a binary
   `g_score_t` packet with a fixed `MAX_TEAMS` aggregate tail
   ([`g_client_stats.c`](../src/game/default/g_client_stats.c#L65-L123)).
   Notably, the server writes `CS_GAMEPLAY`, but
   `Cg_UpdateConfigString` currently has no `CS_GAMEPLAY` case
   ([`cg_main.c`](../src/cgame/default/cg_main.c#L245-L275)); consequently
   `cg_state.gameplay` appears to remain its zero-initialized Deathmatch value.

7. **There are no game-mode characterization tests.** The existing test suite
   has no coverage for mode resolution, victory rules, flag transitions, or
   restarts. Refactoring implicit behavior without such tests is high risk.

8. **Items and entity types are closed sets.** Entity spawn functions live in a
   static table in `g_entity.c`, and item behavior is assigned through a large
   type/classname switch
   ([`g_entity.c`](../src/game/default/g_entity.c#L30-L98),
   [`g_item.c`](../src/game/default/g_item.c#L1456-L1569)). Item tags are also
   compile-time array indices bounded by `ITEM_TOTAL`
   ([`bg_item.h`](../src/game/default/bg_item.h#L34-L148)). Moving CTF to its
   own file is therefore insufficient: flags and any future mode-specific
   entities would still leave hard dependencies in common code.

## 5. Refactor requirements

The proposed architecture is evaluated against these project goals:

- one source file or directory owns each mode's logic;
- modes export a common set of typed hooks;
- modes have no mutable file/global state;
- each active module gets fixed-capacity AoS entity and client-data slabs,
  allocated once during activation rather than piecemeal during play;
- team, spawn, damage, respawn, loadout, scoring, and similar policies can be
  reusable components;
- a mode can replace or decorate crucial policies such as damage and respawn;
- mode-specific cvar declarations live with the mode;
- AI consumes mode-provided goals and weights without the mode calling AI
  internals;
- each bot can have mode-private state that survives ordinary respawns;
- modes can register their own items and entity classes;
- mode-owned entity implementations stay in the mode file or mode directory,
  splitting into focused entity files when their size or number warrants it;
- removing a mode removes its cvars, registrations, data, and behavior without
  editing unrelated common files.

These goals favor compile-time mode modules inside `game.so`, with runtime
activation and instance state. They do not require one shared library per
mode.

## 6. Mode definition and instance

The core should know only `g_mode_def_t` and `g_mode_t`. The definition is
immutable and safe to expose; the instance owns all mutable state:

```c
typedef struct {
  const char *name;                    // stable external identifier
  g_mode_kind_t kind;                  // primary mode or modifier
  size_t state_size;                   // private, zeroed instance storage
  size_t entity_data_size;             // one private record per entity slot
  size_t entity_data_align;
  size_t client_data_size;             // optional: one record per client slot
  size_t client_data_align;
  const g_mode_cvar_def_t *cvars;
  size_t num_cvars;
  const g_mode_ops_t *ops;
  void (*Compose)(g_mode_builder_t *);
  const g_mode_bot_ops_t *bots;
} g_mode_def_t;

typedef struct {
  const g_mode_def_t *def;
  void *state;                         // only mode callbacks cast this
  void *entity_data;                   // entity_capacity * entity_stride
  size_t entity_stride;
  void *client_data;                   // client_capacity * client_stride
  size_t client_stride;
  g_registration_scope_t registrations;
  uint32_t generation;
} g_mode_t;

typedef struct {
  g_mode_t primary;
  g_mode_t *modifiers;
  size_t num_modifiers;
  g_mode_services_t services;
} g_mode_runtime_t;
```

The private CTF state type, for example, stays within the CTF implementation
boundary: in `g_mode_ctf.c` for a small module, or in an unexported
`g_mode_ctf_local.h` shared only by CTF source files when the implementation is
split. The runtime allocates `state_size`, passes `g_mode_t *` to callbacks,
and frees the state at deactivation. No `g_ctf`, `g_level.ctf`, static mutable
singleton, or public CTF state is needed. `static const` descriptors and
definitions are data, not mutable global state, and are acceptable.

`state_size` is the one mode-wide record for an active instance. A Tag mode
stores its single current “it” client there, not as duplicated `is_it` flags
in every client record. Client and entity AoS slabs remain appropriate for
facts that exist once per slot. Any client or entity pointer kept in the
mode-wide record must be cleared by the matching disconnect/free lifecycle
hook.

Mode code should receive world access, clocks, allocation, messaging, and
registries through a narrow `g_mode_api_t`/callback context. It should not read
`g_level`, `g_team_list`, or common cvar-pointer globals directly. Keeping mode
files on a restricted header makes this convention reviewable even though C
cannot enforce it as a language boundary.

A mode exports common lifecycle hooks through `g_mode_ops_t`:

```c
typedef struct {
  bool (*Activate)(g_mode_t *, const g_mode_activate_t *);
  void (*Deactivate)(g_mode_t *);
  bool (*LevelInit)(g_mode_t *, const g_level_init_t *);
  void (*LevelShutdown)(g_mode_t *);
  void (*MatchStart)(g_mode_t *);
  void (*Frame)(g_mode_t *);
  void (*ClientJoined)(g_mode_t *, g_client_t *);
  void (*ClientLeft)(g_mode_t *, g_client_t *);
  void (*ClientKilled)(g_mode_t *, const g_kill_event_t *);
  g_match_result_t (*CheckMatch)(g_mode_t *);
} g_mode_ops_t;
```

Only genuinely common lifecycle events belong here. Damage, spawning, and
respawning need stronger contracts than optional callbacks and are components,
described below.

The runtime supports one primary mode plus zero or more ordered mode
modifiers. This preserves combinations such as CTF plus Instagib without
creating a combined-mode enum: CTF supplies the team objective, while
Instagib decorates loadout, items, and damage. The current Arena behavior also
fits a modifier; a future round-based Arena would be a primary mode. If future
mods disallow combinations, they can configure zero modifiers without changing
the module contract.

Mode selection should use stable string names rather than a shared enum. One
build manifest should generate both the list of source files and the mode
definition registry. Removing a mode then means removing one manifest entry
and its files; no switch in `g_main.c`, cgame, AI, or item code should mention
it. A map that requests an unavailable mode or modifier should produce a clear
validation error or an explicitly configured fallback.

## 7. Fixed-capacity AoS mode data

General per-entity mode data should be one fixed-capacity array of
mode-defined records:

```c
// Common prefix known by the mode runtime.
typedef struct g_mode_entity_s {
  g_entity_t *entity;
  uint8_t spawn_id;
} g_mode_entity_t;

// Private extension visible only inside the CTF implementation.
typedef struct g_ctf_entity_s {
  g_mode_entity_t base;                // must be first
  g_ctf_entity_kind_t kind;

  union {
    g_ctf_flag_entity_t flag;
    g_ctf_capture_trigger_t capture_trigger;
  } locals;
} g_ctf_entity_t;
```

At activation, the runtime allocates
`entity_stride * sv_max_entities->integer` bytes once and zeroes the block.
`entity_stride` is the aligned `entity_data_size` from the definition. The
capacity never changes while the module is active, and spawning an entity does
not allocate mode data. This is an AoS component pool: all CTF-local fields for
one entity are together in one `g_ctf_entity_t`, and each active module has its
own independently sized pool.

This deliberately mirrors the existing structure-prefix convention: the
server knows the common prefix of `g_entity_s`, while the game module defines
the larger structure
([`game.h`](../src/game/game.h#L50-L147),
[`g_types.h`](../src/game/default/g_types.h#L1536-L1600)). Here, the mode
runtime knows `g_mode_entity_t`, while the mode's private sources define and
cast the larger `g_ctf_entity_t`. A descriptor whose entity record size is
non-zero must provide at least `sizeof(g_mode_entity_t)` and compatible
alignment.

Access follows the existing `g_entity_s` pattern and entity-slot identity:

```c
static g_ctf_entity_t *Ctf_Entity(g_mode_t *mode, const g_entity_t *ent) {
  g_ctf_entity_t *data = G_ModeEntityData(mode, ent->s.number);
  return data->base.spawn_id == ent->s.spawn_id ? data : NULL;
}
```

The common entity allocator notifies active modules when a slot is acquired:
it clears that slot's record, sets `base.entity`, and copies the new
`ent->s.spawn_id`. Entity free first invokes registered mode/component
destructors, then clears the record. Generation checks prevent a pointer or
entity reference from silently addressing data belonging to a previous
occupant of the same slot.

`sv_max_entities`, bounded by `MAX_ENTITIES`, is the correct capacity for
general entity data. A separate optional AoS slab uses `sv_max_clients`,
bounded by `MAX_CLIENTS`, for state whose identity is the client slot and which
must survive entity destruction or respawn. Mode-specific bot roles,
round-lifetime player state, queue membership, and per-client objective data
belong there:

```c
typedef struct g_mode_client_s {
  g_client_t *client;
  uint32_t generation;
} g_mode_client_t;

typedef struct g_ctf_client_s {
  g_mode_client_t base;                // must be first
  g_ctf_role_t bot_role;
  uint32_t defend_until;
  int16_t captures;
} g_ctf_client_t;
```

The client slab is reset on connect/disconnect according to the mode's
lifecycle contract, not on ordinary respawn. Both slabs are freed as part of
module teardown. A module that needs no entity- or client-local data declares
the corresponding size as zero and allocates no slab. A non-zero client record
similarly begins with `g_mode_client_t`.

This is preferable to adding a generic `void *mode_data` to every entity,
allocating small objects during spawn, or forcing all modes into one union in
`g_entity_t`. It preserves familiar indexed structs while allowing removal of
a module to remove its record types entirely.

## 8. Reusable and replaceable components

The active mode composes a set of service slots through `g_mode_builder_t`.
Common implementations live outside mode directories:

| Component | Typical implementations |
|---|---|
| Teams | disabled, fixed teams, auto-assigning teams |
| Spawn selection | deathmatch, team spawn, wave/queue spawn |
| Respawn | immediate, delayed, queued/wave, round-elimination |
| Damage | standard, friendly-fire decorator, instagib, invulnerable warmup |
| Loadout/items | map pickups, fixed loadout, filtered item set |
| Objective/scoring | frags, CTF, control points, round wins |
| Match lifecycle | time/score limit, rounds, warmup/intermission |
| Bot policy | generic combat, objective roles and goal weights |

A mode assembles these components and supplies only mode-specific policy:

```c
static void Ctf_Compose(g_mode_builder_t *builder) {
  G_ModeBuilder_SetTeams(builder, &g_teams_auto);
  G_ModeBuilder_SetSpawns(builder, &g_spawns_team);
  G_ModeBuilder_SetRespawn(builder, &g_respawn_immediate);
  G_ModeBuilder_DecorateDamage(builder, &g_damage_friendly_fire);
  G_ModeBuilder_SetObjective(builder, &ctf_objective);
}
```

Every component interface receives an opaque component context. Context is
allocated per active mode/component instance, never stored in a component
global. Components may be parameterized by immutable definitions, so several
modes can reuse the same implementation with different settings.

Composition starts with safe core defaults, applies the primary mode, then
applies modifiers in declared order. The builder rejects ambiguous ownership,
such as two components both claiming the respawn strategy, unless the later
module explicitly declares replacement. Lifecycle and event ordering follows
the same order; teardown runs in reverse. This makes mode combinations
deterministic and testable.

Use three explicit extension forms:

| Need | Mechanism |
|---|---|
| Observe a committed event | Read-only event subscriber |
| Modify an operation | Ordered, typed modifier pipeline |
| Own an algorithm/lifecycle | Single replaceable strategy component |

This distinction avoids an unordered “hook everything” event bus. Damage is a
typed pipeline with a mutable `g_damage_context_t`, deterministic modifier
order, one canonical apply step, and a read-only result event. Respawning is a
strategy because a queue or round-elimination mode must own death-to-spawn
lifecycle:

```c
typedef struct {
  void (*ClientDied)(void *ctx, const g_kill_event_t *);
  bool (*Request)(void *ctx, g_client_t *, bool voluntary);
  void (*Frame)(void *ctx);
  g_entity_t *(*SelectSpawn)(void *ctx, g_client_t *);
  void (*PrepareClient)(void *ctx, g_client_t *);
} g_respawn_ops_t;
```

Common client code performs the low-level unlink/reset/link mechanics, but
calls the active respawn strategy to decide **when**, **where**, and with what
state a client returns. The same principle applies to damage: common combat
code owns safety invariants and entity mutation, while the selected damage
strategy and modifiers determine policy.

## 9. CTF as a removable mode

CTF should own its objective state, cvar schema, flag item registrations, flag
entity behavior, scoring, announcements, and bot directives. It should import
the team and team-spawn components instead of implementing them.

For CTF, introduce one state record per active flag:

```c
typedef enum { CTF_AT_BASE, CTF_CARRIED, CTF_DROPPED } g_ctf_flag_state_t;

typedef struct {
  g_team_id_t owner;
  g_ctf_flag_state_t state;
  g_entity_t *base;
  g_entity_t *dropped;
  g_client_t *carrier;
  uint32_t return_at;
} g_ctf_flag_t;
```

`g_mode_ctf_state_t` contains the flag array, capture counters, resolved cvar
values, and any registered handles. Only sources inside the private CTF
implementation cast the mode state and mutate these records. Entity visibility,
inventory compatibility fields, render effects, announcements, counters, bot
notifications, and stats become effects of private transitions such as
`Ctf_Take`, `Ctf_Drop`, `Ctf_Return`, and `Ctf_Capture`. Assertions can then
enforce exactly one location for every flag.

No common file should contain `if (mode == CTF)`, flag-item cases, capture
limits, or CTF bot heuristics. It may contain generic calls to the active
objective, item registry, event publisher, or bot policy.

## 10. Mode-owned cvars

Mode files should export declarative cvar schemas rather than mutable
`cvar_t *` globals:

```c
enum {
  CTF_CVAR_CAPTURE_LIMIT,
  CTF_CVAR_HOOK,
  CTF_CVAR_TECHS,
  CTF_CVAR_TOTAL
};

static const g_mode_cvar_def_t ctf_cvars[] = {
  [CTF_CVAR_CAPTURE_LIMIT] = {
    "g_ctf_capture_limit", "8", CVAR_SERVER_INFO,
    "The capture limit per match."
  }
};
```

During game initialization, the registry asks every compiled mode for its
schema and registers those cvars. The active instance obtains values through
`G_ModeCvar(mode, CTF_CVAR_CAPTURE_LIMIT)` or caches the returned handles in
its private state. Thus declaration and interpretation remain in the mode,
while actual cvar storage remains in the common cvar system.

The implementation also keeps selector metadata on the mode descriptor. The
common level code asks the registry whether the objective selector or capture
limit changed; it no longer owns `g_ctf` or `g_capture_limit` pointers. CTF
still uses the historical cvar names as compatibility aliases, but their
registration and selection metadata live in `g_mode_ctf.c`.

Common cvars such as time limit or friendly-fire factor belong to the
component that implements them, not whichever mode first used them. Legacy
names can be aliases during migration.

## 11. Bot behavior without AI coupling

The AI core should query a generic active `g_mode_bot_ops_t`; the mode should
not call private `G_Ai_*` functions or manipulate `ai_t` goals directly:

```c
typedef struct {
  void (*Init)(g_mode_t *, g_client_t *, void *client_data);
  void (*Shutdown)(g_mode_t *, g_client_t *, void *client_data);
  void (*Directives)(g_mode_t *, const g_ai_mode_view_t *,
                     void *client_data, g_ai_directives_t *);
  void (*Event)(g_mode_t *, void *client_data, const g_game_event_t *);
} g_mode_bot_ops_t;
```

`g_ai_mode_view_t` is a read-only view of generic world facts. The output can
adjust objective desirability, item weights, enemy priority, preferred areas,
roles (attack/defend/escort), or supply candidate goals. The ordinary AI
planner remains responsible for navigation, combat, and command generation.

The callback receives the bot's record from the module's fixed
`sv_max_clients` AoS slab. The record persists across ordinary respawns and is
reset on disconnect and freed with the slab at module teardown; there is no
per-bot allocation and
no `void *` stored on `ai_t`. If AI goals retain mode data beyond a callback,
they store `{ client_slot, module_generation }`, not the record pointer. The
private client-record type remains inside the mode implementation, allowing
CTF to track roles or defensive assignments without adding CTF fields to
`ai_t`.

## 12. Owner-scoped entity and item registration

All registrations made during activation should carry the mode's
`g_registration_scope_t`. Deactivation removes the complete scope only after
its entities, item instances, and bot state have been destroyed. Duplicate
class names or identifiers are activation errors.

### Entity classes

Replace `g_entity_classes[]` with a classname registry used by both common
code and modes:

```c
typedef struct {
  const char *classname;
  void (*Spawn)(void *owner_ctx, g_entity_t *, void *entity_data);
  void (*Destroy)(void *owner_ctx, g_entity_t *, void *entity_data);
} g_entity_class_def_t;
```

Registration binds `owner_ctx` separately from the immutable definition; a
mode passes its instance and a built-in class can pass `NULL` or a component
context. For a mode-owned class, `entity_data` is the already allocated record
at `ent->s.number` in that mode's entity slab; spawning performs no private
allocation. An entity retains only its class handle and owner scope. The
registry guarantees that callbacks cannot run after their owner is
deactivated. Built-in entities should eventually use the same registry, so
there is only one dispatch path. Item classnames are registered through the
item registry and remain higher-level item entities, not special cases in
`G_SpawnEntity`.

### Items

Mode-defined items cannot be implemented safely by merely appending to
`bg_item_defs`: the built-in tags are compile-time indices used by `g_items`,
HUD events, AI loops, and range tests such as `FLAG_FIRST..FLAG_LAST`. The
runtime now reserves the rest of `MAX_INVENTORY` for owner-scoped mode items;
the remaining catalog migration is:

1. use a runtime item tag/handle bounded by `MAX_INVENTORY`;
2. size server inventory by `MAX_INVENTORY`, not `ITEM_TOTAL` (implemented);
3. replace enum-range loops with registry queries by category/capability;
4. register a complete item record—definition plus pickup/use/drop/think
   behavior—so `G_InitItem` has no classname switch;
5. assign active-mode items owner-scoped IDs and invalidate them only after
   all clients/entities release them;
6. publish the active item catalog or a version/hash to cgame before snapshots
   refer to those IDs (the implementation publishes dynamic names, icons,
   models, classnames, categories, and effect colors through `CS_MODE_ITEMS`).

Generic item presentation data (name, icon, model, effects) can be sent in the
catalog. A mode needing custom client effects should have a corresponding
cgame-side presentation provider registered under the same stable item name.
The server remains authoritative; cgame never owns pickup or objective rules.

The runtime descriptor has an explicit `dynamic` form. It clones a complete
`g_item_t` prototype into a level-owned slot beginning at `ITEM_TOTAL`, mirrors
that record into the familiar `g_items[tag]` catalog, publishes its
  presentation metadata—including pickup/respawn effect color, quantity, and
  ammo relation—through `CS_MODE_ITEMS`, and tears it down with the mode.
  Lookup by classname and display name therefore works without a common mode
  switch. Built-in items still use their legacy IDs until all category
  consumers are migrated; a future mode can already add a custom pickup,
  objective token, or weapon presentation without modifying `bg_item.h`.

### Client presentation extensions

Server-only entities can use existing generic entity state, models, effects,
and events. A mode that needs custom HUD, scoreboard, item effects, or entity
rendering should provide a paired cgame presentation definition under the same
stable mode/item/entity names. The cgame registry maps negotiated runtime IDs
to those providers; generic catalog data remains the fallback.

The mode manifest should control both the game and cgame provider sources.
This prevents CTF-specific HUD and config parsing from remaining in common
cgame after CTF is removed. Connection setup must validate the catalog version
or required provider set before any snapshot uses a mode-defined ID.

## 13. Activation and teardown order

A safe map transition is a transaction:

1. preflight map metadata/worldspawn to select the primary mode and modifiers
   before spawning entities;
2. end the old match and call old module level-shutdown hooks in reverse order;
3. destroy old module-owned entities, items, and per-bot data;
4. remove old registration scopes and free component contexts, AoS slabs, and
   instance state;
5. allocate each new module instance, fixed-capacity AoS slabs, and
   registration scope;
6. register their entity classes and items, then compose and validate the
   complete component set;
7. resolve common/component/module cvars into an immutable match configuration;
8. initialize the level, spawn map entities through the registries, and start
   the match;
9. publish the normalized mode/component/item catalog to clients.

If any activation step fails, discard the new scope as a unit. Do not leave
partially registered classes or cvars pointing into freed mode state. Cvars
declared for compiled modes may remain registered across activations; they
contain no mode-instance pointers.

## 14. Suggested source layout

```text
src/game/default/
  g_mode.c, g_mode.h                 runtime, registry, builder
  g_event.c, g_event.h               typed committed events
  components/
    g_damage.c
    g_match.c
    g_respawn.c
    g_spawn.c
    g_team.c
  modes/
    deathmatch/g_mode_deathmatch.c
    instagib/g_mode_instagib.c
    arena/g_mode_arena.c
    ctf/
      g_mode_ctf.c
      g_mode_ctf_local.h              private to CTF sources
      g_mode_ctf_entities.c           registration and small entity classes
      g_mode_ctf_flag.c               optional substantial entity class
      g_mode_ctf_bot.c                optional substantial bot policy
```

Keep mode-specific entity definitions with their mode:

- a small mode with one or two small entity classes may keep them in
  `g_mode_<name>.c`;
- many small entity classes should move together to
  `g_mode_<name>_entities.c`;
- a substantial entity class should get a focused file such as
  `g_mode_ctf_flag.c`;
- several substantial classes may each have a file, with
  `g_mode_<name>_entities.c` acting as their registration aggregator.

Split sources may share `g_mode_<name>_local.h` for the private mode state, AoS
record types, and internal functions. That header is not installed or included
by common game code. The mode manifest owns the complete source list so all of
these files disappear as one removable unit.

A mode with shared game/cgame item definitions may also contain
`bg_<mode>.c` plus its server and presentation adapters. Common item, entity,
client, combat, and AI files call registries/components; they do not include
mode-private headers.

## 15. Incremental migration

1. Add characterization tests for resolution precedence, the composition
   matrix above, match limits, restarts, and every CTF transition (including
   death, disconnect, team change, timeout, and multi-team play).
2. Introduce the mode runtime, immutable descriptors, per-instance opaque
   state, fixed entity/client AoS slabs, allocation/free notifications, and one
   build manifest. Initially provide a “legacy” mode adapter that delegates to
   current behavior.
3. Extract team, spawn selection, immediate respawn, match limits, and standard
   damage as components. Route existing entry points through them before moving
   mode-specific policy.
4. Add typed damage modifiers and the replaceable respawn strategy. Test an
   artificial damage modifier and queued-respawn component to prove the model.
5. Replace the static entity table with an owner-scoped registry; migrate
   built-ins without changing map behavior.
6. Convert items to a runtime catalog within `MAX_INVENTORY`, remove
   type-range assumptions, and synchronize the catalog with cgame.
7. Move CTF into its mode directory: private state, cvar schema, flags,
   entity/item registrations, objective/scoring hooks, and bot policy. Split
   substantial or numerous entity classes according to the rules above.
   Removing CTF from the manifest should now compile and run non-CTF modes.
8. Move Instagib and Arena policy into their own modules/components. Decide
   explicitly whether Arena gains real rounds and whether “auto-balance = 2”
   is implemented or removed.
9. Replace fragmented mode configstrings with a normalized, versioned client
   description. Fix or retire the unused `CS_GAMEPLAY` path.
10. Add teardown/leak tests that repeatedly switch modes and verify no
    registrations, entities, item IDs, bot data, or callbacks survive their
    owner scope. Exercise maximum entity/client capacities, aligned record
    strides, slot reuse, and spawn/generation mismatches in the AoS slabs.

The runtime, component seams, owner-scoped entities, dynamic item catalog, CTF
extraction, and a reusable techs modifier are now implemented. Characterization
and teardown tests, remaining category-query migrations, and transactional
runtime mode reconfiguration are the next validation work. The legacy adapter
remains to preserve current behavior while those closed-set consumers are
migrated. Dynamic weapon presentation and selection now use runtime item tags
for the first-person model, ammo HUD, item metadata, and selector traversal.

## 16. Implemented refactor

The current refactor includes that first runtime seam in the default game
module:

- `g_mode.c`/`g_mode.h` provide a compiled-mode registry, primary-plus-modifier
  activation (up to four ordered modifiers), immutable descriptors, opaque instances, aligned fixed-capacity
  `sv_max_entities` and `sv_max_clients` AoS slabs, generation/slot prefixes,
  lifecycle dispatch, and teardown.
- Each active instance receives a refreshed `g_mode_context_t` service view
  (level, teams, media, item catalog, and entity/client pools). Mode sources
  use this view instead of directly naming the legacy mutable globals. The
  runtime also exposes behavioral capability bits, so common code can ask for
  team play, objective flags, suppressed pickups, no-ammo, or no-self-damage
  without naming CTF, Instagib, or Arena.
- Primary and modifier selection is descriptor-driven (`G_ModePrimaryName`
  and `G_ModeModifierName`); worldspawn no longer embeds literal mode names.
- The reusable fixed-team helpers now live in `g_mode_team.c`; mode-provided
  `AssignTeam` and `SelectSpawn` hooks can replace those defaults without
  coupling the client lifecycle to a particular mode.
- Common spawn selection is now isolated in `g_mode_spawn.c`, with the mode
  spawn hook consulted before team/deathmatch fallback. Ordinary frag-limit
  enforcement is similarly isolated in `g_mode_match.c`; objective scoring is
  dispatched through the active mode's rules hook.
- Mode descriptors declare their cvars, item classes, and entity classes. The
  runtime performs owner-scoped lookup and dispatch, so common entity spawning
  and item lookup no longer need to know a mode's private class table.
  Dynamic item descriptors receive tags from the reserved `MAX_INVENTORY`
  range; the runtime publishes their presentation records to cgame and clears
  those slots during owner teardown.
- Typed extension points now exist for damage modification, replacement
  respawn decisions, mode-owned client loadouts, bot directives, target
  priority, pickup eligibility, and flag-objective services. The AI asks for
  generic mode policy and never calls CTF code directly. Instagib and Arena
  loadouts now live in the modifier module rather than the client lifecycle.
- CTF flag transitions, flag queries, cvar declarations, objective state, and
  per-client bot role data live in `g_mode_ctf.c`; common item/client paths
  invoke the mode services. The CTF state uses a flag-state record and the
  per-entity record follows the `g_entity_s`-style prefix convention. The four
  small flag entity classes and four mode-owned item descriptors are
  registered by that module and dispatch through the owner-scoped registries.
  CTF clones the legacy flag definitions as immutable prototypes, assigns
  owner-scoped runtime tags, and publishes their cgame presentation records.
  CTF sounds are stored in its private state and precached from its level-begin
  hook instead of the common media initializer.
- Default, Deathmatch, Instagib, and Arena each have their own mode source
  file. Deathmatch remains the primary legacy adapter; Instagib and Arena are
  ordered modifiers, preserving their existing behavior while giving each
  implementation a removable module seam.
- Techs are now represented by the reusable `g_mode_techs.c` modifier. Its
  `g_techs` cvar schema is registered through the mode registry, tech enablement
  resolution is exposed as a mode service, and tech spawning is invoked through
  the modifier's item-reset hook. This allows techs to accompany CTF, team
  deathmatch, Instagib, Arena, or a future primary mode without duplicating
  mode branches. Damage, post-damage vampire healing, fire timing, regeneration,
  media indices, and tech timers are owned by the modifier; common item/stats/HUD paths
  retain narrow service calls while their catalog assumptions are migrated.
- Defining `G_MODE_ENABLE_CTF=0` compiles out the CTF implementation and
  registration. Common level code falls back to the non-objective selector
  and no longer requires CTF cvar pointers or CTF hook symbols.

The built-in item catalog and cgame presentation still retain the legacy
compile-time IDs for compatibility. Category/range consumers, team assignment,
and some map-spawn synthesis remain common components. Dynamic mode item
descriptors—including CTF flags—have owner-scoped runtime IDs in the reserved
`MAX_INVENTORY` range and publish presentation metadata through
`CS_MODE_ITEMS`; cgame consumes names, classnames, icons, models, categories,
effect colors, quantities, and ammo tags. Objective items, generic pickups,
and runtime-tagged weapon presentation and selection are represented
end-to-end. Remaining work is chiefly transactional live reconfiguration and
tests around ownership teardown and capacity limits.
