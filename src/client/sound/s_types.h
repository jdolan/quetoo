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
#include "quake2world.h"

typedef struct s_sample_s {
	char name[MAX_QPATH];
	Mix_Chunk *chunk;
	boolean_t alias;
} s_sample_t;

#define MAX_SAMPLES (MAX_SOUNDS * 2)  // max server side sounds, plus room for local samples

typedef struct s_channel_s {
	vec3_t org;  // for temporary entities and other positioned sounds
	int ent_num;  // for entities and dynamic sounds
	int count;  // for looped sounds
	int atten;
	s_sample_t *sample;
} s_channel_t;

#define MAX_CHANNELS 64

typedef struct s_music_s {
	char name[MAX_QPATH];
	Mix_Music *music;
	void *buffer;  // remains resident while the music is active
} s_music_t;

// the sound environment
typedef struct s_env_s {
	s_sample_t samples[MAX_SAMPLES];
	int num_samples;

	s_channel_t channels[MAX_CHANNELS];

	s_music_t musics[MAX_MUSICS];
	int num_musics;

	s_music_t *active_music;

	boolean_t initialized;

	boolean_t update;  // inform the client of state changes
} s_env_t;

#endif /* __S_TYPES_H__ */
