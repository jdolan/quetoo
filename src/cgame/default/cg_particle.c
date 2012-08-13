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

static cg_particle_t *cg_free_particles;
static cg_particle_t *cg_active_particles[MAX_GL_TEXTURES];

static cg_particle_t cg_particles[MAX_PARTICLES];

/*
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

/*
 * @brief Pops the particle from the specified list, repairing the list if it
 * becomes broken.
 */
static void Cg_PopParticle(cg_particle_t *p, cg_particle_t **list) {

	if (*list == p) {
		*list = p->next;

		if (*list) {
			(*list)->prev = NULL;
		}
	} else if (p->prev) {
		p->prev->next = p->next;
	}

	p->prev = p->next = NULL;
}

/*
 * @brief Allocates a free particle with the specified type and image.
 */
cg_particle_t *Cg_AllocParticle(const uint16_t type, const r_image_t *image) {

	if (!cg_add_particles->integer)
		return NULL ;

	if (!type || !image) {
		cgi.Debug("Cg_AllocParticle: Bad type or image\n");
		return NULL ;
	}

	if (!cg_free_particles) {
		cgi.Debug("Cg_AllocParticle: No free particles\n");
		return NULL ;
	}

	cg_particle_t *p = cg_free_particles;

	Cg_PopParticle(p, &cg_free_particles);

	p->part.type = type;
	p->part.image = image ? image : cg_particle_normal;

	Cg_PushParticle(p, &cg_active_particles[p->part.image->texnum]);

	p->time = cgi.client->time;

	return p;
}

/*
 * @brief Frees the specified particle, returning the particle it was pointing
 * to as a convenience for continued iteration.
 */
static cg_particle_t *Cg_FreeParticle(cg_particle_t *p, cg_particle_t **list) {
	cg_particle_t *next = p->next;

	if (list) {
		Cg_PopParticle(p, list);
	}

	memset(p, 0, sizeof(cg_particle_t));

	p->part.blend = GL_ONE;
	p->part.alpha = 1.0;
	p->part.scale = 1.0;

	Cg_PushParticle(p, &cg_free_particles);

	/*int32_t i = 0;
	while (p) {
		i++;
		p = p->next;
	}
	printf("FREE PARTICLES %d\n", i);*/

	return next;
}

/*
 * @brief Frees all particles, returning them to the eligible list.
 */
void Cg_FreeParticles(void) {
	int32_t i;

	cg_free_particles = NULL;
	memset(cg_active_particles, 0, sizeof(cg_active_particles));

	memset(cg_particles, 0, sizeof(cg_particles));

	for (i = 0; i < MAX_PARTICLES; i++) {
		Cg_FreeParticle(&cg_particles[i], NULL );
	}
}

/*
 * @brief Adds all particles that are active for this frame to the view.
 * Particles that fade or shrink beyond visibility are freed.
 */
void Cg_AddParticles(void) {
	static uint32_t last_particle_time;
	int32_t i;

	if (!cg_add_particles->value)
		return;

	if (last_particle_time > cgi.client->time)
		last_particle_time = 0;

	const float delta = (cgi.client->time - last_particle_time) * 0.001;
	const float delta_squared = delta * delta;

	last_particle_time = cgi.client->time;

	for (i = 0; i < MAX_GL_TEXTURES; i++) {
		cg_particle_t *p = cg_active_particles[i];

		while (p) {
			// update any particles allocated in previous frames
			if (p->time != cgi.client->time) {
				int32_t j;

				p->part.alpha += delta * p->alpha_vel;
				p->part.scale += delta * p->scale_vel;

				// free up particles that have disappeared
				if (p->part.alpha <= 0.0 || p->part.scale <= 0.0) {
					p = Cg_FreeParticle(p, &cg_active_particles[i]);
					continue;
				}

				for (j = 0; j < 3; j++) { // update origin, end, and acceleration
					p->part.org[j] += p->vel[j] * delta + p->accel[j] * delta_squared;
					p->part.end[j] += p->vel[j] * delta + p->accel[j] * delta_squared;

					p->vel[j] += p->accel[j] * delta;
				}

				// free up weather particles that have hit the ground
				if (p->part.type == PARTICLE_WEATHER && (p->part.org[2] <= p->end_z)) {
					p = Cg_FreeParticle(p, &cg_active_particles[i]);
					continue;
				}
			}

			cgi.AddParticle(&p->part);
			p = p->next;
		}
	}
}
