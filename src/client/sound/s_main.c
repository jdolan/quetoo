/*
 * Copyright(c) 1997-2001 Id Software, Inc.
 * Copyright(c) 2002 The Quakeforge Project.
 * Copyright(c) 2006 Quake2World.
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

cvar_t *s_reverse;
cvar_t *s_rate;
cvar_t *s_volume;

/*
 * S_Stop
 */
static void S_Stop(void) {

	Mix_HaltChannel(-1);

	memset(s_env.channels, 0, sizeof(s_env.channels));
}

/*
 * S_Frame
 */
void S_Frame(void) {
	s_channel_t *ch;
	int i, j;

	if (!s_env.initialized)
		return;

	S_FrameMusic();

	if (cls.state != CL_ACTIVE) {
		if (Mix_Playing(-1) > 0)
			S_Stop();
		return;
	}

	if (s_reverse->modified) { // update reverse stereo
		Mix_SetReverseStereo(MIX_CHANNEL_POST, s_reverse->integer);
		s_reverse->modified = false;
	}

	// update spatialization for current sounds
	ch = s_env.channels;

	for (i = 0; i < MAX_CHANNELS; i++, ch++) {

		if (!ch->sample)
			continue;

		// reset channel's count for loop samples
		ch->count = 0;

		S_SpatializeChannel(ch);
	}

	// add new dynamic sounds
	for (i = 0; i < cl.frame.num_entities; i++) {

		const int e = (cl.frame.entity_state + i) & ENTITY_STATE_MASK;
		const entity_state_t *ent = &cl.entity_states[e];

		if (!ent->sound)
			continue;

		for (j = 0; j < MAX_CHANNELS; j++) {
			if (s_env.channels[j].ent_num == ent->number)
				break;
		}

		if (j == MAX_CHANNELS)
			S_PlaySample(NULL, ent->number, cl.sound_precache[ent->sound],
					ATTN_NORM);
	}

	if (cl.underwater) // add under water sample if appropriate
		S_LoopSample(r_view.origin, S_LoadSample("world/under_water"));

	// reset the update flag
	s_env.update = false;
}

/*
 * S_LoadMedia
 */
void S_LoadMedia(void) {

	Cl_LoadProgress(80);

	S_LoadSamples();

	Cl_LoadProgress(85);

	S_LoadMusics();

	s_env.update = true;
}

/*
 * S_Play_f
 */
static void S_Play_f(void) {
	int i;

	i = 1;
	while (i < Cmd_Argc()) {
		S_StartLocalSample(Cmd_Argv(i));
		i++;
	}
}

/*
 * S_Stop_f
 */
static void S_Stop_f(void) {
	S_Stop();
}

/*
 * S_List_f
 */
static void S_List_f(void) {
	s_sample_t *sample;
	int i;

	sample = s_env.samples;
	for (i = 0; i < s_env.num_samples; i++, sample++) {

		if (!sample->name[0])
			continue;

		Com_Print("  %s\n", sample->name);
	}
}

/*
 * S_Restart_f
 */
static void S_Restart_f(void) {

	S_Shutdown();

	S_Init();

	cls.loading = 1;

	S_LoadMedia();

	cls.loading = 0;
}

/*
 * S_Init
 */
void S_Init(void) {
	int freq, channels;
	unsigned short format;

	memset(&s_env, 0, sizeof(s_env));

	if (Cvar_GetValue("s_disable")) {
		Com_Warn("Sound disabled.\n");
		return;
	}

	Com_Print("Sound initialization...\n");

	s_rate = Cvar_Get("s_rate", "44100", CVAR_ARCHIVE | CVAR_S_DEVICE,
			"Sound sampling rate in Hz.");
	s_reverse = Cvar_Get("s_reverse", "0", CVAR_ARCHIVE,
			"Reverse left and right channels.");
	s_volume = Cvar_Get("s_volume", "1.0", CVAR_ARCHIVE,
			"Global sound volume level.");

	Cmd_AddCommand("s_restart", S_Restart_f, "Restart the sound subsystem");
	Cmd_AddCommand("s_play", S_Play_f, NULL);
	Cmd_AddCommand("s_stop", S_Stop_f, NULL);
	Cmd_AddCommand("s_list", S_List_f, NULL);

	if (SDL_WasInit(SDL_INIT_EVERYTHING) == 0) {
		if (SDL_Init(SDL_INIT_AUDIO) < 0) {
			Com_Warn("S_Init: %s.\n", SDL_GetError());
			return;
		}
	} else if (SDL_WasInit(SDL_INIT_AUDIO) == 0) {
		if (SDL_InitSubSystem(SDL_INIT_AUDIO) < 0) {
			Com_Warn("S_Init: %s.\n", SDL_GetError());
			return;
		}
	}

	if (Mix_OpenAudio(s_rate->integer, MIX_DEFAULT_FORMAT, 2, 1024) == -1) {
		Com_Warn("S_Init: %s\n", Mix_GetError());
		return;
	}

	if (Mix_QuerySpec(&freq, &format, &channels) == 0) {
		Com_Warn("S_Init: %s\n", Mix_GetError());
		return;
	}

	if (Mix_AllocateChannels(MAX_CHANNELS) != MAX_CHANNELS) {
		Com_Warn("S_Init: %s\n", Mix_GetError());
		return;
	}

	Mix_ChannelFinished(S_FreeChannel);

	Com_Print("Sound initialized %dKHz %d channels.\n", freq, channels);

	s_env.initialized = true;

	S_InitMusic();
}

/*
 * S_Shutdown
 */
void S_Shutdown(void) {

	S_ShutdownMusic();

	S_Stop();

	Mix_AllocateChannels(0);

	S_FreeSamples();

	Mix_CloseAudio();

	if (SDL_WasInit(SDL_INIT_EVERYTHING) == SDL_INIT_AUDIO)
		SDL_Quit();
	else
		SDL_QuitSubSystem(SDL_INIT_AUDIO);

	Cmd_RemoveCommand("s_play");
	Cmd_RemoveCommand("s_stop");
	Cmd_RemoveCommand("s_list");
	Cmd_RemoveCommand("s_restart");

	s_env.initialized = false;
}
