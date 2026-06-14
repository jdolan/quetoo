/*
 * Copyright(c) 1997-2001 id Software, Inc.
 * Copyright(c) 2002 The Quakeforge Project.
 * Copyright(c) 2006 Quetoo.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */

#include "cg_local.h"
#include "game/default/bg_pmove.h"

cvar_t *cg_demo_freecam;
cvar_t *cg_draw_demo_bar;
static cvar_t *cg_draw_demo_hud;
static cvar_t *cg_demo_letterbox;
static cvar_t *cg_demo_fov;
static cvar_t *cg_demo_roll;
static cvar_t *cg_demo_smoothing;
static cvar_t *cg_demo_orbit_radius;
static cvar_t *cg_demo_orbit_height;
static cvar_t *cg_demo_orbit_speed;

/**
 * @brief Demo camera modes.
 */
typedef enum {
  CG_DEMO_CAM_LOCKED, // the recorded player's eye (default)
  CG_DEMO_CAM_FREE,   // free-fly spectator under local input
  CG_DEMO_CAM_FOLLOW, // chase camera following a player entity
  CG_DEMO_CAM_ORBIT,  // cinematic auto-orbit around a player
  CG_DEMO_CAM_PATH,   // cinematic keyframed dolly path
} cg_demo_cam_t;

/**
 * @brief A cinematic camera path keyframe.
 */
typedef struct {
  uint32_t time;  // demo frame time (frame_num * QUETOO_TICK_MILLIS)
  vec3_t origin;
  vec3_t angles;
} cg_demo_key_t;

#define CG_DEMO_MAX_CAM_KEYS 256

static cg_demo_key_t cg_demo_cam_keys[CG_DEMO_MAX_CAM_KEYS];
static int32_t cg_demo_cam_count;

/**
 * @brief Demo camera state. While not LOCKED, the camera detaches from the
 * recorded player and only the view is overridden; the recorded player stays
 * frozen (see Cl_ParsePlayerState).
 */
static struct {
  cg_demo_cam_t mode;
  pm_state_t s;   // free-fly spectator state (CG_DEMO_CAM_FREE)
  int32_t follow; // followed entity number (CG_DEMO_CAM_FOLLOW / ORBIT)

  // cinematic camera smoothing (FOLLOW / ORBIT): the view is eased toward the
  // computed target so player jitter does not transfer to the camera
  vec3_t smooth_origin;
  vec3_t smooth_angles;
  uint32_t smooth_time;
  bool smooth_valid;

  // auto-orbit state (CG_DEMO_CAM_ORBIT)
  float orbit_angle; // current orbit yaw, degrees
  uint32_t orbit_time;
  bool orbit_valid;
} cg_demo;

/**
 * @return The entity number of the first player in the current frame, or -1.
 */
static int32_t Cg_DemoFirstPlayer(void) {

  const cl_frame_t *frame = &cgi.client->frame;

  for (int32_t i = 0; i < frame->num_entities; i++) {
    const uint32_t snum = (frame->entity_state + i) & ENTITY_STATE_MASK;
    const entity_state_t *s = &cgi.client->entity_states[snum];
    if (s->effects & EF_CLIENT) {
      return s->number;
    }
  }

  return -1;
}

/**
 * @brief Resets the camera smoothing so the next FOLLOW/ORBIT frame eases from
 * the current view rather than snapping from a stale position.
 */
static void Cg_DemoSeedSmoothing(void) {
  cg_demo.smooth_origin = cgi.view->origin;
  cg_demo.smooth_angles = cgi.view->angles;
  cg_demo.smooth_time = cgi.client->unclamped_time;
  cg_demo.smooth_valid = true;
}

/**
 * @return True if a demo is playing and the free camera is active.
 */
bool Cg_DemoInFreeCamera(void) {
  return cgi.client->demo_server && cg_demo.mode == CG_DEMO_CAM_FREE;
}

/**
 * @return True if the demo camera is overriding the view (free or follow).
 */
bool Cg_DemoOverridingView(void) {
  return cgi.client->demo_server && cg_demo.mode != CG_DEMO_CAM_LOCKED;
}

/**
 * @return True if the HUD should be hidden for a clean cinematic shot.
 */
bool Cg_DemoHidesHud(void) {
  return cgi.client->demo_server && !cg_draw_demo_hud->integer;
}

/**
 * @brief Toggles the free demo camera, seeding it from the current view so the
 * camera does not jump when it is enabled.
 */
static void Cg_DemoFreecam_f(void) {

  if (!cgi.client->demo_server) {
    cgi.Print("Not playing a demo\n");
    return;
  }

  if (cg_demo.mode == CG_DEMO_CAM_FREE) {
    cg_demo.mode = CG_DEMO_CAM_LOCKED;
    cgi.Print("Demo free camera disabled\n");
  } else {
    cg_demo.mode = CG_DEMO_CAM_FREE;
    cg_demo.s = (pm_state_t) {
      .type = PM_SPECTATOR,
      .origin = cgi.view->origin,
    };
    cgi.client->angles = cgi.view->angles;
    cgi.Print("Demo free camera enabled\n");
  }
}

/**
 * @brief Cycles the follow camera through the player entities present in the
 * current frame. Works on any demo (the players' entities are always present).
 */
static void Cg_DemoFollow(int32_t dir) {

  if (!cgi.client->demo_server) {
    cgi.Print("Not playing a demo\n");
    return;
  }

  const cl_frame_t *frame = &cgi.client->frame;

  int32_t players[MAX_CLIENTS];
  int32_t count = 0;

  for (int32_t i = 0; i < frame->num_entities && count < MAX_CLIENTS; i++) {
    const uint32_t snum = (frame->entity_state + i) & ENTITY_STATE_MASK;
    const entity_state_t *s = &cgi.client->entity_states[snum];
    if (s->effects & EF_CLIENT) {
      players[count++] = s->number;
    }
  }

  if (count == 0) {
    cgi.Print("No players to follow\n");
    return;
  }

  int32_t index = -1;
  if (cg_demo.mode == CG_DEMO_CAM_FOLLOW) {
    for (int32_t i = 0; i < count; i++) {
      if (players[i] == cg_demo.follow) {
        index = i;
        break;
      }
    }
  }

  if (index < 0) {
    index = (dir > 0) ? 0 : count - 1;
  } else {
    index = (index + dir + count) % count;
  }

  cg_demo.follow = players[index];

  if (cg_demo.mode != CG_DEMO_CAM_FOLLOW) {
    cg_demo.mode = CG_DEMO_CAM_FOLLOW;
    Cg_DemoSeedSmoothing();
  }

  cgi.Print("Following player %d\n", cg_demo.follow);
}

static void Cg_DemoFollowNext_f(void) {
  Cg_DemoFollow(1);
}

static void Cg_DemoFollowPrev_f(void) {
  Cg_DemoFollow(-1);
}

/**
 * @brief Trace wrapper for the spectator Pm_Move.
 */
static cm_trace_t Cg_DemoCamera_Trace(const vec3_t start, const vec3_t end, const box3_t bounds) {
  return cgi.Trace(start, end, bounds, NULL, CONTENTS_MASK_CLIP_PLAYER);
}

/**
 * @brief Integrates the free camera from the pending user commands as a no-clip
 * spectator. The command msec is real frame time, so the camera flies at a
 * constant real-time speed regardless of the demo playback speed.
 */
void Cg_PredictDemoCamera(const GPtrArray *cmds) {

  pm_move_t pm = { .s = cg_demo.s };
  pm.s.type = PM_SPECTATOR;

  pm.PointContents = cgi.PointContents;
  pm.BoxContents = cgi.BoxContents;
  pm.Trace = Cg_DemoCamera_Trace;

  pm.Debug = cgi.Debug;
  pm.DebugMask = cgi.DebugMask;
  pm.debug_mask = DEBUG_PMOVE_CLIENT;

  for (guint i = 0; i < cmds->len; i++) {
    const cl_cmd_t *cmd = g_ptr_array_index(cmds, i);
    if (cmd->cmd.msec) {
      pm.cmd = cmd->cmd;
      Pm_Move(&pm);
    }
  }

  cg_demo.s = pm.s;
}

/**
 * @brief Writes the free camera origin and angles into the view.
 */
static void Cg_UpdateDemoFreeCamera(void) {

  cgi.view->origin = Vec3_Add(cg_demo.s.origin, cg_demo.s.view_offset);
  cgi.view->angles = cgi.client->angles;
  cgi.view->angles.z += cg_demo_roll->value; // cinematic camera roll (Dutch angle)

  Vec3_Vectors(cgi.view->angles, &cgi.view->forward, &cgi.view->right, &cgi.view->up);
}

/**
 * @brief Eases the view toward the given target origin/angles using a
 * time-constant exponential filter (cg_demo_smoothing, in seconds of lag), then
 * writes the smoothed result into the view. With smoothing at 0 the view snaps.
 */
static void Cg_DemoSmoothView(const vec3_t origin, const vec3_t angles) {

  const float lag = cg_demo_smoothing->value;
  const uint32_t now = cgi.client->unclamped_time;

  if (lag <= 0.f || !cg_demo.smooth_valid) {
    cg_demo.smooth_origin = origin;
    cg_demo.smooth_angles = angles;
  } else {
    const float dt = (float) (now - cg_demo.smooth_time);
    const float alpha = Clampf(1.f - expf(-dt / (lag * 1000.f)), 0.f, 1.f);
    cg_demo.smooth_origin = Vec3_Mix(cg_demo.smooth_origin, origin, alpha);
    cg_demo.smooth_angles = Vec3_MixEuler(cg_demo.smooth_angles, angles, alpha);
  }

  cg_demo.smooth_time = now;
  cg_demo.smooth_valid = true;

  cgi.view->origin = cg_demo.smooth_origin;
  cgi.view->angles = cg_demo.smooth_angles;

  Vec3_Vectors(cgi.view->angles, &cgi.view->forward, &cgi.view->right, &cgi.view->up);
}

/**
 * @brief Resolves the followed entity, re-acquiring if the current target is no
 * longer a live player in this frame (e.g. after a seek, a disconnect, or the
 * player leaving the recorder's view). Returns NULL if there is no one to follow.
 */
static const cl_entity_t *Cg_DemoFollowEntity(void) {

  if (cg_demo.follow >= 0 && cg_demo.follow < MAX_ENTITIES) {
    const cl_entity_t *ent = &cgi.client->entities[cg_demo.follow];
    if (ent->frame_num == cgi.client->frame.frame_num && (ent->current.effects & EF_CLIENT)) {
      return ent; // still current
    }
  }

  cg_demo.follow = Cg_DemoFirstPlayer(); // stale; re-acquire a present player
  if (cg_demo.follow >= 0) {
    return &cgi.client->entities[cg_demo.follow];
  }

  return NULL;
}

/**
 * @brief Positions the camera as a third-person chase of the followed player.
 */
static void Cg_UpdateDemoFollowCamera(void) {

  const cl_entity_t *ent = Cg_DemoFollowEntity();
  if (ent == NULL) {
    return;
  }

  const vec3_t target = ent->origin;

  vec3_t forward, right, up;
  Vec3_Vectors(Vec3(0.f, ent->angles.y, 0.f), &forward, &right, &up);

  vec3_t desired = Vec3_Fmaf(target, -120.f, forward);
  desired.z += 50.f;

  const cm_trace_t tr = cgi.Trace(target, desired, Box3f(8.f, 8.f, 8.f), NULL, CONTENTS_MASK_CLIP_PLAYER);

  const vec3_t dir = Vec3_Subtract(target, tr.end);
  Cg_DemoSmoothView(tr.end, Vec3_Euler(Vec3_Normalize(dir)));
}

/**
 * @brief Slowly orbits the camera around the followed player at a configurable
 * radius, height, and angular speed -- a hands-free cinematic shot. The orbit
 * advances in real time so it is unaffected by demo speed or pausing.
 */
static void Cg_UpdateDemoOrbitCamera(void) {

  const cl_entity_t *ent = Cg_DemoFollowEntity();
  if (ent == NULL) {
    return;
  }

  const vec3_t target = Vec3_Add(ent->origin, Vec3(0.f, 0.f, 24.f));

  const uint32_t now = cgi.client->unclamped_time;
  if (cg_demo.orbit_valid) {
    const float dt = (float) (now - cg_demo.orbit_time) / 1000.f;
    cg_demo.orbit_angle += dt * cg_demo_orbit_speed->value;
  }
  cg_demo.orbit_time = now;
  cg_demo.orbit_valid = true;

  const float yaw = Radians(cg_demo.orbit_angle);
  const float radius = cg_demo_orbit_radius->value;

  vec3_t desired = target;
  desired.x += cosf(yaw) * radius;
  desired.y += sinf(yaw) * radius;
  desired.z += cg_demo_orbit_height->value;

  const cm_trace_t tr = cgi.Trace(target, desired, Box3f(8.f, 8.f, 8.f), NULL, CONTENTS_MASK_CLIP_PLAYER);

  const vec3_t dir = Vec3_Subtract(target, tr.end);
  Cg_DemoSmoothView(tr.end, Vec3_Euler(Vec3_Normalize(dir)));
}

/**
 * @brief demo_orbit — toggles the cinematic auto-orbit camera, targeting the
 * followed player or, if none, the first player in the frame.
 */
static void Cg_DemoOrbit_f(void) {

  if (!cgi.client->demo_server) {
    cgi.Print("Not playing a demo\n");
    return;
  }

  if (cg_demo.mode == CG_DEMO_CAM_ORBIT) {
    cg_demo.mode = CG_DEMO_CAM_LOCKED;
    cgi.Print("Orbit camera disabled\n");
    return;
  }

  if (cg_demo.follow < 0 || cg_demo.follow >= MAX_ENTITIES ||
      !(cgi.client->entities[cg_demo.follow].current.effects & EF_CLIENT)) {
    cg_demo.follow = Cg_DemoFirstPlayer();
  }

  if (cg_demo.follow < 0) {
    cgi.Print("No players to orbit\n");
    return;
  }

  cg_demo.mode = CG_DEMO_CAM_ORBIT;
  cg_demo.orbit_valid = false;
  Cg_DemoSeedSmoothing();

  cgi.Print("Orbit camera on player %d\n", cg_demo.follow);
}

/**
 * @brief demo_cinematic — one-shot toggle for a clean cinematic look: letterbox
 * bars on, HUD and timeline hidden, extra camera smoothing. Toggling off
 * restores the defaults.
 */
static void Cg_DemoCinematic_f(void) {
  static bool on = false;

  if (!cgi.client->demo_server) {
    cgi.Print("Not playing a demo\n");
    return;
  }

  on = !on;

  if (on) {
    cgi.Cbuf("cg_demo_letterbox 0.12; cg_draw_demo_hud 0; cg_draw_demo_bar 0; cg_demo_smoothing 0.3\n");
    cgi.Print("Cinematic mode enabled\n");
  } else {
    cgi.Cbuf("cg_demo_letterbox 0; cg_draw_demo_hud 1; cg_draw_demo_bar 1; cg_demo_smoothing 0.15\n");
    cgi.Print("Cinematic mode disabled\n");
  }
}

#pragma mark - Camera paths

#define CG_DEMO_CAM_PATH_FILE "demos/cinematic.cam"

/**
 * @brief Catmull-Rom interpolation of four control points at fraction t [0,1].
 */
static vec3_t Cg_DemoCatmullRom(const vec3_t p0, const vec3_t p1, const vec3_t p2, const vec3_t p3, float t) {

  const float t2 = t * t;
  const float t3 = t2 * t;

  // c = 2*p0 - 5*p1 + 4*p2 - p3
  vec3_t c = Vec3_Scale(p0, 2.f);
  c = Vec3_Fmaf(c, -5.f, p1);
  c = Vec3_Fmaf(c, 4.f, p2);
  c = Vec3_Subtract(c, p3);

  // d = -p0 + 3*p1 - 3*p2 + p3
  vec3_t d = Vec3_Scale(p1, 3.f);
  d = Vec3_Subtract(d, p0);
  d = Vec3_Fmaf(d, -3.f, p2);
  d = Vec3_Add(d, p3);

  // r = (2*p1 + t*(p2 - p0) + t2*c + t3*d) / 2
  vec3_t r = Vec3_Scale(p1, 2.f);
  r = Vec3_Fmaf(r, t, Vec3_Subtract(p2, p0));
  r = Vec3_Fmaf(r, t2, c);
  r = Vec3_Fmaf(r, t3, d);

  return Vec3_Scale(r, 0.5f);
}

/**
 * @brief demo_cam_add — captures the current view as a camera keyframe.
 */
static void Cg_DemoCamAdd_f(void) {

  if (!cgi.client->demo_server) {
    cgi.Print("Not playing a demo\n");
    return;
  }

  if (cg_demo_cam_count >= CG_DEMO_MAX_CAM_KEYS) {
    cgi.Print("Camera path is full\n");
    return;
  }

  const uint32_t t = cgi.client->frame.time;

  // keep the keyframes sorted by time
  int32_t i = cg_demo_cam_count;
  while (i > 0 && cg_demo_cam_keys[i - 1].time > t) {
    cg_demo_cam_keys[i] = cg_demo_cam_keys[i - 1];
    i--;
  }

  cg_demo_cam_keys[i].time = t;
  cg_demo_cam_keys[i].origin = cgi.view->origin;
  cg_demo_cam_keys[i].angles = cgi.view->angles;
  cg_demo_cam_count++;

  cgi.Print("Camera keyframe %d at %.1fs\n", cg_demo_cam_count, t / 1000.0);
}

/**
 * @brief demo_cam_clear — removes all camera keyframes.
 */
static void Cg_DemoCamClear_f(void) {

  cg_demo_cam_count = 0;

  if (cg_demo.mode == CG_DEMO_CAM_PATH) {
    cg_demo.mode = CG_DEMO_CAM_LOCKED;
  }

  cgi.Print("Camera path cleared\n");
}

/**
 * @brief demo_cam_play — toggles flying the cinematic camera path.
 */
static void Cg_DemoCamPlay_f(void) {

  if (!cgi.client->demo_server) {
    cgi.Print("Not playing a demo\n");
    return;
  }

  if (cg_demo.mode == CG_DEMO_CAM_PATH) {
    cg_demo.mode = CG_DEMO_CAM_LOCKED;
    cgi.Print("Camera path stopped\n");
    return;
  }

  if (cg_demo_cam_count < 2) {
    cgi.Print("Add at least 2 camera keyframes first (demo_cam_add)\n");
    return;
  }

  cg_demo.mode = CG_DEMO_CAM_PATH;
  cgi.Print("Flying camera path (%d keyframes)\n", cg_demo_cam_count);
}

/**
 * @brief demo_cam_save — writes the camera path to a sidecar file.
 */
static void Cg_DemoCamSave_f(void) {

  if (cg_demo_cam_count == 0) {
    cgi.Print("No camera path to save\n");
    return;
  }

  file_t *file = cgi.OpenFileWrite(CG_DEMO_CAM_PATH_FILE);
  if (!file) {
    cgi.Print("Couldn't write %s\n", CG_DEMO_CAM_PATH_FILE);
    return;
  }

  for (int32_t i = 0; i < cg_demo_cam_count; i++) {
    const cg_demo_key_t *k = &cg_demo_cam_keys[i];
    const char *line = va("%u %f %f %f %f %f %f\n", k->time,
                          k->origin.x, k->origin.y, k->origin.z,
                          k->angles.x, k->angles.y, k->angles.z);
    cgi.WriteFile(file, line, 1, strlen(line));
  }

  cgi.CloseFile(file);
  cgi.Print("Saved %d camera keyframes to %s\n", cg_demo_cam_count, CG_DEMO_CAM_PATH_FILE);
}

/**
 * @brief demo_cam_load — reads the camera path from a sidecar file.
 */
static void Cg_DemoCamLoad_f(void) {

  file_t *file = cgi.OpenFile(CG_DEMO_CAM_PATH_FILE);
  if (!file) {
    cgi.Print("No saved camera path (%s)\n", CG_DEMO_CAM_PATH_FILE);
    return;
  }

  static char buffer[CG_DEMO_MAX_CAM_KEYS * 80];
  const int64_t len = cgi.ReadFile(file, buffer, 1, sizeof(buffer) - 1);
  cgi.CloseFile(file);

  if (len <= 0) {
    return;
  }
  buffer[len] = '\0';

  cg_demo_cam_count = 0;
  char *line = strtok(buffer, "\n");
  while (line && cg_demo_cam_count < CG_DEMO_MAX_CAM_KEYS) {
    cg_demo_key_t *k = &cg_demo_cam_keys[cg_demo_cam_count];
    if (sscanf(line, "%u %f %f %f %f %f %f", &k->time,
               &k->origin.x, &k->origin.y, &k->origin.z,
               &k->angles.x, &k->angles.y, &k->angles.z) == 7) {
      cg_demo_cam_count++;
    }
    line = strtok(NULL, "\n");
  }

  cgi.Print("Loaded %d camera keyframes\n", cg_demo_cam_count);
}

/**
 * @brief Drives the view along the cinematic camera path by the demo time.
 */
static void Cg_UpdateDemoPathCamera(void) {

  const uint32_t now = cgi.client->frame.time;

  // find the segment [i, i+1] containing the current time
  int32_t i = 0;
  while (i < cg_demo_cam_count - 2 && cg_demo_cam_keys[i + 1].time <= now) {
    i++;
  }

  const cg_demo_key_t *k1 = &cg_demo_cam_keys[i];
  const cg_demo_key_t *k2 = &cg_demo_cam_keys[i + 1];
  const cg_demo_key_t *k0 = &cg_demo_cam_keys[i > 0 ? i - 1 : i];
  const cg_demo_key_t *k3 = &cg_demo_cam_keys[i + 2 < cg_demo_cam_count ? i + 2 : i + 1];

  float t = (k2->time > k1->time) ? (float) (now - k1->time) / (float) (k2->time - k1->time) : 0.f;
  t = Clampf(t, 0.f, 1.f);

  cgi.view->origin = Cg_DemoCatmullRom(k0->origin, k1->origin, k2->origin, k3->origin, t);
  cgi.view->angles = Vec3_MixEuler(k1->angles, k2->angles, t);

  Vec3_Vectors(cgi.view->angles, &cgi.view->forward, &cgi.view->right, &cgi.view->up);
}

/**
 * @brief Writes the demo camera into the view, dispatching by mode.
 */
void Cg_UpdateDemoView(void) {

  if (cg_demo.mode == CG_DEMO_CAM_FREE) {
    Cg_UpdateDemoFreeCamera();
  } else if (cg_demo.mode == CG_DEMO_CAM_FOLLOW) {
    Cg_UpdateDemoFollowCamera();
  } else if (cg_demo.mode == CG_DEMO_CAM_ORBIT) {
    Cg_UpdateDemoOrbitCamera();
  } else if (cg_demo.mode == CG_DEMO_CAM_PATH) {
    Cg_UpdateDemoPathCamera();
  }
}

/**
 * @brief Applies the cinematic FOV override during demo playback. When
 * cg_demo_fov is greater than zero it replaces the computed field of view,
 * allowing dramatic wide or zoomed shots. Called after Cg_UpdateFov.
 */
void Cg_UpdateDemoFov(void) {

  if (!cgi.client->demo_server) {
    return;
  }

  const float fov = Clampf(cg_demo_fov->value, 0.f, 170.f);
  if (fov <= 0.f) {
    return;
  }

  // fov.x/fov.y are half-angles; derive them exactly as Cg_UpdateFov does
  cgi.view->fov.x = fov / 2.f;

  const float width = cgi.view->viewport.z;
  const float height = cgi.view->viewport.w;

  cgi.view->fov.y = Degrees(atanf(tanf(Radians(fov / 2.f)) * height / width));
}

/**
 * @brief Draws cinematic letterbox bars (top and bottom) during demo playback.
 * cg_demo_letterbox is the fraction of the screen height each bar covers.
 */
void Cg_DrawDemoLetterbox(void) {

  if (!cgi.client->demo_server) {
    return;
  }

  const float frac = Clampf(cg_demo_letterbox->value, 0.f, 0.45f);
  if (frac <= 0.f) {
    return;
  }

  const GLint w = cgi.context->w;
  const GLint h = cgi.context->h;
  const GLint bar = (GLint) (h * frac);

  cgi.Draw2DFill(0, 0, w, bar, color_black);
  cgi.Draw2DFill(0, h - bar, w, bar, color_black);
}

/**
 * @brief Draws the demo timeline scrubber bar during playback. The demo time
 * and duration come from the server via the CS_DEMO_STATUS config string.
 */
void Cg_DrawDemoBar(void) {

  if (!cg_draw_demo_bar->integer) {
    return; // bar hidden by the user
  }

  if (!cgi.client->demo_server) {
    return;
  }

  const char *status = cgi.ConfigString(CS_DEMO_STATUS);
  if (!status || !*status) {
    return; // v1 demo, or status not yet received
  }

  uint32_t time = 0, duration = 0;
  int32_t paused = 0;
  float speed = 1.f;
  sscanf(status, "%u %u %d %f", &time, &duration, &paused, &speed);

  if (duration == 0) {
    return;
  }

  const GLint w = cgi.context->w;
  const GLint h = cgi.context->h;

  const GLint barWidth = (GLint) (w * 0.6f);
  const GLint barHeight = 6;
  const GLint barX = (w - barWidth) / 2;
  const GLint barY = h - 48;

  const float frac = Clampf((float) time / (float) duration, 0.f, 1.f);
  const GLint progress = (GLint) (barWidth * frac);

  cgi.Draw2DFill(barX, barY, barWidth, barHeight, ColorHSVA(0.f, 0.f, 0.f, 0.5f));
  cgi.Draw2DFill(barX, barY, progress, barHeight, color_orange);
  cgi.Draw2DFill(barX + progress - 1, barY - 4, 3, barHeight + 8, color_white);

  char text[MAX_STRING_CHARS];
  g_snprintf(text, sizeof(text), "%u:%02u / %u:%02u   %.2gx%s",
             time / 60000, (time / 1000) % 60,
             duration / 60000, (duration / 1000) % 60,
             speed, paused ? "   PAUSED" : "");

  cgi.Draw2DString(barX, barY - 22, text, color_white);
}

/**
 * @brief Resets all demo camera state. Called when a server connection is
 * established (Cg_ClearState) so the camera mode, follow target, smoothing,
 * orbit, and cinematic path start clean for each demo rather than leaking from
 * the previously played one.
 */
void Cg_ClearDemo(void) {

  memset(&cg_demo, 0, sizeof(cg_demo));
  cg_demo.mode = CG_DEMO_CAM_LOCKED;
  cg_demo.follow = -1;

  cg_demo_cam_count = 0;
}

/**
 * @brief Registers the demo camera cvar and commands.
 */
void Cg_InitDemo(void) {

  cg_demo_freecam = cgi.AddCvar("cg_demo_freecam", "1", CVAR_ARCHIVE,
      "Enables the free spectator camera during demo playback.");

  cg_draw_demo_bar = cgi.AddCvar("cg_draw_demo_bar", "1", CVAR_ARCHIVE,
      "Draws the demo playback timeline bar. Set to 0 to hide it.");

  cg_draw_demo_hud = cgi.AddCvar("cg_draw_demo_hud", "1", CVAR_ARCHIVE,
      "Draws the HUD during demo playback. Set to 0 for a clean cinematic view.");

  cg_demo_letterbox = cgi.AddCvar("cg_demo_letterbox", "0", CVAR_ARCHIVE,
      "Cinematic letterbox bar size during demo playback (0 to 0.45 of screen height).");

  cg_demo_fov = cgi.AddCvar("cg_demo_fov", "0", CVAR_ARCHIVE,
      "Cinematic field of view override during demo playback (0 to use the normal fov).");

  cg_demo_roll = cgi.AddCvar("cg_demo_roll", "0", CVAR_ARCHIVE,
      "Cinematic free-camera roll (Dutch angle), in degrees.");

  cg_demo_smoothing = cgi.AddCvar("cg_demo_smoothing", "0.15", CVAR_ARCHIVE,
      "Follow/orbit camera smoothing, in seconds of lag (0 to disable).");

  cg_demo_orbit_radius = cgi.AddCvar("cg_demo_orbit_radius", "180", CVAR_ARCHIVE,
      "Cinematic orbit camera radius, in units.");

  cg_demo_orbit_height = cgi.AddCvar("cg_demo_orbit_height", "64", CVAR_ARCHIVE,
      "Cinematic orbit camera height above the target, in units.");

  cg_demo_orbit_speed = cgi.AddCvar("cg_demo_orbit_speed", "30", CVAR_ARCHIVE,
      "Cinematic orbit camera angular speed, in degrees per second.");

  cgi.AddCmd("demo_freecam", Cg_DemoFreecam_f, CMD_CGAME,
      "Toggle the free-fly camera while watching a demo.");

  cgi.AddCmd("demo_orbit", Cg_DemoOrbit_f, CMD_CGAME,
      "Toggle the cinematic auto-orbit camera while watching a demo.");

  cgi.AddCmd("demo_cinematic", Cg_DemoCinematic_f, CMD_CGAME,
      "Toggle a clean cinematic look (letterbox, no HUD, smoothing).");

  cgi.AddCmd("demo_follow_next", Cg_DemoFollowNext_f, CMD_CGAME,
      "Follow the next player while watching a demo.");

  cgi.AddCmd("demo_follow_prev", Cg_DemoFollowPrev_f, CMD_CGAME,
      "Follow the previous player while watching a demo.");

  cgi.AddCmd("demo_cam_add", Cg_DemoCamAdd_f, CMD_CGAME,
      "Add a cinematic camera keyframe at the current view.");
  cgi.AddCmd("demo_cam_clear", Cg_DemoCamClear_f, CMD_CGAME,
      "Clear all cinematic camera keyframes.");
  cgi.AddCmd("demo_cam_play", Cg_DemoCamPlay_f, CMD_CGAME,
      "Toggle flying the cinematic camera path.");
  cgi.AddCmd("demo_cam_save", Cg_DemoCamSave_f, CMD_CGAME,
      "Save the cinematic camera path to " CG_DEMO_CAM_PATH_FILE ".");
  cgi.AddCmd("demo_cam_load", Cg_DemoCamLoad_f, CMD_CGAME,
      "Load the cinematic camera path from " CG_DEMO_CAM_PATH_FILE ".");
}
