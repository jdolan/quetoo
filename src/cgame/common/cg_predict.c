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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#include "cg_local.h"
#include "game/common/bg_pmove.h"

/**
 * @brief Returns true if client side prediction should be used.
 */
static bool Cg_UsePrediction_Common(void) {

  if (!cg_predict->value) {
    return false;
  }

  if (cgi.client->demoServer) {
    return false;
  }

  if (cgi.client->thirdPerson) {
    return false;
  }

  if (cgi.client->deltaFrame == NULL) {
    return false;
  }

  if (cgi.client->frame.ps.pmState.type == PM_DEAD) {
    return false;
  }

  if (cgi.client->frame.ps.pmState.type == PM_FREEZE) {
    return false;
  }

  return true;
}

UsePrediction Cg_UsePrediction = Cg_UsePrediction_Common;

/**
 * @brief The `UsePrediction` export. The client holds this rather than the chain head, so
 * that the chain a module installs from `Cg_Module_Init` is the one that gets
 * called.
 */
bool Cg_ExportUsePrediction(void) {
  return Cg_UsePrediction();
}

/**
 * @brief The tail of the `Cg_MoveCommandWillRun` chain: a notification, so it does nothing.
 */
static void Cg_MoveCommandWillRun_Common(PMove *pm, const ClientCmd *cmd) {
}

MoveCommandWillRun Cg_MoveCommandWillRun = Cg_MoveCommandWillRun_Common;

/**
 * @brief The tail of the `Cg_MoveCommandDidRun` chain: a notification, so it does nothing.
 */
static void Cg_MoveCommandDidRun_Common(const PMove *pm, const ClientCmd *cmd) {
}

MoveCommandDidRun Cg_MoveCommandDidRun = Cg_MoveCommandDidRun_Common;

/**
 * @brief The tail of the `Cg_PredictionDidComplete` chain: a notification, so it does nothing.
 */
static void Cg_PredictionDidComplete_Common(const PMove *pm) {
}

PredictionDidComplete Cg_PredictionDidComplete = Cg_PredictionDidComplete_Common;

/**
 * @brief Trace wrapper for `Pm_Move`.
 */
static CmTrace Cg_PredictMovement_Trace(const Vec3 start, const Vec3 end, const Box3 bounds) {
  return cgi.Trace(start, end, bounds, cgi.client->entity, CONTENTS_MASK_CLIP_PLAYER);
}

/**
 * @brief The `Cg_ClipEntity` chain has no tail: it is `NULL` until a module
 * installs a link, and `Cg_Init` exports whatever is installed, so that a
 * client game with nothing to say is never asked.
 */
ClipClientEntity Cg_ClipEntity = NULL;

/**
 * @brief Run recent movement commands through the player movement code locally, storing the
 * resulting state so that it may be interpolated to and reconciled later.
 */
void Cg_PredictMovement(const Vector *cmds) {

  assert(cmds);
  assert(cmds->count);

  ClientPredictedState *pr = &cgi.client->predictedState;

  // copy current state to into the move
  PMove pm = {};
  pm.s = cgi.client->frame.ps.pmState;

  pm.ground = pr->ground;
#if defined(G_HOOK)
  pm.hookPullSpeed = cgState.hookPullSpeed;
#endif

  pm.PointContents = cgi.PointContents;
  pm.BoxContents = cgi.BoxContents;
  
  pm.Trace = Cg_PredictMovement_Trace;

  pm.Debug = cgi.Debug;
  pm.DebugMask = cgi.DebugMask;
  pm.debugMask = DEBUG_PMOVE_CLIENT;

  // run the commands
  for (uint32_t i = 0; i < cmds->count; i++) {
    ClientCmd *cmd = VectorValue(cmds, ClientCmd *, i);

    if (cmd->cmd.msec) { // if the command has time, run it

      // timestamp it so the client knows we have valid results
      cmd->prediction.time = cgi.client->time;

      // simulate the movement
      pm.cmd = cmd->cmd;

      Cg_MoveCommandWillRun(&pm, cmd);

      Pm_Move(&pm);

      Cg_MoveCommandDidRun(&pm, cmd);
    }

    // save for error detection
    cmd->prediction.origin = pm.s.origin;
  }

  Cg_PredictionDidComplete(&pm);

  // save for rendering
  if (Vec3_Distance(pr->view.origin, pm.s.origin) > TRACE_EPSILON) {
    pr->view.origin = pm.s.origin;
  }
  pr->view.offset = pm.s.viewOffset;
  pr->view.stepOffset = pm.s.stepOffset;

  // If the server is requesting a snap, use the authoritative angles rather than
  // the last cmd angles, which may be stale (pre-snap) pending commands.
  if (cgState.snapAngles) {
    pr->view.angles = cgState.snapViewAngles;
  } else {
    pr->view.angles = pm.cmd.angles;
  }

  pr->ground = pm.ground;
}

/**
 * @brief Drives demo playback's free-flight camera directly through `Pm_Move`, independent of
 * the recorded `PlayerState` and of the network command backlog `Cg_PredictMovement` relies
 * on (which never resolves during demo playback: no server ever acknowledges a demo's locally
 * numbered outgoing commands, so `Cl_PredictMovement` always exceeds `CMD_BACKUP` and never
 * calls in). Called every movement command cycle from `Cg_Move`, the same cadence prediction
 * would otherwise run at, using the `cmd` that cycle already built for us.
 */
void Cg_UpdateSpectate(PMoveCmd *cmd) {

  if (!cgState.spectate.initialized) {
    cgState.spectate.state.type = PM_SPECTATOR;
    cgState.spectate.state.origin = cgi.view->origin;

    // take over the look angles from wherever the camera is pointing, rather than from the
    // recorded player's aim, which is what cgi.client->angles still holds: Cg_UpdateAngles stops
    // syncing it once this mode resolves the view, and every move from here reads it back
    cgi.client->angles = cgi.view->angles;

    cgState.spectate.initialized = true;
  }

  PMove pm = {};
  pm.s = cgState.spectate.state;

  // Pm_SpectatorMove reads speedSpectator, accelSpectator and frictionSpectator from the
  // movement parameters, which the recording carries; without them the camera holds still
  pm.s.params = cgi.client->frame.ps.pmState.params;

  pm.cmd = *cmd;
  pm.cmd.angles = cgi.client->angles;

  pm.PointContents = cgi.PointContents;
  pm.BoxContents = cgi.BoxContents;
  pm.Trace = Cg_PredictMovement_Trace;

  pm.Debug = cgi.Debug;
  pm.DebugMask = cgi.DebugMask;
  pm.debugMask = DEBUG_PMOVE_CLIENT;

  Pm_Move(&pm);

  cgState.spectate.state = pm.s;
}
