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

#include <ctype.h>
#include <math.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

#ifdef HAVE_STDINT_H
#include <stdint.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#endif
#ifdef HAVE_STRINGS_H
#include <strings.h>
#endif

#include <time.h>

#ifdef _WIN32
#include "win32.h"
#endif

#if defined _WIN64
# define Q2W_SIZE_T "%I64u"
#elif defined _WIN32
# define Q2W_SIZE_T "%u"
#else
# define Q2W_SIZE_T "%zu"
#endif

#ifndef byte
typedef unsigned char byte;
#endif

#ifndef true
#define true 1
#define false 0
#endif

typedef byte boolean_t;

// angle indexes
#define PITCH				0  // up / down
#define YAW					1  // left / right
#define ROLL				2  // tilt / lean
#define MAX_STRING_CHARS	1024  // max length of a string passed to Cmd_TokenizeString
#define MAX_STRING_TOKENS	128   // max tokens resulting from Cmd_TokenizeString
#define MAX_TOKEN_CHARS		256   // max length of an individual token
#define MAX_QPATH			64    // max length of a quake game pathname
#define MAX_OSPATH			128   // max length of a filesystem pathname

// per-level limits
#define MIN_CLIENTS			1     // duh
#define MAX_CLIENTS			256   // absolute limit
#define MAX_EDICTS			1024  // must change protocol to increase more
#define MAX_MODELS			256   // these are sent over the net as bytes
#define MAX_SOUNDS			256   // so they cannot be blindly increased
#define MAX_MUSICS			8     // per level
#define MAX_IMAGES			256   // that the server knows about
#define MAX_ITEMS			64    // pickup items
#define MAX_GENERAL			256   // general config strings

// print levels
#define PRINT_LOW			0  // pickup messages
#define PRINT_MEDIUM		1  // death messages
#define PRINT_HIGH			2  // critical messages
#define PRINT_CHAT			3  // chat messages
#define PRINT_TEAMCHAT		4  // teamchat messages

// console variables
#define CVAR_ARCHIVE		0x1  // saved to quake2world.cfg
#define CVAR_USER_INFO		0x2  // added to user_info when changed
#define CVAR_SERVER_INFO	0x4  // added to server_info when changed
#define CVAR_LO_ONLY		0x8  // don't allow change when connected
#define CVAR_NO_SET			0x10  // don't allow change from console at all
#define CVAR_LATCH			0x20  // save changes until server restart
#define CVAR_R_IMAGES		0x40  // effects image filtering
#define CVAR_R_CONTEXT		0x80  // effects OpenGL context
#define CVAR_R_PROGRAMS		0x100  // effects GLSL programs
#define CVAR_S_DEVICE		0x200  // effects sound device parameters
#define CVAR_S_SAMPLES		0x400  // effects sound samples

#define CVAR_R_MASK			(CVAR_R_IMAGES | CVAR_R_CONTEXT | CVAR_R_PROGRAMS | CVAR_R_CONTEXT)
#define CVAR_S_MASK 		(CVAR_S_DEVICE | CVAR_S_SAMPLES)

typedef struct cvar_s {
	const char *name;
	const char *description;
	char *default_value;
	char *string;
	char *latched_string; // for CVAR_LATCH vars
	unsigned int flags;
	boolean_t modified; // set each time the cvar is changed
	float value;
	int integer;
	struct cvar_s *next;
} cvar_t;

// file opening modes
typedef enum {
	FILE_READ, FILE_WRITE, FILE_APPEND
} file_mode_t;

// server multicast scope for entities and events
typedef enum {
	MULTICAST_ALL,
	MULTICAST_PHS,
	MULTICAST_PVS,
	MULTICAST_ALL_R,
	MULTICAST_PHS_R,
	MULTICAST_PVS_R
} multicast_t;

// server to client communication
typedef enum {
	SV_CMD_BAD,
	SV_CMD_CBUF_TEXT, // [string] stuffed into client's console buffer, should be \n terminated
	SV_CMD_CENTER_PRINT, // [string] to put in center of the screen
	SV_CMD_CONFIG_STRING, // [short] [string]
	SV_CMD_DISCONNECT,
	SV_CMD_DOWNLOAD, // [short] size [size bytes]
	SV_CMD_ENTITY_BASELINE,
	SV_CMD_FRAME,
	SV_CMD_MUZZLE_FLASH,
	SV_CMD_PRINT, // [byte] id [string] null terminated string
	SV_CMD_RECONNECT,
	SV_CMD_SERVER_DATA, // [long] protocol ...
	SV_CMD_SOUND, // <see code>
	SV_CMD_TEMP_ENTITY,
} sv_cmd_t;

// additional command types can be defined and written by the game module
// and handled directly by the client game
#define SV_CMD_CGAME SV_CMD_TEMP_ENTITY + 1

// client to server
typedef enum {
	CL_CMD_BAD,
	CL_CMD_MOVE, // [[usercmd_t]
	CL_CMD_STRING, // [string] message
	CL_CMD_USER_INFO // [[user_info string]
} cl_cmd_t;

typedef float vec_t;
typedef vec_t vec2_t[2];
typedef vec_t vec3_t[3];
typedef vec_t vec4_t[4];

extern vec3_t vec3_origin;

#define DotProduct(x,y)			(x[0]*y[0]+x[1]*y[1]+x[2]*y[2])
#define VectorSubtract(a,b,c)	(c[0]=a[0]-b[0],c[1]=a[1]-b[1],c[2]=a[2]-b[2])
#define VectorAdd(a,b,c)		(c[0]=a[0]+b[0],c[1]=a[1]+b[1],c[2]=a[2]+b[2])
#define VectorScale(a,s,b)		(b[0]=a[0]*(s),b[1]=a[1]*(s),b[2]=a[2]*(s))
#define VectorCopy(a,b)			((b)[0]=(a)[0],(b)[1]=(a)[1],(b)[2]=(a)[2])
#define Vector4Copy(a,b)		(b[0]=a[0],b[1]=a[1],b[2]=a[2],b[3]=a[3])
#define VectorClear(a)			(a[0]=a[1]=a[2]=0)
#define VectorNegate(a,b)		(b[0]=-a[0],b[1]=-a[1],b[2]=-a[2])
#define VectorSet(v, x, y, z)	(v[0]=(x), v[1]=(y), v[2]=(z))
#define VectorSum(a)			(a[0] + a[1] + a[2])

#ifndef M_PI
#define M_PI 3.14159265358979323846  // matches value in gcc v2 math.h
#endif

#define DEG2RAD(a)				(a * M_PI) / 180.0F
#define ANGLE2SHORT(x)			((unsigned short)((x) * 65536 / 360.0) & 65535)
#define SHORT2ANGLE(x)			((x) * (360.0 / 65536.0))

// lower bits are stronger, and will eat weaker brushes completely
#define CONTENTS_SOLID			0x1  // an eye is never valid in a solid
#define CONTENTS_WINDOW			0x2  // translucent, but not watery
#define CONTENTS_AUX			0x4  // not used at the moment
#define CONTENTS_LAVA			0x8
#define CONTENTS_SLIME			0x10
#define CONTENTS_WATER			0x20
#define CONTENTS_MIST			0x40

#define LAST_VISIBLE_CONTENTS	0x40

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

#define CONTENTS_ORIGIN			0x1000000  // removed during bsp stage
#define CONTENTS_MONSTER		0x2000000  // should never be on a brush, only in game
#define CONTENTS_DEAD_MONSTER	0x4000000

#define CONTENTS_DETAIL			0x8000000  // brushes to be added after vis leafs
#define CONTENTS_TRANSLUCENT	0x10000000  // auto set if any surface has trans
#define CONTENTS_LADDER			0x20000000

// leafs will have some combination of the above, nodes will always be -1
#define CONTENTS_NODE			-1

// surface flags
#define SURF_LIGHT				0x1  // value will hold the light strength
#define SURF_SLICK				0x2  // effects game physics
#define SURF_SKY				0x4  // don't draw, but add to skybox
#define SURF_WARP				0x8  // turbulent water warp
#define SURF_BLEND_33			0x10  // 0.33 alpha blending
#define SURF_BLEND_66			0x20  // 0.66 alpha blending
#define SURF_FLOWING			0x40  // scroll towards angle, no longer supported
#define SURF_NO_DRAW			0x80  // don't bother referencing the texture
#define SURF_HINT				0x100  // make a primary bsp splitter
#define SURF_SKIP				0x200  // completely skip, allowing non-closed brushes
#define SURF_ALPHA_TEST			0x400  // alpha test (grates, foliage, etc..)
#define SURF_PHONG				0x800  // phong interpolated lighting at compile time
// content masks
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

// gi.BoxEdicts() can return a list of either solid or trigger entities
#define AREA_SOLID				1
#define AREA_TRIGGERS			2

// plane_t structure
typedef struct c_bsp_plane_s {
	vec3_t normal;
	float dist;
	int type; // for fast side tests
	int sign_bits; // signx + (signy << 1) + (signz << 1)
} c_bsp_plane_t;

typedef struct c_model_s {
	vec3_t mins, maxs;
	vec3_t origin; // for sounds or lights
	int head_node;
} c_model_t;

typedef struct c_bsp_surface_s {
	char name[32];
	int flags;
	int value;
} c_bsp_surface_t;

// a trace is returned when a box is swept through the world
typedef struct c_trace_s {
	boolean_t all_solid; // if true, plane is not valid
	boolean_t start_solid; // if true, the initial point was in a solid area
	float fraction; // time completed, 1.0 = didn't hit anything
	vec3_t end; // final position
	c_bsp_plane_t plane; // surface normal at impact
	c_bsp_surface_t *surface; // surface hit
	int leaf_num;
	int contents; // contents on other side of surface hit
	struct g_edict_s *ent; // not set by CM_*() functions
} c_trace_t;

// player bbox and view_height scaling
extern vec3_t PM_MINS;
extern vec3_t PM_MAXS;

#define PM_SCALE			1.2  // global player scale factor
#define PM_STAIR_HEIGHT		16.0  // maximum stair height player can walk up
#define PM_STAIR_NORMAL		0.7  // can't step up onto very steep slopes
// pmove_state_t is the information necessary for client side movement prediction
typedef enum {
	// can accelerate and turn
	PM_NORMAL,
	PM_SPECTATOR,
	// no acceleration or turning
	PM_DEAD,
	PM_FREEZE
} pm_type_t;

// pmove->pm_flags
#define PMF_DUCKED			0x1
#define PMF_JUMP_HELD		0x2
#define PMF_ON_GROUND		0x4
#define PMF_ON_STAIRS		0x8
#define PMF_TIME_WATERJUMP	0x10   // pm_time is time before control
#define PMF_TIME_LAND		0x20   // pm_time is time before rejump
#define PMF_TIME_TELEPORT	0x40   // pm_time is non-moving time
#define PMF_NO_PREDICTION	0x80   // temporarily disables prediction
#define PMF_PUSHED			0x100  // disables stair checking and velocity clamp
#define PMF_UNDER_WATER		0x200

#define PMF_TIME_MASK		(PMF_TIME_WATERJUMP | PMF_TIME_LAND | PMF_TIME_TELEPORT)

// this structure needs to be communicated bit-accurate
// from the server to the client to guarantee that
// prediction stays in sync, so no floats are used.
// if any part of the game code modifies this struct, it
// will result in a prediction error of some degree.
typedef struct pm_move_state_s {
	pm_type_t pm_type;

	short origin[3]; // 12.3
	short velocity[3]; // 12.3
	short pm_flags; // ducked, jump_held, etc
	byte pm_time; // each unit = 8 ms
	short gravity;
	short delta_angles[3]; // add to command angles to get view direction
// changed by spawns, rotating objects, and teleporters
} pm_move_state_t;

// button bits
#define BUTTON_ATTACK		1
#define BUTTON_WALK			2

// user_cmd_t is sent to the server each client frame
typedef struct user_cmd_s {
	byte msec;
	byte buttons;
	short angles[3];
	short forward, side, up;
} user_cmd_t;

#define MAX_TOUCH_ENTS 32
typedef struct {
	pm_move_state_t s; // state (in / out)

	user_cmd_t cmd; // command (in)

	int num_touch; // results (out)
	struct g_edict_s *touch_ents[MAX_TOUCH_ENTS];

	vec3_t angles; // clamped
	float view_height;

	vec3_t mins, maxs; // bounding box size

	struct g_edict_s *ground_entity;

	int water_type;
	int water_level;

	// callbacks to test the world
	c_trace_t (*Trace)(vec3_t start, vec3_t mins, vec3_t maxs, vec3_t end);
	int (*PointContents)(vec3_t point);
} pm_move_t;

// entity_state_t->effects
// handled on the client side (particles, lights, ..)
// an entity that has effects will be sent to the client
// even if it has a zero index model.
#define EF_ROTATE			(1 << 0)  // rotate on z
#define EF_BOB				(1 << 1)  // bob on z
#define EF_PULSE			(1 << 2)  // pulsate lighting color
#define EF_GRENADE			(1 << 3)  // smoke trail above water, bubble trail in water
#define EF_ROCKET			(1 << 4)  // smoke trail above water, bubble trail in water
#define EF_HYPERBLASTER		(1 << 5)  // bubble trail in water
#define EF_LIGHTNING		(1 << 6)  // lightning bolt
#define EF_BFG				(1 << 7)  // big particle snotball
#define EF_TELEPORTER		(1 << 8)  // particle fountain
#define EF_QUAD				(1 << 9)  // quad damage
#define EF_CTF_BLUE			(1 << 10)  // blue flag carrier
#define EF_CTF_RED			(1 << 11)  // red flag carrier
#define EF_BEAM				(1 << 12)  // overload old_origin for 2nd endpoint
// small or full-bright entities can skip static and dynamic lighting
#define EF_NO_LIGHTING		(EF_ROCKET)

// the 16 high bits are never transmitted, they're for the renderer only
#define EF_WEAPON			(1 << 27)  // view weapon
#define EF_ALPHATEST		(1 << 28)  // alpha test
#define EF_BLEND			(1 << 29)  // blend
#define EF_NO_SHADOW		(1 << 30)  // no shadow
#define EF_NO_DRAW			(1 << 31)  // no draw (but perhaps shadow)

// muzzle flashes
typedef enum {
	MZ_SHOTGUN,
	MZ_SSHOTGUN,
	MZ_MACHINEGUN,
	MZ_GRENADE,
	MZ_ROCKET,
	MZ_HYPERBLASTER,
	MZ_LIGHTNING,
	MZ_RAILGUN,
	MZ_BFG,
	MZ_LOGOUT,
} muzzle_flash_t;

// Temp entity events are for things that happen
// at a location separate from any existing entity.
// Temporary entity messages are explicitly constructed
// and broadcast.
typedef enum {
	TE_TRACER,
	TE_BULLET,
	TE_BURN,
	TE_BLOOD,
	TE_SPARKS,
	TE_HYPERBLASTER,
	TE_LIGHTNING,
	TE_RAIL,
	TE_EXPLOSION,
	TE_BUBBLES,
	TE_BFG,
	TE_GIB
} temp_event_t;

// sound attenuation values
#define ATTN_NONE  			0  // full volume the entire level
#define ATTN_NORM  			1
#define ATTN_IDLE  			2
#define ATTN_STATIC  		3  // diminish very rapidly with distance
#define DEFAULT_SOUND_ATTENUATION	ATTN_NORM

// player_state->stats[] indexes
#define STAT_HEALTH_ICON	0
#define STAT_HEALTH			1
#define STAT_AMMO_ICON		2
#define STAT_AMMO			3
#define STAT_AMMO_LOW		4
#define STAT_ARMOR_ICON		5
#define STAT_ARMOR			6
#define STAT_PICKUP_ICON	7
#define STAT_PICKUP_STRING	8
#define STAT_WEAPON_ICON	9
#define STAT_WEAPON			10
#define STAT_TEAM			11
#define STAT_FRAGS			12
#define STAT_CAPTURES		13
#define STAT_SPECTATOR		14
#define STAT_CHASE			15
#define STAT_VOTE			16
#define STAT_TIME			17
#define STAT_ROUND			18
#define STAT_READY			19
#define STAT_SCORES			20
#define STAT_GENERAL		STAT_SCORES  // for mods to extend
#define MAX_STATS			32

// -4096 up to +4096
#define MAX_WORLD_WIDTH		4096

/*
 * ConfigStrings are a general means of communication from the server to all
 * connected clients. Each ConfigString can be at most MAX_STRING_CHARS in length.
 */
#define CS_NAME				0
#define CS_GRAVITY			1
#define CS_SKY				2
#define CS_WEATHER			3
#define CS_PAK				4  // pak for current level
#define CS_BSP_SIZE			5  // for catching incompatible maps
#define CS_GAMEPLAY			6  // gameplay string
#define CS_TEAMS			7
#define CS_CTF				8
#define CS_MATCH			9
#define CS_ROUNDS			10
#define CS_TEAM_GOOD		11  // team names
#define CS_TEAM_EVIL		12
#define CS_TIME				13  // level or match timer
#define CS_ROUND			14
#define CS_VOTE				15  // vote string\yes count\no count
#define CS_MODELS			16
#define CS_SOUNDS			(CS_MODELS + MAX_MODELS)
#define CS_MUSICS			(CS_SOUNDS + MAX_SOUNDS)
#define CS_IMAGES			(CS_MUSICS + MAX_MUSICS)
#define CS_ITEMS			(CS_IMAGES + MAX_IMAGES)
#define CS_CLIENT_INFO		(CS_ITEMS + MAX_ITEMS)
#define CS_GENERAL			(CS_CLIENT_INFO + MAX_CLIENTS)

#define MAX_CONFIG_STRINGS	(CS_GENERAL + MAX_GENERAL)

/*
 * Entity animation sequences (player animations) are resolved on the server
 * but are run (interpolated) on the client.
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

#define ANIM_TOGGLE_BIT 0x80  // used to restart the same animation
/*
 * Entity events are instantaneous, transpiring at an entity's origin for
 * precisely one frame.
 */
typedef enum {
	EV_NONE,
	EV_ITEM_RESPAWN,
	EV_ITEM_PICKUP,
	EV_CLIENT_FOOTSTEP,
	EV_CLIENT_LAND,
	EV_CLIENT_FALL,
	EV_CLIENT_FALL_FAR,
	EV_CLIENT_JUMP,
	EV_TELEPORT
} entity_event_t;

/*
 * Entity states are transmitted by the server to the client using delta
 * compression.  The client parses these states and adds or removes entities
 * from the scene as needed.
 */
typedef struct entity_state_s {
	unsigned short number; // edict index

	vec3_t origin;
	vec3_t old_origin; // for interpolating

	vec3_t angles;

	byte animation1, animation2; // animations (running, attacking, ..)

	byte event; // client side events (particles, lights, ..)

	unsigned short effects; // particles, lights, etc..

	byte model1, model2, model3, model4; // primary model, linked models

	byte client; // client info index

	byte sound; // looped sounds

	/*
	 * Encoded bounding box dimensions for mesh entities.  This facilitates
	 * client-sided prediction so that players don't e.g. run through each
	 * other.  See gi.LinkEntity.
	 */
	unsigned short solid;
} entity_state_t;

/*
 * Player state structures contain authoritative snapshots of the player's
 * movement, as well as the player's statistics (inventory, health, etc.).
 */
typedef struct player_state_s {
	pm_move_state_t pmove;
	vec3_t angles; // for fixed views like chase camera & demo recording
	short stats[MAX_STATS]; // status bar updates
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
#define CASSERT(x) extern int ASSERT_COMPILE[((x) != 0) * 2 - 1]

#endif /* __QUAKE2WORLD_H__ */
