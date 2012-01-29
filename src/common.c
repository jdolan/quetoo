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

static int rd_target;
static char *rd_buffer;
static unsigned int rd_buffersize;
static void (*rd_flush)(int target, char *buffer);

/*
 * Com_BeginRedirect
 */
void Com_BeginRedirect(int target, char *buffer, int buffersize,
		void(*flush)(int, char*)) {

	if (!target || !buffer || !buffersize || !flush)
		return;

	rd_target = target;
	rd_buffer = buffer;
	rd_buffersize = buffersize;
	rd_flush = flush;

	*rd_buffer = 0;
}

/*
 * Com_EndRedirect
 */
void Com_EndRedirect(void) {
	rd_flush(rd_target, rd_buffer);

	rd_target = 0;
	rd_buffer = NULL;
	rd_buffersize = 0;
	rd_flush = NULL;
}

/*
 * Com_Debug
 */
void Com_Debug(const char *fmt, ...) {
	va_list args;
	char msg[MAX_PRINT_MSG];

	va_start(args, fmt);
	vsnprintf(msg, sizeof(msg), fmt, args);
	va_end(args);

	if (quake2world.Debug)
		quake2world.Debug((const char *) msg);
	else
		printf("%s", msg);
}

/*
 * Com_Error
 *
 * An error condition has occurred.  This function does not return.
 */
void Com_Error(err_t err, const char *fmt, ...) {
	va_list args;
	char msg[MAX_PRINT_MSG];

	va_start(args, fmt);
	vsnprintf(msg, sizeof(msg), fmt, args);
	va_end(args);

	if (quake2world.Error)
		quake2world.Error(err, (const char *) msg);
	else {
		fprintf(stderr, "%s", msg);
		exit(1);
	}
}

/*
 * Com_Print
 */
void Com_Print(const char *fmt, ...) {
	va_list args;
	char msg[MAX_PRINT_MSG];

	va_start(args, fmt);
	vsnprintf(msg, sizeof(msg), fmt, args);
	va_end(args);

	if (rd_target) { // handle redirection (rcon)
		if ((strlen(msg) + strlen(rd_buffer)) > (rd_buffersize - 1)) {
			rd_flush(rd_target, rd_buffer);
			*rd_buffer = 0;
		}
		strcat(rd_buffer, msg);
		return;
	}

	if (quake2world.Print)
		quake2world.Print((const char *) msg);
	else
		printf("%s", msg);
}

/*
 * Com_Warn
 */
void Com_Warn(const char *fmt, ...) {
	va_list args;
	static char msg[MAX_PRINT_MSG];

	va_start(args, fmt);
	vsnprintf(msg, sizeof(msg), fmt, args);
	va_end(args);

	if (quake2world.Warn)
		quake2world.Warn((const char *) msg);
	else
		fprintf(stderr, "%s", msg);
}

/*
 * Com_Verbose
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
 * Com_Subsystem
 */
unsigned int Com_WasInit(unsigned int s) {
	return quake2world.subsystems & s;
}

/*
 * Com_InitSubsystem
 */
void Com_InitSubsystem(unsigned int s) {
	quake2world.subsystems |= s;
}

/*
 * Com_QuitSubsystem
 */
void Com_QuitSubsystem(unsigned int s) {
	quake2world.subsystems &= ~s;
}

/*
 *
 * MESSAGE IO FUNCTIONS
 *
 * Handles byte ordering and avoids alignment errors.
 */

/*
 * Msg_WriteData
 */
void Msg_WriteData(size_buf_t *sb, void *data, size_t len) {
	Sb_Write(sb, data, len);
}

/*
 * Msg_WriteChar
 */
void Msg_WriteChar(size_buf_t *sb, int c) {
	byte *buf;

	buf = Sb_Alloc(sb, sizeof(char));
	buf[0] = c;
}

/*
 * Msg_WriteByte
 */
void Msg_WriteByte(size_buf_t *sb, int c) {
	byte *buf;

	buf = Sb_Alloc(sb, sizeof(byte));
	buf[0] = c;
}

/*
 * Msg_WriteShort
 */
void Msg_WriteShort(size_buf_t *sb, int c) {
	byte *buf;

	buf = Sb_Alloc(sb, sizeof(short));
	buf[0] = c & 0xff;
	buf[1] = c >> 8;
}

/*
 * Msg_WriteLong
 */
void Msg_WriteLong(size_buf_t *sb, int c) {
	byte *buf;

	buf = Sb_Alloc(sb, sizeof(int));
	buf[0] = c & 0xff;
	buf[1] = (c >> 8) & 0xff;
	buf[2] = (c >> 16) & 0xff;
	buf[3] = c >> 24;
}

/*
 * Msg_WriteString
 */
void Msg_WriteString(size_buf_t *sb, const char *s) {
	if (!s)
		Sb_Write(sb, "", 1);
	else
		Sb_Write(sb, s, strlen(s) + 1);
}

/*
 * Msg_WriteCoord
 */
void Msg_WriteCoord(size_buf_t *sb, float f) {
	Msg_WriteShort(sb, (int) (f * 8.0));
}

/*
 * Msg_WritePos
 */
void Msg_WritePos(size_buf_t *sb, vec3_t pos) {
	Msg_WriteShort(sb, (int) (pos[0] * 8.0));
	Msg_WriteShort(sb, (int) (pos[1] * 8.0));
	Msg_WriteShort(sb, (int) (pos[2] * 8.0));
}

/*
 * Msg_WriteAngle
 */
void Msg_WriteAngle(size_buf_t *sb, float f) {
	Msg_WriteByte(sb, (int) (f * 255.0 / 360.0) & 255);
}

/*
 * Msg_WriteAngles
 */
void Msg_WriteAngles(size_buf_t *sb, vec3_t angles) {
	Msg_WriteAngle(sb, angles[0]);
	Msg_WriteAngle(sb, angles[1]);
	Msg_WriteAngle(sb, angles[2]);
}

/*
 * Msg_WriteAngle16
 */
void Msg_WriteAngle16(size_buf_t *sb, float f) {
	Msg_WriteShort(sb, ANGLE2SHORT(f));
}

/*
 * Msg_WriteDeltaUsercmd
 */
void Msg_WriteDeltaUsercmd(size_buf_t *buf, user_cmd_t *from, user_cmd_t *cmd) {
	int bits;

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
	if (cmd->side != from->side)
		bits |= CMD_SIDE;
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
	if (bits & CMD_SIDE)
		Msg_WriteShort(buf, cmd->side);
	if (bits & CMD_UP)
		Msg_WriteShort(buf, cmd->up);

	if (bits & CMD_BUTTONS)
		Msg_WriteByte(buf, cmd->buttons);

	Msg_WriteByte(buf, cmd->msec);
}

/*
 * Msg_WriteDir
 */
void Msg_WriteDir(size_buf_t *sb, vec3_t dir) {
	int i, best;
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
 * Msg_ReadDir
 */
void Msg_ReadDir(size_buf_t *sb, vec3_t dir) {
	int b;

	b = Msg_ReadByte(sb);

	if (b >= NUM_APPROXIMATE_NORMALS) {
		Com_Error(ERR_DROP, "Msg_ReadDir: out of range.\n");
	}

	VectorCopy(approximate_normals[b], dir);
}

/*
 * Msg_WriteDeltaEntity
 *
 * Writes part of a packetentities message.
 * Can delta from either a baseline or a previous packet_entity
 */
void Msg_WriteDeltaEntity(entity_state_t *from, entity_state_t *to,
		size_buf_t *msg, boolean_t force, boolean_t is_new) {

	unsigned short bits = 0;

	if (to->number <= 0) {
		Com_Error(ERR_FATAL, "Msg_WriteDeltaEntity: Unset entity number.\n");
	}

	if (to->number >= MAX_EDICTS) {
		Com_Error(ERR_FATAL,
				"Msg_WriteDeltaEntity: Entity number >= MAX_EDICTS.\n");
	}

	if (!VectorCompare(to->origin, from->origin))
		bits |= U_ORIGIN;

	if (is_new || !VectorCompare(from->old_origin, to->old_origin))
		bits |= U_OLD_ORIGIN;

	if (!VectorCompare(to->angles, from->angles))
		bits |= U_ANGLES;

	if (to->animation1 != from->animation1 || to->animation2
			!= from->animation2)
		bits |= U_ANIMATIONS;

	if (to->event) // event is not delta compressed, just 0 compressed
		bits |= U_EVENT;

	if (to->effects != from->effects)
		bits |= U_EFFECTS;

	if (to->model1 != from->model1 || to->model2 != from->model2 || to->model3
			!= from->model3 || to->model4 != from->model4)
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
 * Msg_ReadDeltaEntity
 */
void Msg_ReadDeltaEntity(entity_state_t *from, entity_state_t *to,
		size_buf_t *msg, unsigned short number, unsigned short bits) {

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
 * Msg_BeginReading
 */
void Msg_BeginReading(size_buf_t *msg) {
	msg->read = 0;
}

/*
 * Msg_ReadData
 */
void Msg_ReadData(size_buf_t *sb, void *data, size_t len) {
	unsigned int i;

	for (i = 0; i < len; i++) {
		((byte *) data)[i] = Msg_ReadByte(sb);
	}
}

/*
 * Msg_ReadChar
 *
 * Returns -1 if no more characters are available.
 */
int Msg_ReadChar(size_buf_t *sb) {
	int c;

	if (sb->read + 1 > sb->size)
		c = -1;
	else
		c = (signed char) sb->data[sb->read];
	sb->read++;

	return c;
}

/*
 * Msg_ReadByte
 */
int Msg_ReadByte(size_buf_t *sb) {
	int c;

	if (sb->read + 1 > sb->size)
		c = -1;
	else
		c = (unsigned char) sb->data[sb->read];
	sb->read++;

	return c;
}

/*
 * Msg_ReadShort
 */
int Msg_ReadShort(size_buf_t *sb) {
	int c;

	if (sb->read + 2 > sb->size)
		c = -1;
	else
		c = (short) (sb->data[sb->read] + (sb->data[sb->read + 1] << 8));

	sb->read += 2;

	return c;
}

/*
 * Msg_ReadLong
 */
int Msg_ReadLong(size_buf_t *sb) {
	int c;

	if (sb->read + 4 > sb->size)
		c = -1;
	else
		c = sb->data[sb->read] + (sb->data[sb->read + 1] << 8)
				+ (sb->data[sb->read + 2] << 16) + (sb->data[sb->read + 3]
				<< 24);

	sb->read += 4;

	return c;
}

/*
 * Msg_ReadString
 */
char *Msg_ReadString(size_buf_t *sb) {
	static char string[MAX_STRING_CHARS];
	int c;
	unsigned int l;
	
	l = 0;
	do {
		c = Msg_ReadChar(sb);
		if (c == -1 || c == 0)
			break;
		string[l] = c;
		l++;
	} while (l < sizeof(string) - 1);

	string[l] = 0;

	return string;
}

/*
 * Msg_ReadStringLine
 */
char *Msg_ReadStringLine(size_buf_t *sb) {
	static char string[MAX_STRING_CHARS];
	int c;
	unsigned int l;

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
 * Msg_ReadCoord
 */
float Msg_ReadCoord(size_buf_t *sb) {
	return Msg_ReadShort(sb) * (1.0 / 8.0);
}

/*
 * Msg_ReadPos
 */
void Msg_ReadPos(size_buf_t *sb, vec3_t pos) {
	pos[0] = Msg_ReadCoord(sb);
	pos[1] = Msg_ReadCoord(sb);
	pos[2] = Msg_ReadCoord(sb);
}

/*
 * Msg_ReadAngle
 */
float Msg_ReadAngle(size_buf_t *sb) {
	return Msg_ReadChar(sb) * (360.0 / 255.0);
}

/*
 * Msg_ReadAngles
 */
void Msg_ReadAngles(size_buf_t *sb, vec3_t angles) {
	angles[0] = Msg_ReadAngle(sb);
	angles[1] = Msg_ReadAngle(sb);
	angles[2] = Msg_ReadAngle(sb);
}

/*
 * Msg_ReadAngle16
 */
float Msg_ReadAngle16(size_buf_t *sb) {
	return SHORT2ANGLE(Msg_ReadShort(sb));
}

/*
 * Msg_ReadDeltaUsercmd
 */
void Msg_ReadDeltaUsercmd(size_buf_t *sb, user_cmd_t *from, user_cmd_t *move) {
	int bits;

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
	if (bits & CMD_SIDE)
		move->side = Msg_ReadShort(sb);
	if (bits & CMD_UP)
		move->up = Msg_ReadShort(sb);

	// read buttons
	if (bits & CMD_BUTTONS)
		move->buttons = Msg_ReadByte(sb);

	// read time to run command
	move->msec = Msg_ReadByte(sb);
}

/*
 * Sb_Init
 */
void Sb_Init(size_buf_t *buf, byte *data, size_t length) {
	memset(buf, 0, sizeof(*buf));
	buf->data = data;
	buf->max_size = length;
}

/*
 * Sb_Clear
 */
void Sb_Clear(size_buf_t *buf) {
	buf->size = 0;
	buf->overflowed = false;
}

/*
 * Sb_Alloc
 */
void *Sb_Alloc(size_buf_t *buf, size_t length) {
	void *data;

	if (buf->size + length > buf->max_size) {
		if (!buf->allow_overflow) {
			Com_Error(ERR_FATAL,
					"Sb_GetSpace: Overflow without allow_overflow set.\n");
		}

		if (length > buf->max_size) {
			Com_Error(ERR_FATAL,
					"Sb_GetSpace: "Q2W_SIZE_T" is > full buffer size.\n",
					length);
		}

		Com_Warn("Sb_GetSpace: overflow.\n");
		Sb_Clear(buf);
		buf->overflowed = true;
	}

	data = buf->data + buf->size;
	buf->size += length;

	return data;
}

/*
 * Sb_Write
 */
void Sb_Write(size_buf_t *buf, const void *data, size_t len) {
	memcpy(Sb_Alloc(buf, len), data, len);
}

/*
 * Sb_Print
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
 * Com_Argc
 */
int Com_Argc(void) {
	return quake2world.argc;
}

/*
 * Com_Argv
 */
char *Com_Argv(int arg) {
	if (arg < 0 || arg >= Com_Argc() || !quake2world.argv[arg])
		return "";
	return quake2world.argv[arg];
}

/*
 * Com_ClearArgv
 */
void Com_ClearArgv(int arg) {
	if (arg < 0 || arg >= quake2world.argc || !quake2world.argv[arg])
		return;
	quake2world.argv[arg] = "";
}

/*
 * Com_InitArgv
 */
void Com_InitArgv(int argc, char **argv) {
	int i;

	if (argc > MAX_NUM_ARGVS)
		Com_Warn("Com_InitArgv: argc > MAX_NUM_ARGVS.");

	quake2world.argc = argc;

	for (i = 0; i < argc && i < MAX_NUM_ARGVS; i++) {
		if (!argv[i] || strlen(argv[i]) >= MAX_TOKEN_CHARS)
			quake2world.argv[i] = "";
		else
			quake2world.argv[i] = argv[i];
	}
}

/*
 * Com_PrintInfo
 */
void Com_PrintInfo(const char *s) {
	char key[512];
	char value[512];
	char *o;
	int l;

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
