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
} cg_demo_cam_t;

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

/**
 * @brief Writes the demo camera into the view, dispatching by mode.
 */
void Cg_UpdateDemoView(void) {

  if (cg_demo.mode == CG_DEMO_CAM_FREE) {
    Cg_UpdateDemoFreeCamera();
  } else if (cg_demo.mode == CG_DEMO_CAM_FOLLOW) {
    Cg_UpdateDemoFollowCamera();
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
}
