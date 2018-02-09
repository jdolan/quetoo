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

#include "config.h"

#include <assert.h>
#include <ctype.h>
#include <glib.h>
#include <inttypes.h>
#include <math.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if HAVE_UNISTD_H
	#include <unistd.h>
#endif

#if defined(_WIN32)
	// A hack to fix buggy MingW behavior
	#if defined(__MINGW32__)
		#undef PRIdPTR
		#undef PRIiPTR
		#undef PRIoPTR
		#undef PRIuPTR
		#undef PRIxPTR
		#undef PRIXPTR

		#undef SCNdPTR
		#undef SCNiPTR
		#undef SCNoPTR
		#undef SCNuPTR
		#undef SCNxPTR

		#if defined(__MINGW64__)
			#define PRIdPTR PRId64
			#define PRIiPTR PRIi64
			#define PRIoPTR PRIo64
			#define PRIuPTR PRIu64
			#define PRIxPTR PRIx64
			#define PRIXPTR PRIX64

			#define SCNdPTR SCNd64
			#define SCNiPTR SCNi64
			#define SCNoPTR SCNo64
			#define SCNuPTR SCNu64
			#define SCNxPTR SCNx64
		#else
			#define PRIdPTR PRId32
			#define PRIiPTR PRIi32
			#define PRIoPTR PRIo32
			#define PRIuPTR PRIu32
			#define PRIxPTR PRIx32
			#define PRIXPTR PRIX32

			#define SCNdPTR SCNd32
			#define SCNiPTR SCNi32
			#define SCNoPTR SCNo32
			#define SCNuPTR SCNu32
			#define SCNxPTR SCNx32
		#endif
#endif
#endif

#ifndef byte
	typedef uint8_t byte;
#endif

#ifndef M_PI
	#define M_PI 3.14159265358979323846264338327950288
#endif

#ifndef M_PI_2
	#define M_PI_2 1.57079632679489661923132169163975144
#endif

/**
 * @return The number of elements, rather than the number of bytes.
 */
#ifndef lengthof
	#define lengthof(x) (sizeof(x) / sizeof(x[0]))
#endif

#define VECTOR_TYPENAME(name) name ## _t
#define VECTOR_TYPENAME_N(name, n) name ## n ## _t[n]
#define DECLARE_VECTOR_TYPE(type, typename) \
	typedef type VECTOR_TYPENAME(typename); \
	typedef VECTOR_TYPENAME(typename) VECTOR_TYPENAME_N(typename, 2); \
	typedef VECTOR_TYPENAME(typename) VECTOR_TYPENAME_N(typename, 3); \
	typedef VECTOR_TYPENAME(typename) VECTOR_TYPENAME_N(typename, 4)

DECLARE_VECTOR_TYPE(float, vec);
DECLARE_VECTOR_TYPE(double, dvec);
DECLARE_VECTOR_TYPE(uint8_t, u8vec);
DECLARE_VECTOR_TYPE(uint16_t, u16vec);
DECLARE_VECTOR_TYPE(int16_t, s16vec);

#undef VECTOR_TYPE
#undef VECTOR_TYPENAME_N
#undef VECTOR_TYPENAME

/**
 * @brief Indices for angle vectors.
 */
#define PITCH				0 // up / down
#define YAW					1 // left / right
#define ROLL				2 // tilt / lean

/**
 * @brief String length constants.
 */
#define MAX_STRING_CHARS	1024 // max length of a string passed to Cmd_TokenizeString
#define MAX_STRING_TOKENS	128 // max tokens resulting from Cmd_TokenizeString
#define MAX_TOKEN_CHARS		512 // max length of an individual token
#define MAX_QPATH			64 // max length of a Quake game path
#define MAX_OS_PATH			260 // max length of a system path

/**
 * @brief Protocol limits.
 */
#define MIN_CLIENTS			1 // duh
#define MAX_CLIENTS			64 // absolute limit
#define MIN_ENTITIES		128 // necessary for even the simplest game
#define MAX_ENTITIES		1024 // this can be increased with minimal effort
#define MAX_MODELS			256 // these are sent over the net as uint8_t
#define MAX_SOUNDS			256 // so they cannot be blindly increased
#define MAX_MUSICS			8 // per level
#define MAX_IMAGES			256 // that the server knows about
#define MAX_ITEMS			64 // pickup items
#define MAX_GENERAL			256 // general config strings

/**
 * @brief Print message levels, for filtering.
 */
#define PRINT_LOW			0x1
#define PRINT_MEDIUM		0x2
#define PRINT_HIGH			0x4
#define PRINT_CHAT			0x8
#define PRINT_TEAM_CHAT		0x10
#define PRINT_ECHO			0x20

/**
 * @brief Console command flags.
 */
#define CMD_SYSTEM			0x1 // always execute, even if not connected
#define CMD_SERVER			0x2 // added by server
#define CMD_GAME			0x4 // added by game module
#define CMD_CLIENT			0x8 // added by client
#define CMD_RENDERER		0x10 // added by renderer
#define CMD_SOUND			0x20 // added by sound
#define CMD_UI				0x40 // added by user interface
#define CMD_CGAME			0x80 // added by client game module
#define CMD_AI				0x100 // added by AI module

/**
 * @brief Console variable flags.
 */
#define CVAR_CLI			0x1 // will retain value through initialization
#define CVAR_ARCHIVE		0x2 // saved to quetoo.cfg
#define CVAR_USER_INFO		0x4 // added to user_info when changed
#define CVAR_SERVER_INFO	0x8 // added to server_info when changed
#define CVAR_DEVELOPER		0x10 // don't allow change when connected
#define CVAR_NO_SET			0x20 // don't allow change from console at all
#define CVAR_LATCH			0x40 // save changes until server restart
#define CVAR_R_CONTEXT		0x80 // affects OpenGL context
#define CVAR_R_MEDIA		0x100 // affects renderer media filtering
#define CVAR_S_DEVICE		0x200 // affects sound device parameters
#define CVAR_S_MEDIA		0x400 // affects sound media
#define CVAR_R_MASK			(CVAR_R_CONTEXT | CVAR_R_MEDIA)
#define CVAR_S_MASK 		(CVAR_S_DEVICE | CVAR_S_MEDIA)

/**
 * @brief Managed memory tags allow freeing of subsystem memory in batch.
 */
typedef enum {
	MEM_TAG_DEFAULT,
	MEM_TAG_SERVER,
	MEM_TAG_AI,
	MEM_TAG_GAME,
	MEM_TAG_GAME_LEVEL,
	MEM_TAG_CLIENT,
	MEM_TAG_RENDERER,
	MEM_TAG_SOUND,
	MEM_TAG_UI,
	MEM_TAG_CGAME,
	MEM_TAG_CGAME_LEVEL,
	MEM_TAG_MATERIAL,
	MEM_TAG_CMD,
	MEM_TAG_CVAR,
	MEM_TAG_CMODEL,
	MEM_TAG_BSP,
	MEM_TAG_FS,

	MEM_TAG_TOTAL,
	MEM_TAG_ALL = -1
} mem_tag_t;

/**
 * @brief The server, game and player movement frame rate.
 */
#define QUETOO_TICK_RATE	40
#define QUETOO_TICK_SECONDS	(1.0 / QUETOO_TICK_RATE)
#define QUETOO_TICK_MILLIS	(1000 / QUETOO_TICK_RATE)

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
	const char *default_value;
	char *string;
	char *latched_string; // for CVAR_LATCH vars
	vec_t value;
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

// lower bits are stronger, and will eat weaker brushes completely
#define CONTENTS_SOLID			0x1 // an eye is never valid in a solid
#define CONTENTS_WINDOW			0x2 // translucent, but not watery
#define CONTENTS_AUX			0x4 // not used at the moment
#define CONTENTS_LAVA			0x8
#define CONTENTS_SLIME			0x10
#define CONTENTS_WATER			0x20
#define CONTENTS_MIST			0x40

#define LAST_VISIBLE_CONTENTS	CONTENTS_MIST

// remaining contents are non-visible, and don't eat brushes
#define CONTENTS_AREA_PORTAL	0x8000
#define CONTENTS_PLAYER_CLIP	0x10000
#define CONTENTS_MONSTER_CLIP	0x20000

// currents can be added to any other contents, and may be mixed
#define CONTENTS_CURRENT_0		0x40000
#define CONTENTS_CURRENT_90		0x80000
#define CONTENTS_CURRENT_180	0x100000
#define CONTENTS_CURRENT_270	0x200000
#define CONTENTS_CURRENT_UP		0x400000
#define CONTENTS_CURRENT_DOWN	0x800000

#define CONTENTS_ORIGIN			0x1000000 // removed during BSP compilation
#define CONTENTS_MONSTER		0x2000000 // should never be on a brush, only in game
#define CONTENTS_DEAD_MONSTER	0x4000000

#define CONTENTS_DETAIL			0x8000000 // brushes to be added after vis leafs
#define CONTENTS_TRANSLUCENT	0x10000000 // auto set if any surface has trans
#define CONTENTS_LADDER			0x20000000

/**
 * @brief Leafs will have some combination of the above flags; nodes will
 * always be -1.
 */
#define CONTENTS_NODE			-1

/**
 * @brief c_bsp_surface_t.flags.
 */
#define SURF_LIGHT				0x1 // value will hold the light strength
#define SURF_SLICK				0x2 // effects game physics
#define SURF_SKY				0x4 // don't draw, but add to skybox
#define SURF_WARP				0x8 // turbulent water warp
#define SURF_BLEND_33			0x10 // 0.33 alpha blending
#define SURF_BLEND_66			0x20 // 0.66 alpha blending
#define SURF_FLOWING			0x40 // scroll towards angle, no longer supported
#define SURF_NO_DRAW			0x80 // don't bother referencing the texture
#define SURF_HINT				0x100 // make a primary bsp splitter
#define SURF_SKIP				0x200 // completely skip, allowing non-closed brushes
#define SURF_ALPHA_TEST			0x400 // alpha test (grates, foliage, etc..)
#define SURF_PHONG				0x800 // phong interpolated lighting at compile time
#define SURF_MATERIAL			0x1000 // retain the geometry, but don't draw diffuse pass

/**
 * @brief Contents masks: frequently combined contents flags.
 */
#define MASK_ALL				(-1)
#define MASK_SOLID				(CONTENTS_SOLID | CONTENTS_WINDOW)
#define MASK_LIQUID				(CONTENTS_WATER | CONTENTS_LAVA | CONTENTS_SLIME)
#define MASK_MEAT				(CONTENTS_MONSTER | CONTENTS_DEAD_MONSTER)
#define MASK_CLIP_CORPSE		(MASK_SOLID | CONTENTS_PLAYER_CLIP)
#define MASK_CLIP_PLAYER		(MASK_CLIP_CORPSE | CONTENTS_MONSTER)
#define MASK_CLIP_MONSTER		(MASK_CLIP_PLAYER | CONTENTS_MONSTER_CLIP)
#define MASK_CLIP_PROJECTILE	(MASK_SOLID | MASK_MEAT)

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
	int16_t view_offset[3]; // add to origin to resolve eyes
	uint16_t view_angles[3]; // base view angles
	uint16_t delta_angles[3]; // offset for spawns, pushers, etc.

	vec3_t hook_position; // position we're hooking to
	uint16_t hook_length; // length of the hook, for swing hook
} pm_state_t;

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
	uint8_t buttons; // bit mask of buttons down
	uint16_t angles[3]; // the final view angles for this command
	int16_t forward, right, up; // directional intentions
} pm_cmd_t;

/**
 * @brief Sound attenuation constants.
 */
#define ATTEN_NONE  		0 // full volume the entire level
#define ATTEN_NORM  		1 // normal linear attenuation
#define ATTEN_IDLE  		2 // exponential decay
#define ATTEN_STATIC  		3 // high exponential decay
#define ATTEN_DEFAULT		ATTEN_NORM

/**
 * @brief The absolute world bounds is +/- 4096. This is the largest box we can
 * safely encode using 16 bit integers (vec_t * 8.0).
 */
#define MIN_WORLD_COORD		-4096.0
#define MAX_WORLD_COORD		 4096.0

/**
 * @brief Therefore, the maximum distance across the world is the
 * sqrt((2 * 4096.0)^2 + (2 * 4096.0)^2) = 11585.237
 */
#define MAX_WORLD_DIST		 11586.0

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
 * @brief Constants for animation1 that the lightning gun uses for
 * its hit detection routines.
 */
#define LIGHTNING_NO_HIT		0
#define LIGHTNING_SOLID_HIT		1

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
#define EF_GAME				(1 << 4) // the game may extend from here

/**
 * @brief The 16 high bits of the effects mask are not transmitted by the
 * protocol. Rather, they are reserved for the renderer.
 */
#define EF_LINKED			(1 << 23) // linked to another model
#define EF_CLIENT			(1 << 24) // player model
#define EF_WEAPON			(1 << 25) // view weapon
#define EF_SHELL			(1 << 26) // environment map shell
#define EF_ALPHATEST		(1 << 27) // alpha test
#define EF_BLEND			(1 << 28) // alpha blend
#define EF_NO_LIGHTING		(1 << 29) // no lighting (full bright)
#define EF_NO_SHADOW		(1 << 30) // no shadow
#define EF_NO_DRAW			(1u << 31) // no draw (but perhaps shadow)

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
 * @brief Entity states are transmitted by the server to the client using delta
 * compression. The client parses these states and adds or removes entities
 * from the scene as needed.
 */
typedef struct {
	uint16_t number; // entity index

	vec3_t origin;
	vec3_t termination; // beams (lightning, grapple, lasers, ..)
	vec3_t angles;

	uint8_t animation1, animation2; // animations (running, attacking, ..)
	uint8_t event; // client side events (sounds, blood, ..)
	uint16_t effects; // pulse, bob, rotate, etc..
	uint8_t trail; // particle trails, dynamic lights, etc..
	uint8_t model1, model2, model3, model4; // primary model, linked models
	uint8_t client; // client info index
	uint8_t sound; // looped sounds

	/**
	 * @brief The solid type. All solid types are sent to the client, but the
	 * client may elect to only predict collisions with certain types.
	 */
	uint8_t solid;

	/**
	 * @brief Encoded bounding box dimensions for mesh entities. This enables
	 * client-sided prediction so that players don't e.g. run through each
	 * other. See PackBounds.
	 */
	uint32_t bounds;
} entity_state_t;

/**
 * @brief The max number of generic stats the server can communicate to a client.
 */
#define MAX_STATS			32

/**
 * @brief The number of bits a stat can hold
 */
#define MAX_STAT_BITS		(sizeof(int16_t) * 8)

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

/**
 * @brief Colored text escape and code definitions.
 */
#define COLOR_ESC			'^'

#define CON_COLOR_BLACK		0
#define CON_COLOR_RED		1
#define CON_COLOR_GREEN		2
#define CON_COLOR_YELLOW	3
#define CON_COLOR_BLUE		4
#define CON_COLOR_CYAN		5
#define CON_COLOR_MAGENTA	6
#define CON_COLOR_WHITE		7

#define MAX_COLORS			8

#define CON_COLOR_DEFAULT	CON_COLOR_WHITE
#define CON_COLOR_ALT		CON_COLOR_GREEN

#define CON_COLOR_INFO		CON_COLOR_ALT
#define CON_COLOR_CHAT		CON_COLOR_ALT
#define CON_COLOR_TEAMCHAT	CON_COLOR_YELLOW

/**
 * @return True if the byte represents a color escape sequence.
 */
#define IS_COLOR(c)( \
                     *c == COLOR_ESC && ( \
                             *(c + 1) >= '0' && *(c + 1) <= '7' \
                                        ) \
                   )

/**
 * @return True if the byte represents a legacy color escape sequence.
 */
#define IS_LEGACY_COLOR(c)( \
                            *c == 1 || *c == 2 \
                          )
