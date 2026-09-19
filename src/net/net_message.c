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

#include "net_message.h"

typedef union {
  int32_t i;
  float v;
} net_float;

/**
 * @brief Appends raw bytes to a network message buffer.
 */
void Net_WriteData(MemBuf *msg, const void *data, size_t len) {
  Mem_WriteBuffer(msg, data, len);
}

/**
 * @brief Writes a signed 8-bit integer to a network message buffer.
 */
void Net_WriteChar(MemBuf *msg, int32_t c) {
  byte *buf;

  buf = Mem_AllocBuffer(msg, sizeof(char));
  buf[0] = c;
}

/**
 * @brief Writes an unsigned 8-bit integer to a network message buffer.
 */
void Net_WriteByte(MemBuf *msg, int32_t c) {
  byte *buf;

  buf = Mem_AllocBuffer(msg, sizeof(byte));
  buf[0] = c;
}

/**
 * @brief Writes a 16-bit little-endian integer to a network message buffer.
 */
void Net_WriteShort(MemBuf *msg, int32_t c) {
  byte *buf;

  buf = Mem_AllocBuffer(msg, sizeof(int16_t));
  buf[0] = c & 0xff;
  buf[1] = c >> 8;
}

/**
 * @brief Writes a 32-bit little-endian integer to a network message buffer.
 */
void Net_WriteLong(MemBuf *msg, int32_t c) {
  byte *buf;

  buf = Mem_AllocBuffer(msg, sizeof(int32_t));
  buf[0] = c & 0xff;
  buf[1] = (c >> 8) & 0xff;
  buf[2] = (c >> 16) & 0xff;
  buf[3] = c >> 24;
}

/**
 * @brief Writes a null-terminated string to a network message buffer.
 */
void Net_WriteString(MemBuf *msg, const char *s) {
  if (!s) {
    Mem_WriteBuffer(msg, "", 1);
  } else {
    Mem_WriteBuffer(msg, s, q_strlen(s) + 1);
  }
}

/**
 * @brief Writes a 32-bit float (as its raw integer bit pattern) to a network message buffer.
 */
void Net_WriteFloat(MemBuf *msg, float v) {

  const net_float vec = {
    .v = v
  };

  Net_WriteLong(msg, vec.i);
}

/**
 * @brief Writes a 3D world-space position as three consecutive floats to a network message buffer.
 */
void Net_WritePosition(MemBuf *msg, const Vec3 pos) {
  Net_WriteFloat(msg, pos.x);
  Net_WriteFloat(msg, pos.y);
  Net_WriteFloat(msg, pos.z);
}

/**
 * @brief Encodes an angle in degrees as a 16-bit integer and writes it to a network message buffer.
 */
void Net_WriteAngle(MemBuf *msg, float angle) {

  while (angle < 0.f) {
    angle += 360.f;
  }

  while (angle >= 360.f) {
    angle -= 360.f;
  }

  Net_WriteShort(msg, (uint16_t) ((angle / 360.0f) * UINT16_MAX));
}

/**
 * @brief Writes three Euler angles (pitch, yaw, roll) to a network message buffer.
 */
void Net_WriteAngles(MemBuf *msg, const Vec3 angles) {
  Net_WriteAngle(msg, angles.x);
  Net_WriteAngle(msg, angles.y);
  Net_WriteAngle(msg, angles.z);
}

/**
 * @brief Encodes a direction vector as the index of the closest approximate normal and writes it to a network message buffer.
 */
void Net_WriteDir(MemBuf *msg, const Vec3 dir) {
  int32_t i, best = 0;
  float bestD = 0.0;

  for (i = 0; i < NUM_APPROXIMATE_NORMALS; i++) {
    const float d = Vec3_Dot(dir, approximate_normals[i]);
    if (d > bestD) {
      bestD = d;
      best = i;
    }
  }

  Net_WriteByte(msg, best);
}

/**
 * @brief Writes an axis-aligned bounding box as six 16-bit integers (mins then maxs) to a network message buffer.
 */
void Net_WriteBounds(MemBuf *msg, const Box3 bounds) {

  const Vec3s _mins = Vec3_CastVec3s(bounds.mins);

  Net_WriteShort(msg, _mins.x);
  Net_WriteShort(msg, _mins.y);
  Net_WriteShort(msg, _mins.z);

  const Vec3s _maxs = Vec3_CastVec3s(bounds.maxs);

  Net_WriteShort(msg, _maxs.x);
  Net_WriteShort(msg, _maxs.y);
  Net_WriteShort(msg, _maxs.z);
}

/**
 * @brief Writes only the changed fields of a movement command as a delta from `from` to `to`.
 */
void Net_WriteDeltaMoveCmd(MemBuf *msg, const PlayerMoveCmd *from, const PlayerMoveCmd *to) {

  byte bits = 0;

  if (to->angles.x != from->angles.x) {
    bits |= CMD_ANGLE1;
  }
  if (to->angles.y != from->angles.y) {
    bits |= CMD_ANGLE2;
  }
  if (to->angles.z != from->angles.z) {
    bits |= CMD_ANGLE3;
  }

  if (to->forward != from->forward) {
    bits |= CMD_FORWARD;
  }
  if (to->right != from->right) {
    bits |= CMD_RIGHT;
  }
  if (to->up != from->up) {
    bits |= CMD_UP;
  }

  if (to->buttons != from->buttons) {
    bits |= CMD_BUTTONS;
  }

  if (!Vec3_Equal(to->muzzle, from->muzzle)) {
    bits |= CMD_MUZZLE;
  }

  Net_WriteByte(msg, bits);

  if (bits & CMD_ANGLE1) {
    Net_WriteAngle(msg, to->angles.x);
  }
  if (bits & CMD_ANGLE2) {
    Net_WriteAngle(msg, to->angles.y);
  }
  if (bits & CMD_ANGLE3) {
    Net_WriteAngle(msg, to->angles.z);
  }

  if (bits & CMD_FORWARD) {
    Net_WriteShort(msg, to->forward);
  }
  if (bits & CMD_RIGHT) {
    Net_WriteShort(msg, to->right);
  }
  if (bits & CMD_UP) {
    Net_WriteShort(msg, to->up);
  }

  if (bits & CMD_BUTTONS) {
    Net_WriteByte(msg, to->buttons);
  }

  if (bits & CMD_MUZZLE) {
    Net_WriteChar(msg, (int8_t) to->muzzle.x);
    Net_WriteChar(msg, (int8_t) to->muzzle.y);
    Net_WriteChar(msg, (int8_t) to->muzzle.z);
  }

  Net_WriteByte(msg, to->msec);
}

/**
 * @brief Writes only the changed fields of a player state as a delta from `from` to `to`.
 */
void Net_WriteDeltaPlayerState(MemBuf *msg, const PlayerState *from, const PlayerState *to) {

  uint32_t bits = 0;

  if (to->client != from->client) {
    bits |= PS_PM_CLIENT;
  }

  if (to->entity != from->entity) {
    bits |= PS_PM_ENTITY;
  }

  if (to->pmState.type != from->pmState.type) {
    bits |= PS_PM_TYPE;
  }

  if (!Vec3_Equal(to->pmState.origin, from->pmState.origin)) {
    bits |= PS_PM_ORIGIN;
  }

  if (!Vec3_Equal(to->pmState.velocity, from->pmState.velocity)) {
    bits |= PS_PM_VELOCITY;
  }

  if (to->pmState.flags != from->pmState.flags) {
    bits |= PS_PM_FLAGS;
  }

  if (to->pmState.time != from->pmState.time) {
    bits |= PS_PM_TIME;
  }

  if (to->pmState.params.gravity != from->pmState.params.gravity) {
    bits |= PS_PM_GRAVITY;
  }

  if (to->pmState.params.movement != from->pmState.params.movement) {
    bits |= PS_PM_MOVEMENT;
  }

  if (!Vec3_Equal(to->pmState.viewOffset, from->pmState.viewOffset)) {
    bits |= PS_PM_VIEW_OFFSET;
  }

  if (!Vec3_Equal(to->pmState.viewAngles, from->pmState.viewAngles)) {
    bits |= PS_PM_VIEW_ANGLES;
  }

  if (!Vec3_Equal(to->pmState.deltaAngles, from->pmState.deltaAngles)) {
    bits |= PS_PM_DELTA_ANGLES;
  }

  if (!Vec3_Equal(to->pmState.hookPosition, from->pmState.hookPosition)) {
    bits |= PS_PM_HOOK_POSITION;
  }

  if (to->pmState.hookLength != from->pmState.hookLength) {
    bits |= PS_PM_HOOK_LENGTH;
  }

  if (to->pmState.stepOffset != from->pmState.stepOffset) {
    bits |= PS_PM_STEP_OFFSET;
  }

  if (memcmp(&to->pmState.params.accelGround, &from->pmState.params.accelGround,
             sizeof(PlayerMoveParams) - offsetof(PlayerMoveParams, accelGround)) != 0) {
    bits |= PS_PM_PARAMS;
  }

  Net_WriteLong(msg, bits);

  if (bits & PS_PM_CLIENT) {
    Net_WriteByte(msg, to->client);
  }

  if (bits & PS_PM_ENTITY) {
    Net_WriteShort(msg, to->entity);
  }

  if (bits & PS_PM_TYPE) {
    Net_WriteByte(msg, to->pmState.type);
  }

  if (bits & PS_PM_ORIGIN) {
    Net_WritePosition(msg, to->pmState.origin);
  }

  if (bits & PS_PM_VELOCITY) {
    Net_WritePosition(msg, to->pmState.velocity);
  }

  if (bits & PS_PM_FLAGS) {
    Net_WriteShort(msg, to->pmState.flags);
  }

  if (bits & PS_PM_TIME) {
    Net_WriteShort(msg, to->pmState.time);
  }

  if (bits & PS_PM_GRAVITY) {
    Net_WriteShort(msg, to->pmState.params.gravity);
  }

  if (bits & PS_PM_MOVEMENT) {
    Net_WriteByte(msg, to->pmState.params.movement);
  }

  if (bits & PS_PM_VIEW_OFFSET) {
    Net_WritePosition(msg, to->pmState.viewOffset);
  }

  if (bits & PS_PM_VIEW_ANGLES) {
    Net_WriteAngles(msg, to->pmState.viewAngles);
  }

  if (bits & PS_PM_DELTA_ANGLES) {
    Net_WriteAngles(msg, to->pmState.deltaAngles);
  }

  if (bits & PS_PM_HOOK_POSITION) {
    Net_WritePosition(msg, to->pmState.hookPosition);
  }

  if (bits & PS_PM_HOOK_LENGTH) {
    Net_WriteShort(msg, to->pmState.hookLength);
  }

  if (bits & PS_PM_STEP_OFFSET) {
    Net_WriteFloat(msg, to->pmState.stepOffset);
  }

  if (bits & PS_PM_PARAMS) {
    float params[PM_PARAMS_FLOATS];
    memcpy(params, &to->pmState.params.accelGround, sizeof(params));

    for (size_t i = 0; i < PM_PARAMS_FLOATS; i++) {
      Net_WriteFloat(msg, params[i]);
    }
  }

  uint32_t statBits = 0;

  for (int32_t i = 0; i < MAX_STATS; i++) {
    if (to->stats[i] != from->stats[i]) {
      statBits |= 1 << i;
    }
  }

  Net_WriteLong(msg, statBits);

  for (int32_t i = 0; i < MAX_STATS; i++) {
    if (statBits & (1U << i)) {
      Net_WriteShort(msg, to->stats[i]);
    }
  }

  uint64_t invBits = 0;

  for (int32_t i = 0; i < MAX_INVENTORY; i++) {
    if (to->inventory[i] != from->inventory[i]) {
      invBits |= (uint64_t) 1 << i;
    }
  }

  Net_WriteLong(msg, (int32_t) (invBits & 0xFFFFFFFF));
  Net_WriteLong(msg, (int32_t) (invBits >> 32));

  for (int32_t i = 0; i < MAX_INVENTORY; i++) {
    if (invBits & ((uint64_t) 1 << i)) {
      Net_WriteShort(msg, to->inventory[i]);
    }
  }
}

/**
 * @brief Writes an entity's state changes to a net message. Can delta from a baseline or a previous state.
 */
void Net_WriteDeltaEntity(MemBuf *msg, const EntityState *from, const EntityState *to, bool force) {

  uint16_t bits = 0;

  if (to->number >= MAX_ENTITIES) {
    Com_Error(ERROR_FATAL, "Entity number >= MAX_ENTITIES\n");
  }

  if (to->stepOffset != from->stepOffset) {
    bits |= U_STEP_OFFSET;
  }

  if (to->spawnId != from->spawnId) {
    bits |= U_SPAWN_ID;
  }

  if (!Vec3_Equal(to->origin, from->origin)) {
    bits |= U_ORIGIN;
  }

  if (!Vec3_Equal(from->termination, to->termination)) {
    bits |= U_TERMINATION;
  }

  if (!Vec3_Equal(to->angles, from->angles)) {
    bits |= U_ANGLES;
  }

  if (to->animation1 != from->animation1 || to->animation2 != from->animation2) {
    bits |= U_ANIMATIONS;
  }

  if (to->event) { // event is not delta compressed, just 0 compressed
    bits |= U_EVENT;
  }

  if (to->effects != from->effects) {
    bits |= U_EFFECTS;
  }

  if (to->trail != from->trail) {
    bits |= U_TRAIL;
  }

  if (to->model1 != from->model1 || to->model2 != from->model2 ||
          to->model3 != from->model3 || to->model4 != from->model4) {
    bits |= U_MODELS;
  }

  if (to->color.rgba != from->color.rgba) {
    bits |= U_COLOR;
  }

  if (to->client != from->client) {
    bits |= U_CLIENT;
  }

  if (to->sound != from->sound) {
    bits |= U_SOUND;
  }

  if (to->solid != from->solid) {
    bits |= U_SOLID;
  }

  if (!Box3_Equal(to->bounds, from->bounds)) {
    bits |= U_BOUNDS;
  }

  if (!bits && !force) {
    return; // nothing to send
  }

  // write the message

  Net_WriteShort(msg, to->number);
  Net_WriteShort(msg, bits);

  if (bits & U_STEP_OFFSET) {
    Net_WriteByte(msg, to->stepOffset);
  }

  if (bits & U_SPAWN_ID) {
    Net_WriteByte(msg, to->spawnId);
  }

  if (bits & U_ORIGIN) {
    Net_WritePosition(msg, to->origin);
  }

  if (bits & U_TERMINATION) {
    Net_WritePosition(msg, to->termination);
  }

  if (bits & U_ANGLES) {
    Net_WriteAngles(msg, to->angles);
  }

  if (bits & U_ANIMATIONS) {
    Net_WriteByte(msg, to->animation1);
    Net_WriteByte(msg, to->animation2);
  }

  if (bits & U_EVENT) {
    Net_WriteByte(msg, to->event);
    Net_WriteByte(msg, to->eventData);
  }

  if (bits & U_EFFECTS) {
    Net_WriteLong(msg, (int32_t) to->effects);
  }

  if (bits & U_TRAIL) {
    Net_WriteByte(msg, to->trail);
  }

  if (bits & U_MODELS) {
    Net_WriteByte(msg, to->model1);
    Net_WriteByte(msg, to->model2);
    Net_WriteByte(msg, to->model3);
    Net_WriteByte(msg, to->model4);
  }

  if (bits & U_COLOR) {
    Net_WriteByte(msg, to->color.r);
    Net_WriteByte(msg, to->color.g);
    Net_WriteByte(msg, to->color.b);
    Net_WriteByte(msg, to->color.a);
  }

  if (bits & U_CLIENT) {
    Net_WriteByte(msg, to->client);
  }

  if (bits & U_SOUND) {
    Net_WriteByte(msg, to->sound);
  }

  if (bits & U_SOLID) {
    Net_WriteByte(msg, to->solid);
  }

  if (bits & U_BOUNDS) {
    Net_WriteBounds(msg, to->bounds);
  }
}

/**
 * @brief Resets the read cursor of a message buffer to the beginning.
 */
void Net_BeginReading(MemBuf *msg) {
  msg->read = 0;
}

/**
 * @brief Reads `len` raw bytes from the network message buffer into `data`.
 */
void Net_ReadData(MemBuf *msg, void *data, size_t len) {
  size_t i;

  for (i = 0; i < len; i++) {
    ((byte *) data)[i] = Net_ReadByte(msg);
  }
}

/**
 * @brief Returns -1 if no more characters are available.
 */
int32_t Net_ReadChar(MemBuf *msg) {
  int32_t c;

  if (msg->read + 1 > msg->size) {
    c = -1;
  } else {
    c = (signed char) msg->data[msg->read];
  }
  msg->read++;

  return c;
}

/**
 * @brief Reads an unsigned byte from a network message buffer; returns -1 on underflow.
 */
int32_t Net_ReadByte(MemBuf *msg) {
  int32_t c;

  if (msg->read + 1 > msg->size) {
    c = -1;
  } else {
    c = (byte) msg->data[msg->read];
  }
  msg->read++;

  return c;
}

/**
 * @brief Reads a 16-bit little-endian signed integer from a network message buffer; returns -1 on underflow.
 */
int32_t Net_ReadShort(MemBuf *msg) {
  int32_t c;

  if (msg->read + 2 > msg->size) {
    c = -1;
  } else {
    c = (int16_t) (msg->data[msg->read] + (msg->data[msg->read + 1] << 8));
  }

  msg->read += 2;

  return c;
}

/**
 * @brief Reads a 32-bit little-endian integer from a network message buffer; returns -1 on underflow.
 */
int32_t Net_ReadLong(MemBuf *msg) {
  uint32_t c;

  if (msg->read + 4 > msg->size) {
    c = -1;
  } else
    c = ((uint32_t) msg->data[msg->read + 0] << 0)
      + ((uint32_t) msg->data[msg->read + 1] << 8)
      + ((uint32_t) msg->data[msg->read + 2] << 16)
      + ((uint32_t) msg->data[msg->read + 3] << 24);

  msg->read += 4;

  return (int32_t) c;
}

/**
 * @brief Reads a null-terminated string from a network message buffer into a static buffer.
 * @remarks Uses a static buffer; not reentrant.
 */
char *Net_ReadString(MemBuf *msg) {
  static char string[MAX_STRING_CHARS];

  size_t l = 0;
  do {
    const int32_t c = Net_ReadChar(msg);
    if (c == -1 || c == 0) {
      break;
    }
    string[l] = c;
    l++;
  } while (l < sizeof(string) - 1);

  string[l] = '\0';

  return string;
}

/**
 * @brief Reads a newline- or null-terminated string from a network message buffer into a static buffer.
 * @remarks Uses a static buffer; not reentrant.
 */
char *Net_ReadStringLine(MemBuf *msg) {
  static char string[MAX_STRING_CHARS];

  size_t l = 0;
  do {
    const int32_t c = Net_ReadChar(msg);
    if (c == -1 || c == 0 || c == '\n') {
      break;
    }
    string[l] = c;
    l++;
  } while (l < sizeof(string) - 1);

  string[l] = '\0';

  return string;
}

/**
 * @brief Reads a 32-bit float (stored as a raw integer bit pattern) from a network message buffer.
 */
float Net_ReadFloat(MemBuf *msg) {

  const net_float vec = {
    .i = Net_ReadLong(msg)
  };

  return vec.v;
}

/**
 * @brief Reads a 3D world-space position from three consecutive floats in a network message buffer.
 */
Vec3 Net_ReadPosition(MemBuf *msg) {
  return (Vec3) {
    .x = Net_ReadFloat(msg),
    .y = Net_ReadFloat(msg),
    .z = Net_ReadFloat(msg)
  };
}

/**
 * @brief Reads a 16-bit encoded angle and converts it to degrees.
 */
float Net_ReadAngle(MemBuf *msg) {
  return Net_ReadShort(msg) * 360.f / UINT16_MAX;
}

/**
 * @brief Reads three Euler angles (pitch, yaw, roll) from a network message buffer.
 */
Vec3 Net_ReadAngles(MemBuf *msg) {
  return (Vec3) {
    .x = Net_ReadAngle(msg),
    .y = Net_ReadAngle(msg),
    .z = Net_ReadAngle(msg)
  };
}

/**
 * @brief Reads a direction byte index and returns the corresponding approximate normal vector.
 */
Vec3 Net_ReadDir(MemBuf *msg) {

  const int32_t b = Net_ReadByte(msg);

  if (b < 0 || b >= NUM_APPROXIMATE_NORMALS) {
    Com_Error(ERROR_DROP, "%d out of range\n", b);
  }

  return approximate_normals[b];
}

/**
 * @brief Reads an axis-aligned bounding box (mins then maxs) from a network message buffer.
 */
Box3 Net_ReadBounds(MemBuf *msg) {
  Box3 b;

  b.mins.x = Net_ReadShort(msg);
  b.mins.y = Net_ReadShort(msg);
  b.mins.z = Net_ReadShort(msg);
  b.maxs.x = Net_ReadShort(msg);
  b.maxs.y = Net_ReadShort(msg);
  b.maxs.z = Net_ReadShort(msg);

  return b;
}

/**
 * @brief Reads delta-compressed movement command fields into `to`, starting from the baseline in `from`.
 */
void Net_ReadDeltaMoveCmd(MemBuf *msg, const PlayerMoveCmd *from, PlayerMoveCmd *to) {

  *to = *from;

  const uint8_t bits = Net_ReadByte(msg);

  if (bits & CMD_ANGLE1) {
    to->angles.x = Net_ReadAngle(msg);
  }
  if (bits & CMD_ANGLE2) {
    to->angles.y = Net_ReadAngle(msg);
  }
  if (bits & CMD_ANGLE3) {
    to->angles.z = Net_ReadAngle(msg);
  }

  if (bits & CMD_FORWARD) {
    to->forward = Net_ReadShort(msg);
  }
  if (bits & CMD_RIGHT) {
    to->right = Net_ReadShort(msg);
  }
  if (bits & CMD_UP) {
    to->up = Net_ReadShort(msg);
  }

  if (bits & CMD_BUTTONS) {
    to->buttons = Net_ReadByte(msg);
  }

  if (bits & CMD_MUZZLE) {
    to->muzzle.x = Net_ReadChar(msg);
    to->muzzle.y = Net_ReadChar(msg);
    to->muzzle.z = Net_ReadChar(msg);
  }

  to->msec = Net_ReadByte(msg);
}

/**
 * @brief Reads delta-compressed player state fields into `to`, starting from the baseline in `from`.
 */
void Net_ReadDeltaPlayerState(MemBuf *msg, const PlayerState *from, PlayerState *to) {

  *to = *from;

  const uint32_t bits = Net_ReadLong(msg);

  if (bits & PS_PM_CLIENT) {
    to->client = Net_ReadByte(msg);
  }

  if (bits & PS_PM_ENTITY) {
    to->entity = Net_ReadShort(msg);
  }

  if (bits & PS_PM_TYPE) {
    to->pmState.type = Net_ReadByte(msg);
  }

  if (bits & PS_PM_ORIGIN) {
    to->pmState.origin = Net_ReadPosition(msg);
  }

  if (bits & PS_PM_VELOCITY) {
    to->pmState.velocity = Net_ReadPosition(msg);
  }

  if (bits & PS_PM_FLAGS) {
    to->pmState.flags = Net_ReadShort(msg);
  }

  if (bits & PS_PM_TIME) {
    to->pmState.time = Net_ReadShort(msg);
  }

  if (bits & PS_PM_GRAVITY) {
    to->pmState.params.gravity = Net_ReadShort(msg);
  }

  if (bits & PS_PM_MOVEMENT) {
    to->pmState.params.movement = Net_ReadByte(msg);
  }

  if (bits & PS_PM_VIEW_OFFSET) {
    to->pmState.viewOffset = Net_ReadPosition(msg);
  }

  if (bits & PS_PM_VIEW_ANGLES) {
    to->pmState.viewAngles = Net_ReadAngles(msg);
  }

  if (bits & PS_PM_DELTA_ANGLES) {
    to->pmState.deltaAngles = Net_ReadAngles(msg);
  }

  if (bits & PS_PM_HOOK_POSITION) {
    to->pmState.hookPosition = Net_ReadPosition(msg);
  }

  if (bits & PS_PM_HOOK_LENGTH) {
    to->pmState.hookLength = Net_ReadShort(msg);
  }

  if (bits & PS_PM_STEP_OFFSET) {
    to->pmState.stepOffset = Net_ReadFloat(msg);
  }

  if (bits & PS_PM_PARAMS) {
    float params[PM_PARAMS_FLOATS];

    for (size_t i = 0; i < PM_PARAMS_FLOATS; i++) {
      params[i] = Net_ReadFloat(msg);
    }

    memcpy(&to->pmState.params.accelGround, params, sizeof(params));
  }

  const int32_t statBits = Net_ReadLong(msg);

  for (int32_t i = 0; i < MAX_STATS; i++) {
    if (statBits & (1U << i)) {
      to->stats[i] = Net_ReadShort(msg);
    }
  }

  const uint64_t invBits = (uint64_t) (uint32_t) Net_ReadLong(msg) |
                            ((uint64_t) (uint32_t) Net_ReadLong(msg) << 32);

  for (int32_t i = 0; i < MAX_INVENTORY; i++) {
    if (invBits & ((uint64_t) 1 << i)) {
      to->inventory[i] = Net_ReadShort(msg);
    }
  }
}

/**
 * @brief Reads delta-compressed entity state fields into `to`, starting from the baseline in `from`.
 */
void Net_ReadDeltaEntity(MemBuf *msg, const EntityState *from, EntityState *to,
                         int16_t number, uint16_t bits) {

  *to = *from;

  to->number = number;

  if (bits & U_STEP_OFFSET) {
    to->stepOffset = Net_ReadByte(msg);
  }

  if (bits & U_SPAWN_ID) {
    to->spawnId = Net_ReadByte(msg);
  }

  if (bits & U_ORIGIN) {
    to->origin = Net_ReadPosition(msg);
  }

  if (bits & U_TERMINATION) {
    to->termination = Net_ReadPosition(msg);
  }

  if (bits & U_ANGLES) {
    to->angles = Net_ReadAngles(msg);
  }

  if (bits & U_ANIMATIONS) {
    to->animation1 = Net_ReadByte(msg);
    to->animation2 = Net_ReadByte(msg);
  }

  if (bits & U_EVENT) {
    to->event = Net_ReadByte(msg);
    to->eventData = Net_ReadByte(msg);
  } else {
    to->event = 0;
    to->eventData = 0;
  }

  if (bits & U_EFFECTS) {
    to->effects = (uint32_t) Net_ReadLong(msg);
  }

  if (bits & U_TRAIL) {
    to->trail = Net_ReadByte(msg);
  }

  if (bits & U_MODELS) {
    to->model1 = Net_ReadByte(msg);
    to->model2 = Net_ReadByte(msg);
    to->model3 = Net_ReadByte(msg);
    to->model4 = Net_ReadByte(msg);
  }

  if (bits & U_COLOR) {
    to->color.r = Net_ReadByte(msg);
    to->color.g = Net_ReadByte(msg);
    to->color.b = Net_ReadByte(msg);
    to->color.a = Net_ReadByte(msg);
  }

  if (bits & U_CLIENT) {
    to->client = Net_ReadByte(msg);
  }

  if (bits & U_SOUND) {
    to->sound = Net_ReadByte(msg);
  }

  if (bits & U_SOLID) {
    to->solid = Net_ReadByte(msg);
  }

  if (bits & U_BOUNDS) {
    to->bounds = Net_ReadBounds(msg);
  }
}
