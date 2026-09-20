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

#include <SDL3/SDL_timer.h>

#include "s_local.h"

Cvar *s_musicVolume;

#define MUSIC_BUFFERS 8
#define MUSIC_BUFFER_SIZE 16384

static struct {
  ALuint source;
  ALuint musicBuffers[MUSIC_BUFFERS];
  float *rawFrameBuffer;
  int16_t *frameBuffer;
  size_t resampleFrameBufferSize;
  int16_t *resampleFrameBuffer;
  uint32_t nextBuffer;
  SoundMusic *defaultMusic;
  SoundMusic *currentMusic;
  List *playlist;

  SDL_Thread *thread; // thread sound system runs on
  SDL_Mutex *mutex; // mutex for music state
  bool shutdown;
} module;

/**
 * @brief Returns effective music gain with master volume applied.
 */
static float S_MusicGain(void) {
  return Clampf01(s_volume->value) * Clampf01(s_musicVolume->value);
}

/**
 * @brief Retain event listener for `SoundMusic`.
 */
static bool S_RetainMusic(SoundMedia *self) {
  (void) self;

  return true;
}

/**
 * @brief Free event listener for `SoundMusic`.
 */
static void S_FreeMusic(SoundMedia *self) {
  SoundMusic *music = (SoundMusic *) self;

  if (music->snd) {
    sf_close(music->snd);
  }

  if (music->file) {
    Fs_Close(music->file);
  }
}

/**
 * @brief Handles the actual loading of .ogg music files.
 */
static bool S_LoadMusicFile(const char *name, SF_INFO *info, SNDFILE **snd, File **file) {
  char path[MAX_QPATH];

  *snd = NULL;

  StripExtension(name, path);
  q_snprintf(path, sizeof(path), "music/%s.ogg", name);

  if ((*file = Fs_OpenRead(path)) != NULL) {
  
    memset(info, 0, sizeof(*info));

    *snd = sf_open_virtual(&sPhysfsIo, SFM_READ, info, *file);

    if (!*snd || sf_error(*snd)) {
      Com_Warn("%s: %s\n", path, sf_strerror(*snd));

      sf_close(*snd);

      Fs_Close(*file);

      *snd = NULL;
    }
  } else {
    Com_Debug(DEBUG_SOUND, "Failed to load %s\n", name);
  }

  return !!*snd;
}

/**
 * @brief Clears the musics playlist so that it may be rebuilt.
 */
void S_ClearPlaylist(void) {

    module.playlist = release(module.playlist);
}

/**
 * @brief Returns the currently playing music track, or `NULL` if none.
 */
SoundMusic *S_CurrentMusic(void) {
  return module.currentMusic;
}

/**
 * @brief Returns true if @p music is in the current playlist.
 */
bool S_PlaylistContains(const SoundMusic *music) {
  if (!module.playlist) { return false; }
  for (const ListNode *n = module.playlist->head; n; n = n->next) {
    if (n->element == music) { return true; }
  }
  return false;
}

/**
 * @brief Loads the music by the specified name.
 */
SoundMusic *S_LoadMusic(const char *name) {
  char key[MAX_QPATH];
  SoundMusic *music = NULL;

  StripExtension(name, key);

  if (!(music = (SoundMusic *) S_FindMedia(key, S_MEDIA_MUSIC))) {
    SF_INFO info;
    SNDFILE *snd;
    File *file;

    if (S_LoadMusicFile(key, &info, &snd, &file)) {

      music = (SoundMusic *) S_AllocMedia(key, sizeof(SoundMusic), S_MEDIA_MUSIC);

      music->media.type = S_MEDIA_MUSIC;

      music->media.Retain = S_RetainMusic;
      music->media.Free = S_FreeMusic;
      
      music->info = info;
      music->snd = snd;
      music->file = file;

      S_RegisterMedia((SoundMedia *) music);
    } else {
      Com_Debug(DEBUG_SOUND, "S_LoadMusic: Couldn't load %s\n", key);
      music = NULL;
    }
  }

  if (music) {
    if (!module.playlist) {
      module.playlist = $(alloc(List), init);
    }
    $(module.playlist, append, music);
  }

  return music;
}

/**
 * @brief Stops music playback.
 */
void S_StopMusic(void) {

  if (module.currentMusic == NULL) {
    return;
  }
  
  Com_Debug(DEBUG_SOUND, "Stopping\n");

  alSourceStop(module.source);
  S_GetError(NULL);

  module.currentMusic = NULL;
}

/**
 * @brief Handles music buffering for the specified music
 * @param setupBuffers If the buffers should be pulled directly from the buffer list instead of
 * from the consumed buffer list. Use this on first call of Play only.
 */
static void S_BufferMusic(SoundMusic *music, bool setupBuffers) {

  if (!music->snd) {
    return;
  }

  int32_t buffersProcessed = MUSIC_BUFFERS;

  if (!setupBuffers) {
    // if we're EOF, we can quit here and just let the source expire buffers
    if (music->eof) {
      return;
    }

    alGetSourcei(module.source, AL_BUFFERS_PROCESSED, &buffersProcessed);
  } else {
    music->eof = false;
    sf_seek(music->snd, 0, SEEK_SET);
  }

  if (!buffersProcessed) {
    return;
  }

  int32_t i;

  // go through the buffers we have left to add and start decoding
  for (i = 0; i < buffersProcessed; i++) {

    const sf_count_t wantedFrames = (MUSIC_BUFFER_SIZE / sizeof(*module.frameBuffer)) / music->info.channels;
    sf_count_t frames = sf_readf_float(music->snd, module.rawFrameBuffer, wantedFrames) * music->info.channels;

    if (!frames) {
      break;
    }
    
    S_ConvertSamples(module.rawFrameBuffer, frames, &module.frameBuffer, NULL);

    const int16_t *frameBuffer = module.frameBuffer;

    if (music->info.samplerate != s_rate->integer) {
      frames = S_Resample(music->info.channels,
                          music->info.samplerate,
                          s_rate->integer,
                          frames,
                          module.frameBuffer,
                          &module.resampleFrameBuffer,
                          &module.resampleFrameBufferSize);
      frameBuffer = module.resampleFrameBuffer;
    }

    ALuint buffer;

    if (setupBuffers) {
      buffer = module.musicBuffers[module.nextBuffer];
      module.nextBuffer = (module.nextBuffer + 1) % MUSIC_BUFFERS;
    } else {
      alSourceUnqueueBuffers(module.source, 1, &buffer);
    }

    const ALsizei size = (ALsizei) frames * sizeof(int16_t);
    alBufferData(buffer, AL_FORMAT_STEREO16, frameBuffer, size, s_rate->integer);

    alSourceQueueBuffers(module.source, 1, &buffer);
    S_GetError(NULL);
  }
}

/**
 * @brief Begins playback of the specified `SoundMusic`.
 */
static void S_PlayMusic(SoundMusic *music) {

  Com_Debug(DEBUG_SOUND, "Playing %s\n", music->media.name);

  SDL_LockMutex(module.mutex);

  S_StopMusic();

  int32_t buffersProcessed;
  alGetSourcei(module.source, AL_BUFFERS_PROCESSED, &buffersProcessed);

  if (buffersProcessed) {
    ALuint buffersList[buffersProcessed];
    alSourceUnqueueBuffers(module.source, buffersProcessed, buffersList);
  }

  module.nextBuffer = 0;

  module.currentMusic = music;

  S_BufferMusic(music, true);

  alSourcePlay(module.source);

  S_GetError(NULL);

  SDL_UnlockMutex(module.mutex);
}

/**
 * @brief Returns the previous track in the configured playlist.
 */
static SoundMusic *S_PrevMusic(void) {

  if (module.playlist && module.playlist->count) {

    for (const ListNode *n = module.playlist->head; n; n = n->next) {
      if (n->element == module.currentMusic) {
        if (n->prev) {
          return (SoundMusic *) n->prev->element;
        }
        break;
      }
    }

    return (SoundMusic *) module.playlist->tail->element;
  }

  return module.defaultMusic;
}

/**
 * @brief Returns the next track in the configured playlist.
 */
static SoundMusic *S_NextMusic(void) {

  if (module.playlist && module.playlist->count) {

    for (const ListNode *n = module.playlist->head; n; n = n->next) {
      if (n->element == module.currentMusic) {
        if (n->next) {
          return (SoundMusic *) n->next->element;
        }
        break;
      }
    }

    return (SoundMusic *) module.playlist->head->element;
  }

  return module.defaultMusic;
}

/**
 * @brief Single music thread tick.
 */
static void S_MusicThreadTick(void) {

  if (module.currentMusic) {
    S_BufferMusic(module.currentMusic, false);
  }
}

/**
 * @brief Music thread loop.
 */
static int S_MusicThread(void *data) {

  while (true) {
    
    SDL_LockMutex(module.mutex);
  
    if (module.shutdown) {
      SDL_UnlockMutex(module.mutex);
      return 1;
    }

    S_MusicThreadTick();

    SDL_UnlockMutex(module.mutex);

    // sleep a bit, so music thread doesn't eat cycles
    SDL_Delay(QUETOO_TICK_MILLIS);
  }
}

/**
 * @brief Ensures music playback continues by selecting a new track when one
 * completes.
 */
void S_RenderMusic(const SoundStage *stage) {

  SDL_LockMutex(module.mutex);

  if (s_musicVolume->modified || s_volume->modified) {
    const float volume = S_MusicGain();

    if (volume) {
      alSourcef(module.source, AL_GAIN, volume);
    } else {
      S_StopMusic();
    }

    s_musicVolume->modified = false;
  }

  // if music is enabled but not playing, play that funky music
  ALenum state;
  alGetSourcei(module.source, AL_SOURCE_STATE, &state);

  S_GetError(NULL);

  SDL_UnlockMutex(module.mutex);

  if (S_MusicGain() && (state == AL_STOPPED || state == AL_INITIAL)) {
    S_NextTrack_f();
  }

  if (!module.thread) {
    S_MusicThreadTick();
  }
}

/**
 * @brief Plays the next track in the configured playlist.
 */
void S_NextTrack_f(void) {

  if (S_MusicGain()) {
    SoundMusic *current = S_CurrentMusic();
    SoundMusic *music = S_NextMusic();

    if (music) {
      if (music == module.defaultMusic && current == module.defaultMusic) {
        Com_Debug(DEBUG_SOUND, "Default music already playing\n");
      } else {
        S_PlayMusic(music);
      }
    } else {
      Com_Debug(DEBUG_SOUND, "No music available\n");
    }
  } else {
    Com_Debug(DEBUG_SOUND, "Music is muted\n");
  }
}

/**
 * @brief Plays the previous track in the configured playlist.
 */
void S_PrevTrack_f(void) {

  if (S_MusicGain()) {
    SoundMusic *music = S_PrevMusic();

    if (music) {
      S_PlayMusic(music);
    } else {
      Com_Debug(DEBUG_SOUND, "No music available\n");
    }
  } else {
    Com_Debug(DEBUG_SOUND, "Music is muted\n");
  }
}

/**
 * @brief Toggles pause of music playback.
 */
void S_PauseMusic_f(void) {

  SDL_LockMutex(module.mutex);

  ALenum state;
  alGetSourcei(module.source, AL_SOURCE_STATE, &state);

  if (state == AL_PLAYING) {
    alSourcePause(module.source);
  } else if (state == AL_PAUSED) {
    alSourcePlay(module.source);
  }

  S_GetError(NULL);

  SDL_UnlockMutex(module.mutex);
}

/**
 * @brief Initializes the music state, loading the default track.
 */
void S_InitMusic(void) {

  memset(&module, 0, sizeof(module));
  
  s_musicVolume = Cvar_Add("s_musicVolume", "0.5", CVAR_ARCHIVE, "Music volume level.");

  module.rawFrameBuffer = Mem_TagMalloc(sizeof(float) * MUSIC_BUFFER_SIZE, MEM_TAG_SOUND);
  module.frameBuffer = Mem_TagMalloc(sizeof(int16_t) * MUSIC_BUFFER_SIZE, MEM_TAG_SOUND);
  module.resampleFrameBuffer = NULL;

  Cmd_Add("s_nextTrack", S_NextTrack_f, CMD_SOUND, "Play the next music track.");
  Cmd_Add("s_prevTrack", S_PrevTrack_f, CMD_SOUND, "Play the previous music track.");
  Cmd_Add("s_pauseMusic", S_PauseMusic_f, CMD_SOUND, "Pause or resume music playback.");

  alGenSources(1, &module.source);
  
  if (!module.source) {
    Com_Warn("Couldn't allocate source: %s\n", alGetString(alGetError()));
    return;
  }

  alSourcef(module.source, AL_GAIN, S_MusicGain());
  alSourcei(module.source, AL_SOURCE_RELATIVE, AL_TRUE);
  alSourcef(module.source, AL_ROLLOFF_FACTOR, 0.f);

  // Bypass spatialization and HRTF for the stereo music stream
  if (alIsExtensionPresent("AL_SOFT_direct_channels")) {
    alSourcei(module.source, AL_DIRECT_CHANNELS_SOFT, AL_TRUE);
  }

  alGenBuffers(MUSIC_BUFFERS, module.musicBuffers);

  if (!*module.musicBuffers) {
    Com_Warn("Couldn't allocate buffers: %s\n", alGetString(alGetError()));
    return;
  }

  module.defaultMusic = S_LoadMusic("gtdstudio-explore");
  S_ClearPlaylist();

  module.mutex = SDL_CreateMutex();

  module.thread = SDL_CreateThread(S_MusicThread, __func__, NULL);
}

/**
 * @brief Shuts down music playback.
 */
void S_ShutdownMusic(void) {
  
  SDL_LockMutex(module.mutex);
  S_StopMusic();

  if (module.source) {
    alDeleteSources(1, &module.source);
    alDeleteBuffers(MUSIC_BUFFERS, module.musicBuffers);

    S_GetError(NULL);
  }

  if (module.thread) {
    module.shutdown = true;
  
    SDL_UnlockMutex(module.mutex);
    SDL_WaitThread(module.thread, NULL); // wait for thread to end
  } else {
    SDL_UnlockMutex(module.mutex);
  }

  // kill mutex
  SDL_DestroyMutex(module.mutex);
  
  Mem_Free(module.rawFrameBuffer);
  Mem_Free(module.frameBuffer);

  if (module.resampleFrameBuffer) {
    Mem_Free(module.resampleFrameBuffer);
  }
}
