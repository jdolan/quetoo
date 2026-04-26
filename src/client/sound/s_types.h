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

#include <SDL3/SDL_audio.h>

#include <sndfile.h>

#include "common/common.h"

/**
 * @brief Media types.
 */
typedef enum {
  S_MEDIA_GENERIC,
  S_MEDIA_SAMPLE,
  S_MEDIA_MUSIC,
  S_MEDIA_TOTAL
} s_media_type_t;

/**
 * @brief Samples, musics, etc. are all managed as media.
 */
typedef struct s_media_s {

  /**
   * @brief The media name.
   */
  char name[MAX_QPATH];

  /**
   * @brief The media type.
   */
  s_media_type_t type;

  /**
   * @brief The media on which this media depends.
   */
  GList *dependencies;

  /**
   * @brief The media retain callback, to avoid being freed.
   */
  bool (*Retain)(struct s_media_s *self);

  /**
   * @brief The free callback, to release any system resources.
   */
  void (*Free)(struct s_media_s *self);

  /**
   * @brief The media seed, to determine if this media is current.
   */
  int32_t seed;
} s_media_t;

/**
 * @brief A sound sample.
 */
typedef struct {

  /**
   * @brief The media.
   */
  s_media_t media;

  /**
   * @brief The OpenAL buffer object.
   */
  ALuint buffer;

  /**
   * @brief The number of samples.
   */
  size_t num_samples;

  /**
   * @brief True for stereo sounds, which will not be spatialized.
   */
  bool stereo;
} s_sample_t;

#define S_PLAY_AMBIENT      0x1 // this is an ambient sound and may be culled by the user
#define S_PLAY_LOOP         0x2 // loop the sound continuously
#define S_PLAY_FRAME        0x4 // cull the sound if it is not added at each frame
#define S_PLAY_RELATIVE      0x8 // play relative to the listener origin
#define S_PLAY_UNDERWATER   0x10 // sound is of a different liquid state than the listener: extreme low pass
#define S_PLAY_OCCLUDED      0x20 // sound is occluded by an occluder: low pass
#define S_PLAY_UI           0x40 // sound is a user-interface effect and may be culled by the user

#define TONES_PER_OCTAVE  48

struct s_play_sample_s;
struct s_stage_s;

/**
 * @brief Think function for sound samples to update effects, pitch, etc.. per frame.
 */
typedef void (*PlaySampleThink)(const struct s_stage_s *stage, struct s_play_sample_s *play);

/**
 * @brief The sample instance type, used to dispatch playback of a sample.
 */
typedef struct s_play_sample_s {

  /**
   * @brief The sample to play.
   */
  const s_sample_t *sample;

  /**
   * @brief The sample origin.
   */
  vec3_t origin;

  /**
   * @brief The sample velocity, for Doppler effects.
   */
  vec3_t velocity;

  /**
   * @brief The sample attenuation.
   */
  sound_atten_t atten;

  /**
   * @brief The sample flags.
   */
  int32_t flags;

  /**
   * @brief The sample pitch shift, positive or negative.
   */
  int32_t pitch;

  /**
   * @brief The entity associated with this sample, so that occlusion traces may skip it.
   */
  const void *entity;

  /**
   * @brief User data associated with this sample.
   */
  void *data;

  /**
   * @brief An optional think function run once per frame.
   */
  PlaySampleThink Think;
} s_play_sample_t;

/**
 * @brief Samples are collected into channels that are spatialized and played back.
 */
typedef struct {

  /**
   * @brief The play sample.
   */
  s_play_sample_t play;

  /**
   * @brief The time when this channel was last started.
   */
  uint32_t start_time;

  /**
   * @brief The stage frame number this channel was last added in.
   */
  uint32_t timestamp;

  /**
   * @brief The channel gain.
   */
  float gain;

  /**
   * @brief The channel pitch.
   */
  float pitch;

  /**
   * @brief The channel filter.
   */
  ALuint filter;
} s_channel_t;

#define MAX_CHANNELS 128

/**
 * @brief A music track.
 */
typedef struct {

  /**
   * @brief The media.
   */
  s_media_t media;

  /**
   * @brief The libsndfile stream info.
   */
  SF_INFO info;

  /**
   * @brief The libsndfile handle.
   */
  SNDFILE *snd;

  /**
   * @brief The backing file.
   */
  file_t *file;

  /**
   * @brief True when the end of the file has been reached.
   */
  bool eof;
} s_music_t;

/**
 * @brief Filters used by the sound system if s_effects is enabled & supported.
 */
typedef struct {

  /**
   * @brief Low-pass filter applied to occluded sound sources.
   */
  ALuint occluded;

  /**
   * @brief Extreme low-pass filter applied to sounds of different liquid state than the listener.
   */
  ALuint underwater;

  /**
   * @brief True if the filters above are currently loaded.
   */
  bool loaded;
} s_effects_t;

/**
 * @brief The sound environment.
 */
typedef struct {

  /**
   * @brief The OpenAL playback device.
   */
  ALCdevice *device;

  /**
   * @brief The OpenAL playback context.
   */
  ALCcontext *context;

  /**
   * @brief The renderer string reported by the AL driver.
   */
  const char *renderer;

  /**
   * @brief The vendor string reported by the AL driver.
   */
  const char *vendor;

  /**
   * @brief The version string reported by the AL driver.
   */
  const char *version;

  /**
   * @brief The size in bytes of the raw sample buffer.
   */
  size_t raw_sample_buffer_size;

  /**
   * @brief Scratch buffer for raw float sample data before conversion.
   */
  float *raw_sample_buffer;

  /**
   * @brief The size in bytes of the converted sample buffer.
   */
  size_t converted_sample_buffer_size;

  /**
   * @brief Converted raw sample buffer (float → int16).
   */
  int16_t *converted_sample_buffer;

  /**
   * @brief The size in bytes of the resampling scratch buffer.
   */
  size_t resample_buffer_size;

  /**
   * @brief Scratch buffer for resampled audio data.
   */
  int16_t *resample_buffer;

  /**
   * @brief The mixed channels.
   */
  s_channel_t channels[MAX_CHANNELS];

  /**
   * @brief The number of channels currently playing.
   */
  int32_t num_active_channels;

  /**
   * @brief The OpenAL sound sources.
   */
  ALuint sources[MAX_CHANNELS];

  /**
   * @brief Effect IDs.
   */
  s_effects_t effects;
} s_context_t;

/**
 * @brief The sound stage type.
 */
typedef struct s_stage_s {

  /**
   * @brief Unclamped simulation time, in milliseconds.
   */
  uint32_t ticks;

  /**
   * @brief The listener origin.
   */
  vec3_t origin;

  /**
   * @brief The listener angles.
   */
  vec3_t angles;

  /**
   * @brief The forward vector, derived from angles.
   */
  vec3_t forward;

  /**
   * @brief The right vector, derived from angles.
   */
  vec3_t right;

  /**
   * @brief The up vector, derived from angles.
   */
  vec3_t up;

  /**
   * @brief The listener velocity.
   */
  vec3_t velocity;

  /**
   * @brief The contents mask at the listener origin.
   */
  int32_t contents;

  /**
   * @brief The samples to render for the current frame.
   */
  s_play_sample_t samples[MAX_SOUNDS];

  /**
   * @brief The count of samples.
   */
  int32_t num_samples;
} s_stage_t;

#if defined(__S_LOCAL_H__)

extern SF_VIRTUAL_IO s_rwops_io;
extern SF_VIRTUAL_IO s_physfs_io;

#endif /* __S_LOCAL_H__ */
