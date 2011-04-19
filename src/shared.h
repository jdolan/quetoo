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

#ifndef __SHARED_H__
#define __SHARED_H__

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

#ifndef _MSC_VER
typedef unsigned char byte;
#endif

#if defined _WIN64
# define Q2W_SIZE_T "%I64u"
#elif defined _WIN32
# define Q2W_SIZE_T "%u"
#else
# define Q2W_SIZE_T "%zu"
#endif

#ifndef true
# define false 0
# define true 1
#endif

typedef byte qboolean;

// byte ordering facilities
extern void Swap_Init(void);
extern short BigShort(short l);
extern short LittleShort(short l);
extern int BigLong(int l);
extern int LittleLong(int l);
extern float BigFloat(float l);
extern float LittleFloat(float l);

// angle indexes
#define PITCH				0  // up / down
#define YAW					1  // left / right
#define ROLL				2  // tilt / lean

// plane sides
#define SIDE_FRONT			0
#define SIDE_ON				2
#define SIDE_BACK			1
#define SIDE_CROSS			-2

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
#define MAX_IMAGES			256
#define MAX_ITEMS			256
#define MAX_GENERAL			(MAX_CLIENTS * 2)  // general config strings

// game print flags
#define PRINT_LOW			0  // pickup messages
#define PRINT_MEDIUM		1  // death messages
#define PRINT_HIGH			2  // critical messages
#define PRINT_CHAT			3  // chat messages
#define PRINT_TEAMCHAT		4  // teamchat messages

// file opening modes
typedef enum {
	FILE_READ, FILE_WRITE, FILE_APPEND
} file_mode_t;

// destination class for gi.multicast()
typedef enum {
	MULTICAST_ALL,
	MULTICAST_PHS,
	MULTICAST_PVS,
	MULTICAST_ALL_R,
	MULTICAST_PHS_R,
	MULTICAST_PVS_R
} multicast_t;

/*

MATH LIBRARY

*/

#ifndef M_PI
#define M_PI 3.14159265358979323846  // matches value in gcc v2 math.h
#endif

float frand(void);  // 0 to 1
float crand(void);  // -1 to 1

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

void ClearBounds(vec3_t mins, vec3_t maxs);
void AddPointToBounds(const vec3_t v, vec3_t mins, vec3_t maxs);

qboolean VectorCompare(const vec3_t v1, const vec3_t v2);
qboolean VectorNearer(const vec3_t v1, const vec3_t v2, const vec3_t comp);

vec_t VectorNormalize(vec3_t v);  // returns vector length
vec_t VectorLength(const vec3_t v);
void VectorMix(const vec3_t v1, const vec3_t v2, const float mix, vec3_t out);
void VectorMA(const vec3_t veca, const vec_t scale, const vec3_t vecb, vec3_t vecc);
void CrossProduct(const vec3_t v1, const vec3_t v2, vec3_t cross);
vec_t ColorNormalize(const vec3_t in, vec3_t out);
void ColorFilter(const vec3_t in, vec3_t out, float brightness, float saturation, float contrast);

#define DEG2RAD(a)				(a * M_PI) / 180.0F

void ConcatRotations(vec3_t in1[3], vec3_t in2[3], vec3_t out[3]);

void VectorAngles(const vec3_t vector, vec3_t angles);
void AngleVectors(const vec3_t angles, vec3_t forward, vec3_t right, vec3_t up);

void VectorLerp(const vec3_t from, const vec3_t to, const vec_t frac, vec3_t out);
void AngleLerp(const vec3_t from, const vec3_t to, const vec_t frac, vec3_t out);

struct cplane_s;
int BoxOnPlaneSide(const vec3_t emins, const vec3_t emaxs, const struct cplane_s *plane);

#define BOX_ON_PLANE_SIDE(emins, emaxs, p)	\
	(((p)->type <= PLANE_Z)?				\
	(										\
		((p)->dist <= (emins)[(p)->type])?	\
			1								\
		:									\
		(									\
			((p)->dist >= (emaxs)[(p)->type])?\
				2							\
			:								\
				3							\
		)									\
	)										\
	:										\
		BoxOnPlaneSide((emins),(emaxs),(p)))

void ProjectPointOnPlane(vec3_t dst, const vec3_t p, const vec3_t normal);
void PerpendicularVector(vec3_t dst, const vec3_t src);
void TangentVectors(const vec3_t normal, const vec3_t sdir, const vec3_t tdir, vec4_t tangent, vec3_t bitangent);
void RotatePointAroundVector(vec3_t dst, const vec3_t dir, const vec3_t point, float degrees);

int Com_GlobMatchStar(const char *pattern, const char *text);
int Com_GlobMatch(const char *pattern, const char *text);

qboolean Com_Mixedcase(const char *s);
char *Com_CommonPrefix(const char *words[], int nwords);
char *Com_Lowercase(char *s);
char *Com_Trim(char *s);
const char *Com_Basename(const char *path);
void Com_Dirname(const char *in, char *out);
void Com_StripExtension(const char *in, char *out);
void Com_StripColor(const char *in, char *out);
char *Com_TrimString(char *in);

// data is an in/out parm, returns a parsed out token
char *Com_Parse(const char **data_p);

// variable argument string concatenation
char *va(const char *format, ...) __attribute__((format(printf, 1, 2)));

// key / value info strings
#define MAX_INFO_KEY	64
#define MAX_INFO_VALUE	64
#define MAX_INFO_STRING	512

char *Info_ValueForKey(const char *s, const char *key);
void Info_RemoveKey(char *s, const char *key);
void Info_SetValueForKey(char *s, const char *key, const char *value);
qboolean Info_Validate(const char *s);

// resolve palette colors 0-255 by their names
int ColorByName(const char *s, int def);

// this is only here so the functions in shared.c and q_sh.c can link
void Com_Print(const char *fmt, ...) __attribute__((format(printf, 1, 2)));

/*

CVARS (console variables)

*/

#define CVAR_ARCHIVE		0x1  // set to cause it to be saved to vars.rc
#define CVAR_USER_INFO		0x2  // added to user_info when changed
#define CVAR_SERVER_INFO	0x4  // added to serverinfo when changed
#define CVAR_NOSET			0x8  // don't allow change from console at all
#define CVAR_LATCH			0x10  // save changes until server restart
#define CVAR_R_IMAGES		0x20  // effects image filtering
#define CVAR_R_CONTEXT		0x40  // effects OpenGL context
#define CVAR_R_PROGRAMS		0x80  // effects GLSL programs
#define CVAR_R_MODE			0x100  // effects screen resolution
#define CVAR_S_DEVICE		0x200  // effects sound device parameters
#define CVAR_S_SAMPLES		0x400  // effects sound samples

#define CVAR_R_MASK			(CVAR_R_IMAGES | CVAR_R_CONTEXT | CVAR_R_PROGRAMS | CVAR_R_MODE)

#define CVAR_S_MASK 		(CVAR_S_DEVICE | CVAR_S_SAMPLES)

// nothing outside the Cvar_*() functions should modify these fields!
typedef struct cvar_s {
	const char *name;
	const char *description;
	char *string;
	char *latched_string;  // for CVAR_LATCH vars
	int flags;
	qboolean modified;  // set each time the cvar is changed
	float value;
	int integer;
	struct cvar_s *next;
} cvar_t;

/*

COLLISION DETECTION

*/

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
#define CONTENTS_AREA_PORTAL		0x8000
#define CONTENTS_PLAYERCLIP		0x10000
#define CONTENTS_MONSTERCLIP	0x20000

// currents can be added to any other contents, and may be mixed
#define CONTENTS_CURRENT_0		0x40000
#define CONTENTS_CURRENT_90		0x80000
#define CONTENTS_CURRENT_180	0x100000
#define CONTENTS_CURRENT_270	0x200000
#define CONTENTS_CURRENT_UP		0x400000
#define CONTENTS_CURRENT_DOWN	0x800000

#define CONTENTS_ORIGIN			0x1000000  // removed before bsping an entity

#define CONTENTS_MONSTER		0x2000000  // should never be on a brush, only in game
#define CONTENTS_DEADMONSTER	0x4000000

#define CONTENTS_DETAIL			0x8000000  // brushes to be added after vis leafs
#define CONTENTS_TRANSLUCENT	0x10000000  // auto set if any surface has trans
#define CONTENTS_LADDER			0x20000000

// leafs will have some combination of the above, nodes will always be -1
#define CONTENTS_NODE			-1

// surface flags
#define SURF_LIGHT		0x1  // value will hold the light strength
#define SURF_SLICK		0x2  // effects game physics
#define SURF_SKY		0x4  // don't draw, but add to skybox
#define SURF_WARP		0x8  // turbulent water warp
#define SURF_BLEND33	0x10  // 0.33 alpha blending
#define SURF_BLEND66	0x20  // 0.66 alpha blending
#define SURF_FLOWING	0x40  // scroll towards angle, no longer supported
#define SURF_NO_DRAW	0x80  // don't bother referencing the texture
#define SURF_HINT		0x100  // make a primary bsp splitter
#define SURF_SKIP		0x200  // completely skip, allowing non-closed brushes
#define SURF_ALPHA_TEST	0x400  // alpha test (grates, foliage, etc..)
#define SURF_PHONG		0x800  // phong interpolated lighting at compile time

// content masks
#define MASK_ALL			(-1)
#define MASK_SOLID			(CONTENTS_SOLID|CONTENTS_WINDOW)
#define MASK_PLAYERSOLID	(CONTENTS_SOLID|CONTENTS_PLAYERCLIP|CONTENTS_WINDOW|CONTENTS_MONSTER)
#define MASK_DEADSOLID		(CONTENTS_SOLID|CONTENTS_PLAYERCLIP|CONTENTS_WINDOW)
#define MASK_MONSTERSOLID	(CONTENTS_SOLID|CONTENTS_MONSTERCLIP|CONTENTS_WINDOW|CONTENTS_MONSTER)
#define MASK_WATER			(CONTENTS_WATER|CONTENTS_LAVA|CONTENTS_SLIME)
#define MASK_OPAQUE			(CONTENTS_SOLID|CONTENTS_SLIME|CONTENTS_LAVA)
#define MASK_SHOT			(CONTENTS_SOLID|CONTENTS_MONSTER|CONTENTS_WINDOW|CONTENTS_DEADMONSTER)
#define MASK_CURRENT		(CONTENTS_CURRENT_0|CONTENTS_CURRENT_90|CONTENTS_CURRENT_180|\
							 CONTENTS_CURRENT_270|CONTENTS_CURRENT_UP|CONTENTS_CURRENT_DOWN)

// gi.BoxEdicts() can return a list of either solid or trigger entities
// FIXME: eliminate AREA_ distinction?
#define AREA_SOLID 1
#define AREA_TRIGGERS 2

// plane_t structure
typedef struct cplane_s {
	vec3_t normal;
	float dist;
	int type;  // for fast side tests
	int sign_bits;  // signx + (signy << 1) + (signz << 1)
} c_plane_t;

typedef struct c_model_s {
	vec3_t mins, maxs;
	vec3_t origin;  // for sounds or lights
	int head_node;
} c_model_t;

typedef struct csurface_s {
	char name[32];
	int flags;
	int value;
} c_surface_t;

// a trace is returned when a box is swept through the world
typedef struct {
	qboolean all_solid;  // if true, plane is not valid
	qboolean start_solid;  // if true, the initial point was in a solid area
	float fraction;  // time completed, 1.0 = didn't hit anything
	vec3_t end;  // final position
	c_plane_t plane;  // surface normal at impact
	c_surface_t *surface;  // surface hit
	int leaf_num;
	int contents;  // contents on other side of surface hit
	struct edict_s *ent;  // not set by CM_*() functions
} trace_t;

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

#define PMF_TIME_MASK		(PMF_TIME_WATERJUMP | PMF_TIME_LAND | PMF_TIME_TELEPORT)

// this structure needs to be communicated bit-accurate
// from the server to the client to guarantee that
// prediction stays in sync, so no floats are used.
// if any part of the game code modifies this struct, it
// will result in a prediction error of some degree.
typedef struct {
	pm_type_t pm_type;

	short origin[3];  // 12.3
	short velocity[3];  // 12.3
	short pm_flags;  // ducked, jump_held, etc
	byte pm_time;  // each unit = 8 ms
	short gravity;
	short delta_angles[3];  // add to command angles to get view direction
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

#define MAXTOUCH 32
typedef struct {
	pm_move_state_t s;  // state (in / out)

	user_cmd_t cmd;  // command (in)

	int num_touch;  // results (out)
	struct edict_s *touch_ents[MAXTOUCH];

	vec3_t angles;  // clamped
	float view_height;

	vec3_t mins, maxs;  // bounding box size

	struct edict_s *ground_entity;

	int water_type;
	int water_level;

	// callbacks to test the world
	trace_t (*trace)(vec3_t start, vec3_t mins, vec3_t maxs, vec3_t end);
	int (*pointcontents)(vec3_t point);
} pm_move_t;

// entity_state_t->effects
// handled on the client side (particles, frame animations, ..)
// an entity that has effects will be sent to the client
// even if it has a zero index model.
#define EF_ROTATE			(1 << 0)  // rotate on z
#define EF_BOB				(1 << 1)  // bob on z
#define EF_PULSE			(1 << 2)  // pulsate lighting color
#define EF_ANIMATE			(1 << 3)  // automatically cycle through all frames at 2hz
#define EF_ANIMATE_FAST		(1 << 4)  // automatically cycle through all frames at 10hz
#define EF_GRENADE			(1 << 5)  // smoke trail above water, bubble trail in water
#define EF_ROCKET			(1 << 6)  // smoke trail above water, bubble trail in water
#define EF_HYPERBLASTER		(1 << 7)  // bubble trail in water
#define EF_LIGHTNING		(1 << 8)  // lightning bolt
#define EF_BFG				(1 << 9)  // big particle snotball
#define EF_TELEPORTER		(1 << 10)  // particle fountain
#define EF_QUAD				(1 << 11)  // quad damage
#define EF_CTF_BLUE			(1 << 12)  // blue flag carrier
#define EF_CTF_RED			(1 << 13)  // red flag carrier
#define EF_BEAM				(1 << 14)  // overload old_origin for 2nd endpoint

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

// temp entity events
// Temp entity events are for things that happen
// at a location seperate from any existing entity.
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
#define STAT_ARMOR_ICON		4
#define STAT_ARMOR			5
#define STAT_PICKUP_ICON	6
#define STAT_PICKUP_STRING	7
#define STAT_WEAPICON		8
#define STAT_WEAPON			9
#define STAT_LAYOUTS		10
#define STAT_FRAGS			11
#define STAT_SPECTATOR		12
#define STAT_CHASE			13
#define STAT_VOTE			14
#define STAT_TEAMNAME		15
#define STAT_TIME			16
#define STAT_READY			17
#define STAT_AMMO_LOW		18
#define STAT_GENERAL		STAT_AMMO_LOW  // for mods to extend
#define MAX_STATS			32

#define ANGLE2SHORT(x)	((int)((x)*65536/360) & 65535)
#define SHORT2ANGLE(x)	((x)*(360.0/65536))

// -4096 up to +4096
#define MAX_WORLD_WIDTH 4096

// config strings are a general means of communication from
// the server to all connected clients.
// Each config string can be at most MAX_QPATH characters.
#define CS_NAME				0
#define CS_GRAVITY			1
#define CS_SKY				2
#define CS_WEATHER			3
#define CS_PAK				4  // pak for current level
#define CS_LAYOUT		5  // display program string

#define CS_MAX_CLIENTS		30
#define CS_BSP_SIZE			31  // for catching incompatible maps

#define CS_MODELS			32
#define CS_SOUNDS			(CS_MODELS + MAX_MODELS)
#define CS_MUSICS			(CS_SOUNDS + MAX_SOUNDS)
#define CS_IMAGES			(CS_MUSICS + MAX_MUSICS)
#define CS_ITEMS			(CS_IMAGES + MAX_IMAGES)
#define CS_PLAYER_SKINS		(CS_ITEMS + MAX_ITEMS)
#define CS_GENERAL			(CS_PLAYER_SKINS + MAX_CLIENTS)

#define CS_VOTE				(CS_GENERAL + 1)  // current vote
#define CS_TEAM_GOOD		(CS_GENERAL + 2)  // team names
#define CS_TEAM_EVIL		(CS_GENERAL + 3)
#define CS_TIME				(CS_GENERAL + 4)  // level/match timer

#define MAX_CONFIG_STRINGS	(CS_GENERAL + MAX_GENERAL)


// entity_state_t->event values
// entity events are for effects that take place relative
// to an existing entities origin.  Very network efficient.
// All muzzle flashes really should be converted to events...
typedef enum {
	EV_NONE,
	EV_ITEM_RESPAWN,
	EV_ITEM_PICKUP,
	EV_FOOTSTEP,
	EV_FALLSHORT,
	EV_FALL,
	EV_FALLFAR,
	EV_TELEPORT
} entity_event_t;

// entity_state_t is the information conveyed from the server
// in an update message about entities that the client will
// need to render in some way
typedef struct entity_state_s {
	int number;  // edict index

	vec3_t origin;
	vec3_t angles;
	vec3_t old_origin;  // for lerping

	// primary model, weapon model, CTF flag, etc
	byte model_index, model_index2, model_index3, model_index4;
	byte frame;  // frame number for animations

	unsigned short skin_num;  // masked off for players
	unsigned short effects;  // particles, lights, envmapping, ..
	unsigned short solid;  // for client side prediction, 8 * (bits 0-4) is x/y radius
	// 8 * (bits 5-9) is z down distance, 8 * (bits10-15) is z up
	// gi.linkentity sets this properly
	byte sound;  // for looping sounds, to guarantee shutoff
	byte event;  // muzzle flashes, footsteps, etc
	// events only go out for a single frame, they
	// are automatically cleared each frame
} entity_state_t;

// player_state_t is the information needed in addition to pmove_state_t
// to rendered a view.  There will only be 10 player_state_t sent each second,
// but the number of pmove_state_t changes will be reletive to client
// frame rates
typedef struct {
	pm_move_state_t pmove;  // for prediction
	// these fields do not need to be communicated bit-precise
	vec3_t angles;  // for fixed views like chasecams
	short stats[MAX_STATS];  // fast status bar updates
} player_state_t;


// colored text escape and code defs
#define COLOR_ESC		'^'

#define CON_COLOR_BLACK		0
#define CON_COLOR_RED		1
#define CON_COLOR_GREEN		2
#define CON_COLOR_YELLOW	3
#define CON_COLOR_BLUE		4
#define CON_COLOR_CYAN		5
#define CON_COLOR_MAGENTA	6
#define CON_COLOR_WHITE		7

#define MAX_COLORS		8

#define CON_COLOR_DEFAULT		CON_COLOR_WHITE
#define CON_COLOR_ALT		CON_COLOR_GREEN

#define CON_COLOR_INFO		CON_COLOR_ALT
#define CON_COLOR_CHAT		CON_COLOR_ALT
#define CON_COLOR_TEAMCHAT		CON_COLOR_YELLOW

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

#endif /* __SHARED_H__ */
