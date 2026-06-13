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

/**
 * @brief Demo camera modes.
 */
typedef enum {
  CG_DEMO_CAM_LOCKED, // the recorded player's eye (default)
  CG_DEMO_CAM_FREE,   // free-fly spectator under local input
  CG_DEMO_CAM_FOLLOW, // chase camera following a player entity
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
  int32_t follow; // followed entity number (CG_DEMO_CAM_FOLLOW)
} cg_demo;

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
  cg_demo.mode = CG_DEMO_CAM_FOLLOW;

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

  Vec3_Vectors(cgi.view->angles, &cgi.view->forward, &cgi.view->right, &cgi.view->up);
}

/**
 * @brief Positions the camera as a third-person chase of the followed player.
 */
static void Cg_UpdateDemoFollowCamera(void) {

  const cl_entity_t *ent = &cgi.client->entities[cg_demo.follow];
  const vec3_t target = ent->origin;

  vec3_t forward, right, up;
  Vec3_Vectors(Vec3(0.f, ent->angles.y, 0.f), &forward, &right, &up);

  vec3_t desired = Vec3_Fmaf(target, -120.f, forward);
  desired.z += 50.f;

  const cm_trace_t tr = cgi.Trace(target, desired, Box3f(8.f, 8.f, 8.f), NULL, CONTENTS_MASK_CLIP_PLAYER);
  cgi.view->origin = tr.end;

  const vec3_t dir = Vec3_Subtract(target, cgi.view->origin);
  cgi.view->angles = Vec3_Euler(Vec3_Normalize(dir));

  Vec3_Vectors(cgi.view->angles, &cgi.view->forward, &cgi.view->right, &cgi.view->up);
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
  } else if (cg_demo.mode == CG_DEMO_CAM_PATH) {
    Cg_UpdateDemoPathCamera();
  }
}

/**
 * @brief Registers the demo camera cvar and commands.
 */
void Cg_InitDemo(void) {

  cg_demo_freecam = cgi.AddCvar("cg_demo_freecam", "1", CVAR_ARCHIVE,
      "Enables the free spectator camera during demo playback.");

  cgi.AddCmd("demo_freecam", Cg_DemoFreecam_f, CMD_CGAME,
      "Toggle the free-fly camera while watching a demo.");

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
