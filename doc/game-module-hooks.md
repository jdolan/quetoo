# Game module hooks

How the game modules share code, and the plan for finishing the job.

## The problem

`src/game/ctf` is a fork of `src/game/default`. Measured across the nine
duplicated `.c` files, before any of this work:

| | count |
| --- | --- |
| functions present in both | 166 |
| byte-identical | 130 |
| differing | 36 |
| present only in ctf | 4 |

So **78% of the fork is pure duplication**, and every fix to `default` had to be
carried across by hand or it silently rotted. The goal was that `default` is
little more than a manifest over `src/game/common`, and that a mod is its
manifest plus the handful of behaviours it genuinely changes. That is now the
case: see [State of the work](#state-of-the-work). What follows is why the
mechanisms are shaped the way they are, which is what you need in order to add
the next one.

Function-level override is *not* the answer on its own, because most of the
divergence is not function-shaped. Of the 36 differing functions, **24 are
additive only** — insertions inside an existing body, not replacements:

| function | body lines | inserted |
| --- | --- | --- |
| `G_ClientObituary` | 237 | 3 |
| `G_ClientMove` | 220 | 3 |
| `G_Damage` | 213 | 21 |
| `G_Init` | 200 | 5 |
| `G_ClientUserInfoChanged` | 174 | 3 |

Overriding `G_ClientObituary` wholesale would have ctf carry a 237-line copy to
change three lines: the same drift as today, but invisible to `diff`. So the
mechanism has to follow the *shape* of each difference.

## Four mechanisms

Pick by what kind of difference it is. In rough order of preference:

1. **Manifest, per-module.** Wire numbering, the item roster, tuning constants:
   `g_types.h`, `bg_item.h`, `bg_item.c`. These stay forked *by design*, and the
   reason is not only that a module numbers its own wire values, as
   `g_hook_types.h` already states. It is that a mod Quetoo does not ship can only
   edit files in its own directory: the manifest is what a modder owns, and
   sharing it would make adding one stat a reason to fork the engine. Do not try
   to share them. Roughly 265 of the diverging lines belong here.

2. **`#if` guard in common.** For additive divergence in few hunks. Duplicates
   nothing. This is right for most of the 24 additive functions.

3. **Chainable hook.** For a variation point that *several optional features may
   each want a say in*. See below. This is the interesting one.

4. **A hook that is not chained.** Where exactly one answer is possible - a win
   condition, the name of the gameplay - the mechanism is still a hook, but the
   feature installing it does not call super. Nothing enforces that, so the
   docblock on the hook MUST say which kind it is.

5. **A contract.** A plain function declared in `g_module.h` that every module
   defines, where the module is the only possible answer. There are two:
   `G_Module_Init` and `G_Module_Shutdown`. A missing definition is a link error,
   which is how a new module finds out what it owes.

One more option always remains: **override the whole file**. `vpath` resolves a
module's own copy ahead of common's, so forking one file is a bounded, per-file
decision rather than a per-module one. Use it when a file resists everything
else.

### Signals that you picked wrong

- Reaching for a hook to insert three lines mid-function → use a guard.
- A guard that needs an `#else` around game logic → that is a decision point
  asking to become a hook or a contract.
- Hunks averaging under ~5 lines across a whole file → the file is being
  shredded; find the seam that collects them. `g_main.c` was 39 hunks over 136
  lines before the features started owning their own cvars.
- A file that resists everything → it is probably a manifest and a mechanism
  wearing one name. `G_CheckRules` turned out to be three things: a win condition
  (`CheckWinCondition`), a pile of per-feature cvar blocks (`CheckCvars`), and
  the core rules, which never diverged at all.

## The chainable hook

Common holds the default; a feature installs itself over the top, keeping the
previous value to call as super.

```c
/* g_module.h — the contract, and the single authoritative list of hooks */
typedef void (*ResetDroppedItem)(g_entity_t *ent);

extern ResetDroppedItem G_ResetDroppedItem;
```

```c
/* g_item.c — the domain file owns the tail of the chain */
static void G_ResetDroppedItem_Common(g_entity_t *ent) {
  G_FreeEntity(ent);
}

ResetDroppedItem G_ResetDroppedItem = G_ResetDroppedItem_Common;
```

```c
/* g_tech.c — a feature installs over the top */
static struct {
  ResetDroppedItem ResetDroppedItem;
  DropInventoryItem DropInventoryItem;
} super;

static void G_ResetDroppedItem_Tech(g_entity_t *ent) {

  if (ent->item->def.type == ITEM_TYPE_TECH) {
    G_ResetDroppedTech(ent);
    return;
  }

  super.ResetDroppedItem(ent);
}

/* in G_Tech_Init */
super.ResetDroppedItem = G_ResetDroppedItem;
G_ResetDroppedItem = G_ResetDroppedItem_Tech;
```

`g_ctf.c` does the same for the flags, independently. Composition then falls out of
which features a module builds:

| module | chain |
| --- | --- |
| default | `_Common` |
| ctf | ctf → tech → hook → `_Common` |
| lithium | tech → hook → `_Common` |

Neither feature mentions the other, and no module hand-writes a dispatcher.

### Rules

- **Installation MUST be idempotent.** `G_Init` runs on *every* server
  initialization, not once per process — a single short session shows two
  `Game module initialization` cycles before the first map even loads — and
  `dlclose` does not reliably unload the module on macOS, so the file statics
  survive. Installing a second time sets `super.X = G_X` when `G_X` is already
  this feature's own function: the chain points at itself and the first call
  spins forever. Guard the installs with a `static bool installed`. This is not
  theoretical; it hung `drop tech` with a beachball.
- **Install from `G_Init`**, behind that guard. Never from anything per-level.
- **Chain order is installation order**, so the order of the `_Init` calls in a
  module's `G_Init` is part of its behaviour. Document it when it matters.
- **Call super, not the default.** Super is "let whoever installed before me
  decide". Calling the default directly instead gives last-writer-wins: with
  flags and techs both installed, whichever went second would swallow the
  other's item type. This is why the defaults are not public.
- **Switching modules mid-session is safe.** `default/game.so` and
  `ctf/game.so` are distinct images with distinct data, and game modules are
  opened `RTLD_LOCAL`, so one module's chain cannot reach the other's. Verified:
  `game default; game ctf` across a session gives 85 / 80 / 85 entities on
  `edge`. The rule to remember is that *persisting* state is harmless — media
  indices, enabled flags and `g_items` are reassigned on every init — while
  *accumulating* state is not, which is what makes the guard above necessary.
- **A feature holds only the hooks it replaced**, in its own local `super`
  struct. Do not thread a shared table of every hook — `super` would then be a
  snapshot of hooks you never touched, and correctness would depend on install
  order invisibly.

### Naming

- Hook type: **VerbSubject**, PascalCase, no prefix — `ResetDroppedItem`,
  `DropInventoryItem`, `InhibitItem`, `ConfigureLevel`, `PrepareMove`. This
  matches `g_entity_t::Think` and `::Touch`, and the `cg_entity.h` typedefs.
- Dispatch pointer: the type with a `G_` prefix — `G_ResetDroppedItem`.
- Feature implementation: the dispatch pointer with the feature suffixed —
  `G_ResetDroppedItem_Tech`, `G_DropInventoryItem_Flag`. Reading a call site's
  chain then only means grepping for the hook's name.
- The tail of the chain: the dispatch pointer suffixed `_Common` —
  `G_ResetDroppedItem_Common`. Every suffix names where an implementation *comes
  from*, which is the whole rule: `_Ctf`, `_Hook`, `_Tech` name a feature, and
  `_Common` names the shared sources. It is deliberately not `_Default`: the tails
  live in `common` and serve every module, so naming them after one of the three
  would invite the question of why default's code is in common.
- Avoid a leading underscore: C reserves `_` followed by an uppercase letter.

### Where things live

- **Declarations** (typedef + `extern`) go in `g_module.h`, so there is one
  authoritative list of every variation point.
- **Tails** go in the domain file that owns the behaviour — `g_item.c` for items
  and for parting a client from them, `g_combat.c` for damage, `g_entity.c` for the
  level, `g_client.c` for movement, `g_rules.c` for the rules. Never a catch-all:
  `g_module.c` existed for exactly two tails and was deleted, because a file that
  accumulates every hookable function in the game is what this design exists to
  prevent.
- **A file invented to dodge the shadowing rule should not outlive it.**
  `g_inventory.c` existed only because `src/game/common/g_item.c` was impossible
  while each module still had one; it held three hook tails whose neighbours were
  all in `g_item.c`, one of which did nothing but call a function 170 lines away in
  that file. Once `g_item.c` became common the reason was gone, so the file is too.
  The name borrowed its justification from `cg_inventory.c`, but that is a real
  subsystem — the client's read-only view for the HUD — whereas the game's
  "inventory" is an array on the client that item code writes.
- `g_local.h` is common, like `cg_local.h` always was. What made it look
  per-module was `GAME_NAME`, which is manifest and now sits in `g_types.h` beside
  `PROTOCOL_MINOR`, and the optional feature headers, which are guards like any
  other. A module's own headers need no seam here: the module's own sources include
  them, and common must not know they exist.
- A common file cannot share a name with a file that still exists in *any*
  module, because `vpath` resolves the module's own copy first. That is why the
  order of work is always: make both copies identical, delete both, then add the
  one in common. This is worth knowing in both directions: `g_inventory.c` was
  invented to dodge the rule while `g_item.c` was still forked, and outlived its
  reason by three commits.

## State of the work

Every duplicated `.c` file is gone. `src/game/default` and `src/game/ctf` now
hold only their manifest, and the seam where they install their own behaviour:

    bg_item.c  bg_item.h  g_types.h  g_module.c  Makefile.am

Everything else is one copy in `src/game/common`, compiled once per module.
`ctf` builds it with `-DG_CTF -DG_HOOK -DG_TECH`; `default` builds it with none of
those; `lithium` takes `-DG_HOOK -DG_TECH`. A mod is its manifest, its
`Makefile.am`, and the features it switches on.

### Lithium, which is the proof

`src/game/lithium` is deathmatch with the grappling hook and the techs - "CTF
minus the CTF". It shares no source with `ctf`; it was built from `default`'s
manifest by following what `bg_hook.h` and `g_tech.c` say a module adopting them
must supply, and it cost **125 lines** over that manifest:

| | lines |
| --- | --- |
| `bg_item.c` - the five tech item definitions | 74 |
| `g_types.h` - `CS_HOOK_PULL_SPEED`, `MOD_HOOK`, `TE_HOOK_IMPACT`, `TRAIL_HOOK`, `STAT_TECH`, and the two per-client structures | 24 |
| `bg_item.h` - `ITEM_TYPE_TECH` and the `TECH_*` tags | 13 |
| `g_module.c` - docblocks; both functions are still empty | 8 |
| `Makefile.am` - two defines, two sources | 6 |

No new logic, and none of the hook or tech behaviour was touched to make room for
it. On `edge` it spawns 85 entities where `default` spawns 80: the five techs,
placed by the `G_TECH` guard in `G_ResetItems`. The composition is visible in the
built objects - `lithium/game.so` carries `G_HookThink` and `G_Tech_Init` and no
`G_Ctf_Init`, and no `item_flag_team*` anywhere in its roster.

The one thing the feature docblocks did not mention, and now do not need to,
is `CS_HOOK_PULL_SPEED`: `bg_hook.h` listed `MOD_HOOK`, `TE_HOOK_IMPACT` and
`TRAIL_HOOK` as the wire values a module must number, but the config string the
`ConfigureLevel` hook publishes is a fourth. Following the docblock alone would
have produced a module that did not compile.

### The hooks

| hook | tail lives in | installed by |
| --- | --- | --- |
| `ResetDroppedItem` | `g_item.c` | ctf, techs |
| `DropInventoryItem` | `g_item.c` | ctf, techs |
| `TossInventory` | `g_item.c` | ctf, techs, hook |
| `ModifyDamage` | `g_combat.c` | techs |
| `CheckCvars` | `g_rules.c` | ctf, techs, hook |
| `CheckWinCondition` | `g_rules.c` | ctf |
| `FormatGameName` | `g_rules.c` | ctf |
| `ResetItem` | `g_item.c` | ctf |
| `InhibitItem` | `g_item.c` | ctf |
| `InitItem` | `g_item.c` | ctf, techs |
| `InitMedia` | `g_entity.c` | ctf, techs, hook |
| `ConfigureLevel` | `g_entity.c` | techs, hook |
| `PrepareMove` | `g_client.c` | hook |

The tail of each chain lives beside the code that calls it, never in a catch-all:
`g_module.c` was deleted once its last default moved out, because a file that
collects every hookable function in the game is the thing this design exists to
avoid. `g_rules.c` is new, and owns the rules a module enforces.

Every tail is named `G_TheHook_Common` and every installation
`G_TheHook_Feature`, so the whole of a variation point is one grep, and the suffix
always answers "where does this come from".

### What stayed a guard

Additive one-liners on manifest fields, where a hook would be ceremony:
the team roster's `.flag` and `.effect`, the capture and tech scoreboard stats,
`g_level.captures`, `cl->persistent.captures`, the grapple's per-client state,
the `MOD_HOOK` obituary and weapon name, the haste refire scaling, the vampire
heal, and the tech branches of `G_ResetItems` and `G_ClientThink`.

A guard MUST name a **feature** - `G_CTF`, `G_HOOK`, `G_TECH` - and never a
module. The distinction is what the define means, not what it is spelled: a guard
on a feature asks "was this behaviour built?", which shared code is entitled to
ask, while a guard on a module asks "am I inside ctf?", which puts knowledge of
every module that will ever exist into shared code. `g_module.h` says not to do
the second.

`G_CTF` is the **capture the flag feature**, which `src/game/common/g_ctf.c`
implements: the captures, the capture limit, the win condition, forced team play,
the flag items and their effects. That the ctf module is named after the mode it
was built to play is a coincidence of naming, and the module has no define of its
own - there is nothing for a guard to reach for. This is also why the feature
moved into common at all rather than staying in `src/game/ctf`: without it, every
capture-shaped divergence in a shared file had no legal mechanism. Nine guards in
the shared sources predated this and were spelled as the module - the bots'
flag-carrier priority and chase chance, the flag item in the bots' want-item test,
the carrier trails and effects, and the Discord game mode string. They are part of
the feature, so a mod that builds it gets bots that hunt carriers and a client
that draws the trails.

It is `G_CTF` and `g_ctf.c` rather than `G_FLAG` and `g_flag.c` because "flag" is
badly overloaded in this codebase - `spawn_flags`, `sv_flags`, `dflags`, a score's
`flags`, the `EF_` and `SF_` bits - and because the feature is more than the item.
"Flag" is kept only where it means the item: `G_TossFlag`, `G_PickupFlag`,
`G_TeamForFlag`, `G_FlagForTeam`. Everything naming the feature is `_Ctf`:
`G_Ctf_Init`, `G_CheckCvars_Ctf`, `G_ResetItem_Ctf`.

A guard MUST also be balanced. A guard that opens on `}` or `else`, straddling a
brace so that the two branches of the preprocessor close different blocks,
compiles and then breaks the next person to edit around it. Restate the condition
and make the block additive instead.

### The client side

`cg_hud.c` and `cg_score.c` were the same fork on the client, and moved the same
way, on `G_CTF` and `G_TECH` guards. `cg_team_mode.c` stays per-module: the list of
team modes a mod offers is a manifest, like the item roster.

The cgame gets the **same** feature defines as its game module, in all three build
systems, and it includes that module's own `g_types.h`. That is what keeps the two
sides of the wire from disagreeing; a define set that differs between a module's
game and cgame is the layout fault described above, and it would present as a
network fault rather than a build failure.

### Diagnostics

`G_Debug`, `G_Warn`, `G_Error`, `G_Ai_Debug` and the `Cg_` three are macros, and
that is deliberate: they supply `__func__`, which C gives a function no way to
learn about its caller. They take the shape `src/common/common.h` established -
a prefixed macro over the real function behind it - and they live in the common
`g_main.h`, beside the `gi` they wrap, rather than in each module.

Two rules come out of the way they used to be written:

- **A diagnostic macro MUST be prefixed for its layer.** The game's were once bare
  `Warn` and `Error`, and because the preprocessor expands a function-like macro
  wherever the identifier is followed by a paren - it does not care that a dot
  precedes it - they rewrote member accesses. `gi.Warn(fmt, ...)` compiled as
  `gi.Warn_(__func__, fmt, ...)`, so `gi.Warn` was not a member of `g_import_t` at
  all and looking it up in `game.h` found nothing.
- **Do not gate on the debug mask.** `Com_Debugv_` returns early on an inactive
  mask, so a gate in the macro only skips formatting the arguments. The one place
  that is worth having is `Pm_Debug`, called per move from the movement loop, and
  it spells the gate `do while` rather than as a GNU statement expression - which
  compiles on Windows only because the toolset is ClangCL.

### `bg_` is for what both sides use

`bg_pmove.{c,h}` and `bg_item.{c,h}` are compiled into the game *and* the client
game, and that is what the prefix means. `g_types.h` is not `bg_` despite the
client game including it, because it is the game module's own manifest, which the
client game borrows wholesale - so the two prefixes are not in conflict.

`bg_hook.h` and `bg_tech.h` were `g_hook_types.h` and `g_tech_types.h`. They exist
because `g_types.h` must embed `g_client_hook_t` in `g_client_t`, and `g_hook.h`
cannot be the source of it: `g_hook.h` includes `g_types.h` for the `g_client_t`
its own declarations take, and `__GAME_LOCAL_H__` is defined for the whole
translation unit, so folding the types into `g_hook.h` puts its declarations in
front of the types they need. That is not a style choice, it is a cycle - it was
tried, and the compiler says `unknown type name 'g_client_t'`.

They are `bg_` rather than `g_` because both sides genuinely use them:
`Hook_StyleName` and `Hook_StyleByName` live in `bg_hook.h`, so the three style
names the client game offers in its menu are the same three the game parses out of
a cvar and a client's user info, rather than two lists that must agree by
inspection. There is no `bg_hook.c`, because those two functions are all the shared
code there is.

### Where a mod installs its own hooks

`G_Init` lives in `common/g_main.c` now, and it calls each shipped feature behind
its own define:

```c
#if defined(G_HOOK)
  G_Hook_Init();
#endif
#if defined(G_TECH)
  G_Tech_Init();
#endif
#if defined(G_CTF)
  G_Ctf_Init();
#endif

  G_Module_Init();
```

A mod Quetoo does not ship cannot add a line to that list, and a guard named
after it could never be committed here, so **`G_Module_Init` is the seam it owns**:
declared in `g_module.h`, defined by every module in its own `src/game/<mod>/g_module.c`,
and called last. Without it a mod could compose the features common ships but
could not add behaviour of its own without forking `g_main.c`, which is the fork
this whole exercise removed.

Being called last means a module's hooks sit at the head of every chain, so they
may wrap a shipped feature rather than only precede the tail. `default` and `ctf`
both define it empty, because everything they do is either a common feature or the
default itself.

`G_Module_Shutdown` pairs with it, called from `G_Shutdown` before the game's
memory tags are freed so a module can still touch what it allocated. It is for
resources, and it **MUST NOT uninstall hooks**: `G_Init` and `G_Shutdown` run on
every server initialization, while a hook installs exactly once per module image
behind its `installed` guard, so uninstalling would tear a link out of a chain the
next `G_Init` declines to rebuild. Chains are built once and left. That asymmetry
is the same one that makes the `installed` guard necessary in the first place, seen
from the other end.

### What is left

1. **Nothing structural.** Lithium builds in all three systems, so the pattern is
   exercised end to end by three modules that share every line of behaviour.
2. **Do not merge `common` into `default`.** It looks tempting - `default` is a
   shell of four files, and a shorter `vpath` would make the pattern read as "a mod
   overrides default". It is a trap. `#include "..."` searches the *including
   file's own directory* first, ahead of every `-I`, so a shared source living in
   `default/` that includes `"g_types.h"` gets **default's** manifest - its wire
   values, its item roster - while compiling ctf or lithium. Clean build, network
   fault. Demonstrated: a probe in `default/` compiled with `-I ctf` first read
   default's value. It works today only because `common/` holds no `g_types.h`, so
   the quoted include falls through to `-I`. `common` is what makes per-module
   manifests possible; it is not ceremony.
3. **The manifest stays forked. That is the decision, not a deferral.** It is now
   the whole of the remaining duplication - measured:

   | file | lines | diverging |
   | --- | --- | --- |
   | `g_types.h` | 1726 | 98 |
   | `bg_item.c` | 820 | 130 |
   | `bg_item.h` | 232 | 23 |
   | `g_local.h` | 58 | 3 |

   ~2500 duplicated lines expressing ~250 lines of real difference, almost all of
   it wire values inserted mid-enum: `STAT_CAPTURES` and `STAT_TECH`,
   `TE_HOOK_IMPACT`, `TRAIL_HOOK`, the `EF_CTF_*` bits, `CS_HOOK_PULL_SPEED`, and
   the flag and tech item tags.

   Sharing them behind the feature defines would be less code, and arguably less
   error prone, because a module's game and cgame include the same header and so
   could not drift apart by hand. **Do not do it.** A mod that Quetoo does not
   ship - and most of them will not be - can only touch files inside its own
   module directory. A modder adding a stat, an item, a temp entity or an effect
   edits *their* `g_types.h` and *their* `bg_item.c`; if those lived in `common`
   they would have to fork Quetoo to add a single wire value, and a guard named
   after their mod could never be committed here anyway.

   So the duplication is not a defect to remove, it is the interface: the manifest
   is the part of a module a modder owns outright, and its size is the price of
   their independence. Improve the template if it helps them, but do not move it.

### Behaviour that changed on purpose

Each is small, and each is a consequence of a chain replacing a hand-written list
of calls:

- `TossInventory` runs in reverse installation order, so flags, techs and the
  grapple are shed first, where death used to shed them last - after the quad,
  the powerups and the weapon. Nothing reads another's inventory slot, so only
  the order the dropped entities spawn in changes.
- Death used to toss a tech only `if (G_Tech_Enabled())`. It now always tosses,
  which matters only for a client still holding a tech after techs were turned
  off mid-level: they now drop it instead of keeping it.
- `G_Tech_CheckState` and `G_Hook_CheckState` lost their `enabled_by_default`
  argument. Every caller passed `true`, and with the features compiled in per
  module, compiling one in is the module saying it wants it.
- Resist, quad and strength still scale damage in that order, because the tech
  implementation calls super between them. Chain order is behaviour.
- A flag whose team is not playing is now hidden outright. It used to be hidden
  before `G_ResetItem` read `SF_ITEM_NO_TOUCH`, so a map that set that flag on
  such a flag got `SOLID_BOX` back; the hook runs after super, so `SOLID_NOT`
  wins. Reachable only for a map that marks an out-of-play team's flag
  no-touch.
- The feature cvar announcements now all print at the end of `G_CheckRules`,
  in chain order, rather than interleaved with the core ones. `restart` is acted
  on at the same point either way.

## Next: carrying the pattern into the client game

The client game is where the game side was two days ago, minus the duplication:
its fork is already gone. Each cgame module holds one file.

    src/cgame/{default,ctf,lithium}/  cg_team_mode.c  Makefile.am

What it does *not* have is any of the three mechanisms. There is no `cg_module.h`,
no chainable hook, and no seam a mod owns - a cgame mod today can only compose
what common ships, exactly the gap `G_Module_Init` closed on the game side. All of
its variation lives in **33 feature guards across 15 files**:

| file | G_CTF | G_HOOK | G_TECH |
| --- | --- | --- | --- |
| `cg_hud.c` | 5 | | 1 |
| `cg_entity_trail.c` | 4 | 3 | |
| `cg_score.c` | 3 | | |
| `cg_main.c` / `cg_main.h` | | 6 | |
| `cg_media.c` / `cg_media.h` | | 3 | |
| `cg_temp_entity.c` | | 2 | |
| `ui/controls/MovementCombatViewController.c` | | 2 | |
| `cg_entity_effect.c` | 1 | | |
| `cg_discord.c` | 1 | | |
| `cg_predict.c` | | 1 | |
| `cg_types.h` | | 1 | |

### Check these two premises before designing anything

Both were assumed on the game side and turned out to matter. Neither is verified
for the cgame:

1. **Does `Cg_Init` run more than once per process?** It does - `Cl_InitCgame` is
   called from client startup *and* from `Cl_Frame` whenever `game->modified`,
   which is precisely what switching mods does. `Cl_ShutdownCgame` calls
   `Sys_CloseLibrary`, but `dlclose` does not reliably unload on macOS, which is
   what makes the `installed` guard load-bearing on the game side. **Confirm
   whether cgame file statics survive a `game` change** before trusting anything
   to run once. If they do, every install needs the same `static bool installed`,
   and the failure mode is the same beachball.
2. **There is no `__CGAME_LOCAL_H__`.** The game headers hide their declarations
   behind `#if defined(__GAME_LOCAL_H__)`, which is what lets `g_module.h` be
   included from anywhere safely. `cg_local.h` defines no such macro. Decide
   whether `cg_module.h` needs one before writing it, and remember the include
   cycle that shape exists to prevent: `g_hook.h` cannot hold its own types
   because `g_types.h` must embed them first.

### The hooks worth extracting

In descending order of how much guard they retire. Each mirrors a game-side hook,
so take the name from the server where one exists:

| candidate | retires | notes |
| --- | --- | --- |
| `DrawHud` | `cg_hud.c`'s 5 + 1 | features draw their own elements: the carried flag, the capture count, the tech icon. The layout shift - the timer moving up when there is no capture count - is the interesting part, and is the same shape as `ModifyDamage`: a value several features adjust |
| `AddEntityTrail` | `cg_entity_trail.c`'s 7 | the largest single cluster; flags and the grapple each add a trail |
| `DrawScore` | `cg_score.c`'s 3 | the team line, the carrier icon, the per-player captures. The "%d frags" against "%d captures" line is a replacement, so it is a not-chained hook, like `CheckWinner` |
| `FormatGameName` | `cg_discord.c`'s 1 | **the same hook already exists on the game side.** Give it the same name; a mod naming its mode should say so once |
| `AddEntityEffects` | `cg_entity_effect.c`'s 1 | small, but pairs with the trail hook |

### What should stay a guard

Additive one-liners on state a module declares, which is what guards are for:
`cg_types.h`'s `hook_pull_speed` in `cg_state_t`, the `cg_media.{c,h}` indices,
`cg_predict.c`'s single prediction branch, and the menu outlets in
`MovementCombatViewController.c`, which are driven by a JSON resource rather than
code. `cg_main.{c,h}`'s six are worth reading before deciding: some are wiring
that a `Cg_Module_Init` would absorb.

`cg_team_mode.c` stays per-module. A list of the team modes a mod offers is a
manifest, like the item roster.

### Everything else carries over unchanged

The rules, the naming, `_Common` tails in domain files rather than a catch-all
`cg_module.c` in common, a guard naming a feature and never a module, and the
prohibition on a gate around a diagnostic. The two contracts become
`Cg_Module_Init` and `Cg_Module_Shutdown`, defined per module in
`src/cgame/<mod>/cg_module.c`, called from `Cg_Init` and `Cg_Shutdown` after the
shipped features, so a mod's hooks sit at the head of every chain.

### Verifying it

The cgame is the visual half, so the check is a screenshot rather than an entity
count. The recipe that works: run the client as its own listen server, because a
client cannot connect to a dedicated server holding the same port -

    printf 'wait\n%.0s' {1..1200} > "$WRITE_DIR/probe.cfg"
    echo 'r_screenshot' >> "$WRITE_DIR/probe.cfg" && echo quit >> "$WRITE_DIR/probe.cfg"
    quetoo +set game lithium +set sv_min_clients 4 +map edge +exec probe.cfg

then read `$WRITE_DIR/screenshots/`. Enough `wait` lines to get past the map load,
or the screenshot is of the console. Compare all three modules: `default` has no
capture count and the timer sits where it would be, `ctf` has one, `lithium` has
the tech icon and no capture count. That last combination has never been looked at
- it is new with lithium, and it is the one the layout hook could break.

And the rule that makes all of this necessary: **a module whose manifest differs at
all needs its own cgame.** Lithium's wire values are shifted from default's by the
hook and tech insertions, and with no `lithium/cgame.so` the client silently loads
`default/cgame.so` off the search path, with every stat and temp entity one index
out and no warning.

## Watch out for

- **The `Drop` contract.** `it->Drop(cl, it)` receives the item with the
  inventory *already adjusted* by the caller. `G_DropTech` and `G_DropFlag` used
  to ignore their argument and re-derive the item from inventory, so once the
  generic path called them they silently dropped nothing. Any implementation
  behind a hook must use what it is handed.
- **Layout, not logic, is the usual failure.** `G_HOOK` adds `hook_pull_speed` to
  `cg_state_t`, shifting every field after it. A module compiled with a different
  define set than the objects it links reads those at the wrong offsets and
  presents as a network fault. This is why every common source is compiled once
  per module rather than shared, in all three build systems.
- **The server's view of a client and an entity is the first member of the
  module's.** `game.h` declares `sv_game_client_t` and `sv_game_entity_t` - the
  fields the server reads, in the order it reads them - and a module's
  `g_client_s` and `g_entity_s` embed the matching one as their first member.
  Because C guarantees that a pointer to a structure points at its first member,
  the two views of the same memory are a fact of the language, not a rule anyone
  has to remember: there is nothing a module can write that moves those fields.

  It is an *anonymous* member, so the fields are still reached without naming it -
  `cl->entity`, `ent->s.origin` - which is why the game targets compile with
  `-fms-extensions` and `-Wno-microsoft-anon-tag` in all three build systems. The
  server needs neither; only a module embeds.

  This replaced a hand-copied mirror of the field list in `g_module.h` and 19
  `static_assert`s over it - three copies of the same list, one of which existed
  only to check the other two. The assertions could compare offsets and sizes but
  not types, and they had been passing over a real discrepancy: `game.h` said
  `struct g_ai_s *ai` where every module said `struct ai_s *ai`. Both are
  pointers, so both are the same size at the same offset.

## Adding a common source file

All three build systems, or the Windows build rots silently:

1. `src/game/{default,ctf}/Makefile.am` — add to `_SOURCES` (alphabetical).
2. `Quetoo.vs15/game_common.props` — add a `ClCompile`; use a
   `Condition="'$(QuetooGameX)'=='true'"` group if it is an opt-in feature.
3. `Quetoo.vs15/game{,-ctf}.vcxproj.filters` — add under `src\common`.
4. `Quetoo.xcodeproj/project.pbxproj` — one `PBXFileReference`, one
   `PBXBuildFile` *per target*, add to the `src/game/common` group and to each
   target's Sources phase. Keep the phase entries alphabetical.

Cross-check afterwards that each module's source list matches its `Makefile.am`
in both directions. Verifying a list is complete is not the same as verifying its
entries resolve.

### Adding a module, or duplicating a target

Duplicating an Xcode target copies the *original's* file references, and they are
what decide which manifest gets compiled. A copy of `game-ctf` therefore builds
ctf's `g_types.h` and `bg_item.c` under the new target's name, with ctf's
`USER_HEADER_SEARCH_PATHS` and ctf's defines, and it builds cleanly. It happened;
the target had to be repointed at `src/game/lithium` in four places - Sources,
Headers, the search paths, and the defines - and its install script, which was
still copying over `ctf/game.so`.

Worse, the duplication moved `g_ctf.c` and `G_CTF` *out of* `game-ctf`, which
still linked, because with `G_CTF` undefined nothing references `G_Ctf_Init`. Xcode
had quietly been building ctf without capture the flag. **After duplicating a
target, check what the original lost**, not only what the copy gained.

The cheap check for all of it is comparing each target's Sources against its
`Makefile.am`, in every build system, which is how both of these were found.

### Moving a forked file into common

The `_SOURCES` entry does not move: `vpath` already searches `src/game/common`
after `srcdir`, so a file listed in both `Makefile.am`s resolves to whichever copy
exists. Only the two project files need editing.

1. Make the two copies **byte-identical** — `diff` must be silent — then delete
   both and add the one in `src/game/common`. A common file that shares a name
   with a surviving module file is shadowed by it, silently.
2. `Quetoo.vs15`: move the `ClCompile`/`ClInclude` from `game.vcxproj` and
   `game-ctf.vcxproj` into `game_common.props`, and retarget both
   `.vcxproj.filters` entries to `src\common`.
3. `Quetoo.xcodeproj`: keep **one** `PBXFileReference` and point every
   `PBXBuildFile` at it, delete the other reference, and move the surviving one
   from the module group into the `src/game/common` group. The per-target
   `PBXBuildFile` objects are what keep the file in each target, and for an
   opt-in feature they are what make it opt-in.

## Verifying

- Autotools: `./configure && make`.
- Xcode: build through `Quetoo.xcworkspace`, not `-project`, so the Objectively
  frameworks resolve from the sibling checkouts.
- Windows: there is a `build-windows` job in `.github/workflows/build.yml`, so
  the MSVS projects are verifiable without a Windows machine. `gh pr checks`,
  then `gh run view <run> --log`.
- Runtime, modules: build a fake bundle so per-module paths resolve as installed
  — engine at `Quetoo.app/Contents/MacOS/quetoo`, each module's `game.so` and
  `cgame.so` under `lib/quetoo/{default,ctf}/`, symlinks for
  `lib/quetoo/default/{ui,shaders}` and `Contents/Resources`. `Fs_Init` keys off
  `Quetoo.app` in the executable path.
- `edge` spawns 80 entities for default and 85 for ctf — a quick read on which
  module actually loaded, and on whether the five techs spawned.
- Bots: `+set sv_min_clients 6`. They fight and die, which tosses flags and
  techs, whose expiration is what walks the `ResetDroppedItem` chain.

### Observability traps

- The dedicated server takes the terminal with curses unconditionally
  (`Sv_InitConsole`), so its output only appears through a pty — wrap it in
  `script -q <log>`. A plain redirect is block buffered and loses everything if
  the process is signalled. `script` needs a controlling terminal of its own: run
  it in the foreground, because started with `&` from a non-interactive shell it
  writes an empty log.
- **Keep `-dedicated` in the executable's name.** `main.c` decides `dedicated`
  by matching that substring against `Sys_ExecutablePath()`, so a bundle that
  installs the binary as, say, `quetoo-srv` runs with `dedicated` 0. It still
  loads the map and runs frames, but `Sv_BroadcastPrint` only echoes to the
  console when `dedicated` is set, so every obituary and bot join disappears from
  `quetoo.log` while the game underneath is perfectly healthy. That reads exactly
  like bots failing to spawn.
- The client routes `Print` to its in-game console once initialised, so its
  engine output does *not* reach stdout. `Sys_UserDir()/<game>/quetoo.log` has it,
  but **not** `gi.ClientPrint` output, so per-client messages need a human
  in-game.
- **`Sys_OpenLibrary`'s path is a request, not a fact.** dyld searches
  `DYLD_LIBRARY_PATH` for a library's *leaf name* before it honours the path it
  was given, and every module is named `game.so` or `cgame.so`. Xcode injects
  that variable with each target's build directory when it launches a scheme, so
  two modules building into their own directories made every `dlopen` resolve to
  whichever came first in the list -- the engine reported loading `ctf/game.so`
  and then ran default's code, with a file on disk that was demonstrably
  correct. A scheme environment variable does not help: Xcode prepends yours and
  appends its own. The fix is to keep the products out of that search path,
  which the module targets now do by building under their own names at the
  products root and installing a correctly named copy into a directory Xcode
  never adds. If you add a fifth module, it needs the same install script, and
  the copy needs signing because the script runs before Xcode's CodeSign step.
  `Sys_LoadLibrary` reports the image `dladdr` attributes the entry point to,
  and `Sys_OpenLibrary` warns when that is not the file it asked for -- believe
  those two lines over the `Loading ...` line above them.
- Kill leftover servers between runs. A stale process holding port 1998 makes the
  next one die with `bind: Address already in use`, which looks exactly like a
  crash you just introduced.
- Scripting client commands: a cfg of N `wait` lines then the command then `quit`,
  exec'd from the game's write directory. `Cmd_Wait_f` defers one frame. Let the
  process exit on its own so stdio flushes.
