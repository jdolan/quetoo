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

#define AI_API_VERSION 4

/**
 * @brief Forward declaration of entity type, since
 * game.h requires ai.h
 */
typedef struct g_entity_s g_entity_t;

/**
 * @brief Items are opaque pointers to AI.
 */
typedef struct g_item_s g_item_t;

typedef _Bool (*EntityFilterFunc)(const g_entity_t *ent);

/**
 * @brief Functions and the like that the AI system imports from the game.
 */
typedef struct {
	/**
	 * @brief The AI's write directory
	 */
	char write_dir[MAX_OS_PATH];

	void (*Print)(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
	void (*Debug_)(debug_t debug, const char *func, const char *fmt, ...) __attribute__((format(printf, 3, 4)));
	void (*PmDebug_)(const char *func, const char *fmt, ...) __attribute__((format(printf, 2, 3)));
	void (*Warn_)(const char *func, const char *fmr, ...) __attribute__((format(printf, 2, 3)));
	void (*Error_)(const char *func, const char *fmt, ...) __attribute__((noreturn, format(printf, 2, 3)));

	/**
	 * @brief Memory management. The AI module should use MEM_TAG_AI for any allocations it makes.
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
	 * @return The contents mask at the specific point. The point is tested
	 * against the world as well as all solid entities.
	 */
	int32_t (*PointContents)(const vec3_t point);

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
	_Bool (*AreasConnected)(int32_t area1, int32_t area2);

	/**
	 * @brief Populates a list of entities occupying the specified bounding
	 * box, filtered by the given type (BOX_SOLID, BOX_TRIGGER, ..).
	 *
	 * @param mins The area bounds in world space.
	 * @param maxs The area bounds in world space.
	 * @param list The list of edicts to populate.
	 * @param len The maximum number of edicts to return (lengthof(list)).
	 * @param type The entity type to return (BOX_SOLID, BOX_TRIGGER, ..).
	 *
	 * @return The number of entities found.
	 */
	size_t (*BoxEntities)(const vec3_t mins, const vec3_t maxs, g_entity_t **list, const size_t len,
	                      const uint32_t type);

	/**
	 * @brief Configuration strings are used to transmit arbitrary tokens such
	 * as model names, skin names, team names and weather effects. See CS_GAME.
	 */
	const char *(*GetConfigString)(const uint16_t index);

	/**
	 * @brief Query if two entities are on the same team
	 */
	_Bool (*OnSameTeam)(const g_entity_t *self, const g_entity_t *other);

	/**
	 * @brief Called to issue a command from the bot
	 */
	void (*ClientCommand)(g_entity_t *self);

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
	 * @brief TODO FIXME - TEMPORARY API SCRATCH SPACE.
	 */
	const g_entity_t *entities;
	size_t entity_size;
	uint16_t (*ItemIndex)(const g_item_t *item);
	_Bool (*CanPickupItem)(const g_entity_t *self, const g_entity_t *other);
} ai_import_t;

/**
 * @brief Forward declaration of AI structs.
 */
typedef struct ai_item_s ai_item_t;
typedef struct ai_entity_data_s ai_entity_data_t;
typedef struct ai_client_data_s ai_client_data_t;

/**
 * @brief Functions and the like that the AI system exports to the game.
 */
typedef struct {
	int32_t	api_version;

	/**
	 * @brief Called when the game has restarted
	 */
	void (*GameRestarted)(void);

	/**
	 * @brief Initialize AI facilities. This is called by the server, do not call it.
	 */
	void (*Init)(void);

	/**
	 * @brief Shutdown AI facilities. This is called by the server, do not call it.
	 */
	void (*Shutdown)(void);

	/**
	 * @brief Called once by game to setup its data pointers
	 */
	void (*SetDataPointers)(ai_entity_data_t *entity, ai_client_data_t *client);

	/**
	 * @brief Run an AI frame.
	 */
	void (*Frame)(void);

	/**
	 * @brief Generate the user info for the specified bot.
	 */
	void (*GetUserInfo)(const g_entity_t *self, char *userinfo);

	/**
	 * @brief Called to setup an AI's initial state.
	 */
	void (*Begin)(g_entity_t *self);

	/**
	 * @brief Called when an AI spawns.
	 */
	void (*Spawn)(g_entity_t *self);

	/**
	 * @brief Call cause an AI to think. Returns the movement command for the bot.
	 */
	void (*Think)(g_entity_t *self, pm_cmd_t *cmd);

	/**
	 * @brief Register an item on the AI system
	 */
	void (*RegisterItem)(const uint16_t index, const ai_item_t *item);
} ai_export_t;
