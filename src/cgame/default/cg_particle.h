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

#define PARTICLE_GRAVITY 180.0

#define CORONA_SCALE(radius, flicker) \
	((radius) + ((radius) * (flicker) * sin(0.09 * cgi.client->unclamped_time)))

cg_particle_t *Cg_AllocParticle(const r_particle_type_t type, cg_particles_t *particles, const _Bool force);
cg_particles_t *Cg_AllocParticles(const char *name, r_image_type_t type, _Bool use_atlas);
void Cg_InitParticles(void);
void Cg_CompileParticleAtlas(void);
void Cg_FreeParticles(void);
void Cg_AddParticles(void);
#endif /* __CG_LOCAL_H__ */
