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

#ifndef __GAME_H__
#define __GAME_H__

#define GAME_API_VERSION 5

// edict->sv_flags
#define SVF_NOCLIENT 1  // don't send entity to clients

// edict->solid values
typedef enum {
	SOLID_NOT,   // no interaction with other objects
	SOLID_TRIGGER,   // only touch when inside, after moving
	SOLID_BBOX,   // touch on edge
	SOLID_MISSILE,  // touch on edge
	SOLID_BSP  // bsp clip, touch on edge
} solid_t;


// link_t is only used for entity area links now
typedef struct link_s {
	struct link_s *prev, *next;
} link_t;

#define MAX_ENT_CLUSTERS 16

typedef struct g_client_s g_client_t;  // typedef'ed here, defined below
typedef struct edict_s edict_t;  // OR in game module

#ifndef GAME_INCLUDE

struct g_client_s {
	player_state_t ps;  // communicated by server to clients
	int ping;
	// the game dll can add anything it wants after
	// this point in the structure
};

struct edict_s {
	entity_state_t s;
	struct g_client_s *client;

	qboolean in_use;
	int link_count;

	// FIXME: move these fields to a server private sv_entity_t
	link_t area;  // linked to a division node or leaf

	int num_clusters;  // if -1, use head_node instead
	int cluster_nums[MAX_ENT_CLUSTERS];
	int head_node;  // unused if num_clusters != -1
	int area_num, area_num2;

	int sv_flags;  // SVF_NOCLIENT, etc
	vec3_t mins, maxs;
	vec3_t abs_mins, abs_maxs, size;
	solid_t solid;
	int clipmask;
	edict_t *owner;

	// the game dll can add anything it wants after
	// this point in the structure
};

#endif  /* !GAME_INCLUDE */

// functions provided by the main engine
typedef struct g_import_s {

	int frame_rate;  // server frames per second
	float server_frame;  // seconds per frame

	void (*Print)(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
	void (*Debug)(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
	void (*BroadcastPrint)(int printlevel, const char *fmt, ...) __attribute__((format(printf, 2, 3)));
	void (*ClientPrint)(edict_t *ent, int printlevel, const char *fmt, ...) __attribute__((format(printf, 3, 4)));
	void (*ClientCenterPrint)(edict_t *ent, const char *fmt, ...) __attribute__((format(printf, 2, 3)));

	void (*Error)(const char *fmt, ...) __attribute__((format(printf, 1, 2)));

	// config_strings are used to transmit arbitrary tokens such
	// as model names, skin names, team names, and weather effects
	void (*ConfigString)(int num, const char *string);

	// create config_strings and some internal server state
	int (*ModelIndex)(const char *name);
	int (*SoundIndex)(const char *name);
	int (*ImageIndex)(const char *name);

	void (*SetModel)(edict_t *ent, const char *name);
	void (*Sound)(edict_t *ent, int soundindex, int atten);
	void (*PositionedSound)(vec3_t origin, edict_t *ent, int soundindex, int atten);

	// collision detection
	trace_t (*Trace)(vec3_t start, vec3_t mins, vec3_t maxs, vec3_t end, edict_t *passent, int contentmask);
	int (*PointContents)(vec3_t point);
	qboolean (*inPVS)(const vec3_t p1, const vec3_t p2);
	qboolean (*inPHS)(const vec3_t p1, const vec3_t p2);
	void (*SetAreaPortalState)(int portal_num, qboolean open);
	qboolean (*AreasConnected)(int area1, int area2);
	void (*Pmove)(pm_move_t *pmove);  // player movement code common with client prediction

	// an entity will never be sent to a client or used for collision
	// if it is not passed to linkentity.  if the size, position, or
	// solidity changes, it must be relinked.
	void (*LinkEntity)(edict_t *ent);
	void (*UnlinkEntity)(edict_t *ent);  // call before removing an interactive edict
	int (*BoxEdicts)(vec3_t mins, vec3_t maxs, edict_t **list, int maxcount, int areatype);

	// network messaging
	void (*Multicast)(vec3_t origin, multicast_t to);
	void (*Unicast)(edict_t *ent, qboolean reliable);
	void (*WriteChar)(int c);
	void (*WriteByte)(int c);
	void (*WriteShort)(int c);
	void (*WriteLong)(int c);
	void (*WriteFloat)(float f);
	void (*WriteString)(const char *s);
	void (*WritePosition)(vec3_t pos);  // some fractional bits
	void (*WriteDir)(vec3_t pos);  // single byte encoded, very coarse
	void (*WriteAngle)(float f);

	// managed memory allocation
	void *(*TagMalloc)(size_t size, int tag);
	void (*TagFree)(void *block);
	void (*FreeTags)(int tag);

	// filesystem interaction
	const char *(*Gamedir)(void);
	int (*OpenFile)(const char *file_name, FILE **file, file_mode_t mode);
	void (*CloseFile)(FILE *file);
	int (*LoadFile)(const char *file_name, void **buffer);

	// console variable interaction
	cvar_t *(*Cvar)(const char *var_name, const char *value, int flags, const char *description);

	// command function parameter access
	int (*Argc)(void);
	char *(*Argv)(int n);
	char *(*Args)(void);  // concatenation of all argv >= 1

	// add commands to the server console as if they were typed in
	// for map changing, etc
	void (*AddCommandString)(const char *text);
} g_import_t;

// functions exported by the game subsystem
typedef struct g_export_s {
	int api_version;

	// the init function will only be called when a game starts,
	// not each time a level is loaded.  Persistent data for clients
	// and the server can be allocated in init
	void (*Init)(void);
	void (*Shutdown)(void);

	// each new level entered will cause a call to SpawnEntities
	void (*SpawnEntities)(const char *name, const char *entities);

	qboolean (*ClientConnect)(edict_t *ent, char *user_info);
	void (*ClientBegin)(edict_t *ent);
	void (*ClientUserInfoChanged)(edict_t *ent, const char *user_info);
	void (*ClientDisconnect)(edict_t *ent);
	void (*ClientCommand)(edict_t *ent);
	void (*ClientThink)(edict_t *ent, user_cmd_t *cmd);

	void (*Frame)(void);

	// global variables shared between game and server

	// The edict array is allocated in the game dll so it
	// can vary in size from one game to another.
	//
	// The size will be fixed when ge->Init() is called
	struct edict_s *edicts;
	int edict_size;
	int num_edicts;  // current number, <= max_edicts
	int max_edicts;
} g_export_t;

// entry point in game module, called by the server
g_export_t *G_LoadGame(g_import_t *import);

#endif /* __GAME_H__ */
