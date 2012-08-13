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

#ifndef __CG_PARTICLE_H__
#define __CG_PARTICLE_H__

#ifdef __CG_LOCAL_H__

typedef struct cg_particle_s {
	r_particle_t part; // the r_particle_t to add to the view
	vec3_t vel;
	vec3_t accel;
	float alpha_vel;
	float scale_vel;
	float end_z; // weather particles are freed at this Z
	uint32_t time; // client time when allocated
	struct cg_particle_s *prev; // previous particle in the chain
	struct cg_particle_s *next; // next particle in chain
} cg_particle_t;

cg_particle_t *Cg_AllocParticle(const uint16_t type, const r_image_t *image);
void Cg_FreeParticles(void);
void Cg_AddParticles(void);
#endif /* __CG_LOCAL_H__ */

#endif /* __CG_PARTICLE_H__ */
