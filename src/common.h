/*
* Copyright(c) 1997-2001 Id Software, Inc.
* Copyright(c) 2002 The Quakeforge Project.
* Copyright(c) 2006 Quake2World.
*
* This program is free software; you can redistribute it and/or
* modify it under the terms of the GNU General Public License
* as published by the Free Software Foundation; either version 2
* of the License, or(at your option) any later version.
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
#include "files.h"

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

// filesystem searching
const char *Sys_FindFirst(const char *path);
const char *Sys_FindNext(void);
void Sys_FindClose(void);

extern int curtime;  // time returned by last Sys_Milliseconds

int Sys_Milliseconds(void);
const char *Sys_GetCurrentUser(void);
void Sys_Mkdir(char *path);

// command parsing facilities
int Com_Argc(void);
char *Com_Argv(int arg);  // range and null checked
void Com_ClearArgv(int arg);
void Com_InitArgv(int argc, char **argv);

char *CopyString(const char *in);
void Info_Print(const char *s);


/*

PROTOCOL

*/


#define PROTOCOL 13  // unlucky

#define IP_MASTER "81.169.143.159"  // satgnu

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

/*

CMD

Command text buffering and command execution

*/

/*

Any number of commands can be added in a frame, from several different sources.
Most commands come from either keybindings or console line input, but remote
servers can also send across commands and entire text files can be execed.

The + command line options are also added to the command buffer.
*/

void Cbuf_Init(void);
// allocates an initial text buffer that will grow as needed

void Cbuf_AddText(const char *text);
// as new commands are generated from the console or keybindings,
// the text is added to the end of the command buffer.

void Cbuf_InsertText(const char *text);
// when a command wants to issue other commands immediately, the text is
// inserted at the beginning of the buffer, before any remaining unexecuted
// commands.

void Cbuf_AddEarlyCommands(qboolean clear);
// adds all the +set commands from the command line

void Cbuf_AddLateCommands(void);
// adds all the remaining + commands from the command line

void Cbuf_Execute(void);
// Pulls off \n terminated lines of text from the command buffer and sends
// them through Cmd_ExecuteString.  Stops when the buffer is empty.
// Normally called once per frame, but may be explicitly invoked.
// Do not call inside a command function!

void Cbuf_CopyToDefer(void);
void Cbuf_InsertFromDefer(void);
// These two functions are used to defer any pending commands while a map
// is being loaded


/*

Command execution takes a null terminated string, breaks it into tokens,
then searches for a command or variable that matches the first token.

*/

typedef void(*xcommand_t)(void);

void Cmd_Init(void);

void Cmd_AddCommand(const char *cmd_name, xcommand_t function, const char *description);
// called by the init functions of other parts of the program to
// register commands and functions to call for them.
// The cmd_name is referenced later, so it should not be in temp memory
// if function is NULL, the command will be forwarded to the server
// as a clc_stringcmd instead of executed locally
void Cmd_RemoveCommand(const char *cmd_name);

int Cmd_CompleteCommand(const char *partial, const char *matches[]);
// attempts to match a partial command for automatic command line completion

int Cmd_Argc(void);
char *Cmd_Argv(int arg);
char *Cmd_Args(void);
// The functions that execute commands get their parameters with these
// functions. Cmd_Argv() will return an empty string, not a NULL
// if arg > argc, so string operations are always safe.

void Cmd_TokenizeString(const char *text);
// Takes a null terminated string.  Does not need to be /n terminated.
// breaks the string up into arg tokens.

void Cmd_ExecuteString(const char *text);
// Parses a single line of text into arguments and tries to execute it
// as if it was typed at the console

void Cmd_ForwardToServer(void);
// adds the current command line as a clc_stringcmd to the client message.
// things like godmode, noclip, etc, are commands directed to the server,
// so when they are typed in at the console, they will need to be forwarded.


/*

CVAR

*/

/*

cvar_t variables are used to hold scalar or string variables that
can be changed or displayed at the console or prog code as well
as accessed directly in C code.

The user can access cvars from the console in three ways:
r_draworder			prints the current value
r_draworder 0		sets the current value to 0
set r_draworder 0	as above, but creates the cvar if not present

Cvars are restricted from having the same names as commands to keep this
interface from being ambiguous.
*/

extern cvar_t *cvar_vars;

void Cvar_LockCheatVars(qboolean lock);

cvar_t *Cvar_Get(const char *var_name, const char *value, int flags, const char *description);
// creates the variable if it doesn't exist, or returns the existing one
// if it exists, the value will not be changed, but flags will be ORed in
// that allows variables to be unarchived without needing bitflags

cvar_t *Cvar_Set(const char *var_name, const char *value);
// will create the variable if it doesn't exist

cvar_t *Cvar_ForceSet(const char *var_name, const char *value);
// will set the variable even if NOSET or LATCH

cvar_t *Cvar_FullSet(const char *var_name, const char *value, int flags);

void Cvar_SetValue(const char *var_name, float value);
// expands value to a string and calls Cvar_Set

float Cvar_VariableValue(const char *var_name);
// returns 0 if not defined or non numeric

char *Cvar_VariableString(const char *var_name);
// returns an empty string if not defined

int Cvar_CompleteVariable(const char *partial, const char *matches[]);
// attempts to match a partial variable name for command line completion

qboolean Cvar_PendingLatchedVars(void);
// are there pending latch changes?

void Cvar_UpdateLatchedVars(void);
// any CVAR_LATCHED variables that have been set will now take effect

qboolean Cvar_PendingVars(int flags);
// are there pending changes?

void Cvar_ClearVars(int flags);
// clear modified booleans on vars

qboolean Cvar_Command(void);
// called by Cmd_ExecuteString when Cmd_Argv(0) doesn't match a known
// command.  Returns true if the command was a variable reference that
// was handled.(print or change)

void Cvar_WriteVariables(const char *path);
// appends lines containing "set variable value" for all variables
// with the archive flag set to true.

void Cvar_Init(void);

char *Cvar_Userinfo(void);
// returns an info string containing all the CVAR_USERINFO cvars

char *Cvar_Serverinfo(void);
// returns an info string containing all the CVAR_SERVERINFO cvars

extern qboolean userinfo_modified;
// this is set each time a CVAR_USERINFO variable is changed
// so that the client knows to send it to the server

/*

NET

*/

#define PORT_ANY	-1

#define MAX_MSGLEN 	1400  // max length of a message

#define PACKET_HEADER 10  // two ints and a short

typedef enum {
	NA_LOOPBACK,
	NA_IP_BROADCAST,
	NA_IP
} netadrtype_t;

typedef enum {
	NS_CLIENT,
	NS_SERVER
} netsrc_t;

typedef struct {
	netadrtype_t type;
	byte ip[4];
	unsigned short port;
} netadr_t;

void Net_Init(void);
void Net_Shutdown(void);

void Net_Config(netsrc_t source, qboolean up);

qboolean Net_GetPacket(netsrc_t source, netadr_t *from, sizebuf_t *message);
void Net_SendPacket(netsrc_t source, size_t length, void *data, netadr_t to);

qboolean Net_CompareAdr(netadr_t a, netadr_t b);
qboolean Net_CompareBaseAdr(netadr_t a, netadr_t b);
qboolean Net_IsLocalAddress(netadr_t adr);
char *Net_AdrToString(netadr_t a);
qboolean Net_StringToAdr(const char *s, netadr_t *a);
void Net_Sleep(int msec);


typedef struct {
	qboolean fatal_error;

	netsrc_t source;

	int dropped;  // between last packet and previous

	int last_received;  // for timeouts
	int last_sent;  // for retransmits

	netadr_t remote_address;

	int qport;  // qport value to write when transmitting

	// sequencing variables
	int incoming_sequence;
	int incoming_acknowledged;
	int incoming_reliable_acknowledged;  // single bit

	int incoming_reliable_sequence;  // single bit, maintained local

	int outgoing_sequence;
	int reliable_sequence;  // single bit
	int last_reliable_sequence;  // sequence number of last send

	// reliable staging and holding areas
	sizebuf_t message;  // writing buffer to send to server
	byte message_buf[MAX_MSGLEN - 16];  // leave space for header

	// message is copied to this buffer when it is first transfered
	int reliable_length;
	byte reliable_buf[MAX_MSGLEN - 16];  // unacked reliable message
} netchan_t;

extern netadr_t net_from;
extern sizebuf_t net_message;
extern byte net_message_buffer[MAX_MSGLEN];

void Netchan_Init(void);
void Netchan_Setup(netsrc_t source, netchan_t *chan, netadr_t adr, int qport);
void Netchan_Transmit(netchan_t *chan, int length, byte *data);
void Netchan_OutOfBand(int net_socket, netadr_t adr, int length, byte *data);
void Netchan_OutOfBandPrint(int net_socket, netadr_t adr, const char *format, ...) __attribute__((format(printf, 3, 4)));
qboolean Netchan_Process(netchan_t *chan, sizebuf_t *msg);
qboolean Netchan_CanReliable(netchan_t *chan);
qboolean Netchan_NeedReliable(netchan_t *chan);


/*

CMODEL

*/

cmodel_t *Cm_LoadMap(const char *name, int *mapsize);
cmodel_t *Cm_InlineModel(const char *name);  // *1, *2, etc

int Cm_NumClusters(void);
int Cm_NumInlineModels(void);
char *Cm_EntityString(void);

// creates a clipping hull for an arbitrary box
int Cm_HeadnodeForBox(const vec3_t mins, const vec3_t maxs);

// returns an ORed contents mask
int Cm_PointContents(const vec3_t p, int headnode);
int Cm_TransformedPointContents(const vec3_t p, int headnode, const vec3_t origin, const vec3_t angles);

trace_t Cm_BoxTrace(const vec3_t start, const vec3_t end, const vec3_t mins, const vec3_t maxs,
		int headnode, int brushmask);
trace_t Cm_TransformedBoxTrace(const vec3_t start, const vec3_t end, const vec3_t mins, const vec3_t maxs,
		int headnode, int brushmask, const vec3_t origin, const vec3_t angles);

byte *Cm_ClusterPVS(int cluster);
byte *Cm_ClusterPHS(int cluster);

int Cm_PointLeafnum(const vec3_t p);

// call with topnode set to the headnode, returns with topnode
// set to the first node that splits the box
int Cm_BoxLeafnums(const vec3_t mins, const vec3_t maxs, int *list, int listsize, int *topnode);

int Cm_LeafContents(int leafnum);
int Cm_LeafCluster(int leafnum);
int Cm_LeafArea(int leafnum);

void Cm_SetAreaPortalState(int portalnum, qboolean open);
qboolean Cm_AreasConnected(int area1, int area2);

int Cm_WriteAreaBits(byte *buffer, int area);
qboolean Cm_HeadnodeVisible(int headnode, byte *visbits);

/*

PLAYER MOVEMENT CODE

Common between server and client so prediction matches

*/

extern float pm_airaccelerate;

void Pmove(pmove_t *pmove);

/*

FILESYSTEM

*/

void Fs_InitFilesystem(void);
void Fs_SetGamedir(const char *dir);
const char *Fs_Gamedir(void);
const char *Fs_NextPath(const char *prevpath);
const char *Fs_FindFirst(const char *path, qboolean fullpath);
void Fs_ExecAutoexec(void);
int Fs_OpenFile(const char *filename, FILE **file, filemode_t mode);
void Fs_CloseFile(FILE *f);
int Fs_LoadFile(const char *path, void **buffer);
void Fs_AddPakfile(const char *pakfile);
size_t Fs_Write(void *ptr, size_t size, size_t nmemb, FILE *stream);
size_t Fs_Read(void *ptr, size_t size, size_t nmemb, FILE *stream);

// a null buffer will just return the file length without loading
// a -1 length is not present
void Fs_ReadFile(void *buffer, int len, FILE *f);
void Fs_FreeFile(void *buffer);
void Fs_CreatePath(const char *path);
void Fs_GunzipFile(const char *path);

/*

MISC

*/


#define ERR_FATAL		0  // unsafe error condition, game exiting
#define ERR_DROP		1  // recoverable error condition
#define ERR_NONE		2  // just a break condition

#define EXEC_NOW	0  // don't return until completed
#define EXEC_INSERT	1  // insert at current position, but don't run yet
#define EXEC_APPEND	2  // add to end of the command buffer

void Com_BeginRedirect(int target, char *buffer, int buffersize, void(*flush)(int, char*));
void Com_EndRedirect(void);
void Com_Printf(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
void Com_Dprintf(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
void Com_Error(int code, const char *fmt, ...) __attribute__((noreturn, format(printf, 2, 3)));
void Com_Warn(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
void Com_Quit(void);

int Com_ServerState(void);
void Com_SetServerState(int state);

extern cvar_t *dedicated;
extern cvar_t *timedemo;

void Z_Init(void);
void Z_Shutdown(void);
void Z_Free(void *ptr);
void *Z_Malloc(size_t size);  // returns 0 filled memory
void *Z_TagMalloc(size_t size, int tag);
void Z_FreeTags(int tag);

void Com_Init(int argc, char **argv);
void Com_Frame(int msec);
void Com_Shutdown(void);

#define NUMVERTEXNORMALS 162
extern const vec3_t bytedirs[NUMVERTEXNORMALS];

/*

NON-PORTABLE SYSTEM SERVICES

*/

void Sys_Init(void);
void Sys_OpenLibrary(const char *name, void **handle);
void Sys_CloseLibrary(void **handle);
void *Sys_LoadGame(void *parms);
void Sys_UnloadGame(void);
void Sys_Backtrace(void);
void Sys_Error(const char *error, ...) __attribute__((noreturn, format(printf, 1, 2)));
void Sys_Quit(void);

/*

CLIENT / SERVER SYSTEMS

*/

void Cl_Init(void);
void Cl_Drop(void);
void Cl_Shutdown(void);
void Cl_Frame(int msec);
void Sv_Init(void);
void Sv_Shutdown(const char *finalmsg, qboolean reconnect);
void Sv_Frame(int msec);

#endif /* __COMMON_H__ */
