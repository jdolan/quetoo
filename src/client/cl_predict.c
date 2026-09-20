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

#include "cl_local.h"

/**
 * @brief Prepares the collision model to clip to the specified entity. For
 * mesh models, the box hull must be set to reflect the bounds of the entity.
 */
static int32_t Cl_HullForEntity(const EntityState *s) {

  const ClientEntity *ent = &cl.entities[s->number];

  switch (s->solid) {

    case SOLID_BOX: {
      if (s->effects & EF_CLIENT) {
        return Cm_SetBoxHull(ent->bounds, CONTENTS_MONSTER);
      } else {
        return Cm_SetBoxHull(ent->bounds, CONTENTS_SOLID);
      }
    }

    case SOLID_BSP: {
      const CmBspModel *mod = cl.cmModels[s->model1];
      if (!mod) {
        Com_Error(ERROR_DROP, "SOLID_BSP with no model\n");
      }
      return mod->headNode;
    }

    case SOLID_EDITOR: {
      if (s->effects & EF_WORLD) {
        return 0;
      } else {
        return Cm_SetBoxHull(ent->bounds, CONTENTS_EDITOR);
      }
    }

    default:
      return -1;
  }
}

/**
 * @brief Yields the contents mask (bitwise OR) for the specified point. The
 * world model and all solids are checked.
 */
int32_t Cl_PointContents(const Vec3 point) {

  int32_t contents = Cm_PointContents(point, 0, Mat4_Identity());

  for (int32_t i = 0; i < cl.frame.numEntities; i++) {

    const uint32_t snum = (cl.frame.entityState + i) & ENTITY_STATE_MASK;
    const EntityState *s = &cl.entityStates[snum];

    if (s->solid < SOLID_BOX) {
      continue;
    }

    const ClientEntity *ent = &cl.entities[s->number];

    if (ent == cl.entity) {
      continue;
    }

    const int32_t headNode = Cl_HullForEntity(s);

    contents |= Cm_PointContents(point, headNode, ent->inverseMatrix);
  }

  return contents;
}

/**
 * @return The contents mask of all leafs that span the specified bounds.
 */
int32_t Cl_BoxContents(const Box3 bounds) {

  int32_t contents = Cm_BoxContents(bounds, 0);

  for (int32_t i = 0; i < cl.frame.numEntities; i++) {

    const uint32_t snum = (cl.frame.entityState + i) & ENTITY_STATE_MASK;
    const EntityState *s = &cl.entityStates[snum];

    if (s->solid < SOLID_BOX) {
      continue;
    }

    const ClientEntity *ent = &cl.entities[s->number];

    if (ent == cl.entity) {
      continue;
    }

    if (!Box3_Intersects(bounds, ent->absBounds)) {
      continue;
    }

    const int32_t headNode = Cl_HullForEntity(s);

    contents |= Cm_BoxContents(Mat4_TransformBounds(ent->inverseMatrix, bounds), headNode);
  }

  return contents;
}

/**
 * @brief A structure facilitating clipping to `SOLID_BOX` entities.
 */
typedef struct {
  Vec3 start, end;
  Box3 bounds;
  Box3 absBounds;
  CmTrace trace;
  const ClientEntity *skip;
  int32_t contents;
} ClientTrace;

/**
 * @brief Clips the specified trace to the specified entity.
 * @return True if the trace began in solid, so that the caller may stop.
 */
static bool Cl_ClipTraceToEntity(ClientTrace *trace, ClientEntity *ent) {
  const EntityState *s = &ent->current;

  if (s->solid < SOLID_BOX) {
    return false;
  }

  if (ent == trace->skip) {
    return false;
  }

  if (ent == cl.entity) {
    return false;
  }

  if (!Box3_Intersects(ent->absBounds, trace->absBounds)) {
    return false;
  }

  if (cls.cgame->ClipEntity && !cls.cgame->ClipEntity(trace->skip, ent)) {
    return false;
  }

  const int32_t headNode = Cl_HullForEntity(s);

  CmTrace tr;

  if (Mat4_Equal(ent->matrix, Mat4_Identity())) {
    tr = Cm_BoxTrace(trace->start, trace->end, trace->bounds, headNode, trace->contents);
  } else {
    tr = Cm_TransformedBoxTrace(trace->start, trace->end, trace->bounds, headNode, trace->contents, ent->matrix, ent->inverseMatrix);
  }

  if (tr.startSolid || tr.fraction < trace->trace.fraction) {
    trace->trace = tr;
    trace->trace.ent = ent;
  }

  return tr.startSolid;
}

/**
 * @brief Clips the specified trace to other solid entities in the frame.
 */
static void Cl_ClipTraceToEntities(ClientTrace *trace) {

  for (int32_t i = 0; i < cl.frame.numEntities; i++) {

    const uint32_t snum = (cl.frame.entityState + i) & ENTITY_STATE_MASK;
    const EntityState *s = &cl.entityStates[snum];

    if (Cl_ClipTraceToEntity(trace, &cl.entities[s->number])) {
      return;
    }
  }
}

/**
 * @brief Client-side collision model tracing. This is the reciprocal of
 * `Sv_Trace`.
 *
 * @param skip An optional entity to skip.
 */
CmTrace Cl_Trace(const Vec3 start, const Vec3 end, const Box3 bounds, const ClientEntity *skip, int32_t contents) {

  ClientTrace trace = {
    .start = start,
    .end = end,
    .bounds = bounds,
    .absBounds = Cm_TraceBounds(start, end, bounds),
    .skip = skip,
    .contents = contents,
    .trace = {
      .fraction = 1.f,
      .end = end,
    }
  };

  Cl_ClipTraceToEntities(&trace);

  return trace.trace;
}

/**
 * @brief Entry point for client-side prediction. For each server frame, run
 * the player movement code with the user commands we've sent to the server
 * but have not yet received acknowledgment for. Store the resulting move so
 * that it may be interpolated into by `Cl_UpdateView`.
 *
 * Most of the work is passed off to the client game, which is responsible for
 * the implementation `Pm_Move`.
 */
void Cl_PredictMovement(void) {

  if (!cls.cgame->UsePrediction()) {
    return;
  }

  const uint32_t last = cls.netChan.outgoingSequence;
  uint32_t ack = cls.netChan.incomingAcknowledged;

  // if we are too far out of date, just freeze in place
  if (last - ack >= CMD_BACKUP) {
    Com_Debug(DEBUG_CLIENT, "Exceeded CMD_BACKUP\n");
    return;
  }

  Vector *cmds = $(alloc(Vector), initWithSize, sizeof(ClientCmd *));

  while (++ack <= last) {
    ClientCmd *cmd = &cl.cmds[ack & CMD_MASK];
    $(cmds, add, &cmd);
  }

  if (cmds->count) {
    cls.cgame->PredictMovement(cmds);
  }

  release(cmds);
}

/**
 * @brief Checks for client side prediction errors. These will occur under normal gameplay
 * conditions if the client is pushed by another entity on the server (projectile, platform, etc.).
 */
void Cl_CheckPredictionError(void) {

  const PlayerMoveState *in = &cl.frame.ps.pmState;

  ClientPredictedState *out = &cl.predictedState;

  // calculate the last ClientCmd we sent that the server has processed
  ClientCmd *cmd = &cl.cmds[cls.netChan.incomingAcknowledged & CMD_MASK];

  // if prediction was not run (just spawned), don't sweat it
  if (cmd->prediction.time == 0) {

    out->view.origin = in->origin;
    out->view.offset = in->viewOffset;
    out->view.angles = in->viewAngles;
    out->view.stepOffset = 0.f;

    out->error = Vec3_Zero();
    return;
  }

  // subtract what the server returned from our predicted origin for that frame
  out->error = cmd->prediction.error = Vec3_Subtract(cmd->prediction.origin, in->origin);

  // if the error is too large, it was likely a teleport or respawn, so ignore it
  const float len = Vec3_Length(out->error);
  if (len > .1f) {
    if (len > MAX_DELTA_ORIGIN) {
      Com_Debug(DEBUG_CLIENT, "MAX_DELTA_ORIGIN: %s\n", vtos(out->error));

      out->view.origin = in->origin;
      out->view.offset = in->viewOffset;
      out->view.angles = in->viewAngles;
      out->view.stepOffset = 0.f;

      out->error = Vec3_Zero();
    } else {
      Com_Debug(DEBUG_CLIENT, "%s\n", vtos(out->error));
    }
  }
}

/**
 * @brief Ensures client-side prediction has the current collision model at its
 * disposal.
 */
void Cl_UpdatePrediction(void) {

  // ensure the world model is loaded
  if (!Com_WasInit(QUETOO_SERVER) || cl.demoServer || !Cm_NumModels()) {

    Cm_LoadBspModel(cl.configStrings[CS_BSP], NULL);

    // prove it is the bsp the server loaded. The manifest can not answer this:
    // maps/<name>.mf is read from our own filesystem, so a locally consistent but
    // stale pair passes it. Only a hash the server computed is authoritative
    const char *expected = cl.configStrings[CS_BSP_HASH];
    if (*expected) {

      char hash[MAX_QPATH];
      if (!Cm_HashFile(cl.configStrings[CS_BSP], hash, sizeof(hash))) {
        Com_Error(ERROR_DROP, "Failed to hash %s\n", cl.configStrings[CS_BSP]);
      }

      if (q_strcmp(hash, expected)) {
        Com_Error(ERROR_DROP, "%s differs from server (%s, expected %s)\n",
                  cl.configStrings[CS_BSP], hash, expected);
      }
    }
  }

  // load the BSP models for prediction as well
  for (int32_t i = 0; i < MAX_MODELS; i++) {

    const char *s = cl.configStrings[CS_MODELS + i];
    if (*s == '*') {
      cl.cmModels[i] = Cm_Model(cl.configStrings[CS_MODELS + i]);
    } else {
      cl.cmModels[i] = NULL;
    }
  }
}
