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

#ifndef __COMMON_H__
#define __COMMON_H__

#include "shared.h"
#include "threads.h"

#define DEFAULT_GAME	"default"

#define MAX_PRINT_MSG	4096
#define MAX_NUM_ARGVS	64

// sizebuf and net message facilities
typedef struct size_buf_s {
	boolean_t allow_overflow;  // error if false and overflow occurs
	boolean_t overflowed;  // set to true when a write exceeds max_size
	byte *data;
	size_t max_size;
	size_t size;
	size_t read;
} size_buf_t;

void Sb_Init(size_buf_t *buf, byte *data, size_t length);
void Sb_Clear(size_buf_t *buf);
void *Sb_Alloc(size_buf_t *buf, size_t length);
void Sb_Write(size_buf_t *buf, const void *data, size_t len);
void Sb_Print(size_buf_t *buf, const char *data);

void Msg_WriteData(size_buf_t *sb, void *data, size_t len);
void Msg_WriteChar(size_buf_t *sb, int c);
void Msg_WriteByte(size_buf_t *sb, int c);
void Msg_WriteShort(size_buf_t *sb, int c);
void Msg_WriteLong(size_buf_t *sb, int c);
void Msg_WriteString(size_buf_t *sb, const char *s);
void Msg_WriteCoord(size_buf_t *sb, float f);
void Msg_WritePos(size_buf_t *sb, vec3_t pos);
void Msg_WriteAngle(size_buf_t *sb, float f);
void Msg_WriteAngles(size_buf_t *sb, vec3_t angles);
void Msg_WriteAngle16(size_buf_t *sb, float f);
void Msg_WriteDeltaUsercmd(size_buf_t *sb, struct user_cmd_s *from, struct user_cmd_s *cmd);
void Msg_WriteDeltaEntity(entity_state_t *from, entity_state_t *to, size_buf_t *msg, boolean_t force, boolean_t newentity);
void Msg_WriteDir(size_buf_t *sb, vec3_t vector);

void Msg_BeginReading(size_buf_t *sb);
void Msg_ReadData(size_buf_t *sb, void *data, size_t len);
int Msg_ReadChar(size_buf_t *sb);
int Msg_ReadByte(size_buf_t *sb);
int Msg_ReadShort(size_buf_t *sb);
int Msg_ReadLong(size_buf_t *sb);
char *Msg_ReadString(size_buf_t *sb);
char *Msg_ReadStringLine(size_buf_t *sb);
float Msg_ReadCoord(size_buf_t *sb);
void Msg_ReadPos(size_buf_t *sb, vec3_t pos);
float Msg_ReadAngle(size_buf_t *sb);
void Msg_ReadAngles(size_buf_t *sb, vec3_t angles);
float Msg_ReadAngle16(size_buf_t *sb);
void Msg_ReadDeltaUsercmd(size_buf_t *sb, struct user_cmd_s *from, struct user_cmd_s *cmd);
void Msg_ReadDeltaEntity(entity_state_t *from, entity_state_t *to, size_buf_t *msg, unsigned short bits, unsigned short number);
void Msg_ReadDir(size_buf_t *sb, vec3_t vector);


/*

PROTOCOL

*/


#define PROTOCOL 13  // unlucky

#define IP_MASTER "67.228.69.114"  // tastyspleen.net

#define PORT_MASTER	1996  // some good years
#define PORT_CLIENT	1997
#define PORT_SERVER	1998

#define UPDATE_BACKUP 1024  // copies of entity_state_t to keep buffered
#define UPDATE_MASK (UPDATE_BACKUP - 1)

// maximum number of entities we would ever reference in a single message
#define MAX_PACKET_ENTITIES 64

// per-client bandwidth throttle, in bytes per second
#define CLIENT_RATE_MIN 8192
#define CLIENT_RATE_MAX 32768
#define CLIENT_RATE 16384

// player_state_t communication

#define PS_M_TYPE			(1<<0)
#define PS_M_ORIGIN			(1<<1)
#define PS_M_VELOCITY		(1<<2)
#define PS_M_TIME			(1<<3)
#define PS_M_FLAGS			(1<<4)
#define PS_M_DELTA_ANGLES	(1<<5)
#define PS_VIEW_ANGLES		(1<<6)

// user_cmd_t communication

#define CMD_ANGLE1 	(1<<0)
#define CMD_ANGLE2 	(1<<1)
#define CMD_ANGLE3 	(1<<2)
#define CMD_FORWARD	(1<<3)
#define CMD_SIDE	(1<<4)
#define CMD_UP		(1<<5)
#define CMD_BUTTONS	(1<<6)


// a sound without an entity or position will be a local only sound
#define S_ATTEN		(1<<0)  // a byte
#define S_ORIGIN	(1<<1)  // three coordinates
#define S_ENTNUM	(1<<2)  // entity number


// entity_state_t communication

// This bit mask is packed into a short for each entity_state_t per frame.
// It describes which fields must be read to successfully parse the delta-
// compression.
#define U_ORIGIN		(1<<0)
#define U_OLD_ORIGIN	(1<<1)  // used by lightning
#define U_ANGLES		(1<<2)
#define U_ANIMATIONS	(1<<3)  // animation frames
#define U_EVENT			(1<<4)  // client side events
#define U_EFFECTS		(1<<5)  // client side effects
#define U_MODELS		(1<<6)  // models (primary and linked)
#define U_CLIENT		(1<<7)  // offset into client skins array
#define U_SOUND			(1<<8)  // looped sounds
#define U_SOLID			(1<<9)
#define U_REMOVE		(1<<10)  // remove this entity, don't add it

#define NUM_APPROXIMATE_NORMALS 162
extern const vec3_t approximate_normals[NUM_APPROXIMATE_NORMALS];

typedef enum {
	ERR_NONE,
	ERR_DROP,
	ERR_FATAL
} err_t;

int Com_Argc(void);
char *Com_Argv(int arg);  // range and null checked
void Com_ClearArgv(int arg);
void Com_InitArgv(int argc, char **argv);

void Com_PrintInfo(const char *s);

void Com_BeginRedirect(int target, char *buffer, int buffersize, void (*flush)(int, char*));
void Com_EndRedirect(void);
void Com_Debug(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
void Com_Error(err_t err, const char *fmt, ...) __attribute__((noreturn, format(printf, 2, 3)));
void Com_Print(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
void Com_Warn(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
void Com_Verbose(const char *fmt, ...) __attribute__((format(printf, 1, 2)));

// subsystems
#define Q2W_SERVER		0x1
#define Q2W_GAME		0x2
#define Q2W_CLIENT		0x4
#define Q2W_CGAME		0x8
#define Q2W_Q2WMAP		0x10

// global engine struct
typedef struct quake2world_s {

	int argc;
	char *argv[MAX_NUM_ARGVS + 1];

	unsigned int time;
	unsigned int subsystems;

	void (*Debug)(const char *msg);
	void (*Error)(err_t err, const char *msg) __attribute__((noreturn));
	void (*Print)(const char *msg);
	void (*Verbose)(const char *msg);
	void (*Warn)(const char *msg);

} quake2world_t;

extern quake2world_t quake2world;

unsigned int Com_WasInit(unsigned int s);
void Com_InitSubsystem(unsigned int s);
void Com_QuitSubsystem(unsigned int s);

extern cvar_t *dedicated;
extern cvar_t *time_demo;
extern cvar_t *time_scale;

#endif /* __COMMON_H__ */
