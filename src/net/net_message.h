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

#pragma once

#include "net_types.h"

/**
 * @brief Delta compression flags for `PlayerMoveState`.
 */
#define PS_PM_CLIENT        (1 << 0)
#define PS_PM_ENTITY        (1 << 1)
#define PS_PM_TYPE          (1 << 2)
#define PS_PM_ORIGIN        (1 << 3)
#define PS_PM_VELOCITY      (1 << 4)
#define PS_PM_FLAGS         (1 << 5)
#define PS_PM_TIME          (1 << 6)
#define PS_PM_GRAVITY       (1 << 7)
#define PS_PM_MOVEMENT      (1 << 8)
#define PS_PM_VIEW_OFFSET   (1 << 9)
#define PS_PM_VIEW_ANGLES   (1 << 10)
#define PS_PM_DELTA_ANGLES  (1 << 11)
#define PS_PM_HOOK_POSITION (1 << 12)
#define PS_PM_HOOK_LENGTH   (1 << 13)
#define PS_PM_STEP_OFFSET   (1 << 14)
#define PS_PM_PARAMS        (1 << 15)

/**
 * @brief Delta compression flags for `user_cmd_t`.
 */
#define CMD_ANGLE1  0x01
#define CMD_ANGLE2  0x02
#define CMD_ANGLE3  0x04
#define CMD_FORWARD 0x08
#define CMD_RIGHT   0x10
#define CMD_UP      0x20
#define CMD_BUTTONS 0x40
#define CMD_MUZZLE  0x80

/**
 * @brief These flags indicate which fields in a given `EntityState` must be
 * written or read for delta compression from one snapshot to the next.
 */
#define U_ORIGIN      (1 << 0)
#define U_TERMINATION (1 << 1)
#define U_ANGLES      (1 << 2)
#define U_ANIMATIONS  (1 << 3)
#define U_EVENT       (1 << 4)
#define U_EFFECTS     (1 << 5)
#define U_TRAIL       (1 << 6)
#define U_MODELS      (1 << 7)
#define U_COLOR       (1 << 8)
#define U_CLIENT      (1 << 9)
#define U_SOUND       (1 << 10)
#define U_SOLID       (1 << 11)
#define U_BOUNDS      (1 << 12)
#define U_REMOVE      (1 << 13)
#define U_SPAWN_ID    (1 << 14)
#define U_STEP_OFFSET (1 << 15)

/**
 * @brief Message writing and reading facilities.
 */
void Net_WriteData(MemBuf *msg, const void *data, size_t len);
void Net_WriteChar(MemBuf *msg, int32_t c);
void Net_WriteByte(MemBuf *msg, int32_t c);
void Net_WriteShort(MemBuf *msg, int32_t c);
void Net_WriteLong(MemBuf *msg, int32_t c);
void Net_WriteString(MemBuf *msg, const char *s);
void Net_WriteFloat(MemBuf *msg, float f);
void Net_WritePosition(MemBuf *msg, const Vec3 pos);
void Net_WriteAngle(MemBuf *msg, float f);
void Net_WriteAngles(MemBuf *msg, const Vec3 angles);
void Net_WriteDir(MemBuf *msg, const Vec3 dir);
void Net_WriteBounds(MemBuf *msg, const Box3 bounds);
void Net_WriteDeltaMoveCmd(MemBuf *msg, const PlayerMoveCmd *from, const PlayerMoveCmd *to);
void Net_WriteDeltaPlayerState(MemBuf *msg, const PlayerState *from, const PlayerState *to);
void Net_WriteDeltaEntity(MemBuf *msg, const EntityState *from, const EntityState *to, bool force);

void Net_BeginReading(MemBuf *msg);
void Net_ReadData(MemBuf *msg, void *data, size_t len);
int32_t Net_ReadChar(MemBuf *msg);
int32_t Net_ReadByte(MemBuf *msg);
int32_t Net_ReadShort(MemBuf *msg);
int32_t Net_ReadLong(MemBuf *msg);
char *Net_ReadString(MemBuf *msg);
char *Net_ReadStringLine(MemBuf *msg);
float Net_ReadFloat(MemBuf *msg);
Vec3 Net_ReadPosition(MemBuf *msg);
float Net_ReadAngle(MemBuf *msg);
Vec3 Net_ReadAngles(MemBuf *msg);
Vec3 Net_ReadDir(MemBuf *msg);
Box3 Net_ReadBounds(MemBuf *msg);
void Net_ReadDeltaMoveCmd(MemBuf *msg, const PlayerMoveCmd *from, PlayerMoveCmd *to);
void Net_ReadDeltaPlayerState(MemBuf *msg, const PlayerState *from, PlayerState *to);
void Net_ReadDeltaEntity(MemBuf *msg, const EntityState *from, EntityState *to, int16_t number, uint16_t bits);
