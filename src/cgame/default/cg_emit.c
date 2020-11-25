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

#include "cg_local.h"
#include "parse.h"

/**
 * @file Emits are client-sided entities for emitting lights, particles, coronas,
 * ambient sounds, etc. They are run once per frame, and culled by both
 * PHS and PVS, depending on their flags.
 */

#define EMIT_LIGHT		0x1
#define EMIT_SPARKS		0x2
#define EMIT_STEAM		0x4
#define EMIT_FLAME		0x8
#define EMIT_CORONA		0x10
#define EMIT_SOUND		0x20
#define EMIT_MODEL		0x40

#define EMIT_PVS (EMIT_LIGHT | EMIT_SPARKS | EMIT_STEAM | EMIT_FLAME | EMIT_CORONA | EMIT_MODEL)
#define EMIT_PHS (EMIT_PVS | EMIT_SOUND)

typedef struct {
	int32_t flags;
	vec3_t org;
	vec3_t angles; // for model orientation
	vec3_t dir; // for particle direction
	vec3_t vel; // for particle velocity
	vec3_t color;
	float hz, drift; // how frequently and randomly we fire
	float radius; // flame and corona radius
	float flicker;
	float scale; // mesh model scale
	int32_t count; // particle counts
	char sound[MAX_QPATH]; // sound name
	s_sample_t *sample; // sound sample
	int32_t atten; // sound attenuation
	_Bool loop; // loop sound versus timed
	char model[MAX_QPATH]; // model name
	r_model_t *mod; // model
	const r_bsp_leaf_t *leaf; // for pvs culling
	uint32_t time; // when to fire next
} cg_emit_t;

#define MAX_EMITS 256
static cg_emit_t cg_emits[MAX_EMITS];
static size_t cg_num_emits;

/**
 * @brief Parse misc_emits from the bsp after it has been loaded. This must
 * be called after Cm_LoadMap, once per pre-cache routine.
 */
void Cg_LoadEmits(void) {
	const char *ents;
	char class_name[MAX_QPATH];
	cg_emit_t *e;
	_Bool entity, emit;

	Cg_FreeEmits();

	ents = cgi.EntityString();

	memset(class_name, 0, sizeof(class_name));
	entity = emit = false;

	e = NULL;

	char token[MAX_BSP_ENTITY_VALUE];
	parser_t parser;

	Parse_Init(&parser, ents, PARSER_NO_COMMENTS);

	while (true) {

		if (!Parse_Token(&parser, PARSE_DEFAULT, token, sizeof(token))) {
			break;
		}

		if (*token == '{') {
			entity = true;
			continue;
		}

		if (!entity) { // skip any whitespace between ents
			continue;
		}

		if (*token == '}') {
			entity = false;

			if (emit) {

				// resolve the leaf so that the entity may be added only when in PVS
				e->leaf = cgi.LeafForPoint(e->org);

				// add default sounds and models
				if (!e->sample) {

					if (e->flags & EMIT_SPARKS) {
						strcpy(e->sound, "world/sparks");
						e->sample = cgi.LoadSample(e->sound);
					}

					else if (e->flags & EMIT_STEAM) { // steam hissing
						strcpy(e->sound, "world/steam");
						e->sample = cgi.LoadSample(e->sound);
					}

					else if (e->flags & EMIT_FLAME) { // fire crackling
						strcpy(e->sound, "world/fire");
						e->sample = cgi.LoadSample(e->sound);
					}
				}

				// crutch up flags as a convenience
				if (e->sample) {
					e->flags |= EMIT_SOUND;
				}

				if (e->mod) {
					e->flags |= EMIT_MODEL;
				}

				if (Vec3_Equal(e->color, Vec3_Zero())) { // default color
					e->color = Vec3(1.0, 1.0, 1.0);
				}

				if (e->count <= 0) { // default particle count
					e->count = 12;
				}

				if (e->radius <= 0.0) { // default light and corona radius

					if (e->flags & EMIT_CORONA) {
						e->radius = 12.0;
					} else if (e->flags & EMIT_LIGHT) {
						e->radius = 100.0;
					} else {
						e->radius = 1.0;
					}
				}

				if (Vec3_Equal(e->vel, Vec3_Zero())) { // default velocity

					if (e->flags & EMIT_STEAM) {
						e->vel = Vec3(0.0, 0.0, 40.0);
					}
				}

				if (e->flags & EMIT_SPARKS) { // default directional scale
					e->dir = Vec3_Scale(e->dir, 40.0);
				}

				if (e->scale <= 0.0) { // default mesh model scale
					e->scale = 1.0;
				}

				if (e->hz <= 0.0) { // default hz and drift

					if (e->flags & EMIT_LIGHT) {
						if (e->hz == 0.0) { // -1.0 for constant light
							e->hz = 0.5;
						}
					} else if (e->flags & EMIT_SPARKS) {
						e->hz = 0.5;
					} else if (e->flags & EMIT_STEAM) {
						e->hz = 20.0;
					} else if (e->flags & EMIT_FLAME) {
						e->hz = 5.0;
					} else if (e->flags & EMIT_SOUND) {
						e->hz = 0.0;    // ambient
					} else {
						e->hz = 1.0;
					}
				}

				if (e->drift <= 0.0) {

					if (e->flags & (EMIT_LIGHT | EMIT_SPARKS)) {
						e->drift = 3.0;
					} else {
						e->drift = 0.01;
					}
				}

				if (e->flags & EMIT_SOUND) { // resolve attenuation and looping

					if (e->atten == -1) { // explicit -1 for global
						e->atten = ATTEN_NONE;
					} else {
						if (e->atten == 0) { // default
							if (e->flags & (EMIT_SPARKS | EMIT_STEAM | EMIT_FLAME)) {
								e->atten = ATTEN_STATIC;
							} else {
								e->atten = ATTEN_IDLE;
							}
						}
					}

					// flame and steam sounds are always looped
					if (e->flags & (EMIT_FLAME | EMIT_STEAM)) {
						e->loop = true;
					} else
						// the default is to honor the hz parameter
					{
						e->loop = e->hz == 0.0;
					}
				}

				if (e->flags & EMIT_SPARKS) { // don't combine sparks and light
					e->flags &= ~EMIT_LIGHT;
				}

				Cg_Debug("Added %d emit at %s\n", e->flags, vtos(e->org));

				cg_num_emits++;

				if (cg_num_emits == lengthof(cg_emits)) {
					cgi.Warn("MAX_EMITS reached\n");
					return;
				}
			} else {
				memset(&cg_emits[cg_num_emits], 0, sizeof(cg_emit_t));
			}

			emit = false;
			continue;
		}

		if (!g_strcmp0(token, "classname")) {

			if (!Parse_Token(&parser, PARSE_DEFAULT, token, sizeof(token))) {
				break;
			}

			if (!g_strcmp0(token, "misc_emit") || !g_strcmp0(token, "misc_model")) {
				emit = true;
			}

			continue;
		}

		e = &cg_emits[cg_num_emits];

		if (!g_strcmp0(token, "flags") || !g_strcmp0(token, "spawnflags")) {

			if (!Parse_Primitive(&parser, PARSE_DEFAULT, PARSE_INT32, &e->flags, 1)) {
				break;
			}

			continue;
		}

		if (!g_strcmp0(token, "origin")) {

			if (Parse_Primitive(&parser, PARSE_DEFAULT | PARSE_WITHIN_QUOTES, PARSE_FLOAT, e->org.xyz, 3) != 3) {
				break;
			}

			continue;
		}

		if (!g_strcmp0(token, "angles")) { // resolve angles and directional vector

			if (Parse_Primitive(&parser, PARSE_DEFAULT | PARSE_WITHIN_QUOTES, PARSE_FLOAT, e->angles.xyz, 3) != 3) {
				break;
			}

			Vec3_Vectors(e->angles, &e->dir, NULL, NULL);
			continue;
		}

		if (!g_strcmp0(token, "color")) { // resolve color as vector

			if (Parse_Primitive(&parser, PARSE_DEFAULT | PARSE_WITHIN_QUOTES, PARSE_FLOAT, e->color.xyz, 3) != 3) {
				break;
			}

			continue;
		}

		if (!g_strcmp0(token, "hz")) {

			if (!Parse_Primitive(&parser, PARSE_DEFAULT, PARSE_FLOAT, &e->hz, 1)) {
				break;
			}

			continue;
		}

		if (!g_strcmp0(token, "drift")) {

			if (!Parse_Primitive(&parser, PARSE_DEFAULT, PARSE_FLOAT, &e->drift, 1)) {
				break;
			}

			continue;
		}

		if (!g_strcmp0(token, "radius")) {

			if (!Parse_Primitive(&parser, PARSE_DEFAULT, PARSE_FLOAT, &e->radius, 1)) {
				break;
			}

			continue;
		}

		if (!g_strcmp0(token, "flicker")) {

			if (!Parse_Primitive(&parser, PARSE_DEFAULT, PARSE_FLOAT, &e->flicker, 1)) {
				break;
			}

			continue;
		}

		if (!g_strcmp0(token, "scale")) {

			if (!Parse_Primitive(&parser, PARSE_DEFAULT, PARSE_FLOAT, &e->scale, 1)) {
				break;
			}

			continue;
		}

		if (!g_strcmp0(token, "count")) {

			if (!Parse_Primitive(&parser, PARSE_DEFAULT, PARSE_INT32, &e->count, 1)) {
				break;
			}

			continue;
		}

		if (!g_strcmp0(token, "sound")) {

			if (!Parse_Token(&parser, PARSE_DEFAULT, e->sound, sizeof(e->sound))) {
				break;
			}

			e->sample = cgi.LoadSample(e->sound);
			continue;
		}

		if (!g_strcmp0(token, "attenuation") || !g_strcmp0(token, "atten")) {

			if (!Parse_Primitive(&parser, PARSE_DEFAULT, PARSE_INT32, &e->atten, 1)) {
				break;
			}

			continue;
		}

		if (!g_strcmp0(token, "model")) {

			if (!Parse_Token(&parser, PARSE_DEFAULT, e->model, sizeof(e->model))) {
				break;
			}

			e->mod = cgi.LoadModel(e->model);
			continue;
		}

		if (!g_strcmp0(token, "velocity")) {

			if (Parse_Primitive(&parser, PARSE_DEFAULT | PARSE_WITHIN_QUOTES, PARSE_FLOAT, e->vel.xyz, 3) != 3) {
				break;
			}

			continue;
		}
	}
}

/**
 * @brief
 */
void Cg_FreeEmits(void) {

	memset(&cg_emits, 0, sizeof(cg_emits));
	cg_num_emits = 0;
}

/**
 * @brief Perform PVS and PHS filtering, returning a copy of the specified emit with
 * the correct flags stripped away for this frame.
 */
static cg_emit_t *Cg_UpdateEmit(cg_emit_t *e) {
	static cg_emit_t em;

	em = *e;

	if (!cgi.LeafHearable(e->leaf)) {
		em.flags &= ~EMIT_PHS;
	} else if (!cgi.LeafVisible(e->leaf)) {
		em.flags &= ~EMIT_PVS;
	}

	if (em.flags && em.hz && em.time < cgi.client->unclamped_time) { // update the time stamp
		const float drift = e->drift * Randomf() * 1000.0;
		e->time = cgi.client->unclamped_time + (1000.0 / e->hz) + drift;
	}

	return &em;
}

/**
 * @brief
 */
void Cg_AddEmits(void) {

	if (!cg_add_emits->value) {
		return;
	}

	for (size_t i = 0; i < cg_num_emits; i++) {

		cg_emit_t *e = Cg_UpdateEmit(&cg_emits[i]);

		// first add emits which fire every frame

		if ((e->flags & EMIT_LIGHT) && !e->hz) {
			Cg_AddLight(&(cg_light_t) {
				.origin = e->org,
				.radius = e->radius,
				.color = e->color,
				.decay = 0
			});
		}

		if ((e->flags & EMIT_SOUND) && e->loop) {
			cgi.AddSample(&(const s_play_sample_t) {
				.sample = e->sample,
				.origin = e->org,
				.attenuation = e->atten,
				.flags = S_PLAY_POSITIONED | S_PLAY_AMBIENT | S_PLAY_LOOP | S_PLAY_FRAME
			});
		}

		if (e->flags & EMIT_MODEL) {
			r_entity_t ent;

			memset(&ent, 0, sizeof(ent));

			ent.origin = e->org;
			ent.angles = e->angles;

			ent.model = e->mod;
			ent.lerp = 1.0;

			ent.scale = e->scale;

			cgi.AddEntity(&ent);
		}

		if (e->flags & EMIT_CORONA) {

//			cg_sprite_t *p = Cg_AllocSprite();
//
//			if (p) {
//				p->color = e->color;
//				p->origin = e->org;
//
//				p->part.scale = CORONA_SCALE(e->radius, e->flicker);
//			}
		}

		// then add emits with timed events if they are due to run

		if (e->time > cgi.client->unclamped_time) {
			continue;
		}

		if ((e->flags & EMIT_LIGHT) && e->hz) {
			Cg_AddLight(&(cg_light_t) {
				.origin = e->org,
				.radius = e->radius,
				.color = e->color,
				.decay = 1000
			});
		}

		if (e->flags & EMIT_SPARKS) {
			Cg_SparksEffect(e->org, e->dir, e->count);
		}

		if (e->flags & EMIT_STEAM) {
			Cg_SteamTrail(NULL, e->org, e->vel);
		}

		if (e->flags & EMIT_FLAME) {
			Cg_FlameTrail(NULL, e->org, e->org);
		}

		if ((e->flags & EMIT_SOUND) && !e->loop) {
			cgi.AddSample(&(const s_play_sample_t) {
				.sample = e->sample,
				.origin = e->org,
				.attenuation = e->atten,
				.flags = S_PLAY_POSITIONED | S_PLAY_AMBIENT
			});
		}
	}
}
