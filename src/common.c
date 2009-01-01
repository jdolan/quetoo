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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */

#include <setjmp.h>
#include <SDL/SDL_thread.h>

#include "common.h"

int com_argc;
char *com_argv[MAX_NUM_ARGVS + 1];

jmp_buf env;  // graceful rollback for ERR_DROP and ERR_NONE

static cvar_t *developer;
static cvar_t *showtrace;
cvar_t *timescale;
cvar_t *timedemo;
cvar_t *dedicated;

static int server_state;

// These functions need an implementation
void Con_Init(void);
void Con_Shutdown(void);
void Con_Print(const char *text);


/*
 *
 * CLIENT / SERVER interactions
 *
 */

static int rd_target;
static char *rd_buffer;
static int rd_buffersize;
static void	(*rd_flush)(int target, char *buffer);


/*
 * Com_BeginRedirect
 */
void Com_BeginRedirect(int target, char *buffer, int buffersize, void(*flush)(int, char*)){
	if(!target || !buffer || !buffersize || !flush)
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
void Com_EndRedirect(void){
	rd_flush(rd_target, rd_buffer);

	rd_target = 0;
	rd_buffer = NULL;
	rd_buffersize = 0;
	rd_flush = NULL;
}


/*
 * Com_Printf
 *
 * Both client and server can use this, and it will output to the apropriate place.
 */
void Com_Printf(const char *fmt, ...){
	va_list	argptr;
	char msg[MAX_PRINT_MSG];

	va_start(argptr, fmt);
	vsnprintf(msg, sizeof(msg), fmt, argptr);
	va_end(argptr);

	if(rd_target){  // handle redirection (rcon)
		if((strlen(msg) + strlen(rd_buffer)) > (rd_buffersize - 1)){
			rd_flush(rd_target, rd_buffer);
			*rd_buffer = 0;
		}
		strcat(rd_buffer, msg);
		return;
	}

	// Call console print function
	Con_Print(msg);
}


/*
 * Com_Dprintf
 *
 * A Com_Printf that only shows up if the "developer" cvar is set
 */
void Com_Dprintf(const char *fmt, ...){
	va_list	argptr;
	char msg[MAX_PRINT_MSG];

	if(!developer || !developer->value)
		return;

	va_start(argptr, fmt);
	vsnprintf(msg, sizeof(msg), fmt, argptr);
	va_end(argptr);

	// Call console print function
	Com_Printf("%s", msg);
}


/*
 * Com_Error
 *
 * Both client and server can use this, and it will do the apropriate things.
 */
void Com_Error(int code, const char *fmt, ...){
	va_list	argptr;
	static char msg[MAX_PRINT_MSG];

	va_start(argptr, fmt);
	vsnprintf(msg, sizeof(msg), fmt, argptr);
	va_end(argptr);

	switch(code){
		case ERR_NONE:
		case ERR_DROP:
			Com_Printf("^1%s\n", msg);

			Sv_Shutdown(msg, false);
			Cl_Drop();

			// roll back to last known good frame
			longjmp(env, -1);

			break;
		default:  // we're at ERR_FATAL, game will exit
			Sys_Error("%s", msg);
	}
}


/*
 * Com_Warn
 */
void Com_Warn(const char *fmt, ...){
	va_list	argptr;
	static char msg[MAX_PRINT_MSG];

	va_start(argptr, fmt);
	vsnprintf(msg, sizeof(msg), fmt, argptr);
	va_end(argptr);

	Com_Printf("^3%s", msg);
}


/*
 * Com_Quit
 *
 * Both client and server can use this, and it will do the apropriate things.
 */
void Com_Quit(void){

	Sv_Shutdown("Server quit.\n", false);
	Cl_Shutdown();

	Sys_Quit();
}


/*
 * Com_ServerState
 */
int Com_ServerState(void){
	return server_state;
}


/*
 * Com_SetServerState
 */
void Com_SetServerState(int state){
	server_state = state;
}


/*
 *
 * MESSAGE IO FUNCTIONS
 *
 * Handles byte ordering and avoids alignment errors
 */


const vec3_t bytedirs[NUMVERTEXNORMALS] = {
#include "anorms.h"
};


// writing functions

void Msg_WriteChar(sizebuf_t *sb, int c){
	byte *buf;

	buf = Sb_GetSpace(sb, 1);
	buf[0] = c;
}

void Msg_WriteByte(sizebuf_t *sb, int c){
	byte *buf;

	buf = Sb_GetSpace(sb, 1);
	buf[0] = c;
}

void Msg_WriteShort(sizebuf_t *sb, int c){
	byte *buf;

	buf = Sb_GetSpace(sb, 2);
	buf[0] = c & 0xff;
	buf[1] = c >> 8;
}

void Msg_WriteLong(sizebuf_t *sb, int c){
	byte *buf;

	buf = Sb_GetSpace(sb, 4);
	buf[0] = c & 0xff;
	buf[1] = (c >> 8) & 0xff;
	buf[2] = (c >> 16) & 0xff;
	buf[3] = c >> 24;
}

void Msg_WriteFloat(sizebuf_t *sb, float f){
	union {
		float f;
		int l;
	} dat;

	dat.f = f;
	dat.l = LittleLong(dat.l);

	Sb_Write(sb, &dat.l, 4);
}

void Msg_WriteString(sizebuf_t *sb, const char *s){
	if(!s)
		Sb_Write(sb, "", 1);
	else
		Sb_Write(sb, s, strlen(s) + 1);
}

void Msg_WriteCoord(sizebuf_t *sb, float f){
	Msg_WriteShort(sb, (int)(f * 8));
}

void Msg_WritePos(sizebuf_t *sb, vec3_t pos){
	Msg_WriteShort(sb, (int)(pos[0] * 8));
	Msg_WriteShort(sb, (int)(pos[1] * 8));
	Msg_WriteShort(sb, (int)(pos[2] * 8));
}

void Msg_WriteAngle(sizebuf_t *sb, float f){
	Msg_WriteByte(sb, (int)(f * 256 / 360) & 255);
}

void Msg_WriteAngle16(sizebuf_t *sb, float f){
	Msg_WriteShort(sb, ANGLE2SHORT(f));
}


void Msg_WriteDeltaUsercmd(sizebuf_t *buf, usercmd_t *from, usercmd_t *cmd){
	int bits;

	// send the movement message
	bits = 0;
	if(cmd->angles[0] != from->angles[0])
		bits |= CMD_ANGLE1;
	if(cmd->angles[1] != from->angles[1])
		bits |= CMD_ANGLE2;
	if(cmd->angles[2] != from->angles[2])
		bits |= CMD_ANGLE3;
	if(cmd->forwardmove != from->forwardmove)
		bits |= CMD_FORWARD;
	if(cmd->sidemove != from->sidemove)
		bits |= CMD_SIDE;
	if(cmd->upmove != from->upmove)
		bits |= CMD_UP;
	if(cmd->buttons != from->buttons)
		bits |= CMD_BUTTONS;

	Msg_WriteByte(buf, bits);

	if(bits & CMD_ANGLE1)
		Msg_WriteShort(buf, cmd->angles[0]);
	if(bits & CMD_ANGLE2)
		Msg_WriteShort(buf, cmd->angles[1]);
	if(bits & CMD_ANGLE3)
		Msg_WriteShort(buf, cmd->angles[2]);

	if(bits & CMD_FORWARD)
		Msg_WriteShort(buf, cmd->forwardmove);
	if(bits & CMD_SIDE)
		Msg_WriteShort(buf, cmd->sidemove);
	if(bits & CMD_UP)
		Msg_WriteShort(buf, cmd->upmove);

	if(bits & CMD_BUTTONS)
		Msg_WriteByte(buf, cmd->buttons);

	Msg_WriteByte(buf, cmd->msec);
}


void Msg_WriteDir(sizebuf_t *sb, vec3_t dir){
	int i, best;
	float d, bestd;

	if(!dir){
		Msg_WriteByte(sb, 0);
		return;
	}

	bestd = 0;
	best = 0;
	for(i = 0; i < NUMVERTEXNORMALS; i++){
		d = DotProduct(dir, bytedirs[i]);
		if(d > bestd){
			bestd = d;
			best = i;
		}
	}
	Msg_WriteByte(sb, best);
}


void Msg_ReadDir(sizebuf_t *sb, vec3_t dir){
	int b;

	b = Msg_ReadByte(sb);
	if(b >= NUMVERTEXNORMALS){
		Com_Error(ERR_DROP, "Msg_ReadDir: out of range");
	}
	VectorCopy(bytedirs[b], dir);
}


/*
 * Msg_WriteDeltaEntity
 *
 * Writes part of a packetentities message.
 * Can delta from either a baseline or a previous packet_entity
 */
void Msg_WriteDeltaEntity(entity_state_t *from, entity_state_t *to, sizebuf_t *msg, qboolean force, qboolean newentity){
	int bits;

	if(!to->number){
		Com_Error(ERR_FATAL, "Msg_WriteDeltaEntity: Unset entity number.");
	}
	if(to->number >= MAX_EDICTS){
		Com_Error(ERR_FATAL, "Msg_WriteDeltaEntity: Entity number >= MAX_EDICTS.");
	}

	// send an update
	bits = 0;

	if(to->number >= 256)
		bits |= U_NUMBER16;  // number8 is implicit otherwise

	if(to->origin[0] != from->origin[0])
		bits |= U_ORIGIN1;
	if(to->origin[1] != from->origin[1])
		bits |= U_ORIGIN2;
	if(to->origin[2] != from->origin[2])
		bits |= U_ORIGIN3;

	if(to->angles[0] != from->angles[0])
		bits |= U_ANGLE1;
	if(to->angles[1] != from->angles[1])
		bits |= U_ANGLE2;
	if(to->angles[2] != from->angles[2])
		bits |= U_ANGLE3;

	if(to->skinnum != from->skinnum){  // used for vwep and colors
		if(to->skinnum < 256)
			bits |= U_SKIN8;
		else bits |= U_SKIN16;
	}

	if(to->frame != from->frame)
		bits |= U_FRAME;

	if(to->effects != from->effects){
		if(to->effects < 256)
			bits |= U_EFFECTS8;
		else
			bits |= U_EFFECTS16;
	}

	if(to->solid != from->solid)
		bits |= U_SOLID;

	// event is not delta compressed, just 0 compressed
	if(to->event)
		bits |= U_EVENT;

	if(to->modelindex != from->modelindex)
		bits |= U_MODEL;
	if(to->modelindex2 != from->modelindex2)
		bits |= U_MODEL2;
	if(to->modelindex3 != from->modelindex3)
		bits |= U_MODEL3;
	if(to->modelindex4 != from->modelindex4)
		bits |= U_MODEL4;

	if(to->sound != from->sound)
		bits |= U_SOUND;

	if(newentity || !VectorCompare(from->old_origin, to->old_origin))
		bits |= U_OLDORIGIN;

	// write the message
	if(!bits && !force)
		return;  // nothing to send!

	if(bits & 0xff000000)
		bits |= U_MOREBITS3 | U_MOREBITS2 | U_MOREBITS1;
	else if(bits & 0x00ff0000)
		bits |= U_MOREBITS2 | U_MOREBITS1;
	else if(bits & 0x0000ff00)
		bits |= U_MOREBITS1;

	Msg_WriteByte(msg, bits & 255);

	if(bits & 0xff000000){
		Msg_WriteByte(msg, (bits >> 8) & 255);
		Msg_WriteByte(msg, (bits >> 16) & 255);
		Msg_WriteByte(msg, (bits >> 24) & 255);
	} else if(bits & 0x00ff0000){
		Msg_WriteByte(msg, (bits >> 8) & 255);
		Msg_WriteByte(msg, (bits >> 16) & 255);
	} else if(bits & 0x0000ff00){
		Msg_WriteByte(msg, (bits >> 8) & 255);
	}

	//----------

	if(bits & U_NUMBER16)
		Msg_WriteShort(msg, to->number);
	else
		Msg_WriteByte(msg, to->number);

	if(bits & U_MODEL)
		Msg_WriteByte(msg, to->modelindex);
	if(bits & U_MODEL2)
		Msg_WriteByte(msg, to->modelindex2);
	if(bits & U_MODEL3)
		Msg_WriteByte(msg, to->modelindex3);
	if(bits & U_MODEL4)
		Msg_WriteByte(msg, to->modelindex4);

	if(bits & U_FRAME)
		Msg_WriteByte(msg, to->frame);

	if(bits & U_SKIN8)
		Msg_WriteByte(msg, to->skinnum);
	else if(bits & U_SKIN16)
		Msg_WriteShort(msg, to->skinnum);

	if(bits & U_EFFECTS8)
		Msg_WriteByte(msg, to->effects);
	else if(bits & U_EFFECTS16)
		Msg_WriteShort(msg, to->effects);

	if(bits & U_ORIGIN1)
		Msg_WriteCoord(msg, to->origin[0]);
	if(bits & U_ORIGIN2)
		Msg_WriteCoord(msg, to->origin[1]);
	if(bits & U_ORIGIN3)
		Msg_WriteCoord(msg, to->origin[2]);

	if(bits & U_ANGLE1)
		Msg_WriteAngle(msg, to->angles[0]);
	if(bits & U_ANGLE2)
		Msg_WriteAngle(msg, to->angles[1]);
	if(bits & U_ANGLE3)
		Msg_WriteAngle(msg, to->angles[2]);

	if(bits & U_OLDORIGIN){
		Msg_WriteCoord(msg, to->old_origin[0]);
		Msg_WriteCoord(msg, to->old_origin[1]);
		Msg_WriteCoord(msg, to->old_origin[2]);
	}

	if(bits & U_SOUND)
		Msg_WriteByte(msg, to->sound);
	if(bits & U_EVENT)
		Msg_WriteByte(msg, to->event);
	if(bits & U_SOLID)
		Msg_WriteShort(msg, to->solid);
}



// reading functions

void Msg_BeginReading(sizebuf_t *msg){
	msg->readcount = 0;
}

// returns -1 if no more characters are available
int Msg_ReadChar(sizebuf_t *msg_read){
	int c;

	if(msg_read->readcount + 1 > msg_read->cursize)
		c = -1;
	else
		c = (signed char)msg_read->data[msg_read->readcount];
	msg_read->readcount++;

	return c;
}

int Msg_ReadByte(sizebuf_t *msg_read){
	int c;

	if(msg_read->readcount + 1 > msg_read->cursize)
		c = -1;
	else
		c = (unsigned char)msg_read->data[msg_read->readcount];
	msg_read->readcount++;

	return c;
}

int Msg_ReadShort(sizebuf_t *msg_read){
	int c;

	if(msg_read->readcount + 2 > msg_read->cursize)
		c = -1;
	else
		c = (short)(msg_read->data[msg_read->readcount]
			+ (msg_read->data[msg_read->readcount + 1] << 8));

	msg_read->readcount += 2;

	return c;
}

int Msg_ReadLong(sizebuf_t *msg_read){
	int c;

	if(msg_read->readcount + 4 > msg_read->cursize)
		c = -1;
	else
		c = msg_read->data[msg_read->readcount]
			+ (msg_read->data[msg_read->readcount + 1] << 8)
			+ (msg_read->data[msg_read->readcount + 2] << 16)
			+ (msg_read->data[msg_read->readcount + 3] << 24);

	msg_read->readcount += 4;

	return c;
}

float Msg_ReadFloat(sizebuf_t *msg_read){
	union {
		byte b[4];
		float f;
		int l;
	} dat;

	if(msg_read->readcount + 4 > msg_read->cursize)
		dat.f = -1;
	else {
		dat.b[0] = msg_read->data[msg_read->readcount];
		dat.b[1] = msg_read->data[msg_read->readcount + 1];
		dat.b[2] = msg_read->data[msg_read->readcount + 2];
		dat.b[3] = msg_read->data[msg_read->readcount + 3];
	}
	msg_read->readcount += 4;

	dat.l = LittleLong(dat.l);

	return dat.f;
}

char *Msg_ReadString(sizebuf_t *msg_read){
	static char string[2048];
	int l, c;

	l = 0;
	do {
		c = Msg_ReadChar(msg_read);
		if(c == -1 || c == 0)
			break;
		string[l] = c;
		l++;
	} while(l < sizeof(string) - 1);

	string[l] = 0;

	return string;
}

char *Msg_ReadStringLine(sizebuf_t *msg_read){
	static char string[2048];
	int l, c;

	l = 0;
	do {
		c = Msg_ReadChar(msg_read);
		if(c == -1 || c == 0 || c == '\n')
			break;
		string[l] = c;
		l++;
	} while(l < sizeof(string) - 1);

	string[l] = 0;

	return string;
}

float Msg_ReadCoord(sizebuf_t *msg_read){
	return Msg_ReadShort(msg_read) * (1.0 / 8);
}

void Msg_ReadPos(sizebuf_t *msg_read, vec3_t pos){
	pos[0] = Msg_ReadShort(msg_read) * (1.0 / 8);
	pos[1] = Msg_ReadShort(msg_read) * (1.0 / 8);
	pos[2] = Msg_ReadShort(msg_read) * (1.0 / 8);
}

float Msg_ReadAngle(sizebuf_t *msg_read){
	return Msg_ReadChar(msg_read) * (360.0 / 256);
}

float Msg_ReadAngle16(sizebuf_t *msg_read){
	return SHORT2ANGLE(Msg_ReadShort(msg_read));
}

void Msg_ReadDeltaUsercmd(sizebuf_t *msg_read, usercmd_t *from, usercmd_t *move){
	int bits;

	*move = *from;

	bits = Msg_ReadByte(msg_read);

	// read current angles
	if(bits & CMD_ANGLE1)
		move->angles[0] = Msg_ReadShort(msg_read);
	if(bits & CMD_ANGLE2)
		move->angles[1] = Msg_ReadShort(msg_read);
	if(bits & CMD_ANGLE3)
		move->angles[2] = Msg_ReadShort(msg_read);

	// read movement
	if(bits & CMD_FORWARD)
		move->forwardmove = Msg_ReadShort(msg_read);
	if(bits & CMD_SIDE)
		move->sidemove = Msg_ReadShort(msg_read);
	if(bits & CMD_UP)
		move->upmove = Msg_ReadShort(msg_read);

	// read buttons
	if(bits & CMD_BUTTONS)
		move->buttons = Msg_ReadByte(msg_read);

	// read time to run command
	move->msec = Msg_ReadByte(msg_read);
}


void Msg_ReadData(sizebuf_t *msg_read, void *data, size_t len){
	int i;

	for(i = 0; i < len; i++)
		((byte *)data)[i] = Msg_ReadByte(msg_read);
}

/*
 *
 *    Memory chunk management
 *
 */

void Sb_Init(sizebuf_t *buf, byte *data, size_t length){
	memset(buf, 0, sizeof(*buf));
	buf->data = data;
	buf->maxsize = length;
}

void Sb_Clear(sizebuf_t *buf){
	buf->cursize = 0;
	buf->overflowed = false;
}

void *Sb_GetSpace(sizebuf_t *buf, size_t length){
	void *data;

	if(buf->cursize + length > buf->maxsize){
		if(!buf->allowoverflow){
			Com_Error(ERR_FATAL, "Sb_GetSpace: Overflow without allowoverflow set.");
		}

		if(length > buf->maxsize){
			Com_Error(ERR_FATAL, "Sb_GetSpace: "Q2W_SIZE_T" is > full buffer size.", length);
		}

		Com_Warn("Sb_GetSpace: overflow.\n");
		Sb_Clear(buf);
		buf->overflowed = true;
	}

	data = buf->data + buf->cursize;
	buf->cursize += length;

	return data;
}

void Sb_Write(sizebuf_t *buf, const void *data, size_t length){
	memcpy(Sb_GetSpace(buf, length), data, length);
}

void Sb_Print(sizebuf_t *buf, const char *data){
	size_t len;

	len = strlen(data) + 1;

	if(buf->cursize){
		if(buf->data[buf->cursize - 1])
			memcpy((byte *)Sb_GetSpace(buf, len), data, len); // no trailing 0
		else
			memcpy((byte *)Sb_GetSpace(buf, len - 1) - 1, data, len); // write over trailing 0
	} else
		memcpy((byte *)Sb_GetSpace(buf, len), data, len);
}

int Com_Argc(void){
	return com_argc;
}

char *Com_Argv(int arg){
	if(arg < 0 || arg >= com_argc || !com_argv[arg])
		return "";
	return com_argv[arg];
}

void Com_ClearArgv(int arg){
	if(arg < 0 || arg >= com_argc || !com_argv[arg])
		return;
	com_argv[arg] = "";
}

void Com_InitArgv(int argc, char **argv){
	int i;

	if(argc > MAX_NUM_ARGVS)
		Com_Warn("Com_InitArgv: argc > MAX_NUM_ARGVS.");
	com_argc = argc;
	for(i = 0; i < argc && i < MAX_NUM_ARGVS; i++){
		if(!argv[i] || strlen(argv[i]) >= MAX_TOKEN_CHARS)
			com_argv[i] = "";
		else
			com_argv[i] = argv[i];
	}
}


char *CopyString(const char *in){
	char *out;

	out = Z_Malloc(strlen(in) + 1);
	strcpy(out, in);
	return out;
}


void Info_Print(const char *s){
	char key[512];
	char value[512];
	char *o;
	int l;

	if(*s == '\\')
		s++;
	while(*s){
		o = key;
		while(*s && *s != '\\')
			*o++ = *s++;

		l = o - key;
		if(l < 20){
			memset(o, ' ', 20 - l);
			key[20] = 0;
		} else
			*o = 0;
		Com_Printf("%s", key);

		if(!*s){
			Com_Printf("MISSING VALUE\n");
			return;
		}

		o = value;
		s++;
		while(*s && *s != '\\')
			*o++ = *s++;
		*o = 0;

		if(*s)
			s++;
		Com_Printf("%s\n", value);
	}
}


/*
 *
 *    ZONE MEMORY ALLOCATION
 *
 */

#define Z_MAGIC 0x1d1d

typedef struct zhead_s {
	struct zhead_s	*prev, *next;
	short magic;
	short tag;  // for group free
	size_t size;
} zhead_t;

static zhead_t z_chain;
static size_t z_count, z_bytes;

static SDL_mutex *z_lock;


/*
 * Z_Init
 */
void Z_Init(void){

	z_chain.next = z_chain.prev = &z_chain;

	z_lock = SDL_CreateMutex();
}

void Z_Shutdown(void){

	SDL_DestroyMutex(z_lock);
}


/*
 * Z_Free
 */
void Z_Free(void *ptr){
	zhead_t *z;

	z = ((zhead_t *)ptr) - 1;

	if(z->magic != Z_MAGIC){
		Com_Error(ERR_FATAL, "Z_Free: Bad magic for %p.", ptr);
	}

	SDL_mutexP(z_lock);

	z->prev->next = z->next;
	z->next->prev = z->prev;

	SDL_mutexV(z_lock);

	z_count--;
	z_bytes -= z->size;
	free(z);
}


/*
 * Z_Stats_f
 */
static void Z_Stats_f(void){
	Com_Printf("%zd bytes in "Q2W_SIZE_T" blocks\n", z_bytes, z_count);
}


/*
 * Z_FreeTags
 */
void Z_FreeTags(int tag){
	zhead_t *z, *next;

	for(z = z_chain.next; z != &z_chain; z = next){
		next = z->next;
		if(z->tag == tag)
			Z_Free((void *)(z + 1));
	}
}


/*
 * Z_TagMalloc
 */
void *Z_TagMalloc(size_t size, int tag){
	zhead_t *z;

	size = size + sizeof(zhead_t);
	z = malloc(size);
	if(!z){
		Com_Error(ERR_FATAL, "Z_TagMalloc: Failed to allocate "Q2W_SIZE_T" bytes.", size);
	}

	z_count++;
	z_bytes += size;

	memset(z, 0, size);
	z->magic = Z_MAGIC;
	z->tag = tag;
	z->size = size;

	SDL_mutexP(z_lock);

	z->next = z_chain.next;
	z->prev = &z_chain;
	z_chain.next->prev = z;
	z_chain.next = z;

	SDL_mutexV(z_lock);

	return (void *)(z + 1);
}


/*
 * Z_Malloc
 */
void *Z_Malloc(size_t size){
	return Z_TagMalloc(size, 0);
}


/*
 * Com_Init
 */
void Com_Init(int argc, char **argv){
	char *s;

	if(setjmp(env))
		Sys_Error("Error during initialization.");

	Z_Init();

	// prepare enough of the subsystems to handle
	// cvar and command buffer management
	Com_InitArgv(argc, argv);

	Swap_Init();

	Cbuf_Init();

	Cmd_Init();
	Cvar_Init();

	// we need to add the early commands twice, because
	// a basedir needs to be set before execing config files,
	// but we want other parms to override the settings of
	// the config files
	Cbuf_AddEarlyCommands(false);

	Fs_InitFilesystem();

	Cbuf_AddEarlyCommands(true);

	// init commands and vars
	Cmd_AddCommand("z_stats", Z_Stats_f, NULL);

	developer = Cvar_Get("developer", "0", 0, NULL);
	timedemo = Cvar_Get("timedemo", "0", 0, NULL);
	timescale = Cvar_Get("timescale", "1", 0, NULL);
	showtrace = Cvar_Get("showtrace", "0", 0, NULL);

#ifndef BUILD_CLIENT
	dedicated = Cvar_Get("dedicated", "1", CVAR_NOSET, NULL);
#else
	dedicated = Cvar_Get("dedicated", "0", CVAR_NOSET, NULL);
#endif

	// initialize console subsystem
	Con_Init();

	s = va("Quake2World %s %s %s", VERSION, __DATE__, BUILDHOST);
	Cvar_Get("version", s, CVAR_SERVERINFO | CVAR_NOSET, NULL);

	if(dedicated->value)
		Cmd_AddCommand("quit", Com_Quit, NULL);

	Net_Init();
	Netchan_Init();

	Sv_Init();
	Cl_Init();

	Com_Printf("Quake2World initialized.\n");

	// add + commands from command line
	Cbuf_AddLateCommands();

	// dedicated server, nothing specified, use fractures.bsp
	if(dedicated->value && !Com_ServerState()){
		Cbuf_AddText("map fractures\n");
		Cbuf_Execute();
	}
}


/*
 * Com_Frame
 */
void Com_Frame(int msec){
	extern int c_traces, c_brush_traces;
	extern int c_pointcontents;

	if(setjmp(env))
		return;  // an ERR_DROP or ERR_NONE was thrown

	if(timescale->value)
		msec = (int)((float)msec * timescale->value);

	if(showtrace->value){
		Com_Printf("%4i traces  %4i points\n", c_traces, c_pointcontents);
		c_traces = c_brush_traces = c_pointcontents = 0;
	}

	Cbuf_Execute();

	Sv_Frame(msec);

	Cl_Frame(msec);
}


/*
 * Com_Shutdown
 */
void Com_Shutdown(void){
	// shutdown the console subsystem
	Con_Shutdown();
}
