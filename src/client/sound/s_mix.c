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

#include "s_local.h"
#include "collision/cm_voxel.h"

/**
 * @brief Returns effective gain for non-ambient samples.
 */
static float S_EffectsGain(void) {
  return Clampf01(s_volume->value) * Clampf01(s_effects_volume->value);
}

/**
 * @brief Returns effective gain for ambient samples.
 */
static float S_AmbientGain(void) {
  return Clampf01(s_volume->value) * Clampf01(s_ambient_volume->value);
}

/**
 * @brief Returns the index of a free channel, or -1 if all channels are in use.
 */
int32_t S_AllocChannel(void) {

  for (int32_t i = 0; i < MAX_CHANNELS; i++) {
    if (!s_context.channels[i].play.sample) {
      return i;
    }
  }

  Com_Debug(DEBUG_SOUND, "Failed\n");
  return -1;
}

/**
 * @brief Stops playback and clears state for the given channel index.
 */
void S_FreeChannel(int32_t c) {

  alSourceStop(s_context.sources[c]);
  alSourcei(s_context.sources[c], AL_BUFFER, 0);

  const ALuint filter = s_context.channels[c].filter;
  memset(&s_context.channels[c], 0, sizeof(s_context.channels[c]));
  s_context.channels[c].filter = filter;
}

/**
 * @brief Resolve the frame-fade gain, pitch, and effects for the specified channel.
 * @details Distance attenuation and 3D panning are handled by OpenAL.
 * @param stage The sound stage.
 * @param ch The channel.
 * @return True if the channel is audible, false if it should be freed.
 */
static bool S_SpatializeChannel(const s_stage_t *stage, s_channel_t *ch) {

  ch->gain = 1.f;

  if (!(ch->play.flags & S_PLAY_UI)) {

    // fade out frame sounds that are no longer being submitted
    if (ch->start_time && (ch->play.flags & S_PLAY_FRAME)) {
      if (ch->timestamp != stage->ticks) {
        const uint32_t delta = stage->ticks - ch->timestamp;

        if (delta > 250) {
          return false;
        }

        ch->gain = 1.f - (delta / 250.f);
      }
    }
  }

  if (ch->play.gain) {
    ch->gain *= ch->play.gain;
  }

  ch->pitch = 1.f;

  if (stage->contents & CONTENTS_MASK_LIQUID) {
    ch->pitch = .5f;
  }

  if (ch->play.pitch) {
    const float octaves = (float) pow(2.0, 0.69314718 * ((float) ch->play.pitch / TONES_PER_OCTAVE));
    ch->pitch *= octaves;
  }

  if (s_context.effects.loaded) {
    const uint32_t delta = s_context.prev_ticks ? stage->ticks - s_context.prev_ticks : 0;

    // Underwater: 300 ms transition, heavy lowpass (muffled and dark)
    {
      const float target = (ch->play.flags & S_PLAY_UNDERWATER) ? 1.f : 0.f;
      const float step = delta / 300.f;
      ch->underwater = Clampf(ch->underwater + (target > ch->underwater ? step : -step), 0.f, 1.f);
    }

    // Occlusion: 300 ms transition, extreme HF cut (through-wall muffle)
    {
      const float target = (ch->play.flags & S_PLAY_OCCLUDED) ? 1.f : 0.f;
      const float step = delta / 300.f;
      ch->occlusion = Clampf(ch->occlusion + (target > ch->occlusion ? step : -step), 0.f, 1.f);
    }

    // Combine both into a single filter by multiplying gains; only one AL_DIRECT_FILTER slot exists per source.
    {
      const float gain   = Mixf(1.f, 0.66f,  ch->underwater) * Mixf(1.f, 0.33f, ch->occlusion);
      const float gainhf = Mixf(1.f, 0.33f,  ch->underwater) * Mixf(1.f, 0.88f, ch->occlusion);
      alFilterf(ch->filter, AL_LOWPASS_GAIN,   gain);
      alFilterf(ch->filter, AL_LOWPASS_GAINHF, gainhf);
    }
  }

  return true;
}

/**
 * @brief Updates all active channels for the current frame.
 */
void S_MixChannels(const s_stage_t *stage) {

  if (s_doppler->modified) {
    alDopplerFactor(s_doppler->value);
    s_doppler->modified = false;
  }

  alListenerfv(AL_POSITION, stage->origin.xyz);

  alListenerfv(AL_ORIENTATION, (float []) {
    stage->forward.x, stage->forward.y, stage->forward.z,
    stage->up.x, stage->up.y, stage->up.z
  });

  if (s_doppler->value) {
    alListenerfv(AL_VELOCITY, stage->velocity.xyz);
  } else {
    alListenerfv(AL_VELOCITY, Vec3_Zero().xyz);
  }

  if (s_context.effects.loaded) {
    const cm_voxel_t *voxel = Cm_VoxelForPoint(stage->origin);
    const float r = voxel ? voxel->occlusion : 0.f;
    if (r != s_context.reverb) {
      s_context.reverb = r;
      ALenum type;
      alGetEffecti(s_context.effects.reverb, AL_EFFECT_TYPE, &type);
      if (type == AL_EFFECT_EAXREVERB) {
        alEffectf(s_context.effects.reverb, AL_EAXREVERB_GAIN, 0.32f * r);
        alEffectf(s_context.effects.reverb, AL_EAXREVERB_DECAY_TIME, 0.1f + 2.4f * r);
        alEffectf(s_context.effects.reverb, AL_EAXREVERB_ROOM_ROLLOFF_FACTOR, r);
      } else {
        alEffectf(s_context.effects.reverb, AL_REVERB_GAIN, 0.32f * r);
        alEffectf(s_context.effects.reverb, AL_REVERB_DECAY_TIME, 0.1f + 2.4f * r);
        alEffectf(s_context.effects.reverb, AL_REVERB_ROOM_ROLLOFF_FACTOR, r);
      }
      alAuxiliaryEffectSloti(s_context.effects.reverb_slot, AL_EFFECTSLOT_EFFECT, (ALint) s_context.effects.reverb);
    }
  }

  s_context.num_active_channels = 0;

  s_channel_t *ch = s_context.channels;
  for (int32_t i = 0; i < MAX_CHANNELS; i++, ch++) {

    if (ch->play.sample == NULL) {
      continue;
    }

    assert(ch->play.sample->buffer);

    const ALuint src = s_context.sources[i];
    assert(src);

    if (ch->play.Think) {
      ch->play.Think(stage, &ch->play);
    }

    if (!S_SpatializeChannel(stage, ch)) {
      S_FreeChannel(i);
      continue;
    }

    alSourcefv(src, AL_POSITION, ch->play.origin.xyz);

    if (s_doppler->value) {
      alSourcefv(src, AL_VELOCITY, ch->play.velocity.xyz);
    } else {
      alSourcefv(src, AL_VELOCITY, Vec3_Zero().xyz);
    }

    float volume;
    if (ch->play.flags & S_PLAY_AMBIENT) {
      volume = S_AmbientGain();
    } else {
      volume = S_EffectsGain();
    }

    alSourcef(src, AL_GAIN, ch->gain * volume);
    alSourcef(src, AL_PITCH, ch->pitch);

    if (s_context.effects.loaded) {
      alSourcei(src, AL_DIRECT_FILTER, (ALint) ch->filter);
      alSourcef(src, AL_AIR_ABSORPTION_FACTOR, 0.025f); // 0.05 dB/m × (1 m / 40 units)
      const ALuint send = (ch->play.flags & S_PLAY_UI) ? AL_EFFECTSLOT_NULL : (ALuint) s_context.effects.reverb_slot;
      alSource3i(src, AL_AUXILIARY_SEND_FILTER, (ALint) send, 0, AL_FILTER_NULL);
    }

    if (ch->start_time == 0) {
      ch->start_time = stage->ticks;

      alSourcei(src, AL_BUFFER, ch->play.sample->buffer);

      if (ch->play.flags & (S_PLAY_UI | S_PLAY_RELATIVE)) {
        alSourcei(src, AL_SOURCE_RELATIVE, 1);
      } else {
        alSourcei(src, AL_SOURCE_RELATIVE, 0);
      }

      alSourcef(src, AL_ROLLOFF_FACTOR, 1.f);
      alSourcef(src, AL_REFERENCE_DISTANCE, 256.f);
      alSourcef(src, AL_MAX_DISTANCE, 2048.f);

      if (ch->play.flags & S_PLAY_LOOP) {
        alSourcei(src, AL_LOOPING, 1);
      } else {
        alSourcei(src, AL_LOOPING, 0);
      }

      if (ch->play.flags & S_PLAY_AMBIENT) {
        alSourcei(src, AL_SAMPLE_OFFSET, Randomf() * (int32_t) ch->play.sample->num_samples);
      }

      alSourcePlay(src);

    } else {
      ALenum state;
      alGetSourcei(src, AL_SOURCE_STATE, &state);

      if (state != AL_PLAYING) {
        S_FreeChannel(i);
        continue;
      }
    }

    S_GetError(ch->play.sample->media.name);

    s_context.num_active_channels++;
  }

  s_context.prev_ticks = stage->ticks;
}

/**
 * @brief Plays a sample immediately without spatialization or stage processing.
 * Use for UI sounds that must work regardless of client connection state.
 */
void S_PlaySample(s_sample_t *sample) {

  if (!s_context.context) {
    return;
  }

  if (!S_EffectsGain()) {
    return;
  }

  if (!sample || !sample->buffer) {
    return;
  }

  const int32_t c = S_AllocChannel();
  if (c == -1) {
    return;
  }

  s_context.channels[c].play = (s_play_sample_t) {
    .sample = sample,
    .flags = S_PLAY_UI,
  };
  s_context.channels[c].gain = 1.f;
  s_context.channels[c].pitch = 1.f;
  s_context.channels[c].start_time = (uint32_t) SDL_GetTicks();

  const ALuint src = s_context.sources[c];
  alSourcef(src, AL_GAIN, S_EffectsGain());
  alSourcef(src, AL_PITCH, 1.f);
  alSourcei(src, AL_SOURCE_RELATIVE, 1);
  alSourcei(src, AL_LOOPING, 0);
  alSourcei(src, AL_BUFFER, sample->buffer);
  if (s_context.effects.loaded) {
    alSourcei(src, AL_DIRECT_FILTER, AL_FILTER_NULL);
  }
  alSourcePlay(src);
}

/**
 * @brief Adds a sample to the sound stage for mixing in the current frame.
 */
void S_AddSample(s_stage_t *stage, const s_play_sample_t *play) {

  assert(stage);

  if (!s_context.context) {
    return;
  }

  if (!play) {
    return;
  }

  if (!play->sample) {
    return;
  }

  if (!play->sample->buffer) {
    return;
  }

  if (play->flags & S_PLAY_AMBIENT) {
    if (!S_AmbientGain()) {
      return;
    }
  } else {
    if (!S_EffectsGain()) {
      return;
    }
  }

  if (stage->num_samples == MAX_SOUNDS) {
    Com_Debug(DEBUG_SOUND, "MAX_SOUNDS");
    return;
  }

  stage->samples[stage->num_samples] = *play;
  stage->num_samples++;
}
