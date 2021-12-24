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
		(*out_samples)[i] = (int16_t) Clampf(input_samples[i] * 32768.0f, INT16_MIN, INT16_MAX);
	}
}

/**
 * @brief
 */
static int32_t S_LoadSampleBuffer_(s_sample_t *sample, char *path) {

	void *buf;
	const int64_t len = Fs_Load(path, &buf);

	if (len != -1) {

		SDL_RWops *rw = SDL_RWFromConstMem(buf, (int32_t) len);

		SF_INFO info;
		memset(&info, 0, sizeof(info));

		SNDFILE *snd = sf_open_virtual(&s_rwops_io, SFM_READ, &info, rw);

		if (snd) {
			const size_t raw_size = sizeof(float) * info.frames * info.channels;

			if (s_context.raw_sample_buffer_size < raw_size) {
				s_context.raw_sample_buffer = Mem_Realloc(s_context.raw_sample_buffer, raw_size);
				s_context.raw_sample_buffer_size = raw_size;
			}

			sf_count_t count = sf_readf_float(snd, s_context.raw_sample_buffer, info.frames) * info.channels;

			S_ConvertSamples(s_context.raw_sample_buffer, count, &s_context.converted_sample_buffer, &s_context.converted_sample_buffer_size);

			const int16_t *buffer = s_context.converted_sample_buffer;

			if (info.samplerate != s_rate->integer) {
				count = S_Resample(info.channels, info.samplerate, s_rate->integer, count, buffer, &s_context.resample_buffer, &s_context.resample_buffer_size);
				buffer = s_context.resample_buffer;
			}

			sample->stereo = info.channels != 1;
			sample->num_samples = count;

			assert(sample->num_samples);

			alGenBuffers(1, &sample->buffer);

			const ALenum format = info.channels == 1 ? AL_FORMAT_MONO16 : AL_FORMAT_STEREO16;
			const ALsizei size = (ALsizei) count * sizeof(int16_t);

			alBufferData(sample->buffer, format, buffer, size, s_rate->integer);

			S_GetError(NULL);
		} else {
			Com_Warn("%s: %s\n", path, sf_strerror(snd));
		}

		sf_close(snd);

		SDL_RWclose(rw);

		Fs_Free(buf);
	}

	return sample->buffer;
}

/**
 * @brief
 */
static void S_LoadSampleBuffer(s_sample_t *sample) {
	const char *snd_formats[] = { "ogg", "wav", NULL };

	if (sample->media.name[0] == '*') { // placeholder
		return;
	}

	char name[MAX_QPATH];
	if (sample->media.name[0] == '#') {
		g_strlcpy(name, (sample->media.name + 1), sizeof(name));
	} else {
		g_snprintf(name, sizeof(name), "sounds/%s", sample->media.name);
	}

	char path[MAX_QPATH];
	for (const char **fmt = snd_formats; *fmt; fmt++) {
		g_snprintf(path, sizeof(path), "%s.%s", name, *fmt);
		if (S_LoadSampleBuffer_(sample, path)) {
			break;
		}
	}

	if (sample->buffer) {
		Com_Debug(DEBUG_SOUND, "Loaded %s for %s\n", path, sample->media.name);
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
	}
}

/**
 * @brief
 */
s_sample_t *S_LoadSample(const char *name) {

	if (!s_context.context) {
		return NULL;
	}

	if (!name || !name[0]) {
		Com_Error(ERROR_DROP, "NULL name\n");
	}

	char key[MAX_QPATH];
	StripExtension(name, key);

	s_sample_t *sample = (s_sample_t *) S_FindMedia(key, S_MEDIA_SAMPLE);
	if (sample == NULL) {

		sample = (s_sample_t *) S_AllocMedia(key, sizeof(s_sample_t), S_MEDIA_SAMPLE);

		sample->media.Free = S_FreeSample;

		S_LoadSampleBuffer(sample);

		S_RegisterMedia((s_media_t *) sample);
	}

	return sample;
}

/**
 * @brief
 */
s_sample_t *S_LoadClientModelSample(const char *model, const char *name) {

	if (!s_context.context) {
		return NULL;
	}

	if (!model || !model[0] || !name || !name[0]) {
		Com_Error(ERROR_DROP, "NULL model or name\n");
	}

	char key[MAX_QPATH];
	g_snprintf(key, sizeof(key), "#players/%s/%s", model, name + 1);

	s_sample_t *sample = (s_sample_t *) S_FindMedia(key, S_MEDIA_SAMPLE);
	if (sample == NULL) {

		sample = S_LoadSample(key);
		if (sample->buffer) {
			Com_Debug(DEBUG_SOUND, "Loaded %s\n", key);
		} else {
			char alias[MAX_QPATH];
			g_snprintf(alias, sizeof(alias), "#players/common/%s", name + 1);

			s_sample_t *aliased = S_LoadSample(alias);
			if (aliased->buffer) {

				S_RegisterDependency((s_media_t *) sample, (s_media_t *) aliased);

				sample->buffer = aliased->buffer;
				sample->num_samples = aliased->num_samples;
				sample->stereo = aliased->stereo;

				Com_Debug(DEBUG_SOUND, "Aliased %s for %s\n", aliased->media.name, key);
			} else {
				Com_Warn("Failed to load %s\n", key);
			}
		}
	}

	return sample;
}
