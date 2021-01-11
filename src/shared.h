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

#include "vector.h"
#include "color.h"

#include "collision/cm_types.h"

/**
 * @brief ConfigStrings are a general means of communication from the server to
 * all connected clients. Each ConfigString can be at most MAX_STRING_CHARS in
 * length. The game module is free to populate CS_GENERAL - MAX_CONFIG_STRINGS.
 */
#define CS_NAME				0 // the name (message) of the current level
#define CS_SKY				1 // the sky box
#define CS_WEATHER			2 // the weather string
#define CS_ZIP				3 // zip name for current level
#define CS_BSP_SIZE			4 // for catching incompatible maps
#define CS_WEAPONS			5 // weapon list, for the change weapon UI

#define CS_MODELS			6 // bsp, bsp sub-models, and mesh models
#define CS_SOUNDS			(CS_MODELS + MAX_MODELS)
#define CS_MUSICS			(CS_SOUNDS + MAX_SOUNDS)
#define CS_IMAGES			(CS_MUSICS + MAX_MUSICS)
#define CS_ITEMS			(CS_IMAGES + MAX_IMAGES)
#define CS_CLIENTS			(CS_ITEMS + MAX_ITEMS)
#define CS_GENERAL			(CS_CLIENTS + MAX_CLIENTS)

#define MAX_CONFIG_STRINGS	(CS_GENERAL + MAX_GENERAL)

/**
 * @brief Entity animation sequences (player animations) are dictated by the
 * game module but are run (interpolated) by the client game.
 */
typedef enum {
	ANIM_BOTH_DEATH1,
	ANIM_BOTH_DEAD1,
	ANIM_BOTH_DEATH2,
	ANIM_BOTH_DEAD2,
	ANIM_BOTH_DEATH3,
	ANIM_BOTH_DEAD3,

	ANIM_TORSO_GESTURE,

	ANIM_TORSO_ATTACK1,
	ANIM_TORSO_ATTACK2,

	ANIM_TORSO_DROP,
	ANIM_TORSO_RAISE,

	ANIM_TORSO_STAND1,
	ANIM_TORSO_STAND2,

	ANIM_LEGS_WALKCR,
	ANIM_LEGS_WALK,
	ANIM_LEGS_RUN,
	ANIM_LEGS_BACK,
	ANIM_LEGS_SWIM,

	ANIM_LEGS_JUMP1,
	ANIM_LEGS_LAND1,
	ANIM_LEGS_JUMP2,
	ANIM_LEGS_LAND2,

	ANIM_LEGS_IDLE,
	ANIM_LEGS_IDLECR,

	ANIM_LEGS_TURN,

	/**
	 * @brief Masks out the bits and only keeps the animation value
	 */
	ANIM_MASK_VALUE = (1 << 6) - 1,

	/**
	 * @brief Play the animation backwards.
	 */
	ANIM_REVERSE_BIT = (1 << 6),

	/**
	 * @brief Restarts the current animation sequence.
	 */
	ANIM_TOGGLE_BIT = (1 << 7)
} entity_animation_t;

/**
 * @brief Entity events are instantaneous, transpiring at an entity's origin
 * for precisely one frame. The game module can define custom events as well
 * (8 bits).
 */
typedef enum {
	EV_NONE,
	EV_GAME, // the game may extend from here
} entity_event_t;

/**
 * @brief Entity state effects are a bit mask used to combine common effects
 * such as rotating, bobbing, and pulsing. The game module may define custom
 * effects, up to 16 bits.
 */
#define EF_NONE				(0)
#define EF_ROTATE			(1 << 0) // rotate on z
#define EF_BOB				(1 << 1) // bob on z
#define EF_PULSE			(1 << 2) // pulsing light effect
#define EF_INACTIVE			(1 << 3) // inactive icon for when input is not going to game
#define EF_AMBIENT			(1 << 4) // ambient light to spot dimly lit entities easier
#define EF_GAME				(1 << 5) // the game may extend from here

/**
 * @brief The 16 high bits of the effects mask are not transmitted by the
 * protocol. Rather, they are reserved for the renderer.
 */
#define EF_SELF             (1 << 24) // client's entity model
#define EF_WEAPON			(1 << 25) // view weapon
#define EF_SHELL			(1 << 26) // colored shell
#define EF_ALPHATEST		(1 << 27) // alpha test
#define EF_BLEND			(1 << 28) // alpha blend
#define EF_NO_SHADOW		(1 << 29) // no shadow
#define EF_NO_DRAW			(1 << 30) // no draw (but perhaps shadow)

/**
 * @brief Entity trails are used to apply unique trail effects to entities
 * (typically projectiles).
 */
typedef enum {
	TRAIL_NONE,
	TRAIL_GAME, // the game may extend from here
} entity_trail_t;

/**
 * @brief Entity bounds are to be handled by the protocol based on
 * entity_state_t.solid. Box entities encode their bounds into a 16 bit
 * integer. The rest simply send their respective constant.
 */
typedef enum {
	SOLID_NOT, // no interaction with other objects
	SOLID_TRIGGER, // triggers touch objects which occupy them
	SOLID_PROJECTILE, // projectiles are triggers which can themselves occupy triggers
	SOLID_DEAD, // dead solids clip to the world, but do not clip to boxes
	SOLID_BOX, // boxes collide with the world and other boxes
	SOLID_BSP, // inline models interact just like the static world
} solid_t;

/**
 * @brief Sound attenuation levels.
 */
typedef enum {
	SOUND_ATTEN_NONE,
	SOUND_ATTEN_LINEAR,
	SOUND_ATTEN_SQUARE,
	SOUND_ATTEN_CUBIC,
} sound_atten_t;

/**
 * @brief Entity states are transmitted by the server to the client using delta
 * compression. The client parses these states and adds or removes entities
 * from the scene as needed.
 */
typedef struct {
	/**
	 * @brief The entity number that this state update belongs to.
	 */
	uint16_t number;
	
	/**
	 * @brief The entity's spawn_id; this will differ if an entity is replaced.
	 */
	uint8_t spawn_id;

	/**
	 * @brief The entity origin.
	 */
	vec3_t origin;

	/**
	 * @brief The entity termination for beams.
	 */
	vec3_t termination;

	/**
	 * @brief The entity angles.
	 */
	vec3_t angles;

	/**
	 * @brief The entity animation identifiers.
	 */
	uint8_t animation1, animation2;

	/**
	 * @brief The entity event (entity_event_t).
	 */
	uint8_t event;

	/**
	 * @brief The entity effects flags (EF_ROTATE, EF_BOB, etc).
	 */
	uint16_t effects;

	/**
	 * @brief The entity trail effect (entity_trail_t).
	 */
	uint8_t trail;

	/**
	 * @brief The entity model indexes (primary and linked models).
	 */
	uint8_t model1, model2, model3, model4;

	/**
	 * @brief The entity color.
	 */
	color32_t color;

	/**
	 * @brief The entity client info index.
	 */
	uint8_t client;

	/**
	 * @brief The entity ambient sound index.
	 */
	uint8_t sound;

	/**
	 * @brief The solid type. All solid types are sent to the client, but the
	 * client may elect to only predict collisions with certain types.
	 */
	uint8_t solid;

	/**
	 * @brief Bounding box dimensions for mesh entities. This enables
	 * client-sided prediction so that players don't e.g. run through each
	 * other.
	 */
	vec3_t mins, maxs;
} entity_state_t;

/**
 * @brief Some constants for the hook movement
 */
#define PM_HOOK_MIN_DIST		(32.0)
#define PM_HOOK_MAX_DIST		(MAX_WORLD_DIST)
#define PM_HOOK_DEF_DIST		(2048.0)

/**
 * @brief Water level
 */
typedef enum {
	WATER_UNKNOWN = -1,
	WATER_NONE,
	WATER_FEET,
	WATER_WAIST,
	WATER_UNDER
} pm_water_level_t;

/**
 * @brief General player movement and capabilities classification.
 */
typedef enum {
	PM_NORMAL, // walking, jumping, falling, swimming, etc.
	PM_HOOK_PULL, // pull hook
	PM_HOOK_SWING, // swing hook
	PM_SPECTATOR, // free-flying movement with acceleration and friction
	PM_DEAD, // no movement, but the ability to rotate in place
	PM_FREEZE // no movement at all
} pm_type_t;

/**
 * @brief Player movement flags. The game is free to define up to 16 bits.
 */
#define PMF_GAME			(1 << 0)

/**
 * @brief The player movement state contains quantized snapshots of player
 * position, orientation, velocity and world interaction state. This should
 * be modified only through invoking Pm_Move.
 */
typedef struct {
	pm_type_t type;
	vec3_t origin;
	vec3_t velocity;
	uint16_t flags; // game-specific state flags
	uint16_t time; // duration for temporal state flags
	int16_t gravity;
	vec3_t view_offset; // add to origin to resolve eyes
	vec3_t view_angles; // base view angles
	vec3_t delta_angles; // offset for spawns, pushers, etc.
	vec3_t hook_position; // position we're hooking to
	uint16_t hook_length; // length of the hook, for swing hook
} pm_state_t;

/**
 * @brief The max number of generic stats the server can communicate to a client.
 */
#define MAX_STATS			32

/**
 * @brief The number of bits a stat can hold.
 */
#define MAX_STAT_BITS		16

/**
 * @brief Player state structures contain authoritative snapshots of the
 * player's movement, as well as the player's statistics (inventory, health,
 * etc.). The game module is free to define what the stats array actually
 * contains.
 */
typedef struct player_state_s {
	pm_state_t pm_state; // movement and contents state
	int16_t stats[MAX_STATS]; // status bar updates
} player_state_t;

/*
 * KEY BUTTONS
 *
 * Continuous button event tracking is complicated by the fact that two different
 * input sources (say, mouse button 1 and the control key) can both press the
 * same button, but the button should only be released when both of the
 * pressing key have been released.
 *
 * When a key event issues a button command (+forward, +attack, etc), it appends
 * its key number as a parameter to the command so it can be matched up with
 * the release.
 */

typedef enum {
	BUTTON_STATE_HELD	= (1 << 0),
	BUTTON_STATE_DOWN	= (1 << 1),
	BUTTON_STATE_UP		= (1 << 2)
} button_state_t;

typedef struct {
	uint32_t keys[2]; // keys holding it down
	uint32_t down_time; // msec timestamp
	uint32_t msec; // msec down this frame
	button_state_t state;
} button_t;

/**
 * @brief Player movement commands, sent to the server at each client frame.
 */
typedef struct {
	uint8_t msec; // duration of the command, in milliseconds
	vec3_t angles; // the final view angles for this command
	int16_t forward, right, up; // directional intentions
	uint8_t buttons; // bit mask of buttons down
} pm_cmd_t;

/**
 * @brief Autocomplete function definition. You must fill "matches"
 * with any results that match the state of the current input buffer.
 * You can fetch the current state of the typed command with the Cmd_Arg*
 * functions.
 *
 * @param argi The index of the argument being autocompleted.
 * @param matches The list of matches you need to write to.
 */
typedef void (*AutocompleteFunc)(const uint32_t argi, GList **matches);

/**
 * @brief Console variables hold mutable scalars and strings.
 */
typedef struct cvar_s {
	const char *name;
	const char *default_string;
	char *string;
	char *latched_string; // for CVAR_LATCH vars
	float value;
	int32_t integer;
	uint32_t flags;
	const char *description;
	_Bool modified; // set each time the cvar is changed
	AutocompleteFunc Autocomplete;
} cvar_t;

typedef void (*CmdExecuteFunc)(void);

/**
 * @brief Console commands provide a scripting environment for users.
 */
typedef struct cmd_s {
	const char *name;
	const char *description;
	CmdExecuteFunc Execute;
	AutocompleteFunc Autocomplete;
	const char *commands; // for alias commands
	uint32_t flags;
} cmd_t;

/**
 * @brief Server multicast scope for entities and events.
 */
typedef enum {
	MULTICAST_ALL,
	MULTICAST_PHS,
	MULTICAST_PVS,
	MULTICAST_ALL_R,
	MULTICAST_PHS_R,
	MULTICAST_PVS_R
} multicast_t;

/**
 * @brief Server protocol commands. The game and client game module are free
 * to implement custom commands as well (8 bits).
 */
typedef enum {
	SV_CMD_BAD,
	SV_CMD_BASELINE,
	SV_CMD_CBUF_TEXT, // [string] stuffed into client's console buffer, should be \n terminated
	SV_CMD_CONFIG_STRING, // [short] [string]
	SV_CMD_DISCONNECT,
	SV_CMD_DOWNLOAD, // [short] size [size bytes]
	SV_CMD_DROP,
	SV_CMD_FRAME,
	SV_CMD_PRINT, // [byte] id [string] null terminated string
	SV_CMD_RECONNECT,
	SV_CMD_SERVER_DATA, // [long] protocol ...
	SV_CMD_SOUND,
	SV_CMD_CGAME, // the game may extend from here
} sv_packet_cmd_t;

/**
 * @brief Client protocol commands. The game and client game module are free
 * to implement custom commands as well (8 bits).
 */
typedef enum {
	CL_CMD_BAD,
	CL_CMD_MOVE, // [user_cmd_t]
	CL_CMD_STRING, // [string] message
	CL_CMD_USER_INFO, // [user_info_string]
	CL_CMD_CGAME, // the game may extend from here
} cl_packet_cmd_t;

#define NearestMultiple(n, align)	((n) == 0 ? 0 : ((n) - 1 - ((n) - 1) % (align) + (align)))

/**
 * @brief Math and trigonometry functions.
 */
int32_t Step(int32_t value, int32_t step);

/**
 * @brief A table of approximate normal vectors is used to save bandwidth when
 * transmitting entity angles, which would otherwise require 12 bytes.
 */
#define NUM_APPROXIMATE_NORMALS 162
extern const vec3_t approximate_normals[NUM_APPROXIMATE_NORMALS];

/**
 * @brief String manipulation functions.
 */
typedef enum {
	GLOB_FLAGS_NONE = 0,
	GLOB_CASE_INSENSITIVE = (1 << 0)
} glob_flags_t;

_Bool GlobMatch(const char *pattern, const char *text, const glob_flags_t flags);
const char *Basename(const char *path);
void Dirname(const char *in, char *out);
void StripNewline(const char *in, char *out);
void StripExtension(const char *in, char *out);

/**
 * @brief Escape sequences for color encoded strings.
 */
#define ESC_COLOR			'^'
#define ESC_COLOR_BLACK		0
#define ESC_COLOR_RED		1
#define ESC_COLOR_GREEN		2
#define ESC_COLOR_YELLOW	3
#define ESC_COLOR_BLUE		4
#define ESC_COLOR_MAGENTA	5
#define ESC_COLOR_CYAN		6
#define ESC_COLOR_WHITE		7

#define MAX_ESC_COLORS		8

#define ESC_COLOR_DEFAULT	ESC_COLOR_WHITE
#define ESC_COLOR_ALT		ESC_COLOR_GREEN

#define ESC_COLOR_INFO		ESC_COLOR_ALT
#define ESC_COLOR_CHAT		ESC_COLOR_ALT
#define ESC_COLOR_TEAMCHAT	ESC_COLOR_YELLOW

_Bool StrIsColor(const char *s);
color_t ColorEsc(int32_t esc);
size_t StrColorLen(const char *s);
int32_t StrColorCmp(const char *s1, const char *s2);
int32_t StrColor(const char *s);
int32_t StrrColor(const char *s);
void StripColors(const char *in, char *out);

char *va(const char *format, ...) __attribute__((format(printf, 1, 2)));
char *vtos(const vec3_t v);

void StrLower(const char *in, char *out);

// a cute little hack for printing g_entity_t
#define etos(e) (e ? va("%u: %s @ %s", e->s.number, e->class_name, vtos(e->s.origin)) : "null")

// key / value info strings
#define MAX_USER_INFO_KEY		64
#define MAX_USER_INFO_VALUE		64
#define MAX_USER_INFO_STRING	512

// max name of a team
#define MAX_TEAM_NAME			32

char *GetUserInfo(const char *s, const char *key);
void DeleteUserInfo(char *s, const char *key);
void SetUserInfo(char *s, const char *key, const char *value);
_Bool ValidateUserInfo(const char *s);

gboolean g_stri_equal (gconstpointer v1, gconstpointer v2);
guint g_stri_hash (gconstpointer v);

/**
 * @brief The type of an AI node.
 */
typedef uint16_t ai_node_id_t;

/**
 * @brief Default filesystem initialization flags.
 */
#define FS_NONE					0x0

/**
 * @brief If set, supported archives (.pk3, .pak) in search paths will be
 * automatically loaded. Set this to false for tools that require the write
 * directory, but not read access to the Quake file system (e.g quetoo-master).
 */
#define FS_AUTO_LOAD_ARCHIVES   0x1

typedef struct {
	void *opaque;
} file_t;

typedef void (*Fs_Enumerator)(const char *path, void *data);

/**
 * @brief Debug cateogories.
 */
typedef enum {
	DEBUG_AI			= 1 << 0,
	DEBUG_CGAME			= 1 << 1,
	DEBUG_CLIENT		= 1 << 2,
	DEBUG_COLLISION		= 1 << 3,
	DEBUG_CONSOLE		= 1 << 4,
	DEBUG_FILESYSTEM	= 1 << 5,
	DEBUG_GAME			= 1 << 6,
	DEBUG_NET			= 1 << 7,
	DEBUG_PMOVE_CLIENT	= 1 << 8,
	DEBUG_PMOVE_SERVER  = 1 << 9,
	DEBUG_RENDERER		= 1 << 10,
	DEBUG_SERVER		= 1 << 11,
	DEBUG_SOUND			= 1 << 12,
	DEBUG_UI			= 1 << 13,

	DEBUG_BREAKPOINT	= (int32_t) (1u << 31),
	DEBUG_ALL			= (int32_t) (0xFFFFFFFF & ~DEBUG_BREAKPOINT),
} debug_t;
