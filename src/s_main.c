/*
 * Copyright(c) 1997-2001 Id Software, Inc.
 * Copyright(c) 2002 The Quakeforge Project.
 * Copyright(c) 2006 Quake2World.
 * *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or(at your option) any later version.
 * *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * *
 * See the GNU General Public License for more details.
 * *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */

#include "client.h"

// only begin attenuating sound volumes when outside the FULLVOLUME range
#define SOUND_FULLVOLUME 50
#define SOUND_LOOPATTENUATE 0.003

channel_t channels[MAX_CHANNELS];

static qboolean s_initialized;

dma_t dma;

int s_soundtime;  // sample pairs
int s_paintedtime;   // sample pairs

sfx_t known_sfx[MAX_SOUNDS];
int num_sfx;

#define MAX_PLAYSOUNDS 128
static playsound_t s_playsounds[MAX_PLAYSOUNDS];
static playsound_t s_freeplays;
playsound_t s_pendingplays;

cvar_t *s_channels;
cvar_t *s_mixahead;
cvar_t *s_rate;
cvar_t *s_testsound;
cvar_t *s_volume;

static cvar_t *s_ambient;
static cvar_t *s_show;

static int s_beginofs;
int s_rawend;


/*
 * S_FindName
 */
static sfx_t *S_FindName(const char *name, qboolean create){
	int i;
	sfx_t *sfx;

	if(!name || !name[0]){
		Com_Warn("S_FindName: NULL or empty name.\n");
		return NULL;
	}

	if(strlen(name) >= MAX_QPATH){
		Com_Warn("S_FindName: name too long: %s.\n", name);
		return NULL;
	}

	// see if already loaded
	for(i = 0; i < num_sfx; i++)
		if(!strcmp(known_sfx[i].name, name))
			return &known_sfx[i];

	if(!create)
		return NULL;

	// find a free sfx
	for(i = 0; i < num_sfx; i++)
		if(!known_sfx[i].name[0])
			break;

	if(i == num_sfx){
		if(num_sfx == MAX_SOUNDS){
			Com_Warn("S_FindName: MAX_SOUNDS reached.\n");
			return NULL;
		}
		num_sfx++;
	}

	sfx = &known_sfx[i];
	memset(sfx, 0, sizeof(*sfx));
	strcpy(sfx->name, name);

	return sfx;
}


/*
 * S_AliasName
 */
static sfx_t *S_AliasName(const char *aliasname, const char *truename){
	sfx_t *sfx;
	char *s;
	int i;

	s = Z_Malloc(MAX_QPATH);
	strcpy(s, truename);

	// find a free sfx
	for(i = 0; i < num_sfx; i++)
		if(!known_sfx[i].name[0])
			break;

	if(i == num_sfx){
		if(num_sfx == MAX_SOUNDS){
			Com_Warn("S_AliasName: MAX_SOUNDS reached.\n");
			return NULL;
		}
		num_sfx++;
	}

	sfx = &known_sfx[i];
	memset(sfx, 0, sizeof(*sfx));
	strcpy(sfx->name, aliasname);
	sfx->truename = s;

	return sfx;
}


/*
 * S_LoadSound
 */
sfx_t *S_LoadSound(const char *name){
	sfx_t *sfx;

	if(!s_initialized)
		return NULL;

	sfx = S_FindName(name, true);

	if(!sfx)
		return NULL;

	S_LoadSfx(sfx);
	return sfx;
}


/*
 * S_FreeAll
 */
static void S_FreeAll(void){
	int i;

	for(i = 0; i < MAX_SOUNDS; i++){
		if(!known_sfx[i].name[0])
			continue;
		if(known_sfx[i].cache)
			Z_Free(known_sfx[i].cache);
		known_sfx[i].name[0] = 0;
	}

	num_sfx = 0;
}


/*
 * S_LoadSounds
 */
void S_LoadSounds(void){
	int i;

	S_FreeAll();

	Cl_LoadEffectSounds();

	Cl_LoadTempEntitySounds();

	Cl_LoadProgress(95);

	for(i = 1; i < MAX_SOUNDS; i++){
		if(!cl.configstrings[CS_SOUNDS + i][0])
			break;
		cl.sound_precache[i] = S_LoadSound(cl.configstrings[CS_SOUNDS + i]);
	}

	Cl_LoadProgress(100);
}


/*
 * S_PickChannel
 */
static channel_t *S_PickChannel(int entnum, int entchannel){
	int ch_idx;
	int first_to_die;
	int life_left;
	channel_t *ch;

	if(entchannel < 0){
		Com_Warn("S_PickChannel: entchannel < 0.\n");
		return NULL;
	}

	// Check for replacement sound, or find the best one to replace
	first_to_die = -1;
	life_left = 0x7fffffff;
	for(ch_idx = 0; ch_idx < MAX_CHANNELS; ch_idx++){
		if(entchannel != 0  // channel 0 never overrides
				&& channels[ch_idx].entnum == entnum
				&& channels[ch_idx].entchannel == entchannel){  // always override sound from same entity
			first_to_die = ch_idx;
			break;
		}

		if(channels[ch_idx].end - s_paintedtime < life_left){
			life_left = channels[ch_idx].end - s_paintedtime;
			first_to_die = ch_idx;
		}
	}

	if(first_to_die == -1)
		return NULL;

	ch = &channels[first_to_die];
	memset(ch, 0, sizeof(*ch));

	return ch;
}


/*
 * S_SpatializeOrigin
 * 
 * Used for spatializing channels and autosounds
 */
static void S_SpatializeOrigin(vec3_t origin, float master_vol, float dist_mult, int *left_vol, int *right_vol){
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

	if(dma.channels == 1 || !dist_mult){  // no attenuation = no spatialization
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
static void S_Spatialize(channel_t *ch){
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
 * S_AllocPlaysound
 */
static playsound_t *S_AllocPlaysound(void){
	playsound_t *ps;

	ps = s_freeplays.next;
	if(ps == &s_freeplays)
		return NULL;  // no free playsounds

	// unlink from freelist
	ps->prev->next = ps->next;
	ps->next->prev = ps->prev;

	return ps;
}


/*
 * S_FreePlaysound
 */
static void S_FreePlaysound(playsound_t *ps){
	// unlink from channel
	ps->prev->next = ps->next;
	ps->next->prev = ps->prev;

	// add to free list
	ps->next = s_freeplays.next;
	s_freeplays.next->prev = ps;
	ps->prev = &s_freeplays;
	s_freeplays.next = ps;
}


/*
 * S_IssuePlaysound
 */
void S_IssuePlaysound(playsound_t *ps){
	channel_t *ch;
	sfxcache_t *sc;

	if(s_show->value)
		Com_Printf("Issue %i\n", ps->begin);

	// pick a channel to play on
	ch = S_PickChannel(ps->entnum, ps->entchannel);
	if(!ch){
		S_FreePlaysound(ps);
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
	ch->sfx = ps->sfx;
	VectorCopy(ps->origin, ch->origin);
	ch->fixed_origin = ps->fixed_origin;

	S_Spatialize(ch);

	ch->pos = 0;
	sc = S_LoadSfx(ch->sfx);
	ch->end = s_paintedtime + sc->length;

	// free the playsound
	S_FreePlaysound(ps);
}


/*
 * S_LoadModelSound
 */
static sfx_t *S_LoadModelSound(entity_state_t *ent, const char *base){
	int n;
	char *p;
	sfx_t *sfx;
	char model[MAX_QPATH];
	char skin[MAX_QPATH];
	char modelFilename[MAX_QPATH];
	char filename[MAX_QPATH];
	FILE *f;

	if(!s_initialized)
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
	sfx = S_FindName(modelFilename, false);

	if(sfx)  // found it
		return sfx;

	// not yet loaded, so try model specific sound
	if(Fs_OpenFile(modelFilename + 1, &f, FILE_READ) > 0){
		Fs_CloseFile(f);
		return S_LoadSound(modelFilename);
	}

	// that didn't work, so use common, and alias it for future calls
	snprintf(filename, sizeof(filename), "#players/common/%s", base + 1);
	return S_AliasName(modelFilename, filename);
}


/*
 * S_StartSound
 * 
 * Validates the parms and ques the sound up if pos is NULL, the sound
 * will be dynamically sourced from the entity.  Entchannel 0 will never
 * override a playing sound
 */
void S_StartSound(vec3_t origin, int entnum, int entchannel, sfx_t *sfx,
		float fvol, float attenuation, float timeofs){

	sfxcache_t *sc;
	int vol;
	playsound_t *ps, *sort;
	int start;

	if(!s_initialized)
		return;

	if(!sfx)
		return;

	if(sfx->name[0] == '*')
		sfx = S_LoadModelSound(&cl_entities[entnum].current, sfx->name);

	// make sure the sound is loaded
	sc = S_LoadSfx(sfx);
	if(!sc)
		return;  // couldn't load the sound's data

	vol = fvol * 255;

	// make the playsound_t
	ps = S_AllocPlaysound();
	if(!ps)
		return;

	if(origin){
		VectorCopy(origin, ps->origin);
		ps->fixed_origin = true;
	} else
		ps->fixed_origin = false;

	ps->entnum = entnum;
	ps->entchannel = entchannel;
	ps->attenuation = attenuation;
	ps->volume = vol;
	ps->sfx = sfx;

	// drift s_beginofs
	start = cl.frame.servertime * 0.001 * dma.rate + s_beginofs;
	if(start < s_paintedtime){
		start = s_paintedtime;
		s_beginofs = start -(cl.frame.servertime * 0.001 * dma.rate);
	} else if(start > s_paintedtime + 0.3 * dma.rate){
		start = s_paintedtime + 0.1 * dma.rate;
		s_beginofs = start -(cl.frame.servertime * 0.001 * dma.rate);
	} else {
		s_beginofs -= 10;
	}

	if(!timeofs)
		ps->begin = s_paintedtime;
	else
		ps->begin = start + timeofs * dma.rate;

	// sort into the pending sound list
	for(sort = s_pendingplays.next;
			sort != &s_pendingplays && sort->begin < ps->begin;
			sort = sort->next)
		;

	ps->next = sort;
	ps->prev = sort->prev;

	ps->next->prev = ps;
	ps->prev->next = ps;
}


/*
 * S_StartLocalSound
 */
void S_StartLocalSound(char *sound){
	sfx_t *sfx;

	if(!s_initialized)
		return;

	sfx = S_LoadSound(sound);
	if(!sfx){
		Com_Warn("S_StartLocalSound: Failed to load %s.\n", sound);
		return;
	}

	S_StartSound(NULL, cl.playernum + 1, 0, sfx, 1, 1, 0);
}


/*
 * Entities with a sound associated to them will generate looped ambient sounds
 * that are automatically spatialized and mixed to a channel.
 */

typedef struct loop_sound_s {
	sfx_t *sfx;
	int left, right;
} loop_sound_t;

static loop_sound_t loop_sounds[MAX_EDICTS];
int num_loop_sounds = 0;


/*
 * S_AddLoopSound
 */
void S_AddLoopSound(vec3_t org, sfx_t *sfx){
	loop_sound_t *ls;
	int i, l, r;

	if(!sfx || !sfx->cache)
		return;

	ls = NULL;

	for(i = 0; i < num_loop_sounds; i++){  // find existing loop sound
		if(loop_sounds[i].sfx == sfx){
			ls = &loop_sounds[i];
			break;
		}
	}

	if(!ls){  // or allocate a new one

		if(i == MAX_EDICTS){
			Com_Warn("S_AddLoopSound: Max loop sounds exceeded: %s\n", sfx->name);
			return;
		}

		ls = &loop_sounds[i];
		num_loop_sounds++;
	}

	ls->sfx = sfx;

	S_SpatializeOrigin(org, 255.0, SOUND_LOOPATTENUATE, &l, &r);

	ls->left += l;
	ls->right += r;
}


/*
 * S_AddLoopSounds
 */
static void S_AddLoopSounds(void){
	int i, num;
	entity_state_t *ent;
	loop_sound_t *ls;
	channel_t *ch;
	sfx_t *sfx;

	if(!s_ambient->value)
		return;

	for(i = 0; i < cl.frame.num_entities; i++){

		num = (cl.frame.parse_entities + i) & (MAX_PARSE_ENTITIES - 1);
		ent = &cl_parse_entities[num];

		if(!ent->sound)
			continue;

		sfx = cl.sound_precache[ent->sound];
		S_AddLoopSound(ent->origin, sfx);
	}

	for(i = 0; i < num_loop_sounds; i++){

		ls = &loop_sounds[i];

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
		ch->sfx = ls->sfx;

		ch->autosound = true;  // remove next frame

		if(ls->sfx->cache->length == 0){
			ch->pos = ch->end = 0;
		} else {
			ch->pos = s_paintedtime % ls->sfx->cache->length;
			ch->end = s_paintedtime + ls->sfx->cache->length - ch->pos;
		}
	}

	memset(loop_sounds, 0, sizeof(loop_sounds));
	num_loop_sounds = 0;

	// add under water sound when swimming
	if(cl.underwater){
		ch = S_PickChannel(0, 0);
		if(!ch)
			return;
		ch->leftvol = ch->rightvol = 128;
		ch->autosound = true;
		ch->sfx = S_LoadSound("world/under_water.wav");
		if(!ch->sfx->cache)
			return;

		if(ch->sfx->cache->length == 0){
			ch->pos = ch->end = 0;
		} else {
			ch->pos = s_paintedtime % ch->sfx->cache->length;
			ch->end = s_paintedtime + ch->sfx->cache->length - ch->pos;
		}
	}
}


/*
 * S_Stop
 */
static void S_Stop(void){
	int i;

	// reset playsounds
	memset(s_playsounds, 0, sizeof(s_playsounds));

	s_freeplays.next = s_freeplays.prev = &s_freeplays;
	s_pendingplays.next = s_pendingplays.prev = &s_pendingplays;

	for(i = 0; i < MAX_PLAYSOUNDS; i++){
		s_playsounds[i].prev = &s_freeplays;
		s_playsounds[i].next = s_freeplays.next;
		s_playsounds[i].prev->next = &s_playsounds[i];
		s_playsounds[i].next->prev = &s_playsounds[i];
	}

	// clear channels
	memset(channels, 0, sizeof(channels));

	if(dma.buffer)  // clear any pending pcm
		memset(dma.buffer, 0, dma.samples * dma.bits / 8);

	s_rawend = 0;
}


/*
 * S_GetSoundTime
 */
static void S_GetSoundTime(void){
	static int buffers;
	static int oldoffset;
	int fullsamples;

	fullsamples = dma.samples / dma.channels;

	if(dma.offset < oldoffset){
		buffers++;  // buffer wrapped

		if(s_paintedtime > 0x40000000){  // time to chop things off to avoid 32 bit limits
			buffers = 0;
			s_paintedtime = fullsamples;
			S_Stop();
		}
	}
	oldoffset = dma.offset;

	s_soundtime = buffers * fullsamples + dma.offset / dma.channels;
}


/*
 * S_Update
 */
static void S_Update_(void){
	unsigned endtime;
	int samps;

	S_GetSoundTime();

	// check to make sure that we haven't overshot
	if(s_paintedtime < s_soundtime){
		Com_Dprintf("S_Update_ : overflow\n");
		s_paintedtime = s_soundtime;
	}

	// mix ahead of current position
	endtime = s_soundtime + s_mixahead->value * dma.rate;

	// mix to an even submission block size
	endtime = (endtime + dma.chunk - 1)
			  & ~(dma.chunk - 1);
	samps = dma.samples >> (dma.channels - 1);
	if(endtime - s_soundtime > samps)
		endtime = s_soundtime + samps;

	S_PaintChannels(endtime);
}


/*
 * S_Update
 */
void S_Update(void){
	int i;
	int total;
	channel_t *ch;
	channel_t *combine;

	if(!s_initialized)
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
	ch = channels;
	for(i = 0; i < MAX_CHANNELS; i++, ch++){
		if(!ch->sfx)
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
	S_AddLoopSounds();

	// debugging output
	if(s_show->value){
		total = 0;
		ch = channels;
		for(i = 0; i < MAX_CHANNELS; i++, ch++)
			if(ch->sfx &&(ch->leftvol || ch->rightvol)){
				Com_Printf("%3i %3i %s\n", ch->leftvol, ch->rightvol, ch->sfx->name);
				total++;
			}

		Com_Printf("----(%i)---- painted: %i\n", total, s_paintedtime);
	}

	// mix some sound
	S_Update_();
}


/*
 * S_Info_f
 */
static void S_Info_f(void){

	if(!s_initialized){
		Com_Printf("Sound system not started.\n");
		return;
	}

	Com_Printf("%5d channels\n", dma.channels);
	Com_Printf("%5d samples\n", dma.samples);
	Com_Printf("%5d offset\n", dma.offset);
	Com_Printf("%5d bits\n", dma.bits);
	Com_Printf("%5d chunk\n", dma.chunk);
	Com_Printf("%5d speed\n", dma.rate);
	Com_Printf("0x%lx dma buffer\n", (unsigned long)dma.buffer);
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
		S_StartSound(NULL, cl.playernum + 1, 0, S_LoadSound(name), 1.0, 1.0, 0);
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
	sfx_t *sfx;
	sfxcache_t *sc;
	int size, total;

	total = 0;
	for(sfx = known_sfx, i = 0; i < num_sfx; i++, sfx++){
		if(!sfx->name[0])
			continue;

		sc = sfx->cache;
		if(sc){
			size = sc->length * sc->width * (sc->stereo + 1);
			total += size;
			if(sc->loopstart >= 0)
				Com_Printf("L");
			else
				Com_Printf(" ");
			Com_Printf(" (%2db) %6i : %s\n", sc->width * 8, size, sfx->name);
		} else {
			if(sfx->name[0] == '*')
				Com_Printf("  placeholder : %s\n", sfx->name);
			else
				Com_Printf("  not loaded  : %s\n", sfx->name);
		}
	}
	Com_Printf("Total resident: %i\n", total);
}


/*
 * S_Reload_f
 */
static void S_Reload_f(void){
	S_LoadSounds();
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

	if(Cvar_VariableValue("s_disable")){
		Com_Printf("Sound disabled.\n");
		return;
	}

	Com_Printf("Sound initialization..\n");

	s_volume = Cvar_Get("s_volume", "0.7", CVAR_ARCHIVE, NULL);
	s_mixahead = Cvar_Get("s_mixahead", "0.05", CVAR_ARCHIVE, NULL);
	s_rate = Cvar_Get("s_rate", "", CVAR_ARCHIVE, NULL);
	s_channels = Cvar_Get("s_channels", "2", CVAR_ARCHIVE, "Set the desired sound channels");

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

	num_sfx = 0;
	s_soundtime = 0;
	s_paintedtime = 0;

	S_Stop_f();

	s_initialized = S_InitDevice();

	Com_Printf("Sound initialized %dbit %dKHz %dch.\n", dma.bits,
			(dma.rate / 1000), dma.channels);
}


/*
 * S_Shutdown
 */
void S_Shutdown(void){

	if(!s_initialized)
		return;

	S_ShutdownDevice();

	s_initialized = false;

	Cmd_RemoveCommand("s_play");
	Cmd_RemoveCommand("s_stop");
	Cmd_RemoveCommand("s_list");
	Cmd_RemoveCommand("s_info");
	Cmd_RemoveCommand("s_restart");
	Cmd_RemoveCommand("s_reload");

	S_FreeAll();
}

