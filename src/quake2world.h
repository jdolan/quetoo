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

#ifndef __QUAKE2WORLD_H__
#define __QUAKE2WORLD_H__

#include "config.h"

// to support the gnuc __attribute__ command
#if defined __ICC || !defined __GNUC__
#  define __attribute__(x)  /*NOTHING*/
#endif

#include <glib.h>
#include <math.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#ifdef _WIN32
#undef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

#ifndef true
#define true 1
#define false 0
#endif

#ifndef byte
typedef unsigned char byte;
#endif

typedef float vec_t;
typedef vec_t vec2_t[2];
typedef vec_t vec3_t[3];
typedef vec_t vec4_t[4];

typedef double dvec_t;
typedef dvec_t dvec2_t[2];
typedef dvec_t dvec3_t[3];
typedef dvec_t dvec4_t[4];

/*
 * @brief Indices for angle vectors.
 */
#define PITCH				0 // up / down
#define YAW					1 // left / right
#define ROLL				2 // tilt / lean
/*
 * @brief String length constants.
 */
#define MAX_STRING_CHARS	1024 // max length of a string passed to Cmd_TokenizeString
#define MAX_STRING_TOKENS	128 // max tokens resulting from Cmd_TokenizeString
#define MAX_TOKEN_CHARS		256 // max length of an individual token
#define MAX_QPATH			64 // max length of a quake game pathname
#define MAX_OSPATH			256 // max length of a filesystem pathname
/*
 * @brief Protocol limits.
 */
#define MIN_CLIENTS			1 // duh
#define MAX_CLIENTS			256 // absolute limit
#define MAX_EDICTS			1024 // must change protocol to increase more
#define MAX_MODELS			256 // these are sent over the net as uint8_t
#define MAX_SOUNDS			256 // so they cannot be blindly increased
#define MAX_MUSICS			8 // per level
#define MAX_IMAGES			256 // that the server knows about
#define MAX_ITEMS			64 // pickup items
#define MAX_GENERAL			256 // general config strings
/*
 * @brief Print message levels, for filtering.
 */
#define PRINT_LOW			0 // pickup messages
#define PRINT_MEDIUM		1 // death messages
#define PRINT_HIGH			2 // critical messages
#define PRINT_CHAT			3 // chat messages
#define PRINT_TEAMCHAT		4 // teamchat messages
/*
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
/*
 * @brief Console variable flags.
 */
#define CVAR_CLI			0x1 // will retain value through initialization
#define CVAR_ARCHIVE		0x2 // saved to quake2world.cfg
#define CVAR_USER_INFO		0x4 // added to user_info when changed
#define CVAR_SERVER_INFO	0x8 // added to server_info when changed
#define CVAR_LO_ONLY		0x10 // don't allow change when connected
#define CVAR_NO_SET			0x20 // don't allow change from console at all
#define CVAR_LATCH			0x40 // save changes until server restart
#define CVAR_R_CONTEXT		0x80 // affects OpenGL context
#define CVAR_R_MEDIA		0x100 // affects renderer media filtering
#define CVAR_S_DEVICE		0x200 // affects sound device parameters
#define CVAR_S_MEDIA		0x400 // affects sound media
#define CVAR_R_MASK			(CVAR_R_CONTEXT | CVAR_R_MEDIA)
#define CVAR_S_MASK 		(CVAR_S_DEVICE | CVAR_S_MEDIA)

/*
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
	MEM_TAG_ALL = -1
} mem_tag_t;

/*
 * @brief Console variables hold mutable scalars and strings.
 */
typedef struct cvar_s {
	const char *name;
	const char *description;
	const char *default_value;
	char *string;
	char *latched_string; // for CVAR_LATCH vars
	uint32_t flags;
	_Bool modified; // set each time the cvar is changed
	vec_t value;
	int32_t integer;
} cvar_t;

typedef void (*CmdExecuteFunc)(void);

/*
 * @brief Console commands provide a scripting environment for users.
 */
typedef struct cmd_s {
	const char *name;
	const char *description;
	CmdExecuteFunc Execute;
	const char *commands; // for alias commands
	uint32_t flags;
} cmd_t;

/*
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

/*
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
	SV_CMD_FRAME,
	SV_CMD_PRINT, // [byte] id [string] null terminated string
	SV_CMD_RECONNECT,
	SV_CMD_SERVER_DATA, // [long] protocol ...
	SV_CMD_SOUND,
	SV_CMD_CGAME, // the game may extend from here
} sv_packet_cmd_t;

/*
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

#define CONTENTS_ORIGIN			0x1000000 // removed during bsp stage
#define CONTENTS_MONSTER		0x2000000 // should never be on a brush, only in game
#define CONTENTS_DEAD_MONSTER	0x4000000

#define CONTENTS_DETAIL			0x8000000 // brushes to be added after vis leafs
#define CONTENTS_TRANSLUCENT	0x10000000 // auto set if any surface has trans
#define CONTENTS_LADDER			0x20000000

/*
 * @brief Leafs will have some combination of the above flags; nodes will
 * always be -1.
 */
#define CONTENTS_NODE			-1

/*
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
/*
 * @brief Contents masks; OR'ed combinations of CONTENTS_*.
 */
#define MASK_ALL				(-1)
#define MASK_SOLID				(CONTENTS_SOLID|CONTENTS_WINDOW)
#define MASK_PLAYER_SOLID		(CONTENTS_SOLID|CONTENTS_PLAYER_CLIP|CONTENTS_WINDOW|CONTENTS_MONSTER)
#define MASK_DEAD_SOLID			(CONTENTS_SOLID|CONTENTS_PLAYER_CLIP|CONTENTS_WINDOW)
#define MASK_MONSTER_SOLID		(CONTENTS_SOLID|CONTENTS_MONSTER_CLIP|CONTENTS_WINDOW|CONTENTS_MONSTER)
#define MASK_WATER				(CONTENTS_WATER|CONTENTS_LAVA|CONTENTS_SLIME)
#define MASK_OPAQUE				(CONTENTS_SOLID|CONTENTS_SLIME|CONTENTS_LAVA)
#define MASK_SHOT				(CONTENTS_SOLID|CONTENTS_MONSTER|CONTENTS_WINDOW|CONTENTS_DEAD_MONSTER)
#define MASK_CURRENT			(CONTENTS_CURRENT_0|CONTENTS_CURRENT_90|CONTENTS_CURRENT_180|\
							 	 	 CONTENTS_CURRENT_270|CONTENTS_CURRENT_UP|CONTENTS_CURRENT_DOWN)

// gi.AreaEdicts() can return a list of either solid or trigger entities
#define AREA_SOLID				1
#define AREA_TRIGGERS			2

typedef struct c_bsp_plane_s {
	vec3_t normal;
	vec_t dist;
	int32_t type; // for fast side tests
	int32_t sign_bits; // sign_x + (sign_y << 1) + (sign_z << 2)
} c_bsp_plane_t;

typedef struct c_bsp_surface_s {
	char name[32];
	int32_t flags;
	int32_t value;
} c_bsp_surface_t;

typedef struct c_model_s {
	vec3_t mins, maxs;
	vec3_t origin; // for sounds or lights
	int32_t head_node;
} c_model_t;

// a trace is returned when a box is swept through the world
typedef struct c_trace_s {
	_Bool all_solid; // if true, plane is not valid
	_Bool start_solid; // if true, the initial point was in a solid area
	vec_t fraction; // time completed, 1.0 = didn't hit anything
	vec3_t end; // final position
	c_bsp_plane_t plane; // surface normal at impact
	c_bsp_surface_t *surface; // surface hit
	int32_t leaf_num;
	int32_t contents; // contents on other side of surface hit
	struct g_edict_s *ent; // not set by Cm_*() functions
} c_trace_t;

// pm_state_t is the information necessary for client side movement prediction
typedef enum {
	// can accelerate and turn
	PM_NORMAL,
	PM_SPECTATOR,
	// no acceleration or turning
	PM_DEAD,
	PM_FREEZE
} pm_type_t;

// pm_state_t flags, the game is free to define the rest of these
#define PMF_NO_PREDICTION	0x1

/*
 * @brief The player movement state contains quantized snapshots of player
 * position, orientation, velocity and world interaction state. This should
 * be modified only through invoking Pm_Move.
 */
typedef struct pm_state_s {
	pm_type_t type;
	int16_t origin[3];
	int16_t velocity[3];
	uint16_t flags; // ducked, jump_held, etc
	uint16_t time; // duration for PMF_TIME_* flags
	int16_t gravity;
	int16_t view_offset[3]; // add to origin to resolve eyes
	int16_t view_angles[3]; // base view angles
	int16_t kick_angles[3]; // offset for kick
	int16_t delta_angles[3]; // offset for spawns, pushers, etc.
} pm_state_t;

// button bits
#define BUTTON_ATTACK		1
#define BUTTON_WALK			2

// user_cmd_t is sent to the server each client frame
typedef struct user_cmd_s {
	uint8_t msec;
	uint8_t buttons;
	int16_t angles[3];
	int16_t forward, right, up;
} user_cmd_t;

#define MAX_TOUCH_ENTS 32
typedef struct {
	pm_state_t s; // state (in / out)

	user_cmd_t cmd; // command (in)

	uint16_t num_touch; // results (out)
	struct g_edict_s *touch_ents[MAX_TOUCH_ENTS];

	vec3_t angles; // clamped, and including kick and delta
	vec3_t mins, maxs; // bounding box size

	struct g_edict_s *ground_entity;

	int32_t water_type;
	uint8_t water_level;

	vec_t step; // stair interaction

	// collision with the world and solid entities
	int32_t (*PointContents)(const vec3_t point);
	c_trace_t (*Trace)(const vec3_t start, const vec3_t end, const vec3_t mins, const vec3_t maxs);

	// print debug messages for development
	void (*Debug)(const char *msg);
} pm_move_t;

/*
 * @brief Sound attenuation constants.
 */
#define ATTEN_NONE  		0 // full volume the entire level
#define ATTEN_NORM  		1 // normal linear attenuation
#define ATTEN_IDLE  		2 // exponential decay
#define ATTEN_STATIC  		3 // high exponential decay
#define ATTEN_DEFAULT		ATTEN_NORM

/*
 * @brief The absolute world bounds is +/- 4096. This is the largest box we can
 * safely encode using 16 bit integers (vec_t * 8.0).
 */
#define MIN_WORLD_COORD		-4096.0
#define MAX_WORLD_COORD		4096.0

/*
 * @brief ConfigStrings are a general means of communication from the server to
 * all connected clients. Each ConfigString can be at most MAX_STRING_CHARS in
 * length. The game module is free to populate CS_GENERAL - MAX_CONFIG_STRINGS.
 */
#define CS_NAME				0 // the name (message) of the current level
#define CS_SKY				1 // the sky box
#define CS_WEATHER			2 // the weather string
#define CS_ZIP				3 // zip name for current level
#define CS_BSP_SIZE			4 // for catching incompatible maps
#define CS_MODELS			5 // bsp, bsp sub-models, and mesh models
#define CS_SOUNDS			(CS_MODELS + MAX_MODELS)
#define CS_MUSICS			(CS_SOUNDS + MAX_SOUNDS)
#define CS_IMAGES			(CS_MUSICS + MAX_MUSICS)
#define CS_ITEMS			(CS_IMAGES + MAX_IMAGES)
#define CS_CLIENTS			(CS_ITEMS + MAX_ITEMS)
#define CS_GENERAL			(CS_CLIENTS + MAX_CLIENTS)

#define MAX_CONFIG_STRINGS	(CS_GENERAL + MAX_GENERAL)

/*
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

	ANIM_LEGS_TURN
} entity_animation_t;

/*
 * @brief Restarts the current animation sequence.
 */
#define ANIM_TOGGLE_BIT 0x80

/*
 * @brief Entity events are instantaneous, transpiring at an entity's origin
 * for precisely one frame. The game module can define custom events as well
 * (8 bits).
 */
typedef enum {
	EV_NONE,
	EV_CLIENT_TELEPORT,
	EV_GAME, // the game may extend from here
} entity_event_t;

/*
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

/*
 * @brief The 16 high bits of the effects mask are not transmitted by the
 * protocol. Rather, they are reserved for the renderer.
 */
#define EF_WEAPON			(1 << 25) // view weapon
#define EF_ALPHATEST		(1 << 27) // alpha test
#define EF_BLEND			(1 << 28) // blend
#define EF_NO_LIGHTING		(1 << 29) // no lighting (full bright)
#define EF_NO_SHADOW		(1 << 30) // no shadow
#define EF_NO_DRAW			(1 << 31) // no draw (but perhaps shadow)

/*
 * @brief Entity trails are used to apply unique trail effects to entities
 * (typically projectiles).
 */
typedef enum {
	TRAIL_NONE,
	TRAIL_GAME, // the game may extend from here
} entity_trail_t;

/*
 * Entity bounds are to be handled by the protocol based on their
 * solid field. Box entities encode their bounds into a 16 bit
 * integer. The rest simply send their value.
 */
typedef enum {
	SOLID_NOT, // no interaction with other objects
	SOLID_TRIGGER, // only touch when inside, after moving
	SOLID_BOX, // touch on edge
	SOLID_MISSILE, // touch on edge
	SOLID_BSP = 31, // BSP clip, touch on edge
} solid_t;

/*
 * Entity states are transmitted by the server to the client using delta
 * compression. The client parses these states and adds or removes entities
 * from the scene as needed.
 */
typedef struct {
	uint16_t number; // edict index

	vec3_t origin;
	vec3_t old_origin; // for interpolating

	vec3_t angles;

	uint8_t animation1, animation2; // animations (running, attacking, ..)

	uint8_t event; // client side events (sounds, blood, ..)

	uint16_t effects; // pulse, bob, rotate, etc..

	uint8_t trail; // particle trails, dynamic lights, etc..

	uint8_t model1, model2, model3, model4; // primary model, linked models

	uint8_t client; // client info index

	uint8_t sound; // looped sounds

	/*
	 * Encoded bounding box dimensions for mesh entities. This facilitates
	 * client-sided prediction so that players don't e.g. run through each
	 * other. See gi.LinkEdict.
	 */
	uint16_t solid;
} entity_state_t;

#define MAX_STATS			32

/*
 * Player state structures contain authoritative snapshots of the player's
 * movement, as well as the player's statistics (inventory, health, etc.).
 * The game module is free to define what the stats array actually contains.
 */
typedef struct player_state_s {
	pm_state_t pm_state; // movement and contents state
	int16_t stats[MAX_STATS]; // status bar updates
} player_state_t;

/*
 * Colored text escape and code definitions.
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

#define IS_COLOR(c)( \
	*c == COLOR_ESC && ( \
		*(c + 1) >= '0' && *(c + 1) <= '7' \
	) \
)

#define IS_LEGACY_COLOR(c)( \
	*c == 1 || *c == 2 \
)

/* returns the amount of elements - not the amount of bytes */
#define lengthof(x) (sizeof(x) / sizeof(*(x)))
#define CASSERT(x) extern int32_t ASSERT_COMPILE[((x) != 0) * 2 - 1]

#endif /* __QUAKE2WORLD_H__ */
