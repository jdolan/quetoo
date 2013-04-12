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

#ifndef __S_TYPES_H__
#define __S_TYPES_H__

#include <SDL/SDL_mixer.h>

#include "common.h"
#include "sys.h"

// media handles
typedef struct s_media_s {
	char name[MAX_QPATH];
	uint32_t seed;
	bool (*Retain)(struct s_media_s *self);
	void (*Free)(struct s_media_s *self);
} s_media_t;

typedef struct s_sample_s {
	s_media_t media;
	Mix_Chunk *chunk;
	bool alias;
} s_sample_t;

typedef struct s_channel_s {
	vec3_t org; // for temporary entities and other positioned sounds
	int32_t ent_num; // for entities and dynamic sounds
	int32_t count; // for looped sounds
	int32_t atten;
	s_sample_t *sample;
} s_channel_t;

#define MAX_CHANNELS 64

typedef struct s_music_s {
	s_media_t media;
	void *buffer;
	SDL_RWops *rw;
	Mix_Music *music;
} s_music_t;

// the sound environment
typedef struct s_env_s {
	s_channel_t channels[MAX_CHANNELS];
	bool initialized; // is the sound subsystem initialized
	bool update; // inform the client of state changes
} s_env_t;

#endif /* __S_TYPES_H__ */
