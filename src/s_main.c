/*
 * Copyright(c) 1997-2001 Id Software, Inc.
 * Copyright(c) 2002 The Quakeforge Project.
 * Copyright(c) 2006 Quake2World.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or(at your option) any later version.
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

#include "client.h"

// the sound environment
s_env_t s_env;

cvar_t *s_invert;
cvar_t *s_rate;
cvar_t *s_volume;


/*
 * S_Stop
 */
static void S_Stop(void){

	Mix_HaltChannel(-1);

	memset(s_env.channels, 0, sizeof(s_env.channels));
}


/*
 * S_Frame
 */
void S_Frame(void){
	s_channel_t *ch;
	int i, j;

	if(!s_env.initialized)
		return;

	if(cls.state != ca_active){
		if(Mix_Playing(-1) > 0)
			S_Stop();
		return;
	}

	if(s_invert->modified){  // update reverse stereo
		Mix_SetReverseStereo(MIX_CHANNEL_POST, (int)s_invert->value);
		s_invert->modified = false;
	}

	// update right angle for stereo panning
	AngleVectors(r_view.angles, NULL, s_env.right, NULL);

	// update spatialization for current sounds
	ch = s_env.channels;

	for(i = 0; i < MAX_CHANNELS; i++, ch++){

		if(!ch->sample)
			continue;

		S_SpatializeChannel(ch);
	}

	// add new dynamic sounds
	for(i = 0; i < cl.frame.num_entities; i++){

		const int e = (cl.frame.parse_entities + i) & (MAX_PARSE_ENTITIES - 1);
		const entity_state_t *ent = &cl_parse_entities[e];

		if(!ent->sound)
			continue;

		for(j = 0; j < MAX_CHANNELS; j++){
			if(s_env.channels[j].entnum == ent->number)
				break;
		}

		if(j == MAX_CHANNELS)
			S_StartSample(NULL, ent->number, cl.sound_precache[ent->sound], ATTN_NORM);
	}

	if(cl.underwater)  // add under water sample if appropriate
		S_StartLoopSample(r_view.origin, S_LoadSample("world/under_water"));
}


/*
 * S_Play_f
 */
static void S_Play_f(void){
	int i;

	i = 1;
	while(i < Cmd_Argc()){
		S_StartLocalSample(Cmd_Argv(i));
		i++;
	}
}


/*
 * S_Stop_f
 */
static void S_Stop_f(void){
	S_Stop();
}


/*
 * S_List_f
 */
static void S_List_f(void){
	s_sample_t *sample;
	int i;

	sample = s_env.samples;
	for(i = 0; i < s_env.num_samples; i++, sample++){

		if(!sample->name[0])
			continue;

		Com_Printf("  %s\n", sample->name);
	}
}


/*
 * S_Reload_f
 */
static void S_Reload_f(void){

	S_Stop();

	S_LoadSamples();

	Com_Printf("\n");
	Con_ClearNotify();  // TODO inadequate
}


/*
 * S_Restart_f
 */
static void S_Restart_f(void){

	S_Shutdown();

	S_Init();

	S_Reload_f();
}


/*
 * S_Init
 */
void S_Init(void){
	int freq, channels;
	unsigned short format;

	memset(&s_env, 0, sizeof(s_env));

	if(Cvar_GetValue("s_disable")){
		Com_Warn("Sound disabled.\n");
		return;
	}

	Com_Printf("Sound initialization..\n");

	s_invert = Cvar_Get("s_invert", "0", CVAR_ARCHIVE, "Invert left and right channels.");
	s_volume = Cvar_Get("s_volume", "0.7", CVAR_ARCHIVE, "Global sound volume level.");
	s_rate = Cvar_Get("s_rate", "", CVAR_ARCHIVE | CVAR_S_DEVICE, "Sound sampling rate in Hz.");

	Cmd_AddCommand("s_restart", S_Restart_f, "Restart the sound subsystem");
	Cmd_AddCommand("s_reload", S_Reload_f, "Reload all loaded sounds");
	Cmd_AddCommand("s_play", S_Play_f, NULL);
	Cmd_AddCommand("s_stop", S_Stop_f, NULL);
	Cmd_AddCommand("s_list", S_List_f, NULL);

	if(SDL_WasInit(SDL_INIT_EVERYTHING) == 0){
		if(SDL_Init(SDL_INIT_AUDIO) < 0){
			Com_Warn("S_Init: %s.\n", SDL_GetError());
			return;
		}
	} else if(SDL_WasInit(SDL_INIT_AUDIO) == 0){
		if(SDL_InitSubSystem(SDL_INIT_AUDIO) < 0){
			Com_Warn("S_Init: %s.\n", SDL_GetError());
			return;
		}
	}

	if(Mix_OpenAudio((int)s_rate->value, MIX_DEFAULT_FORMAT, 2, 1024) == -1){
		Com_Warn("S_Init: %s\n", Mix_GetError());
		return;
	}

	if(Mix_QuerySpec(&freq, &format, &channels) == 0){
		Com_Warn("S_Init: %s\n", Mix_GetError());
		return;
	}

	if(Mix_AllocateChannels(MAX_CHANNELS) != MAX_CHANNELS){
		Com_Warn("S_Init: %s\n", Mix_GetError());
		return;
	}

	Mix_ChannelFinished(S_FreeChannel);

	Com_Printf("Sound initialized %dKHz %d channels.\n", freq, channels);

	s_env.initialized = true;
}


/*
 * S_Shutdown
 */
void S_Shutdown(void){

	S_Stop();

	Mix_AllocateChannels(0);

	S_FreeSamples();

	Mix_CloseAudio();

	if(SDL_WasInit(SDL_INIT_EVERYTHING) == SDL_INIT_AUDIO)
		SDL_Quit();
	else
		SDL_QuitSubSystem(SDL_INIT_AUDIO);

	Cmd_RemoveCommand("s_play");
	Cmd_RemoveCommand("s_stop");
	Cmd_RemoveCommand("s_list");
	Cmd_RemoveCommand("s_restart");
	Cmd_RemoveCommand("s_reload");

	s_env.initialized = false;
}
