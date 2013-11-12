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

#include "net_message.h"

/*
 * @brief
 */
void Net_WriteData(mem_buf_t *buf, const void *data, size_t len) {
	Mem_WriteBuffer(buf, data, len);
}

/*
 * @brief
 */
void Net_WriteChar(mem_buf_t *sb, const int32_t c) {
	byte *buf;

	buf = Mem_AllocBuffer(sb, sizeof(char));
	buf[0] = c;
}

/*
 * @brief
 */
void Net_WriteByte(mem_buf_t *sb, const int32_t c) {
	byte *buf;

	buf = Mem_AllocBuffer(sb, sizeof(byte));
	buf[0] = c;
}

/*
 * @brief
 */
void Net_WriteShort(mem_buf_t *sb, const int32_t c) {
	byte *buf;

	buf = Mem_AllocBuffer(sb, sizeof(int16_t));
	buf[0] = c & 0xff;
	buf[1] = c >> 8;
}

/*
 * @brief
 */
void Net_WriteLong(mem_buf_t *sb, const int32_t c) {
	byte *buf;

	buf = Mem_AllocBuffer(sb, sizeof(int32_t));
	buf[0] = c & 0xff;
	buf[1] = (c >> 8) & 0xff;
	buf[2] = (c >> 16) & 0xff;
	buf[3] = c >> 24;
}

/*
 * @brief
 */
void Net_WriteString(mem_buf_t *sb, const char *s) {
	if (!s)
		Mem_WriteBuffer(sb, "", 1);
	else
		Mem_WriteBuffer(sb, s, strlen(s) + 1);
}

/*
 * @brief
 */
void Net_WriteVector(mem_buf_t *sb, const vec_t v) {
	Net_WriteShort(sb, (int32_t) (v * 8.0));
}

/*
 * @brief
 */
void Net_WritePosition(mem_buf_t *sb, const vec3_t pos) {
	Net_WriteShort(sb, (int32_t) (pos[0] * 8.0));
	Net_WriteShort(sb, (int32_t) (pos[1] * 8.0));
	Net_WriteShort(sb, (int32_t) (pos[2] * 8.0));
}

/*
 * @brief
 */
void Net_WriteAngle(mem_buf_t *sb, const vec_t v) {
	Net_WriteByte(sb, (int32_t) (v * 255.0 / 360.0) & 255);
}

/*
 * @brief
 */
void Net_WriteAngles(mem_buf_t *sb, const vec3_t angles) {
	Net_WriteAngle(sb, angles[0]);
	Net_WriteAngle(sb, angles[1]);
	Net_WriteAngle(sb, angles[2]);
}

/*
 * @brief
 */
void Net_WriteDir(mem_buf_t *sb, const vec3_t dir) {
	int32_t i, best = 0;
	vec_t best_d = 0.0;

	for (i = 0; i < NUM_APPROXIMATE_NORMALS; i++) {
		const vec_t d = DotProduct(dir, approximate_normals[i]);
		if (d > best_d) {
			best_d = d;
			best = i;
		}
	}

	Net_WriteByte(sb, best);
}

/*
 * @brief
 */
void Net_WriteDeltaUserCmd(mem_buf_t *buf, const user_cmd_t *from, const user_cmd_t *to) {
	byte bits = 0;

	// send the movement message
	if (to->angles[0] != from->angles[0])
		bits |= CMD_ANGLE1;
	if (to->angles[1] != from->angles[1])
		bits |= CMD_ANGLE2;
	if (to->angles[2] != from->angles[2])
		bits |= CMD_ANGLE3;

	if (to->forward != from->forward)
		bits |= CMD_FORWARD;
	if (to->right != from->right)
		bits |= CMD_RIGHT;
	if (to->up != from->up)
		bits |= CMD_UP;

	if (to->buttons != from->buttons)
		bits |= CMD_BUTTONS;

	Net_WriteByte(buf, bits);

	if (bits & CMD_ANGLE1)
		Net_WriteShort(buf, to->angles[0]);
	if (bits & CMD_ANGLE2)
		Net_WriteShort(buf, to->angles[1]);
	if (bits & CMD_ANGLE3)
		Net_WriteShort(buf, to->angles[2]);

	if (bits & CMD_FORWARD)
		Net_WriteShort(buf, to->forward);
	if (bits & CMD_RIGHT)
		Net_WriteShort(buf, to->right);
	if (bits & CMD_UP)
		Net_WriteShort(buf, to->up);

	if (bits & CMD_BUTTONS)
		Net_WriteByte(buf, to->buttons);

	Net_WriteByte(buf, to->msec);
}

/*
 * @brief Writes an entity's state changes to a net message. Can delta from
 * either a baseline or a previous packet_entity
 */
void Net_WriteDeltaEntity(mem_buf_t *buf, const entity_state_t *from, const entity_state_t *to,
		_Bool force, _Bool is_new) {

	uint16_t bits = 0;

	if (to->number <= 0) {
		Com_Error(ERR_FATAL, "Unset entity number\n");
	}

	if (to->number >= MAX_EDICTS) {
		Com_Error(ERR_FATAL, "Entity number >= MAX_EDICTS\n");
	}

	if (!VectorCompare(to->origin, from->origin))
		bits |= U_ORIGIN;

	if (is_new || !VectorCompare(from->old_origin, to->old_origin))
		bits |= U_OLD_ORIGIN;

	if (!VectorCompare(to->angles, from->angles))
		bits |= U_ANGLES;

	if (to->animation1 != from->animation1 || to->animation2 != from->animation2)
		bits |= U_ANIMATIONS;

	if (to->event) // event is not delta compressed, just 0 compressed
		bits |= U_EVENT;

	if (to->effects != from->effects)
		bits |= U_EFFECTS;

	if (to->trail != from->trail)
		bits |= U_TRAIL;

	if (to->model1 != from->model1 || to->model2 != from->model2 || to->model3 != from->model3
			|| to->model4 != from->model4)
		bits |= U_MODELS;

	if (to->client != from->client)
		bits |= U_CLIENT;

	if (to->sound != from->sound)
		bits |= U_SOUND;

	if (to->solid != from->solid)
		bits |= U_SOLID;

	if (!bits && !force)
		return; // nothing to send

	// write the message

	Net_WriteShort(buf, to->number);
	Net_WriteShort(buf, bits);

	if (bits & U_ORIGIN)
		Net_WritePosition(buf, to->origin);

	if (bits & U_OLD_ORIGIN)
		Net_WritePosition(buf, to->old_origin);

	if (bits & U_ANGLES)
		Net_WriteAngles(buf, to->angles);

	if (bits & U_ANIMATIONS) {
		Net_WriteByte(buf, to->animation1);
		Net_WriteByte(buf, to->animation2);
	}

	if (bits & U_EVENT)
		Net_WriteByte(buf, to->event);

	if (bits & U_EFFECTS)
		Net_WriteShort(buf, to->effects);

	if (bits & U_TRAIL)
		Net_WriteByte(buf, to->trail);

	if (bits & U_MODELS) {
		Net_WriteByte(buf, to->model1);
		Net_WriteByte(buf, to->model2);
		Net_WriteByte(buf, to->model3);
		Net_WriteByte(buf, to->model4);
	}

	if (bits & U_CLIENT)
		Net_WriteByte(buf, to->client);

	if (bits & U_SOUND)
		Net_WriteByte(buf, to->sound);

	if (bits & U_SOLID)
		Net_WriteShort(buf, to->solid);
}

/*
 * @brief
 */
void Net_BeginReading(mem_buf_t *msg) {
	msg->read = 0;
}

/*
 * @brief
 */
void Net_ReadData(mem_buf_t *sb, void *data, size_t len) {
	size_t i;

	for (i = 0; i < len; i++) {
		((byte *) data)[i] = Net_ReadByte(sb);
	}
}

/*
 * @brief Returns -1 if no more characters are available.
 */
int32_t Net_ReadChar(mem_buf_t *sb) {
	int32_t c;

	if (sb->read + 1 > sb->size)
		c = -1;
	else
		c = (signed char) sb->data[sb->read];
	sb->read++;

	return c;
}

/*
 * @brief
 */
int32_t Net_ReadByte(mem_buf_t *sb) {
	int32_t c;

	if (sb->read + 1 > sb->size)
		c = -1;
	else
		c = (byte) sb->data[sb->read];
	sb->read++;

	return c;
}

/*
 * @brief
 */
int32_t Net_ReadShort(mem_buf_t *sb) {
	int32_t c;

	if (sb->read + 2 > sb->size)
		c = -1;
	else
		c = (int16_t) (sb->data[sb->read] + (sb->data[sb->read + 1] << 8));

	sb->read += 2;

	return c;
}

/*
 * @brief
 */
int32_t Net_ReadLong(mem_buf_t *sb) {
	int32_t c;

	if (sb->read + 4 > sb->size)
		c = -1;
	else
		c = sb->data[sb->read] + (sb->data[sb->read + 1] << 8) + (sb->data[sb->read + 2] << 16)
				+ (sb->data[sb->read + 3] << 24);

	sb->read += 4;

	return c;
}

/*
 * @brief
 */
char *Net_ReadString(mem_buf_t *sb) {
	static char string[MAX_STRING_CHARS];
	int32_t c;
	uint32_t l;

	l = 0;
	do {
		c = Net_ReadChar(sb);
		if (c == -1 || c == 0)
			break;
		string[l] = c;
		l++;
	} while (l < sizeof(string) - 1);

	string[l] = '\0';

	return string;
}

/*
 * @brief
 */
char *Net_ReadStringLine(mem_buf_t *sb) {
	static char string[MAX_STRING_CHARS];
	int32_t c;
	uint32_t l;

	l = 0;
	do {
		c = Net_ReadChar(sb);
		if (c == -1 || c == 0 || c == '\n')
			break;
		string[l] = c;
		l++;
	} while (l < sizeof(string) - 1);

	string[l] = 0;

	return string;
}

/*
 * @brief
 */
vec_t Net_ReadVector(mem_buf_t *sb) {
	return Net_ReadShort(sb) * (1.0 / 8.0);
}

/*
 * @brief
 */
void Net_ReadPosition(mem_buf_t *sb, vec3_t pos) {
	pos[0] = Net_ReadVector(sb);
	pos[1] = Net_ReadVector(sb);
	pos[2] = Net_ReadVector(sb);
}

/*
 * @brief
 */
vec_t Net_ReadAngle(mem_buf_t *sb) {
	return Net_ReadChar(sb) * (360.0 / 255.0);
}

/*
 * @brief
 */
void Net_ReadAngles(mem_buf_t *sb, vec3_t angles) {
	angles[0] = Net_ReadAngle(sb);
	angles[1] = Net_ReadAngle(sb);
	angles[2] = Net_ReadAngle(sb);
}

/*
 * @brief
 */
void Net_ReadDir(mem_buf_t *sb, vec3_t dir) {
	int32_t b;

	b = Net_ReadByte(sb);

	if (b >= NUM_APPROXIMATE_NORMALS) {
		Com_Error(ERR_DROP, "%d out of range\n", b);
	}

	VectorCopy(approximate_normals[b], dir);
}

/*
 * @brief
 */
void Net_ReadDeltaUserCmd(mem_buf_t *sb, const user_cmd_t *from, user_cmd_t *to) {
	int32_t bits;

	*to = *from;

	bits = Net_ReadByte(sb);

	// read current angles
	if (bits & CMD_ANGLE1)
		to->angles[0] = Net_ReadShort(sb);
	if (bits & CMD_ANGLE2)
		to->angles[1] = Net_ReadShort(sb);
	if (bits & CMD_ANGLE3)
		to->angles[2] = Net_ReadShort(sb);

	// read movement
	if (bits & CMD_FORWARD)
		to->forward = Net_ReadShort(sb);
	if (bits & CMD_RIGHT)
		to->right = Net_ReadShort(sb);
	if (bits & CMD_UP)
		to->up = Net_ReadShort(sb);

	// read buttons
	if (bits & CMD_BUTTONS)
		to->buttons = Net_ReadByte(sb);

	// read time to run command
	to->msec = Net_ReadByte(sb);
}

/*
 * @brief
 */
void Net_ReadDeltaEntity(mem_buf_t *buf, const entity_state_t *from, entity_state_t *to,
		uint16_t number, uint16_t bits) {

	// set everything to the state we are delta'ing from
	*to = *from;

	to->number = number;

	if (bits & U_ORIGIN)
		Net_ReadPosition(buf, to->origin);

	if (bits & U_OLD_ORIGIN)
		Net_ReadPosition(buf, to->old_origin);

	if (bits & U_ANGLES)
		Net_ReadAngles(buf, to->angles);

	if (bits & U_ANIMATIONS) {
		to->animation1 = Net_ReadByte(buf);
		to->animation2 = Net_ReadByte(buf);
	}

	if (bits & U_EVENT)
		to->event = Net_ReadByte(buf);
	else
		to->event = 0;

	if (bits & U_EFFECTS)
		to->effects = Net_ReadShort(buf);

	if (bits & U_TRAIL)
		to->trail = Net_ReadByte(buf);

	if (bits & U_MODELS) {
		to->model1 = Net_ReadByte(buf);
		to->model2 = Net_ReadByte(buf);
		to->model3 = Net_ReadByte(buf);
		to->model4 = Net_ReadByte(buf);
	}

	if (bits & U_CLIENT)
		to->client = Net_ReadByte(buf);

	if (bits & U_SOUND)
		to->sound = Net_ReadByte(buf);

	if (bits & U_SOLID)
		to->solid = Net_ReadShort(buf);
}

