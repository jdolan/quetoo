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

#include "shared/shared.h"
#include "ai/ai.h"
#include "collision/cm_types.h"

#define GAME_API_VERSION 13

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
	 * @brief The entity definition from the BSP file.
	 */
	const cm_entity_t *def;

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
	 * @brief Server-specific flags bitmask (e.g. SVF_NO_CLIENT).
	 */
	uint32_t sv_flags;

	/**
	 * @brief The entity bounding box, set by the game, defines its relative
	 * bounds. These are typically populated in the entity's spawn function.
	 */
	bounds_t bounds;

	/**
	 * @brief The entity bounding box, set by the server, in world space. These
	 * are populated by gi.LinkEntity / Sv_LinkEntity.
	 */
	bounds_t abs_bounds;
	
	/**
	 * @brief The entity size, set by the server. This
	 * is populated by gi.LinkEntity / Sv_LinkEntity.
	 */
	vec3_t size;

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
	g_entity_locals_t locals;
};

typedef _Bool (*EntityFilterFunc)(const g_entity_t *ent);

/**
 * @brief The game import provides engine functionality and core configuration
 * such as frame intervals to the game module.
 */
typedef struct g_import_s {

	/**
	 * @defgroup console-appending Console appending
	 * @{
	 */

	/**
	 * @brief Console logging facilities.
	 */
	void (*Print)(const char *fmt, ...) __attribute__((format(printf, 1, 2)));

	/**
	 * @return The active debug mask.
	 */
	debug_t (*DebugMask)(void);

	/**
	 * @brief Prints a formatted debug message to the configured consoles.
	 * @details If the proivided `debug` mask is inactive, the message will not be printed.
	 */
	void (*Debug_)(const debug_t debug, const char *func, const char *fmt, ...) __attribute__((format(printf, 3, 4)));

	/**
	 * @brief Prints a formatted warning message to the configured consoles.
	 */
	void (*Warn_)(const char *func, const char *fmr, ...) __attribute__((format(printf, 2, 3)));

	/**
	 * @brief Prints a formattet error message to the configured consoles.
	 */
	void (*Error_)(const char *func, const char *fmt, ...) __attribute__((noreturn, format(printf, 2, 3)));

	/**
	 * @}
	 * @defgroup memory-management Memory management
	 * @{
	 */

	/**
	 * @param tag The tag to associate the managed block with (e.g. MEM_TAG_GAME_LEVEL).
	 * @return A newly allocated block of managed memory under the given `tag`.
	 */
	void *(*Malloc)(size_t size, mem_tag_t tag);

	/**
	 * @return A newly allocated block of managed memory, linked to `parent`.
	 * @remarks The returned memory will be freed automatically when `parent` is freed.
	 */
	void *(*LinkMalloc)(size_t size, void *parent);

	/**
	 * @brief Frees the specified managed memory.
	 */
	void (*Free)(void *p);

	/**
	 * @brief Frees all managed memory allocated with the given `tag`.
	 */
	void (*FreeTag)(mem_tag_t tag);

	/**
	 * @}
	 * @defgroup filesystem Filesystem
	 * @{
	 */

	/**
	 * @brief Opens the specified file for reading.
	 * @param path The file path (e.g. `"maps/torn.bsp"`).
	 */
	file_t *(*OpenFile)(const char *path);

	/**
	 * @brief Seeks to the specified offset.
	 * @param file The file.
	 * @param offset The offset.
	 * @return True on success, false on error.
	 */
	_Bool (*SeekFile)(file_t *file, int64_t offset);

	/**
	 * @brief Reads from the specified file.
	 * @param file The file.
	 * @param buffer The buffer into which to read.
	 * @param size The size of the objects to read.
	 * @param count The count of the objects to read.
	 * @return The number of objects read, or -1 on failure.
	 */
	int64_t (*ReadFile)(file_t *file, void *buffer, size_t size, size_t count);

	/**
	 * @brief Opens the specified file for writing.
	 * @param path The file path (e.g. `"maps.ui.list"`).
	 */
	file_t *(*OpenFileWrite)(const char *path);

	/**
	 * @brief Writes `count` objects of size `size` from `buffer` to `file`.
	 * @param file The file.
	 * @param buffer The buffer to write from.
	 * @param size The size of the objects to write.
	 * @param count The count of the objecst to write.
	 * @return The number of objects written, or `-1` on error.
	 */
	int64_t (*WriteFile)(file_t *file, const void *buffer, size_t size, size_t count);

	/**
	 * @brief Closes the specified file.
	 * @param file The file.
	 * @return True on success, false on error.
	 */
	_Bool (*CloseFile)(file_t *file);

	/**
	 * @brief Check if a file exists or not.
	 * @return True if the specified filename exists on the search path.
	 */
	_Bool (*FileExists)(const char *path);

	/**
	 * @brief Loads the specified file into the given buffer.
	 * @param filename The game-relative filename.
	 * @param buffer The buffer to allocate and load.
	 * @return The length of the file in bytes.
	 */
	int64_t (*LoadFile)(const char *filename, void **buffer);

	/**
	 * @brief Frees a file loaded via `LoadFile`.
	 * @param buffer The buffer.
	 */
	void (*FreeFile)(void *buffer);

	/**
	 * @brief Creates the specified directory (and any ancestors) in the game's write directory.
	 * @param dir The directory name to create.
	 */
	_Bool (*Mkdir)(const char *dir);

	/**
	 * @return The real path name of the specified file or directory.
	 */
	const char *(*RealPath)(const char *path);

	/**
	 * @brief Enumerates files matching `pattern`, calling the given function.
	 * @param pattern A Unix glob style pattern.
	 * @param enumerator The enumerator function.
	 * @param data User data.
	 */
	void (*EnumerateFiles)(const char *pattern, Fs_Enumerator enumerator, void *data);

	/*
	 * @}
	 * @defgroup console-variables Console variables & commands
	 * @{
	 */

	/**
	 * @brief Resolves a console variable, creating it if not found.
	 * @param name The variable name.
	 * @param value The variable value string.
	 * @param flags The variable flags (e.g. CVAR_ARCHIVE).
	 * @param desc The variable description for builtin console help.
	 * @return The console variable.
	 */
	cvar_t *(*AddCvar)(const char *name, const char *value, uint32_t flags, const char *desc);

	/**
	 * @brief Resolves a console variable that is expected to be defined by the engine.
	 * @return The predefined console variable.
	 */
	cvar_t *(*GetCvar)(const char *name);

	/**
	 * @return The integer value of the console variable with the given name.
	 */
	int32_t (*GetCvarInteger)(const char *name);

	/**
	 * @return The string value of the console variable with the given name.
	 */
	const char *(*GetCvarString)(const char *name);

	/**
	 * @return The floating point value of the console variable with the given name.
	 */
	float (*GetCvarValue)(const char *name);

	/**
	 * @brief Sets the console variable by `name` to `value`.
	 */
	cvar_t *(*SetCvarInteger)(const char *name, int32_t value);

	/**
	 * @brief Sets the console variable by `name` to `string`.
	 */
	cvar_t *(*SetCvarString)(const char *name, const char *string);

	/**
	 * @brief Sets the console variable by `name` to `value`.
	 */
	cvar_t *(*SetCvarValue)(const char *name, float value);

	/**
	 * @brief Forces the console variable to take the value of the string immediately.
	 * @param name The variable name.
	 * @param string The variable string.
	 * @return The modified variable.
	 */
	cvar_t *(*ForceSetCvarString)(const char *name, const char *string);

	/**
	 * @brief Forces the console variable to take the given value immediately.
	 * @param name The variable name.
	 * @param value The variable value.
	 * @return The modified variable.
	 */
	cvar_t *(*ForceSetCvarValue)(const char *name, float value);

	/**
	 * @brief Toggles the console variable by `name`.
	 */
	cvar_t *(*ToggleCvar)(const char *name);

	/**
	 * @brief Registers and returns a console command.
	 * @param name The command name (e.g. `"wave"`).
	 * @param function The command function.
	 * @param flags The command flags (e.g. CMD_CGAME).
	 * @param desc The command description for builtin console help.
	 * @return The console command.
	 */
	cmd_t *(*AddCmd)(const char *name, CmdExecuteFunc function, uint32_t flags, const char *desc);

	/**
	 * @return The argument count for the currently executing command.
	 * @remarks This should only be called from within `CmdExecuteFunc`.
	 */
	int32_t (*Argc)(void);

	/**
	 * @return The nth argument for the currently executing command.
	 * @param arg The argument index. Pass `0` for the command name itself.
	 * @remarks This should only be called from within `CmdExecuteFunc`.
	 */
	const char *(*Argv)(int32_t arg);

	/**
	 * @return The arguments vector for the currently executing command.
	 * @remarks This should only be called from within `CmdExecuteFunc`.
	 */
	const char *(*Args)(void);

	/**
	 * @brief Tokenizes `text`, setting up the arguments vector for `CmdExecuteFunc`.
	 * @param text The user command to tokenize.
	 * @remarks This can be useful if dispatching commands to another subsystem (e.g. AI).
	 */
	void (*TokenizeString)(const char *text);

	/**
	 * @brief Appends `text` to the pending command buffer for execution.
	 * @param text The user command to execute.
	 * @remarks This can be useful for "command stuffing" client or server commands.
	 */
	void (*Cbuf)(const char *text);

	/**
	 * @}
	 * @defgroup configstrings Configuration strings
	 * @details Configuration strings are used to transmit arbitrary tokens such
	 * as model names, skin names, team names and weather effects. See CS_GAME.
	 * @{
	 */

	/**
	 * @brief Sets the configuration string at index to the specified string.
	 * @param index The index.
	 * @param string The string.
	 */
	void (*SetConfigString)(const uint16_t index, const char *string);

	/**
	 @param index The index.
	 @return The configuration string at `index`.
	 */
	const char *(*GetConfigString)(const uint16_t index);

	/**
	 * @brief Finds or inserts a string in the appropriate range for the given model name.
	 * @param name The asset name, e.g. `models/weapons/rocketlauncher/tris`.
	 * @return The configuration string index.
	 */
	uint16_t (*ModelIndex)(const char *name);

	/**
	 * @brief Finds or inserts a string in the appropriate range for the given sound name.
	 * @param name The asset name, e.g. `sounds/weapons/rocketlauncher/fire`.
	 * @return The configuration string index.
	 */
	uint16_t (*SoundIndex)(const char *name);

	/**
	 * @brief Finds or inserts a string in the appropriate range for the given image name.
	 * @param name The asset name, e.g. `pics/items/health_i`.
	 * @return The configuration string index.
	 */
	uint16_t (*ImageIndex)(const char *name);

	/**
	 * @}
	 */

	/**
	 * @brief Set the model of a given entity by name.
	 * @details For inline BSP models, the bounding box is also set and the entity linked.
	 */
	void (*SetModel)(g_entity_t *ent, const char *name);

	/**
	 * @brief Sound sample playback dispatch.
	 *
	 * @param ent The entity originating the sound.
	 * @param index The configuration string index of the sound to be played.
	 * @param atten The sound attenuation constant (e.g. SOUND_ATTEN_SQUARE).
	 * @param pitch Pitch change, in tones x 2; 24 = +1 octave, 48 = +2 octave, etc.
	 */
	void (*Sound)(const g_entity_t *ent, uint16_t index, sound_atten_t atten, int8_t pitch);

	/**
	 * @brief Sound sample playback dispatch for server-local entities, or
	 * sounds that do not originate from any specific entity.
	 *
	 * @param origin The origin of the sound (required).
	 * @param ent The entity originating the sound, `NULL` for worldspawn.
	 * @param index The configuration string index of the sound to be played.
	 * @param atten The sound attenuation constant (e.g. SOUND_ATTEN_SQUARE).
	 * @param pitch Pitch change, in tones x 2; 24 = +1 octave, 48 = +2 octave, etc.
	 */
	void (*PositionedSound)(const vec3_t origin, const g_entity_t *ent, uint16_t index, sound_atten_t atten, int8_t pitch);

	/**
	 * @defgroup collision Collision model
	 * @{
	 */

	/**
	 * @brief Finds the key-value pair for `key` within the specifed entity.
	 * @param entity The entity.
	 * @param key The entity key.
	 * @return The key-value pair for the specified key within entity.
	 * @remarks This function will always return non-NULL for convenience. Check the
	 * parsed types on the returned pair to differentiate "not present" from "0."
	 */
	const cm_entity_t *(*EntityValue)(const cm_entity_t *entity, const char *key);

	/**
	 * @brief Finds all brushes within the specified entity.
	 * @param entity The entity.
	 * @return A pointer array of brushes originally defined within `entity`.
	 * @remarks This function returns the brushes within an entity as it was defined
	 * in the source .map file. Even `func_group` and other entities which have their
	 * contents merged into `worldspawn` during the compilation step are fully supported.
	 */
	GPtrArray *(*EntityBrushes)(const cm_entity_t *entity);

	/**
	 * @return The contents mask at the specific point. The point is tested
	 * against the world as well as all solid entities.
	 */
	int32_t (*PointContents)(const vec3_t point);

	/**
	 * @return `true` if `point` resides inside `brush`, `false` otherwise.
	 * @param point The point to test.
	 * @param brush The brush to test against.
	 * @remarks This function is useful for testing points against non-solid brushes
	 * from brush entities. For general purpose collision detection, use PointContents.
	 */
	_Bool (*PointInsideBrush)(const vec3_t point, const cm_bsp_brush_t *brush);

	/**
	 * @brief Collision detection. Traces between the two endpoints, impacting
	 * world and solid entity planes matching the specified contents mask.
	 *
	 * @param start The start point.
	 * @param end The end point.
	 * @param bounds The bounding box mins (optional; Bounds_Zero() for a line trace).
	 * @param skip The entity to skip (e.g. self) (optional).
	 * @param contents The contents mask to intersect with (e.g. CONTENTS_MASK_SOLID).
	 *
	 * @return The resulting trace. A fraction less than 1.0 indicates that
	 * the trace intersected a plane.
	 */
	cm_trace_t (*Trace)(const vec3_t start, const vec3_t end, const bounds_t bounds,
	                    const g_entity_t *skip, const int32_t contents);

	/**
	 * @brief PVS and PHS query facilities, returning true if the two points
	 * can see or hear each other.
	 */
	_Bool (*inPVS)(const vec3_t p1, const vec3_t p2);
	_Bool (*inPHS)(const vec3_t p1, const vec3_t p2);

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
	 * @param bounds The box bounds in world space.
	 * @param list The list of entities to populate.
	 * @param len The maximum number of entities to return (lengthof(list)).
	 * @param type The entity type to return (BOX_SOLID, BOX_TRIGGER, ..).
	 *
	 * @return The number of entities found.
	 */
	size_t (*BoxEntities)(const bounds_t bounds, g_entity_t **list, const size_t len,
	                      const uint32_t type);

	/**
	 * @}
	 * @defgroup network Network messaging.
	 */

	void (*Multicast)(const vec3_t org, multicast_t to, EntityFilterFunc filter);
	void (*Unicast)(const g_entity_t *ent, const _Bool reliable);
	void (*WriteData)(const void *data, size_t len);
	void (*WriteChar)(const int32_t c);
	void (*WriteByte)(const int32_t c);
	void (*WriteShort)(const int32_t c);
	void (*WriteLong)(const int32_t c);
	void (*WriteString)(const char *s);
	void (*WriteVector)(const float v);
	void (*WritePosition)(const vec3_t pos);
	void (*WriteDir)(const vec3_t pos); // single byte encoded, very coarse
	void (*WriteAngle)(const float v);
	void (*WriteAngles)(const vec3_t angles);

	/**
	 * @brief Network console IO.
	 */
	void (*BroadcastPrint)(const int32_t level, const char *fmt, ...) __attribute__((format(printf, 2, 3)));
	void (*ClientPrint)(const g_entity_t *ent, const int32_t level, const char *fmt, ...) __attribute__((format(printf, 3,
	        4)));

	/**
	 * @}
	 */

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
	int32_t api_version;

	/**
	 * @brief Minor protocol version.
	 */
	int32_t protocol;

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
	int32_t num_entities;

	/**
	 * @brief The total number of allocated g_entity_t (MAX_ENTITIES).
	 */
	int32_t max_entities;

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
	void (*SpawnEntities)(const char *name, cm_entity_t *const *entities, size_t num_entities);

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
