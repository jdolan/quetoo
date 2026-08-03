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

So **78% of the fork is pure duplication**, and every fix to `default` has to be
carried across by hand or it silently rots. The goal is that `default` is little
more than a manifest over `src/game/common`, and that a mod is its manifest plus
the handful of behaviours it genuinely changes.

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

4. **Single-owner contract.** A plain function declared in `g_module.h` that
   every module defines, where exactly one answer is possible and chaining is
   meaningless. A missing implementation is a link error.

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
  wearing one name. `G_CheckRules` is a win condition (single owner) plus a pile
  of per-feature cvar blocks (hook); `G_worldspawn` is a feature config publish
  plus two ctf level settings.

## The chainable hook

Common holds the default; a feature installs itself over the top, keeping the
previous value to call as super.

```c
/* g_module.h — the contract, and the single authoritative list of hooks */
typedef void (*ResetDroppedItem)(g_entity_t *ent);

extern ResetDroppedItem G_ResetDroppedItem;
```

```c
/* g_inventory.c — the domain file owns the default */
static void FreeDroppedItem(g_entity_t *ent) {
  G_FreeEntity(ent);
}

ResetDroppedItem G_ResetDroppedItem = FreeDroppedItem;
```

```c
/* g_tech.c — a feature installs over the top */
static struct {
  ResetDroppedItem ResetDroppedItem;
  DropInventoryItem DropInventoryItem;
} super;

static void TechResetDroppedItem(g_entity_t *ent) {

  if (ent->item->def.type == ITEM_TYPE_TECH) {
    G_ResetDroppedTech(ent);
    return;
  }

  super.ResetDroppedItem(ent);
}

/* in G_Tech_Init */
super.ResetDroppedItem = G_ResetDroppedItem;
G_ResetDroppedItem = TechResetDroppedItem;
```

`g_flag.c` does the same for flags, independently. Composition then falls out of
which features a module builds:

| module | chain |
| --- | --- |
| default | `FreeDroppedItem` |
| ctf | flag → tech → `FreeDroppedItem` |
| a techs-only mod | tech → `FreeDroppedItem` |

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
- Feature implementation: the feature name then the hook —
  `TechResetDroppedItem`, `FlagDropInventoryItem`.
- Default implementation: named for **what it does**, not "default" —
  `FreeDroppedItem`, `DropItemByName`.
- Avoid a leading underscore: C reserves `_` followed by an uppercase letter.

### Where things live

- **Declarations** (typedef + `extern`) go in `g_module.h`, so there is one
  authoritative list of every variation point.
- **Defaults** go in the domain file that owns the behaviour — `g_inventory.c`
  for the inventory and dropping, not a catch-all. `g_module.c` holds only what
  has no better home.
- A domain file cannot be named after a file that still exists per-module.
  `src/game/common/g_item.c` is impossible today because `vpath` would have each
  module's own `g_item.c` shadow it. That is why the inventory default lives in
  `g_inventory.c`, which parallels `cg_inventory.c` on the client side.
- `g_inventory.h` does not exist yet; add it when `G_AddAmmo`, `G_SetAmmo`,
  `G_InitClientInventory` and `G_ClientInventoryThink` migrate there.

## State of the work

Done:

| hook | absorbed | result |
| --- | --- | --- |
| `ResetDroppedItem` | `G_ResetDroppedItem`, `G_DropItem`, `G_DropItem_SetExpiration` | the latter two are byte-identical between modules |
| `DropInventoryItem` | `G_Drop_f` | both modules' command handler is one line |

Also done, as prerequisites: techs extracted to `g_tech.{c,h}` behind `G_TECH`;
`g_ai_item.c`'s item switch split so techs no longer require `G_CTF`;
`G_ResetDroppedFlag` moved to `g_flag.c` with a `G_Flag_Init`.

Current: **165 shared functions, 133 identical, 32 differing** (from 166 / 130 /
36).

### Remaining hooks

Chainable, roughly in order of yield:

| hook | absorbs | note |
| --- | --- | --- |
| `ModifyDamage` | `G_Damage` (213 lines, +21) | highest yield; the tech resist/strength/vampire modifiers. Also the first hook that *transforms values* rather than dispatching on type. |
| `CheckRules` | ~59 lines of `G_CheckRules`, `G_RestartGame`'s `*_CheckState` calls | every `g_hook_*` / `g_techs` `->modified` block; belongs in the feature, as `g_hook.c`'s docblock already says |
| `PrepareMove` | `G_ClientMove` (220 lines, +3) | `G_Hook_ApplyPmove` is already the seam; only the call site diverges |
| `InhibitItem` | `G_ResetItem`'s `inhibited` expression | flags opt out of arena/instagib inhibition |
| `ResetItem` | `G_ResetItem`'s visibility block | flag hidden when its team is not in play |
| `ConfigureLevel` | `G_worldspawn`'s `SetConfigString(CS_HOOK_PULL_SPEED, …)` | each feature publishes its own config strings |

Single-owner contracts:

| contract | why one owner |
| --- | --- |
| `FormatGameName` | ctf appends " CTF", default prefixes "Team " — a replacement, not additive |
| `CheckWinCondition` | capture limit versus frag limit; extract from `G_CheckRules` |
| `PostStats` | ctf passes its captures list alongside frags |

Guards, no mechanism: `G_ClientRespawn`'s `captures = 0`, `G_RestartGame`'s
captures resets, `G_worldspawn`'s `capture_limit` parsing and `g_level.teams = 1`.

### Suggested order

1. `ModifyDamage` — largest single body recovered, and proves the
   value-transforming shape.
2. `CheckRules` — removes the per-feature cvar blocks, the thing that shreds
   `g_main.c`.
3. `PrepareMove`, `InhibitItem`, `ResetItem`, `ConfigureLevel` — small.
4. The three single-owner contracts.
5. Then move the guarded files into `src/game/common` and delete ctf's copies.
6. Then Lithium: a `Makefile.am`, a `g_types.h`, a `bg_item.{c,h}`, `g_tech.c`,
   `g_hook.c`, and `-DG_HOOK -DG_TECH`.

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
  the process is signalled.
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
