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

	if (!color.a) {
		return default_color;
	}

	return color;
}

/**
 * @brief
 */
static void Cg_InactiveEffect(cl_entity_t *ent, const vec3_t org) {
	cg_particle_t *p;

	if (Cg_IsSelf(ent) && !cgi.client->third_person) {
		return;
	}

	if (!(p = Cg_AllocParticle())) {
		return;
	}

	p->origin = org;
	p->origin.z += 50.f;

	p->color = color_white;
	p->size = 10.0;
}

/**
 * @brief Processes the entity's effects mask, augmenting the renderer entity.
 */
void Cg_EntityEffects(cl_entity_t *ent, r_entity_t *e) {

	e->effects = ent->current.effects;

	if (e->effects & EF_ROTATE) {
		const float rotate = cgi.client->unclamped_time;
		e->angles.y = cg_entity_rotate->value * rotate / M_PI;
	}

	if (e->effects & EF_BOB) {
		e->termination = e->origin;
		const float bob = sinf(cgi.client->unclamped_time * 0.005f + ent->current.number);
		e->origin.z += cg_entity_bob->value * bob;
	}

	if (e->effects & EF_PULSE) {
		/* FIXME: temporarily disabled here to see how pickups look without pulse
		const float pulse = (cosf(cgi.client->unclamped_time * 0.0033f + ent->current.number) + 1.f);
		const float c = 1.f - (cg_entity_pulse->value * 0.5f * pulse);
		e->color = Vec4(c, c, c, 1.f);
		*/
		e->color = Vec4(1.f, 1.f, 1.f, 1.f);
	} else {
		e->color = Vec4(1.f, 1.f, 1.f, 1.f);
	}

	if (e->effects & EF_INACTIVE) {
		Cg_InactiveEffect(ent, e->origin);
	}

	if (e->effects & EF_RESPAWN) {
		const vec3_t color = Vec3(0.5f, 0.5f, 0.f);
		e->shell = Vec3_Add(e->shell, Vec3_Scale(color, 0.5));
	}

	if (e->effects & EF_QUAD) {
		const cg_light_t l = {
			.origin = e->origin,
			.radius = 80.0,
			.color = Vec3(0.3f, 0.7f, 0.7f)
		};

		Cg_AddLight(&l);

		e->shell = Vec3_Add(e->shell, Vec3_Scale(l.color, 0.5));
	}

	if (e->effects & EF_CTF_RED) {
		const cg_light_t l = {
			.origin = e->origin,
			.radius = 80.f,
			.color = Vec3(1.f, .3f, .3f)
		};

		Cg_AddLight(&l);

		e->shell = Vec3_Add(e->shell, Vec3_Scale(l.color, 0.5));
	}

	if (e->effects & EF_CTF_BLUE) {
		const cg_light_t l = {
			.origin = e->origin,
			.radius = 80.0,
			.color = Vec3(0.3f, 0.3f, 1.f)
		};

		Cg_AddLight(&l);

		e->shell = Vec3_Add(e->shell, Vec3_Scale(l.color, 0.5));
	}

	if (e->effects & EF_CTF_GREEN) {
		const cg_light_t l = {
			.origin =  e->origin,
			.radius = 80.0,
			.color = Vec3(0.3f, 1.0f, 0.3f)
		};

		Cg_AddLight(&l);

		e->shell = Vec3_Add(e->shell, Vec3_Scale(l.color, 0.5));
	}

	if (e->effects & EF_CTF_ORANGE) {
		const cg_light_t l = {
			.origin = e->origin,
			.radius = 80.0,
			.color = Vec3(1.f, 0.7f, 0.1f)
		};

		Cg_AddLight(&l);

		e->shell = Vec3_Add(e->shell, Vec3_Scale(l.color, 0.5));
	}

	if (Vec3_Length(e->shell) > 0.0) {
		e->shell = Vec3_Normalize(e->shell);
		e->effects |= EF_SHELL;
	}

	if (e->effects & EF_DESPAWN) {

		if (!(ent->prev.effects & EF_DESPAWN)) {
			ent->timestamp = cgi.client->unclamped_time;
		}

		e->effects |= (EF_BLEND | EF_NO_SHADOW);
		e->color.w = 1.f - ((cgi.client->unclamped_time - ent->timestamp) / 3000.f);
	}

	if (e->effects & EF_LIGHT) {

		const color_t color = cgi.ColorFromPalette(ent->current.client);

		const cg_light_t l = {
			.origin = e->origin,
			.radius = ent->current.termination.x,
			.color = Color_Vec3(color)
		};

		Cg_AddLight(&l);
	}

	if (ent->current.trail == TRAIL_ROCKET) {
		e->effects |= EF_NO_SHADOW;
	}
}
