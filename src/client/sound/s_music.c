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
#include "client.h"

static cvar_t *s_music_buffer_count;
static cvar_t *s_music_buffer_size;

cvar_t *s_music_volume;

typedef struct s_music_state_s {
	ALuint source;
	ALuint *music_buffers;
	uint32_t next_buffer;
	s_music_t *default_music;
	s_music_t *current_music;
	GList *playlist;

	thread_t *thread; // thread sound system runs on
	SDL_mutex *mutex; // mutex for music state
	_Bool shutdown;
} s_music_state_t;

static s_music_state_t s_music_state;

static void S_CheckALError() {

	const ALenum v = alGetError();

	if (v == AL_NO_ERROR) {
		return;
	}

	Com_Debug(DEBUG_BREAKPOINT | DEBUG_SOUND, "AL error: %s\n", alGetString(v));
}

/**
 * @brief Retain event listener for s_music_t.
 */
static _Bool S_RetainMusic(s_media_t *self) {
	s_music_t *music = (s_music_t *) self;

	return GlobMatch("track*", music->media.name, GLOB_FLAGS_NONE);
}

/**
 * @brief Free event listener for s_music_t.
 */
static void S_FreeMusic(s_media_t *self) {
	s_music_t *music = (s_music_t *) self;

	if (music->sample) {
		Sound_FreeSample(music->sample);
		music->sample = NULL;
	}
}

/**
 * @brief Handles the actual loading of .ogg music files.
 */
static _Bool S_LoadMusicFile(const char *name, Sound_Sample **sample) {
	char path[MAX_QPATH];
	void *buffer;

	*sample = NULL;

	StripExtension(name, path);
	g_snprintf(path, sizeof(path), "music/%s.ogg", name);

	int64_t len;
	if ((len = Fs_Load(path, &buffer)) != -1) {

		Sound_AudioInfo desired = {
			.format = AUDIO_S16,
			.channels = 2,
			.rate = s_rate->integer
		};

		Sound_Sample *music = Sound_NewSampleFromMem((const Uint8 *) buffer, (Uint32) len, "ogg", &desired, s_music_buffer_size->value);

		if (!music) {
			Com_Warn("%s\n", Sound_GetError());
		} else {

			if (music->flags & SOUND_SAMPLEFLAG_ERROR) {
				Com_Warn("%s\n", Sound_GetError());
			} else if (!(music->flags & SOUND_SAMPLEFLAG_CANSEEK)) {
				Com_Warn("Music format for %s does not appear to be seekable\n", name);
			} else {
				*sample = music;
			}

			if (!*sample) {
				Sound_FreeSample(music);
			}
		}

	} else {
		Com_Debug(DEBUG_SOUND, "Failed to load %s\n", name);
	}

	return !!*sample;
}

/**
 * @brief Clears the musics playlist so that it may be rebuilt.
 */
void S_ClearPlaylist(void) {

	g_list_free(s_music_state.playlist);

	s_music_state.playlist = NULL;
}

/**
 * @brief Loads the music by the specified name.
 */
s_music_t *S_LoadMusic(const char *name) {
	char key[MAX_QPATH];
	s_music_t *music = NULL;

	StripExtension(name, key);

	if (!(music = (s_music_t *) S_FindMedia(key))) {

		Sound_Sample *sample;

		if (S_LoadMusicFile(key, &sample)) {

			music = (s_music_t *) S_AllocMedia(key, sizeof(s_music_t));

			music->media.type = S_MEDIA_MUSIC;

			music->media.Retain = S_RetainMusic;
			music->media.Free = S_FreeMusic;

			music->sample = sample;

			S_RegisterMedia((s_media_t *) music);
		} else {
			Com_Debug(DEBUG_SOUND, "S_LoadMusic: Couldn't load %s\n", key);
			music = NULL;
		}
	}

	if (music) {
		s_music_state.playlist = g_list_append(s_music_state.playlist, music);
	}

	return music;
}

/**
 * @brief Stops music playback.
 */
static void S_StopMusic(void) {

	alSourceStop(s_music_state.source);

	s_music_state.current_music = NULL;

	S_CheckALError();
}

/**
 * @brief Handles music buffering for the specified music
 * @param setup_buffers If the buffers should be pulled directly from the buffer list instead of
 * from the consumed buffer list. Use this on first call of Play only.
 */
static void S_BufferMusic(s_music_t *music, _Bool setup_buffers) {

	if (!music->sample) {
		return; // ???
	}

	// if we're EOF, we can quit here and just let the source expire buffers
	if (music->sample->flags & SOUND_SAMPLEFLAG_EOF) {
		return;
	}

	S_CheckALError();

	int32_t buffers_processed = s_music_buffer_count->integer;

	if (!setup_buffers) {
		alGetSourcei(s_music_state.source, AL_BUFFERS_PROCESSED, &buffers_processed);
		S_CheckALError();
	} else {
		Sound_Seek(music->sample, 0);
	}

	if (!buffers_processed) {
		return;
	}

	int32_t i;

	// go through the buffers we have left to add and start decoding
	for (i = 0; i < buffers_processed; i++) {

		const uint32_t size_decoded = Sound_Decode(music->sample);

		if (!size_decoded || size_decoded != music->sample->buffer_size) {

			if (music->sample->flags & SOUND_SAMPLEFLAG_ERROR) {
				Com_Warn("Error during music decoding: %s\n", Sound_GetError());
				break;
			}
		}

		ALuint buffer;

		if (setup_buffers) {
			buffer = s_music_state.music_buffers[s_music_state.next_buffer];
			s_music_state.next_buffer = (s_music_state.next_buffer + 1) % s_music_buffer_count->integer;
		} else {
			alSourceUnqueueBuffers(s_music_state.source, 1, &buffer);
		}

		alBufferData(buffer, AL_FORMAT_STEREO16, music->sample->buffer, size_decoded, s_rate->integer);
		alSourceQueueBuffers(s_music_state.source, 1, &buffer);

		S_CheckALError();
	}

	Com_Debug(DEBUG_SOUND, "%i music chunks processed\n", i);
}

/**
 * @brief Begins playback of the specified s_music_t.
 */
static void S_PlayMusic(s_music_t *music) {
	
	SDL_mutexP(s_music_state.mutex);

	S_StopMusic();

	int32_t buffers_processed;
	alGetSourcei(s_music_state.source, AL_BUFFERS_PROCESSED, &buffers_processed);

	if (buffers_processed) {
		ALuint buffers_list[buffers_processed];
		alSourceUnqueueBuffers(s_music_state.source, buffers_processed, buffers_list);
	}

	S_CheckALError();

	s_music_state.next_buffer = 0;

	s_music_state.current_music = music;

	S_BufferMusic(music, true);

	alSourcePlay(s_music_state.source);

	S_CheckALError();

	SDL_mutexV(s_music_state.mutex);
}

/**
 * @brief Returns the next track in the configured playlist.
 */
static s_music_t *S_NextMusic(void) {
	GList *elt;

	if (g_list_length(s_music_state.playlist)) {

		if ((elt = g_list_find(s_music_state.playlist, s_music_state.current_music))) {

			if (elt->next) {
				return (s_music_t *) elt->next->data;
			}
		}

		return g_list_nth_data(s_music_state.playlist, 0);
	}

	return s_music_state.default_music;
}

/**
 * @brief Ensures music playback continues by selecting a new track when one
 * completes.
 */
void S_FrameMusic(void) {
	extern cl_static_t cls;
	static cl_state_t last_state = CL_UNINITIALIZED;

	if (s_music_buffer_count->modified ||
		s_music_buffer_size->modified) {
		S_Restart_f();

		s_music_buffer_size->modified = s_music_buffer_count->modified = false;
		return;
	}
	
	SDL_mutexP(s_music_state.mutex);

	// revert to the default music when the client disconnects
	if (last_state == CL_ACTIVE && cls.state != CL_ACTIVE) {
		S_ClearPlaylist();
		S_StopMusic();
	}

	last_state = cls.state;

	if (s_music_volume->modified) {
		const vec_t volume = Clamp(s_music_volume->value, 0.0, 1.0);

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

	S_CheckALError();

	SDL_mutexV(s_music_state.mutex);

	if (s_music_volume->value && state != AL_PLAYING) {
		S_NextTrack_f();
	}
}

/**
 * @brief Music thread loop.
 */
static void S_MusicThread(void *data) {

	while (true) {
		SDL_mutexP(s_music_state.mutex);
	
		if (s_music_state.shutdown) {
			SDL_mutexV(s_music_state.mutex);
			return;
		}

		if (s_music_state.current_music) {
			S_BufferMusic(s_music_state.current_music, false);
		}

		SDL_mutexV(s_music_state.mutex);
	}
}

/**
 * @brief Plays the next track in the configured playlist.
 */
void S_NextTrack_f(void) {

	if (s_music_volume->value) {
		s_music_t *music = S_NextMusic();

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
 * @brief Initializes the music state, loading the default track.
 */
void S_InitMusic(void) {

	memset(&s_music_state, 0, sizeof(s_music_state));
	
	s_music_buffer_count = Cvar_Add("s_music_buffer_count", "8", CVAR_S_MEDIA, "The number of buffers to store for music streaming.");
	s_music_buffer_size = Cvar_Add("s_music_buffer_size", "16384", CVAR_S_MEDIA, "The size of each buffer for music streaming.");
	
	s_music_volume = Cvar_Add("s_music_volume", "0.15", CVAR_ARCHIVE, "Music volume level.");
	Cmd_Add("s_next_track", S_NextTrack_f, CMD_SOUND, "Play the next music track.");

	s_music_buffer_count->modified = 
		s_music_buffer_size->modified = 
		s_music_volume->modified = false;

	alGenSources(1, &s_music_state.source);
	
	if (!s_music_state.source) {
		Com_Warn("Couldn't allocate source: %s\n", alGetString(alGetError()));
		return;
	}

	alSourcef(s_music_state.source, AL_GAIN, s_music_volume->value);

	s_music_state.music_buffers = Mem_TagMalloc(sizeof(ALuint) * s_music_buffer_count->integer, MEM_TAG_SOUND);
	alGenBuffers(s_music_buffer_count->integer, s_music_state.music_buffers);

	if (!*s_music_state.music_buffers) {
		Com_Warn("Couldn't allocate buffers: %s\n", alGetString(alGetError()));
		return;
	}

	s_music_state.default_music = S_LoadMusic("track3");

	s_music_state.mutex = SDL_CreateMutex();
	s_music_state.thread = Thread_Create(S_MusicThread, NULL);
}

/**
 * @brief Shuts down music playback.
 */
void S_ShutdownMusic(void) {
	
	SDL_mutexP(s_music_state.mutex);
	S_StopMusic();

	if (s_music_state.source) {
		alDeleteSources(1, &s_music_state.source);
		alDeleteBuffers(s_music_buffer_count->integer, s_music_state.music_buffers);
		Mem_Free(s_music_state.music_buffers);
	}

	if (s_music_state.thread) {
		s_music_state.shutdown = true;
	
		SDL_mutexV(s_music_state.mutex);
		Thread_Wait(s_music_state.thread); // wait for thread to end
	} else {
		SDL_mutexV(s_music_state.mutex);
	}

	// kill mutex
	SDL_DestroyMutex(s_music_state.mutex);
}
