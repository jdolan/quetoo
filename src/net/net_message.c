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
 * @brief
 */
void Net_WriteData(mem_buf_t *msg, const void *data, size_t len) {
	Mem_WriteBuffer(msg, data, len);
}

/**
 * @brief
 */
void Net_WriteChar(mem_buf_t *msg, int32_t c) {
	byte *buf;

	buf = Mem_AllocBuffer(msg, sizeof(char));
	buf[0] = c;
}

/**
 * @brief
 */
void Net_WriteByte(mem_buf_t *msg, int32_t c) {
	byte *buf;

	buf = Mem_AllocBuffer(msg, sizeof(byte));
	buf[0] = c;
}

/**
 * @brief
 */
void Net_WriteShort(mem_buf_t *msg, int32_t c) {
	byte *buf;

	buf = Mem_AllocBuffer(msg, sizeof(int16_t));
	buf[0] = c & 0xff;
	buf[1] = c >> 8;
}

/**
 * @brief
 */
void Net_WriteLong(mem_buf_t *msg, int32_t c) {
	byte *buf;

	buf = Mem_AllocBuffer(msg, sizeof(int32_t));
	buf[0] = c & 0xff;
	buf[1] = (c >> 8) & 0xff;
	buf[2] = (c >> 16) & 0xff;
	buf[3] = c >> 24;
}

/**
 * @brief
 */
void Net_WriteString(mem_buf_t *msg, const char *s) {
	if (!s) {
		Mem_WriteBuffer(msg, "", 1);
	} else {
		Mem_WriteBuffer(msg, s, strlen(s) + 1);
	}
}

/**
 * @brief
 */
void Net_WriteFloat(mem_buf_t *msg, float v) {

	const net_float vec = {
		.v = v
	};

	Net_WriteLong(msg, vec.i);
}

/**
 * @brief
 */
void Net_WritePosition(mem_buf_t *msg, const vec3_t pos) {
	Net_WriteFloat(msg, pos.x);
	Net_WriteFloat(msg, pos.y);
	Net_WriteFloat(msg, pos.z);
}

/**
 * @brief
 */
void Net_WriteAngle(mem_buf_t *msg, float angle) {

	while (angle < 0.f) {
		angle += 360.f;
	}

	while (angle > 360.f) {
		angle -= 360.f;
	}

	Net_WriteShort(msg, (int16_t) ((angle / 360.0f) * UINT16_MAX));
}

/**
 * @brief
 */
void Net_WriteAngles(mem_buf_t *msg, const vec3_t angles) {
	Net_WriteAngle(msg, angles.x);
	Net_WriteAngle(msg, angles.y);
	Net_WriteAngle(msg, angles.z);
}

/**
 * @brief
 */
void Net_WriteDir(mem_buf_t *msg, const vec3_t dir) {
	int32_t i, best = 0;
	float best_d = 0.0;

	for (i = 0; i < NUM_APPROXIMATE_NORMALS; i++) {
		const float d = Vec3_Dot(dir, approximate_normals[i]);
		if (d > best_d) {
			best_d = d;
			best = i;
		}
	}

	Net_WriteByte(msg, best);
}

/**
 * @brief
 */
void Net_WriteBounds(mem_buf_t *msg, const vec3_t mins, const vec3_t maxs) {

	const vec3s_t _mins = Vec3_CastVec3s(mins);

	Net_WriteShort(msg, _mins.x);
	Net_WriteShort(msg, _mins.y);
	Net_WriteShort(msg, _mins.z);

	const vec3s_t _maxs = Vec3_CastVec3s(maxs);

	Net_WriteShort(msg, _maxs.x);
	Net_WriteShort(msg, _maxs.y);
	Net_WriteShort(msg, _maxs.z);
}

/**
 * @brief
 */
void Net_WriteDeltaMoveCmd(mem_buf_t *msg, const pm_cmd_t *from, const pm_cmd_t *to) {

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

	Net_WriteByte(msg, to->msec);
}

/**
 * @brief
 */
void Net_WriteDeltaPlayerState(mem_buf_t *msg, const player_state_t *from, const player_state_t *to) {

	uint16_t bits = 0;

	if (to->pm_state.type != from->pm_state.type) {
		bits |= PS_PM_TYPE;
	}

	if (!Vec3_Equal(to->pm_state.origin, from->pm_state.origin)) {
		bits |= PS_PM_ORIGIN;
	}

	if (!Vec3_Equal(to->pm_state.velocity, from->pm_state.velocity)) {
		bits |= PS_PM_VELOCITY;
	}

	if (to->pm_state.flags != from->pm_state.flags) {
		bits |= PS_PM_FLAGS;
	}

	if (to->pm_state.time != from->pm_state.time) {
		bits |= PS_PM_TIME;
	}

	if (to->pm_state.gravity != from->pm_state.gravity) {
		bits |= PS_PM_GRAVITY;
	}

	if (!Vec3_Equal(to->pm_state.view_offset, from->pm_state.view_offset)) {
		bits |= PS_PM_VIEW_OFFSET;
	}

	if (!Vec3_Equal(to->pm_state.view_angles, from->pm_state.view_angles)) {
		bits |= PS_PM_VIEW_ANGLES;
	}

	if (!Vec3_Equal(to->pm_state.delta_angles, from->pm_state.delta_angles)) {
		bits |= PS_PM_DELTA_ANGLES;
	}

	if (!Vec3_Equal(to->pm_state.hook_position, from->pm_state.hook_position)) {
		bits |= PS_PM_HOOK_POSITION;
	}

	if (to->pm_state.hook_length != from->pm_state.hook_length) {
		bits |= PS_PM_HOOK_LENGTH;
	}

	if (to->pm_state.step_offset != from->pm_state.step_offset) {
		bits |= PS_PM_STEP_OFFSET;
	}

	Net_WriteShort(msg, bits);

	if (bits & PS_PM_TYPE) {
		Net_WriteByte(msg, to->pm_state.type);
	}

	if (bits & PS_PM_ORIGIN) {
		Net_WritePosition(msg, to->pm_state.origin);
	}

	if (bits & PS_PM_VELOCITY) {
		Net_WritePosition(msg, to->pm_state.velocity);
	}

	if (bits & PS_PM_FLAGS) {
		Net_WriteShort(msg, to->pm_state.flags);
	}

	if (bits & PS_PM_TIME) {
		Net_WriteShort(msg, to->pm_state.time);
	}

	if (bits & PS_PM_GRAVITY) {
		Net_WriteShort(msg, to->pm_state.gravity);
	}

	if (bits & PS_PM_VIEW_OFFSET) {
		Net_WritePosition(msg, to->pm_state.view_offset);
	}

	if (bits & PS_PM_VIEW_ANGLES) {
		Net_WriteAngles(msg, to->pm_state.view_angles);
	}

	if (bits & PS_PM_DELTA_ANGLES) {
		Net_WriteAngles(msg, to->pm_state.delta_angles);
	}

	if (bits & PS_PM_HOOK_POSITION) {
		Net_WritePosition(msg, to->pm_state.hook_position);
	}

	if (bits & PS_PM_HOOK_LENGTH) {
		Net_WriteShort(msg, to->pm_state.hook_length);
	}

	if (bits & PS_PM_STEP_OFFSET) {
		Net_WriteFloat(msg, to->pm_state.step_offset);
	}

	uint32_t stat_bits = 0;

	for (int32_t i = 0; i < MAX_STATS; i++) {
		if (to->stats[i] != from->stats[i]) {
			stat_bits |= 1 << i;
		}
	}

	Net_WriteLong(msg, stat_bits);

	for (int32_t i = 0; i < MAX_STATS; i++) {
		if (stat_bits & (1 << i)) {
			Net_WriteShort(msg, to->stats[i]);
		}
	}
}

/**
 * @brief Writes an entity's state changes to a net message. Can delta from
 * either a baseline or a previous packet_entity
 */
void Net_WriteDeltaEntity(mem_buf_t *msg, const entity_state_t *from, const entity_state_t *to,
                          _Bool force) {

	uint16_t bits = 0;

	if (to->number <= 0) {
		Com_Error(ERROR_FATAL, "Unset entity number\n");
	}

	if (to->number >= MAX_ENTITIES) {
		Com_Error(ERROR_FATAL, "Entity number >= MAX_ENTITIES\n");
	}

	if (to->step_offset != from->step_offset) {
		bits |= U_STEP_OFFSET;
	}

	if (to->spawn_id != from->spawn_id) {
		bits |= U_SPAWNID;
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

	if (!Vec3_Equal(to->mins, from->mins) || !Vec3_Equal(to->maxs, from->maxs)) {
		bits |= U_BOUNDS;
	}

	if (!bits && !force) {
		return;    // nothing to send
	}

	// write the message

	Net_WriteShort(msg, to->number);
	Net_WriteShort(msg, bits);

	if (bits & U_STEP_OFFSET) {
		Net_WriteByte(msg, to->step_offset);
	}

	if (bits & U_SPAWNID) {
		Net_WriteByte(msg, to->spawn_id);
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
	}

	if (bits & U_EFFECTS) {
		Net_WriteShort(msg, to->effects);
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
		Net_WriteBounds(msg, to->mins, to->maxs);
	}
}

/**
 * @brief
 */
void Net_BeginReading(mem_buf_t *msg) {
	msg->read = 0;
}

/**
 * @brief
 */
void Net_ReadData(mem_buf_t *msg, void *data, size_t len) {
	size_t i;

	for (i = 0; i < len; i++) {
		((byte *) data)[i] = Net_ReadByte(msg);
	}
}

/**
 * @brief Returns -1 if no more characters are available.
 */
int32_t Net_ReadChar(mem_buf_t *msg) {
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
 * @brief
 */
int32_t Net_ReadByte(mem_buf_t *msg) {
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
 * @brief
 */
int32_t Net_ReadShort(mem_buf_t *msg) {
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
 * @brief
 */
int32_t Net_ReadLong(mem_buf_t *msg) {
	int32_t c;

	if (msg->read + 4 > msg->size) {
		c = -1;
	} else
		c = msg->data[msg->read] + (msg->data[msg->read + 1] << 8) + (msg->data[msg->read + 2]
		        << 16) + (msg->data[msg->read + 3] << 24);

	msg->read += 4;

	return c;
}

/**
 * @brief
 */
char *Net_ReadString(mem_buf_t *msg) {
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
 * @brief
 */
char *Net_ReadStringLine(mem_buf_t *msg) {
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
 * @brief
 */
float Net_ReadFloat(mem_buf_t *msg) {

	const net_float vec = {
		.i = Net_ReadLong(msg)
	};

	return vec.v;
}

/**
 * @brief
 */
vec3_t Net_ReadPosition(mem_buf_t *msg) {
	return (vec3_t) {
		.x = Net_ReadFloat(msg),
		.y = Net_ReadFloat(msg),
		.z = Net_ReadFloat(msg)
	};
}

/**
 * @brief
 */
float Net_ReadAngle(mem_buf_t *msg) {
	return Net_ReadShort(msg) * 360.f / UINT16_MAX;
}

/**
 * @brief
 */
vec3_t Net_ReadAngles(mem_buf_t *msg) {
	return (vec3_t) {
		.x = Net_ReadAngle(msg),
		.y = Net_ReadAngle(msg),
		.z = Net_ReadAngle(msg)
	};
}

/**
 * @brief
 */
vec3_t Net_ReadDir(mem_buf_t *msg) {

	const int32_t b = Net_ReadByte(msg);

	if (b >= NUM_APPROXIMATE_NORMALS) {
		Com_Error(ERROR_DROP, "%d out of range\n", b);
	}

	return approximate_normals[b];
}

/**
 * @brief
 */
void Net_ReadBounds(mem_buf_t *msg, vec3_t *mins, vec3_t *maxs) {
	
	mins->x = Net_ReadShort(msg);
	mins->y = Net_ReadShort(msg);
	mins->z = Net_ReadShort(msg);
	maxs->x = Net_ReadShort(msg);
	maxs->y = Net_ReadShort(msg);
	maxs->z = Net_ReadShort(msg);
}

/**
 * @brief
 */
void Net_ReadDeltaMoveCmd(mem_buf_t *msg, const pm_cmd_t *from, pm_cmd_t *to) {

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

	to->msec = Net_ReadByte(msg);
}

/**
 * @brief
 */
void Net_ReadDeltaPlayerState(mem_buf_t *msg, const player_state_t *from, player_state_t *to) {

	*to = *from;

	const int32_t bits = Net_ReadShort(msg);

	if (bits & PS_PM_TYPE) {
		to->pm_state.type = Net_ReadByte(msg);
	}

	if (bits & PS_PM_ORIGIN) {
		to->pm_state.origin = Net_ReadPosition(msg);
	}

	if (bits & PS_PM_VELOCITY) {
		to->pm_state.velocity = Net_ReadPosition(msg);
	}

	if (bits & PS_PM_FLAGS) {
		to->pm_state.flags = Net_ReadShort(msg);
	}

	if (bits & PS_PM_TIME) {
		to->pm_state.time = Net_ReadShort(msg);
	}

	if (bits & PS_PM_GRAVITY) {
		to->pm_state.gravity = Net_ReadShort(msg);
	}

	if (bits & PS_PM_VIEW_OFFSET) {
		to->pm_state.view_offset = Net_ReadPosition(msg);
	}

	if (bits & PS_PM_VIEW_ANGLES) {
		to->pm_state.view_angles = Net_ReadAngles(msg);
	}

	if (bits & PS_PM_DELTA_ANGLES) {
		to->pm_state.delta_angles = Net_ReadAngles(msg);
	}

	if (bits & PS_PM_HOOK_POSITION) {
		to->pm_state.hook_position = Net_ReadPosition(msg);
	}

	if (bits & PS_PM_HOOK_LENGTH) {
		to->pm_state.hook_length = Net_ReadShort(msg);
	}

	if (bits & PS_PM_STEP_OFFSET) {
		to->pm_state.step_offset = Net_ReadFloat(msg);
	}

	const int32_t stat_bits = Net_ReadLong(msg);

	for (int32_t i = 0; i < MAX_STATS; i++) {
		if (stat_bits & (1 << i)) {
			to->stats[i] = Net_ReadShort(msg);
		}
	}
}

/**
 * @brief
 */
void Net_ReadDeltaEntity(mem_buf_t *msg, const entity_state_t *from, entity_state_t *to,
                         uint16_t number, uint16_t bits) {

	*to = *from;

	to->number = number;

	if (bits & U_STEP_OFFSET) {
		to->step_offset = Net_ReadByte(msg);
	}

	if (bits & U_SPAWNID) {
		to->spawn_id = Net_ReadByte(msg);
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
	} else {
		to->event = 0;
	}

	if (bits & U_EFFECTS) {
		to->effects = Net_ReadShort(msg);
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
		Net_ReadBounds(msg, &to->mins, &to->maxs);
	}
}
