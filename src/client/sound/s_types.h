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
  char name[MAX_QPATH]; ///< The media name.
  s_media_type_t type; ///< The media type.
  GList *dependencies; ///< The media on which this media depends.
  bool (*Retain)(struct s_media_s *self); ///< The media retain callback, to avoid being freed.
  void (*Free)(struct s_media_s *self); ///< The free callback, to release any system resources.
  int32_t seed; ///< The media seed, to determine if this media is current.
} s_media_t;

/**
 * @brief A sound sample.
 */
typedef struct {
  s_media_t media; ///< The media.
  ALuint buffer; ///< The OpenAL buffer object.
  size_t num_samples; ///< The number of samples.
  bool stereo; ///< True for stereo sounds, which will not be spatialized.
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
  const s_sample_t *sample; ///< The sample to play.
  vec3_t origin; ///< The sample origin.
  vec3_t velocity; ///< The sample velocity, for Doppler effects.
  sound_atten_t atten; ///< The sample attenuation.
  int32_t flags; ///< The sample flags.
  int32_t pitch; ///< The sample pitch shift, positive or negative.
  const void *entity; ///< The entity associated with this sample, so that occlusion traces may skip it.
  void *data; ///< User data associated with this sample.
  PlaySampleThink Think; ///< An optional think function run once per frame.
} s_play_sample_t;

/**
 * @brief Samples are collected into channels that are spatialized and played back.
 */
typedef struct {
  s_play_sample_t play; ///< The play sample.
  uint32_t start_time; ///< The time when this channel was last started.
  uint32_t timestamp; ///< The stage frame number this channel was last added in.
  float gain; ///< The channel gain.
  float pitch; ///< The channel pitch.
  ALuint filter; ///< The channel filter.
} s_channel_t;

#define MAX_CHANNELS 128

/**
 * @brief A music track.
 */
typedef struct {
  s_media_t media; ///< The media.
  SF_INFO info; ///< The libsndfile stream info.
  SNDFILE *snd; ///< The libsndfile handle.
  file_t *file; ///< The backing file.
  bool eof; ///< True when the end of the file has been reached.
} s_music_t;

/**
 * @brief Filters used by the sound system if s_effects is enabled & supported.
 */
typedef struct {
  ALuint occluded; ///< Low-pass filter applied to occluded sound sources.
  ALuint underwater; ///< Extreme low-pass filter applied to sounds of different liquid state than the listener.
  bool loaded; ///< True if the filters above are currently loaded.
} s_effects_t;

/**
 * @brief The sound environment.
 */
typedef struct {
  ALCdevice *device; ///< The OpenAL playback device.
  ALCcontext *context; ///< The OpenAL playback context.

  const char *renderer; ///< The renderer string reported by the AL driver.
  const char *vendor; ///< The vendor string reported by the AL driver.
  const char *version; ///< The version string reported by the AL driver.

  size_t raw_sample_buffer_size; ///< The size in bytes of the raw sample buffer.
  float *raw_sample_buffer; ///< Scratch buffer for raw float sample data before conversion.

  size_t converted_sample_buffer_size; ///< The size in bytes of the converted sample buffer.
  int16_t *converted_sample_buffer; ///< Converted raw sample buffer (float → int16).

  size_t resample_buffer_size; ///< The size in bytes of the resampling scratch buffer.
  int16_t *resample_buffer; ///< Scratch buffer for resampled audio data.

  s_channel_t channels[MAX_CHANNELS]; ///< The mixed channels.
  int32_t num_active_channels; ///< The number of channels currently playing.

  ALuint sources[MAX_CHANNELS]; ///< The OpenAL sound sources.
  s_effects_t effects; ///< Effect IDs.
} s_context_t;

/**
 * @brief The sound stage type.
 */
typedef struct s_stage_s {
  uint32_t ticks; ///< Unclamped simulation time, in milliseconds.
  vec3_t origin; ///< The listener origin.
  vec3_t angles; ///< The listener angles.
  vec3_t forward; ///< The forward vector, derived from angles.
  vec3_t right; ///< The right vector, derived from angles.
  vec3_t up; ///< The up vector, derived from angles.
  vec3_t velocity; ///< The listener velocity.
  int32_t contents; ///< The contents mask at the listener origin.
  s_play_sample_t samples[MAX_SOUNDS]; ///< The samples to render for the current frame.
  int32_t num_samples; ///< The count of samples.
} s_stage_t;

#if defined(__S_LOCAL_H__)

extern SF_VIRTUAL_IO s_rwops_io;
extern SF_VIRTUAL_IO s_physfs_io;

#endif /* __S_LOCAL_H__ */
