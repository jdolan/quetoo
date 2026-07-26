# Quetoo game-mode refactor — architecture summary

This is a short review guide for reimplementing the default game module. The
detailed source audit is in [game-modes-refactor-report.md](game-modes-refactor-report.md).

## Current behavior

The effective ruleset is composed from several independent values rather than
one mode: gameplay (`deathmatch`, `instagib`, `arena`), teams, CTF, item set,
hook, techs, and match limits. CTF supplies team-objective behavior; Instagib
and Arena alter loadouts/items and can be combined with CTF. Most policy is
still reached through legacy `g_level` fields, so adding a mode requires
auditing clients, items, combat, AI, commands, and scoring.

## Implemented refactor seam

`g_mode.c`/`g_mode.h` now provide:

- one primary mode plus up to four ordered modifiers;
- immutable descriptors and opaque per-level instances;
- aligned fixed-capacity AoS slabs indexed by `sv_max_entities` and
  `sv_max_clients`, with `g_entity_s`-style identity prefixes;
- lifecycle, item, entity, damage, respawn, spawn, team, rule, and bot hooks;
- owner-scoped dynamic item/entity registration and teardown;
- runtime item presentation metadata published to cgame through `CS_MODE_ITEMS`.

Each instance also owns one zeroed mode-wide state record. Unique
relationships such as Tag's one current “it” client belong there; per-client
AoS records hold facts that genuinely occur once per participant.

Default, Deathmatch, Instagib, Arena, and CTF are separate translation units.
CTF owns its flag state machine, flag descriptors, cvars, objective hooks, and
bot role data. Removing CTF is guarded by `G_MODE_ENABLE_CTF`.

## Techs as a reusable modifier

Techs are not a CTF special case. `g_mode_techs.c` registers the `g_techs`
cvar schema, resolves map/world/cvar enablement, and owns tech spawning through
the mode item-reset hook. The modifier is composed independently of CTF, so it
can accompany team deathmatch, Instagib, Arena, or a future primary mode.

Common combat and weapon paths now call typed modifier services for damage,
post-damage effects, fire timing, and client-frame regeneration. Pickup/drop,
stats, and HUD code use narrow compatibility services while the shared item
catalog is being migrated; tech timers and sound throttling live in the
modifier's per-client AoS record rather than in `g_client_t`.
Its sound indices are likewise held in modifier state rather than the common
media structure.

Dynamic item callbacks can resolve their owning mode instance, and custom
weapon callbacks can reuse common fire validation, ammunition/refire
bookkeeping, modifier timing, and muzzle-flash publication.

## Recommended component boundaries

Keep reusable policies separate from mode files:

| Component | Examples |
| --- | --- |
| Teams | disabled, fixed teams, auto-assignment |
| Spawning | deathmatch, team spawn, queued/wave spawn |
| Respawn | immediate, delayed, queue, round elimination |
| Damage | standard, friendly-fire, instagib, temporary modifiers |
| Loadout/items | map pickups, fixed loadout, filtered roster |
| Objective/scoring | frags, CTF, control points, round wins |
| Bot policy | combat goals, objective roles, per-bot state |

Use replacement strategies for respawn and ordered modifier pipelines for
damage. Give each active mode a private client record for bot/round/objective
state; do not add a global `void *` or mode-specific fields to every client.

## Remaining migration risks

1. Legacy branches still read `g_level.ctf`, `teams`, and `gameplay`; migrate
   them to capability/component queries.
2. Some item consumers still assume the built-in tag range; category-based
   weapon selection and runtime presentation are migrated, but broader item
   consumers still need characterization coverage.
3. Runtime cvar changes currently reset common state without rebuilding the
   composed mode instances; a future reconfiguration transaction should own
   that transition.
4. Add characterization and teardown tests for resolution precedence, CTF flag
   transitions, modifier ordering, mode removal, slot reuse, and max capacities.

## Validation

The normal configured tree builds successfully with `make -j2`; `make check
-j2` completes with no enabled tests in the current configuration. `git diff
--check` is clean.
