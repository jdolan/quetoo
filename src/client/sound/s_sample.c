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

extern cl_client_t cl;

static const char *SAMPLE_TYPES[] = { ".ogg", ".wav", NULL };
static const char *SOUND_PATHS[] = { "sounds/", "sound/", NULL };

/**
 * @brief Resample audio. outdata will be realloc'd to the size required to handle this operation,
 * so be sure to initialize to NULL before calling if it's first time!
 */
size_t S_Resample(const int32_t channels, const int32_t source_rate, const int32_t dest_rate, const size_t num_frames, const int16_t *in_frames, int16_t **out_frames, size_t *out_size) {
	
	const float stepscale = (float) source_rate / (float) dest_rate;
	const size_t outcount = NearestMultiple((size_t) (num_frames / stepscale), channels);
	const size_t size = outcount * sizeof(int16_t);

	if (out_size && *out_size < size) {
		*out_frames = Mem_Realloc(*out_frames, size);
		*out_size = size;
	}

	int32_t samplefrac = 0;
	const float fracstep = stepscale * 256.0;

	for (size_t i = 0; i < outcount; ) {
		for (int32_t c = 0; c < channels; c++, i++) {
			const int32_t srcsample = NearestMultiple(samplefrac >> 8, channels) + c;

			samplefrac += fracstep;
			(*out_frames)[i] = LittleShort(in_frames[srcsample]);
		}
	}

	return outcount;
}

/**
 * @brief
 */
void S_ConvertSamples(const float *input_samples, const sf_count_t num_samples, int16_t **out_samples, size_t *out_size) {
	const size_t size = sizeof(int16_t) * num_samples;

	if (out_size && *out_size < size) {
		*out_samples = Mem_Realloc(*out_samples, size);
		*out_size = size;
	}

	for (sf_count_t i = 0; i < num_samples; i++) {
		(*out_samples)[i] = (int16_t) Clampf(input_samples[i] * 32768.0, INT16_MIN, INT16_MAX);
	}
}

/**
 * @brief
 */
static _Bool S_LoadSampleChunkFromPath(s_sample_t *sample, char *path, const size_t pathlen) {

	void *buf;
	int32_t i;

	buf = NULL;

	i = 0;
	while (SAMPLE_TYPES[i]) {

		StripExtension(path, path);
		g_strlcat(path, SAMPLE_TYPES[i], pathlen);

		i++;

		int64_t len;
		if ((len = Fs_Load(path, &buf)) == -1) {
			continue;
		}

		SDL_RWops *rw = SDL_RWFromConstMem(buf, (int32_t) len);

		SF_INFO info;
		memset(&info, 0, sizeof(info));

		SNDFILE *snd = sf_open_virtual(&s_rwops_io, SFM_READ, &info, rw);

		if (!snd || sf_error(snd)) {
			Com_Warn("%s\n", sf_strerror(snd));
		} else {
			const size_t raw_size = sizeof(float) * info.frames * info.channels;

			if (s_env.raw_sample_buffer_size < raw_size) {
				s_env.raw_sample_buffer = Mem_Realloc(s_env.raw_sample_buffer, raw_size);
				s_env.raw_sample_buffer_size = raw_size;
			}

			sf_count_t count = sf_readf_float(snd, s_env.raw_sample_buffer, info.frames) * info.channels;

			S_ConvertSamples(s_env.raw_sample_buffer, count, &s_env.converted_sample_buffer, &s_env.converted_sample_buffer_size);

			const int16_t *buffer = s_env.converted_sample_buffer;

			if (info.samplerate != s_rate->integer) {
				count = S_Resample(info.channels, info.samplerate, s_rate->integer, count, buffer, &s_env.resample_buffer, &s_env.resample_buffer_size);
				buffer = s_env.resample_buffer;
			}

			sample->stereo = info.channels != 1;
			sample->num_samples = count;

			alGenBuffers(1, &sample->buffer);
			S_CheckALError();

			const ALenum format = info.channels == 1 ? AL_FORMAT_MONO16 : AL_FORMAT_STEREO16;
			const ALsizei size = (ALsizei) count * sizeof(int16_t);

			alBufferData(sample->buffer, format, buffer, size, s_rate->integer);
			S_CheckALError();
		}

		sf_close(snd);

		SDL_RWclose(rw);

		Fs_Free(buf);

		if (sample->buffer) { // success
			break;
		}
	}

	return !!sample->buffer;
}

/**
 * @brief
 */
static void S_LoadSampleChunk(s_sample_t *sample) {
	char path[MAX_QPATH];
	_Bool found = false;

	if (sample->media.name[0] == '*') { // place holder
		return;
	}

	if (sample->media.name[0] == '#') { // global path

		g_strlcpy(path, (sample->media.name + 1), sizeof(path));
		found = S_LoadSampleChunkFromPath(sample, path, sizeof(path));
	} else { // or relative
		int32_t i = 0;

		while (SOUND_PATHS[i]) {

			g_snprintf(path, sizeof(path), "%s%s", SOUND_PATHS[i], sample->media.name);

			if ((found = S_LoadSampleChunkFromPath(sample, path, sizeof(path)))) {
				break;
			}

			++i;
		}
	}

	if (found) {
		Com_Debug(DEBUG_SOUND, "Loaded %s\n", path);
	} else {
		if (g_str_has_prefix(sample->media.name, "#players")) {
			Com_Debug(DEBUG_SOUND, "Failed to load player sample %s\n", sample->media.name);
		} else {
			Com_Warn("Failed to load %s\n", sample->media.name);
		}
	}
}

/**
 * @brief Free event listener for s_sample_t.
 */
static void S_FreeSample(s_media_t *self) {
	s_sample_t *sample = (s_sample_t *) self;

	if (sample->buffer) {
		alDeleteBuffers(1, &sample->buffer);
		sample->buffer = 0;
		S_CheckALError();
	}
}

/**
 * @brief
 */
s_sample_t *S_LoadSample(const char *name) {
	char key[MAX_QPATH];
	s_sample_t *sample;

	if (!s_env.context) {
		return NULL;
	}

	if (!name || !name[0]) {
		Com_Error(ERROR_DROP, "NULL name\n");
	}

	StripExtension(name, key);

	if (!(sample = (s_sample_t *) S_FindMedia(key))) {
		sample = (s_sample_t *) S_AllocMedia(key, sizeof(s_sample_t));

		sample->media.type = S_MEDIA_SAMPLE;

		sample->media.Free = S_FreeSample;

		S_LoadSampleChunk(sample);

		S_RegisterMedia((s_media_t *) sample);
	}

	return sample;
}

/**
 * @brief Registers and returns a new sample, aliasing the chunk provided by
 * the specified sample.
 */
static s_sample_t *S_AliasSample(s_sample_t *sample, const char *alias) {

	s_sample_t *s = (s_sample_t *) S_AllocMedia(alias, sizeof(s_sample_t));

	sample->media.type = S_MEDIA_SAMPLE;

	s->buffer = sample->buffer;

	S_RegisterMedia((s_media_t *) s);

	// dependency back to the main sample
	S_RegisterDependency((s_media_t *) s, (s_media_t *) sample);

	return s;
}

/**
 * @brief
 */
s_sample_t *S_LoadModelSample(const char *model, const char *name) {
	char path[MAX_QPATH];
	char alias[MAX_QPATH];
	s_sample_t *sample;

	if (!s_env.context) {
		return NULL;
	}

	// if we can't figure it out, use common
	if (!model || *model == '\0') {
		model = "common";
	}

	// see if we already know of the model-specific sound
	g_snprintf(alias, sizeof(path), "#players/%s/%s", model, name + 1);
	sample = (s_sample_t *) S_FindMedia(alias);

	if (sample) {
		return sample;
	}

	// we don't, see if we already have this alias loaded
	if (S_FindMedia(alias)) {

		sample = S_LoadSample(alias);

		if (sample->buffer) {
			return sample;
		}

		Com_Warn("%s: media found but not loaded?\n", alias);
		return NULL;
	}

	// that didn't work, so load the common one and alias it.
	g_snprintf(path, sizeof(path), "#players/common/%s", name + 1);
	sample = S_LoadSample(path);

	if (sample->buffer) {
		return S_AliasSample(sample, alias);
	}

	Com_Warn("Failed to load %s\n", alias);
	return NULL;
}

/**
 * @brief
 */
s_sample_t *S_LoadEntitySample(const entity_state_t *ent, const char *name) {
	char model[MAX_QPATH];

	if (!s_env.context) {
		return NULL;
	}

	// determine what model the client is using
	memset(model, 0, sizeof(model));

	if (ent->number - 1 >= MAX_CLIENTS) {
		Com_Warn("Invalid client entity: %d\n", ent->number - 1);
		return NULL;
	}

	uint16_t n = CS_CLIENTS + ent->number - 1;
	if (cl.config_strings[n][0]) {
		char *p = strchr(cl.config_strings[n], '\\');
		if (p) {
			p += 1;
			strcpy(model, p);
			p = strchr(model, '/');
			if (p) {
				*p = 0;
			}
		}
	}

	// if we can't figure it out, use common
	if (*model == '\0') {
		strcpy(model, "common");
	}

	return S_LoadModelSample(model, name);
}
