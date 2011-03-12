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

#ifndef __SOUND_H__

#include <SDL/SDL_mixer.h>

typedef struct s_sample_s {
	char name[MAX_QPATH];
	Mix_Chunk *chunk;
	qboolean alias;
} s_sample_t;

#define MAX_SAMPLES 256

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

	qboolean initialized;
	qboolean update;
} s_env_t;

extern s_env_t s_env;

extern cvar_t *s_music_volume;
extern cvar_t *s_rate;
extern cvar_t *s_reverse;
extern cvar_t *s_volume;

// s_main.c
void S_Frame(void);
void S_LoadMedia(void);
void S_Init(void);
void S_Shutdown(void);

// s_mix.c
void S_FreeChannel(int c);
void S_SpatializeChannel(s_channel_t *channel);
void S_PlaySample(const vec3_t org, int ent_num, s_sample_t *sample, int atten);
void S_LoopSample(const vec3_t org, s_sample_t *sample);
void S_StartLocalSample(const char *name);

// s_music.c
void S_LoadMusics(void);
void S_FrameMusic(void);
void S_InitMusic(void);
void S_ShutdownMusic(void);

// s_sample.c
s_sample_t *S_LoadSample(const char *name);
s_sample_t *S_LoadModelSample(entity_state_t *ent, const char *name);
void S_LoadSamples(void);
void S_FreeSamples(void);

#endif /* __SOUND_H__ */
