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
 * @brief Free demo camera state. While active, the camera detaches from the
 * recorded player and flies under local input as a PM_SPECTATOR. The recorded
 * player remains frozen (see Cl_ParsePlayerState); only the view is overridden.
 */
static struct {
  bool active;
  pm_state_t s;
} cg_demo;

/**
 * @return True if a demo is playing and the free camera is active.
 */
bool Cg_DemoInFreeCamera(void) {
  return cgi.client->demo_server && cg_demo_freecam->value && cg_demo.active;
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

  cg_demo.active = !cg_demo.active;

  if (cg_demo.active) {
    cg_demo.s = (pm_state_t) {
      .type = PM_SPECTATOR,
      .origin = cgi.view->origin,
    };
    cgi.client->angles = cgi.view->angles;
    cgi.Print("Demo free camera enabled\n");
  } else {
    cgi.Print("Demo free camera disabled\n");
  }
}

/**
 * @brief Trace wrapper for the spectator Pm_Move.
 */
static cm_trace_t Cg_DemoCamera_Trace(const vec3_t start, const vec3_t end, const box3_t bounds) {
  return cgi.Trace(start, end, bounds, NULL, CONTENTS_MASK_CLIP_PLAYER);
}

/**
 * @brief Integrates the free camera from the pending user commands as a no-clip
 * spectator. Because the command msec reflects real frame time, the camera flies
 * at a constant real-time speed regardless of the demo playback speed.
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
 * @brief Writes the free camera origin and angles into the view, replacing the
 * recorded player's eye for this frame.
 */
void Cg_UpdateDemoCamera(void) {

  cgi.view->origin = Vec3_Add(cg_demo.s.origin, cg_demo.s.view_offset);
  cgi.view->angles = cgi.client->angles;

  Vec3_Vectors(cgi.view->angles, &cgi.view->forward, &cgi.view->right, &cgi.view->up);
}

/**
 * @brief Registers the demo camera cvar and command.
 */
void Cg_InitDemo(void) {

  cg_demo_freecam = cgi.AddCvar("cg_demo_freecam", "1", CVAR_ARCHIVE,
      "Enables the free spectator camera during demo playback.");

  cgi.AddCmd("demo_freecam", Cg_DemoFreecam_f, CMD_CGAME,
      "Toggle the free-fly camera while watching a demo.");
}
