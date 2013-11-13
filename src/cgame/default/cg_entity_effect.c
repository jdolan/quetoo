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
 * @brief
 */
void Cg_InactiveEffect(cl_entity_t *ent, const vec3_t org) {

	if (!IS_SELF(ent) || cg_third_person->value) {
		cg_particle_t *p;

		if (!(p = Cg_AllocParticle(PARTICLE_NORMAL, cg_particles_inactive)))
			return;

		p->part.color = 11;

		p->part.alpha = 1.0;
		p->alpha_vel = -99999999.0;

		p->part.scale = 10.0;

		VectorCopy(org, p->part.org);
		p->part.org[2] += 50.0;
	}
}

/*
 * @brief Processes the specified entity's effects mask, augmenting the given renderer
 * entity and adding additional effects such as particle trails to the view.
 */
void Cg_EntityEffects(cl_entity_t *ent, r_entity_t *e) {

	e->effects = ent->current.effects;

	if (e->effects & EF_ROTATE) {
		e->angles[YAW] = cgi.client->time / 3.4;
	}

	if (e->effects & EF_BOB) {
		e->origin[2] += 4.0 * sin((cgi.client->time * 0.005) + e->origin[0] + e->origin[1]);
	}

	if (e->effects & EF_INACTIVE) {
		Cg_InactiveEffect(ent, e->origin);
	}

	if (e->effects & EF_RESPAWN) {
		const vec3_t color = { 0.5, 0.5, 0.0 };

		VectorMA(e->shell, 0.5, color, e->shell);
	}

	if (e->effects & EF_QUAD) {
		r_light_t l = { { 0.0, 0.0, 0.0 }, 80.0, { 0.3, 0.7, 0.7 } };

		VectorCopy(e->origin, l.origin);
		cgi.AddLight(&l);

		VectorMA(e->shell, 0.5, l.color, e->shell);
	}

	if (e->effects & EF_CTF_BLUE) {
		r_light_t l = { { 0.0, 0.0, 0.0 }, 80.0, { 0.3, 0.3, 1.0 } };

		VectorCopy(e->origin, l.origin);
		cgi.AddLight(&l);

		VectorMA(e->shell, 0.5, l.color, e->shell);
	}

	if (e->effects & EF_CTF_RED) {
		r_light_t l = { { 0.0, 0.0, 0.0 }, 80.0, { 1.0, 0.3, 0.3 } };

		VectorCopy(e->origin, l.origin);
		cgi.AddLight(&l);

		VectorMA(e->shell, 0.5, l.color, e->shell);
	}

	VectorNormalize(e->shell);

	if (e->effects & EF_DESPAWN) {

		if (!(ent->prev.effects & EF_DESPAWN)) {
			ent->time = cgi.client->time;
		}

		e->effects |= EF_BLEND;
		e->alpha = 1.0 - (cgi.client->time - ent->time) / 3000.0;
	}
}
