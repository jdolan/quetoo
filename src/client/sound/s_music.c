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

#include <SDL3/SDL_timer.h>

#include "s_local.h"

cvar_t *s_music_volume;

#define MUSIC_BUFFERS 8
#define MUSIC_BUFFER_SIZE 16384

static struct {
  ALuint source;
  ALuint music_buffers[MUSIC_BUFFERS];
  float *raw_frame_buffer;
  int16_t *frame_buffer;
  size_t resample_frame_buffer_size;
  int16_t *resample_frame_buffer;
  uint32_t next_buffer;
  s_music_t *default_music;
  s_music_t *current_music;
  List *playlist;

  SDL_Thread *thread; // thread sound system runs on
  SDL_Mutex *mutex; // mutex for music state
  bool shutdown;
} s_music_state;

/**
 * @brief Returns effective music gain with master volume applied.
 */
static float S_MusicGain(void) {
  return Clampf01(s_volume->value) * Clampf01(s_music_volume->value);
}

/**
 * @brief Retain event listener for `s_music_t`.
 */
static bool S_RetainMusic(s_media_t *self) {
  (void) self;

  return true;
}

/**
 * @brief Free event listener for `s_music_t`.
 */
static void S_FreeMusic(s_media_t *self) {
  s_music_t *music = (s_music_t *) self;

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
static bool S_LoadMusicFile(const char *name, SF_INFO *info, SNDFILE **snd, file_t **file) {
  char path[MAX_QPATH];

  *snd = NULL;

  StripExtension(name, path);
  q_snprintf(path, sizeof(path), "music/%s.ogg", name);

  if ((*file = Fs_OpenRead(path)) != NULL) {
  
    memset(info, 0, sizeof(*info));

    *snd = sf_open_virtual(&s_physfs_io, SFM_READ, info, *file);

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

    s_music_state.playlist = release(s_music_state.playlist);
}

/**
 * @brief Returns the currently playing music track, or `NULL` if none.
 */
s_music_t *S_CurrentMusic(void) {
  return s_music_state.current_music;
}

/**
 * @brief Returns true if @p music is in the current playlist.
 */
bool S_PlaylistContains(const s_music_t *music) {
  if (!s_music_state.playlist) { return false; }
  for (const ListNode *n = s_music_state.playlist->head; n; n = n->next) {
    if (n->element == music) { return true; }
  }
  return false;
}

/**
 * @brief Loads the music by the specified name.
 */
s_music_t *S_LoadMusic(const char *name) {
  char key[MAX_QPATH];
  s_music_t *music = NULL;

  StripExtension(name, key);

  if (!(music = (s_music_t *) S_FindMedia(key, S_MEDIA_MUSIC))) {
    SF_INFO info;
    SNDFILE *snd;
    file_t *file;

    if (S_LoadMusicFile(key, &info, &snd, &file)) {

      music = (s_music_t *) S_AllocMedia(key, sizeof(s_music_t), S_MEDIA_MUSIC);

      music->media.type = S_MEDIA_MUSIC;

      music->media.Retain = S_RetainMusic;
      music->media.Free = S_FreeMusic;
      
      music->info = info;
      music->snd = snd;
      music->file = file;

      S_RegisterMedia((s_media_t *) music);
    } else {
      Com_Debug(DEBUG_SOUND, "S_LoadMusic: Couldn't load %s\n", key);
      music = NULL;
    }
  }

  if (music) {
    if (!s_music_state.playlist) {
      s_music_state.playlist = $(alloc(List), init);
    }
    $(s_music_state.playlist, appendElement, music);
  }

  return music;
}

/**
 * @brief Stops music playback.
 */
void S_StopMusic(void) {

  Com_Debug(DEBUG_SOUND, "Stopping\n");

  alSourceStop(s_music_state.source);
  S_GetError(NULL);

  s_music_state.current_music = NULL;
}

/**
 * @brief Handles music buffering for the specified music
 * @param setup_buffers If the buffers should be pulled directly from the buffer list instead of
 * from the consumed buffer list. Use this on first call of Play only.
 */
static void S_BufferMusic(s_music_t *music, bool setup_buffers) {

  if (!music->snd) {
    return;
  }

  int32_t buffers_processed = MUSIC_BUFFERS;

  if (!setup_buffers) {
    // if we're EOF, we can quit here and just let the source expire buffers
    if (music->eof) {
      return;
    }

    alGetSourcei(s_music_state.source, AL_BUFFERS_PROCESSED, &buffers_processed);
  } else {
    music->eof = false;
    sf_seek(music->snd, 0, SEEK_SET);
  }

  if (!buffers_processed) {
    return;
  }

  int32_t i;

  // go through the buffers we have left to add and start decoding
  for (i = 0; i < buffers_processed; i++) {

    const sf_count_t wanted_frames = (MUSIC_BUFFER_SIZE / sizeof(*s_music_state.frame_buffer)) / music->info.channels;
    sf_count_t frames = sf_readf_float(music->snd, s_music_state.raw_frame_buffer, wanted_frames) * music->info.channels;

    if (!frames) {
      break;
    }
    
    S_ConvertSamples(s_music_state.raw_frame_buffer, frames, &s_music_state.frame_buffer, NULL);

    const int16_t *frame_buffer = s_music_state.frame_buffer;

    if (music->info.samplerate != s_rate->integer) {
      frames = S_Resample(music->info.channels,
                          music->info.samplerate,
                          s_rate->integer,
                          frames,
                          s_music_state.frame_buffer,
                          &s_music_state.resample_frame_buffer,
                          &s_music_state.resample_frame_buffer_size);
      frame_buffer = s_music_state.resample_frame_buffer;
    }

    ALuint buffer;

    if (setup_buffers) {
      buffer = s_music_state.music_buffers[s_music_state.next_buffer];
      s_music_state.next_buffer = (s_music_state.next_buffer + 1) % MUSIC_BUFFERS;
    } else {
      alSourceUnqueueBuffers(s_music_state.source, 1, &buffer);
    }

    const ALsizei size = (ALsizei) frames * sizeof(int16_t);
    alBufferData(buffer, AL_FORMAT_STEREO16, frame_buffer, size, s_rate->integer);

    alSourceQueueBuffers(s_music_state.source, 1, &buffer);
    S_GetError(NULL);
  }
}

/**
 * @brief Begins playback of the specified `s_music_t`.
 */
static void S_PlayMusic(s_music_t *music) {

  Com_Debug(DEBUG_SOUND, "Playing %s\n", music->media.name);

  SDL_LockMutex(s_music_state.mutex);

  S_StopMusic();

  int32_t buffers_processed;
  alGetSourcei(s_music_state.source, AL_BUFFERS_PROCESSED, &buffers_processed);

  if (buffers_processed) {
    ALuint buffers_list[buffers_processed];
    alSourceUnqueueBuffers(s_music_state.source, buffers_processed, buffers_list);
  }

  s_music_state.next_buffer = 0;

  s_music_state.current_music = music;

  S_BufferMusic(music, true);

  alSourcePlay(s_music_state.source);

  S_GetError(NULL);

  SDL_UnlockMutex(s_music_state.mutex);
}

/**
 * @brief Returns the previous track in the configured playlist.
 */
static s_music_t *S_PrevMusic(void) {

  if (s_music_state.playlist && s_music_state.playlist->count) {

    for (const ListNode *n = s_music_state.playlist->head; n; n = n->next) {
      if (n->element == s_music_state.current_music) {
        if (n->prev) {
          return (s_music_t *) n->prev->element;
        }
        break;
      }
    }

    return (s_music_t *) s_music_state.playlist->tail->element;
  }

  return s_music_state.default_music;
}

/**
 * @brief Returns the next track in the configured playlist.
 */
static s_music_t *S_NextMusic(void) {

  if (s_music_state.playlist && s_music_state.playlist->count) {

    for (const ListNode *n = s_music_state.playlist->head; n; n = n->next) {
      if (n->element == s_music_state.current_music) {
        if (n->next) {
          return (s_music_t *) n->next->element;
        }
        break;
      }
    }

    return (s_music_t *) s_music_state.playlist->head->element;
  }

  return s_music_state.default_music;
}

/**
 * @brief Single music thread tick.
 */
static void S_MusicThreadTick(void) {

  if (s_music_state.current_music) {
    S_BufferMusic(s_music_state.current_music, false);
  }
}

/**
 * @brief Music thread loop.
 */
static int S_MusicThread(void *data) {

  while (true) {
    
    SDL_LockMutex(s_music_state.mutex);
  
    if (s_music_state.shutdown) {
      SDL_UnlockMutex(s_music_state.mutex);
      return 1;
    }

    S_MusicThreadTick();

    SDL_UnlockMutex(s_music_state.mutex);

    // sleep a bit, so music thread doesn't eat cycles
    SDL_Delay(QUETOO_TICK_MILLIS);
  }
}

/**
 * @brief Ensures music playback continues by selecting a new track when one
 * completes.
 */
void S_RenderMusic(const s_stage_t *stage) {

  SDL_LockMutex(s_music_state.mutex);

  if (s_music_volume->modified || s_volume->modified) {
    const float volume = S_MusicGain();

    if (volume) {
      alSourcef(s_music_state.source, AL_GAIN, volume);
    } else {
      S_StopMusic();
    }

    s_music_volume->modified = false;
  }

  // if music is enabled but not playing, play that funky music
  ALenum state;
  alGetSourcei(s_music_state.source, AL_SOURCE_STATE, &state);

  S_GetError(NULL);

  SDL_UnlockMutex(s_music_state.mutex);

  if (S_MusicGain() && (state == AL_STOPPED || state == AL_INITIAL)) {
    S_NextTrack_f();
  }

  if (!s_music_state.thread) {
    S_MusicThreadTick();
  }
}

/**
 * @brief Plays the next track in the configured playlist.
 */
void S_NextTrack_f(void) {

  if (S_MusicGain()) {
    s_music_t *current = S_CurrentMusic();
    s_music_t *music = S_NextMusic();

    if (music) {
      if (music == s_music_state.default_music && current == s_music_state.default_music) {
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
    s_music_t *music = S_PrevMusic();

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

  SDL_LockMutex(s_music_state.mutex);

  ALenum state;
  alGetSourcei(s_music_state.source, AL_SOURCE_STATE, &state);

  if (state == AL_PLAYING) {
    alSourcePause(s_music_state.source);
  } else if (state == AL_PAUSED) {
    alSourcePlay(s_music_state.source);
  }

  S_GetError(NULL);

  SDL_UnlockMutex(s_music_state.mutex);
}

/**
 * @brief Initializes the music state, loading the default track.
 */
void S_InitMusic(void) {

  memset(&s_music_state, 0, sizeof(s_music_state));
  
  s_music_volume = Cvar_Add("s_music_volume", "0.5", CVAR_ARCHIVE, "Music volume level.");

  s_music_state.raw_frame_buffer = Mem_TagMalloc(sizeof(float) * MUSIC_BUFFER_SIZE, MEM_TAG_SOUND);
  s_music_state.frame_buffer = Mem_TagMalloc(sizeof(int16_t) * MUSIC_BUFFER_SIZE, MEM_TAG_SOUND);
  s_music_state.resample_frame_buffer = NULL;

  Cmd_Add("s_next_track", S_NextTrack_f, CMD_SOUND, "Play the next music track.");
  Cmd_Add("s_prev_track", S_PrevTrack_f, CMD_SOUND, "Play the previous music track.");
  Cmd_Add("s_pause_music", S_PauseMusic_f, CMD_SOUND, "Pause or resume music playback.");

  alGenSources(1, &s_music_state.source);
  
  if (!s_music_state.source) {
    Com_Warn("Couldn't allocate source: %s\n", alGetString(alGetError()));
    return;
  }

  alSourcef(s_music_state.source, AL_GAIN, S_MusicGain());
  alSourcei(s_music_state.source, AL_SOURCE_RELATIVE, AL_TRUE);
  alSourcef(s_music_state.source, AL_ROLLOFF_FACTOR, 0.f);

  // Bypass spatialization and HRTF for the stereo music stream
  if (alIsExtensionPresent("AL_SOFT_direct_channels")) {
    alSourcei(s_music_state.source, AL_DIRECT_CHANNELS_SOFT, AL_TRUE);
  }

  alGenBuffers(MUSIC_BUFFERS, s_music_state.music_buffers);

  if (!*s_music_state.music_buffers) {
    Com_Warn("Couldn't allocate buffers: %s\n", alGetString(alGetError()));
    return;
  }

  s_music_state.default_music = S_LoadMusic("gtdstudio-explore");
  S_ClearPlaylist();

  s_music_state.mutex = SDL_CreateMutex();

  s_music_state.thread = SDL_CreateThread(S_MusicThread, __func__, NULL);
}

/**
 * @brief Shuts down music playback.
 */
void S_ShutdownMusic(void) {
  
  SDL_LockMutex(s_music_state.mutex);
  S_StopMusic();

  if (s_music_state.source) {
    alDeleteSources(1, &s_music_state.source);
    alDeleteBuffers(MUSIC_BUFFERS, s_music_state.music_buffers);

    S_GetError(NULL);
  }

  if (s_music_state.thread) {
    s_music_state.shutdown = true;
  
    SDL_UnlockMutex(s_music_state.mutex);
    SDL_WaitThread(s_music_state.thread, NULL); // wait for thread to end
  } else {
    SDL_UnlockMutex(s_music_state.mutex);
  }

  // kill mutex
  SDL_DestroyMutex(s_music_state.mutex);
  
  Mem_Free(s_music_state.raw_frame_buffer);
  Mem_Free(s_music_state.frame_buffer);

  if (s_music_state.resample_frame_buffer) {
    Mem_Free(s_music_state.resample_frame_buffer);
  }
}
