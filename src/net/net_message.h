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
 * @brief Delta compression flags for pm_state_t.
 */
#define PS_PM_TYPE				(1 << 0)
#define PS_PM_ORIGIN			(1 << 1)
#define PS_PM_VELOCITY			(1 << 2)
#define PS_PM_FLAGS				(1 << 3)
#define PS_PM_TIME				(1 << 4)
#define PS_PM_GRAVITY			(1 << 5)
#define PS_PM_VIEW_OFFSET		(1 << 6)
#define PS_PM_VIEW_ANGLES		(1 << 7)
#define PS_PM_DELTA_ANGLES		(1 << 8)
#define PS_PM_HOOK_POSITION		(1 << 9)
#define PS_PM_HOOK_LENGTH		(1 << 10)
#define PS_PM_STEP_OFFSET		(1 << 11)

/**
 * @brief Delta compression flags for user_cmd_t.
 */
#define CMD_ANGLE1 				0x1
#define CMD_ANGLE2 				0x2
#define CMD_ANGLE3 				0x4
#define CMD_FORWARD				0x8
#define CMD_RIGHT				0x10
#define CMD_UP					0x20
#define CMD_BUTTONS				0x40

/**
 * @brief These flags indicate which fields in a given entity_state_t must be
 * written or read for delta compression from one snapshot to the next.
 */
#define U_ORIGIN				(1 << 0)
#define U_TERMINATION			(1 << 1)
#define U_ANGLES				(1 << 2)
#define U_ANIMATIONS			(1 << 3)
#define U_EVENT					(1 << 4)
#define U_EFFECTS				(1 << 5)
#define U_TRAIL					(1 << 6)
#define U_MODELS				(1 << 7)
#define U_COLOR					(1 << 8)
#define U_CLIENT				(1 << 9)
#define U_SOUND					(1 << 10)
#define U_SOLID					(1 << 11)
#define U_BOUNDS				(1 << 12)
#define U_REMOVE				(1 << 13)
#define U_SPAWN_ID              (1 << 14)
#define U_STEP_OFFSET           (1 << 15)

/**
 * @brief These flags indicate which fields a given sound packet will contain. Maximum 8 flags.
 */
#define S_ATTEN					(1 << 0) // Flag is unused now; kept for net protocol compatibility
#define S_ORIGIN				(1 << 1)
#define S_ENTITY				(1 << 2)
#define S_PITCH					(1 << 3)

/**
 * @brief Message writing and reading facilities.
 */
void Net_WriteData(mem_buf_t *msg, const void *data, size_t len);
void Net_WriteChar(mem_buf_t *msg, int32_t c);
void Net_WriteByte(mem_buf_t *msg, int32_t c);
void Net_WriteShort(mem_buf_t *msg, int32_t c);
void Net_WriteLong(mem_buf_t *msg, int32_t c);
void Net_WriteString(mem_buf_t *msg, const char *s);
void Net_WriteFloat(mem_buf_t *msg, float f);
void Net_WritePosition(mem_buf_t *msg, const vec3_t pos);
void Net_WriteAngle(mem_buf_t *msg, float f);
void Net_WriteAngles(mem_buf_t *msg, const vec3_t angles);
void Net_WriteDir(mem_buf_t *msg, const vec3_t dir);
void Net_WriteBounds(mem_buf_t *msg, const box3_t bounds);
void Net_WriteDeltaMoveCmd(mem_buf_t *msg, const pm_cmd_t *from, const pm_cmd_t *to);
void Net_WriteDeltaPlayerState(mem_buf_t *msg, const player_state_t *from, const player_state_t *to);
void Net_WriteDeltaEntity(mem_buf_t *msg, const entity_state_t *from, const entity_state_t *to, _Bool force);

void Net_BeginReading(mem_buf_t *msg);
void Net_ReadData(mem_buf_t *msg, void *data, size_t len);
int32_t Net_ReadChar(mem_buf_t *msg);
int32_t Net_ReadByte(mem_buf_t *msg);
int32_t Net_ReadShort(mem_buf_t *msg);
int32_t Net_ReadLong(mem_buf_t *msg);
char *Net_ReadString(mem_buf_t *msg);
char *Net_ReadStringLine(mem_buf_t *msg);
float Net_ReadFloat(mem_buf_t *msg);
vec3_t Net_ReadPosition(mem_buf_t *msg);
float Net_ReadAngle(mem_buf_t *msg);
vec3_t Net_ReadAngles(mem_buf_t *msg);
vec3_t Net_ReadDir(mem_buf_t *msg);
box3_t Net_ReadBounds(mem_buf_t *msg);
void Net_ReadDeltaMoveCmd(mem_buf_t *msg, const pm_cmd_t *from, pm_cmd_t *to);
void Net_ReadDeltaPlayerState(mem_buf_t *msg, const player_state_t *from, player_state_t *to);
void Net_ReadDeltaEntity(mem_buf_t *msg, const entity_state_t *from, entity_state_t *to,
                         uint16_t number, uint16_t bits);
