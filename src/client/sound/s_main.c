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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#include <SDL3/SDL.h>

#include "s_local.h"

SoundContext soundContext;

Cvar *s_getError;

Cvar *s_ambientVolume;
Cvar *s_doppler;
Cvar *s_effects;
Cvar *s_effectsVolume;
Cvar *s_hrtf;
Cvar *s_rate;
Cvar *s_volume;

/**
 * @brief Check and report OpenAL errors.
 */
void S_GetError_(const char *function, const char *msg) {

  if (!s_getError->integer) {
    return;
  }

  const ALenum v = alGetError();

  if (v == AL_NO_ERROR) {
    return;
  }

  Com_Warn("%s threw %s: %s\n", function, alGetString(v), msg);

  if (s_getError->integer == 2) {
    SDL_TriggerBreakpoint();
  }
}

/**
 * @brief Returns the size of the `SDL_IOStream` for use as a libsndfile virtual file length callback.
 */
static sf_count_t S_RWops_GetFileLength(void *userData) {
  SDL_IOStream *rwops = (SDL_IOStream *) userData;
  return SDL_GetIOSize(rwops);
}

/**
 * @brief Seeks the `SDL_IOStream` for use as a libsndfile virtual seek callback.
 */
static sf_count_t S_RWops_Seek(sf_count_t offset, int whence, void *userData) {
  SDL_IOStream *rwops = (SDL_IOStream *) userData;
  return SDL_SeekIO(rwops, offset, whence);
}

/**
 * @brief Reads from the `SDL_IOStream` for use as a libsndfile virtual read callback.
 */
static sf_count_t S_RWops_Read(void *ptr, sf_count_t count, void *userData) {
  SDL_IOStream *rwops = (SDL_IOStream *) userData;
  return SDL_ReadIO(rwops, ptr, count);
}

/**
 * @brief Writes to the `SDL_IOStream` for use as a libsndfile virtual write callback.
 */
static sf_count_t S_RWops_Write(const void *ptr, sf_count_t count, void *userData) {
  SDL_IOStream *rwops = (SDL_IOStream *) userData;
  return SDL_WriteIO(rwops, ptr, count);
}

/**
 * @brief Returns the current position of the `SDL_IOStream` for use as a libsndfile virtual tell callback.
 */
static sf_count_t S_RWops_Tell(void *userData) {
  SDL_IOStream *rwops = (SDL_IOStream *) userData;
  return SDL_TellIO(rwops);
}

/**
 * @brief An interface to `SDL_IOStream` for libsndfile
 */
SF_VIRTUAL_IO soundRwopsIo = {
  .get_filelen = S_RWops_GetFileLength,
  .seek = S_RWops_Seek,
  .read = S_RWops_Read,
  .write = S_RWops_Write,
  .tell = S_RWops_Tell
};

/**
 * @brief Returns the size of the PhysFS file for use as a libsndfile virtual file length callback.
 */
static sf_count_t S_PhysFS_GetFileLength(void *userData) {
  File *file = (File *) userData;
  return Fs_FileLength(file);
}

/**
 * @brief Seeks the PhysFS file for use as a libsndfile virtual seek callback.
 */
static sf_count_t S_PhysFS_Seek(sf_count_t offset, int whence, void *userData) {
  File *file = (File *) userData;

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
static sf_count_t S_PhysFS_Read(void *ptr, sf_count_t count, void *userData) {
  File *file = (File *) userData;
  return Fs_Read(file, ptr, 1, count);
}

/**
 * @brief Writes to the PhysFS file for use as a libsndfile virtual write callback.
 */
static sf_count_t S_PhysFS_Write(const void *ptr, sf_count_t count, void *userData) {
  File *file = (File *) userData;
  return Fs_Write(file, ptr, 1, count);
}

/**
 * @brief Returns the current position of the PhysFS file for use as a libsndfile virtual tell callback.
 */
static sf_count_t S_PhysFS_Tell(void *userData) {
  File *file = (File *) userData;
  return Fs_Tell(file);
}

/**
 * @brief An interface to PhysFS for libsndfile
 */
SF_VIRTUAL_IO soundPhysfsIo = {
  .get_filelen = S_PhysFS_GetFileLength,
  .seek = S_PhysFS_Seek,
  .read = S_PhysFS_Read,
  .write = S_PhysFS_Write,
  .tell = S_PhysFS_Tell
};

/**
 * @brief Stop sounds that are playing, if any.
 */
void S_Stop(void) {

  // Preserve per-channel filter handles (AL objects outlive channel state)
  ALuint filters[MAX_CHANNELS];
  for (int32_t i = 0; i < MAX_CHANNELS; i++) {
    filters[i] = soundContext.channels[i].filter;
  }

  memset(soundContext.channels, 0, sizeof(soundContext.channels));

  for (int32_t i = 0; i < MAX_CHANNELS; i++) {
    soundContext.channels[i].filter = filters[i];
  }

  soundContext.prevTicks = 0;

  alSourceStopv(MAX_CHANNELS, soundContext.sources);

  for (size_t i = 0; i < MAX_CHANNELS; i++) {
    alSourcei(soundContext.sources[i], AL_BUFFER, 0);
  }

  S_GetError(NULL);
}

/**
 * @brief Initialize the per-frame attributes of a sound stage.
 */
void S_InitStage(SoundStage *stage) {
  stage->ticks = (uint32_t) SDL_GetTicks();
  stage->numSamples = 0;
}

/**
 * @brief Renders the specified stage, adding channels from the defined play samples.
 */
void S_RenderStage(SoundStage *stage) {

  assert(stage);

  if (!soundContext.context) {
    return;
  }

  const SoundPlaySample *s = stage->samples;
  for (int32_t i = 0; i < stage->numSamples; i++, s++) {

    if (s->flags & S_PLAY_FRAME) {
      SoundChannel *ch = soundContext.channels;
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

    soundContext.channels[c].play = *s;
    soundContext.channels[c].timestamp = stage->ticks;
  }

  S_RenderMusic(stage);

  S_MixChannels(stage);
  
  s_volume->modified = false;
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

  s_getError = Cvar_Add("s_getError", "0", CVAR_DEVELOPER, "Log OpenAL errors to the console (developer tool");

  S_InitDevices();

  s_ambientVolume = Cvar_Add("s_ambientVolume", "1", CVAR_ARCHIVE, "Ambient sound volume.");
  s_doppler = Cvar_Add("s_doppler", "1", CVAR_ARCHIVE, "Doppler effect intensity (default 1).");
  s_effects = Cvar_Add("s_effects", "1", CVAR_ARCHIVE | CVAR_S_DEVICE, "Enables advanced sound effects.");
  s_effectsVolume = Cvar_Add("s_effectsVolume", "1", CVAR_ARCHIVE, "Effects sound volume.");
  s_hrtf = Cvar_Add("s_hrtf", "0", CVAR_ARCHIVE | CVAR_S_DEVICE, "Enables HRTF sound spatialization. Recommended for headphones.");
  s_rate = Cvar_Add("s_rate", "44100", CVAR_ARCHIVE | CVAR_S_DEVICE, "Sound sample rate in Hz.");
  s_volume = Cvar_Add("s_volume", "1", CVAR_ARCHIVE, "Master sound volume level.");

  Cvar_ClearAll(CVAR_S_MASK);

  Cmd_Add("s_listMedia", S_ListMedia_f, CMD_SOUND, "List all currently loaded media");
  Cmd_Add("s_stop", S_Stop_f, CMD_SOUND, NULL);
}

/**
 * @brief Initializes the sound subsystem.
 */
void S_Init(void) {
  memset(&soundContext, 0, sizeof(soundContext));

  if (Cvar_GetValue("s_disable")) {
    Com_Warn("Sound disabled\n");
    return;
  }

  Com_Print("Sound initialization...\n");

  S_InitLocal();

  if (!SDL_InitSubSystem(SDL_INIT_AUDIO)) {
    Com_Warn("Failed to initialize audio: %s\n", SDL_GetError());
    return;
  }

  soundContext.initialized = true;

  if (!alcIsExtensionPresent(NULL, "ALC_SOFT_loopback")) {
    Com_Warn("OpenAL driver does not support ALC_SOFT_loopback\n");
    return;
  }

  soundContext.device = alcLoopbackOpenDeviceSOFT(NULL);

  if (!soundContext.device) {
    Com_Warn("%s\n", alcGetString(NULL, alcGetError(NULL)));
    return;
  }

  if (!alcIsRenderFormatSupportedSOFT(soundContext.device, s_rate->integer, ALC_STEREO_SOFT, ALC_SHORT_SOFT)) {
    Com_Warn("Unsupported render format: %dhz stereo 16 bit\n", s_rate->integer);
    return;
  }

  {
    ALCint attrs[11] = {
      ALC_FREQUENCY, s_rate->integer,
      ALC_FORMAT_CHANNELS_SOFT, ALC_STEREO_SOFT,
      ALC_FORMAT_TYPE_SOFT, ALC_SHORT_SOFT,
    };
    int n = 6;

    if (s_hrtf->integer && alcIsExtensionPresent(soundContext.device, "ALC_SOFT_HRTF")) {
      attrs[n++] = ALC_HRTF_SOFT;
      attrs[n++] = ALC_TRUE;

      if (alcIsExtensionPresent(soundContext.device, "ALC_SOFT_output_mode")) {
        attrs[n++] = ALC_OUTPUT_MODE_SOFT;
        attrs[n++] = ALC_STEREO_HRTF_SOFT;
      }
    }

    attrs[n] = 0;
    soundContext.context = alcCreateContext(soundContext.device, attrs);
  }

  if (!soundContext.context || !alcMakeContextCurrent(soundContext.context)) {
    Com_Warn("%s\n", alcGetString(NULL, alcGetError(NULL)));
    return;
  }

  if (!S_InitPlayback()) {

    // leave nothing half built: S_Shutdown keys media and music off the context, and neither
    // has been initialized yet
    alcMakeContextCurrent(NULL);
    alcDestroyContext(soundContext.context);
    soundContext.context = NULL;

    alcCloseDevice(soundContext.device);
    soundContext.device = NULL;
    return;
  }

  const int efxSupported = alcIsExtensionPresent(soundContext.device, "ALC_EXT_EFX");

  soundContext.renderer = (const char *) alGetString(AL_RENDERER);
  soundContext.vendor = (const char *) alGetString(AL_VENDOR);
  soundContext.version = (const char *) alGetString(AL_VERSION);

  Com_Print("  Renderer:   ^2%s^7\n", soundContext.renderer);
  Com_Print("  Vendor:     ^2%s^7\n", soundContext.vendor);
  Com_Print("  Version:    ^2%s^7\n", soundContext.version);

  if (s_hrtf->integer) {
    ALCint hrtfStatus;
    alcGetIntegerv(soundContext.device, ALC_HRTF_STATUS_SOFT, 1, &hrtfStatus);
    if (hrtfStatus == ALC_HRTF_ENABLED_SOFT || hrtfStatus == ALC_HRTF_REQUIRED_SOFT) {
      const ALCchar *name = alcGetString(soundContext.device, ALC_HRTF_SPECIFIER_SOFT);
      Com_Print("  HRTF:       ^2%s^7\n", name ? name : "enabled");
    } else {
      Com_Warn("HRTF requested but not enabled (status %d)\n", hrtfStatus);
    }
  }

  {
    char extBuf[4096];
    Str_Copy(extBuf, alGetString(AL_EXTENSIONS) ? alGetString(AL_EXTENSIONS) : "", sizeof(extBuf));
    char *save = NULL;
    bool first = true;
    for (char *tok = Str_Tokenize(extBuf, " ", &save); tok; tok = Str_Tokenize(NULL, " ", &save)) {
      if (first) {
        Com_Verbose("  Extensions: ^2%s^7\n", tok);
        first = false;
      } else {
        Com_Verbose("              ^2%s^7\n", tok);
      }
    }
  }

  {
    const char *alcExt = alcGetString(soundContext.device, ALC_EXTENSIONS);
    char extBuf[4096];
    Str_Copy(extBuf, alcExt ? alcExt : "", sizeof(extBuf));
    char *save = NULL;
    for (char *tok = Str_Tokenize(extBuf, " ", &save); tok; tok = Str_Tokenize(NULL, " ", &save)) {
      Com_Verbose("              ^2%s^7\n", tok);
    }
  }

  if (s_effects->integer) {
    if (!efxSupported) {
      Com_Warn("s_effects is enabled but OpenAL driver does not support them.\n");
      Cvar_ForceSetInteger(s_effects->name, 0);
      s_effects->modified = false;
      soundContext.effects.loaded = false;
    } else {

      // Per-channel combined lowpass filter (occlusion + underwater blended into one)
      ALuint filters[MAX_CHANNELS];
      alGenFilters(MAX_CHANNELS, filters);
      for (int32_t i = 0; i < MAX_CHANNELS; i++) {
        alFilteri(filters[i], AL_FILTER_TYPE, AL_FILTER_LOWPASS);
        soundContext.channels[i].filter = filters[i];
      }

      alGenEffects(1, &soundContext.effects.reverb);
      if (alGetError() == AL_NO_ERROR) {
        alEffecti(soundContext.effects.reverb, AL_EFFECT_TYPE, AL_EFFECT_EAXREVERB);
        if (alGetError() != AL_NO_ERROR) {
          alEffecti(soundContext.effects.reverb, AL_EFFECT_TYPE, AL_EFFECT_REVERB);
          S_GetError("Failed to set reverb effect type");
        }
      }

      alGenAuxiliaryEffectSlots(1, &soundContext.effects.reverbSlot);
      alAuxiliaryEffectSloti(soundContext.effects.reverbSlot, AL_EFFECTSLOT_EFFECT, (ALint) soundContext.effects.reverb);

      if (alGetError() == AL_NO_ERROR) {
        soundContext.effects.loaded = true;
      } else {
        Com_Warn("s_effects: failed to create filters, disabling.\n");
        soundContext.effects.loaded = false;
      }
    }
  } else {
    soundContext.effects.loaded = false;
  }

  alDistanceModel(AL_LINEAR_DISTANCE_CLAMPED);
  alGenSources(MAX_CHANNELS, soundContext.sources);
  alSpeedOfSound(343.3f * 40.f); // 1 Quake unit ≈ 1 inch; 1 meter ≈ 40 units

  S_GetError(NULL);

  Com_Print("Sound initialized (OpenAL loopback via SDL, %dhz)\n", s_rate->integer);

  S_InitMedia();

  S_InitMusic();

  S_InitVoice();

  soundContext.resampleBuffer = Mem_TagMalloc(sizeof(int16_t) * 2048, MEM_TAG_SOUND);
}

/**
 * @brief Shuts down the sound subsystem, releasing all OpenAL resources and the context.
 * @details Tears down in the reverse order of initialization, and tolerates initialization having
 * failed part way: the playback stream goes first so that no in-flight SDL callback can render
 * through a context that is being destroyed, and each stage is skipped if it never came up.
 */
void S_Shutdown(void) {

  if (!soundContext.initialized) {
    return;
  }

  S_ShutdownPlayback();

  if (soundContext.context) {

    S_Stop();

    alDeleteSources(MAX_CHANNELS, soundContext.sources);

    if (soundContext.effects.loaded) {
      ALuint filters[MAX_CHANNELS];
      for (int32_t i = 0; i < MAX_CHANNELS; i++) {
        filters[i] = soundContext.channels[i].filter;
      }
      alDeleteFilters(MAX_CHANNELS, filters);
      alDeleteAuxiliaryEffectSlots(1, &soundContext.effects.reverbSlot);
      alDeleteEffects(1, &soundContext.effects.reverb);
      soundContext.effects.loaded = false;
    }

    S_GetError(NULL);

    S_ShutdownVoice();

    S_ShutdownMusic();

    S_ShutdownMedia();

    alcMakeContextCurrent(NULL);
    alcDestroyContext(soundContext.context);
  }

  if (soundContext.device) {
    alcCloseDevice(soundContext.device);
  }

  SDL_QuitSubSystem(SDL_INIT_AUDIO);

  Cmd_RemoveAll(CMD_SOUND);

  Mem_Free(soundContext.rawSampleBuffer);
  Mem_Free(soundContext.convertedSampleBuffer);
  Mem_Free(soundContext.resampleBuffer);

  Mem_FreeTag(MEM_TAG_SOUND);
}
