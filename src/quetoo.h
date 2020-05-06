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

/**
 * @return The number of elements, rather than the number of bytes.
 */
#ifndef lengthof
	#define lengthof(x) (sizeof(x) / sizeof(x[0]))
#endif

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
	MEM_TAG_COLLISION,
	MEM_TAG_POLYLIB,
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

#define SECONDS_TO_MILLIS(t)	((t) * 1000.0)
#define MILLIS_TO_SECONDS(t)	((t) / 1000.0)

#define FRAMES_TO_SECONDS(t)	(1000.0 / (t))



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
 * @brief Contents masks: frequently combined contents flags.
 */
#define CONTENTS_MASK_SOLID				(CONTENTS_SOLID | CONTENTS_WINDOW)
#define CONTENTS_MASK_LIQUID			(CONTENTS_WATER | CONTENTS_LAVA | CONTENTS_SLIME)
#define CONTENTS_MASK_MEAT				(CONTENTS_MONSTER | CONTENTS_DEAD_MONSTER)
#define CONTENTS_MASK_CLIP_CORPSE		(CONTENTS_MASK_SOLID | CONTENTS_PLAYER_CLIP)
#define CONTENTS_MASK_CLIP_PLAYER		(CONTENTS_MASK_CLIP_CORPSE | CONTENTS_MONSTER)
#define CONTENTS_MASK_CLIP_MONSTER		(CONTENTS_MASK_CLIP_PLAYER | CONTENTS_MONSTER_CLIP)
#define CONTENTS_MASK_CLIP_PROJECTILE	(CONTENTS_MASK_SOLID | CONTENTS_MASK_MEAT)

/**
 * @brief Texinfo flags.
 */
#define SURF_LIGHT				0x1 // value will hold the light radius
#define SURF_SLICK				0x2 // affects game physics
#define SURF_SKY				0x4 // don't draw, but add to skybox
#define SURF_LIQUID				0x8 // water, lava, slime, etc.
#define SURF_BLEND_33			0x10 // 0.33 alpha blending
#define SURF_BLEND_66			0x20 // 0.66 alpha blending
#define SURF_FLOWING			0x40 // scroll towards angle, no longer supported
#define SURF_NO_DRAW			0x80 // don't bother referencing the texture
#define SURF_HINT				0x100 // make a primary bsp splitter
#define SURF_SKIP				0x200 // completely skip, allowing non-closed brushes
#define SURF_ALPHA_TEST			0x400 // alpha test (grates, foliage, etc..)
#define SURF_PHONG				0x800 // phong interpolated lighting at compile time
#define SURF_MATERIAL			0x1000 // retain the geometry, but don't draw diffuse pass
#define SURF_NO_WELD			0x2000 // don't weld (merge vertices) during face creation
#define SURF_DEBUG_LUXEL		0x10000000 // generate luxel debugging information in quemap

/**
 * @brief Texinfos with these flags should not be considered equal for draw elements merging.
 */
#define SURF_MASK_TEXINFO_CMP	~(SURF_LIGHT | SURF_PHONG | SURF_NO_WELD | SURF_DEBUG_LUXEL)

/**
 * @brief Texinfos with these flags require transparency.
 */
#define SURF_MASK_BLEND			(SURF_BLEND_33 | SURF_BLEND_66)

/**
 * @brief Texinfos with these flags imply translucent contents.
 */
#define SURF_MASK_TRANSLUCENT	(SURF_ALPHA_TEST | SURF_MASK_BLEND)

/**
 * @brief Texinfos with these flags will not have lightmap data.
 */
#define SURF_MASK_NO_LIGHTMAP	(SURF_SKY | SURF_NO_DRAW | SURF_HINT)

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
 * safely encode using 16 bit integers (float * 8.0).
 */
#define MIN_WORLD_COORD		-4096.0
#define MAX_WORLD_COORD		 4096.0
#define MAX_WORLD_AXIAL		(MAX_WORLD_COORD - MIN_WORLD_COORD)

/**
 * @brief Therefore, the maximum distance across the world is the
 * sqrtf((2 * 4096.0)^2 + (2 * 4096.0)^2) = 11585.237
 */
#define MAX_WORLD_DIST		 11586.0
