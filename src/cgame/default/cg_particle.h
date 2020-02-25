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

#pragma once

#ifdef __CG_LOCAL_H__

#define PARTICLE_GRAVITY 180.f

/**
 * @brief Client game particles can persist over multiple frames.
 */
typedef struct cg_particle_s {

	/**
	 * @brief The particle origin.
	 */
	vec3_t origin;

	/**
	 * @brief The particle velocity.
	 */
	vec3_t velocity;

	/**
	 * @brief The particle acceleration.
	 */
	vec3_t acceleration;

	/**
	 * @brief The particle color.
	 */
	color_t color;

	/**
	 * @brief The particle color velocity.
	 */
	vec4_t color_velocity;

	/**
	 * @brief The particle color acceleration.
	 */
	vec4_t color_acceleration;

	/**
	 * @brief The particle size, in world units.
	 */
	float size;

	/**
	 * @brief The particle size velocity.
	 */
	float size_velocity;

	/**
	 * @brief The particle size acceleration.
	 */
	float size_acceleration;

	/**
	 * @brief Collide with solids.
	 */
	float bounce;

	/**
	 * @brief The client time when the particle was allocated.
	 */
	uint32_t time;

	/**
	 * @brief The lifetime, after which point it decays.
	 */
	uint32_t lifetime;

	/**
	 * @brief The time when this particle was last updated.
	 */
	uint32_t timestamp;

	struct cg_particle_s *prev;
	struct cg_particle_s *next;
} cg_particle_t;

cg_particle_t *Cg_AllocParticle(void);
cg_particle_t *Cg_FreeParticle(cg_particle_t *p);
void Cg_FreeParticles(void);
void Cg_AddParticles(void);
#endif /* __CG_LOCAL_H__ */
