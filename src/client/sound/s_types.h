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

#include <AL/al.h>
#include <AL/alc.h>

#include "s_al_ext.h"

#include <SDL_rwops.h>

#include <sndfile.h>

#include "common.h"
#include "filesystem.h"
#include "sys.h"

typedef enum {
	S_MEDIA_GENERIC, // generic/unknown sound media

	S_MEDIA_SAMPLE, // a sample, which may be aliasing another piece of media
	S_MEDIA_MUSIC // a piece of music
} s_media_type_t;

// media handles
typedef struct s_media_s {
	char name[MAX_QPATH];
	s_media_type_t type;
	GList *dependencies;
	_Bool (*Retain)(struct s_media_s *self);
	void (*Free)(struct s_media_s *self);
	int32_t seed;
} s_media_t;

typedef struct s_sample_s {
	s_media_t media;
	ALuint buffer;
	sf_count_t num_samples; // number of samples total
	_Bool stereo; // whether this is stereo sample or not; they can't be spatialized
} s_sample_t;

#define S_PLAY_POSITIONED   0x1 // position the sound at a fixed origin
#define S_PLAY_ENTITY       0x2 // position the sound at the entity's origin at each frame
#define S_PLAY_AMBIENT      0x4 // this is an ambient sound, and may be culled by the user
#define S_PLAY_LOOP         0x8 // loop the sound continuously
#define S_PLAY_FRAME        0x10 // cull the sound if it is not added at each frame

#define TONES_PER_OCTAVE	48

typedef struct s_play_sample_s {
	const s_sample_t *sample;
	vec3_t origin;
	int32_t entity;
	sound_atten_t atten;
	int32_t flags;
	int32_t pitch; // pitch offset; 0 is no adjustment, TONES_PER_OCTAVE is +1 octave, -TONES_PER_OCTAVE is -1 octave, etc.
} s_play_sample_t;

typedef struct s_channel_s {
	s_play_sample_t play;
	const s_sample_t *sample;
	uint32_t start_time;
	int32_t frame;
	vec3_t position;
	vec3_t velocity;
	float gain;
	float pitch;
	ALuint filter;
	_Bool relative; // sound is relative to listener
} s_channel_t;

#define MAX_CHANNELS 128

typedef struct s_music_s {
	s_media_t media;
	SF_INFO info;
	SNDFILE *snd;
	file_t *file;
	_Bool eof; // whether we're out of samples or not
} s_music_t;

/**
 * @brief Filters used by the sound system if s_effects is enabled & supported.
 */
typedef struct {
	ALuint occluded;
	ALuint underwater;

	_Bool loaded; // whether the above are currently loaded.
} s_effects_t;

/**
 * @brief The sound environment.
 */
typedef struct s_env_s {
	s_channel_t channels[MAX_CHANNELS];
	uint16_t num_active_channels;

	/**
	 * @brief The OpenAL playback device.
	 */
	ALCdevice *device;

	/**
	 * @brief The OpenAL playback context.
	 */
	ALCcontext *context;

	const char *renderer;
	const char *vendor;
	const char *version;

	size_t raw_sample_buffer_size;
	float *raw_sample_buffer;

	size_t converted_sample_buffer_size;
	int16_t *converted_sample_buffer; // converted raw_samples

	size_t resample_buffer_size;
	int16_t *resample_buffer; // temp space used for resampling

	/**
	 * @brief The OpenAL sound sources.
	 */
	ALuint sources[MAX_CHANNELS];

	/**
	 * @brief Effect IDs.
	 */
	s_effects_t effects;

	/**
	 * @brief True when media has been reloaded, and the client should update its media references.
	 */
	_Bool update;
} s_env_t;

#ifdef __S_LOCAL_H__
extern SF_VIRTUAL_IO s_rwops_io;
extern SF_VIRTUAL_IO s_physfs_io;

#endif /* __S_LOCAL_H__ */
