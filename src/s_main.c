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

// only begin attenuating sound volumes when outside the FULLVOLUME range
#define SOUND_FULLVOLUME 50
#define SOUND_LOOPATTENUATE 0.003

// the sound environment
s_env_t s_env;

cvar_t *s_channels;
cvar_t *s_mixahead;
cvar_t *s_rate;
cvar_t *s_testsound;
cvar_t *s_volume;

static cvar_t *s_ambient;
static cvar_t *s_show;


/*
 * S_FindName
 */
static s_sample_t *S_FindName(const char *name, qboolean create){
	int i;
	s_sample_t *sample;

	if(!name || !name[0]){
		Com_Warn("S_FindName: NULL or empty name.\n");
		return NULL;
	}

	if(strlen(name) >= MAX_QPATH){
		Com_Warn("S_FindName: name too long: %s.\n", name);
		return NULL;
	}

	// see if already loaded
	for(i = 0; i < s_env.num_samples; i++)
		if(!strcmp(s_env.samples[i].name, name))
			return &s_env.samples[i];

	if(!create)
		return NULL;

	// find a free sample
	for(i = 0; i < s_env.num_samples; i++)
		if(!s_env.samples[i].name[0])
			break;

	if(i == s_env.num_samples){
		if(s_env.num_samples == MAX_SOUNDS){
			Com_Warn("S_FindName: MAX_SOUNDS reached.\n");
			return NULL;
		}
		s_env.num_samples++;
	}

	sample = &s_env.samples[i];
	memset(sample, 0, sizeof(*sample));
	strcpy(sample->name, name);

	return sample;
}


/*
 * S_AliasName
 */
static s_sample_t *S_AliasName(const char *aliasname, const char *truename){
	s_sample_t *sample;
	char *s;
	int i;

	s = Z_Malloc(MAX_QPATH);
	strcpy(s, truename);

	// find a free sample
	for(i = 0; i < s_env.num_samples; i++)
		if(!s_env.samples[i].name[0])
			break;

	if(i == s_env.num_samples){
		if(s_env.num_samples == MAX_SOUNDS){
			Com_Warn("S_AliasName: MAX_SOUNDS reached.\n");
			return NULL;
		}
		s_env.num_samples++;
	}

	sample = &s_env.samples[i];
	memset(sample, 0, sizeof(*sample));
	strcpy(sample->name, aliasname);
	sample->truename = s;

	return sample;
}


/*
 * S_LoadSample
 */
s_sample_t *S_LoadSample(const char *name){
	s_sample_t *sample;

	if(!s_env.initialized)
		return NULL;

	sample = S_FindName(name, true);

	if(!sample)
		return NULL;

	S_LoadSamplecache(sample);
	return sample;
}


/*
 * S_FreeAll
 */
static void S_FreeAll(void){
	int i;

	for(i = 0; i < MAX_SOUNDS; i++){

		if(!s_env.samples[i].name[0])
			continue;

		if(s_env.samples[i].cache)
			Z_Free(s_env.samples[i].cache);

		s_env.samples[i].name[0] = 0;
	}

	s_env.num_samples = 0;
}


/*
 * S_LoadSamples
 */
void S_LoadSamples(void){
	int i;

	S_FreeAll();

	Cl_LoadEffectSamples();

	Cl_LoadTempEntitySamples();

	Cl_LoadProgress(95);

	for(i = 1; i < MAX_SOUNDS; i++){
		if(!cl.configstrings[CS_SOUNDS + i][0])
			break;
		cl.sound_precache[i] = S_LoadSample(cl.configstrings[CS_SOUNDS + i]);
	}

	Cl_LoadProgress(100);

	s_env.update = true;
}


/*
 * S_PickChannel
 */
static s_channel_t *S_PickChannel(int entnum, int entchannel){
	int ch_idx;
	int first_to_die;
	int life_left;
	s_channel_t *ch;

	if(entchannel < 0){
		Com_Warn("S_PickChannel: entchannel < 0.\n");
		return NULL;
	}

	// check for replacement sound, or find the best one to replace
	first_to_die = -1;
	life_left = 0x7fffffff;
	for(ch_idx = 0; ch_idx < MAX_CHANNELS; ch_idx++){
		if(entchannel != 0  // channel 0 never overrides
				&& s_env.channels[ch_idx].entnum == entnum
				&& s_env.channels[ch_idx].entchannel == entchannel){
			// always override sound from same entity
			first_to_die = ch_idx;
			break;
		}

		if(s_env.channels[ch_idx].end - s_env.paintedtime < life_left){
			life_left = s_env.channels[ch_idx].end - s_env.paintedtime;
			first_to_die = ch_idx;
		}
	}

	if(first_to_die == -1)
		return NULL;

	ch = &s_env.channels[first_to_die];
	memset(ch, 0, sizeof(*ch));

	return ch;
}


/*
 * S_SpatializeOrigin
 *
 * Used for spatializing channels and autosounds
 */
static void S_SpatializeOrigin(const vec3_t origin, float master_vol, float dist_mult, int *left_vol, int *right_vol){
	vec_t dot;
	vec_t dist;
	vec_t lscale, rscale, scale;
	vec3_t source_vec;

	if(cls.state != ca_active){
		*left_vol = *right_vol = 255;
		return;
	}

	// calculate stereo seperation and distance attenuation
	VectorSubtract(origin, r_view.origin, source_vec);

	dist = VectorNormalize(source_vec);
	dist -= SOUND_FULLVOLUME;
	if(dist < 0)
		dist = 0;  // close enough to be at full volume
	dist *= dist_mult;  // different attenuation levels

	dot = DotProduct(r_locals.right, source_vec);

	if(s_env.device.channels == 1 || !dist_mult){  // no attenuation = no spatialization
		rscale = 1.0;
		lscale = 1.0;
	} else {
		rscale = 0.5 * (1.0 + dot);
		lscale = 0.5 * (1.0 - dot);
	}

	// add in distance effect
	scale = (1.0 - dist) * rscale;
	*right_vol = (int)(master_vol * scale);
	if(*right_vol < 0)
		*right_vol = 0;

	scale = (1.0 - dist) * lscale;
	*left_vol = (int)(master_vol * scale);
	if(*left_vol < 0)
		*left_vol = 0;
}


/*
 * S_Spatialize
 */
static void S_Spatialize(s_channel_t *ch){
	vec3_t origin;

	// anything coming from the view entity will always be full volume
	if(ch->entnum == cl.playernum + 1){
		ch->leftvol = ch->master_vol;
		ch->rightvol = ch->master_vol;
		return;
	}

	if(ch->fixed_origin){
		VectorCopy(ch->origin, origin);
	} else
		VectorCopy(cl_entities[ch->entnum].current.origin, origin);

	S_SpatializeOrigin(origin, ch->master_vol, ch->dist_mult, &ch->leftvol, &ch->rightvol);
}


/*
 * S_AllocSampleplay
 */
static s_sampleplay_t *S_AllocSampleplay(void){
	s_sampleplay_t *ps;

	ps = s_env.freeplays.next;
	if(ps == &s_env.freeplays)
		return NULL;  // no free s_env.sampleplays

	// unlink from freelist
	ps->prev->next = ps->next;
	ps->next->prev = ps->prev;

	return ps;
}


/*
 * S_FreeSampleplay
 */
static void S_FreeSampleplay(s_sampleplay_t *ps){
	// unlink from channel
	ps->prev->next = ps->next;
	ps->next->prev = ps->prev;

	// add to free list
	ps->next = s_env.freeplays.next;
	s_env.freeplays.next->prev = ps;
	ps->prev = &s_env.freeplays;
	s_env.freeplays.next = ps;
}


/*
 * S_IssueSampleplay
 */
void S_IssueSampleplay(s_sampleplay_t *ps){
	s_channel_t *ch;
	s_samplecache_t *sc;

	if(s_show->value)
		Com_Printf("Issue %i\n", ps->begin);

	// pick a channel to play on
	ch = S_PickChannel(ps->entnum, ps->entchannel);
	if(!ch){
		S_FreeSampleplay(ps);
		return;
	}

	// spatialize
	if(ps->attenuation == ATTN_STATIC)
		ch->dist_mult = ps->attenuation * 0.001;
	else
		ch->dist_mult = ps->attenuation * 0.0005;

	ch->master_vol = ps->volume;
	ch->entnum = ps->entnum;
	ch->entchannel = ps->entchannel;
	ch->sample = ps->sample;
	VectorCopy(ps->origin, ch->origin);
	ch->fixed_origin = ps->fixed_origin;

	S_Spatialize(ch);

	ch->pos = 0;
	sc = S_LoadSamplecache(ch->sample);
	ch->end = s_env.paintedtime + sc->length;

	// free the sampleplay
	S_FreeSampleplay(ps);
}


/*
 * S_LoadModelSample
 */
static s_sample_t *S_LoadModelSample(entity_state_t *ent, const char *base){
	int n;
	char *p;
	s_sample_t *sample;
	char model[MAX_QPATH];
	char skin[MAX_QPATH];
	char modelFilename[MAX_QPATH];
	char filename[MAX_QPATH];
	FILE *f;

	if(!s_env.initialized)
		return NULL;

	// determine what model the client is using
	model[0] = skin[0] = 0;
	n = CS_PLAYERSKINS + ent->number - 1;
	if(cl.configstrings[n][0]){
		p = strchr(cl.configstrings[n], '\\');
		if(p){
			p += 1;
			strcpy(model, p);
			strcpy(skin, p);
			p = strchr(model, '/');
			if(p)
				*p = 0;
		}
	}

	// if we cant figure it out, use common
	if(!model[0] || !skin[0])
		strcpy(model, "common");

	// see if we already know of the model specific sound
	snprintf(modelFilename, sizeof(modelFilename), "#players/%s/%s", model, base + 1);
	sample = S_FindName(modelFilename, false);

	if(sample)  // found it
		return sample;

	// not yet loaded, so try model specific sound
	if(Fs_OpenFile(modelFilename + 1, &f, FILE_READ) > 0){
		Fs_CloseFile(f);
		return S_LoadSample(modelFilename);
	}

	// that didn't work, so use common, and alias it for future calls
	snprintf(filename, sizeof(filename), "#players/common/%s", base + 1);
	return S_AliasName(modelFilename, filename);
}


/*
 * S_StartSample
 *
 * Validates the params and queues the sound up if pos is NULL, the sound
 * will be dynamically sourced from the entity.  Entchannel 0 will never
 * override a playing sound
 */
void S_StartSample(const vec3_t org, int entnum, int entchannel, s_sample_t *sample,
		float fvol, float attenuation, float timeofs){

	s_samplecache_t *sc;
	int vol;
	s_sampleplay_t *ps, *sort;
	int start;

	if(!s_env.initialized)
		return;

	if(!sample)
		return;

	if(sample->name[0] == '*')
		sample = S_LoadModelSample(&cl_entities[entnum].current, sample->name);

	// make sure the sound is loaded
	sc = S_LoadSamplecache(sample);
	if(!sc)
		return;  // couldn't load the sound's data

	vol = fvol * 255;

	// make the s_sampleplay_t
	ps = S_AllocSampleplay();
	if(!ps)
		return;

	if(org){
		VectorCopy(org, ps->origin);
		ps->fixed_origin = true;
	} else
		ps->fixed_origin = false;

	ps->entnum = entnum;
	ps->entchannel = entchannel;
	ps->attenuation = attenuation;
	ps->volume = vol;
	ps->sample = sample;

	// drift s_env.beginofs
	start = cl.frame.servertime * 0.001 * s_env.device.rate + s_env.beginofs;
	if(start < s_env.paintedtime){
		start = s_env.paintedtime;
		s_env.beginofs = start -(cl.frame.servertime * 0.001 * s_env.device.rate);
	} else if(start > s_env.paintedtime + 0.3 * s_env.device.rate){
		start = s_env.paintedtime + 0.1 * s_env.device.rate;
		s_env.beginofs = start -(cl.frame.servertime * 0.001 * s_env.device.rate);
	} else {
		s_env.beginofs -= 10;
	}

	if(!timeofs)
		ps->begin = s_env.paintedtime;
	else
		ps->begin = start + timeofs * s_env.device.rate;

	// sort into the pending sound list
	for(sort = s_env.pendingplays.next;
			sort != &s_env.pendingplays && sort->begin < ps->begin;
			sort = sort->next){
		// simply find the insertion slot
	}

	ps->next = sort;
	ps->prev = sort->prev;

	ps->next->prev = ps;
	ps->prev->next = ps;
}


/*
 * S_StartLocalSample
 */
void S_StartLocalSample(const char *name){
	s_sample_t *sample;

	if(!s_env.initialized)
		return;

	sample = S_LoadSample(name);

	if(!sample){
		Com_Warn("S_StartLocalSample: Failed to load %s.\n", name);
		return;
	}

	S_StartSample(NULL, cl.playernum + 1, 0, sample, 1, 1, 0);
}


/*
 * S_AddLoopSample
 */
void S_AddLoopSample(const vec3_t org, s_sample_t *sample){
	s_loopsample_t *ls;
	int i, l, r;

	if(!sample || !sample->cache)
		return;

	ls = NULL;

	for(i = 0; i < s_env.num_loopsamples; i++){  // find existing loop sound
		if(s_env.loopsamples[i].sample == sample){
			ls = &s_env.loopsamples[i];
			break;
		}
	}

	if(!ls){  // or allocate a new one

		if(i == MAX_EDICTS){
			Com_Warn("S_AddLoopSample: MAX_EDICTS exceeded: %s\n", sample->name);
			return;
		}

		ls = &s_env.loopsamples[i];
		s_env.num_loopsamples++;
	}

	ls->sample = sample;

	S_SpatializeOrigin(org, 255.0, SOUND_LOOPATTENUATE, &l, &r);

	ls->left += l;
	ls->right += r;
}


/*
 * S_AddLoopSamples
 */
static void S_AddLoopSamples(void){
	int i, num;
	entity_state_t *ent;
	s_loopsample_t *ls;
	s_channel_t *ch;
	s_sample_t *sample;

	if(!s_ambient->value)
		return;

	for(i = 0; i < cl.frame.num_entities; i++){

		num = (cl.frame.parse_entities + i) & (MAX_PARSE_ENTITIES - 1);
		ent = &cl_parse_entities[num];

		if(!ent->sound)
			continue;

		sample = cl.sound_precache[ent->sound];
		S_AddLoopSample(ent->origin, sample);
	}

	for(i = 0; i < s_env.num_loopsamples; i++){

		ls = &s_env.loopsamples[i];

		if(!ls->left && !ls->right)
			continue;  // not audible

		// allocate a channel
		ch = S_PickChannel(0, 0);

		if(!ch)  // can't mix any more
			return;

		// clamp
		if(ls->left > 255)
			ls->left = 255;

		if(ls->right > 255)
			ls->right = 255;

		ch->leftvol = ls->left;
		ch->rightvol = ls->right;
		ch->sample = ls->sample;

		ch->autosound = true;  // remove next frame

		if(ls->sample->cache->length == 0){
			ch->pos = ch->end = 0;
		} else {
			ch->pos = s_env.paintedtime % ls->sample->cache->length;
			ch->end = s_env.paintedtime + ls->sample->cache->length - ch->pos;
		}
	}

	memset(s_env.loopsamples, 0, sizeof(s_env.loopsamples));
	s_env.num_loopsamples = 0;

	// add under water sound when swimming
	if(cl.underwater){

		ch = S_PickChannel(0, 0);

		if(!ch)
			return;

		ch->leftvol = ch->rightvol = 128;
		ch->autosound = true;

		ch->sample = S_LoadSample("world/under_water.wav");

		if(!ch->sample->cache)
			return;

		if(ch->sample->cache->length == 0){
			ch->pos = ch->end = 0;
		} else {
			ch->pos = s_env.paintedtime % ch->sample->cache->length;
			ch->end = s_env.paintedtime + ch->sample->cache->length - ch->pos;
		}
	}
}


/*
 * S_Stop
 */
static void S_Stop(void){
	int i;

	// reset s_env.sampleplays
	memset(s_env.sampleplays, 0, sizeof(s_env.sampleplays));

	s_env.freeplays.next = s_env.freeplays.prev = &s_env.freeplays;
	s_env.pendingplays.next = s_env.pendingplays.prev = &s_env.pendingplays;

	for(i = 0; i < MAX_SAMPLEPLAYS; i++){
		s_env.sampleplays[i].prev = &s_env.freeplays;
		s_env.sampleplays[i].next = s_env.freeplays.next;
		s_env.sampleplays[i].prev->next = &s_env.sampleplays[i];
		s_env.sampleplays[i].next->prev = &s_env.sampleplays[i];
	}

	// clear channels
	memset(s_env.channels, 0, sizeof(s_env.channels));

	if(s_env.device.buffer)  // clear any pending pcm
		memset(s_env.device.buffer, 0, s_env.device.samples * s_env.device.bits / 8);

	s_env.rawend = 0;
}


/*
 * S_GetSampleTime
 */
static void S_GetSampleTime(void){
	static int buffers;
	static int oldoffset;
	int fullsamples;

	fullsamples = s_env.device.samples / s_env.device.channels;

	if(s_env.device.offset < oldoffset){
		buffers++;  // buffer wrapped

		if(s_env.paintedtime > 0x40000000){  // time to chop things off to avoid 32 bit limits
			buffers = 0;
			s_env.paintedtime = fullsamples;
			S_Stop();
		}
	}
	oldoffset = s_env.device.offset;

	s_env.soundtime = buffers * fullsamples + s_env.device.offset / s_env.device.channels;
}


/*
 * S_Update
 */
static void S_Update_(void){
	unsigned endtime;
	int samps;

	S_GetSampleTime();

	// check to make sure that we haven't overshot
	if(s_env.paintedtime < s_env.soundtime){
		Com_Dprintf("S_Update_: Overflow\n");
		s_env.paintedtime = s_env.soundtime;
	}

	// mix ahead of current position
	endtime = s_env.soundtime + s_mixahead->value * s_env.device.rate;

	// mix to an even submission block size
	endtime = (endtime + s_env.device.chunk - 1) & ~(s_env.device.chunk - 1);

	samps = s_env.device.samples >> (s_env.device.channels - 1);

	if(endtime - s_env.soundtime > samps)
		endtime = s_env.soundtime + samps;

	S_PaintChannels(endtime);

	s_env.update = false;
}


/*
 * S_Update
 */
void S_Update(void){
	int i;
	int total;
	s_channel_t *ch;
	s_channel_t *combine;

	if(!s_env.initialized)
		return;

	if(cls.state != ca_active){
		S_Stop();
		return;
	}

	// rebuild scale tables if volume is modified
	if(s_volume->modified)
		S_InitScaletable();

	combine = NULL;

	// update spatialization for dynamic sounds
	ch = s_env.channels;
	for(i = 0; i < MAX_CHANNELS; i++, ch++){

		if(!ch->sample)
			continue;

		if(ch->autosound){  // autosounds are regenerated fresh each frame
			memset(ch, 0, sizeof(*ch));
			continue;
		}

		S_Spatialize(ch);  // respatialize channel

		if(!ch->leftvol && !ch->rightvol){
			memset(ch, 0, sizeof(*ch));
			continue;
		}
	}

	// add loopsounds
	S_AddLoopSamples();

	// debugging output
	if(s_show->value){
		total = 0;
		ch = s_env.channels;
		for(i = 0; i < MAX_CHANNELS; i++, ch++)
			if(ch->sample && (ch->leftvol || ch->rightvol)){
				Com_Printf("%3i %3i %s\n", ch->leftvol, ch->rightvol, ch->sample->name);
				total++;
			}

		Com_Printf("----(%i)---- painted: %i\n", total, s_env.paintedtime);
	}

	// mix some sound
	S_Update_();
}


/*
 * S_Info_f
 */
static void S_Info_f(void){

	if(!s_env.initialized){
		Com_Printf("Sample system not started.\n");
		return;
	}

	Com_Printf("%5d channels\n", s_env.device.channels);
	Com_Printf("%5d samples\n", s_env.device.samples);
	Com_Printf("%5d offset\n", s_env.device.offset);
	Com_Printf("%5d bits\n", s_env.device.bits);
	Com_Printf("%5d chunk\n", s_env.device.chunk);
	Com_Printf("%5d speed\n", s_env.device.rate);
	Com_Printf("0x%lx s_env.device buffer\n", (unsigned long)s_env.device.buffer);
}


/*
 * S_Play_f
 */
static void S_Play_f(void){
	int i;
	char name[MAX_QPATH];

	i = 1;
	while(i < Cmd_Argc()){
		if(!strrchr(Cmd_Argv(i), '.')){
			strcpy(name, Cmd_Argv(i));
			strcat(name, ".wav");
		} else
			strcpy(name, Cmd_Argv(i));
		S_StartSample(NULL, cl.playernum + 1, 0, S_LoadSample(name), 1.0, 1.0, 0);
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
	int i;
	s_sample_t *sample;
	s_samplecache_t *sc;
	int size, total;

	total = 0;
	for(sample = s_env.samples, i = 0; i < s_env.num_samples; i++, sample++){
		if(!sample->name[0])
			continue;

		sc = sample->cache;
		if(sc){
			size = sc->length * sc->width * (sc->stereo + 1);
			total += size;
			if(sc->loopstart >= 0)
				Com_Printf("L");
			else
				Com_Printf(" ");
			Com_Printf(" (%2db) %6i : %s\n", sc->width * 8, size, sample->name);
		} else {
			if(sample->name[0] == '*')
				Com_Printf("  placeholder : %s\n", sample->name);
			else
				Com_Printf("  not loaded  : %s\n", sample->name);
		}
	}
	Com_Printf("Total resident: %i\n", total);
}


/*
 * S_Reload_f
 */
static void S_Reload_f(void){
	S_LoadSamples();
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

	memset(&s_env, 0, sizeof(s_env));

	if(Cvar_VariableValue("s_disable")){
		Com_Warn("Sound disabled.\n");
		return;
	}

	Com_Printf("Sound initialization..\n");

	s_volume = Cvar_Get("s_volume", "0.7", CVAR_ARCHIVE, NULL);
	s_mixahead = Cvar_Get("s_mixahead", "0.05", CVAR_ARCHIVE, NULL);
	s_rate = Cvar_Get("s_rate", "", CVAR_ARCHIVE | CVAR_S_DEVICE, NULL);
	s_channels = Cvar_Get("s_channels", "2", CVAR_ARCHIVE | CVAR_S_DEVICE, "Set the desired sound channels");

	s_ambient = Cvar_Get("s_ambient", "1", CVAR_ARCHIVE, "Activate or deactivate ambient loop sounds");

	s_testsound = Cvar_Get("s_testsound", "0", 0, "Plays some test sine wave sound");
	s_show = Cvar_Get("s_show", "0", 0, NULL);

	Cmd_AddCommand("s_restart", S_Restart_f, "Restart the sound subsystem");
	Cmd_AddCommand("s_reload", S_Reload_f, "Reload all loaded sounds");
	Cmd_AddCommand("s_play", S_Play_f, NULL);
	Cmd_AddCommand("s_stop", S_Stop_f, NULL);
	Cmd_AddCommand("s_list", S_List_f, NULL);
	Cmd_AddCommand("s_info", S_Info_f, NULL);

	S_InitScaletable();

	s_env.num_samples = 0;
	s_env.soundtime = 0;
	s_env.paintedtime = 0;

	S_Stop_f();

	s_env.initialized = S_InitDevice();

	Com_Printf("Sound initialized %dbit %dKHz %dch.\n", s_env.device.bits,
			(s_env.device.rate / 1000), s_env.device.channels);
}


/*
 * S_Shutdown
 */
void S_Shutdown(void){

	S_ShutdownDevice();

	Cmd_RemoveCommand("s_play");
	Cmd_RemoveCommand("s_stop");
	Cmd_RemoveCommand("s_list");
	Cmd_RemoveCommand("s_info");
	Cmd_RemoveCommand("s_restart");
	Cmd_RemoveCommand("s_reload");

	S_FreeAll();
}

