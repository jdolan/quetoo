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
 * @brief
 */
void Cg_InactiveEffect(cl_entity_t *ent, const vec3_t org) {
	cg_particle_t *p;

	if (Cg_IsSelf(ent) && !cg_third_person->value) {
		return;
	}

	if (!(p = Cg_AllocParticle(PARTICLE_NORMAL, cg_particles_inactive))) {
		return;
	}

	cgi.ColorFromPalette(11, p->part.color);
	Vector4Set(p->color_vel, 0.0, 0.0, 0.0, -9999.0);

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
		e->angles[YAW] = cgi.client->systime / M_PI;
	}

	if (e->effects & EF_BOB) {
		e->origin[2] += 4.0 * sin(cgi.client->systime * 0.005 + ent->current.number);
	}

	if (e->effects & EF_PULSE) {
		const vec_t v = 0.4 + (cos(cgi.client->systime * 0.005 + ent->current.number) + 1.0) * 0.6;
		VectorSet(e->color, v, v, v);
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

	if (e->effects & EF_CTF_BLUE) {
		r_light_t l = { { 0.0, 0.0, 0.0 }, { 0.3, 0.3, 1.0 }, 80.0 };

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

	if (VectorNormalize(e->shell) > 0.0) {
		e->effects |= EF_SHELL;
	}

	if (e->effects & EF_DESPAWN) {

		if (!(ent->prev.effects & EF_DESPAWN)) {
			ent->time = cgi.client->systime;
		}

		e->effects |= (EF_BLEND | EF_NO_LIGHTING);
		e->color[3] = 1.0 - ((cgi.client->systime - ent->time) / 3000.0);
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
