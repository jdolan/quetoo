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

/**
 * @brief
 */
static int32_t S_AllocChannel(void) {
	int32_t i;

	for (i = 0; i < MAX_CHANNELS; i++) {
		if (!s_env.channels[i].sample)
			return i;
	}

	return -1;
}

/**
 * @brief
 */
void S_FreeChannel(int32_t c) {
	memset(&s_env.channels[c], 0, sizeof(s_env.channels[0]));
}

#define SOUND_MAX_DISTANCE 2048.0

/**
 * @brief Set distance and stereo panning for the specified channel.
 */
_Bool S_SpatializeChannel(s_channel_t *ch) {
	vec3_t delta;

	if (ch->ent_num != -1) { // update the channel origin
		const entity_state_t *ent = &cl.entities[ch->ent_num].current;
		VectorCopy(ent->origin, ch->org);
	}

	VectorSubtract(ch->org, r_view.origin, delta);
	vec_t dist = VectorNormalize(delta) * ch->atten;

	if (dist < SOUND_MAX_DISTANCE) { // check if there's a clear line of sight to the origin
		cm_trace_t tr = Cl_Trace(r_view.origin, ch->org, NULL, NULL, 0, MASK_CLIP_PROJECTILE);
		if (tr.fraction < 1.0) {
			dist *= 1.25;
		}
	}

	dist = (dist * 255.0) / SOUND_MAX_DISTANCE;

	ch->dist = (uint8_t) Clamp(dist, 0.0, 255.0);

	const vec_t dot = DotProduct(r_view.right, delta);
	const vec_t angle = acos(dot) * 180.0 / M_PI - 90.0;

	ch->angle = (int16_t) (360.0 - angle) % 360;

	return ch->dist < 255;
}

/**
 * @brief
 */
void S_PlaySample(const vec3_t org, uint16_t ent_num, s_sample_t *sample, int32_t atten) {
	s_channel_t *ch;
	int32_t i;

	if (!s_env.initialized)
		return;

	if (sample && sample->media.name[0] == '*') // resolve the model-specific sample
		sample = S_LoadModelSample(&cl.entities[ent_num].current, sample->media.name);

	if (!sample || !sample->chunk)
		return;

	if ((i = S_AllocChannel()) == -1)
		return;

	ch = &s_env.channels[i];

	if (org) { // positioned sound
		VectorCopy(org, ch->org);
		ch->ent_num = -1;
	} else
		// entity sound
		ch->ent_num = ent_num;

	ch->atten = atten;
	ch->sample = sample;

	if (S_SpatializeChannel(ch)) {
		Mix_SetPosition(i, ch->angle, ch->dist);
		Mix_PlayChannel(i, ch->sample->chunk, 0);
	} else {
		S_FreeChannel(i);
	}
}

/**
 * @brief
 */
void S_LoopSample(const vec3_t org, s_sample_t *sample) {
	s_channel_t *ch;
	vec3_t delta;
	int32_t i;

	if (!sample || !sample->chunk)
		return;

	ch = NULL;

	for (i = 0; i < MAX_CHANNELS; i++) { // find existing loop sound

		if (s_env.channels[i].ent_num != -1)
			continue;

		if (s_env.channels[i].sample == sample) {

			VectorSubtract(s_env.channels[i].org, org, delta);

			if (VectorLength(delta) < 512.0) {
				ch = &s_env.channels[i];
				break;
			}
		}
	}

	if (ch) { // update existing loop sample
		ch->count++;
		VectorMix(ch->org, org, 1.0 / ch->count, ch->org);
	} else { // or allocate a new one

		if ((i = S_AllocChannel()) == -1)
			return;

		ch = &s_env.channels[i];

		VectorCopy(org, ch->org);
		ch->ent_num = -1;
		ch->count = 1;
		ch->atten = ATTEN_IDLE;
		ch->sample = sample;

		if (S_SpatializeChannel(ch)) {
			Mix_SetPosition(i, ch->angle, ch->dist);
			Mix_PlayChannel(i, ch->sample->chunk, 0);
		} else {
			S_FreeChannel(i);
		}
	}
}

/**
 * @brief
 */
void S_StartLocalSample(const char *name) {
	s_sample_t *sample;

	if (!s_env.initialized)
		return;

	sample = S_LoadSample(name);

	if (!sample) {
		Com_Warn("Failed to load %s.\n", name);
		return;
	}

	S_PlaySample(NULL, cl.client_num + 1, sample, ATTEN_NONE);
}
