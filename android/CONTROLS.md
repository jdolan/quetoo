# Quetoo Android — Virtual Touch Controls (SDL3)

Design + scaffold for on-screen touch controls for the Android port
([jdolan/quetoo#856](https://github.com/jdolan/quetoo/issues/856), work item
**#6**), shared with the iOS port ([#855](https://github.com/jdolan/quetoo/issues/855)).

This is a **design doc + compiling skeleton**. The skeleton lives in
`src/client/cl_touch.{c,h}` and is **not yet wired** into the event pump or the
usercmd path, so desktop builds are unaffected. Every integration point below is
cited as `file:function` against the current worktree so the implementation PR is
mechanical. The driving principle:

> **Touch input must produce the *same* `pm_cmd_t` (usercmd) the desktop
> mouse/keyboard path already produces.** No protocol change, no prediction
> change, no game-module change. We are a second input frontend onto the existing
> command assembly, nothing more.

---

## TL;DR

| Virtual control | Feeds | Existing field / function it mirrors |
|---|---|---|
| Left **move stick** | `cmd->forward`, `cmd->right` | `Cl_Move()` (`cl_input.c`) — `cl_forward_speed`/`cl_right_speed` scaling |
| Right **look drag** | `cl.angles.y` (yaw), `cl.angles.x` (pitch) | `Cl_Look()` + `Cl_MouseMotionEvent()` (`cl_input.c`/`cl_mouse.c`), `m_yaw`/`m_pitch`/`m_sensitivity` |
| **Fire** button | `cmd->buttons |= BUTTON_ATTACK` | `Cg_Move()` (`cg_input.c`), `in_attack` |
| **Jump** button | `cmd->up` | `Cl_Look()` up axis, `cl_up_speed`, `+move_up` |
| **Menu/Back** button | `Cl_SetKeyDest(KEY_UI)` | the Esc / `SDLK_AC_BACK` branch in `Cl_HandleSystemEvent()` (`cl_input.c`) / `LIFECYCLE.md` §5 |

All of it is gated behind one cvar, **`cg_touch_controls`** (default `0` on
desktop, `1` on a touch device), and behind key-dest `KEY_GAME`. When inactive
the whole unit early-returns — desktop sees nothing.

---

## 1. How input becomes movement today (the path we tap into)

### The event pump
`src/client/cl_input.c:Cl_HandleEvents()` polls with `SDL_PollEvent` and routes
each event: system events first via `Cl_HandleSystemEvent()`, then
`Ui_HandleEvent()`, `Cl_HandleEvent()`, and finally `cls.cgame->HandleEvent()`
(see `LIFECYCLE.md` §1, which already identified this as *the* integration point
for new SDL event types). `Cl_HandleEvent()` is a `switch (event->type)` that
dispatches `SDL_EVENT_MOUSE_MOTION` → `Cl_MouseMotionEvent()`, key events →
`Cl_KeyEvent()`, etc. **The finger events join this switch.**

### The usercmd assembly
Each client frame the command is built by two functions in `cl_input.c`, both of
which then call into the cgame:

- **`Cl_Move(pm_cmd_t *cmd)`** accumulates translational movement:
  ```c
  cmd->forward += cl_forward_speed->value * cmd->msec * Cl_KeyState(&in_forward, cmd->msec);
  cmd->forward -= cl_forward_speed->value * cmd->msec * Cl_KeyState(&in_back, cmd->msec);
  cmd->right   += cl_right_speed->value   * cmd->msec * Cl_KeyState(&in_move_right, cmd->msec);
  cmd->right   -= cl_right_speed->value   * cmd->msec * Cl_KeyState(&in_move_left, cmd->msec);
  cls.cgame->Move(cmd);   // -> Cg_Move()
  ```
- **`Cl_Look(pm_cmd_t *cmd)`** accumulates the up axis and view angles, then
  clamps pitch and writes `cmd->angles = cl.angles`:
  ```c
  cmd->up     += cl_up_speed->value  * cmd->msec * Cl_KeyState(&in_up, cmd->msec);
  cmd->up     -= cl_up_speed->value  * cmd->msec * Cl_KeyState(&in_down, cmd->msec);
  cl.angles.y -= cl_yaw_speed->value * cmd->msec * Cl_KeyState(&in_right, cmd->msec);  // key turn
  ...
  cls.cgame->Look(cmd);   // -> Cg_Look() (view kick)
  Cl_ClampPitch(&cl.frame.ps);
  cmd->angles = cl.angles;
  ```

`pm_cmd_t` (`src/shared/shared.h`) is the wire command:
```c
typedef struct {
  uint8_t msec;               // command duration
  vec3_t  angles;             // final view angles (x=pitch, y=yaw, z=roll)
  int16_t forward, right, up; // directional intentions
  uint8_t buttons;            // bit mask of buttons down
  vec3_t  muzzle;             // sent with +attack
} pm_cmd_t;
```

The view angles live in **`cl.angles`** (`cl_client_t.angles`,
`src/client/cl_types.h`), a `vec3_t` where `.x` is pitch and `.y` is yaw. The
mouse path mutates exactly this: `Cl_MouseMotionEvent()` (`src/client/cl_mouse.c`)
applies `m_yaw`/`m_pitch` × `m_sensitivity` (honoring `m_invert`) to `cl.angles`.
**Touch look does the same thing** so prediction and `Cl_ClampPitch()` (already
called by `Cl_Look()`) handle it for free.

### Button bits and the up axis (important subtleties)
The button mask values are defined in **`src/game/default/bg_pmove.h`**:
```c
#define BUTTON_ATTACK (1 << 0)
#define BUTTON_WALK   (1 << 1)
#define BUTTON_HOOK   (1 << 2)
#define BUTTON_SCORE  (1 << 3)
```
Two consequences for this feature:

1. **Fire is a cgame concern, not a client one.** `cmd->buttons |= BUTTON_ATTACK`
   is set in **`Cg_Move()`** (`src/cgame/default/cg_input.c`), gated on the
   `in_attack` button. The client side (`cl_input.c`) never touches `BUTTON_*`.
   So the virtual **fire** button is most cleanly fed by driving the existing
   `in_attack` button in the cgame (see §4), *or* by OR-ing `BUTTON_ATTACK` into
   `cmd->buttons` from `Cl_ApplyTouchToMove()` after `cls.cgame->Move()` runs.
2. **There is no `BUTTON_JUMP`.** Jumping is the **up movement axis**: the
   keyboard binds `+move_up` → `in_up`, which `Cl_Look()` turns into `cmd->up`
   via `cl_up_speed`. The virtual **jump** button therefore adds to `cmd->up`
   (positive), exactly like holding `+move_up` — *not* a button bit.

---

## 2. SDL3 touch events

SDL3 delivers finger input as three event types carrying an `SDL_TouchFingerEvent`
(`event->tfinger`):

| Event | Meaning | Key fields |
|---|---|---|
| `SDL_EVENT_FINGER_DOWN` | a new contact | `fingerID`, `x`, `y` (normalized `0..1`), `pressure` |
| `SDL_EVENT_FINGER_MOTION` | a contact moved | `fingerID`, `x`, `y`, `dx`, `dy` (normalized deltas) |
| `SDL_EVENT_FINGER_UP` | a contact lifted | `fingerID`, `x`, `y` |

- **Coordinates are normalized `[0..1]`** relative to the window, origin
  top-left. We keep the whole layout normalized and only convert to pixels at
  draw time using `r_context.w` / `r_context.h` (`GLint w, h` in `r_context_t`,
  `src/client/renderer/r_types.h`). This makes the controls resolution- and
  orientation-independent.
- **`fingerID` is stable** from a contact's DOWN through its UP, which is what
  lets a finger "own" a widget (a sticky thumbstick) even as it drags. SDL3's
  event/finger types arrive transitively via `renderer/r_types.h`
  (`#include <SDL3/SDL_video.h>` → `SDL_events.h`), already on the client include
  path, so no new include is needed.
- Touch devices can be enumerated with SDL3's `SDL_GetTouchDevices()` for the
  default-on/off decision (§6, Gating).

> Note: SDL may *also* synthesize mouse events from touch (and touch from mouse).
> Because our look handler and the existing `Cl_MouseMotionEvent` both write
> `cl.angles`, we must avoid double-applying. The overlay is only active in
> `KEY_GAME`, and on a real device there is no mouse; if synthetic mouse events
> prove troublesome, disable them with `SDL_SetHint(SDL_HINT_TOUCH_MOUSE_EVENTS,
> "0")` / `SDL_HINT_MOUSE_TOUCH_EVENTS` at init. Flagged for the implementer.

---

## 3. The overlay (where it is drawn)

The HUD is drawn by the **cgame**, not the engine. The cgame's
`UpdateScreen(const cl_frame_t *)` export (`cg_export_t`, `src/cgame/cgame.h`) is
"called each frame to draw any non-view visual elements, such as the HUD"; its
default implementation lives alongside `src/cgame/default/cg_hud.c`. The cgame
draws 2D through the import table **`cg_import_t`** (`src/cgame/cgame.h`):

```c
void   (*Draw2DFill)(GLint x, GLint y, GLint w, GLint h, const color_t color);
void   (*Draw2DImage)(GLint x, GLint y, GLint w, GLint h, const r_image_t *image, const color_t color);
size_t (*Draw2DString)(GLint x, GLint y, const char *s, const color_t color);
void   (*BindFont)(const char *name, GLint *cw, GLint *ch);
r_image_t *(*LoadImage)(const char *name, r_image_type_t type);
```
`cgi.context->w` / `cgi.context->h` give the screen size (see `Cg_DrawVital()` in
`cg_hud.c`, which already positions HUD elements off `cgi.context->h`). Colors come
from `src/shared/color.h` (`color_white`, `Color4bv(0x80ffffff)` for a translucent
fill, etc.).

**Recommended shipping location for the overlay draw:** a new
`Cg_DrawTouchControls()` called from the cgame's HUD update (next to the other
`Cg_Draw*` calls reached from `UpdateScreen`), using `cgi.Draw2DImage` /
`cgi.Draw2DFill`. Rationale: it is 2D HUD content, the cgame already owns the
ortho 2D pass and the screen metrics, and the cgame already owns the fire button's
`in_attack` state. The engine-side `Cl_DrawTouchControls()` in the scaffold exists
only so the unit is self-contained/testable; the production overlay should be
cgame-drawn.

> The **input** half (finger → state) stays engine-side in `cl_touch.c` because
> the event pump and `cl.angles`/`pm_cmd_t` live in the engine. Only the **draw**
> half is most natural in the cgame. The two halves share the normalized layout
> constants; if drawn in the cgame, mirror the `TOUCH_*_CENTER_*`/`_RADIUS`
> constants there (or expose the regions via a small accessor). Keeping input
> engine-side also means look/move work even before art is loaded.

---

## 4. Mapping each control to the usercmd path

### Left move stick → `cmd->forward` / `cmd->right`
- On `FINGER_DOWN` inside the stick circle, the finger claims
  `TOUCH_WIDGET_MOVE_STICK`; its start point becomes the stick center reference.
- On `FINGER_MOTION`, displacement `(current - start)` is clamped to the stick
  radius and normalized to `move_axis ∈ [-1..1]²` (`.x` = strafe, `.y` = forward;
  up on screen = forward, so negate screen-Y).
- Each frame, `Cl_ApplyTouchToMove(cmd)` (called at the **end of `Cl_Move()`**)
  adds, mirroring the keyboard scaling so feel/speed match:
  ```c
  cmd->forward += (int16_t) (cl_forward_speed->value * cmd->msec * move_axis.y);
  cmd->right   += (int16_t) (cl_right_speed->value   * cmd->msec * move_axis.x);
  ```
  `cl_forward_speed` / `cl_right_speed` are `static` in `cl_input.c`; either widen
  their linkage or re-resolve them with `Cvar_Get("cl_forward_speed")` inside
  `cl_touch.c`. (The scaffold notes this in `Cl_ApplyTouchToMove`'s TODO.)

### Right look drag → `cl.angles`
- Any `FINGER_DOWN` on the right half that did not hit a button starts a look
  drag (`TOUCH_WIDGET_LOOK`). It has no fixed rect — it is relative-drag, like the
  mouse.
- `FINGER_MOTION` accumulates `look_delta += (dx, dy)` (normalized).
- `Cl_ApplyTouchToLook(cmd)` (called at the **end of `Cl_Look()`**, before
  `cmd->angles = cl.angles` would re-read it — see hookup note) converts to
  degrees with the existing sensitivity cvars and drains the accumulator:
  ```c
  cl.angles.y -= look_delta.x * m_yaw->value   * m_sensitivity->value * touch_scale;
  cl.angles.x += look_delta.y * m_pitch->value * m_sensitivity->value * touch_scale * (m_invert->integer ? -1 : 1);
  look_delta = Vec2_Zero();
  ```
  `Cl_Look()` already calls `Cl_ClampPitch()` immediately after `cls.cgame->Look()`,
  so pitch clamping is automatic. A dedicated `cg_touch_look_sensitivity` cvar is
  advisable since thumb-drag gain differs from mouse DPI; `touch_scale` is its
  hook. `m_yaw`/`m_pitch`/`m_sensitivity`/`m_invert` are already declared `extern`
  in `cl_local.h`.

### Fire → `BUTTON_ATTACK`
Two equivalent options; pick one:
1. **Drive the cgame button (preferred):** on fire press/release, toggle the
   cgame's `in_attack` (the same `button_t` `Cg_Move()` reads). This reuses the
   weapon-select-on-tap logic in `Cg_Move()` for free. Requires the fire widget
   to live cgame-side or a tiny cgame entry the touch code calls.
2. **OR the bit in the engine:** in `Cl_ApplyTouchToMove()` (which runs *after*
   `cls.cgame->Move(cmd)` inside `Cl_Move()`), do
   `if (pressed[TOUCH_WIDGET_FIRE]) cmd->buttons |= BUTTON_ATTACK;`. Simplest, but
   bypasses the tap-to-select-weapon nicety. `BUTTON_ATTACK` is from
   `game/default/bg_pmove.h`.

### Jump → `cmd->up`
On the jump widget being held, add to the up axis exactly like `+move_up`:
```c
if (pressed[TOUCH_WIDGET_JUMP]) cmd->up += (int16_t) (cl_up_speed->value * cmd->msec);
```
(Placed in `Cl_ApplyTouchToMove()` or `Cl_ApplyTouchToLook()`; `cmd->up` is
written by `Cl_Look()` today, so co-locating with look is fine.)

### Menu / Back → `KEY_UI`
On the menu widget tap (edge-triggered via `menu_pressed_edge`), call
`cgi`/engine `Cl_SetKeyDest(KEY_UI)` — identical to the Esc / `SDLK_AC_BACK`
handling described in `LIFECYCLE.md` §5 and implemented in
`Cl_HandleSystemEvent()` (`cl_input.c`). This opens the in-game menu (pause). The
hardware Back button is already handled by the lifecycle work; this widget is the
on-screen equivalent for devices without a Back gesture.

### Multi-touch finger tracking
`cl_touch_state_t.fingers[TOUCH_MAX_FINGERS]` maps each live `SDL_FingerID` to the
widget it claimed at DOWN. A finger keeps its widget until UP (sticky), so
move + look + fire + jump can be pressed simultaneously without cross-talk. Hit
priority on DOWN: discrete buttons (fire/jump/menu) and the move stick first, then
the LOOK half-screen fallback, so a button placed on the right half is not
swallowed by look. (`Cl_TouchHitTest()` TODO documents this ordering.)

---

## 5. Integration points (the deferred one-line hookups)

The scaffold is intentionally **not** wired in, to keep the build green for the
other agents currently editing `src/client/renderer/` and
`src/common/filesystem.c`. When ready, the wiring is:

| Step | File : function | One-line change |
|---|---|---|
| Register cvar + layout | `cl_input.c : Cl_InitInput()` | `Cl_TouchInit();` (after the input cvars) |
| Reset on clear | `cl_input.c : Cl_ClearInput()` | `Cl_ClearTouch();` |
| Receive finger events | `cl_input.c : Cl_HandleEvent()` (the `switch`) | add `case SDL_EVENT_FINGER_DOWN: case SDL_EVENT_FINGER_UP: case SDL_EVENT_FINGER_MOTION: Cl_TouchEvent(&event->tfinger); break;` |
| Apply movement | `cl_input.c : Cl_Move()` (end) | `Cl_ApplyTouchToMove(cmd);` |
| Apply look | `cl_input.c : Cl_Look()` (after `cls.cgame->Look(cmd)`, before `cmd->angles = cl.angles`) | `Cl_ApplyTouchToLook(cmd);` |
| Draw overlay | cgame HUD update reached from `UpdateScreen` (e.g. near `cg_hud.c`) | `Cg_DrawTouchControls();` (cgame-side; mirrors `Cl_DrawTouchControls`) |
| Build | `src/client/Makefile.am` / `android/CMakeLists.txt` | add `cl_touch.c` to the client sources |

`cl_touch.{c,h}` already `#include "cl_local.h"` (same pattern as `cl_input.c`);
its public prototypes are guarded by `#if defined(__CL_LOCAL_H__)` so they are
only visible to client translation units, matching `cl_input.h`. To call them from
`cl_input.c`, add `#include "cl_touch.h"` there (or fold the prototypes into
`cl_input.h`).

> **Look hookup ordering matters.** `Cl_Look()` ends with
> `cmd->angles = cl.angles;`. `Cl_ApplyTouchToLook()` must mutate `cl.angles`
> *before* that assignment (and after `Cl_ClampPitch()` is fine since we re-clamp
> implicitly on the next frame; simplest is to insert the touch-look call right
> after `cls.cgame->Look(cmd)` and let the existing `Cl_ClampPitch()` +
> `cmd->angles = cl.angles` lines run afterward). The scaffold puts pitch clamping
> on the engine via the existing call, so the touch code does not re-implement it.

---

## 6. Gating (desktop is unaffected)

`Cl_TouchControlsActive()` is the single predicate every entry point checks first;
it returns true only when **all** hold:

- `cg_touch_controls` is enabled (cvar, `CVAR_ARCHIVE`). Default chosen at init:
  `"1"` if `SDL_GetTouchDevices()` reports a device, else `"0"` — so desktop is
  off unless the user opts in, and a desktop with a touchscreen *can* opt in.
- `cls.state == CL_ACTIVE` (in a live game) and `cls.key_state.dest == KEY_GAME`
  (not in console/menu/chat — the UI owns touches there).

Because the scaffold's `Cl_TouchControlsActive()` currently `return false;`, every
function is a no-op until implemented, and the unit links with zero runtime effect.
On desktop, with the cvar defaulting to `0`, the overlay never draws and no finger
handling runs even after full implementation.

---

## 7. Files

| File | Role |
|---|---|
| `src/client/cl_touch.h` | touch-state structs (`cl_touch_state_t`, `cl_touch_finger_t`, `cl_touch_region_t`, `cl_touch_widget_t`), `cg_touch_controls` extern, function prototypes (guarded by `__CL_LOCAL_H__`) |
| `src/client/cl_touch.c` | `Cl_TouchInit` / `Cl_ClearTouch` / `Cl_TouchEvent` / `Cl_ApplyTouchToMove` / `Cl_ApplyTouchToLook` / `Cl_DrawTouchControls` / `Cl_TouchControlsActive`, all with `// TODO(#856)` bodies referencing this doc |
| `android/CONTROLS.md` | this document |

**Validation:** `cl_touch.c` syntax-checks clean against the real client headers
on the build container (`pve2`, container 100, `/root/quetoo-build`):
`gcc -fsyntax-only -I. -Isrc -Isrc/client -Isrc/shared -Isrc/common
$(pkg-config --cflags sdl3 glib-2.0 Objectively) src/client/cl_touch.c` →
**`SYNTAX_OK`** (no warnings from `cl_touch.c` itself, including under
`-Wall -Wextra`; only pre-existing `#pragma mark` notices from `src/shared/vector.h`).
