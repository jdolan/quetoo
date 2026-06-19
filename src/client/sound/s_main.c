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

#include <SDL3/SDL.h>

#include "s_local.h"

s_context_t s_context;

cvar_t *s_get_error;
cvar_t *s_draw_stats;

cvar_t *s_ambient_volume;
cvar_t *s_doppler;
cvar_t *s_effects;
cvar_t *s_effects_volume;
cvar_t *s_hrtf;
cvar_t *s_rate;
cvar_t *s_volume;

/**
 * @brief Check and report OpenAL errors.
 */
void S_GetError_(const char *function, const char *msg) {

  if (!s_get_error->integer) {
    return;
  }

  const ALenum v = alGetError();

  if (v == AL_NO_ERROR) {
    return;
  }

  Com_Warn("%s threw %s: %s\n", function, alGetString(v), msg);

  if (s_get_error->integer == 2) {
    SDL_TriggerBreakpoint();
  }
}

/**
 * @brief Returns the size of the `SDL_IOStream` for use as a libsndfile virtual file length callback.
 */
static sf_count_t S_RWops_get_filelen(void *user_data) {
  SDL_IOStream *rwops = (SDL_IOStream *) user_data;
  return SDL_GetIOSize(rwops);
}

/**
 * @brief Seeks the `SDL_IOStream` for use as a libsndfile virtual seek callback.
 */
static sf_count_t S_RWops_seek(sf_count_t offset, int whence, void *user_data) {
  SDL_IOStream *rwops = (SDL_IOStream *) user_data;
  return SDL_SeekIO(rwops, offset, whence);
}

/**
 * @brief Reads from the `SDL_IOStream` for use as a libsndfile virtual read callback.
 */
static sf_count_t S_RWops_read(void *ptr, sf_count_t count, void *user_data) {
  SDL_IOStream *rwops = (SDL_IOStream *) user_data;
  return SDL_ReadIO(rwops, ptr, count);
}

/**
 * @brief Writes to the `SDL_IOStream` for use as a libsndfile virtual write callback.
 */
static sf_count_t S_RWops_write(const void *ptr, sf_count_t count, void *user_data) {
  SDL_IOStream *rwops = (SDL_IOStream *) user_data;
  return SDL_WriteIO(rwops, ptr, count);
}

/**
 * @brief Returns the current position of the `SDL_IOStream` for use as a libsndfile virtual tell callback.
 */
static sf_count_t S_RWops_tell(void *user_data) {
  SDL_IOStream *rwops = (SDL_IOStream *) user_data;
  return SDL_TellIO(rwops);
}

/**
 * @brief An interface to `SDL_IOStream` for libsndfile
 */
SF_VIRTUAL_IO s_rwops_io = {
  .get_filelen = S_RWops_get_filelen,
  .seek = S_RWops_seek,
  .read = S_RWops_read,
  .write = S_RWops_write,
  .tell = S_RWops_tell
};

/**
 * @brief Returns the size of the PhysFS file for use as a libsndfile virtual file length callback.
 */
static sf_count_t S_PhysFS_get_filelen(void *user_data) {
  file_t *file = (file_t *) user_data;
  return Fs_FileLength(file);
}

/**
 * @brief Seeks the PhysFS file for use as a libsndfile virtual seek callback.
 */
static sf_count_t S_PhysFS_seek(sf_count_t offset, int whence, void *user_data) {
  file_t *file = (file_t *) user_data;

  switch (whence) {
  case SEEK_SET:
    Fs_Seek(file, offset);
    break;
  case SEEK_CUR:
    Fs_Seek(file, Fs_Tell(file) + offset);
    break;
  case SEEK_END:
    Fs_Seek(file, Fs_FileLength(file) - offset);
    break;
  }

  return Fs_Tell(file);
}

/**
 * @brief Reads from the PhysFS file for use as a libsndfile virtual read callback.
 */
static sf_count_t S_PhysFS_read(void *ptr, sf_count_t count, void *user_data) {
  file_t *file = (file_t *) user_data;
  return Fs_Read(file, ptr, 1, count);
}

/**
 * @brief Writes to the PhysFS file for use as a libsndfile virtual write callback.
 */
static sf_count_t S_PhysFS_write(const void *ptr, sf_count_t count, void *user_data) {
  file_t *file = (file_t *) user_data;
  return Fs_Write(file, ptr, 1, count);
}

/**
 * @brief Returns the current position of the PhysFS file for use as a libsndfile virtual tell callback.
 */
static sf_count_t S_PhysFS_tell(void *user_data) {
  file_t *file = (file_t *) user_data;
  return Fs_Tell(file);
}

/**
 * @brief An interface to PhysFS for libsndfile
 */
SF_VIRTUAL_IO s_physfs_io = {
  .get_filelen = S_PhysFS_get_filelen,
  .seek = S_PhysFS_seek,
  .read = S_PhysFS_read,
  .write = S_PhysFS_write,
  .tell = S_PhysFS_tell
};

/**
 * @brief Stop sounds that are playing, if any.
 */
void S_Stop(void) {

  // Preserve per-channel filter handles (AL objects outlive channel state)
  ALuint filters[MAX_CHANNELS];
  for (int32_t i = 0; i < MAX_CHANNELS; i++) {
    filters[i] = s_context.channels[i].filter;
  }

  memset(s_context.channels, 0, sizeof(s_context.channels));

  for (int32_t i = 0; i < MAX_CHANNELS; i++) {
    s_context.channels[i].filter = filters[i];
  }

  s_context.prev_ticks = 0;

  alSourceStopv(MAX_CHANNELS, s_context.sources);

  for (size_t i = 0; i < MAX_CHANNELS; i++) {
    alSourcei(s_context.sources[i], AL_BUFFER, 0);
  }

  S_GetError(NULL);
}

/**
 * @brief Initialize the per-frame attributes of a sound stage.
 */
void S_InitStage(s_stage_t *stage) {
  stage->ticks = (uint32_t) SDL_GetTicks();
  stage->num_samples = 0;
}

/**
 * @brief Renders the specified stage, adding channels from the defined play samples.
 */
void S_RenderStage(const s_stage_t *stage) {

  assert(stage);

  if (!s_context.context) {
    return;
  }

  const s_play_sample_t *s = stage->samples;
  for (int32_t i = 0; i < stage->num_samples; i++, s++) {

    if (s->flags & S_PLAY_FRAME) {
      s_channel_t *ch = s_context.channels;
      int32_t j;
      for (j = 0; j < MAX_CHANNELS; j++, ch++) {
        if (ch->play.sample && (ch->play.flags & S_PLAY_FRAME)) {
          if (ch->play.sample == s->sample) {
            if ((ch->play.entity && ch->play.entity == s->entity)
                || (ch->play.data && ch->play.data == s->data)) {
              ch->play = *s;
              ch->timestamp = stage->ticks;
              break;
            }
          }
        }
      }

      if (j < MAX_CHANNELS) {
        continue;
      }
    }

    const int32_t c = S_AllocChannel();
    if (c == -1) {
      continue;
    }

    s_context.channels[c].play = *s;
    s_context.channels[c].timestamp = stage->ticks;
  }

  S_RenderMusic(stage);

  S_MixChannels(stage);
}

/**
 * @brief Console command wrapper that calls `S_Stop` to silence all active channels.
 */
static void S_Stop_f(void) {
  S_Stop();
}

/**
 * @brief Initializes variables and commands for the sound subsystem.
 */
static void S_InitLocal(void) {

  s_get_error = Cvar_Add("s_get_error", "0", CVAR_DEVELOPER, "Log OpenAL errors to the console (developer tool");
  s_draw_stats = Cvar_Add("s_draw_stats", "0", CVAR_DEVELOPER, "Draw sound performance statistics (developer tool)");

  s_ambient_volume = Cvar_Add("s_ambient_volume", "1", CVAR_ARCHIVE, "Ambient sound volume.");
  s_doppler = Cvar_Add("s_doppler", "1", CVAR_ARCHIVE, "Doppler effect intensity (default 1).");
  s_effects = Cvar_Add("s_effects", "1", CVAR_ARCHIVE | CVAR_S_DEVICE, "Enables advanced sound effects.");
  s_effects_volume = Cvar_Add("s_effects_volume", "1", CVAR_ARCHIVE, "Effects sound volume.");
  s_hrtf = Cvar_Add("s_hrtf", "0", CVAR_ARCHIVE | CVAR_S_DEVICE, "Enables HRTF sound spatialization. Recommended for headphones.");
  s_rate = Cvar_Add("s_rate", "44100", CVAR_ARCHIVE | CVAR_S_DEVICE, "Sound sample rate in Hz.");
  s_volume = Cvar_Add("s_volume", "1", CVAR_ARCHIVE, "Master sound volume level.");

  Cvar_ClearAll(CVAR_S_MASK);

  Cmd_Add("s_list_media", S_ListMedia_f, CMD_SOUND, "List all currently loaded media");
  Cmd_Add("s_stop", S_Stop_f, CMD_SOUND, NULL);
}

/**
 * @brief Initializes the sound subsystem.
 */
void S_Init(void) {
  memset(&s_context, 0, sizeof(s_context));

  if (Cvar_GetValue("s_disable")) {
    Com_Warn("Sound disabled\n");
    return;
  }

  Com_Print("Sound initialization...\n");

  S_InitLocal();

  s_context.device = alcOpenDevice(NULL);

  if (!s_context.device) {
    Com_Warn("%s\n", alcGetString(NULL, alcGetError(NULL)));
    return;
  }

  if (s_hrtf->integer && alcIsExtensionPresent(s_context.device, "ALC_SOFT_HRTF")) {
    ALCint attrs[7] = { ALC_HRTF_SOFT, ALC_TRUE };
    int n = 2;
    if (alcIsExtensionPresent(s_context.device, "ALC_SOFT_output_mode")) {
      attrs[n++] = ALC_OUTPUT_MODE_SOFT;
      attrs[n++] = ALC_STEREO_HRTF_SOFT;
    }
    attrs[n] = 0;
    s_context.context = alcCreateContext(s_context.device, attrs);
  } else {
    s_context.context = alcCreateContext(s_context.device, NULL);
  }

  if (!s_context.context || !alcMakeContextCurrent(s_context.context)) {
    Com_Warn("%s\n", alcGetString(NULL, alcGetError(NULL)));
    return;
  }

  const int efx_supported = alcIsExtensionPresent(s_context.device, "ALC_EXT_EFX");

  s_context.renderer = (const char *) alGetString(AL_RENDERER);
  s_context.vendor = (const char *) alGetString(AL_VENDOR);
  s_context.version = (const char *) alGetString(AL_VERSION);

  Com_Print("  Renderer:   ^2%s^7\n", s_context.renderer);
  Com_Print("  Vendor:     ^2%s^7\n", s_context.vendor);
  Com_Print("  Version:    ^2%s^7\n", s_context.version);

  if (s_hrtf->integer) {
    ALCint hrtf_status;
    alcGetIntegerv(s_context.device, ALC_HRTF_STATUS_SOFT, 1, &hrtf_status);
    if (hrtf_status == ALC_HRTF_ENABLED_SOFT || hrtf_status == ALC_HRTF_REQUIRED_SOFT) {
      const ALCchar *name = alcGetString(s_context.device, ALC_HRTF_SPECIFIER_SOFT);
      Com_Print("  HRTF:       ^2%s^7\n", name ? name : "enabled");
    } else {
      Com_Warn("HRTF requested but not enabled (status %d)\n", hrtf_status);
    }
  }

  {
    char ext_buf[4096];
    SDL_strlcpy(ext_buf, alGetString(AL_EXTENSIONS) ? alGetString(AL_EXTENSIONS) : "", sizeof(ext_buf));
    char *save = NULL;
    bool first = true;
    for (char *tok = SDL_strtok_r(ext_buf, " ", &save); tok; tok = SDL_strtok_r(NULL, " ", &save)) {
      if (first) {
        Com_Verbose("  Extensions: ^2%s^7\n", tok);
        first = false;
      } else {
        Com_Verbose("              ^2%s^7\n", tok);
      }
    }
  }

  {
    const char *alc_ext = alcGetString(s_context.device, ALC_EXTENSIONS);
    char ext_buf[4096];
    SDL_strlcpy(ext_buf, alc_ext ? alc_ext : "", sizeof(ext_buf));
    char *save = NULL;
    for (char *tok = SDL_strtok_r(ext_buf, " ", &save); tok; tok = SDL_strtok_r(NULL, " ", &save)) {
      Com_Verbose("              ^2%s^7\n", tok);
    }
  }

  if (s_effects->integer) {
    if (!efx_supported) {
      Com_Warn("s_effects is enabled but OpenAL driver does not support them.\n");
      Cvar_ForceSetInteger(s_effects->name, 0);
      s_effects->modified = false;
      s_context.effects.loaded = false;
    } else {

      // Per-channel combined lowpass filter (occlusion + underwater blended into one)
      ALuint filters[MAX_CHANNELS];
      alGenFilters(MAX_CHANNELS, filters);
      for (int32_t i = 0; i < MAX_CHANNELS; i++) {
        alFilteri(filters[i], AL_FILTER_TYPE, AL_FILTER_LOWPASS);
        s_context.channels[i].filter = filters[i];
      }

      alGenEffects(1, &s_context.effects.reverb);
      if (alGetError() == AL_NO_ERROR) {
        alEffecti(s_context.effects.reverb, AL_EFFECT_TYPE, AL_EFFECT_EAXREVERB);
        if (alGetError() != AL_NO_ERROR) {
          alEffecti(s_context.effects.reverb, AL_EFFECT_TYPE, AL_EFFECT_REVERB);
          S_GetError("Failed to set reverb effect type");
        }
      }

      alGenAuxiliaryEffectSlots(1, &s_context.effects.reverb_slot);
      alAuxiliaryEffectSloti(s_context.effects.reverb_slot, AL_EFFECTSLOT_EFFECT, (ALint) s_context.effects.reverb);

      if (alGetError() == AL_NO_ERROR) {
        s_context.effects.loaded = true;
      } else {
        Com_Warn("s_effects: failed to create filters, disabling.\n");
        s_context.effects.loaded = false;
      }
    }
  } else {
    s_context.effects.loaded = false;
  }

  alDistanceModel(AL_LINEAR_DISTANCE_CLAMPED);
  alGenSources(MAX_CHANNELS, s_context.sources);
  alSpeedOfSound(343.3f * 40.f); // 1 Quake unit ≈ 1 inch; 1 meter ≈ 40 units

  S_GetError(NULL);

  Com_Print("Sound initialized (OpenAL, resample @ %dhz)\n", s_rate->integer);

  S_InitMedia();

  S_InitMusic();

  s_context.resample_buffer = Mem_TagMalloc(sizeof(int16_t) * 2048, MEM_TAG_SOUND);
}

/**
 * @brief Shuts down the sound subsystem, releasing all OpenAL resources and the context.
 */
void S_Shutdown(void) {

  if (!s_context.context) {
    return;
  }

  S_Stop();

  alDeleteSources(MAX_CHANNELS, s_context.sources);

  if (s_context.effects.loaded) {
    ALuint filters[MAX_CHANNELS];
    for (int32_t i = 0; i < MAX_CHANNELS; i++) {
      filters[i] = s_context.channels[i].filter;
    }
    alDeleteFilters(MAX_CHANNELS, filters);
    alDeleteAuxiliaryEffectSlots(1, &s_context.effects.reverb_slot);
    alDeleteEffects(1, &s_context.effects.reverb);
    s_context.effects.loaded = false;
  }

  S_GetError(NULL);

  S_ShutdownMusic();

  S_ShutdownMedia();

  alcMakeContextCurrent(NULL);
  alcDestroyContext(s_context.context);
  alcCloseDevice(s_context.device);

  SDL_QuitSubSystem(SDL_INIT_AUDIO);

  Cmd_RemoveAll(CMD_SOUND);

  Mem_Free(s_context.raw_sample_buffer);
  Mem_Free(s_context.converted_sample_buffer);
  Mem_Free(s_context.resample_buffer);

  Mem_FreeTag(MEM_TAG_SOUND);
}
