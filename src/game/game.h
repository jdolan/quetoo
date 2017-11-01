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

#pragma once

#include "shared.h"
#include "filesystem.h"
#include "ai/ai.h"

#define GAME_API_VERSION 8

/**
 * @brief Server flags for g_entity_t.
 */
#define SVF_NO_CLIENT 		(1 << 0) // don't send entity to clients
#define SVF_GAME			(1 << 1) // game may extend from here

/**
 * @brief Filter bits to Sv_BoxEntities / gi.BoxEntities.
 */
#define BOX_COLLIDE			(1 << 0) // SOLID_DEAD, SOLID_BOX, SOLID_BSP, ..
#define BOX_OCCUPY			(1 << 1) // SOLID_TRIGGER, SOLID_PROJECTILE, ..

#define BOX_ALL				(BOX_COLLIDE | BOX_OCCUPY)

#ifndef __GAME_LOCAL_H__

/**
 * @brief This is the server's definition of the client and entity structures. The
 * game module is free to add additional members to these structures, provided
 * they communicate the actual size of them at runtime through the game export
 * structure.
 */

typedef struct g_client_s g_client_t;
typedef struct g_entity_s g_entity_t;

typedef struct {
	void *opaque;
} g_client_locals_t;

typedef struct {
	void *opaque;
} g_entity_locals_t;

#endif /* __GAME_LOCAL_H__ */

struct g_client_s {
	/**
	 * @brief True if the client is a bot.
	 */
	_Bool ai;

	/**
	 * @brief True if the client's is connected.
	 */
	_Bool connected;

	/**
	 * @brief Communicated by server to clients
	 */
	player_state_t ps;

	/**
	 * @brief This player's ping
	 */
	uint32_t ping;

	/**
	 * @brief The game module can extend the client structure through this
	 * opaque field. Therefore, the actual size of g_client_t is returned to the
	 * server through ge.client_size.
	 */
	g_client_locals_t locals;
};

/**
 * @brief Entitys (or entities) are autonomous units of game interaction, such
 * as items, moving platforms, giblets and players. The game module and server
 * share a common base for this structure, but the game is free to extend it
 * through g_entity_locals_t.
 */
struct g_entity_s {
	/**
	 * @brief The class name provides basic identification and taxonomy for
	 * the entity. This is guaranteed to be set through G_Spawn.
	 */
	const char *class_name;

	/**
	 * @brief The model name for an entity (optional). For SOLID_BSP entities,
	 * this is the inline model name (e.g. "*1").
	 */
	const char *model;

	/**
	 * @brief The entity state is written by the game module and serialized
	 * using delta compression by the server.
	 */
	entity_state_t s;

	/**
	 * @brief True if the entity is currently allocated and active.
	 */
	_Bool in_use;

	/**
	 * @brief This is a special ID that is increased for every entity that spawns and
	 * wraps around. Because of certain game behaviors, the entity system may, in a very short
	 * period of time, delete and replace an entity without the knowledge of other systems (for instance
	 * a dropped item may get freed and replaced with a projectile in a single frame), and if those systems
	 * (such as the AI) hold a reference to that entity thinking it was still a dropped item, it
	 * may be catastrophic. This ID is a second line of defense, as if this ID changes, the entity
	 * is no longer the same entity it used to reference, and is more accurate than referencing classnames
	 */
	uint16_t spawn_id;

	/**
	 * @brief Server-specific flags bitmask (e.g. SVF_NO_CLIENT).
	 */
	uint32_t sv_flags;

	/**
	 * @brief The entity bounding box, set by the game, defines its relative
	 * bounds. These are typically populated in the entity's spawn function.
	 */
	vec3_t mins, maxs;

	/**
	 * @brief The entity bounding box, set by the server, in world space. These
	 * are populated by gi.LinkEntity / Sv_LinkEntity.
	 */
	vec3_t abs_mins, abs_maxs, size;

	/**
	 * @brief The solid type for the entity (e.g. SOLID_BOX) defines its
	 * clipping behavior and interactions with other entities.
	 */
	solid_t solid;

	/**
	 * @brief Sometimes it is useful for an entity to not be clipped against
	 * the entity that created it (for example, player projectiles).
	 */
	g_entity_t *owner;

	/**
	 * @brief Entities 1 through sv_max_clients->integer will have a valid
	 * pointer to the variable-sized g_client_t.
	 */
	g_client_t *client;

	/**
	 * @brief The game module can extend the entity structure through this
	 * opaque field. Therefore, the actual size of g_entity_t is returned to the
	 * server through ge.entity_size.
	 */
	g_entity_locals_t locals; // game-local data members
};

typedef _Bool (*EntityFilterFunc)(const g_entity_t *ent);

/**
 * @brief The game import provides engine functionality and core configuration
 * such as frame intervals to the game module.
 */
typedef struct g_import_s {

	/**
	 * @brief The game's write directory
	 */
	char write_dir[MAX_OS_PATH];

	void (*Print)(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
	void (*Debug_)(const debug_t debug, const char *func, const char *fmt, ...) __attribute__((format(printf, 3, 4)));
	void (*PmDebug_)(const char *func, const char *fmt, ...) __attribute__((format(printf, 2, 3)));
	void (*Warn_)(const char *func, const char *fmr, ...) __attribute__((format(printf, 2, 3)));
	void (*Error_)(const char *func, const char *fmt, ...) __attribute__((noreturn, format(printf, 2, 3)));

	/**
	 * @brief Memory management. The game module should use MEM_TAG_GAME and
	 * MEM_TAG_GAME_LEVEL for any allocations it makes.
	 */
	void *(*Malloc)(size_t size, mem_tag_t tag);
	void *(*LinkMalloc)(size_t size, void *parent);
	void (*Free)(void *p);
	void (*FreeTag)(mem_tag_t tag);

	/**
	 * @brief Filesystem interaction.
	 */
	int64_t (*LoadFile)(const char *file_name, void **buffer);
	void (*FreeFile)(void *buffer);

	/**
	 * @brief Enumerates files matching `pattern`, calling the given function.
	 * @param pattern A Unix glob style pattern.
	 * @param enumerator The enumerator function.
	 * @param data User data.
	 */
	void (*EnumerateFiles)(const char *pattern, Fs_EnumerateFunc enumerator, void *data);

	/**
	 * @brief Console variable and console command management.
	 */
	cvar_t *(*Cvar)(const char *name, const char *value, uint32_t flags, const char *desc);
	const char *(*CvarString)(const char *name);
	vec_t (*CvarValue)(const char *name);
	cvar_t *(*CvarGet)(const char *name);
	cvar_t *(*CvarSet)(const char *name, const char *string);
	cvar_t *(*CvarSetValue)(const char *name, vec_t value);
	cmd_t *(*Cmd)(const char *name, CmdExecuteFunc Execute, uint32_t flags, const char *desc);
	int32_t (*Argc)(void);
	const char *(*Argv)(int32_t arg);
	const char *(*Args)(void);
	void (*TokenizeString)(const char *text);

	/**
	 * @brief Console command buffer interaction.
	 */
	void (*Cbuf)(const char *text);

	/**
	 * @brief Configuration strings are used to transmit arbitrary tokens such
	 * as model names, skin names, team names and weather effects. See CS_GAME.
	 */
	void (*SetConfigString)(const uint16_t index, const char *string);
	const char *(*GetConfigString)(const uint16_t index);

	/**
	 * @brief Returns the configuration string index for the given asset,
	 * inserting it within the appropriate range if it is not present.
	 */
	uint16_t (*ModelIndex)(const char *name);
	uint16_t (*SoundIndex)(const char *name);
	uint16_t (*ImageIndex)(const char *name);

	/**
	 * @brief Set the model of a given entity by name. For inline BSP models,
	 * the bounding box is also set and the entity linked.
	 */
	void (*SetModel)(g_entity_t *ent, const char *name);

	/**
	 * @brief Sound sample playback dispatch.
	 *
	 * @param ent The entity originating the sound.
	 * @param index The configuration string index of the sound to be played.
	 * @param atten The sound attenuation constant (e.g. ATTEN_IDLE).
	 * @param pitch Pitch change, in tones x 2; 24 = +1 octave, 48 = +2 octave, etc.
	 */
	void (*Sound)(const g_entity_t *ent, const uint16_t index, const uint16_t atten, const int8_t pitch);

	/**
	 * @brief Sound sample playback dispatch for server-local entities, or
	 * sounds that do not originate from any specific entity.
	 *
	 * @param origin The origin of the sound.
	 * @param ent The entity originating the sound.
	 * @param index The configuration string index of the sound to be played.
	 * @param atten The sound attenuation constant (e.g. ATTEN_IDLE).
	 * @param pitch Pitch change, in tones x 2; 24 = +1 octave, 48 = +2 octave, etc.
	 */
	void (*PositionedSound)(const vec3_t origin, const g_entity_t *ent, const uint16_t index,
	                        const uint16_t atten, const int8_t pitch);

	/**
	 * @return The contents mask at the specific point. The point is tested
	 * against the world as well as all solid entities.
	 */
	int32_t (*PointContents)(const vec3_t point);

	/**
	 * @brief Collision detection. Traces between the two endpoints, impacting
	 * world and solid entity planes matching the specified contents mask.
	 *
	 * @param start The start point.
	 * @param end The end point.
	 * @param mins The bounding box mins (optional).
	 * @param maxs The bounding box maxs (optional).
	 * @param skip The entity to skip (e.g. self) (optional).
	 * @param contents The contents mask to intersect with (e.g. MASK_SOLID).
	 *
	 * @return The resulting trace. A fraction less than 1.0 indicates that
	 * the trace intersected a plane.
	 */
	cm_trace_t (*Trace)(const vec3_t start, const vec3_t end, const vec3_t mins, const vec3_t maxs,
	                    const g_entity_t *skip, const int32_t contents);

	/**
	 * @brief PVS and PHS query facilities, returning true if the two points
	 * can see or hear each other.
	 */
	_Bool (*inPVS)(const vec3_t p1, const vec3_t p2);
	_Bool (*inPHS)(const vec3_t p1, const vec3_t p2);

	/**
	 * @brief Area portal management, for doors and other entities that
	 * manipulate BSP visibility.
	 */
	void (*SetAreaPortalState)(int32_t portal_num, _Bool open);
	_Bool (*AreasConnected)(int32_t area1, int32_t area2);

	/**
	 * @brief All solid and trigger entities must be linked when they are
	 * initialized or moved. Linking resolves their absolute bounding box and
	 * makes them eligible for physics interactions.
	 */
	void (*LinkEntity)(g_entity_t *ent);

	/**
	 * @brief All entities should be unlinked before being freed.
	 */
	void (*UnlinkEntity)(g_entity_t *ent);

	/**
	 * @brief Populates a list of entities occupying the specified bounding
	 * box, filtered by the given type (BOX_SOLID, BOX_TRIGGER, ..).
	 *
	 * @param mins The area bounds in world space.
	 * @param maxs The area bounds in world space.
	 * @param list The list of entities to populate.
	 * @param len The maximum number of entities to return (lengthof(list)).
	 * @param type The entity type to return (BOX_SOLID, BOX_TRIGGER, ..).
	 *
	 * @return The number of entities found.
	 */
	size_t (*BoxEntities)(const vec3_t mins, const vec3_t maxs, g_entity_t **list, const size_t len,
	                      const uint32_t type);

	/**
	 * @brief Network messaging facilities.
	 */
	void (*Multicast)(const vec3_t org, multicast_t to, EntityFilterFunc filter);
	void (*Unicast)(const g_entity_t *ent, const _Bool reliable);
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

	/**
	 * @brief Network console IO.
	 */
	void (*BroadcastPrint)(const int32_t level, const char *fmt, ...) __attribute__((format(printf, 2, 3)));
	void (*ClientPrint)(const g_entity_t *ent, const int32_t level, const char *fmt, ...) __attribute__((format(printf, 3,
	        4)));

	/**
	 * @brief Load AI functions
	 */
	ai_export_t *(*LoadAi)(ai_import_t *import);
} g_import_t;

/**
 * @brief The game export structure exposes core game module entry points to
 * the server. The game must populate this structure as part of G_LoadGame.
 */
typedef struct g_export_s {
	/**
	 * @brief Game API version, in case the game module was compiled for a
	 * different version than the engine provides.
	 */
	uint16_t api_version;

	/**
	 * @brief Minor protocol version.
	 */
	uint16_t protocol;

	/**
	 * @brief The g_entity_t array, which must be allocated by the game due to
	 * the opaque nature of g_entity_t.
	 */
	struct g_entity_s *entities;

	/**
	 * @brief To be set to the size of g_entity_t so that the server can safely
	 * iterate the entities array.
	 */
	size_t entity_size;

	/**
	 * @brief The current number of allocated (in use) g_entity_t.
	 */
	uint16_t num_entities;

	/**
	 * @brief The total number of allocated g_entity_t (MAX_ENTITIES).
	 */
	uint16_t max_entities;

	/**
	 * @brief Called only when the game module is first loaded. Persistent
	 * structures for clients and game sate should be allocated here.
	 */
	void (*Init)(void);

	/**
	 * @brief Called when the game module is unloaded.
	 */
	void (*Shutdown)(void);

	/**
	 * @brief Called at the start of a new level.
	 */
	void (*SpawnEntities)(const char *name, const char *entities);

	/**
	 * @brief Called when a client connects with valid user information.
	 */
	_Bool (*ClientConnect)(g_entity_t *ent, char *user_info);

	/**
	 * @brief Called when a client has fully spawned and should begin thinking.
	 */
	void (*ClientBegin)(g_entity_t *ent);
	void (*ClientUserInfoChanged)(g_entity_t *ent, const char *user_info);
	void (*ClientDisconnect)(g_entity_t *ent);

	/**
	 * @brief Called when a client has issued a console command that could not
	 * be handled by the server directly (e.g. voting).
	 */
	void (*ClientCommand)(g_entity_t *ent);

	/**
	 * @brief Called when a client issues a movement command, which may include
	 * button actions such as attacking.
	 */
	void (*ClientThink)(g_entity_t *ent, pm_cmd_t *cmd);

	/**
	 * @brief Called every QUETOO_TICK_SECONDS to advance game logic.
	 */
	void (*Frame)(void);

	/**
	 * @brief Used to advertise the game name to server browsers.
	 */
	const char *(*GameName)(void);
} g_export_t;
