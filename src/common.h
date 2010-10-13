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
* Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
* 02111-1307, USA.
*/

#ifndef __COMMON_H__
#define __COMMON_H__

#include "shared.h"

#define BASEDIRNAME	"default"

#define MAX_PRINT_MSG 4096
#define MAX_NUM_ARGVS 64

// sizebuf and net message facilities
typedef struct sizebuf_s {
	qboolean allowoverflow;  // error if false and overflow occurs
	qboolean overflowed;  // set to true when a write excedes maxsize
	byte *data;
	size_t maxsize;
	size_t cursize;
	size_t readcount;
} sizebuf_t;

void Sb_Init(sizebuf_t *buf, byte *data, size_t length);
void Sb_Clear(sizebuf_t *buf);
void *Sb_GetSpace(sizebuf_t *buf, size_t length);
void Sb_Write(sizebuf_t *buf, const void *data, size_t length);
void Sb_Print(sizebuf_t *buf, const char *data);

struct usercmd_s;
struct entity_state_s;

void Msg_WriteChar(sizebuf_t *sb, int c);
void Msg_WriteByte(sizebuf_t *sb, int c);
void Msg_WriteShort(sizebuf_t *sb, int c);
void Msg_WriteLong(sizebuf_t *sb, int c);
void Msg_WriteFloat(sizebuf_t *sb, float f);
void Msg_WriteString(sizebuf_t *sb, const char *s);
void Msg_WriteCoord(sizebuf_t *sb, float f);
void Msg_WritePos(sizebuf_t *sb, vec3_t pos);
void Msg_WriteAngle(sizebuf_t *sb, float f);
void Msg_WriteAngle16(sizebuf_t *sb, float f);
void Msg_WriteDeltaUsercmd(sizebuf_t *sb, struct usercmd_s *from, struct usercmd_s *cmd);
void Msg_WriteDeltaEntity(struct entity_state_s *from, struct entity_state_s *to, sizebuf_t *msg, qboolean force, qboolean newentity);
void Msg_WriteDir(sizebuf_t *sb, vec3_t vector);

void Msg_BeginReading(sizebuf_t *sb);
int Msg_ReadChar(sizebuf_t *sb);
int Msg_ReadByte(sizebuf_t *sb);
int Msg_ReadShort(sizebuf_t *sb);
int Msg_ReadLong(sizebuf_t *sb);
float Msg_ReadFloat(sizebuf_t *sb);
char *Msg_ReadString(sizebuf_t *sb);
char *Msg_ReadStringLine(sizebuf_t *sb);
float Msg_ReadCoord(sizebuf_t *sb);
void Msg_ReadPos(sizebuf_t *sb, vec3_t pos);
float Msg_ReadAngle(sizebuf_t *sb);
float Msg_ReadAngle16(sizebuf_t *sb);
void Msg_ReadDeltaUsercmd(sizebuf_t *sb, struct usercmd_s *from, struct usercmd_s *cmd);
void Msg_ReadDir(sizebuf_t *sb, vec3_t vector);
void Msg_ReadData(sizebuf_t *sb, void *buffer, size_t size);


/*

PROTOCOL

*/


#define PROTOCOL 13  // unlucky

#define IP_MASTER "74.54.186.226"  // tastyspleen.net

#define PORT_MASTER	1996  // some good years
#define PORT_CLIENT	1997
#define PORT_SERVER	1998

#define UPDATE_BACKUP 128  // copies of entity_state_t to keep buffered
#define UPDATE_MASK (UPDATE_BACKUP - 1)


// the svc_strings[] array in cl_parse.c should mirror this

// server to client
enum svc_ops_e {
	svc_bad,
	svc_nop,
	svc_muzzleflash,
	svc_temp_entity,
	svc_layout,
	svc_disconnect,
	svc_reconnect,
	svc_sound,   // <see code>
	svc_print,   // [byte] id [string] null terminated string
	svc_stufftext,   // [string] stuffed into client's console buffer, should be \n terminated
	svc_serverdata,   // [long] protocol ...
	svc_configstring,   // [short] [string]
	svc_spawnbaseline,
	svc_centerprint,   // [string] to put in center of the screen
	svc_download,   // [short] size [size bytes]
	svc_frame,
	svc_zlib  // quake2world specific zlib compression command
};


// quake2world protocol extensions
#define QUAKE2WORLD_ZLIB 1

// client to server
enum clc_ops_e {
	clc_bad,
	clc_nop,
	clc_move,  // [[usercmd_t]
	clc_userinfo,  // [[userinfo string]
	clc_stringcmd  // [string] message
};


// plyer_state_t communication

#define PS_M_TYPE			(1<<0)
#define PS_M_ORIGIN			(1<<1)
#define PS_M_VELOCITY		(1<<2)
#define PS_M_TIME			(1<<3)
#define PS_M_FLAGS			(1<<4)
#define PS_M_DELTA_ANGLES	(1<<5)
#define PS_VIEWANGLES		(1<<6)

// user_cmd_t communication

#define CMD_ANGLE1 	(1<<0)
#define CMD_ANGLE2 	(1<<1)
#define CMD_ANGLE3 	(1<<2)
#define CMD_FORWARD	(1<<3)
#define CMD_SIDE	(1<<4)
#define CMD_UP		(1<<5)
#define CMD_BUTTONS	(1<<6)


// a sound without an ent or pos will be a local only sound
#define S_ATTEN		(1<<0)  // a byte
#define S_ORIGIN	(1<<1)  // three coordinates
#define S_ENTNUM	(1<<2)  // entity number

#define DEFAULT_SOUND_ATTENUATION	ATTN_NORM


// entity_state_t communication

// try to pack the common update flags into the first byte
#define U_ORIGIN1	(1<<0)
#define U_ORIGIN2	(1<<1)
#define U_ANGLE2	(1<<2)
#define U_ANGLE3	(1<<3)
#define U_FRAME		(1<<4)  // frame is a byte
#define U_EVENT		(1<<5)
#define U_REMOVE	(1<<6)  // REMOVE this entity, don't add it
#define U_MOREBITS1	(1<<7)  // read one additional byte

// second byte
#define U_NUMBER16	(1<<8)  // NUMBER8 is implicit if not set
#define U_ORIGIN3	(1<<9)
#define U_MODEL		(1<<10)
#define U_MODEL2	(1<<11)  // linked model
#define U_EFFECTS8	(1<<12)  // client side effects
#define U_EFFECTS16	(1<<13)
#define U_SOUND		(1<<14)
#define U_MOREBITS2	(1<<15)  // read one additional byte

// third byte
#define U_ANGLE1	(1<<16)
#define U_SKIN8		(1<<17)
#define U_SKIN16	(1<<18)
#define U_MODEL3	(1<<19)
#define U_MODEL4	(1<<20)
#define U_OLDORIGIN	(1<<21)  // used by lightning
#define U_SOLID		(1<<22)
#define U_MOREBITS3	(1<<23)  // read one additional byte

// fourth byte not presently used

#define NUMVERTEXNORMALS 162
extern const vec3_t bytedirs[NUMVERTEXNORMALS];

/*

MISC

*/

typedef enum {
	ERR_FATAL,
	ERR_DROP,
	ERR_NONE
} error_t;

int Com_Argc(void);
char *Com_Argv(int arg);  // range and null checked
void Com_ClearArgv(int arg);
void Com_InitArgv(int argc, char **argv);

void Com_PrintInfo(const char *s);

void Com_BeginRedirect(int target, char *buffer, int buffersize, void(*flush)(int, char*));
void Com_EndRedirect(void);
void Com_Debug(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
void Com_Error(error_t err, const char *fmt, ...) __attribute__((noreturn, format(printf, 2, 3)));
void Com_Print(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
void Com_Warn(const char *fmt, ...) __attribute__((format(printf, 1, 2)));

/*
 * Q2W engine globals.
 */
typedef struct quake2world_s {

	int argc;
	char *argv[MAX_NUM_ARGVS + 1];

	int time;
	int server_state;

	void (*Debug)(const char *msg);
	void (*Error)(error_t err, const char *msg) __attribute__((noreturn));
	void (*Print)(const char *msg);
	void (*Warn)(const char *msg);

} quake2world_t;

extern quake2world_t quake2world;

int Com_ServerState(void);
void Com_SetServerState(int state);

extern cvar_t *dedicated;
extern cvar_t *developer;
extern cvar_t *showtrace;
extern cvar_t *timedemo;
extern cvar_t *timescale;

#endif /* __COMMON_H__ */
