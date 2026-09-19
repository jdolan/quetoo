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

#include "sv_local.h"

/**
 * @brief Writes a delta update of an `EntityState` list to the message.
 */
static void Sv_WriteEntities(ServerClientFrame *from, ServerClientFrame *to, MemBuf *msg) {
  EntityState *oldState = NULL, *newState = NULL;
  int32_t oldIndex, newIndex;
  int16_t oldNum, newNum;
  int16_t fromNumEntities;

  if (!from) {
    fromNumEntities = 0;
  } else {
    fromNumEntities = from->numEntities;
  }

  /*
   * Merge-sort the old and new entity lists, writing delta updates to the message.
   * Both lists are sorted by entity number, so we walk through them in parallel:
   *  - If entity numbers match: send delta from old to new state
   *  - If new_num < old_num: entity is new, send from baseline
   *  - If new_num > old_num: entity was removed, send removal notice
   * Using INT16_MAX as sentinel when we reach the end of either list.
   */
  
  newIndex = 0;
  oldIndex = 0;
  while (newIndex < to->numEntities || oldIndex < fromNumEntities) {
    if (newIndex >= to->numEntities) {
      newNum = INT16_MAX;
    } else {
      newState = &svs.entityStates[(to->entityState + newIndex) % svs.numEntityStates];
      newNum = newState->number;
    }

    if (oldIndex >= fromNumEntities) {
      oldNum = INT16_MAX;
    } else {
      oldState = &svs.entityStates[(from->entityState + oldIndex) % svs.numEntityStates];
      oldNum = oldState->number;
    }

    if (newNum == oldNum) { // delta update from old position
      Net_WriteDeltaEntity(msg, oldState, newState, false);
      oldIndex++;
      newIndex++;
      continue;
    }

    if (newNum < oldNum) { // this is a new entity, send it from the baseline
      Net_WriteDeltaEntity(msg, &sv.entities[newNum].baseline, newState, true);
      newIndex++;
      continue;
    }

    if (newNum > oldNum) { // the old entity isn't present in the new message
      const int16_t bits = U_REMOVE;

      Net_WriteShort(msg, oldNum);
      Net_WriteShort(msg, bits);

      oldIndex++;
      continue;
    }
  }

  Net_WriteShort(msg, -1); // end of entities
}

/**
 * @brief Writes a delta-compressed player state to the message buffer.
 */
static void Sv_WritePlayerState(ServerClientFrame *from, ServerClientFrame *to, MemBuf *msg) {
  static PlayerState null_state;

  if (from) {
    Net_WriteDeltaPlayerState(msg, &from->ps, &to->ps);
  } else {
    Net_WriteDeltaPlayerState(msg, &null_state, &to->ps);
  }
}

/**
 * @brief Assembles and writes a complete client frame to the message buffer.
 */
void Sv_WriteClientFrame(ServerClient *client, MemBuf *msg) {
  ServerClientFrame *frame, *deltaFrame;
  int32_t deltaFrameNum;

  // this is the frame we are creating
  frame = &client->frames[sv.frameNum & PACKET_MASK];

  if (client->lastFrame < 0) {
    // client is asking for a retransmit
    deltaFrame = NULL;
    deltaFrameNum = -1;
  } else if (sv.frameNum - client->lastFrame >= (PACKET_BACKUP - 3)) {
    // client hasn't gotten a good message through in a long time
    deltaFrame = NULL;
    deltaFrameNum = -1;
  } else {
    // we have a valid message to delta from
    deltaFrame = &client->frames[client->lastFrame & PACKET_MASK];
    deltaFrameNum = client->lastFrame;
  }

  Net_WriteByte(msg, SV_CMD_FRAME);
  Net_WriteLong(msg, sv.frameNum);
  Net_WriteLong(msg, deltaFrameNum); // what we are delta'ing from

  // delta encode the player state
  Sv_WritePlayerState(deltaFrame, frame, msg);

  // delta encode the entities
  Sv_WriteEntities(deltaFrame, frame, msg);
}

/**
 * @brief Decides which entities are going to be visible to the client and copies off the player state.
 */
void Sv_BuildClientFrame(ServerClient *client) {

  GameClient *cl = client->gclient;

  if (!cl->inUse) {
    return; // not in game yet
  }

  // this is the frame we are creating
  ServerClientFrame *frame = &client->frames[sv.frameNum & PACKET_MASK];
  frame->sentTime = quetoo.ticks; // timestamp for ping calculation

  // grab the current PlayerState
  frame->ps = cl->ps;

  // build up the list of relevant entities
  frame->numEntities = 0;
  frame->entityState = svs.nextEntityState;

  for (int32_t i = 0; i < sv_max_entities->integer; i++) {

    const GameEntity *ent = sv.entities[i].gent;

    if (!ent->inUse) {
      continue;
    }

    assert(ent->s.number == i);

    if (!editor->value) {

      // ignore entities that are local to the server, except for the
      // client's own entity which must always be up-to-date
      if ((ent->svFlags & SVF_NO_CLIENT) && ent->s.number != cl->ps.entity) {
        continue;
      }

      // ignore entities without visible presence
      if (!ent->s.event && !ent->s.effects && !ent->s.trail && !ent->s.model1 && !ent->s.sound) {
        continue;
      }
    }

    // copy it to the circular EntityState array
    EntityState *s = &svs.entityStates[svs.nextEntityState % svs.numEntityStates];

    *s = ent->s;

    // make the client's own projectiles as not-solid for client side prediction
    if (ent->owner == cl->entity) {
      s->solid = SOLID_NOT;
    }

    svs.nextEntityState++;
    frame->numEntities++;
  }
}
