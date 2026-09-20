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
#include "collision/cm_types.h"
#include <Objectively/Vector.h>

#define GAME_API_VERSION 43

/**
 * @brief Server flags for `GameEntity`.
 */
#define SVF_NO_CLIENT (1 << 0) // don't send entity to clients
#define SVF_GAME      (1 << 1) // game may extend from here

/**
 * @brief Filter bits to `Sv_BoxEntities` / `gi.BoxEntities`.
 */
#define BOX_COLLIDE   (1 << 0) // SOLID_DEAD, SOLID_BOX, SOLID_BSP, ..
#define BOX_OCCUPY    (1 << 1) // SOLID_TRIGGER, SOLID_PROJECTILE, ..

#define BOX_ALL       (BOX_COLLIDE | BOX_OCCUPY)

/**
 * @brief What the server reads out of a client and an entity, and the whole of
 * what it declares.
 * @details The server reads these fields at the offsets these declarations
 * produce. A game module extends a client and an entity by embedding the
 * matching type as the **first** member of its own `GameClient` and
 * `GameEntity`, so that the fields the server wants are always at offset zero
 * and always in this order. C guarantees that a pointer to a structure points
 * at its first member, which makes the two views of the same memory a fact of
 * the language rather than a rule a module has to remember: there is nothing a
 * module can write in its own structures that moves them.
 *
 * The module's declarations are therefore the only ones that name these types.
 * The server's own view is all it needs, so `GameClient` and `GameEntity` mean
 * these types outside a module and the module's extended ones inside it.
 */

typedef struct ServerGameClient ServerGameClient;
typedef struct ServerGameEntity ServerGameEntity;

#if defined(__G_LOCAL_H__)
typedef struct GameClient GameClient;
typedef struct GameEntity GameEntity;
#else
typedef ServerGameClient GameClient;
typedef ServerGameEntity GameEntity;
#endif

struct ServerGameClient {

  /**
   * @brief The entity bound to this client.
   */
  GameEntity *entity;

  /**
   * @brief Player state communicated to clients by the server.
   */
  PlayerState ps;

  /**
   * @brief Round-trip latency in milliseconds.
   */
  uint32_t ping;

  /**
   * @brief Current score (frags, points, etc.), updated each server frame.
   */
  int16_t score;

  /**
   * @brief Raw user info key-value string.
   */
  char userInfo[MAX_INFO_STRING_STRING];

  /**
   * @brief True if this client slot is currently active.
   */
  bool inUse;

  /**
   * @brief Non-null if this client is a bot.
   */
  struct Ai *ai;
};

/**
 * @brief Entities are autonomous units of game interaction, such
 * as items, moving platforms, giblets and players. The game module and server
 * share a common base for this structure, but the game is free to extend it.
 */
struct ServerGameEntity {

  /**
   * @brief Entity definition from the BSP file.
   */
  const CmEntity *def;

  /**
   * @brief Entity class name; guaranteed set through `G_Spawn`.
   */
  const char *classname;

  /**
   * @brief Model name; for `SOLID_BSP` entities this is the inline model name.
   */
  const char *model;

  /**
   * @brief Entity state written by the game and delta-compressed by the server.
   */
  EntityState s;

  /**
   * @brief True if the entity is currently allocated and active.
   */
  bool inUse;

  /**
   * @brief Server-specific flags bitmask (e.g. `SVF_NO_CLIENT`).
   */
  uint32_t svFlags;

  /**
   * @brief Game-set bounding box in entity-local space.
   */
  Box3 bounds;

  /**
   * @brief Server-set bounding box in world space; populated by `gi.LinkEntity`.
   */
  Box3 absBounds;

  /**
   * @brief Server-set entity size; populated by `gi.LinkEntity`.
   */
  Vec3 size;

  /**
   * @brief Solid type defining clipping behavior.
   */
  Solid solid;

  /**
   * @brief Entity that spawned this one; not clipped against its owner.
   */
  GameEntity *owner;

  /**
   * @brief Non-null for client entities 1..`sv_maxClients`.
   */
  GameClient *client;
};

/**
 * @brief A frag event accumulated during a match, submitted to the stats service at intermission.
 */
typedef struct {
  char     level[MAX_QPATH];
  char     attacker[MAX_QPATH];
  char     attackerGuid[MAX_QPATH];
  bool     attackerAi;
  char     target[MAX_QPATH];
  char     targetGuid[MAX_QPATH];
  bool     targetAi;
  char     weapon[MAX_QPATH];
  int32_t  mod;
  uint32_t time;
} GameFrag;

/**
 * @brief A capture event accumulated during a CTF match, submitted to the stats service at intermission.
 */
typedef struct {
  char     level[MAX_QPATH];
  char     player[MAX_QPATH];
  char     playerGuid[MAX_QPATH];
  bool     playerAi;
  char     team[MAX_QPATH];
  uint32_t time;
} GameCapture;

/**
 * @brief The game import provides engine functionality and core configuration
 * such as frame intervals to the game module.
 */
typedef struct {

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
  DebugFlags (*DebugMask)(void);

  /**
   * @brief Prints a formatted debug message to the configured consoles.
   * @details If the provided `debug` mask is inactive, the message will not be printed.
   */
  void (*Debug)(const DebugFlags debug, const char *func, const char *fmt, ...) __attribute__((format(printf, 3, 4)));

  /**
   * @brief Prints a formatted warning message to the configured consoles.
   */
  void (*Warn)(const char *func, const char *fmt, ...) __attribute__((format(printf, 2, 3)));

  /**
   * @brief Prints a formatted error message to the configured consoles.
   */
  void (*Error)(const char *func, const char *fmt, ...) __attribute__((noreturn, format(printf, 2, 3)));

  /**
   * @brief Captures a backtrace of the calling thread's stack, for diagnostic logging.
   * @param start How many innermost frames to skip (e.g. `1` to skip this call itself).
   * @param maxCount The maximum number of frames to include.
   * @return A heap-allocated string describing the stack; caller must `free()` it.
   */
  char *(*Backtrace)(uint32_t start, uint32_t maxCount);

  /**
   * @}
   * @defgroup memory-management Memory management
   * @{
   */

  /**
   * @param tag The tag to associate the managed block with (e.g. `MEM_TAG_GAME_LEVEL`).
   * @return A newly allocated block of managed memory under the given `tag`.
   */
  void *(*Malloc)(size_t size, MemTag tag);

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
  void (*FreeTag)(MemTag tag);

  /**
   * @}
   * @defgroup filesystem Filesystem
   * @{
   */

  /**
   * @brief Opens the specified file for reading.
   * @param path The file path (e.g. `"maps/torn.bsp"`).
   */
  File *(*OpenFile)(const char *path);

  /**
   * @brief Seeks to the specified offset.
   * @param file The file.
   * @param offset The offset.
   * @return True on success, false on error.
   */
  bool (*SeekFile)(File *file, int64_t offset);

  /**
   * @brief Reads from the specified file.
   * @param file The file.
   * @param buffer The buffer into which to read.
   * @param size The size of the objects to read.
   * @param count The count of the objects to read.
   * @return The number of objects read, or -1 on failure.
   */
  int64_t (*ReadFile)(File *file, void *buffer, size_t size, size_t count);

  /**
   * @brief Opens the specified file for writing.
   * @param path The file path (e.g. `"maps.ui.list"`).
   */
  File *(*OpenFileWrite)(const char *path);

  /**
   * @brief Writes `count` objects of size `size` from `buffer` to `file`.
   * @param file The file.
   * @param buffer The buffer to write from.
   * @param size The size of the objects to write.
   * @param count The count of the objects to write.
   * @return The number of objects written, or `-1` on error.
   */
  int64_t (*WriteFile)(File *file, const void *buffer, size_t size, size_t count);

  /**
   * @brief Closes the specified file.
   * @param file The file.
   * @return True on success, false on error.
   */
  bool (*CloseFile)(File *file);

  /**
   * @brief Check if a file exists or not.
   * @return True if the specified filename exists on the search path.
   */
  bool (*FileExists)(const char *path);

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
  bool (*Mkdir)(const char *dir);

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

  /**
   * @}
   * @defgroup console-variables Console variables & commands
   * @{
   */

  /**
   * @brief Resolves a console variable, creating it if not found.
   * @param name The variable name.
   * @param value The variable value string.
   * @param flags The variable flags (e.g. `CVAR_ARCHIVE`).
   * @param desc The variable description for builtin console help.
   * @return The console variable.
   */
  Cvar *(*AddCvar)(const char *name, const char *value, uint32_t flags, const char *desc);

  /**
   * @brief Resolves a console variable that is expected to be defined by the engine.
   * @return The predefined console variable.
   */
  Cvar *(*GetCvar)(const char *name);

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
  Cvar *(*SetCvarInteger)(const char *name, int32_t value);

  /**
   * @brief Sets the console variable by `name` to `string`.
   */
  Cvar *(*SetCvarString)(const char *name, const char *string);

  /**
   * @brief Sets the console variable by `name` to `value`.
   */
  Cvar *(*SetCvarValue)(const char *name, float value);

  /**
   * @brief Forces the console variable to take the value of the string immediately.
   * @param name The variable name.
   * @param string The variable string.
   * @return The modified variable.
   */
  Cvar *(*ForceSetCvarString)(const char *name, const char *string);

  /**
   * @brief Forces the console variable to take the given value immediately.
   * @param name The variable name.
   * @param value The variable value.
   * @return The modified variable.
   */
  Cvar *(*ForceSetCvarValue)(const char *name, float value);

  /**
   * @brief Toggles the console variable by `name`.
   */
  Cvar *(*ToggleCvar)(const char *name);

  /**
   * @brief Registers and returns a console command.
   * @param name The command name (e.g. `"wave"`).
   * @param function The command function.
   * @param flags The command flags (e.g. `CMD_CGAME`).
   * @param desc The command description for builtin console help.
   * @return The console command.
   */
  Cmd *(*AddCmd)(const char *name, CmdExecuteFunc function, uint32_t flags, const char *desc);

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
   * as model names, skin names, team names and weather effects. See `CS_GAME`.
   * @{
   */

  /**
   * @brief Sets the configuration string at index to the specified string.
   * @param index The index.
   * @param string The string.
   */
  void (*SetConfigString)(const int32_t index, const char *string);

  /**
   @param index The index.
   @return The configuration string at `index`.
   */
  const char *(*GetConfigString)(const int32_t index);

  /**
   * @brief Finds or inserts a string in the appropriate range for the given model name.
   * @param name The asset name, e.g. `models/weapons/rocketlauncher/tris`.
   * @return The configuration string index.
   */
  int32_t (*ModelIndex)(const char *name);

  /**
   * @brief Finds or inserts a string in the appropriate range for the given sound name.
   * @param name The asset name, e.g. `sounds/weapons/rocketlauncher/fire`.
   * @return The configuration string index.
   */
  int32_t (*SoundIndex)(const char *name);

  /**
   * @brief Finds or inserts a string in the appropriate range for the given image name.
   * @param name The asset name, e.g. `pics/items/health_i`.
   * @return The configuration string index.
   */
  int32_t (*ImageIndex)(const char *name);

  /**
   * @}
   * @defgroup collision Collision model
   * @{
   */

  /**
   * @return The BSP model for the currently loaded map.
   */
  const CmBsp *(*Bsp)(void);
  
  /**
   * @brief Returns the worldspawn entity definition.
   */
  const CmEntity *(*Worldspawn)(void);

  /**
   * @brief Finds the entity pair for `key` within the specifed entity.
   * @param entity The entity.
   * @param key The entity key.
   * @return The entity pair for the specified key within entity.
   * @remarks This function will always return non-`NULL` for convenience. Check the
   * parsed types on the returned pair to differentiate "not present" from "0."
   */
  const CmEntity *(*EntityValue)(const CmEntity *entity, const char *key);

  /**
   * @brief Finds all brushes within the specified entity.
   * @param entity The entity.
   * @return A pointer array of brushes originally defined within `entity`.
   * @remarks This function returns the brushes within an entity as it was defined
   * in the source .map file. Even `func_group` and other entities which have their
   * contents merged into `worldspawn` during the compilation step are fully supported.
   */
  Vector *(*EntityBrushes)(const CmEntity *entity);

  /**
   * @brief Parses a string of brace-delimited key-value entity definitions, the
   * format of a map's entity string and of `maps.lst`.
   * @return A list of `CmEntity *`, each to be freed with `FreeEntity`.
   */
  List *(*LoadEntities)(const char *entityString);

  /**
   * @brief Frees an entity definition from `LoadEntities`.
   */
  void (*FreeEntity)(CmEntity *entity);

  /**
   * @brief Returns the server's map rotation, as configured by `sv_mapList`.
   * @return A list of `CmEntity *`, each to be freed with `FreeEntity`, or `NULL`
   * if no rotation is configured.
   * @remarks The list is a copy, so a `sv_mapList` edit can not free entries from
   * underneath the caller.
   */
  List *(*MapList)(void);

  /**
   * @return The index in `MapList` the running level was served from, or `-1` if it
   * was not served from the rotation.
   * @remarks This is what identifies the level when a rotation names the same map
   * twice, which its name can not.
   */
  int32_t (*MapIndex)(void);

  /**
   * @brief Chooses the entry of `MapList` that the next `nextMap` serves, in place of
   * the rotation's own pick.
   * @param index The index in `MapList`.
   * @remarks An index rather than a name, so that a rotation naming the same map twice
   * serves, and resumes from, the occurrence that was actually chosen. The override is
   * consumed by that one map change, so that it can not survive to decide a later one.
   */
  void (*SetNextMap)(int32_t index);

  /**
   * @return The contents mask at the specific point. The point is tested
   * against the world as well as all solid entities.
   */
  int32_t (*PointContents)(const Vec3 point);

  /**
   * @return The contents mask of all leafs within bounds. The box is tested
   * against the world as well as all solid entities.
   */
  int32_t (*BoxContents)(const Box3 bounds);

  /**
   * @return `true` if `point` resides inside `brush`, `false` otherwise.
   * @param point The point to test.
   * @param brush The brush to test against.
   * @remarks This function is useful for testing points against non-solid brushes
   * from brush entities. For general purpose collision detection, use PointContents.
   */
  bool (*PointInsideBrush)(const Vec3 point, const CmBspBrush *brush);

  /**
   * @brief Collision detection. Traces between the two endpoints, impacting
   * world and solid entity planes matching the specified contents mask.
   *
   * @param start The start point.
   * @param end The end point.
   * @param bounds The bounding box mins (optional; `Box3_Zero()` for a line trace).
   * @param skip The entity to skip (e.g. self) (optional).
   * @param contents The contents mask to intersect with (e.g. `CONTENTS_MASK_SOLID`).
   *
   * @return The resulting trace. A fraction less than 1.0 indicates that
   * the trace intersected a plane.
   */
  CmTrace (*Trace)(const Vec3 start, const Vec3 end, const Box3 bounds, const GameEntity *skip, int32_t contents);

  /**
   * @brief Collision detection. Traces between the two endpoints, impacting
   * the specified entity's planes matching the specified contents mask.
   *
   * @param start The start point.
   * @param end The end point.
   * @param bounds The bounding box mins (optional; `Box3_Zero()` for a line trace).
   * @param ent The entity to clip against.
   * @param contents The contents mask to intersect with (e.g. `CONTENTS_MASK_SOLID`).
   *
   * @return The resulting trace. A fraction less than 1.0 indicates that
   * the trace intersected a plane.
   */
  CmTrace (*Clip)(const Vec3 start, const Vec3 end, const Box3 bounds, const GameEntity *ent, int32_t contents);

  /**
   * @brief Set the model of a given entity by name.
   * @details For inline BSP models, the bounding box is also set and the entity linked.
   */
  void (*SetModel)(GameEntity *ent, const char *name);

  /**
   * @brief All solid and trigger entities must be linked when they are
   * initialized or moved. Linking resolves their absolute bounding box and
   * makes them eligible for physics interactions.
   */
  void (*LinkEntity)(GameEntity *ent);

  /**
   * @brief All entities should be unlinked before being freed.
   */
  void (*UnlinkEntity)(GameEntity *ent);

  /**
   * @brief Populates a list of entities occupying the specified bounding
   * box, filtered by the given type (`BOX_SOLID`, `BOX_TRIGGER`, ..).
   *
   * @param bounds The box bounds in world space.
   * @param list The list of entities to populate.
   * @param len The maximum number of entities to return (lengthof(list)).
   * @param type The entity type to return (`BOX_SOLID`, `BOX_TRIGGER`, ..).
   *
   * @return The number of entities found.
   */
  size_t (*BoxEntities)(const Box3 bounds, GameEntity **list, const size_t len, uint32_t type);

  /**
   * @}
   * @defgroup network Network messaging.
   */

  void (*Multicast)(const Vec3 org, Multicast to);
  void (*Unicast)(const GameClient *ent, const bool reliable);
  void (*WriteData)(const void *data, size_t len);
  void (*WriteChar)(const int32_t c);
  void (*WriteByte)(const int32_t c);
  void (*WriteShort)(const int32_t c);
  void (*WriteLong)(const int32_t c);
  void (*WriteString)(const char *s);
  void (*WriteVector)(const float v);
  void (*WritePosition)(const Vec3 pos);
  void (*WriteDir)(const Vec3 pos); // single byte encoded, very coarse
  void (*WriteAngle)(const float v);
  void (*WriteAngles)(const Vec3 angles);

  /**
   * @brief Network console IO.
   */
  /**
   * @brief Mutes or unmutes `speaker` for `listener`, so that voice is filtered at the source.
   * @remarks Only the game knows player names, so it owns the policy; the server owns only the
   * mask it consults before relaying. Mutes are cleared when either client disconnects.
   */
  void (*MuteVoice)(const GameClient *listener, const GameClient *speaker, bool mute);

  void (*BroadcastPrint)(const int32_t level, const char *fmt, ...) __attribute__((format(printf, 2, 3)));
  void (*ClientPrint)(const GameClient *cl, const int32_t level, const char *fmt, ...) __attribute__((format(printf, 3, 4)));

  /**
   * @brief Submit frag and capture events accumulated during a match to the stats service.
   * @details The server handles URL, gating, JSON serialization, and HTTP POST.
   * @details This is best-effort delivery only; failures are silently discarded.
   */
  void (*PostStats)(const GameFrag *frags, size_t fragsLen, const GameCapture *captures, size_t capturesLen);

  /**
   * @}
   */
} GameImport;

/**
 * @brief The game export structure exposes core game module entry points to
 * the server. The game must populate this structure as part of `G_Init`.
 */
typedef struct {

  /**
   * @brief Game API version; validated by the server on load.
   */
  int32_t apiVersion;

  /**
   * @brief Minor protocol version; must match that of `cgame`.
   */
  int32_t protocol;

  /**
   * @brief The client game module this game pairs with, e.g. `"default"`.
   * @details Many game directories exist only to separate configs, maps and
   * demos from `default` and ship no client game of their own; this lets such
   * a game declare `default`, or any other installed client game, as the one
   * clients should load, rather than forcing every game directory to also be
   * a full UI/HUD mod. This comes from the game module, not the server admin,
   * because it is a statement about which client game this game's protocol
   * and stats layout were written against, not a runtime setting: a hostile
   * or careless `set` at the console must not be able to steer clients into
   * loading an arbitrary module.
   */
  const char *cgame;

  /**
   * @brief Client array, `sv_maxClients` in length; allocated by the game.
   */
  GameClient *clients[MAX_CLIENTS];

  /**
   * @brief Entity array, `sv_maxEntities` in length; allocated by the game.
   */
  GameEntity *entities[MAX_ENTITIES];

  /**
   * @brief Called once when the game module is first loaded.
   */
  void (*Init)(void);

  /**
   * @brief Called when the game module is unloaded.
   */
  void (*Shutdown)(void);

  /**
   * @brief Called at the start of each new level.
   * @param name The map name, e.g. "edge"
   */
  void (*SpawnEntities)(const char *name, const CmEntity *props, CmEntity *const *entities, size_t numEntities);

  /**
   * @brief Called in editor mode to spawn or respawn a single entity at the given
   * entity number, taking ownership of `def`.
   */
  void (*SpawnEditorEntity)(int32_t number, CmEntity *def);

  /**
   * @brief Called in editor mode to free the entity at the given entity number,
   * as well as any entities it owns.
   */
  void (*FreeEditorEntity)(int32_t number);

  /**
   * @brief Called when a client connects with valid user info; return false to reject.
   */
  /**
   * @brief Returns true if `listener` may hear `speaker` transmitting on `channel`.
   * @param channel The voice channel, defined by the game exactly as chat commands are.
   * @remarks The server asks rather than being told, so that who may be addressed is enforced
   * here, beside the same rules that govern chat, rather than proposed by a client.
   */
  bool (*ClientCanHearVoice)(const GameClient *speaker, const GameClient *listener, uint8_t channel);

  bool (*ClientConnect)(GameClient *cl, char *userInfo);

  /**
   * @brief Called when a client has fully spawned and should begin thinking.
   */
  void (*ClientBegin)(GameClient *cl);

  /**
   * @brief Called when the client's user info string changes.
   */
  void (*ClientUserInfoChanged)(GameClient *cl, const char *userInfo);

  /**
   * @brief Called when a client disconnects.
   */
  void (*ClientDisconnect)(GameClient *cl);

  /**
   * @brief Called for unhandled client console commands (e.g. voting).
   */
  void (*ClientCommand)(GameClient *cl);

  /**
   * @brief Called each frame with the client's movement command.
   */
  void (*ClientThink)(GameClient *cl, PlayerMoveCmd *cmd);

  /**
   * @brief Called every `QUETOO_TICK_SECONDS` to advance game logic.
   */
  void (*Frame)(void);

  /**
   * @brief Returns the game name advertised to server browsers.
   */
  const char *(*GameName)(void);

  /**
   * @brief Called by `Sv_Trace` for each solid entity a trace could clip, after
   * the server's own skip rules, and by `Sv_Clip` for the entity it tests.
   * Returning false leaves `ent` out of the trace, which is how a module makes
   * an entity solid to some movers and not others.
   * @param mover The entity the trace is on behalf of, or `NULL`.
   */
  bool (*ClipEntity)(const GameEntity *mover, const GameEntity *ent);
} GameExport;
