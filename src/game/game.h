/*
 * Copyright(c) 1997-2001 id Software, Inc.
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

#include "shared.h"

#define GAME_API_VERSION 1

/*
 * @brief Server flags for g_edict_t.
 */
#define SVF_NO_CLIENT 		(1 << 0) // don't send entity to clients
#define SVF_DEAD_MONSTER	(1 << 1) // don't clip against corpses
#define SVF_GAME			(1 << 2) // game may extend from here

/*
 * @brief Filter bits to Sv_AreaEdicts / gi.AreaEdicts.
 */
#define AREA_SOLID			(1 << 0) // SOLID_BSP, SOLID_BOX, SOLID_MISSILE..
#define AREA_TRIGGER		(1 << 1) // SOLID_TRIGGER

/*
 * @brief The maximum number of clusters an entity may occupy before PVS
 * culling is skipped for it.
 */
#define MAX_ENT_CLUSTERS	16

/*
 * @brief A link binds an entity's position to the world for clipping. The
 * game module has visibility into this structure, but should treat it as
 * read-only.
 */
typedef struct {
	vec3_t abs_mins, abs_maxs, size;

	int32_t head_node;

	int32_t clusters[MAX_ENT_CLUSTERS];
	int32_t num_clusters; // if -1, use head_node

	int32_t areas[2];
	void *sector;
} g_link_t;

/*
 * This is the server's definition of the client and edict structures. The
 * game module is free to add additional members to these structures, provided
 * they communicate the actual size of them at runtime through the game export
 * structure.
 */

#ifndef __GAME_LOCAL_H__

typedef struct {
	void *opaque;
} g_client_locals_t;

typedef struct {
	void *opaque;
} g_edict_locals_t;

typedef struct g_client_s g_client_t;
typedef struct g_edict_s g_edict_t;

#endif /* __GAME_LOCAL_H__ */

struct g_client_s {
	player_state_t ps; // communicated by server to clients
	uint32_t ping;

	g_client_locals_t locals; // game-local data members
};

struct g_edict_s {
	const char *class_name;
	const char *model;

	// the entity state is delta-compressed based on what each client last
	// received for a given entity
	entity_state_t s;
	_Bool in_use;
	_Bool ai;

	uint32_t sv_flags; // SVF_NO_CLIENT, etc

	// the game should treat link members as read-only
	g_link_t link;

	// the following variables facilitate tracing and basic physics interactions
	vec3_t mins, maxs;
	solid_t solid;
	int32_t clip_mask; // e.g. MASK_SHOT, MASK_PLAYER_SOLID, ..
	g_edict_t *owner; // projectiles are not clipped against their owner

	// the client struct, as a pointer, because it is both optional and variable sized
	g_client_t *client;

	g_edict_locals_t locals; // game-local data members
};

// functions provided by the main engine
typedef struct {

	uint32_t frame_rate; // server frames per second
	uint32_t frame_millis;
	vec_t frame_seconds; // seconds per frame

	void (*Print)(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
	void (*Debug_)(const char *func, const char *fmt, ...) __attribute__((format(printf, 2, 3)));
	void (*Warn_)(const char *func, const char *fmr, ...) __attribute__((format(printf, 2, 3)));
	void (*Error_)(const char *func, const char *fmt, ...) __attribute__((noreturn, format(printf, 2, 3)));

	// zone memory management
	void *(*Malloc)(size_t size, mem_tag_t tag);
	void *(*LinkMalloc)(size_t size, void *parent);
	void (*Free)(void *p);
	void (*FreeTag)(mem_tag_t tag);

	// filesystem interaction
	int64_t (*LoadFile)(const char *file_name, void **buffer);
	void (*FreeFile)(void *buffer);

	// console variable and command interaction
	cvar_t *(*Cvar)(const char *name, const char *value, uint32_t flags, const char *desc);
	cmd_t *(*Cmd)(const char *name, CmdExecuteFunc Execute, uint32_t flags, const char *desc);
	int32_t (*Argc)(void);
	const char *(*Argv)(int32_t arg);
	const char *(*Args)(void);

	// command buffer interaction
	void (*AddCommandString)(const char *text);

	// config_strings are used to transmit arbitrary tokens such
	// as model names, skin names, team names, and weather effects
	void (*ConfigString)(const uint16_t index, const char *string);

	// create config_strings and some internal server state
	uint16_t (*ModelIndex)(const char *name);
	uint16_t (*SoundIndex)(const char *name);
	uint16_t (*ImageIndex)(const char *name);

	void (*SetModel)(g_edict_t *ent, const char *name);
	void (*Sound)(const g_edict_t *ent, const uint16_t index, const uint16_t atten);
	void (*PositionedSound)(const vec3_t origin, const g_edict_t *ent, const uint16_t index,
			const uint16_t atten);

	/*
	 * @brief Collision detection.
	 */
	int32_t (*PointContents)(const vec3_t point);
	cm_trace_t (*Trace)(const vec3_t start, const vec3_t end, const vec3_t mins, const vec3_t maxs,
			const g_edict_t *skip, const int32_t contents);

	/*
	 * @brief PVS and PHS query facilities.
	 */
	_Bool (*inPVS)(const vec3_t p1, const vec3_t p2);
	_Bool (*inPHS)(const vec3_t p1, const vec3_t p2);

	/*
	 * @brief Area portal management, for doors and other entities that
	 * manipulate BSP visibility.
	 */
	void (*SetAreaPortalState)(int32_t portal_num, _Bool open);
	_Bool (*AreasConnected)(int32_t area1, int32_t area2);

	/*
	 * @brief All solid and trigger entities should be linked when they are
	 * initialized or moved. Linking resolves their absolute bounding box and
	 * makes them eligible for physics interactions.
	 */
	void (*LinkEdict)(g_edict_t *ent);

	/*
	 * @brief All linked entities should be unlinked before being freed.
	 */
	void (*UnlinkEdict)(g_edict_t *ent);

	/*
	 * @brief Populates a list of entities occupying the specified bounding
	 * box, filtered by the given type (AREA_SOLID, AREA_TRIGGER, ..).
	 */
	size_t (*AreaEdicts)(const vec3_t mins, const vec3_t maxs, g_edict_t **list, const size_t len,
			const uint32_t type);

	// network messaging
	void (*Multicast)(const vec3_t origin, multicast_t to);
	void (*Unicast)(const g_edict_t *ent, const _Bool reliable);
	void (*WriteData)(const void *data, size_t len);
	void (*WriteChar)(const int32_t c);
	void (*WriteByte)(const int32_t c);
	void (*WriteShort)(const int32_t c);
	void (*WriteLong)(const int32_t c);
	void (*WriteString)(const char *s);
	void (*WriteVector)(const vec_t v);
	void (*WritePosition)(const vec3_t pos);
	void (*WriteDir)(const vec3_t pos); // single byte encoded, very coarse
	void (*WriteAngle)(const vec_t v);
	void (*WriteAngles)(const vec3_t angles);

	// network console IO
	void (*BroadcastPrint)(const int32_t level, const char *fmt, ...) __attribute__((format(printf, 2, 3)));
	void (*ClientPrint)(const g_edict_t *ent, const int32_t level, const char *fmt, ...) __attribute__((format(printf, 3, 4)));

} g_import_t;

// functions exported by the game subsystem
typedef struct {
	/*
	 * @brief Game API version, in case the game module was compiled for a
	 * different version than the engine provides.
	 */
	uint16_t api_version;

	/*
	 * @brief Minor protocol version.
	 */
	uint16_t protocol;

	/*
	 * @brief Called only when the game module is first loaded. Persistent
	 * structures for clients and game sate should be allocated here.
	 */
	void (*Init)(void);

	/*
	 * @brief Called when the game module is unloaded.
	 */
	void (*Shutdown)(void);

	/*
	 * @brief Called at the start of a new level.
	 */
	void (*SpawnEntities)(const char *name, const char *entities);

	/*
	 * @brief Called when a client connects with valid user information.
	 */
	_Bool (*ClientConnect)(g_edict_t *ent, char *user_info);

	/*
	 * @brief Called when a client has fully spawned and should begin thinking.
	 */
	void (*ClientBegin)(g_edict_t *ent);
	void (*ClientUserInfoChanged)(g_edict_t *ent, const char *user_info);
	void (*ClientDisconnect)(g_edict_t *ent);

	/*
	 * @brief Called when a client has issued a console command that could not
	 * be handled by the server directly (e.g. voting).
	 */
	void (*ClientCommand)(g_edict_t *ent);

	/*
	 * @brief Called when a client issues a movement command, which may include
	 * button actions such as attacking.
	 */
	void (*ClientThink)(g_edict_t *ent, user_cmd_t *cmd);

	/*
	 * @brief Called every gi.frame_seconds to advance game logic.
	 */
	void (*Frame)(void);

	/*
	 * @brief Used to advertise the game name to server browsers.
	 */
	const char *(*GameName)(void);

	/*
	 * @brief The g_edict_t array, which must be allocated by the game due to
	 * the opaque nature of g_edict_t.
	 */
	struct g_edict_s *edicts;

	/*
	 * @brief To be set to the size of g_edict_t so that the server can safely
	 * iterate the edicts array.
	 */
	size_t edict_size;

	/*
	 * @brief The current number of allocated (in use) g_edict_t.
	 */
	uint16_t num_edicts;

	/*
	 * @brief The total number of allocated g_edict_t (MAX_EDICTS).
	 */
	uint16_t max_edicts;
} g_export_t;

#endif /* __GAME_H__ */
