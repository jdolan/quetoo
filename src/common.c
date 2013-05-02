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

#include "common.h"
#include "common-anorms.h"


// console redirection (e.g. rcon)
typedef struct redirect_s {
	int32_t target;

	char *buffer;
	size_t size;

	RedirectFlush Flush;
} redirect_t;

static redirect_t redirect;

/*
 * @brief
 */
void Com_BeginRedirect(int32_t target, char *buffer, size_t size, RedirectFlush Flush) {

	if (!target || !buffer || !size || !Flush) {
		Com_Error(ERR_FATAL, "Invalid redirect\n");
	}

	redirect.target = target;
	redirect.buffer = buffer;
	redirect.size = size;
	redirect.Flush = Flush;

	*redirect.buffer = '\0';
}

/*
 * @brief
 */
void Com_EndRedirect(void) {

	if (!redirect.target || !redirect.buffer || !redirect.size || !redirect.Flush) {
		Com_Error(ERR_FATAL, "Invalid redirect\n");
	}

	redirect.Flush(redirect.target, redirect.buffer);

	memset(&redirect, 0, sizeof(redirect));
}

/*
 * @brief Print a debug statement. If the format begins with '!', the function
 * name is omitted.
 */
void Com_Debug_(const char *func, const char *fmt, ...) {
	char msg[MAX_PRINT_MSG];

	if (fmt[0] != '!') {
		g_snprintf(msg, sizeof(msg), "%s: ", func);
	} else {
		msg[0] = '\0';
	}

	const size_t len = strlen(msg);
	va_list args;

	va_start(args, fmt);
	vsnprintf(msg + len, sizeof(msg) - len, fmt, args);
	va_end(args);

	if (quake2world.Debug)
		quake2world.Debug((const char *) msg);
	else
		printf("%s", msg);
}

/*
 * @brief An error condition has occurred. This function does not return.
 */
void Com_Error_(const char *func, err_t err, const char *fmt, ...) {
	char msg[MAX_PRINT_MSG];

	if (fmt[0] != '!') {
		g_snprintf(msg, sizeof(msg), "%s: ", func);
	} else {
		msg[0] = '\0';
	}

	const size_t len = strlen(msg);
	va_list args;

	va_start(args, fmt);
	vsnprintf(msg + len, sizeof(msg) - len, fmt, args);
	va_end(args);

	if (quake2world.Error) {
		quake2world.Error(err, (const char *) msg);
	} else {
		fprintf(stderr, "%s", msg);
		exit(err);
	}
}

/*
 * @brief
 */
void Com_Print(const char *fmt, ...) {
	va_list args;
	char msg[MAX_PRINT_MSG];

	va_start(args, fmt);
	vsnprintf(msg, sizeof(msg), fmt, args);
	va_end(args);

	if (redirect.target) { // handle redirection (rcon)
		if ((strlen(msg) + strlen(redirect.buffer)) > (redirect.size - 1)) {
			redirect.Flush(redirect.target, redirect.buffer);
			*redirect.buffer = '\0';
		}
		g_strlcat(redirect.buffer, msg, redirect.size);
		return;
	}

	if (quake2world.Print)
		quake2world.Print((const char *) msg);
	else
		printf("%s", msg);
}

/*
 * @brief Prints a warning message.
 */
void Com_Warn_(const char *func, const char *fmt, ...) {
	static char msg[MAX_PRINT_MSG];

	if (fmt[0] != '!') {
		g_snprintf(msg, sizeof(msg), "%s: ", func);
	} else {
		msg[0] = '\0';
	}

	const size_t len = strlen(msg);
	va_list args;

	va_start(args, fmt);
	vsnprintf(msg + len, sizeof(msg) - len, fmt, args);
	va_end(args);

	if (quake2world.Warn)
		quake2world.Warn((const char *) msg);
	else
		fprintf(stderr, "WARNING: %s", msg);
}

/*
 * @brief
 */
void Com_Verbose(const char *fmt, ...) {
	va_list args;
	static char msg[MAX_PRINT_MSG];

	va_start(args, fmt);
	vsnprintf(msg, sizeof(msg), fmt, args);
	va_end(args);

	if (quake2world.Verbose)
		quake2world.Verbose((const char *) msg);
	else
		printf("%s", msg);
}

/*
 * @brief Initializes the global arguments list and dispatches to an underlying
 * implementation, if provided. Should be called shortly after program
 * execution begins.
 */
void Com_Init(int32_t argc, char **argv) {

	quake2world.argc = argc;
	quake2world.argv = argv;

	//putenv("G_SLICE=always-malloc");

	if (quake2world.Init) {
		quake2world.Init();
	}
}

/*
 * @brief Program exit point under normal circumstances. Dispatches to a
 * specialized implementation, if provided, or simply prints the message and
 * exits. This function does not return.
 */
void Com_Shutdown(const char *fmt, ...) {
	va_list args;
	char msg[MAX_PRINT_MSG];

	fmt = fmt ? fmt : ""; // normal shutdown will actually pass NULL

	va_start(args, fmt);
	vsnprintf(msg, sizeof(msg), fmt, args);
	va_end(args);

	if (quake2world.Shutdown) {
		quake2world.Shutdown(msg);
	} else {
		Com_Print("%s", msg);
	}

	exit(0);
}

/*
 * @brief
 */
uint32_t Com_WasInit(uint32_t s) {
	return quake2world.subsystems & s;
}

/*
 * @brief
 */
void Com_InitSubsystem(uint32_t s) {
	quake2world.subsystems |= s;
}

/*
 * @brief
 */
void Com_QuitSubsystem(uint32_t s) {
	quake2world.subsystems &= ~s;
}

/*
 *
 * MESSAGE IO FUNCTIONS
 *
 * Handles byte ordering and avoids alignment errors.
 */

/*
 * @brief
 */
void Msg_WriteData(size_buf_t *sb, const void *data, size_t len) {
	Sb_Write(sb, data, len);
}

/*
 * @brief
 */
void Msg_WriteChar(size_buf_t *sb, const int32_t c) {
	byte *buf;

	buf = Sb_Alloc(sb, sizeof(char));
	buf[0] = c;
}

/*
 * @brief
 */
void Msg_WriteByte(size_buf_t *sb, const int32_t c) {
	byte *buf;

	buf = Sb_Alloc(sb, sizeof(byte));
	buf[0] = c;
}

/*
 * @brief
 */
void Msg_WriteShort(size_buf_t *sb, const int32_t c) {
	byte *buf;

	buf = Sb_Alloc(sb, sizeof(int16_t));
	buf[0] = c & 0xff;
	buf[1] = c >> 8;
}

/*
 * @brief
 */
void Msg_WriteLong(size_buf_t *sb, const int32_t c) {
	byte *buf;

	buf = Sb_Alloc(sb, sizeof(int32_t));
	buf[0] = c & 0xff;
	buf[1] = (c >> 8) & 0xff;
	buf[2] = (c >> 16) & 0xff;
	buf[3] = c >> 24;
}

/*
 * @brief
 */
void Msg_WriteString(size_buf_t *sb, const char *s) {
	if (!s)
		Sb_Write(sb, "", 1);
	else
		Sb_Write(sb, s, strlen(s) + 1);
}

/*
 * @brief
 */
void Msg_WriteCoord(size_buf_t *sb, const float f) {
	Msg_WriteShort(sb, (int) (f * 8.0));
}

/*
 * @brief
 */
void Msg_WritePos(size_buf_t *sb, const vec3_t pos) {
	Msg_WriteShort(sb, (int) (pos[0] * 8.0));
	Msg_WriteShort(sb, (int) (pos[1] * 8.0));
	Msg_WriteShort(sb, (int) (pos[2] * 8.0));
}

/*
 * @brief
 */
void Msg_WriteAngle(size_buf_t *sb, const float f) {
	Msg_WriteByte(sb, (int) (f * 255.0 / 360.0) & 255);
}

/*
 * @brief
 */
void Msg_WriteAngles(size_buf_t *sb, const vec3_t angles) {
	Msg_WriteAngle(sb, angles[0]);
	Msg_WriteAngle(sb, angles[1]);
	Msg_WriteAngle(sb, angles[2]);
}

/*
 * @brief
 */
void Msg_WriteDeltaUsercmd(size_buf_t *buf, user_cmd_t *from, user_cmd_t *cmd) {
	int32_t bits;

	// send the movement message
	bits = 0;
	if (cmd->angles[0] != from->angles[0])
		bits |= CMD_ANGLE1;
	if (cmd->angles[1] != from->angles[1])
		bits |= CMD_ANGLE2;
	if (cmd->angles[2] != from->angles[2])
		bits |= CMD_ANGLE3;
	if (cmd->forward != from->forward)
		bits |= CMD_FORWARD;
	if (cmd->right != from->right)
		bits |= CMD_RIGHT;
	if (cmd->up != from->up)
		bits |= CMD_UP;
	if (cmd->buttons != from->buttons)
		bits |= CMD_BUTTONS;

	Msg_WriteByte(buf, bits);

	if (bits & CMD_ANGLE1)
		Msg_WriteShort(buf, cmd->angles[0]);
	if (bits & CMD_ANGLE2)
		Msg_WriteShort(buf, cmd->angles[1]);
	if (bits & CMD_ANGLE3)
		Msg_WriteShort(buf, cmd->angles[2]);

	if (bits & CMD_FORWARD)
		Msg_WriteShort(buf, cmd->forward);
	if (bits & CMD_RIGHT)
		Msg_WriteShort(buf, cmd->right);
	if (bits & CMD_UP)
		Msg_WriteShort(buf, cmd->up);

	if (bits & CMD_BUTTONS)
		Msg_WriteByte(buf, cmd->buttons);

	Msg_WriteByte(buf, cmd->msec);
}

/*
 * @brief
 */
void Msg_WriteDir(size_buf_t *sb, const vec3_t dir) {
	int32_t i, best;
	float d, bestd;

	if (!dir) {
		Msg_WriteByte(sb, 0);
		return;
	}

	bestd = 0;
	best = 0;
	for (i = 0; i < NUM_APPROXIMATE_NORMALS; i++) {
		d = DotProduct(dir, approximate_normals[i]);
		if (d > bestd) {
			bestd = d;
			best = i;
		}
	}
	Msg_WriteByte(sb, best);
}

/*
 * @brief
 */
void Msg_ReadDir(size_buf_t *sb, vec3_t dir) {
	int32_t b;

	b = Msg_ReadByte(sb);

	if (b >= NUM_APPROXIMATE_NORMALS) {
		Com_Error(ERR_DROP, "%d out of range\n", b);
	}

	VectorCopy(approximate_normals[b], dir);
}

/*
 * @brief Writes part of a packetentities message.
 * Can delta from either a baseline or a previous packet_entity
 */
void Msg_WriteDeltaEntity(entity_state_t *from, entity_state_t *to, size_buf_t *msg, _Bool force,
		_Bool is_new) {

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

	Msg_WriteShort(msg, to->number);
	Msg_WriteShort(msg, bits);

	if (bits & U_ORIGIN)
		Msg_WritePos(msg, to->origin);

	if (bits & U_OLD_ORIGIN)
		Msg_WritePos(msg, to->old_origin);

	if (bits & U_ANGLES)
		Msg_WriteAngles(msg, to->angles);

	if (bits & U_ANIMATIONS) {
		Msg_WriteByte(msg, to->animation1);
		Msg_WriteByte(msg, to->animation2);
	}

	if (bits & U_EVENT)
		Msg_WriteByte(msg, to->event);

	if (bits & U_EFFECTS)
		Msg_WriteShort(msg, to->effects);

	if (bits & U_MODELS) {
		Msg_WriteByte(msg, to->model1);
		Msg_WriteByte(msg, to->model2);
		Msg_WriteByte(msg, to->model3);
		Msg_WriteByte(msg, to->model4);
	}

	if (bits & U_CLIENT)
		Msg_WriteByte(msg, to->client);

	if (bits & U_SOUND)
		Msg_WriteByte(msg, to->sound);

	if (bits & U_SOLID)
		Msg_WriteShort(msg, to->solid);
}

/*
 * @brief
 */
void Msg_ReadDeltaEntity(entity_state_t *from, entity_state_t *to, size_buf_t *msg,
		uint16_t number, uint16_t bits) {

	// set everything to the state we are delta'ing from
	*to = *from;

	if (!(from->effects & EF_BEAM))
		VectorCopy(from->origin, to->old_origin);

	to->number = number;

	if (bits & U_ORIGIN)
		Msg_ReadPos(msg, to->origin);

	if (bits & U_OLD_ORIGIN)
		Msg_ReadPos(msg, to->old_origin);

	if (bits & U_ANGLES)
		Msg_ReadAngles(msg, to->angles);

	if (bits & U_ANIMATIONS) {
		to->animation1 = Msg_ReadByte(msg);
		to->animation2 = Msg_ReadByte(msg);
	}

	if (bits & U_EVENT)
		to->event = Msg_ReadByte(msg);
	else
		to->event = 0;

	if (bits & U_EFFECTS)
		to->effects = Msg_ReadShort(msg);

	if (bits & U_MODELS) {
		to->model1 = Msg_ReadByte(msg);
		to->model2 = Msg_ReadByte(msg);
		to->model3 = Msg_ReadByte(msg);
		to->model4 = Msg_ReadByte(msg);
	}

	if (bits & U_CLIENT)
		to->client = Msg_ReadByte(msg);

	if (bits & U_SOUND)
		to->sound = Msg_ReadByte(msg);

	if (bits & U_SOLID)
		to->solid = Msg_ReadShort(msg);
}

/*
 * @brief
 */
void Msg_BeginReading(size_buf_t *msg) {
	msg->read = 0;
}

/*
 * @brief
 */
void Msg_ReadData(size_buf_t *sb, void *data, size_t len) {
	size_t i;

	for (i = 0; i < len; i++) {
		((byte *) data)[i] = Msg_ReadByte(sb);
	}
}

/*
 * @brief Returns -1 if no more characters are available.
 */
int32_t Msg_ReadChar(size_buf_t *sb) {
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
int32_t Msg_ReadByte(size_buf_t *sb) {
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
int32_t Msg_ReadShort(size_buf_t *sb) {
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
int32_t Msg_ReadLong(size_buf_t *sb) {
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
char *Msg_ReadString(size_buf_t *sb) {
	static char string[MAX_STRING_CHARS];
	int32_t c;
	uint32_t l;

	l = 0;
	do {
		c = Msg_ReadChar(sb);
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
char *Msg_ReadStringLine(size_buf_t *sb) {
	static char string[MAX_STRING_CHARS];
	int32_t c;
	uint32_t l;

	l = 0;
	do {
		c = Msg_ReadChar(sb);
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
float Msg_ReadCoord(size_buf_t *sb) {
	return Msg_ReadShort(sb) * (1.0 / 8.0);
}

/*
 * @brief
 */
void Msg_ReadPos(size_buf_t *sb, vec3_t pos) {
	pos[0] = Msg_ReadCoord(sb);
	pos[1] = Msg_ReadCoord(sb);
	pos[2] = Msg_ReadCoord(sb);
}

/*
 * @brief
 */
float Msg_ReadAngle(size_buf_t *sb) {
	return Msg_ReadChar(sb) * (360.0 / 255.0);
}

/*
 * @brief
 */
void Msg_ReadAngles(size_buf_t *sb, vec3_t angles) {
	angles[0] = Msg_ReadAngle(sb);
	angles[1] = Msg_ReadAngle(sb);
	angles[2] = Msg_ReadAngle(sb);
}

/*
 * @brief
 */
void Msg_ReadDeltaUsercmd(size_buf_t *sb, user_cmd_t *from, user_cmd_t *move) {
	int32_t bits;

	*move = *from;

	bits = Msg_ReadByte(sb);

	// read current angles
	if (bits & CMD_ANGLE1)
		move->angles[0] = Msg_ReadShort(sb);
	if (bits & CMD_ANGLE2)
		move->angles[1] = Msg_ReadShort(sb);
	if (bits & CMD_ANGLE3)
		move->angles[2] = Msg_ReadShort(sb);

	// read movement
	if (bits & CMD_FORWARD)
		move->forward = Msg_ReadShort(sb);
	if (bits & CMD_RIGHT)
		move->right = Msg_ReadShort(sb);
	if (bits & CMD_UP)
		move->up = Msg_ReadShort(sb);

	// read buttons
	if (bits & CMD_BUTTONS)
		move->buttons = Msg_ReadByte(sb);

	// read time to run command
	move->msec = Msg_ReadByte(sb);
}

/*
 * @brief
 */
void Sb_Init(size_buf_t *buf, byte *data, size_t length) {
	memset(buf, 0, sizeof(*buf));
	buf->data = data;
	buf->max_size = length;
}

/*
 * @brief
 */
void Sb_Clear(size_buf_t *buf) {
	buf->size = 0;
	buf->overflowed = false;
}

/*
 * @brief
 */
void *Sb_Alloc(size_buf_t *buf, size_t length) {
	void *data;

	if (buf->size + length > buf->max_size) {
		if (!buf->allow_overflow) {
			Com_Error(ERR_FATAL, "Overflow without allow_overflow set\n");
		}

		if (length > buf->max_size) {
			Com_Error(ERR_FATAL, "%zu is > full buffer size %zu\n", length, buf->max_size);
		}

		Com_Warn("Overflow\n");
		Sb_Clear(buf);
		buf->overflowed = true;
	}

	data = buf->data + buf->size;
	buf->size += length;

	return data;
}

/*
 * @brief
 */
void Sb_Write(size_buf_t *buf, const void *data, size_t len) {
	memcpy(Sb_Alloc(buf, len), data, len);
}

/*
 * @brief
 */
void Sb_Print(size_buf_t *buf, const char *data) {
	size_t len;

	len = strlen(data) + 1;

	if (buf->size) {
		if (buf->data[buf->size - 1])
			memcpy((byte *)Sb_Alloc(buf, len), data, len); // no trailing 0
		else
			memcpy((byte *)Sb_Alloc(buf, len - 1) - 1, data, len); // write over trailing 0
	} else
		memcpy((byte *)Sb_Alloc(buf, len), data, len);
}

/*
 * @brief Returns the command line argument count.
 */
int32_t Com_Argc(void) {
	return quake2world.argc;
}

/*
 * @brief Returns the command line argument at the specified index.
 */
char *Com_Argv(int32_t arg) {
	if (arg < 0 || arg >= Com_Argc())
		return "";
	return quake2world.argv[arg];
}

/*
 * @brief
 */
void Com_PrintInfo(const char *s) {
	char key[512];
	char value[512];
	char *o;
	int32_t l;

	if (*s == '\\')
		s++;
	while (*s) {
		o = key;
		while (*s && *s != '\\')
			*o++ = *s++;

		l = o - key;
		if (l < 20) {
			memset(o, ' ', 20 - l);
			key[20] = 0;
		} else
			*o = 0;
		Com_Print("%s", key);

		if (!*s) {
			Com_Print("MISSING VALUE\n");
			return;
		}

		o = value;
		s++;
		while (*s && *s != '\\')
			*o++ = *s++;
		*o = 0;

		if (*s)
			s++;
		Com_Print("%s\n", value);
	}
}
