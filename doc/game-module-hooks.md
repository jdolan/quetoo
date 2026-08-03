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
   `g_types.h`, `bg_item.h`, `bg_item.c`. These stay forked *by design* — a
   module numbers its own wire values, as `g_hook_types.h` already states. Do not
   try to share them. Roughly 265 of the diverging lines belong here.

2. **`#if` guard in common.** For additive divergence in few hunks. Duplicates
   nothing. This is right for most of the 24 additive functions.

3. **Chainable hook.** For a variation point that *several optional features may
   each want a say in*. See below. This is the interesting one.

4. **A hook that is not chained.** Where exactly one answer is possible - a win
   condition, the name of the gameplay - the mechanism is still a hook, but the
   feature installing it does not call super. Nothing enforces that, so the
   docblock on the hook MUST say which kind it is.

A fifth option always remains: **override the whole file**. `vpath` resolves a
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
/* g_inventory.c — the domain file owns the tail of the chain */
static void G_ResetDroppedItem_Default(g_entity_t *ent) {
  G_FreeEntity(ent);
}

ResetDroppedItem G_ResetDroppedItem = G_ResetDroppedItem_Default;
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

`g_flag.c` does the same for flags, independently. Composition then falls out of
which features a module builds:

| module | chain |
| --- | --- |
| default | `_Default` |
| ctf | flag → tech → `_Default` |
| a techs-only mod | tech → `_Default` |

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
- The tail of the chain: the dispatch pointer suffixed `_Default` —
  `G_ResetDroppedItem_Default`. It is the answer the plain deathmatch module
  gives, and the name it would keep if `common` folded back into `default`.
- Avoid a leading underscore: C reserves `_` followed by an uppercase letter.

### Where things live

- **Declarations** (typedef + `extern`) go in `g_module.h`, so there is one
  authoritative list of every variation point.
- **Tails** go in the domain file that owns the behaviour — `g_inventory.c` for
  the inventory and dropping, `g_combat.c` for damage, `g_item.c` for items,
  `g_rules.c` for the rules. Never a catch-all: `g_module.c` existed for exactly
  two tails and was deleted, because a file that accumulates every hookable
  function in the game is what this design exists to prevent.
- A common file cannot share a name with a file that still exists in *any*
  module, because `vpath` resolves the module's own copy first. That is why the
  order of work is always: make both copies identical, delete both, then add the
  one in common. `g_inventory.c` got its name because `g_item.c` was still forked
  at the time; it parallels `cg_inventory.c` on the client side, so it stays.

## State of the work

Every duplicated `.c` file is gone. `src/game/default` and `src/game/ctf` now
hold only their manifest:

    bg_item.c  bg_item.h  g_local.h  g_types.h  Makefile.am

Everything else is one copy in `src/game/common`, compiled once per module.
`ctf` builds it with `-DG_FLAG -DG_HOOK -DG_TECH -DG_CTF`; `default` builds it
with none of those. A mod is its manifest, its `Makefile.am`, and the features it
switches on.

### The hooks

| hook | tail lives in | installed by |
| --- | --- | --- |
| `ResetDroppedItem` | `g_inventory.c` | flags, techs |
| `DropInventoryItem` | `g_inventory.c` | flags, techs |
| `TossInventory` | `g_inventory.c` | flags, techs, hook |
| `ModifyDamage` | `g_combat.c` | techs |
| `CheckCvars` | `g_rules.c` | flags, techs, hook |
| `CheckWinCondition` | `g_rules.c` | flags |
| `FormatGameName` | `g_rules.c` | flags |
| `ResetItem` | `g_item.c` | flags |
| `InhibitItem` | `g_item.c` | flags |
| `InitItem` | `g_item.c` | flags, techs |
| `InitMedia` | `g_entity.c` | flags, techs, hook |
| `ConfigureLevel` | `g_entity.c` | techs, hook |
| `PrepareMove` | `g_client.c` | hook |

The tail of each chain lives beside the code that calls it, never in a catch-all:
`g_module.c` was deleted once its last default moved out, because a file that
collects every hookable function in the game is the thing this design exists to
avoid. `g_rules.c` is new, and owns the rules a module enforces.

Every tail is named `G_TheHook_Default` and every installation
`G_TheHook_Feature`, so the whole of a variation point is one grep. `_Default`
is also what these become if `common` is ever folded back into `default` and
mods override `default` directly.

### What stayed a guard

Additive one-liners on manifest fields, where a hook would be ceremony:
the team roster's `.flag` and `.effect`, the capture and tech scoreboard stats,
`g_level.captures`, `cl->persistent.captures`, the grapple's per-client state,
the `MOD_HOOK` obituary and weapon name, the haste refire scaling, the vampire
heal, and the tech branches of `G_ResetItems` and `G_ClientThink`.

A guard MUST name a **feature** - `G_FLAG`, `G_HOOK`, `G_TECH` - and never a
module. `#if defined(G_CTF)` in common would put knowledge of which modules will
ever exist into shared code, which is the thing `g_module.h` says not to do. This
is why the flags moved into common behind `G_FLAG` rather than staying in
`src/game/ctf`: without that, every capture-shaped divergence in a shared file
had no legal mechanism.

`G_CTF` is **no longer defined by anything**. The nine guards that used it - the
bots' flag-carrier priority, the flag item type, the CTF trails and effects, and
the Discord game mode string - were all flag-shaped and now say `G_FLAG`, so a
flags-only mod gets bots that chase carriers and a client that draws the trails.
A define that nothing consumes is an invitation to guard on a module again, so it
is gone from all three build systems rather than left lying around.

A guard MUST also be balanced. A guard that opens on `}` or `else`, straddling a
brace so that the two branches of the preprocessor close different blocks,
compiles and then breaks the next person to edit around it. Restate the condition
and make the block additive instead.

### The client side

`cg_hud.c` and `cg_score.c` were the same fork on the client, and moved the same
way, on G_FLAG and G_TECH guards. `cg_team_mode.c` stays per-module: the list of
team modes a mod offers is a manifest, like the item roster.

The cgame gets the **same** feature defines as its game module, in all three build
systems, and it includes that module's own `g_types.h`. That is what keeps the two
sides of the wire from disagreeing; a define set that differs between a module's
game and cgame is the layout fault described above, and it would present as a
network fault rather than a build failure.

### What is left

1. **Lithium.** A `Makefile.am`, a `g_types.h`, a `bg_item.{c,h}`, a
   `cg_team_mode.c`, and whichever of `-DG_FLAG -DG_HOOK -DG_TECH` it wants.
   Nothing else, which is the point.
2. **Recombining `common` and `default`.** `default` is now an empty shell over
   `common`, so the two could merge and let `ctf` and friends override `default`
   directly. The `_Default` naming already assumes this.
3. **`g_inventory.h`** still does not exist; add it when `G_AddAmmo`, `G_SetAmmo`,
   `G_InitClientInventory` and `G_ClientInventoryThink` migrate there.
4. **The manifest**, which is now the whole of the remaining duplication and
   needs a decision rather than a refactor. Measured:

   | file | lines | diverging |
   | --- | --- | --- |
   | `g_types.h` | 1726 | 98 |
   | `bg_item.c` | 820 | 130 |
   | `bg_item.h` | 232 | 23 |
   | `g_local.h` | 58 | 3 |

   So ~2500 lines are duplicated to express ~250 lines of real difference, and
   that difference is almost entirely **wire values**: `STAT_CAPTURES` and
   `STAT_TECH` inserted into `g_stat_t`, `TE_HOOK_IMPACT` into the temp entities,
   `TRAIL_HOOK`, the `EF_CTF_*` bits, `CS_HOOK_PULL_SPEED`, and the flag and tech
   item tags. Inserting into the middle of an enum shifts everything after it,
   which is exactly why `g_hook_types.h` says a module numbers its own.

   These could be shared, guarded on the feature defines, and it would be *safer*
   than the fork rather than less safe, because a module's game and cgame include
   the same header and so cannot drift apart by hand. But it makes the wire
   protocol a function of the define set, which is a decision about who owns
   numbering, and the doc has said until now that a module does. **That call is
   not a refactor; make it deliberately.**

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
- **`g_module.h`'s `static_assert`s** pin the leading fields of `g_client_t` and
  `g_entity_t` to what the server reads. Never reorder those; append only.

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
