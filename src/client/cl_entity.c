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

#include "cl_local.h"

/**
 * @brief Parse the `PlayerState` for the current frame from the server, using delta
 * compression for all fields where possible.
 */
static void Cl_ParsePlayerState(const ClientFrame *deltaFrame, ClientFrame *frame) {
  static PlayerState null_state;

  if (deltaFrame && deltaFrame->valid) {
    Net_ReadDeltaPlayerState(&netMessage, &deltaFrame->ps, &frame->ps);
  } else {
    Net_ReadDeltaPlayerState(&netMessage, &null_state, &frame->ps);
  }

  if (cl.demoServer) { // if playing a demo, force freeze
    frame->ps.pmState.type = PM_FREEZE;
  }
}

/**
 * @return True if the delta is valid and the entity should be interpolated, false
 * if the delta is invalid and the entity should be snapped to `to`.
 */
static bool Cl_ValidDeltaEntity(const ClientEntity *ent,
                                 const EntityState *from, const EntityState *to) {

  if (!cl.previousFrame) {
    return false; // no continuous predecessor to interpolate from
  }

  if (ent->frameNum != cl.previousFrame->frameNum) {
    return false;
  }

  if (ent->current.spawnId != to->spawnId) {
    return false;
  }

  if (ent->current.model1 != to->model1) {
    return false;
  }

  if (Vec3_Distance(ent->current.origin, to->origin) > MAX_DELTA_ORIGIN) {
    return false;
  }

  return true;
}

/**
 * @brief Resets all trails to initial values
 * @param ent Entity to reset trails for
 */
static void Cl_ResetTrails(ClientEntity *ent) {

  for (Vec3 *trail = ent->trailOrigins;
       trail < ent->trailOrigins + lengthof(ent->trailOrigins);
       trail++) {
    *trail = ent->previousOrigin;
  }
}

/**
 * @brief Reads deltas from the given base and adds the resulting entity to the
 * current frame.
 */
static void Cl_ReadDeltaEntity(ClientFrame *frame, const EntityState *from, int16_t number, uint16_t bits) {

  ClientEntity *ent = &cl.entities[number];

  EntityState *to = &cl.entityStates[cl.entityState & ENTITY_STATE_MASK];
  cl.entityState++;

  frame->numEntities++;

  Net_ReadDeltaEntity(&netMessage, from, to, number, bits);

  // check to see if the delta was successful and valid
  if (!Cl_ValidDeltaEntity(ent, from, to)) {
    ent->prev = *to; // copy the current state to the previous
    ent->animation1.time = ent->animation2.time = 0;
    ent->animation1.frame = ent->animation2.frame = -1;
    ent->previousOrigin = to->origin;
    Cl_ResetTrails(ent);
    ent->legsCurrentYaw = to->angles.y;
  } else { // shuffle the last state to previous
    ent->prev = ent->current;
  }

  // set the current frame number and entity state
  ent->frameNum = cl.frame.frameNum;
  ent->current = *to;
}

/**
 * @brief An `svc_packetentities` has just been parsed, deal with the rest of the data stream.
 */
static void Cl_ParseEntities(const ClientFrame *deltaFrame, ClientFrame *frame) {

  frame->entityState = cl.entityState;
  frame->numEntities = 0;

  EntityState *from = NULL;
  int16_t fromNumber;

  if (deltaFrame == NULL || deltaFrame->numEntities == 0) {
    fromNumber = INT16_MAX;
  } else {
    from = &cl.entityStates[deltaFrame->entityState & ENTITY_STATE_MASK];
    fromNumber = from->number;
  }

  int32_t index = 0;

  /*
   * Parse entity updates from the server message, merging with the previous frame.
   * The server sends a sorted list of entity numbers with delta updates. We walk through
   * both the new message and the old frame in parallel by entity number:
   *  - If fromNumber < number: unchanged entity from old frame, copy it forward
   *  - If fromNumber == number: delta update, apply changes
   *  - If fromNumber > number: new entity, delta from baseline
   *  - If bits has U_REMOVE: entity removed, don't copy forward
   * The server terminates the list with -1. Using INT16_MAX as sentinel when the
   * old frame list is exhausted.
   */
  
  while (true) {

    const int16_t number = Net_ReadShort(&netMessage);

    if (number == -1) {
      break;
    }

    if (number < 0 || number >= MAX_ENTITIES) {
      Com_Error(ERROR_DROP, "Bad number: %i\n", number);
    }

    if (netMessage.read > netMessage.size) {
      Com_Error(ERROR_DROP, "End of message\n");
    }

    // before dealing with new entities, copy unchanged entities into the frame
    while (fromNumber < number) {

      if (cl_drawNetMessages->integer == 3) {
        Com_Print("   unchanged: %i\n", fromNumber);
      }

      Cl_ReadDeltaEntity(frame, from, fromNumber, 0);

      index++;

      if (index >= deltaFrame->numEntities) {
        fromNumber = INT16_MAX;
      } else {
        from = &cl.entityStates[(deltaFrame->entityState + index) & ENTITY_STATE_MASK];
        fromNumber = from->number;
      }
    }

    // now deal with the new entity
    const uint16_t bits = Net_ReadShort(&netMessage);

    if (bits & U_REMOVE) { // remove it, no delta

      if (cl_drawNetMessages->integer == 3) {
        Com_Print("   remove: %i\n", number);
      }

      if (fromNumber != number) {
        Com_Debug(DEBUG_CLIENT, "U_REMOVE: %u != %u\n", fromNumber, number);
      }

      index++;

      if (index >= deltaFrame->numEntities) {
        fromNumber = INT16_MAX;
      } else {
        from = &cl.entityStates[(deltaFrame->entityState + index) & ENTITY_STATE_MASK];
        fromNumber = from->number;
      }

      continue;
    }

    if (fromNumber == number) { // delta from previous state

      if (cl_drawNetMessages->integer == 3) {
        Com_Print("   delta: %i\n", number);
      }

      Cl_ReadDeltaEntity(frame, from, number, bits);

      index++;

      if (index >= deltaFrame->numEntities) {
        fromNumber = INT16_MAX;
      } else {
        from = &cl.entityStates[(deltaFrame->entityState + index) & ENTITY_STATE_MASK];
        fromNumber = from->number;
      }

      continue;
    }

    if (fromNumber > number) { // delta from baseline

      if (cl_drawNetMessages->integer == 3) {
        Com_Print("   baseline: %i\n", number);
      }

      Cl_ReadDeltaEntity(frame, &cl.entities[number].baseline, number, bits);

      continue;
    }
  }

  // any remaining entities in the old frame are copied over
  while (fromNumber != INT16_MAX) { // one or more entities from the old packet are unchanged

    if (cl_drawNetMessages->integer == 3) {
      Com_Print("   unchanged: %i\n", fromNumber);
    }

    Cl_ReadDeltaEntity(frame, from, fromNumber, 0);

    index++;

    if (index >= deltaFrame->numEntities) {
      fromNumber = INT16_MAX;
    } else {
      from = &cl.entityStates[(deltaFrame->entityState + index) & ENTITY_STATE_MASK];
      fromNumber = from->number;
    }
  }
}

/**
 * @brief Parses a new server frame, ensuring that the previously received frame is interpolated
 * before proceeding. This ensure that all server frames are processed, even if their simulation
 * result doesn't make it to the screen.
 */
void Cl_ParseFrame(void) {

  if (cl.frame.valid && !cl.frame.interpolated) {
    Cl_Interpolate();
  }

  memset(&cl.frame, 0, sizeof(cl.frame));

  cl.frame.frameNum = Net_ReadLong(&netMessage);
  cl.frame.deltaFrameNum = Net_ReadLong(&netMessage);

  if (cl_drawNetMessages->integer == 3) {
    Com_Print("   frame:%i delta:%i\n", cl.frame.frameNum, cl.frame.deltaFrameNum);
  }

  if (cl.frame.deltaFrameNum <= 0) { // uncompressed frame: entities decode from baseline
    cl.deltaFrame = NULL;
  } else { // delta compressed frame: entities decode from cl.deltaFrame
    cl.deltaFrame = &cl.frames[cl.frame.deltaFrameNum & PACKET_MASK];

    if (!cl.deltaFrame->valid) {
      Com_Error(ERROR_DROP, "Delta from invalid frame\n");
    } else if (cl.deltaFrame->frameNum != cl.frame.deltaFrameNum) {
      Com_Error(ERROR_DROP, "Delta frame too old\n");
    } else if (cl.entityState - cl.deltaFrame->entityState > ENTITY_STATE_BACKUP - PACKET_BACKUP) {
      Com_Error(ERROR_DROP, "Delta entity state too old\n");
    }
  }

  // cl.previousFrame tracks simple sequential continuity for interpolation purposes, independent
  // of whether this frame's entities were delta- or baseline-encoded: a demo's recorded frames are
  // always baseline-encoded (see Cl_WriteDemoMessage) but are still sequential and interpolatable,
  // so this must not be tied to cl.deltaFrame the way it once was.
  cl.previousFrame = &cl.frames[(cl.frame.frameNum - 1) & PACKET_MASK];

  if (cl.previousFrame->frameNum != (cl.frame.frameNum - 1)) {
    Com_Debug(DEBUG_CLIENT, "Previous frame too old\n");
    cl.previousFrame = NULL;
  } else if (!cl.previousFrame->valid) {
    Com_Debug(DEBUG_CLIENT, "Previous frame invalid\n");
    cl.previousFrame = NULL;
  }

  cl.frame.valid = true;

  Cl_ParsePlayerState(cl.deltaFrame, &cl.frame);

  Cl_ParseEntities(cl.deltaFrame, &cl.frame);

  // set the simulation time for the frame
  cl.frame.time = cl.frame.frameNum * QUETOO_TICK_MILLIS;

  // save the frame off in the backup array for later delta comparisons
  cl.frames[cl.frame.frameNum & PACKET_MASK] = cl.frame;

  if (cl.frame.valid) {

    // receiving a valid server frame means that loading completed on the
    // previous client frame; set the client to active and key dest to game
    if (cls.state == CL_LOADING) {
      cls.state = CL_ACTIVE;

      Cl_SetKeyDest(KEY_GAME);

      // a demo we are hosting comes up paused on this, its opening frame, with the transport
      // controls showing. Keyed on going active rather than on frameNum, which is 0 again after
      // a scrub back to the start, and confined to a local demo, because pause is server state
      // that a spectator has no business taking from everyone else on a demo server
      if (cl.demoServer && cls.netChan.remoteAddress.type == NA_LOOP) {
        Cbuf_AddText("demo_pause\n");
      }
    }

    Cl_CheckPredictionError();
  }
}

/**
 * @brief Updates the interpolation fraction for the current client frame.
 * Because the client often runs at a higher framerate than the server, we
 * use linear interpolation between the last 2 server frames. We aim to reach
 * the current server time just as a new packet arrives.
 *
 * @remarks The client advances its simulation time each frame, by the elapsed
 * millisecond delta. Here, we clamp the simulation time to be within the
 * range of the current frame. Even under ideal conditions, it's likely that
 * clamping will occur due to e.g. network jitter.
 */
static void Cl_UpdateLerp(void) {

  bool noLerp = cl.previousFrame == NULL || cl_noLerp->value || timeDemo->value;

  if (cl.previousFrame) {
    const float dist = Vec3_Distance(cl.frame.ps.pmState.origin, cl.previousFrame->ps.pmState.origin);
    if (dist > MAX_DELTA_ORIGIN) {
      Com_Debug(DEBUG_CLIENT, "MAX_ORIGIN_DELTA: %.2f\n", dist);
      noLerp = true;
    }
  }

  // the world's clock does not run while a demo is paused, so it can never climb into a frame
  // the transport controls step to. Show that frame outright: stepping asks for a specific
  // tick, and interpolating towards it from the one before is the opposite of what was asked
  if (noLerp || cls.demo.paused) {
    cl.time = cl.frame.time;
    cl.lerp = 1.0;
  } else {
    if (cl.time > cl.frame.time) {
      Com_Debug(DEBUG_CLIENT, "High clamp: %dms\n", cl.time - cl.frame.time);
      cl.time = cl.frame.time;
      cl.lerp = 1.0;
    } else if (cl.time < cl.frame.time - QUETOO_TICK_MILLIS) {
      Com_Debug(DEBUG_CLIENT, "Low clamp: %dms\n", (cl.frame.time - QUETOO_TICK_MILLIS) - cl.time);
      cl.time = cl.frame.time - QUETOO_TICK_MILLIS;
      cl.lerp = 0.0;
    } else {
      cl.lerp = 1.0 - (cl.frame.time - cl.time) / (float) QUETOO_TICK_MILLIS;
    }
  }
}

/**
 * @brief Interpolates the simulation for the most recently parsed server frame.
 * @remarks This can be called multiple times per frame, in the event that the client has received
 * multiple server updates at once. This happens somewhat frequently, especially at higher server
 * tick rates, or with significant network jitter.
 */
void Cl_Interpolate(void) {

  if (cls.state != CL_ACTIVE) {
    return;
  }

  if (!cl.frame.valid) {
    return;
  }

  Cl_UpdateLerp();

  for (int32_t i = 0; i < cl.frame.numEntities; i++) {

    const uint32_t s = (cl.frame.entityState + i) & ENTITY_STATE_MASK;
    ClientEntity *ent = &cl.entities[cl.entityStates[s].number];

    if (!Vec3_Equal(ent->prev.origin, ent->current.origin)) {
      ent->previousOrigin = ent->origin;
      ent->origin = Vec3_Mix(ent->prev.origin, ent->current.origin, cl.lerp);
    } else {
      ent->origin = ent->current.origin;
    }

    if (!Vec3_Equal(ent->prev.termination, ent->current.termination)) {
      ent->termination = Vec3_Mix(ent->prev.termination, ent->current.termination, cl.lerp);
    } else {
      ent->termination = ent->current.termination;
    }

    if (!Vec3_Equal(ent->prev.angles, ent->current.angles)) {
      ent->angles = Vec3_MixEuler(ent->prev.angles, ent->current.angles, cl.lerp);
    } else {
      ent->angles = ent->current.angles;
    }

    if (ent->current.animation1 != ent->prev.animation1 || !ent->animation1.time) {
      ent->animation1.animation = ent->current.animation1 & ANIM_MASK_VALUE;
      ent->animation1.time = cl.unclampedTime;
      ent->animation1.reverse = ent->current.animation1 & ANIM_REVERSE_BIT;
    }

    if (ent->current.animation2 != ent->prev.animation2 || !ent->animation2.time) {
      ent->animation2.animation = ent->current.animation2 & ANIM_MASK_VALUE;
      ent->animation2.time = cl.unclampedTime;
      ent->animation2.reverse = ent->current.animation2 & ANIM_REVERSE_BIT;
    }

    if (ent->prev.stepOffset != ent->current.stepOffset) {
      ent->stepOffset = Mixf(ent->prev.stepOffset, ent->current.stepOffset, cl.lerp);
    } else {
      ent->stepOffset = ent->current.stepOffset;
    }

    Vec3 angles;
    if (ent->current.solid == SOLID_BSP) {
      angles = ent->current.angles;

      const RenderModel *mod = cl.models[ent->current.model1];

      assert(mod);
      assert(mod->bspInline);

      ent->bounds = mod->bspInline->visibleBounds;
    } else {
      angles = Vec3_Zero();
      ent->bounds = ent->current.bounds;
    }

    // jdolan: Note that we use the latest snapshot state, not the interpolated state.
    // This is so client side prediction can work, and the client and server are working
    // with the same entity positions at their respective intervals. A side effect of this
    // is that client side traces are usually _ahead_ of the rendered simulation, so you'll
    // see artifacts like player shadows glitching on func_plats, etc.

    ent->matrix = Mat4_FromRotationTranslationScale(angles, ent->current.origin, 1.f);

    ent->inverseMatrix = Mat4_Inverse(ent->matrix);

    ent->absBounds = Cm_EntityBounds(ent->current.solid, ent->matrix, ent->bounds);
  }

  cls.cgame->Interpolate(&cl.frame);

  cl.frame.interpolated = true;
}
