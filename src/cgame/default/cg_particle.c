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

static cg_particle_t *cg_free_particles; // list of free particles
static cg_particle_t *cg_active_particles; // list of active particles

static cg_particle_t cg_particles[MAX_PARTICLES];

/**
 * @brief Pushes the particle onto the head of specified list.
 */
static void Cg_PushParticle(cg_particle_t *p, cg_particle_t **list) {

	p->prev = NULL;

	if (*list) {
		(*list)->prev = p;
	}

	p->next = *list;
	*list = p;
}

/**
 * @brief Pops the particle from the specified list, repairing the list if it
 * becomes broken.
 */
static void Cg_PopParticle(cg_particle_t *p, cg_particle_t **list) {

	if (p->prev) {
		p->prev->next = p->next;
	}

	if (p->next) {
		p->next->prev = p->prev;
	}

	if (*list == p) {
		*list = p->next;
	}

	p->prev = p->next = NULL;
}

/**
 * @brief Allocates a free particle with the specified type and image.
 */
cg_particle_t *Cg_AllocParticle() {

	if (!cg_add_particles->integer) {
		return NULL;
	}

	if (!cg_free_particles) {
		cgi.Debug("No free particles\n");
		return NULL;
	}

	cg_particle_t *p = cg_free_particles;

	Cg_PopParticle(p, &cg_free_particles);

	memset(p, 0, sizeof(cg_particle_t));

	p->color = color_white;
	p->size = 1.0;

	p->time = p->timestamp = cgi.client->unclamped_time;

	Cg_PushParticle(p, &cg_active_particles);

	return p;
}

/**
 * @brief Frees the specified particle, returning the particle it was pointing
 * to as a convenience for continued iteration.
 */
cg_particle_t *Cg_FreeParticle(cg_particle_t *p) {
	cg_particle_t *next = p->next;

	Cg_PopParticle(p, &cg_active_particles);

	Cg_PushParticle(p, &cg_free_particles);

	return next;
}

/**
 * @brief Frees all particles, returning them to the eligible list.
 */
void Cg_FreeParticles(void) {

	cg_free_particles = NULL;
	cg_active_particles = NULL;

	memset(cg_particles, 0, sizeof(cg_particles));

	for (size_t i = 0; i < lengthof(cg_particles); i++) {
		Cg_PushParticle(&cg_particles[i], &cg_free_particles);
	}
}

/**
 * @brief Slide off of the impacted plane.
 */
static vec3_t Cg_ClipVelocity(const vec3_t in, const vec3_t normal, float bounce) {

	float backoff = Vec3_Dot(in, normal);

	if (backoff < 0.0) {
		backoff *= bounce;
	} else {
		backoff /= bounce;
	}

	return Vec3_Subtract(in, Vec3_Scale(normal, backoff));
}

/**
 * @brief Adds all particles that are active for this frame to the view.
 */
void Cg_AddParticles(void) {

	if (!cg_add_particles->integer) {
		return;
	}

	const float delta = MILLIS_TO_SECONDS(cgi.client->frame_msec);

	cg_particle_t *p = cg_active_particles;
	while (p) {

		if (p->time != cgi.client->unclamped_time) {
			if (cgi.client->unclamped_time - p->time > p->lifetime) {
				p = Cg_FreeParticle(p);
				continue;
			}
		}

		const vec3_t old_origin = p->origin;

		p->velocity = Vec3_Add(p->velocity, Vec3_Scale(p->acceleration, delta));
		p->origin = Vec3_Add(p->origin, Vec3_Scale(p->velocity, delta));

		if (p->bounce && cg_particle_quality->integer) {
			const cm_trace_t tr = cgi.Trace(old_origin, p->origin, Vec3_Zero(), Vec3_Zero(), 0, CONTENTS_MASK_SOLID);
			if (tr.fraction < 1.0) {
				p->velocity = Cg_ClipVelocity(p->velocity, tr.plane.normal, p->bounce);
				p->origin = tr.end;
			}
		}

		p->color_velocity = Vec4_Add(p->color_velocity, Vec4_Scale(p->color_acceleration, delta));
		p->color = Color4fv(Vec4_Add(Color_Vec4(p->color), Vec4_Scale(p->color_velocity, delta)));

		if (p->color.a <= 0) {
			p = Cg_FreeParticle(p);
			continue;
		}

		p->size_velocity += p->size_acceleration * delta;
		p->size += p->size_velocity * delta;

		if (p->size <= 0.f) {
			p = Cg_FreeParticle(p);
			continue;
		}

		cgi.AddParticle(&(r_particle_t) {
			.origin = p->origin,
			.size = p->size,
			.color = p->color
		});
		
		p = p->next;
	}
}
