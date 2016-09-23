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

	for (int32_t i = 0; i < MAX_CHANNELS; i++) {
		if (!s_env.channels[i].sample)
			return i;
	}

	Com_Debug("Failed\n");
	return -1;
}

/**
 * @brief
 */
void S_FreeChannel(int32_t c) {
	memset(&s_env.channels[c], 0, sizeof(s_channel_t));
}

#define SOUND_MAX_DISTANCE 2048.0

/**
 * @brief Set distance and stereo panning for the specified channel.
 */
_Bool S_SpatializeChannel(s_channel_t *ch) {
	vec3_t org, delta, center;

	VectorCopy(r_view.origin, org);

	if (ch->play.flags & S_PLAY_POSITIONED) {
		VectorCopy(ch->play.origin, org);
	} else if (ch->play.flags & S_PLAY_ENTITY) {
		const cl_entity_t *ent = &cl.entities[ch->play.entity];
		if (ent->current.solid == SOLID_BSP) {
			const r_model_t *mod = cl.model_precache[ent->current.model1];
			VectorLerp(mod->mins, mod->maxs, 0.5, center);
			VectorAdd(center, ent->origin, org);
		} else {
			VectorCopy(ent->origin, org);
		}
	}

	VectorSubtract(org, r_view.origin, delta);

	vec_t attenuation;
	switch (ch->play.attenuation) {
		case ATTEN_NORM:
			attenuation = 1.0;
			break;
		case ATTEN_IDLE:
			attenuation = 2.0;
			break;
		case ATTEN_STATIC:
			attenuation = 4.0;
			break;
		default:
			attenuation = 0.0;
			break;
	}

	const vec_t dist = VectorNormalize(delta) * attenuation;
	const vec_t frac = dist / SOUND_MAX_DISTANCE;

	ch->dist = (uint8_t) (Clamp(frac * 255.0, 0.0, 255.0));

	const vec_t dot = DotProduct(r_view.right, delta);
	const vec_t angle = acos(dot) * 180.0 / M_PI - 90.0;

	ch->angle = (int16_t) (360.0 - angle) % 360;

	return frac <= 1.0;
}

/**
 * @brief Updates all active channels for the current frame.
 */
void S_MixChannels(void) {

	s_env.num_active_channels = 0;

	s_channel_t *ch = s_env.channels;
	for (int32_t i = 0; i < MAX_CHANNELS; i++, ch++) {

		if (ch->sample) {

			if (ch->start_time) {
				if (ch->play.flags & S_PLAY_FRAME) {
					if (ch->frame != cl.frame.frame_num) {
						Mix_FadeOutChannel(i, 250);
						continue;
					}
				}
			}

			if (S_SpatializeChannel(ch)) {
				Mix_SetPosition(i, ch->angle, ch->dist);

				if (!ch->start_time) {
					ch->start_time = cl.systime;

					if (ch->play.flags & S_PLAY_LOOP) {
						Mix_PlayChannel(i, ch->sample->chunk, -1);
					} else {
						Mix_PlayChannel(i, ch->sample->chunk, 0);
					}
				}

				s_env.num_active_channels++;
			} else {
				
				if (ch->start_time) {
					Mix_HaltChannel(i);
				} else {
					S_FreeChannel(i);
				}
			}
		}
	}
}

/**
 * @brief
 */
void S_AddSample(const s_play_sample_t *play) {

	if (!s_env.initialized)
		return;

	if (!play)
		return;

	if (!play->sample)
		return;

	if (play->entity >= MAX_ENTITIES)
		return;

	switch (s_ambient->integer) {
		case 0:
			if (play->flags & S_PLAY_AMBIENT)
				return;
			break;
		case 2:
			if (!(play->flags & S_PLAY_AMBIENT))
				return;
			break;
		default:
			break;

	}
	
	const s_sample_t *sample = play->sample;

	const char *name = sample->media.name;
	if (name[0] == '*') {
		if (play->entity != -1) {
			const entity_state_t *ent = &cl.entities[play->entity].current;
			sample = S_LoadModelSample(ent, sample->media.name);
			if (sample == NULL) {
				Com_Debug("Failed to load player model sound %s\n", name);
				return;
			}
		} else {
			Com_Warn("Player model sound %s requested without entity\n", name);
			return;
		}
	}

	// for sounds added every frame, if an instance of the sound already exists, cull this one

	if (play->flags & S_PLAY_FRAME) {
		s_channel_t *ch = s_env.channels;
		for (int32_t i = 0; i < MAX_CHANNELS; i++, ch++) {
			if (ch->sample && (ch->play.flags & S_PLAY_FRAME)) {
				if (memcmp(play, &ch->play, sizeof(*play)) == 0) {
					ch->frame = cl.frame.frame_num;
					return;
				}
			}
		}
	}

	const int32_t i = S_AllocChannel();
	if (i != -1) {
		s_env.channels[i].play = *play;
		s_env.channels[i].sample = sample;
		s_env.channels[i].frame = cl.frame.frame_num;
	}
}
