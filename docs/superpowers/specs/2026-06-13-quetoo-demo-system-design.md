# Quetoo Advanced Demo System — Design Spec

- **Issue:** jdolan/quetoo#377 "Advanced demo tools"
- **Date:** 2026-06-13
- **Status:** Approved (architecture), implementation pending
- **Branch:** `feature/demo-system` (worktree off `origin/main` @ `fab1d8a68`)
- **Author:** James

## 1. Scope

**In scope (this effort):**
- **A —** Free detached spectator camera + follow/cycle any player's POV during playback.
- **v2 format + engine —** A versioned, timecoded, keyframed demo container and a seekable playback path.
- **B —** Timeline control: seek (absolute/relative), pause, variable speed, frame step, reverse.
- **C —** Director tools: keyframed cinematic camera paths + an ObjectivelyMVC timeline/scrubber UI.
- **D —** `serverrecord`: server-side, omniscient, multi-POV demo recording.

**Out of scope (deferred):**
- **E — OBS plugin / native video bridge.** Deferred. However, the engine stays *OBS-friendly*: a real-time camera clock decoupled from playback speed, clean window capture, and a console-drivable control surface, so E is purely additive later.

**Non-goals:**
- No re-encoding or transcoding of existing v1 demos. They remain playable read-only, forward-only.
- No netcode/protocol bump for *live* play. The only wire change is a client-side capability to receive a multi-POV player-state array, gated by a new server→client capability flag, and only emitted during demo playback.

## 2. Background — current state

Anchored to the worktree base (`feature/demo-system`).

- **v1 format:** header written once (`Cl_WriteDemoHeader`, `src/client/cl_demo.c:28`) = `SV_CMD_SERVER_DATA` (with `demo_server=1`) + all config strings + all entity baselines + `precache 0`. Then each server message is appended as `[int32 LE length][bytes]` (`Cl_WriteDemoMessage`, `src/client/cl_demo.c:95`), terminated by `[int32 -1]`.
- **Playback is server-driven:** a local server in `SV_ACTIVE_DEMO` reads one message per tick (`Sv_GetDemoMessage`, `src/server/sv_send.c:344`) and `Netchan_Transmit`s it to the client (`Sv_SendClientPackets`, `src/server/sv_send.c:408`). The client is a normal connected `CL_ACTIVE` client receiving a normal-looking stream.
- **Why no seek today (the FIXME):** `src/server/sv_send.c:340` — *"Multiple messages can constitute a frame. We need a mechanism to indicate frame completion, or we need a timecode in our demos."* The format has no timecodes and no frame boundaries, so random access is impossible.
- **Why POV is locked:** one line — `if (cl.demo_server) { frame->ps.pm_state.type = PM_FREEZE; }` (`src/client/cl_entity.c:37`). Prediction is also disabled in demos (`Cg_UsePrediction`, `src/cgame/default/cg_predict.c:34`).
- **Tick model:** `QUETOO_TICK_RATE 40` → 25 ms/tick (`src/quetoo.h:159`). Frame ring `PACKET_BACKUP = 128` (~3.2 s).
- **Spectator physics already exist:** `Pm_SpectatorMove` (`src/game/default/bg_pmove.c:1239`), invoked for `PM_SPECTATOR`. Third-person/chase math: `Cg_UpdateThirdPerson` (`src/cgame/default/cg_view.c:76`).
- **View pipeline:** `Cg_PrepareView` (`src/cgame/default/cg_view.c:319`) sets `cgi.view->origin/angles` once per frame from the (interpolated or predicted) player state.

## 3. Keystone — server-hosted, client-controlled v2 playback

v2 demos are hosted by the same local-server replay model as v1 (so the client remains a normal connected client and **the full parse/decode/interpolate pipeline is reused unchanged**), with three additions:

1. The **server demo reader is v2-aware**: it parses v2 records, tracks a **demo clock** it advances by real frame time × speed (not locked to sv tick), and reframes records into standard `SV_CMD_*` messages for the client.
2. The **client owns the user-facing controls** (`demo_*` commands and the scrubber UI), which drive the server demo reader over the existing reliable channel (loopback, zero latency).
3. **Seeking is a server file operation**: jump to the nearest keyframe ≤ target, re-emit it as a full (non-delta) frame, fast-replay deltas to the target, resume. Backward and forward seek are the same operation.

**Why not fully client-side:** the client parse path is tightly coupled to connection state (netchan, `cls.state`, reliable channel). Decoupling it is high-risk for no feature gain — every timeline feature, multi-POV, the free camera, and the scrubber are all achievable on the server-hosted model. The client still *owns the timeline* (it issues the control commands and runs the camera on its own real-time clock); the server merely owns file I/O, reframing, and seeking.

**Legacy:** format detection is a single magic check. No magic → v1 → existing server-driven, forward-only path, untouched.

## 4. The v2 demo container format

New shared module: **`src/common/demo.{h,c}`** — read/write primitives used by *both* the client recorder and the server recorder, so both produce byte-identical containers.

### 4.1 File layout

```
┌ File header ───────────────────────────────────────────────┐
│ magic            : "QDM2" (4 bytes)                         │
│ format_version   : uint16  (=2)                             │
│ flags            : uint16  (CLIENT_DEMO|SERVER_DEMO|HAS_IDX)│
│ protocol_major   : uint16  (PROTOCOL_MAJOR at record time) │
│ protocol_minor   : uint16                                  │
│ tick_rate        : uint16  (QUETOO_TICK_RATE)              │
│ epoch_len        : uint32  (length of epoch block)         │
│ epoch block      : SV_CMD_SERVER_DATA + config strings     │
│                    + entity baselines (the v1 header)      │
└────────────────────────────────────────────────────────────┘
┌ Record stream (repeating) ─────────────────────────────────┐
│ type     : uint8   (FRAME_DELTA|FRAME_KEY|RELIABLE|CAMERA) │
│ timecode : uint32  (milliseconds from demo start)         │
│ length   : uint32  (payload length)                       │
│ payload  : length bytes                                   │
└────────────────────────────────────────────────────────────┘
┌ Footer (optional, present iff HAS_IDX) ────────────────────┐
│ index entries  : N × { timecode:uint32, offset:uint64 }   │  (one per FRAME_KEY)
│ index_count    : uint32                                   │
│ duration_ms    : uint32                                   │
│ index_offset   : uint64  (byte offset of index start)     │
│ tail_magic     : "QDMX" (4 bytes)                         │
└────────────────────────────────────────────────────────────┘
```

### 4.2 Record types

- **`FRAME_DELTA`** — one complete server frame, entities + player-state(s) delta-encoded against the previous recorded frame, using the existing `Net_WriteDeltaEntity` / `Net_WriteDeltaPlayerState`. **Exactly one record per frame** — this is what solves the multi-message FIXME: the recorder accumulates a full frame, stamps one timecode, writes one record.
- **`FRAME_KEY`** — a *self-contained* full snapshot: all changed config strings since epoch, full entity baseline of every live entity (delta vs null), and full player-state(s). Sufficient to start playback from this point with no prior records. Emitted every **`DEMO_KEYFRAME_INTERVAL` = 80 ticks (~2 s)** and forced at recording start, map change, and intermission.
- **`RELIABLE`** — non-frame server commands that must survive seeking (config-string changes, layout/scoreboard, centerprints relevant to state). Replayed in order from epoch up to the seek target. (Most are also folded into the next `FRAME_KEY`; `RELIABLE` covers the gap between keyframes.)
- **`CAMERA`** — *reserved* record id for future embedded camera scripts. Payload format deferred (sub-project C uses sidecar `.cam` files first). Reserving the id keeps the format forward-compatible.

### 4.3 Multi-POV encoding (server demos)

- A `FRAME_*` record carries a **count of player-states** followed by that many `{ client_slot:uint8, player_state }` entries.
- **Client demos** write exactly one player-state (the recorder's POV); `count == 1`.
- **Server demos** (`SERVER_DEMO` flag) write one entry per connected, in-game client that frame, and include **all entities with no PVS culling** (omniscient).
- On playback the server streams the recorded frame to the client; the client stores the player-state array (§7.2) and the director picks which one feeds the view (free-fly ignores them).

### 4.4 Recording strategy — re-serialize, don't capture wire

Both recorders **re-encode frames from decoded/authoritative state** rather than capturing raw wire packets:
- **Client recorder:** each frame, delta-encode from `cl.entities` / `cl.frame.ps` into a `FRAME_*` record.
- **Server recorder:** delta-encode from the omniscient observer snapshot (§9).

This yields clean frame boundaries, exact timecodes, and on-demand keyframes by construction, and is the same code path for both recorders.

### 4.5 Seek index

Written at `stoprecord`. If absent (truncated/crashed file), the playback engine **scans the stream once on load** to rebuild the keyframe index in memory. The index is an optimization, never a correctness requirement.

### 4.6 Versioning & legacy

- Files lacking the `QDM2` magic are v1 → legacy path.
- `protocol_major`/`protocol_minor` are stored for diagnostics. A v2 demo recorded under a different protocol than the running engine is played best-effort and warns; if entity/player deltas are incompatible the engine aborts playback with a clear message (matches existing protocol-mismatch behavior).

## 5. Sub-project A — Free camera + follow/cycle POV

Path-agnostic: lives in the cgame view layer, which renders `cl.frame` regardless of how it was populated, so it works on **both** v1 and v2 playback. Built first for immediate value.

### 5.1 Components (`src/cgame/default/cg_demo.c`, new)

```c
typedef enum { DEMO_CAM_LOCKED, DEMO_CAM_FREE, DEMO_CAM_FOLLOW } cg_demo_cam_mode_t;

static struct {
    cg_demo_cam_mode_t mode;
    pm_state_t  s;          // free-fly spectator state (origin, velocity, view_offset)
    vec3_t      angles;     // free-look angles (from Cl_Look accumulation)
    int32_t     follow_slot;// player slot being followed in FOLLOW mode
} cg_demo_cam;

bool Cg_DemoActive(void);              // cgi.client->demo_server
bool Cg_DemoFreecamActive(void);       // mode == DEMO_CAM_FREE
void Cg_DemoFreecam_f(void);           // toggle FREE; seed from current view
void Cg_DemoFollowNext_f(void);        // cycle POV among player slots present
void Cg_DemoFollowPrev_f(void);
void Cg_PredictDemoCamera(const GPtrArray *cmds); // PM_SPECTATOR integrate
void Cg_UpdateDemoCamera(void);        // write cgi.view->origin/angles
```

### 5.2 Integration points (from the verified sketch)
- **`cg_predict.c::Cg_UsePrediction`** — replace `if (demo_server) return false;` with a freecam exception.
- **`cg_predict.c::Cg_PredictMovement`** — when freecam active, branch to `Cg_PredictDemoCamera` (seed `pm.s` from persistent camera, `type = PM_SPECTATOR`, run cmds, persist).
- **`cl_predict.c::Cl_PredictMovement`** — allow feeding cmds to the cgame during a demo when freecam is active (it currently early-returns).
- **`cg_view.c::Cg_PrepareView`** — when freecam active call `Cg_UpdateDemoCamera()` and skip origin/angles/third-person/bob; FOLLOW mode reuses `Cg_UpdateThirdPerson` chase math against `follow_slot`'s entity.
- **`cl_entity.c:37`** — leave the `PM_FREEZE` override (recorded player stays frozen; we override at the view layer).

### 5.3 Commands & binds
`demo_freecam` (toggle), `demo_follow_next` / `demo_follow_prev`, registered `CMD_CGAME`. Default binds added to the demo context. First-person weapon model hidden while free/follow.

### 5.4 Tests
- Manual: play a v1 demo, toggle freecam, fly with WASD+mouse+`+speed`, cycle POV, snap back. Slow-mo/fast-forward while flying (camera stays real-time).
- Regression: live play and normal (locked) demo playback render identically to before (the non-freecam branch is unchanged).

## 6. Engine — v2 recording + server-hosted playback + control API

### 6.1 v2 recording
- **Client:** extend `Cl_Record_f` to write the `QDM2` header and per-frame `FRAME_*`/`RELIABLE` records via `common/demo.c`; emit keyframes on the interval. `Cl_Stop_f` writes the index footer.
- A cvar `cl_demo_legacy_format` (default `0`) can force v1 output for compatibility testing.

### 6.2 Server-hosted v2 playback
- `Sv_GetDemoMessage` becomes v2-aware: detect magic; v1 → existing path; v2 → parse next record(s) due at the current demo clock, reframe into `SV_CMD_*`, transmit.
- **Demo clock:** advanced by real elapsed time × `demo_speed`, owned by the server demo state but *commanded* by the client. Decoupled from sv tick so playback speed and the (client-side, real-time) camera are independent.

### 6.3 Control API (client → server, reliable loopback)
New client commands forwarded to the server demo reader: `demo_pause`, `demo_speed <x>`, `demo_seek <ms|±ms|mm:ss>`, `demo_step <±frames>`, `demo_jump_keyframe <±n>`. The scrubber UI (C) issues the same commands.

## 7. Sub-project B — Timeline control (seek / pause / speed / step / reverse)

### 7.1 Seek algorithm (server-side)
```
seek(target_ms):
    kf = index.floor(target_ms)             # nearest FRAME_KEY ≤ target
    file.seek(kf.offset)
    emit kf as a full (non-delta) frame      # client snaps, no map reload if same map
    replay RELIABLE records (epoch..target)  # restore config-string/layout state
    while next FRAME_DELTA.timecode ≤ target: apply+emit (fast, no render)
    demo_clock = target; resume
```
- **Reverse playback** = seek to `clock − step` each frame; visually smooth because steps land near keyframes frequently.
- **Pause** freezes the demo clock (client camera still live).
- **Step** = seek to exact frame boundary ±N.

### 7.2 Client discontinuity handling
A seek breaks frame-number monotonicity. The server marks the post-seek frame as a **teleport/discontinuity** (reuse the existing large-jump path that already snaps without interpolation, cf. `Cg_UpdateAngles` delta-angle handling and prediction-error reset). Client resets `lerp`/interpolation on that frame.

### 7.3 Multi-POV client storage
Extend the client frame to hold a small `player_state_t pov[MAX_CLIENTS]` array + count, populated only when the server advertises the multi-POV capability flag. Single-POV demos keep `count==1` and the existing `frame->ps` aliases `pov[0]`.

### 7.4 Tests
Seek accuracy (target vs landed timecode within one frame), backward seek correctness (entity state matches a forward replay to the same time), pause/resume, speed 0.1–8×, step ±1, reverse playback.

## 8. Sub-project D — serverrecord (omniscient multi-POV)

New module **`src/server/sv_demo.c`**.

- **Commands:** `sv_record <name>` / `sv_stoprecord`; cvar `sv_demo_keyframe_interval`.
- **Synthetic observer:** build a frame each tick with **all entities (PVS culling bypassed)** and **all in-game clients' `player_state`**, delta-encoded vs the previous omniscient frame, with periodic keyframes — written as v2 `FRAME_*` records with the `SERVER_DEMO` flag. Reuses `Net_WriteDeltaEntity` / `Net_WriteDeltaPlayerState`.
- **Playback:** identical server-hosted v2 path; the `SERVER_DEMO` flag tells the client to expect the multi-POV array, enabling the director's POV selector (free-fly + switch to any player).
- **Capability flag:** the playback server advertises multi-POV support in `SV_CMD_SERVER_DATA`; older clients ignore it and fall back to `pov[0]`.

### 8.1 Tests
Record a bot/LAN match, verify all players' POVs are switchable in replay, omniscient entity completeness (no PVS pop-in), keyframe cadence, file integrity + index.

## 9. Sub-project C — Director: camera paths + timeline scrubber UI

### 9.1 Cinematic camera paths (`cg_demo.c`)
- A camera path = ordered list of keyframes `{ time_ms, origin, angles, fov, ease }`.
- **Interpolation:** Catmull-Rom for `origin`, shortest-arc euler blend for `angles`, smoothstep `ease` in/out per segment; bound to the demo clock so the camera moves as the demo plays *and* as the user scrubs.
- **Authoring:** fly the free camera, `demo_cam_add` drops a keyframe at the current time/pose; `demo_cam_del`, `demo_cam_clear`; `demo_cam_save <name>` / `demo_cam_load <name>` to sidecar `demos/<name>.cam` (simple text). Fixed cams may snap to `info_player_intermission` entities (`g_level.intermission_origin`).
- A `DEMO_CAM_PATH` mode drives `cgi.view` from the interpolated path.

### 9.2 Timeline scrubber UI (`src/cgame/default/ui/demo/DemoViewController.{c,h}`)
ObjectivelyMVC `ViewController` (pattern: `PlayViewController`), pushed via `cgi.PushViewController` when in a demo, toggled by a bind:
- Transport: play/pause, speed, step, current-time / duration readout.
- **Timeline bar** with a draggable **playhead** (→ `demo_seek`), tick marks at keyframes.
- **Camera keyframe track**: markers; add/delete/drag; click to jump.
- **POV selector** (server demos): list of players → `demo_follow`/free.
- All actions issue the §6.3 control commands — the UI is a thin controller over the engine.

### 9.3 Tests
Path interpolation visual smoothness; keyframe add/move/delete; scrub via playhead matches `demo_seek`; POV switch; save/load `.cam` round-trip.

## 10. Cross-cutting

- **Error handling:** corrupt/truncated demos → warn + best-effort (rebuild index by scan; stop cleanly at EOF). Protocol mismatch → clear abort message. Missing `.cam` → ignore. No silent failures.
- **Memory:** multi-POV array bounded by `MAX_CLIENTS`; keyframe index is `O(duration/2 s)` entries; full snapshot ≈ 98 KB (well within budget).
- **Backward compat:** v1 demos play unchanged; live play unaffected (multi-POV array only populated during demo playback behind a capability flag).
- **Code style:** follow `CONTRIBUTING.md` (see `q2server:quetoo-contributing` skill): tabs, brace style, `Cg_`/`Cl_`/`Sv_` prefixes, doxygen comments.

## 11. Build & verification

- **Build:** autotools (`./autogen.sh && ./configure && make`) in the Proxmox build container (per `quetoo-contributing`). CI mirrors exist (`.github/workflows/build.yml`, `build-cgame-windows.yml`).
- **Per-phase gate:** must compile clean (no new warnings) in the container; cgame/client/server link; then in-game smoke test on the Windows RTX 4090 box.
- **Unit tests:** `common/demo.c` record round-trip (write→read→compare), index build/scan, seek-time math, and camera-path interpolation are unit-testable without the full engine (Check-based, matching existing `src/tests` if present).
- **No phase is "done" until it compiles and the manual acceptance checklist for that phase passes** (verification-before-completion).

## 12. Risks & mitigations

| Risk | Mitigation |
|---|---|
| Seek discontinuity confuses client interpolation | Reuse existing teleport/large-jump snap path; explicit discontinuity flag on post-seek frame (§7.2). |
| Multi-POV wire change breaks old clients | Gated behind a capability flag in `SV_CMD_SERVER_DATA`; default off; only during demo playback. |
| Omniscient server frames exceed `MAX_MSG_SIZE` | Keyframes may span multiple transmit packets via existing overflow handling; demo *records* are reframed independently of transmit MTU. |
| Format churn vs upstream | v2 is additive; v1 retained; spec reserves record ids for forward-compat. |
| Big UI (scrubber) risk | Engine + control commands land and are usable via console first; the MVC panel is a thin layer over them, built last. |

## 13. Phased delivery

Each phase: spec→plan→implement→build→verify→commit.

1. **P1 — v2 format primitives** (`common/demo.c`) + unit tests. No engine wiring yet.
2. **P2 — A: free camera + follow/cycle POV.** Works on existing v1 demos. *Immediate value.*
3. **P3 — v2 client recording + server-hosted v2 playback** (forward-only first; legacy v1 retained).
4. **P4 — B: seek/pause/speed/step/reverse** on v2.
5. **P5 — D: serverrecord omniscient multi-POV** + multi-POV client storage + POV selector wiring.
6. **P6 — C: camera paths (console/scripted) then the ObjectivelyMVC timeline scrubber UI.**

## 14. File map

**New:** `src/common/demo.{h,c}`, `src/server/sv_demo.c`, `src/cgame/default/cg_demo.{c,h}`, `src/cgame/default/ui/demo/DemoViewController.{c,h}`.
**Extended:** `src/client/cl_demo.c`, `cl_predict.c`, `cl_entity.c`, `cl_parse.c`, `src/cgame/default/cg_view.c`, `cg_predict.c`, `cg_main.c`, `src/server/sv_send.c`, `sv_init.c`, `sv_main.c`.
