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

#include "cg_local.h"

/*
 * Emits are client-sided entities for emitting lights, particles, coronas,
 * ambient sounds, etc.  They are run once per frame, and culled by both
 * PHS and PVS, depending on their flags.
 */

#define EMIT_LIGHT		0x1
#define EMIT_SPARKS		0x2
#define EMIT_STEAM		0x4
#define EMIT_FLAME		0x8
#define EMIT_CORONA		0x10
#define EMIT_SOUND		0x20
#define EMIT_MODEL		0x40

// these emits are ONLY visible; they have no hearable component
#define EMIT_VISIBLE (EMIT_LIGHT | EMIT_CORONA | EMIT_MODEL)

typedef struct cl_emit_s {
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
	bool loop; // loop sound versus timed
	char model[MAX_QPATH]; // model name
	r_model_t *mod; // model
	const r_bsp_leaf_t *leaf; // for pvs culling
	r_lighting_t lighting; // cached static lighting info
	uint32_t time; // when to fire next
} cg_emit_t;

#define MAX_EMITS 256
static cg_emit_t cg_emits[MAX_EMITS];
static uint16_t cg_num_emits;

/*
 * Cg_LoadEmits
 *
 * Parse misc_emits from the bsp after it has been loaded.  This must
 * be called after Cm_LoadMap, once per pre-cache routine.
 */
void Cg_LoadEmits(void) {
	const char *ents;
	char class_name[MAX_QPATH];
	cg_emit_t *e;
	bool entity, emit;

	memset(&cg_emits, 0, sizeof(cg_emits));
	cg_num_emits = 0;

	ents = cgi.EntityString();

	memset(class_name, 0, sizeof(class_name));
	entity = emit = false;

	e = NULL;

	while (true) {

		const char *c = ParseToken(&ents);

		if (*c == '\0')
			break;

		if (*c == '{')
			entity = true;

		if (!entity) // skip any whitespace between ents
			continue;

		if (*c == '}') {
			entity = false;

			if (emit) {

				// resolve the leaf so that the entity may be added only when in PVS
				e->leaf = cgi.LeafForPoint(e->org, NULL);

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
				if (e->sample)
					e->flags |= EMIT_SOUND;

				if (e->mod)
					e->flags |= EMIT_MODEL;

				if (VectorCompare(e->color, vec3_origin)) // default color
					VectorSet(e->color, 1.0, 1.0, 1.0);

				if (e->count <= 0) // default particle count
					e->count = 12;

				if (e->radius <= 0.0) { // default light and corona radius

					if (e->flags & EMIT_CORONA)
						e->radius = 12.0;
					else if (e->flags & EMIT_LIGHT)
						e->radius = 100.0;
					else
						e->radius = 1.0;
				}

				if (VectorCompare(e->vel, vec3_origin)) { // default velocity

					if (e->flags & EMIT_STEAM)
						VectorSet(e->vel, 0.0, 0.0, 40.0);
				}

				if (e->flags & EMIT_SPARKS) { // default directional scale
					VectorScale(e->dir, 40.0, e->dir);
				}

				if (e->scale <= 0.0) { // default mesh model scale
					e->scale = 1.0;
				}

				if (e->hz <= 0.0) { // default hz and drift

					if (e->flags & EMIT_LIGHT) {
						if (e->hz == 0.0) // -1.0 for constant light
							e->hz = 0.5;
					} else if (e->flags & EMIT_SPARKS)
						e->hz = 0.5;
					else if (e->flags & EMIT_STEAM)
						e->hz = 20.0;
					else if (e->flags & EMIT_FLAME)
						e->hz = 5.0;
					else if (e->flags & EMIT_SOUND)
						e->hz = 0.0; // ambient
					else
						e->hz = 1.0;
				}

				if (e->drift <= 0.0) {

					if (e->flags & (EMIT_LIGHT | EMIT_SPARKS))
						e->drift = 3.0;
					else
						e->drift = 0.01;
				}

				if (e->flags & EMIT_SOUND) { // resolve attenuation and looping

					if (e->atten == -1) // explicit -1 for global
						e->atten = ATTN_NONE;
					else {
						if (e->atten == 0) { // default
							if (e->flags & EMIT_SPARKS) {
								e->atten = ATTN_STATIC;
							} else {
								e->atten = DEFAULT_SOUND_ATTENUATION;
							}
						}
					}

					// flame and steam sounds are always looped
					if (e->flags & (EMIT_FLAME | EMIT_STEAM))
						e->loop = true;
					else
						// the default is to honor the hz parameter
						e->loop = e->hz == 0.0;
				}

				if (e->flags & EMIT_SPARKS) // don't combine sparks and light
					e->flags &= ~EMIT_LIGHT;

				cgi.Debug("Added %d emit at %s\n", e->flags, vtos(e->org));

				cg_num_emits++;
			} else
				memset(&cg_emits[cg_num_emits], 0, sizeof(cg_emit_t));

			emit = false;
		}

		if (!strcmp(c, "classname")) {

			c = ParseToken(&ents);
			strncpy(class_name, c, sizeof(class_name) - 1);

			if (!strcmp(c, "misc_emit") || !strcmp(c, "misc_model"))
				emit = true;
		}

		e = &cg_emits[cg_num_emits];

		if (!strcmp(c, "flags") || !strcmp(c, "spawnflags")) {
			e->flags = atoi(ParseToken(&ents));
			continue;
		}

		if (!strcmp(c, "origin")) {
			sscanf(ParseToken(&ents), "%f %f %f", &e->org[0], &e->org[1], &e->org[2]);
			continue;
		}

		if (!strcmp(c, "angles")) { // resolve angles and directional vector
			sscanf(ParseToken(&ents), "%f %f %f", &e->angles[0], &e->angles[1], &e->angles[2]);
			AngleVectors(e->angles, e->dir, NULL, NULL);
			continue;
		}

		if (!strcmp(c, "color")) { // resolve color as floats
			sscanf(ParseToken(&ents), "%f %f %f", &e->color[0], &e->color[1], &e->color[2]);
			continue;
		}

		if (!strcmp(c, "hz")) {
			e->hz = atof(ParseToken(&ents));
			continue;
		}

		if (!strcmp(c, "drift")) {
			e->drift = atof(ParseToken(&ents));
			continue;
		}

		if (!strcmp(c, "radius")) {
			e->radius = atof(ParseToken(&ents));
			continue;
		}

		if (!strcmp(c, "flicker")) {
			e->flicker = atof(ParseToken(&ents));
			continue;
		}

		if (!strcmp(c, "scale")) {
			sscanf(ParseToken(&ents), "%f", &e->scale);
			continue;
		}

		if (!strcmp(c, "count")) {
			e->count = atoi(ParseToken(&ents));
			continue;
		}

		if (!strcmp(c, "sound")) {
			snprintf(e->sound, sizeof(e->sound), "%s", ParseToken(&ents));
			e->sample = cgi.LoadSample(e->sound);
			continue;
		}

		if (!strcmp(c, "attenuation")) {
			e->atten = atoi(ParseToken(&ents));
			continue;
		}

		if (!strcmp(c, "model")) {
			strncpy(e->model, ParseToken(&ents), sizeof(e->model));
			e->mod = cgi.LoadModel(e->model);
			continue;
		}

		if (!strcmp(c, "velocity")) {
			sscanf(ParseToken(&ents), "%f %f %f", &e->vel[0], &e->vel[1], &e->vel[2]);
			continue;
		}
	}
}

/*
 * Cg_EmitLight
 */
static r_light_t *Cg_EmitLight(cg_emit_t *e) {
	static r_light_t l;

	VectorCopy(e->org, l.origin);
	l.radius = e->radius;
	VectorCopy(e->color, l.color);

	return &l;
}

/*
 * Cg_UpdateEmit
 *
 * Perform PVS and PHS filtering, returning a copy of the specified emit with
 * the correct flags stripped away for this frame.
 */
cg_emit_t *Cg_UpdateEmit(cg_emit_t *e) {
	static cg_emit_t em;

	em = *e;

	if (!cgi.LeafInPhs(e->leaf))
		em.flags = 0;
	else if (!cgi.LeafInPvs(e->leaf)) {
		em.flags &= ~EMIT_VISIBLE;
	}

	if (em.flags && em.hz && em.time < cgi.client->time) { // update the time stamp
		const float drift = e->drift * Randomf() * 1000.0;
		e->time = cgi.client->time + (1000.0 / e->hz) + drift;
	}

	return &em;
}

/*
 * Cg_AddEmits
 */
void Cg_AddEmits(void) {
	r_entity_t ent;
	int32_t i;

	if (!cg_add_emits->value)
		return;

	memset(&ent, 0, sizeof(ent));

	for (i = 0; i < cg_num_emits; i++) {

		cg_emit_t *e = Cg_UpdateEmit(&cg_emits[i]);

		// first add emits which fire every frame

		if (e->flags & EMIT_LIGHT && !e->hz)
			cgi.AddLight(Cg_EmitLight(e));

		if ((e->flags & EMIT_SOUND) && e->loop)
			cgi.LoopSample(e->org, e->sample);

		if (e->flags & EMIT_MODEL) {
			VectorCopy(e->org, ent.origin);
			VectorCopy(e->angles, ent.angles);

			ent.model = e->mod;
			ent.lerp = 1.0;

			ent.scale = e->scale;

			ent.lighting = &e->lighting;

			cgi.AddEntity(&ent);
		}

		if (e->flags & EMIT_CORONA) {
			r_corona_t c;

			VectorCopy(e->org, c.origin);
			c.radius = e->radius;
			c.flicker = e->flicker;
			VectorCopy(e->color, c.color);

			cgi.AddCorona(&c);
		}

		// then add emits with timed events if they are due to run

		if (e->time > cgi.client->time)
			continue;

		if ((e->flags & EMIT_LIGHT) && e->hz) {
			const r_light_t *l = Cg_EmitLight(e);
			r_sustained_light_t s;

			s.light = *l;
			s.sustain = 0.65;

			cgi.AddSustainedLight(&s);
		}

		if (e->flags & EMIT_SPARKS)
			Cg_SparksEffect(e->org, e->dir, e->count);

		if (e->flags & EMIT_STEAM)
			Cg_SteamTrail(e->org, e->vel, NULL);

		if (e->flags & EMIT_FLAME)
			Cg_FlameTrail(e->org, e->org, NULL);

		if ((e->flags & EMIT_SOUND) && !e->loop)
			cgi.PlaySample(e->org, -1, e->sample, e->atten);
	}
}
