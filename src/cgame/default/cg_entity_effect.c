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

/**
 * @brief Resolve a client color from the entity index given in the effect
 */
color_t Cg_ResolveEffectColor(const uint8_t index, const color_t default_color) {

	if (index >= MAX_CLIENTS) {
		return default_color;
	}

	const color_t color = cgi.client->client_info[index].color;

	if (color.r || color.g || color.b) {
		return color;
	}

	return default_color;
}

/**
 * @brief
 */
static void Cg_InactiveEffect(cl_entity_t *ent, const vec3_t org) {
	cg_particle_t *p;

	if (Cg_IsSelf(ent) && !cgi.client->third_person) {
		return;
	}

	if (!(p = Cg_AllocParticle(PARTICLE_NORMAL, cg_particles_inactive))) {
		return;
	}

	p->lifetime = PARTICLE_IMMEDIATE;

	cgi.ColorFromPalette(11, p->part.color);

	p->part.scale = 10.0;

	VectorCopy(org, p->part.org);
	p->part.org[2] += 50.0;

}

/**
 * @brief Processes the entity's effects mask, augmenting the renderer entity.
 */
void Cg_EntityEffects(cl_entity_t *ent, r_entity_t *e) {

	e->effects = ent->current.effects;

	if (e->effects & EF_ROTATE) {
		const vec_t rotate = cgi.client->unclamped_time;
		e->angles[YAW] = cg_entity_rotate->value * rotate / M_PI;
	}

	if (e->effects & EF_BOB) {
		VectorCopy(e->origin, e->termination);
		const vec_t bob = sin(cgi.client->unclamped_time * 0.005 + ent->current.number);
		e->origin[2] += cg_entity_bob->value * bob;
	}

	if (e->effects & EF_PULSE) {
		const vec_t pulse = (cos(cgi.client->unclamped_time * 0.0033 + ent->current.number) + 1.0);
		const vec_t c = 1.0 - (cg_entity_pulse->value * 0.5 * pulse);
		VectorSet(e->color, c, c, c);
	} else {
		VectorSet(e->color, 1.0, 1.0, 1.0);
	}

	if (e->effects & EF_INACTIVE) {
		Cg_InactiveEffect(ent, e->origin);
	}

	if (e->effects & EF_RESPAWN) {
		const vec3_t color = { 0.5, 0.5, 0.0 };

		VectorMA(e->shell, 0.5, color, e->shell);
	}

	if (e->effects & EF_QUAD) {
		r_light_t l = { { 0.0, 0.0, 0.0 }, { 0.3, 0.7, 0.7 }, 80.0 };

		VectorCopy(e->origin, l.origin);
		cgi.AddLight(&l);

		VectorMA(e->shell, 0.5, l.color, e->shell);
	}

	if (e->effects & EF_CTF_RED) {
		r_light_t l = { { 0.0, 0.0, 0.0 }, { 1.0, 0.3, 0.3 }, 80.0 };

		VectorCopy(e->origin, l.origin);
		cgi.AddLight(&l);

		VectorMA(e->shell, 0.5, l.color, e->shell);
	}

	if (e->effects & EF_CTF_BLUE) {
		r_light_t l = { { 0.0, 0.0, 0.0 }, { 0.3, 0.3, 1.0 }, 80.0 };

		VectorCopy(e->origin, l.origin);
		cgi.AddLight(&l);

		VectorMA(e->shell, 0.5, l.color, e->shell);
	}

	if (e->effects & EF_CTF_GREEN) {
		r_light_t l = { { 0.0, 0.0, 0.0 }, { 0.3, 1.0, 0.3 }, 80.0 };

		VectorCopy(e->origin, l.origin);
		cgi.AddLight(&l);

		VectorMA(e->shell, 0.5, l.color, e->shell);
	}

	if (e->effects & EF_CTF_ORANGE) {
		r_light_t l = { { 0.0, 0.0, 0.0 }, { 1.0, 0.7, 0.1 }, 80.0 };

		VectorCopy(e->origin, l.origin);
		cgi.AddLight(&l);

		VectorMA(e->shell, 0.5, l.color, e->shell);
	}

	if (VectorNormalize(e->shell) > 0.0) {
		e->effects |= EF_SHELL;
	}

	if (e->effects & EF_DESPAWN) {

		if (!(ent->prev.effects & EF_DESPAWN)) {
			ent->timestamp = cgi.client->unclamped_time;
		}

		e->effects |= (EF_BLEND | EF_NO_LIGHTING);
		e->color[3] = 1.0 - ((cgi.client->unclamped_time - ent->timestamp) / 3000.0);
	}

	if (e->effects & EF_LIGHT) {
		r_light_t l;

		VectorCopy(e->origin, l.origin);
		cgi.ColorFromPalette(ent->current.client, l.color);
		l.radius = ent->current.termination[0];

		cgi.AddLight(&l);
	}

	if (ent->current.trail == TRAIL_ROCKET) {
		e->effects |= EF_NO_SHADOW;
	}
}
