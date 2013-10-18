/*
 * Copyright(c) 1997-2001 Id Software, Inc.
 * Copyright(c) 2002 The Quakeforge Project.
 * Copyright(c) 2006 Quake2World.
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

#ifndef __NET_MESSAGE_H__
#define __NET_MESSAGE_H__

#include "common.h"

/*
 * @brief Delta compression flags for pm_state_t.
 */
#define PS_PM_TYPE				0x1
#define PS_PM_ORIGIN			0x2
#define PS_PM_VELOCITY			0x4
#define PS_PM_FLAGS				0x8
#define PS_PM_TIME				0x10
#define PS_PM_GRAVITY			0x20
#define PS_PM_VIEW_OFFSET		0x40
#define PS_PM_VIEW_ANGLES		0x80
#define PS_PM_KICK_ANGLES		0x100
#define PS_PM_DELTA_ANGLES		0x200

/*
 * @brief Delta compression flags for user_cmd_t.
 */
#define CMD_ANGLE1 				0x1
#define CMD_ANGLE2 				0x2
#define CMD_ANGLE3 				0x4
#define CMD_FORWARD				0x8
#define CMD_RIGHT				0x10
#define CMD_UP					0x20
#define CMD_BUTTONS				0x40

// a sound without an entity or position will be a local only sound
#define S_ATTEN					0x1 // a byte
#define S_ORIGIN				0x2 // three coordinates
#define S_ENTNUM				0x4 // entity number
//

// entity_state_t communication

// This bit mask is packed into a uint16_t for each entity_state_t per frame.
// It describes which fields must be read to successfully parse the delta-
// compression.
#define U_ORIGIN				0x1
#define U_OLD_ORIGIN			0x2 // used by lightning
#define U_ANGLES				0x4
#define U_ANIMATIONS			0x8 // animation frames
#define U_EVENT					0x10 // client side events
#define U_EFFECTS				0x20 // client side effects
#define U_MODELS				0x40 // models (primary and linked)
#define U_CLIENT				0x80 // offset into client skins array
#define U_SOUND					0x100 // looped sounds
#define U_SOLID					0x200
#define U_REMOVE				0x400 // remove this entity, don't add it
/*
 * @brief Message authoring facilities.
 */
void Net_WriteData(mem_buf_t *buf, const void *data, size_t len);
void Net_WriteChar(mem_buf_t *sb, const int32_t c);
void Net_WriteByte(mem_buf_t *buf, const int32_t c);
void Net_WriteShort(mem_buf_t *buf, const int32_t c);
void Net_WriteLong(mem_buf_t *buf, const int32_t c);
void Net_WriteString(mem_buf_t *buf, const char *s);
void Net_WriteCoord(mem_buf_t *buf, const vec_t f);
void Net_WritePos(mem_buf_t *buf, const vec3_t pos);
void Net_WriteAngle(mem_buf_t *buf, const vec_t f);
void Net_WriteAngles(mem_buf_t *buf, const vec3_t angles);
void Net_WriteDeltaUserCmd(mem_buf_t *buf, const user_cmd_t *from, const user_cmd_t *to);
void Net_WriteDeltaEntity(entity_state_t *from, entity_state_t *to, mem_buf_t *msg, _Bool force,
		_Bool newentity);
void Net_WriteDir(mem_buf_t *buf, const vec3_t dir);

// message reading
void Net_BeginReading(mem_buf_t *buf);
void Net_ReadData(mem_buf_t *buf, void *data, size_t len);
int32_t Net_ReadChar(mem_buf_t *buf);
int32_t Net_ReadByte(mem_buf_t *buf);
int32_t Net_ReadShort(mem_buf_t *buf);
int32_t Net_ReadLong(mem_buf_t *buf);
char *Net_ReadString(mem_buf_t *buf);
char *Net_ReadStringLine(mem_buf_t *buf);
vec_t Net_ReadCoord(mem_buf_t *buf);
void Net_ReadPos(mem_buf_t *buf, vec3_t pos);
vec_t Net_ReadAngle(mem_buf_t *buf);
void Net_ReadAngles(mem_buf_t *buf, vec3_t angles);
void Net_ReadDeltaUsercmd(mem_buf_t *buf, struct user_cmd_s *from, struct user_cmd_s *cmd);
void Net_ReadDeltaEntity(entity_state_t *from, entity_state_t *to, mem_buf_t *msg, uint16_t bits,
		uint16_t number);
void Net_ReadDir(mem_buf_t *buf, vec3_t vector);

#endif /* __NET_MESSAGE_H__ */
