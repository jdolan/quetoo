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

extern cl_client_t cl;

static const char *SAMPLE_TYPES[] = { ".ogg", ".wav", NULL };

/*
 * @brief
 */
static void S_LoadSampleChunk(s_sample_t *sample) {
	char path[MAX_QPATH];
	void *buf;
	int32_t i, len;
	SDL_RWops *rw;

	if (sample->media.name[0] == '*') // place holder
		return;

	if (sample->media.name[0] == '#') { // global path
		g_strlcpy(path, (sample->media.name + 1), sizeof(path));
	} else { // or relative
		g_snprintf(path, sizeof(path), "sounds/%s", sample->media.name);
	}

	buf = NULL;
	rw = NULL;

	i = 0;
	while (SAMPLE_TYPES[i]) {

		StripExtension(path, path);
		strcat(path, SAMPLE_TYPES[i++]);

		if ((len = Fs_LoadFile(path, &buf)) == -1)
			continue;

		if (!(rw = SDL_RWFromMem(buf, len))) {
			Fs_FreeFile(buf);
			continue;
		}

		if (!(sample->chunk = Mix_LoadWAV_RW(rw, false)))
			Com_Warn("S_LoadSoundChunk: %s.\n", Mix_GetError());

		Fs_FreeFile(buf);

		SDL_FreeRW(rw);

		if (sample->chunk) // success
			break;
	}

	if (sample->chunk)
		Mix_VolumeChunk(sample->chunk, s_volume->value * MIX_MAX_VOLUME);
	else
		Com_Warn("S_LoadSoundChunk: Failed to load %s.\n", sample->media.name);
}

/*
 * @brief Free event listener for s_sample_t.
 */
static void S_FreeSample(s_media_t *self) {
	s_sample_t *sample = (s_sample_t *) self;

	if (sample->chunk) {
		Mix_FreeChunk(sample->chunk);
	}
}

/*
 * @brief
 */
s_sample_t *S_LoadSample(const char *name) {
	char key[MAX_QPATH];
	s_sample_t *sample;

	if (!s_env.initialized)
		return NULL;

	if (!name || !name[0]) {
		Com_Error(ERR_DROP, "S_LoadSample: NULL name.\n");
	}

	StripExtension(name, key);

	if (!(sample = (s_sample_t *) S_FindMedia(key))) {
		sample = (s_sample_t *) S_MallocMedia(key, sizeof(s_sample_t));

		sample->media.Free = S_FreeSample;

		S_LoadSampleChunk(sample);

		S_RegisterMedia((s_media_t *) sample);
	}

	return sample;
}

/*
 * @brief
 */
static s_sample_t *S_AliasSample(s_sample_t *sample, const char *alias) {

	s_sample_t *s = (s_sample_t *) S_MallocMedia(alias, sizeof(s_sample_t));

	s->chunk = sample->chunk;
	s->alias = true;

	return s;
}

/*
 * @brief
 */
s_sample_t *S_LoadModelSample(entity_state_t *ent, const char *name) {
	int32_t n;
	char *p;
	s_sample_t *sample;
	char model[MAX_QPATH];
	char alias[MAX_QPATH];
	char path[MAX_QPATH];
	FILE *f;

	if (!s_env.initialized)
		return NULL;

	// determine what model the client is using
	memset(model, 0, sizeof(model));

	if (ent->number - 1 >= MAX_CLIENTS) {
		Com_Warn("S_LoadModelSample: Invalid client entity: %d\n", ent->number - 1);
		return NULL;
	}

	n = CS_CLIENTS + ent->number - 1;
	if (cl.config_strings[n][0]) {
		p = strchr(cl.config_strings[n], '\\');
		if (p) {
			p += 1;
			strcpy(model, p);
			p = strchr(model, '/');
			if (p)
				*p = 0;
		}
	}

	// if we can't figure it out, use common
	if (*model == '\0')
		strcpy(model, "common");

	// see if we already know of the model specific sound
	g_snprintf(alias, sizeof(alias), "#players/%s/%s", model, name + 1);
	sample = (s_sample_t *) S_FindMedia(alias);

	if (sample) // we do, use it
		return sample;

	// we don't, try it
	if (Fs_OpenFile(alias + 1, &f, FILE_READ) > 0) {
		Fs_CloseFile(f);
		return S_LoadSample(alias);
	}

	// that didn't work, so load the common one and alias it
	g_snprintf(path, sizeof(path), "#players/common/%s", name + 1);
	sample = S_LoadSample(path);

	if (sample)
		return S_AliasSample(sample, alias);

	Com_Warn("S_LoadModelSample: Failed to load %s.\n", alias);
	return NULL;
}

