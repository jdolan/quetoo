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

#ifndef __CG_TYPES_H__
#define __CG_TYPES_H__

#include "cgame/cgame.h"
#include "game/default/g_types.h"

#ifdef __CG_LOCAL_H__

typedef struct cg_particle_s {
	r_particle_t part; // the r_particle_t to add to the view

	// common particle components
	uint32_t time; // client time when allocated
	vec3_t vel;
	vec3_t accel;
	vec4_t color_vel;
	vec_t scale_vel;

	// particle type specific
	union {
		struct {
			vec_t end_z; // weather particles are freed at this Z
		} weather;

		struct {
			vec_t radius;
			vec_t flicker;
		} corona;

		struct {
			vec_t length;
		} spark;
	};

	struct cg_particle_s *prev; // previous particle in the chain
	struct cg_particle_s *next; // next particle in chain
} cg_particle_t;

// particles are chained by image
typedef struct cg_particles_s {
	const r_image_t *original_image; // the image that we passed to it initially
	const r_image_t *image; // the loaded atlas image
	cg_particle_t *particles;
	struct cg_particles_s *next;
} cg_particles_t;

#endif /* __CG_LOCAL_H__ */

#endif /* __CG_TYPES_H__ */

