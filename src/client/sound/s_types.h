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

#define AL_ALEXT_PROTOTYPES
#include <AL/al.h>
#include <AL/alc.h>
#include <AL/alext.h>

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
} SoundMediaType;

/**
 * @brief Samples, musics, etc. are all managed as media.
 */
typedef struct SoundMedia {

  /**
   * @brief The media name.
   */
  char name[MAX_QPATH];

  /**
   * @brief The media type.
   */
  SoundMediaType type;

  /**
   * @brief The media on which this media depends.
   */
  List *dependencies;

  /**
   * @brief The media retain callback, to avoid being freed.
   */
  bool (*Retain)(struct SoundMedia *self);

  /**
   * @brief The free callback, to release any system resources.
   */
  void (*Free)(struct SoundMedia *self);

  /**
   * @brief The media seed, to determine if this media is current.
   */
  int32_t seed;
} SoundMedia;

/**
 * @brief A sound sample.
 */
typedef struct {

  /**
   * @brief The media.
   */
  SoundMedia media;

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
} SoundSample;

#define S_PLAY_AMBIENT      0x1 // this is an ambient sound and may be culled by the user
#define S_PLAY_LOOP         0x2 // loop the sound continuously
#define S_PLAY_FRAME        0x4 // cull the sound if it is not added at each frame
#define S_PLAY_RELATIVE      0x8 // play relative to the listener origin
#define S_PLAY_UNDERWATER   0x10 // sound is of a different liquid state than the listener: extreme low pass
#define S_PLAY_OCCLUDED      0x20 // sound is occluded by an occluder: low pass
#define S_PLAY_UI           0x40 // sound is a user-interface effect and may be culled by the user

#define TONES_PER_OCTAVE  48

struct SoundPlaySample;
struct SoundStage;

/**
 * @brief Think function for sound samples to update effects, pitch, etc.. per frame.
 */
typedef void (*PlaySampleThink)(const struct SoundStage *stage, struct SoundPlaySample *play);

/**
 * @brief The sample instance type, used to dispatch playback of a sample.
 */
typedef struct SoundPlaySample {

  /**
   * @brief The sample to play.
   */
  const SoundSample *sample;

  /**
   * @brief The sample origin.
   */
  Vec3 origin;

  /**
   * @brief The sample velocity, for Doppler effects.
   */
  Vec3 velocity;

  /**
   * @brief The sample flags.
   */
  int32_t flags;

  /**
   * @brief The sample pitch shift, positive or negative.
   */
  int32_t pitch;

  /**
   * @brief Gain scalar in [0, 1]; 0 means use the default (1.0).
   */
  float gain;

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
} SoundPlaySample;

/**
 * @brief Samples are collected into channels that are spatialized and played back.
 */
typedef struct {

  /**
   * @brief The play sample.
   */
  SoundPlaySample play;

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
   * @brief The combined lowpass filter applied to this channel (occlusion + underwater).
   */
  ALuint filter;

  /**
   * @brief Occlusion (through-wall) mix fraction, smoothly interpolated [0, 1].
   */
  float occlusion;

  /**
   * @brief Underwater mix fraction, smoothly interpolated [0, 1].
   */
  float underwater;
} SoundChannel;

#define MAX_CHANNELS 128

/**
 * @brief A music track.
 */
typedef struct {

  /**
   * @brief The media.
   */
  SoundMedia media;

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
  File *file;

  /**
   * @brief True when the end of the file has been reached.
   */
  bool eof;
} SoundMusic;

/**
 * @brief Filters and effects used by the sound system if `s_effects` is enabled & supported.
 */
typedef struct {

  /**
   * @brief EAX or standard reverb effect, driven by per-listener voxel enclosure.
   */
  ALuint reverb;

  /**
   * @brief Auxiliary effect slot the reverb effect is attached to.
   */
  ALuint reverb_slot;

  /**
   * @brief True if the filters above are currently loaded.
   */
  bool loaded;
} SoundEffects;

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
   * @brief True once SDL audio is up, whether or not OpenAL initialization went on to succeed.
   */
  bool initialized;

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
  SoundChannel channels[MAX_CHANNELS];

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
  SoundEffects effects;

  /**
   * @brief The current listener reverb level (0=open, 1=fully enclosed).
   */
  float reverb;

  /**
   * @brief Stage ticks at last mix, used to compute per-frame dt for filter interpolation.
   */
  uint32_t prev_ticks;
} SoundContext;

/**
 * @brief Sound statistics, written by the sound module for each rendered stage.
 */
typedef struct {

  /**
   * @brief The count of channels playing after the stage was mixed.
   */
  int32_t num_channels;

  /**
   * @brief The reverb intensity at the listener origin.
   */
  float reverb;
} SoundStageStats;

/**
 * @brief The sound stage type.
 */
typedef struct SoundStage {

  /**
   * @brief Unclamped simulation time, in milliseconds.
   */
  uint32_t ticks;

  /**
   * @brief The listener origin.
   */
  Vec3 origin;

  /**
   * @brief The listener angles.
   */
  Vec3 angles;

  /**
   * @brief The forward vector, derived from angles.
   */
  Vec3 forward;

  /**
   * @brief The right vector, derived from angles.
   */
  Vec3 right;

  /**
   * @brief The up vector, derived from angles.
   */
  Vec3 up;

  /**
   * @brief The listener velocity.
   */
  Vec3 velocity;

  /**
   * @brief The contents mask at the listener origin.
   */
  int32_t contents;

  /**
   * @brief The samples to render for the current frame.
   */
  SoundPlaySample samples[MAX_SOUNDS];

  /**
   * @brief The count of samples.
   */
  int32_t num_samples;

  /**
   * @brief Statistics for the most recent render of this stage.
   */
  SoundStageStats stats;
} SoundStage;

#if defined(__S_LOCAL_H__)

extern SF_VIRTUAL_IO s_rwops_io;
extern SF_VIRTUAL_IO s_physfs_io;

#endif
