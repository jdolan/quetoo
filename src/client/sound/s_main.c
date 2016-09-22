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

// the sound environment
s_env_t s_env;

cvar_t *s_ambient;
cvar_t *s_music_volume;
cvar_t *s_reverse;
cvar_t *s_rate;
cvar_t *s_volume;

extern cl_client_t cl;
extern cl_static_t cls;

/**
 * @brief
 */
static void S_Stop(void) {

	Mix_HaltChannel(-1);

	memset(s_env.channels, 0, sizeof(s_env.channels));
}

/**
 * @brief Adds a single frame of audio. Also, if a loading cycle has completed,
 * media is freed here.
 */
void S_Frame(void) {

	if (!s_env.initialized)
		return;

	S_FrameMusic();

	if (cls.state != CL_ACTIVE) {
		if (Mix_Playing(-1) > 0)
			S_Stop();
		return;
	}

	if (s_env.update) {
		s_env.update = false;
		S_FreeMedia();
	}

	if (s_reverse->modified) {
		Mix_SetReverseStereo(MIX_CHANNEL_POST, s_reverse->integer);
		s_reverse->modified = false;
	}

	S_MixChannels();
}

/**
 * @brief Loads all media for the sound subsystem.
 */
void S_LoadMedia(void) {
	extern cl_client_t cl;

	if (!s_env.initialized)
		return; // sound disabled

	if (!cl.config_strings[CS_MODELS][0]) {
		return; // no map specified
	}

	S_ClearPlaylist();

	S_BeginLoading();

	Cl_LoadingProgress(80, "sounds");

	for (uint32_t i = 0; i < MAX_SOUNDS; i++) {

		if (!cl.config_strings[CS_SOUNDS + i][0])
			break;

		cl.sound_precache[i] = S_LoadSample(cl.config_strings[CS_SOUNDS + i]);
	}

	for (uint32_t i = 0; i < MAX_MUSICS; i++) {

		if (!cl.config_strings[CS_MUSICS + i][0])
			break;

		cl.music_precache[i] = S_LoadMusic(cl.config_strings[CS_MUSICS + i]);
	}

	S_NextTrack_f();

	Cl_LoadingProgress(85, "music");

	s_env.update = true;
}

/**
 * @brief
 */
static void S_Play_f(void) {

	int32_t i = 1;
	while (i < Cmd_Argc()) {
		S_AddSample(&(const s_play_sample_t) {
			.sample = S_LoadSample(Cmd_Argv(i++))
		});
	}
}

/**
 * @brief
 */
static void S_Stop_f(void) {
	S_Stop();
}

/**
 * @brief
 */
static void S_Restart_f(void) {

	if (cls.state == CL_LOADING)
		return;

	S_Shutdown();

	S_Init();

	const cl_state_t state = cls.state;

	if (cls.state == CL_ACTIVE) {
		cls.state = CL_LOADING;
	}

	S_LoadMedia();

	cls.state = state;
}

/**
 * @brief Initializes variables and commands for the sound subsystem.
 */
static void S_InitLocal(void) {

	s_ambient = Cvar_Get("s_ambient", "1", CVAR_ARCHIVE, "Controls playback of ambient sounds.");
	s_music_volume = Cvar_Get("s_music_volume", "0.15", CVAR_ARCHIVE, "Music volume level.");
	s_rate = Cvar_Get("s_rate", "44100", CVAR_ARCHIVE | CVAR_S_DEVICE, "Sound sample rate in Hz.");
	s_reverse = Cvar_Get("s_reverse", "0", CVAR_ARCHIVE, "Reverse left and right channels.");
	s_volume = Cvar_Get("s_volume", "1.0", CVAR_ARCHIVE, "Global sound volume level.");

	Cvar_ClearAll(CVAR_S_MASK);

	Cmd_Add("s_list_media", S_ListMedia_f, CMD_SOUND, "List all currently loaded media");
	Cmd_Add("s_next_track", S_NextTrack_f, CMD_SOUND, "Play the next music track.");
	Cmd_Add("s_play", S_Play_f, CMD_SOUND, NULL);
	Cmd_Add("s_restart", S_Restart_f, CMD_SOUND, "Restart the sound subsystem");
	Cmd_Add("s_stop", S_Stop_f, CMD_SOUND, NULL);
}

/**
 * @brief Initializes the sound subsystem.
 */
void S_Init(void) {
	int32_t freq, channels;
	uint16_t format;

	memset(&s_env, 0, sizeof(s_env));

	if (Cvar_GetValue("s_disable")) {
		Com_Warn("Sound disabled\n");
		return;
	}

	Com_Print("Sound initialization...\n");

	S_InitLocal();

	if (SDL_WasInit(SDL_INIT_AUDIO) == 0) {
		if (SDL_InitSubSystem(SDL_INIT_AUDIO) < 0) {
			Com_Warn("%s\n", SDL_GetError());
			return;
		}
	}

	if (Mix_OpenAudio(s_rate->integer, MIX_DEFAULT_FORMAT, 2, 1024) == -1) {
		Com_Warn("%s\n", Mix_GetError());
		return;
	}

	if (Mix_QuerySpec(&freq, &format, &channels) == 0) {
		Com_Warn("%s\n", Mix_GetError());
		return;
	}

	if (Mix_AllocateChannels(MAX_CHANNELS) != MAX_CHANNELS) {
		Com_Warn("%s\n", Mix_GetError());
		return;
	}

	Mix_ChannelFinished(S_FreeChannel);

	Com_Print("Sound initialized %dKHz %d channels\n", freq, channels);

	s_env.initialized = true;

	S_InitMedia();

	S_InitMusic();
}

/**
 * @brief
 */
void S_Shutdown(void) {

	if (!s_env.initialized)
		return;

	S_Stop();

	Mix_AllocateChannels(0);

	S_ShutdownMusic();

	S_ShutdownMedia();

	Mix_CloseAudio();

	if (SDL_WasInit(SDL_INIT_EVERYTHING) == SDL_INIT_AUDIO)
		SDL_Quit();
	else
		SDL_QuitSubSystem(SDL_INIT_AUDIO);

	Cmd_RemoveAll(CMD_SOUND);

	s_env.initialized = false;

	Mem_FreeTag(MEM_TAG_SOUND);
}
