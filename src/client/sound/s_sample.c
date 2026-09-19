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

#define NearestMultiple(n, align)  ((n) == 0 ? 0 : ((n) - 1 - ((n) - 1) % (align) + (align)))

/**
 * @brief Resample audio. outdata will be realloc'd to the size required to handle this operation,
 * so be sure to initialize to `NULL` before calling if it's first time!
 */
size_t S_Resample(const int32_t channels, const int32_t sourceRate, const int32_t destRate, const size_t numFrames, const int16_t *inFrames, int16_t **outFrames, size_t *outSize) {
  
  const float stepscale = (float) sourceRate / (float) destRate;
  const size_t outcount = NearestMultiple((size_t) (numFrames / stepscale), channels);
  const size_t size = outcount * sizeof(int16_t);

  if (outSize && *outSize < size) {
    *outFrames = Mem_Realloc(*outFrames, size);
    *outSize = size;
  }

  int32_t samplefrac = 0;
  const float fracstep = stepscale * 256.0;

  for (size_t i = 0; i < outcount; ) {
    for (int32_t c = 0; c < channels; c++, i++) {
      int32_t srcsample = NearestMultiple(samplefrac >> 8, channels) + c;
      if (srcsample >= (int32_t)(numFrames * channels)) {
        srcsample = (int32_t)(numFrames * channels) - channels + c;
      }

      samplefrac += fracstep;
      (*outFrames)[i] = LittleShort(inFrames[srcsample]);
    }
  }

  return outcount;
}

/**
 * @brief Converts floating-point audio samples to 16-bit signed integers.
 */
void S_ConvertSamples(const float *inputSamples, const sf_count_t numSamples, int16_t **outSamples, size_t *outSize) {
  const size_t size = sizeof(int16_t) * numSamples;

  if (outSize && *outSize < size) {
    *outSamples = Mem_Realloc(*outSamples, size);
    *outSize = size;
  }

  for (sf_count_t i = 0; i < numSamples; i++) {
    (*outSamples)[i] = (int16_t) Clampf(inputSamples[i] * 32768.0f, INT16_MIN, INT16_MAX);
  }
}

/**
 * @brief Attempts to load a sample's audio data from the given file path into an OpenAL buffer.
 */
static int32_t S_LoadSampleBuffer_(SoundSample *sample, char *path) {

  void *buf;
  const int64_t len = Fs_Load(path, &buf);

  if (len != -1) {

    SDL_IOStream *rw = SDL_IOFromConstMem(buf, (int32_t) len);

    SF_INFO info;
    memset(&info, 0, sizeof(info));

    SNDFILE *snd = sf_open_virtual(&sRwopsIo, SFM_READ, &info, rw);

    if (snd) {
      const size_t rawSize = sizeof(float) * info.frames * info.channels;

      if (sContext.rawSampleBufferSize < rawSize) {
        sContext.rawSampleBuffer = Mem_Realloc(sContext.rawSampleBuffer, rawSize);
        sContext.rawSampleBufferSize = rawSize;
      }

      sf_count_t count = sf_readf_float(snd, sContext.rawSampleBuffer, info.frames) * info.channels;

      S_ConvertSamples(sContext.rawSampleBuffer, count, &sContext.convertedSampleBuffer, &sContext.convertedSampleBufferSize);

      const int16_t *buffer = sContext.convertedSampleBuffer;

      if (info.samplerate != s_rate->integer) {
        count = S_Resample(info.channels, info.samplerate, s_rate->integer, count, buffer, &sContext.resampleBuffer, &sContext.resampleBufferSize);
        buffer = sContext.resampleBuffer;
      }

      sample->stereo = info.channels != 1;
      sample->numSamples = count;

      assert(sample->numSamples);

      alGenBuffers(1, &sample->buffer);

      const ALenum format = info.channels == 1 ? AL_FORMAT_MONO16 : AL_FORMAT_STEREO16;
      const ALsizei size = (ALsizei) count * sizeof(int16_t);

      alBufferData(sample->buffer, format, buffer, size, s_rate->integer);

      S_GetError(NULL);
    } else {
      Com_Warn("%s: %s\n", path, sf_strerror(snd));
    }

    sf_close(snd);

    SDL_CloseIO(rw);

    Fs_Free(buf);
  }

  return sample->buffer;
}

/**
 * @brief Searches for and loads the audio file for the given sample, trying supported formats in order.
 */
static void S_LoadSampleBuffer(SoundSample *sample) {
  const char *sndFormats[] = { "ogg", "wav", NULL };

  if (sample->media.name[0] == '*') { // placeholder
    return;
  }

  char path[MAX_QPATH];
  for (const char **fmt = sndFormats; *fmt; fmt++) {
    q_snprintf(path, sizeof(path), "%s.%s", sample->media.name, *fmt);
    if (S_LoadSampleBuffer_(sample, path)) {
      break;
    }
  }

  if (sample->buffer) {
    Com_Debug(DEBUG_SOUND, "Loaded %s for %s\n", path, sample->media.name);
  } else {
    if (!q_strncmp(sample->media.name, "players/", 8)) {
      Com_Debug(DEBUG_SOUND, "Failed to load player sample %s\n", sample->media.name);
    } else {
      Com_Warn("Failed to load %s\n", sample->media.name);
    }
  }
}

/**
 * @brief Free event listener for `SoundSample`.
 */
static void S_FreeSample(SoundMedia *self) {
  SoundSample *sample = (SoundSample *) self;

  if (sample->buffer) {
    alDeleteBuffers(1, &sample->buffer);
    sample->buffer = 0;
  }
}

/**
 * @brief Free event listener for aliased `SoundSample`. Does not delete the
 * OpenAL buffer, which is owned by the sample being aliased.
 */
static void S_FreeAliasedSample(SoundMedia *self) {
  SoundSample *sample = (SoundSample *) self;
  sample->buffer = 0;
}

/**
 * @brief Loads or returns a cached sound sample by name.
 */
SoundSample *S_LoadSample(const char *name, AssetContext context) {

  if (!sContext.context) {
    return NULL;
  }

  if (!name || !name[0]) {
    Com_Error(ERROR_DROP, "NULL name\n");
  }

  char stripped[MAX_QPATH];
  StripExtension(name, stripped);

  char key[MAX_QPATH];
  if (stripped[0] == '*') { // placeholder, resolved per-client at play time; never context-qualified
    q_strlcpy(key, stripped, sizeof(key));
  } else {
    Asset_Path(stripped, key, sizeof(key), context);
  }

  SoundSample *sample = (SoundSample *) S_FindMedia(key, S_MEDIA_SAMPLE);
  if (sample == NULL) {

    sample = (SoundSample *) S_AllocMedia(key, sizeof(SoundSample), S_MEDIA_SAMPLE);

    sample->media.Free = S_FreeSample;

    S_LoadSampleBuffer(sample);

    S_RegisterMedia((SoundMedia *) sample);
  }

  return sample;
}

/**
 * @brief Loads or returns a cached player-model sound sample from the given model and name.
 * @param model The player model name, e.g. `"nitro"`.
 * @param sound_set The model's sound set, e.g. `"male"`, `"female"`, `"cyborg"` (see `RenderMeshModel.sounds`).
 * @param name The sample name, e.g. `"*death_1"`.
 */
SoundSample *S_LoadClientModelSample(const char *model, const char *soundSet, const char *name) {

  if (!sContext.context) {
    return NULL;
  }

  if (!model || !model[0] || !name || !name[0]) {
    Com_Error(ERROR_DROP, "NULL model or name\n");
  }

  char key[MAX_QPATH];
  q_snprintf(key, sizeof(key), "players/%s/%s", model, name + 1);

  SoundSample *sample = (SoundSample *) S_FindMedia(key, S_MEDIA_SAMPLE);
  if (sample == NULL) {

    char relative[MAX_QPATH];
    q_snprintf(relative, sizeof(relative), "%s/%s", model, name + 1);

    sample = S_LoadSample(relative, ASSET_CONTEXT_PLAYERS);
    if (sample->buffer) {
      Com_Debug(DEBUG_SOUND, "Loaded %s\n", key);
    } else {
      SoundSample *aliased = NULL;

      if (soundSet && soundSet[0]) {
        q_snprintf(relative, sizeof(relative), "common/%s/%s", soundSet, name + 1);

        aliased = S_LoadSample(relative, ASSET_CONTEXT_PLAYERS);
        if (!aliased->buffer) {
          aliased = NULL;
        }
      }

      if (aliased == NULL) {
        q_snprintf(relative, sizeof(relative), "common/%s", name + 1);
        aliased = S_LoadSample(relative, ASSET_CONTEXT_PLAYERS);
      }

      if (aliased->buffer) {

        S_RegisterDependency((SoundMedia *) sample, (SoundMedia *) aliased);

        sample->buffer = aliased->buffer;
        sample->numSamples = aliased->numSamples;
        sample->stereo = aliased->stereo;
        sample->media.Free = S_FreeAliasedSample;

        Com_Debug(DEBUG_SOUND, "Aliased %s for %s\n", aliased->media.name, key);
      } else {
        Com_Warn("Failed to load %s\n", key);
      }
    }
  }

  return sample;
}
