# Refactor default game modes into composable modules

## Summary

This change introduces a mode runtime for the default game module and moves
mode-specific behavior out of the common client, item, combat, AI, and entity
paths.

At a high level, a level is now composed from:

- one primary mode, currently Deathmatch or CTF;
- up to four ordered modifiers, currently Instagib, Arena, and Techs; and
- reusable common components for teams, spawning, and match limits.

Modes describe their behavior through immutable `g_mode_def_t` descriptors and
typed `g_mode_ops_t` hooks. Mutable mode state belongs to the active
`g_mode_t` instance rather than a mutable singleton in the mode source file.

The longer design and source audit remains available in
[game-modes-refactor-report.md](game-modes-refactor-report.md). A shorter
architecture overview is in
[game-modes-refactor-summary.md](game-modes-refactor-summary.md). A
step-by-step contributor tutorial is in
[how-to-add-game-mode.md](how-to-add-game-mode.md). Dedicated extension guides
cover [weapons](how-to-add-weapon.md) and
[mode-specific items](how-to-add-mode-item.md).

## Motivation

Previously, the effective game mode was spread across `g_level.gameplay`,
`g_level.teams`, `g_level.ctf`, Tech settings, item-set settings, and many
conditionals throughout the default game module. CTF behavior in particular
was distributed across item handling, client death, combat, commands, AI,
media initialization, scoring, and world spawning.

That arrangement made a mode difficult to:

- understand as one unit;
- remove from a downstream mod;
- combine safely with other gameplay options;
- extend with private entities, items, or bot state; and
- change without auditing unrelated common systems.

The new runtime provides an incremental boundary around those responsibilities
while retaining the existing map format, shared item definitions, networking,
and most public game-module behavior.

## Maintainer-level architecture

### Composition

`g_mode.c` owns the active composition:

```text
level
├── primary mode
│   └── Deathmatch or CTF
├── ordered modifiers
│   ├── Instagib or Arena
│   └── Techs
└── reusable components
    ├── teams
    ├── spawn selection
    └── match limits
```

Worldspawn resolves the legacy map and cvar settings, selects the primary mode
and modifiers, and calls `G_ModeBeginLevel`. Common code then asks the runtime
for behavior or capabilities instead of calling CTF, Arena, or Instagib
functions directly.

Composition order is deterministic: the primary mode runs first, followed by
modifiers in declaration order. Teardown reverses modifier order and releases
the primary mode last.

### Mode descriptors

Each compiled mode exports a `g_mode_def_t` containing:

- its stable name and kind;
- explicit eligibility for the legacy `g_gameplay` selector;
- behavioral capability flags;
- sizes for private level, entity, and client records;
- mode-owned cvar declarations;
- map entity class registrations;
- item registrations; and
- a table of typed operations.

The hook table covers lifecycle, client inventory and per-frame behavior,
entity allocation/freeing, item behavior, team/objective queries, bot policy,
damage modification, post-damage effects, weapon timing, respawning, spawn
selection, team assignment, and match rules.

This keeps the common call sites generic while allowing a mode to replace
important policies such as damage or respawn behavior.

### Private state and AoS records

An active mode may request three owner-scoped storage areas:

- one mode-wide, per-level state record;
- one AoS entity record per server entity slot; and
- one AoS client record per server client slot.

The mode-wide record is the single source of truth for relationships that
exist once per composition member. A Tag mode, for example, stores its one
current “it” client there instead of distributing competing `is_it` booleans
across client records. Despite being global to that mode instance, this is not
a mutable process-global singleton.

The slabs are allocated at level initialization, aligned to `max_align_t`, and
bounded by `MAX_ENTITIES` and `MAX_CLIENTS`. Records use familiar common
prefixes:

- `g_mode_entity_t` stores the entity pointer and spawn ID;
- `g_mode_client_t` stores the client pointer and mode generation.

Mode implementations extend those prefixes with their private fields. CTF, for
example, stores flag identity per entity and objective role data per client.
Techs stores regeneration and sound-throttling timers in its own client record
instead of adding Tech-specific fields to `g_client_t`.

### Capabilities and common components

Common code can query behavioral capabilities such as:

- team play;
- flag objectives;
- Instagib;
- Arena;
- no ammo;
- no self-damage; and
- suppressed map items.

This avoids using a mode name where the caller only needs a behavior.

Reusable team, spawn, and ordinary match-limit logic has been separated into
`g_mode_team.c`, `g_mode_spawn.c`, and `g_mode_match.c`. Modes may override the
relevant typed operations while retaining these defaults.

## CTF ownership

CTF now lives in `g_mode_ctf.c` and owns:

- `g_ctf` and `g_capture_limit` declarations;
- its private flag state machine;
- capture, steal, return, drop, and reset behavior;
- CTF media indices;
- flag item and entity-class registrations;
- objective score-limit handling;
- team/flag queries; and
- per-client bot objective roles and priorities.

The four flag entity classes are small and closely related, so they remain in
the CTF mode file. A mode with larger or more numerous entity implementations
can place those implementations in a separate mode entity source file while
keeping their registrations in the descriptor.

CTF can be compiled out with `G_MODE_ENABLE_CTF=0`. Common code no longer
requires direct CTF symbols when that mode is unavailable.

## Techs as a modifier

Techs are modeled as a reusable modifier rather than a CTF subsystem.
`g_mode_techs.c` owns:

- the `g_techs` cvar declaration and enablement policy;
- Tech item descriptors;
- Tech spawning and dropped-item reset;
- pickup, drop, and inventory queries;
- private media indices;
- per-client regeneration and sound timers;
- Resist and Strength damage changes;
- Vampire post-damage healing; and
- Haste weapon timing.

The modifier is composed independently, so it can accompany Deathmatch, team
play, CTF, Instagib, Arena, or a future primary mode.

## Mode-owned items and entities

Mode descriptors may register map entity classes and item descriptors.
Registrations are active only while their owner is active and are released
with that owner.

Dynamic item callbacks can recover the primary mode or modifier that owns
their item through `G_ModeForItem`. This keeps dynamic item behavior
owner-relative without assuming composition position or hard-coding a
primary-mode lookup.

Dynamic items receive runtime tags from the reserved
`ITEM_TOTAL..MAX_INVENTORY` range. The server publishes their presentation
metadata through `CS_MODE_ITEMS`, including:

- name and classname;
- icon and model;
- item category;
- quantity and ammo tag; and
- effect color.

The cgame inventory, active-weapon, HUD, first-person weapon, item-event, and
weapon-selector paths now accept runtime item tags. Built-in item tags remain
unchanged for protocol and content compatibility.

Common weapon fire validation, post-fire bookkeeping, and muzzle-flash
publication are exported as reusable helpers. A mode-owned dynamic weapon can
therefore provide its own `Think` callback without duplicating ammunition,
refire, animation, Quad, or modifier-timing behavior.

CTF flags use this mechanism: the legacy flag definitions are immutable
prototypes, while the active CTF mode owns their runtime records and tags.
Flag tint identity is carried independently of the runtime inventory tag so
map-spawned and dropped flags retain the correct team color.

## Bot integration

The generic AI does not call CTF code directly. It asks the composed mode for:

- item weighting and a mode-defined role;
- target priority and chase multipliers; and
- mode-specific pickup eligibility.

Persistent per-mode bot data uses the mode's per-client AoS record. This gives
each mode private bot state without adding a generic `void *` to `g_client_t`
or requiring the mode to hook deeply into the AI implementation.

AI weapon selection and aiming also tolerate the short unarmed window that can
occur during respawn or developer-mode initialization.

## Lifecycle and teardown

The important ownership order is:

1. resolve the level configuration;
2. activate the primary mode and modifiers;
3. allocate private state and runtime item records;
4. initialize existing entity slots and call level-begin hooks;
5. dispatch gameplay through the composed operations;
6. free entities while their owning mode and item records still exist;
7. run level-end hooks in reverse composition order; and
8. clear dynamic catalog slots and release mode storage.

Keeping mode-owned item records alive until entities are released prevents
callbacks and `ent->item` pointers from referring to freed storage.

Stats posting is idempotent, including the edge case where a mode ends a level
at simulation time zero.

## Principal files

| Area | Files |
| --- | --- |
| Runtime and API | `src/game/default/g_mode.c`, `g_mode.h` |
| Primary modes | `g_mode_default.c`, `g_mode_deathmatch.c`, `g_mode_ctf.c` |
| Gameplay modifiers | `g_mode_instagib.c`, `g_mode_arena.c`, `g_mode_techs.c` |
| Reusable components | `g_mode_team.c`, `g_mode_spawn.c`, `g_mode_match.c` |
| Dynamic server items | `g_item.c`, `bg_item.h`, `g_types.h` |
| Dynamic cgame presentation | `cg_inventory.c`, `cg_hud.c`, `cg_weapon.c` |
| Generic integration | client, combat, entity, command, utility, and AI sources |
| Contributor guides | `doc/how-to-add-game-mode.md`, `how-to-add-weapon.md`, `how-to-add-mode-item.md` |

## Benefits

- **Mode locality:** Objective state, callbacks, cvars, media, and bot policy
  are readable in the owning mode module.
- **Removal:** A downstream mod can omit a mode with substantially fewer
  common-code edits; CTF already has a compile-time removal guard.
- **Composition:** Orthogonal mechanics such as Techs, Instagib, and Arena can
  be applied without duplicating primary modes.
- **Private state:** Modes can retain per-level, per-entity, and per-client
  data without expanding common structures for every possible mod.
- **Typed extension points:** Damage, weapon timing, respawning, spawn
  selection, loadouts, items, rules, and bots have explicit contracts.
- **Familiar memory model:** Entity and client AoS records follow the indexing
  and identity style already used by the game module.
- **Incremental migration:** Legacy maps, cvars, item prototypes, and common
  components continue to work while closed-set conditionals are removed.
- **Extensible content:** A mode may introduce entity classes and runtime
  items without adding them permanently to the common spawn table.

## Costs and tradeoffs

- **A larger dispatch surface:** `g_mode_ops_t` is intentionally broad. New
  policies require choosing whether they are hooks, replacement strategies,
  capabilities, or reusable components.
- **Order matters:** Multiple modifiers can affect the same operation. Their
  order is deterministic but must be reviewed when adding combinations.
- **Fixed budgets:** The implementation currently allows four modifiers and is
  limited by the remaining `MAX_INVENTORY` slots for dynamic items.
- **Lifecycle complexity:** Runtime registrations and mode-owned pointers make
  activation and teardown order more important than with a fully static item
  table.
- **Protocol payload:** `CS_MODE_ITEMS` is a compact info string rather than a
  versioned schema. A mode with many richly described items may exhaust the
  configstring budget.
- **Incomplete legacy migration:** Some configuration resolution and common
  item/team behavior still use `g_level` compatibility fields.
- **Live reconfiguration is not transactional yet:** Runtime cvar changes
  reset clients and items, but they do not rebuild the complete active
  composition in place. Map load remains the authoritative composition
  boundary.
- **Registry is compiled:** Adding or removing a mode still requires updating
  the compiled registry and build manifest; this is modular source ownership,
  not runtime plugin loading.
- **More validation is needed:** The configured test target currently has no
  enabled tests for mode composition, teardown, or CTF transitions.

## Reviewer focus

The highest-value review areas are:

1. activation and reverse-order teardown in `g_mode.c`;
2. entity slot reuse and the entity/client AoS identity checks;
3. dynamic item tag allocation, publication, and cgame parsing;
4. CTF base/carried/dropped/returned transitions;
5. damage and weapon-timing modifier order;
6. bot behavior with and without private mode client data;
7. flag behavior for map-spawned, synthesized, carried, and dropped flags; and
8. behavior when CTF is compiled out.

## Validation performed

- `make -j2`
- `make check -j2`
- `git diff --check`

The configured `make check` target completes successfully but reports zero
enabled tests. Manual smoke testing should cover at least:

- Deathmatch, team Deathmatch, and CTF map loads;
- CTF combined with Instagib and Arena;
- Arena on both Quetoo-item and Quake-item maps;
- Tech enable/disable behavior across CTF and non-CTF modes;
- flag steal, drop, return, capture, and team tint;
- bot respawn, weapon selection, item pickup, and flag priorities;
- repeated map changes; and
- a build with `G_MODE_ENABLE_CTF=0`.

## Follow-up work

- Add characterization tests for legacy map/cvar precedence.
- Add unit or integration coverage for composition order and teardown.
- Add CTF flag-transition and dynamic-item synchronization tests.
- Replace the ad hoc dynamic item info string with a versioned client schema if
  future modes require a larger catalog.
- Introduce an explicit transaction for live mode reconfiguration.
- Continue replacing compatibility reads of `g_level.ctf`, `g_level.teams`,
  and `g_level.gameplay` with capability or component queries.
