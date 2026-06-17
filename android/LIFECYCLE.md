# Quetoo Android — App Lifecycle & Writable Filesystem (SDL3)

Plan for handling the Android activity lifecycle and writable storage for the
Android port ([jdolan/quetoo#856](https://github.com/jdolan/quetoo/issues/856)),
work item **#5** in [`PORTING.md`](./PORTING.md).

This is a **design doc only** — no engine file is modified by it. Every insertion
point below is cited as `file:line — function` against the current worktree so the
implementation PR is mechanical.

---

## TL;DR — the port is unusually cheap here

Two facts found during investigation make this item small:

1. **The SDL event pump already exists and already routes system events.**
   `Cl_HandleEvents()` polls with `SDL_PollEvent` and dispatches every event to
   `Cl_HandleSystemEvent()` *first*. That function already handles
   `SDL_EVENT_QUIT`, `SDL_EVENT_WINDOW_FOCUS_LOST`, and `SDL_EVENT_KEY_DOWN`
   (ESC). The Android lifecycle events are just **new `case` labels in the same
   `switch`** — no new pump, no Java glue (SDL3's `SDLActivity` already pushes
   these events for us).

2. **The writable directory already goes through `SDL_GetPrefPath`.**
   `Sys_UserDir()` calls `SDL_GetPrefPath("WickedOldGames", "Quetoo")`, and the
   write dir is derived from it. On Android SDL3 returns the app's internal
   storage dir, so the existing path logic **already does the right thing** — the
   work is verification + the no-base-dir case, not rewiring.

The only genuinely new code is: ~5 `case` labels, one `bool cl_backgrounded`
gate, and a one-line throttle in the main loop.

---

## 1. The main loop and the SDL event pump (where lifecycle events arrive)

### Main loop
`src/main/main.c:500 — main()`

```c
while (true) { // this is our main loop
    ...
    do {
        quetoo.ticks = (uint32_t) SDL_GetTicks();   // main.c:517
        msec = (quetoo.ticks - old_time) * time_scale->value;
    } while (msec < 1);

    Frame(msec);                                    // main.c:521 -> Sv_Frame + Cl_Frame
    old_time = quetoo.ticks;
}
```

`Frame()` (`main.c:408`) calls `Sv_Frame()` then `Cl_Frame()` (`src/client/cl_main.c:632`).
`Cl_Frame()` is the client tick: it calls `Cl_HandleEvents()` (`cl_main.c:678`), then
`R_BeginFrame()` / renders / `R_EndFrame()` / `R_Screenshot()`.

### Event pump
`src/client/cl_input.c:440 — Cl_HandleEvents()`

```c
void Cl_HandleEvents(void) {
    if (!SDL_WasInit(SDL_INIT_VIDEO)) return;
    Cl_UpdateMouseState();
    while (true) {
        SDL_Event event;
        memset(&event, 0, sizeof(event));
        if (SDL_PollEvent(&event)) {
            if (Cl_HandleSystemEvent(&event) == false) {   // <-- system events first
                Ui_HandleEvent(&event);
                Cl_HandleEvent(&event);
                cls.cgame->HandleEvent(&event);
            }
        } else {
            break;
        }
    }
}
```

`src/client/cl_input.c:293 — Cl_HandleSystemEvent()` is the dispatch we extend.
It is a `switch (event->type)` that already owns the cross-cutting cases
(`SDL_EVENT_QUIT` at `cl_input.c:306`, `SDL_EVENT_WINDOW_FOCUS_LOST` at `:310`,
`SDL_EVENT_KEY_DOWN`/ESC at `:328`). **All lifecycle handlers go here.**

> Note on event delivery on Android: SDL3 delivers `WILL_ENTER_BACKGROUND` /
> `DID_ENTER_BACKGROUND` **synchronously** from `onPause()` via an event watch
> *before* `SDL_PollEvent` would normally see them, because Android may freeze
> the process the instant `onPause()` returns. SDL guarantees that registered
> **event watchers** (`SDL_AddEventWatch`) run inline during the OS callback. For
> the *background/foreground* pair we therefore also register a watcher (see §2)
> so the "stop audio + GL" work happens before the process is frozen. `LOW_MEMORY`
> and `TERMINATING` are fine to handle from the normal poll loop, but since they
> share the same handler function the watcher path covers them for free.

---

## 2. WILL_ENTER_BACKGROUND / DID_ENTER_FOREGROUND → pause / resume

**Goal:** when Android backgrounds the app, stop rendering (the EGL surface is
gone — drawing is undefined/will crash), pause audio, and stop burning CPU.
On return, resume.

**Why a watcher and not just the poll loop:** on Android the GL/EGL surface is
torn down during `onPause`; if `Cl_Frame()` runs `R_BeginFrame()` after that
without us having stopped, it touches a dead surface. We must react *before* the
process is frozen, which is what `SDL_AddEventWatch` guarantees.

### New state (client-local)
Add to `cl_static_t` (`src/client/cl_local.h`, the `cls` struct) — read-only here,
so the field is listed for the implementer:

```c
bool backgrounded;   // true between WILL_ENTER_BACKGROUND and DID_ENTER_FOREGROUND
```

### Insertion point A — the event watcher
Register once in `Cl_Init()` (`src/client/cl_main.c:714`), after `R_Init()` /
`S_Init()` so the subsystems exist:

```c
// cl_main.c, end of Cl_Init()
SDL_AddEventWatch(Cl_LifecycleWatch, NULL);
```

```c
// new, near Cl_HandleSystemEvent in cl_input.c
static bool SDLCALL Cl_LifecycleWatch(void *userdata, SDL_Event *event) {
    switch (event->type) {
        case SDL_EVENT_WILL_ENTER_BACKGROUND:
            cls.backgrounded = true;
            S_Stop();                                   // src/client/sound/s_main.c:178
            SDL_PauseAudioDevice(s_env.device);         // stop the audio device (see note)
            break;
        case SDL_EVENT_DID_ENTER_FOREGROUND:
            cls.backgrounded = false;
            SDL_ResumeAudioDevice(s_env.device);
            R_UpdateContext();                          // re-sync to the new surface size
            break;
        default:
            break;
    }
    return true; // keep the event in the queue for normal handling too
}
```

> `s_env.device` is the sound subsystem's `SDL_AudioDeviceID` (see
> `src/client/sound/s_main.c`, `S_Init`). If the current SDL3 audio path uses an
> `SDL_AudioStream` bound to the default device, pause via the bound device id
> SDL returns at open time. The exact field is an implementation detail of the
> SDL3 audio migration; the lifecycle code only needs *a* pause/resume call.

### Insertion point B — throttle / skip the frame while backgrounded
The cleanest gate is at the top of `Cl_Frame()` (`src/client/cl_main.c:632`),
mirroring the existing `if (dedicated->value) return;` guard at `cl_main.c:635`:

```c
// cl_main.c, top of Cl_Frame(), right after the dedicated guard
if (cls.backgrounded) {
    Cl_HandleEvents();          // still drain events so we can catch FOREGROUND/TERMINATING
    SDL_DelayNS(50 * 1000 * 1000); // ~50ms: yield CPU, no rendering/audio
    return;                     // skip R_BeginFrame/R_EndFrame entirely
}
```

This keeps the loop alive (so `DID_ENTER_FOREGROUND` and `TERMINATING` are still
seen) but does **zero GL work** while the surface is invalid, and stops the
busy-spin so Android doesn't kill us for CPU use.

> The renderer guard is the load-bearing part: `Cl_Frame()` calls `R_BeginFrame()`
> (`cl_main.c:680`) unconditionally today. Returning early avoids it. The server
> (`Sv_Frame`) is allowed to keep ticking if a local game is running — that is
> harmless and matches desktop alt-tab behavior — but on mobile a single-player
> pause is usually desired, so optionally also gate `Sv_Frame` in `Frame()`
> (`main.c:430`). Recommended: leave the server running (listen servers / online
> play must not pause); only the client render/audio pause.

---

## 3. SDL_EVENT_LOW_MEMORY → flush caches

**Goal:** on the OS low-memory signal, drop everything reclaimable without
breaking the current map.

The renderer media cache is the big consumer. It already has a **seed-based
eviction mechanism** we can reuse:

- `src/client/renderer/r_media.c:208 — R_EndLoading()` calls
  `g_hash_table_foreach_remove(..., R_FreeMedia_, (void *) 0)`, which frees every
  media object whose `seed` is stale and that is not `Retain`-ed
  (`R_FreeMedia_` at `r_media.c:160`). This is exactly "free anything not in use
  by the current scene."
- `src/client/renderer/r_media.c:185 — R_FreeMedia()` frees one item immediately.
- Sound has the analogous pool in `src/client/sound/s_media.c` (`S_FreeMedia`).

### Insertion point — new `case` in `Cl_HandleSystemEvent()` (`cl_input.c:293`)

```c
case SDL_EVENT_LOW_MEMORY:
    Com_Warn("Low memory: flushing caches\n");
    R_FreeUnseededMedia();   // thin public wrapper over R_EndLoading()'s sweep
    S_FreeUnseededMedia();   // sound equivalent (s_media.c)
    return true;
```

**Required engine support (renderer-team, separate change):** expose a public
"sweep stale media" entry point. The logic already exists privately as the
`(void *) 0` branch of `R_FreeMedia_`; `R_EndLoading()` (`r_media.c:208`) *is*
that sweep but is named for the loading path. Either:

- call `R_EndLoading()` directly (correct today, but semantically odd), or
- add a one-line alias `void R_FreeUnseededMedia(void) { g_hash_table_foreach_remove(r_media_state.media, R_FreeMedia_, (void *) 0); }` in `r_media.c` and declare it in `r_media.h:38` area.

The alias is preferred so the lifecycle call site reads correctly and doesn't
re-roll the loading seed.

> Do **not** call `R_ShutdownMedia()` (`r_media.c:249`, the `(void *) 1`
> "free everything" sweep) — that nukes retained/in-use media and would corrupt
> the live scene. Low-memory must only drop the *unseeded* (unused) set.

---

## 4. SDL_EVENT_TERMINATING → save config + clean shutdown

**Goal:** Android is about to kill the process — persist config and exit cleanly,
fast. We get **one** synchronous chance.

The pieces already exist:

- **Config writer:** `src/client/cl_main.c:496 — Cl_WriteConfiguration()` writes
  key bindings (`Cl_WriteBindings`) + archived cvars (`Cvar_WriteAll`) to
  `quetoo.cfg` via `Fs_OpenWrite`. It is also exposed as the `save_config`
  console command (`cl_main.c:563`).
- **Clean shutdown chain:** the `quit` command → `Quit_f()`
  (`src/main/main.c:221`) → `Com_Shutdown()`. `Com_Shutdown` tears down
  subsystems; the client side `Cl_Shutdown()` (`cl_main.c:755`) already calls
  `Cl_WriteConfiguration()` at `cl_main.c:775` before freeing.
- The pump **already** maps `SDL_EVENT_QUIT` → `Cmd_ExecuteString("quit")`
  (`cl_input.c:306`).

### Insertion point — new `case` in `Cl_HandleSystemEvent()` (`cl_input.c:293`)

```c
case SDL_EVENT_TERMINATING:
    Com_Print("Terminating: saving configuration\n");
    Cl_WriteConfiguration();          // cl_main.c:496 — persist NOW, before we may be frozen
    Cmd_ExecuteString("quit");        // reuse the existing clean-shutdown path (Quit_f)
    return true;
```

Rationale for calling `Cl_WriteConfiguration()` *explicitly first* rather than
relying solely on the `quit` path: on Android the OS may freeze/kill the process
during shutdown, and `Com_Shutdown` does a lot (renderer, sound, cgame unload)
before `Cl_Shutdown` reaches the config write at `cl_main.c:775`. Writing config
as the very first action guarantees the durable bit lands even if the rest of the
teardown is cut short. `Cl_WriteConfiguration()` is idempotent (the second write
during `Cl_Shutdown` just rewrites the same file), so the double-call is safe.

> `Cl_WriteConfiguration()` early-returns if `cls.state == CL_UNINITIALIZED`
> (`cl_main.c:499`), so it is also safe to call before the client is fully up.

---

## 5. Android back button → SDL_EVENT_KEY_DOWN with SDLK_AC_BACK

On Android, SDL3 delivers the hardware/gesture **Back** action as a normal key
event with keycode `SDLK_AC_BACK` (the "application control: back" keycode). It
is *not* a scancode the engine binds, so we intercept it as a system event and
map it to the existing menu/pause semantics — exactly how `SDLK_ESCAPE` is
already handled.

### Existing ESC handling to mirror
`src/client/cl_input.c:328` (`case SDL_EVENT_KEY_DOWN:` inside
`Cl_HandleSystemEvent`) switches on `event->key.key == SDLK_ESCAPE` and toggles
key destinations via `Cl_SetKeyDest()` (`src/client/cl_keys.c:31`):
disconnected+console → toggle console; active+UI/chat → `KEY_GAME`;
active+game → `KEY_UI` (i.e. open the menu / pause).

### Insertion point — extend the existing `SDL_EVENT_KEY_DOWN` case (`cl_input.c:328`)

Add a Back-button branch alongside the ESC branch. Simplest faithful behavior:
**treat Back exactly like ESC** so it closes a menu / opens the pause menu, and
when already at the root menu while disconnected, let it fall through to quit.

```c
case SDL_EVENT_KEY_DOWN:

    if (event->key.key == SDLK_AC_BACK) {
        switch (cls.key_state.dest) {
            case KEY_CONSOLE:
                Cl_ToggleConsole_f();
                return true;
            case KEY_GAME:                 // in-game -> open menu (pause)
                Cl_SetKeyDest(KEY_UI);     // cl_keys.c:31
                return true;
            case KEY_CHAT:
                Cl_SetKeyDest(cls.state == CL_ACTIVE ? KEY_GAME : KEY_UI);
                return true;
            case KEY_UI:
                if (cls.state == CL_ACTIVE) {
                    Cl_SetKeyDest(KEY_GAME); // close menu, back to game
                    return true;
                }
                // disconnected at root menu: let cgame/UI decide (back = "up"
                // a menu); if it's already the root, fall through to quit.
                if (cls.cgame && cls.cgame->HandleEvent(event)) {
                    return true;
                }
                Cmd_ExecuteString("quit");
                return true;
        }
    }

    if (event->key.key == SDLK_ESCAPE) {
        ... // existing block, unchanged, cl_input.c:330
    }
```

> Returning `true` consumes the event so it does **not** reach `Cl_KeyEvent()` /
> the cgame a second time (the pump only calls the downstream handlers when
> `Cl_HandleSystemEvent` returns `false`, see `cl_input.c:455`). The
> `cls.cgame->HandleEvent` call in the root-menu case lets the SDL3/Objectively
> UI pop a sub-view first; only when the UI has nothing to pop do we quit. If the
> virtual-controls/menu work (#856 item 6) prefers different root behavior, this
> is the single branch to adjust.

---

## 6. Writable storage via SDL_GetPrefPath() — maps onto existing write-dir logic

**This already works through the engine's existing path code.** No new
filesystem concept is introduced; we verify and harden the existing chain.

### The existing chain (cite)

1. `src/common/sys.c:111 — Sys_UserDir()`
   ```c
   char *pref = SDL_GetPrefPath("WickedOldGames", "Quetoo");  // sys.c:115
   ```
   Caches the result in a `static char user_dir[MAX_OS_PATH]`, strips the
   trailing separator, returns it. On Android SDL3 implements `SDL_GetPrefPath`
   as the app's **internal storage** directory
   (`Context.getFilesDir()`-derived, e.g.
   `/data/data/<package>/files/WickedOldGames/Quetoo/`) — app-private, no runtime
   permission needed, and wiped on uninstall. That is the correct home for
   config/screenshots/demos.

2. `src/common/filesystem.c:566 — Fs_AddUserSearchPath(dir)`
   ```c
   gchar *path = g_build_path(G_DIR_SEPARATOR_S, Sys_UserDir(), dir, NULL); // fs.c:568
   if (g_mkdir_with_parents(path, 0755)) { ... }
   Fs_AddToSearchPath(path);   // fs.c:575 — mount for reading
   Fs_SetWriteDir(path);       // fs.c:576 — PHYSFS write target
   ```

3. `src/common/filesystem.c:631 — Fs_SetWriteDir(dir)` → `PHYSFS_setWriteDir(dir)`
   (`fs.c:643`). This is the single point that decides where `Fs_OpenWrite` (and
   thus `quetoo.cfg`, screenshots, recorded demos) land.

4. `src/common/filesystem.c:692 — Fs_Init()` drives it: after setting
   `bin/lib/data` dirs it calls `Fs_AddUserSearchPath(DEFAULT_GAME)`
   (`fs.c:796`), establishing the write dir under `Sys_UserDir()`.

So `SDL_GetPrefPath` → `Sys_UserDir()` → `Fs_AddUserSearchPath` →
`Fs_SetWriteDir` → `PHYSFS_setWriteDir` is already the live path on **every**
platform. Android inherits it for free.

### What actually needs doing on Android (small)

**(a) Read-only base/data dirs — the executable-path block is desktop-only.**
`Fs_Init()` resolves `base_dir`/`data_dir` from `Sys_ExecutablePath()` inside
`#if defined(__APPLE__) / __linux__ / _WIN32` (`fs.c:720`–`787`). **Android hits
none of these branches**, so `lib_dir`/`data_dir` stay at the compile-time
`PKGLIBDIR` / `PKGDATADIR` (`fs.c:711-712`) — paths that don't exist on a device.
That's fine for the *write* dir (it comes from `Sys_UserDir`, unaffected), but the
**read** path for bundled assets needs an Android source. Plan:

- Assets (`default.pk3`) ship in the APK and/or are downloaded on first run
  (PORTING.md §5, work item 7). Mount that location by adding an
  `#elif defined(__ANDROID__)` arm to the `Fs_Init` executable-path `switch`
  (or just after it) that points `data_dir` at the extracted/asset dir — e.g.
  `SDL_GetAndroidInternalStoragePath()` for downloaded data, and/or mount via
  SDL3's `SDL_IOStream` Android-asset path for in-APK `.pk3`s.
- This is an **asset-packaging** concern (item 7), not lifecycle; LIFECYCLE.md
  only flags it because `Fs_Init` is the touch point. The write-dir half — the
  part this doc owns — needs no change.

**(b) `SDL_GetPrefPath` is now called from main-thread init — verify timing.**
On Android `SDL_GetPrefPath` requires the SDL Android runtime (JNI/Activity) to
be initialized. `Fs_Init` runs from `Com_Init` (`main.c:457`), which on Android
runs inside SDL's `main` after `SDL_main` bootstraps — so the JNI context exists.
No code change expected; add a one-line assert/log if `Sys_UserDir` ever returns
empty on device.

**(c) No `getenv("HOME")`/XDG assumptions to remove.** Confirmed: the only
`getenv` in the client/common path is `Sys_UserDir`'s comment about Linux XDG,
which is *inside* SDL, not engine code. The engine never reads `HOME` for the
write dir. (The lone `getenv` hit is `src/master/main.c:565`, the master server,
not shipped on Android.)

### Net filesystem change for this work item
- Write dir: **0 lines** — already `SDL_GetPrefPath`-backed.
- Read dir for bundled assets: one `#elif defined(__ANDROID__)` arm in
  `Fs_Init` (`fs.c:~787`), owned by the asset-packaging item, stubbed here.

---

## 7. Summary of insertion points

| Lifecycle concern | SDL3 event / API | Insertion point (file:line — function) | New engine support needed |
|---|---|---|---|
| Event pump (host) | `SDL_PollEvent` | `src/client/cl_input.c:440 — Cl_HandleEvents` → `cl_input.c:293 — Cl_HandleSystemEvent` | none — extend existing `switch` |
| Pause/resume | `SDL_EVENT_WILL_ENTER_BACKGROUND` / `DID_ENTER_FOREGROUND` | watcher reg in `cl_main.c:714 — Cl_Init`; frame gate at `cl_main.c:632 — Cl_Frame` (after the `:635` dedicated guard) | `bool cls.backgrounded`; audio device pause/resume |
| Flush caches | `SDL_EVENT_LOW_MEMORY` | new `case` in `cl_input.c:293 — Cl_HandleSystemEvent` | public `R_FreeUnseededMedia()` wrapper over `r_media.c:208 — R_EndLoading` sweep; `S_FreeUnseededMedia()` |
| Save + shutdown | `SDL_EVENT_TERMINATING` | new `case` in `cl_input.c:293` | none — reuses `cl_main.c:496 — Cl_WriteConfiguration` + `quit`/`main.c:221 — Quit_f` |
| Back button | `SDL_EVENT_KEY_DOWN` + `SDLK_AC_BACK` | extend the `SDL_EVENT_KEY_DOWN` case at `cl_input.c:328` (mirror ESC at `:330`) | none — reuses `cl_keys.c:31 — Cl_SetKeyDest` |
| Writable storage | `SDL_GetPrefPath` | already wired: `sys.c:111 — Sys_UserDir` → `filesystem.c:566 — Fs_AddUserSearchPath` → `filesystem.c:631 — Fs_SetWriteDir` (from `filesystem.c:692 — Fs_Init`) | none for write dir; `__ANDROID__` arm in `Fs_Init` for *read* assets (owned by item 7) |

**All client-side lifecycle handling lands in two files** —
`src/client/cl_input.c` (the event cases + watcher) and a one-line guard in
`src/client/cl_main.c` — plus a small public-API addition in the renderer
(`r_media.c`/`r_media.h`) and sound (`s_media.c`) for the low-memory sweep. The
filesystem requires **no change for the writable path**; only the read-side asset
mount (a separate work item) touches `filesystem.c`.
