# Quetoo

Quetoo is a first-person shooter engine and game derived from id Software's Quake II. It is written
in C, renders through SDL_GPU, and builds for macOS, Linux, BSD and Windows.

This file is the shared instruction set for coding agents. `CLAUDE.md` and
`.github/copilot-instructions.md` point here. Read this first.

It deliberately records only what a careful reading of the code does **not** reveal: rules that fail
silently, constraints that live outside this repository, and ordering that no call site shows. For
anything else, read the code. It is never out of date.

## Naming

| Category | Convention | Example |
|---|---|---|
| Types | `PascalCase`, subsystem prefix spelled out | `RenderEntity`, `CGameSprite`, `PlayerMoveParams` |
| Functions | `Prefix_PascalCase`, unchanged | `R_DrawMaterialStages`, `G_Damage` |
| Function-pointer members | `PascalCase` | `cgi.AddEntity`, `gi.Multicast` |
| Variables, parameters, data members | `camelCase` | `numElements`, `oldOrigin` |
| Cvars and console commands | keep the prefix, camelCase the rest | `r_swapInterval`, `cg_addDecals`, `+moveForward` |
| Enum constants and macros | `UPPER_CASE` | `MAX_CLIENTS`, `SURF_ALPHA_TEST` |

The rule is not "everything camelCases". **Case encodes a category.** A callable is PascalCase, data
is camelCase. That is why a function-pointer member keeps `cgi.AddEntity`, mirroring the
`Cl_AddEntity` it wraps.

- `Cm` stays short, because `src/collision` owns materials, manifests and entities, not only
  collision.
- A cvar or command with no subsystem prefix camelCases whole: `numPlanes`, `nextMap`.
- Where only one word follows the prefix, nothing moves: `r_gamma`, `m_pitch`.
- The file-static struct a module uses to collect its file globals is named `module`. A file holding
  more than one keeps descriptive names.
- File names: `snake_case` for a plain C module, `PascalCase` for a file that declares one
  Objectively class and is named after it (`ChatView.c`). This distinction is load-bearing — it tells
  you which kind of file you are opening. Do not "fix" it.

`Cvar_Get` and `Cmd_Get` still resolve an older snake_case spelling and warn when they do, so
existing configs keep working. Configs migrate themselves on save.

## Sibling repositories

All of these are jdolan's, checked out beside `quetoo/`. **You are free to change them.** When a
fix belongs in one of them, make it there rather than working around it here, and say so.

| Repository | What it is | Reach for it when |
|---|---|---|
| `../Objectively` | The C object system: classes, interfaces, `$(obj, method, ...)` dispatch, collections, threads, JSON, URL sessions | You need the object model itself, or a collection or `Thread` behaves unexpectedly |
| `../ObjectivelyGPU` | The GPU abstraction over SDL_GPU: devices, pipelines, passes, buffers, textures | A renderer call is missing, or a pipeline or pass does not behave as documented |
| `../ObjectivelyMVC` | The UI toolkit: Views, ViewControllers, the Selector / Style / Stylesheet system, `.json` layouts | Anything under `src/cgame/common/ui/` or `src/client/ui/` misbehaves. Read its `README.md` before touching a View |
| `../quetoo-data` | Game content: maps, textures, models, sounds, materials, HUD and UI icons | A `.mat` keyword, a model, a skin or an icon is involved |
| `../quetoo-www` | The website and server browser | Server listing or web-facing behaviour |
| `../quetoo-stats` | Stats collection | Player or match statistics |

### The installed copies are what you build against

The three Objectively libraries are consumed from `/usr/local/include` and `/usr/local/lib`, not
from the sibling checkout. Editing `../ObjectivelyMVC` changes nothing until it is rebuilt and
installed. **A stale installed header is invisible and produces confusing errors**, so when a
sibling's API does not match what you read in its source, suspect the installed copy first.

`/usr/local/share/quetoo` is normally a symlink to `../quetoo-data/target`, so content edits take
effect without installing.

### quetoo-data: `src/` versus `target/`

`src/` holds authoring sources. `target/` holds what ships and what the engine loads, laid out per
game module, with `target/default/` as the shared base every module falls back to. Engine chrome
that renders before a game module loads — backgrounds, fonts, conback, loading, the progress bar,
menu sounds — lives in **this** repo instead. In-game content, including HUD and UI icons under
`pics/`, lives in `quetoo-data`.

An entity is defined in four places across three repos, and drift runs both ways, including back
from TrenchBroom. Check all of them before concluding a definition is wrong.

## Rules that fail silently

These produce no error. They are the reason this file exists.

### Layout and appearance are CSS

Anything under `src/cgame/common/ui/` or `src/client/ui/` is ObjectivelyMVC. Before touching a View,
a ViewController, a `.json` layout or a `.css` stylesheet, read `../ObjectivelyMVC/README.md`, in
particular "Layout and styling are CSS-driven".

- You MUST NOT assign a `@styled` attribute in C. `alignment`, `autoresizingMask`, `padding`,
  `width` / `height`, `minSize`, `pointerEvents`, `StackView::axis`, `StackView::spacing` and
  friends are bound from the computed Style by `View::applyStyle` on every theme pass, so a C
  assignment is overwritten moments later. Check the header: if an attribute is marked `@styled`, it
  belongs in a stylesheet.
- C is for structure and behaviour only: creating and adding subviews, delegates, data source
  callbacks, and model values such as `Slider::min` / `max` / `value`.
- Target views from CSS by giving them a class (`$(view, addClassName, "foo")`) or an identifier,
  then write the rule in `ui/common.css`, the view controller's own `.css`, or
  `ui/hud/<hud>/hud.css`.
- ObjectivelyMVC's own `Assets/stylesheet.css` is always in effect. Its defaults include
  `StackView { axis: vertical }`, `Button { min-width: 100; padding: 8 8 8 8 }` and
  `Control { min-height: 32 }`. A compact icon button MUST overrule `min-width` **and**
  `min-height`; setting `width` alone will not shrink it.

When a view does not appear where or at the size you expect, read the stylesheets before changing
any code.

### An outlet identifier is a contract

`MakeOutlet("serverHostname", &this->hostnameLabel)` binds to the matching `"identifier"` in the
`.json` layout. Rename one and you MUST rename the other. Nothing fails at compile time.

## Constraints that live outside the code

Changing these breaks something this repository cannot see.

- **Cvar names flagged `CVAR_USER_INFO` or `CVAR_SERVER_INFO` are wire keys.** They are read by
  literal key through `InfoString_Get` and `Ms_InfoValue`, not through `Cvar_Get`, so the legacy
  lookup does not cover them. `src/master/main.c` parses `sv_hostname`, `sv_protocol`,
  `sv_maxClients` and `sv_map`, and the master is deployed separately. Renaming one requires
  redeploying the master, and servers are missing from listings until they upgrade.
- **Material keywords are a content format.** `alpha_test`, `no_draw`, `phong` and the rest in
  `cm_surfaceList` and the `Cm_LoadMaterial` parser are how every `.mat` file in `quetoo-data` and
  in user maps is written. They are not identifiers and MUST NOT be renamed with code.
- **GLSL has its own namespace.** Shader struct and function names are independent of the C names
  they mirror. A comment naming a C type should track the C name; the shader's own types should not.
- **Dependency members keep their own spelling.** `SDL_GPUTransferBufferLocation::transfer_buffer`
  and the vendored minizip `m_filename` are theirs. A rename MUST NOT reach our call sites for them.
- **Engine chrome lives here; in-game content lives in `quetoo-data`.** Backgrounds, fonts, conback,
  loading, the progress bar and menu sounds are in this repo. HUD and UI icons under `pics/` are in
  the sibling `quetoo-data` repo, under `target/default/` as the shared base every game module falls
  back to.
- **Protocol versions.** `PROTOCOL_MAJOR` is the engine wire format, `PROTOCOL_MINOR` is game-module
  behaviour. `GAME_API_VERSION` and `CGAME_API_VERSION` gate module loading and are a source-level
  contract for out-of-tree mods.

## Ordering that no call site shows

- `Cl_InitKeys` runs early in `Cl_Init` and calls `Cbuf_Execute()` immediately, so the default binds
  and the `quetoo.cfg` they exec run **before** `Cl_InitInput` registers `+moveLeft` and friends. A
  bind cannot be resolved as it is set. `Cl_CanonicalizeBinds` runs once at the end of `Cl_Init` for
  this reason.
- A config can set a cvar before the owning subsystem registers it. `Cvar_Set_` creates it through
  `Cvar_Add`, so it exists under whatever name the config used until `Cvar_Add` re-keys it.
- Renderer media are reaped by seed at the end of a load pass. Anything holding a `RenderMaterial *`
  across a level change MUST re-resolve it, or the pointer dangles.

## Tooling caveats

- **`clang-tidy` never parses code behind a platform conditional** it is not compiling. On macOS,
  `#if defined(_WIN32)` and `#elif defined(__linux__)` blocks are invisible to it, so an AST-driven
  rename silently skips them and the result fails to build on those platforms. Sweep guarded blocks
  by hand, and let CI build Linux and Windows before trusting a tree-wide change.
- `clang-tidy` rewrites code, never comments. A rename leaves every doc comment naming the old
  identifier.
- Macro arguments and `_Static_assert(offsetof(...))` at file scope are not rewritten either.

## Building

`make -j8` from the repo root builds everything via autotools.

```bash
autoreconf -i
./configure                      # add --with-tests for the unit tests
make -j8
make check                       # 22 Check suites under src/tests
```

Quetoo also ships Xcode (`Quetoo.xcodeproj`) and MSVC (`Quetoo.vs15/`) projects.

- Verify Xcode with `xcodebuild -workspace Quetoo.xcworkspace -scheme <target>`. A `-project` or
  `-target` build fails spuriously, because the Objectively frameworks are products of sibling
  projects referenced by the workspace.
- **A new source file MUST be added to all three build systems**: the module's `Makefile.am`,
  `Quetoo.xcodeproj/project.pbxproj`, and both `Quetoo.vs15/cgame_common.props` (the list MSBuild
  actually compiles) and the per-target `.vcxproj.filters`.
- CI (`.github/workflows/build.yml`) builds Linux and Windows on pushes to `main` and on pull
  requests against it. It is the only check that covers platform-guarded code.

## Conventions

`CONTRIBUTING.md` carries the commit and pull request conventions. Rationale belongs in the commit
body, which is where Quetoo's design decisions are recorded — search there before concluding that a
removal was a mistake.
