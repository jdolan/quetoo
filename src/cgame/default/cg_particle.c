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

static r_particle_t *cg_active_particles, *cg_free_particles;
static r_particle_t cg_particles[MAX_PARTICLES];

/*
 * Cg_AllocParticle
 */
r_particle_t *Cg_AllocParticle(void) {
	r_particle_t *p;

	if (!cg_free_particles)
		return NULL;

	p = cg_free_particles;
	cg_free_particles = p->next;
	p->next = cg_active_particles;
	cg_active_particles = p;

	p->time = cgi.client->time;
	return p;
}

/*
 * Cg_FreeParticle
 */
static void Cg_FreeParticle(r_particle_t *p) {

	if (p->type == PARTICLE_WEATHER)
		cg_num_weather_particles--;

	memset(p, 0, sizeof(r_particle_t));

	p->type = PARTICLE_NORMAL;
	p->image = cg_particle_normal;
	p->scale = 1.0;
	p->blend = GL_ONE;

	p->next = cg_free_particles;
	cg_free_particles = p;
}

/*
 * Cg_FreeParticles
 */
void Cg_FreeParticles(void) {
	int32_t i;

	for (i = 0; i < MAX_PARTICLES; i++) {
		Cg_FreeParticle(&cg_particles[i]);
		cg_particles[i].next = i < MAX_PARTICLES - 1 ? &cg_particles[i + 1] : NULL;
	}

	cg_free_particles = &cg_particles[0];
	cg_active_particles = NULL;

	cg_num_weather_particles = 0;
}

/*
 * Cg_AddParticles
 */
void Cg_AddParticles(void) {
	r_particle_t *p, *next;
	r_particle_t *active, *tail;
	float time;
	int32_t i;

	// add weather effects after all other effects for the frame
	Cg_WeatherEffects();

	active = NULL;
	tail = NULL;

	for (p = cg_active_particles; p; p = next) {
		next = p->next;

		time = (cgi.client->time - p->time) * 0.001;

		p->current_alpha = p->alpha + time * p->alpha_vel;
		p->current_scale = p->scale + time * p->scale_vel;

		// free up particles that have faded or shrunk
		if (p->current_alpha <= 0 || p->current_scale <= 0) {
			Cg_FreeParticle(p);
			continue;
		}

		if (p->current_alpha > 1.0) // clamp alpha
			p->current_alpha = 1.0;

		for (i = 0; i < 3; i++) { // update origin and end
			p->current_org[i] = p->org[i] + p->vel[i] * time + p->accel[i]
					* time * time;
			p->current_end[i] = p->end[i] + p->vel[i] * time + p->accel[i]
					* time * time;
		}

		// free up weather particles that have hit the ground
		if (p->type == PARTICLE_WEATHER && (p->current_org[2] <= p->end[2])) {
			Cg_FreeParticle(p);
			continue;
		}

		p->next = NULL;
		if (!tail)
			active = tail = p;
		else {
			tail->next = p;
			tail = p;
		}

		cgi.AddParticle(p);
	}

	cg_active_particles = active;
}
