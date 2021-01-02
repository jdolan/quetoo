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
 * @brief
 */
int32_t S_AllocChannel(void) {

	for (int32_t i = 0; i < MAX_CHANNELS; i++) {
		if (!s_context.channels[i].play.sample) {
			return i;
		}
	}

	Com_Debug(DEBUG_SOUND, "Failed\n");
	return -1;
}

/**
 * @brief
 */
void S_FreeChannel(int32_t c) {

	alSourceStop(s_context.sources[c]);
	alSourcei(s_context.sources[c], AL_BUFFER, 0);

	memset(&s_context.channels[c], 0, sizeof(s_context.channels[c]));
}

#define SOUND_MAX_DISTANCE 2048.0

/**
 * @brief Resolve gain, pitch and effects for the specified channel.
 * @param ch The channel.
 * @return True if the channel is hearable, false otherwise.
 */
static _Bool S_SpatializeChannel(s_channel_t *ch) {

	vec3_t delta;
	const float dist = Vec3_DistanceDir(ch->play.origin, s_stage.origin, &delta);

	float atten;
	switch (ch->play.atten) {
		case SOUND_ATTEN_NONE:
			atten = 0.f;
			break;
		case SOUND_ATTEN_LINEAR:
			atten = 1.f;
			break;
		case SOUND_ATTEN_SQUARE:
			atten = 2.f;
			break;
		case SOUND_ATTEN_CUBIC:
			atten = 4.f;
			break;
	}

	const float frac = dist * atten / SOUND_MAX_DISTANCE;

	ch->gain = 1.0 - frac;

	// fade out frame sounds if they are no longer in the frame
	if (ch->start_time) {
		if (ch->play.flags & S_PLAY_FRAME) {
			if (ch->timestamp != s_stage.ticks) {
				const uint32_t delta = s_stage.ticks - ch->timestamp;

				if (delta > 250) {
					return false; // x faded out
				}

				ch->gain *= 1.f - (delta / 250.f);
			}
		}
	}

	ch->pitch = 1.f;
	ch->filter = AL_NONE;

	// adjust pitch in liquids
	if (s_stage.contents & CONTENTS_MASK_LIQUID) {
		ch->pitch = .5f;
	}

	// offset pitch by sound-requested offset
	if (ch->play.pitch) {
		const float octaves = (float) pow(2.0, 0.69314718 * ((float) ch->play.pitch / TONES_PER_OCTAVE));
		ch->pitch *= octaves;
	}

	// apply sound effects
	if (s_context.effects.loaded) {

		if (ch->play.flags & S_PLAY_UNDERWATER) {
			ch->filter = s_context.effects.underwater;
		} else if (ch->play.flags & S_PLAY_OCCLUDED) {
			ch->filter = s_context.effects.occluded;
		}
	}

	return ch->gain > 0.f;
}

/**
 * @brief Updates all active channels for the current frame.
 */
void S_MixChannels(void) {

	if (s_effects->modified) {
		S_Restart_f();

		s_effects->modified = false;
		return;
	}

	if (s_doppler->modified) {
		alDopplerFactor(.05f * s_doppler->value);
	}

	alListenerfv(AL_POSITION, s_stage.origin.xyz);

	alListenerfv(AL_ORIENTATION, (float []) {
		s_stage.forward.x, s_stage.forward.y, s_stage.forward.z,
		s_stage.up.x, s_stage.up.y, s_stage.up.z
	});

	if (s_doppler->value) {
		alListenerfv(AL_VELOCITY, s_stage.velocity.xyz);
	} else {
		alListenerfv(AL_VELOCITY, Vec3_Zero().xyz);
	}

	s_context.num_channels = 0;

	s_channel_t *ch = s_context.channels;
	for (int32_t i = 0; i < MAX_CHANNELS; i++, ch++) {

		if (ch->play.sample == NULL) {
			continue;
		}

		const ALuint src = s_context.sources[i];
		assert(src);

		if (!S_SpatializeChannel(ch)) {
			S_FreeChannel(i);
			continue;
		}

		if (ch->play.flags & S_PLAY_RELATIVE) {
			alSourcefv(src, AL_POSITION, Vec3_Zero().xyz);
		} else {
			alSourcefv(src, AL_POSITION, ch->play.origin.xyz);

			if (s_doppler->value) {
				alSourcefv(src, AL_VELOCITY, ch->play.velocity.xyz);
			} else {
				alSourcefv(src, AL_VELOCITY, Vec3_Zero().xyz);
			}
		}

		float volume;
		if (ch->play.flags & S_PLAY_AMBIENT) {
			volume = s_volume->value * s_ambient_volume->value;
		} else {
			volume = s_volume->value * s_effects_volume->value;
		}

		alSourcef(src, AL_GAIN, ch->gain * volume);
		alSourcef(src, AL_PITCH, ch->pitch);

		if (s_context.effects.loaded) {
			alSourcei(src, AL_DIRECT_FILTER, ch->filter);
		}

		if (ch->start_time == 0) {
			ch->start_time = s_stage.ticks;

			alSourcei(src, AL_BUFFER, ch->play.sample->buffer);

			if (ch->play.flags & S_PLAY_RELATIVE) {
				alSourcei(src, AL_SOURCE_RELATIVE, 1);
			} else {
				alSourcei(src, AL_SOURCE_RELATIVE, 0);
			}

			if (ch->play.flags & S_PLAY_LOOP) {
				alSourcei(src, AL_LOOPING, 1);
			} else {
				alSourcei(src, AL_LOOPING, 0);
			}

			if (ch->play.flags & S_PLAY_AMBIENT) {
				alSourcei(src, AL_SAMPLE_OFFSET, Randomf() * (int32_t) ch->play.sample->num_samples);
			}

			alSourcePlay(src);

		} else {
			ALenum state;
			alGetSourcei(src, AL_SOURCE_STATE, &state);

			if (state != AL_PLAYING) {
				S_FreeChannel(i);
				continue;
			}
		}

		S_GetError(ch->play.sample->media.name);

		s_context.num_channels++;
	}
}

/**
 * @brief
 */
void S_AddSample(const s_play_sample_t *play) {

	if (!s_context.context) {
		return;
	}

	if (!s_volume->value) {
		return;
	}

	if (!play) {
		return;
	}

	if (!play->sample) {
		return;
	}

	if (play->flags & S_PLAY_AMBIENT) {
		if (!s_ambient_volume->value) {
			return;
		}
	} else {
		if (!s_effects_volume->value) {
			return;
		}
	}

	if (s_stage.num_samples == MAX_SOUNDS) {
		Com_Debug(DEBUG_SOUND, "MAX_SOUNDS");
		return;
	}

	s_stage.samples[s_stage.num_samples] = *play;
	s_stage.num_samples++;
}
