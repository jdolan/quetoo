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

#include "cg_types.h"

#ifdef __CG_LOCAL_H__
extern s_sample_t *cg_sample_blaster_fire;
extern s_sample_t *cg_sample_blaster_hit;
extern s_sample_t *cg_sample_shotgun_fire;
extern s_sample_t *cg_sample_supershotgun_fire;
extern s_sample_t *cg_sample_machinegun_fire[4];
extern s_sample_t *cg_sample_machinegun_hit[3];
extern s_sample_t *cg_sample_grenadelauncher_fire;
extern s_sample_t *cg_sample_rocketlauncher_fire;
extern s_sample_t *cg_sample_hyperblaster_fire;
extern s_sample_t *cg_sample_hyperblaster_hit;
extern s_sample_t *cg_sample_lightning_fire;
extern s_sample_t *cg_sample_lightning_discharge;
extern s_sample_t *cg_sample_railgun_fire;
extern s_sample_t *cg_sample_bfg_fire;
extern s_sample_t *cg_sample_bfg_hit;

extern s_sample_t *cg_sample_explosion;
extern s_sample_t *cg_sample_teleport;
extern s_sample_t *cg_sample_respawn;
extern s_sample_t *cg_sample_sparks;

extern s_sample_t *cg_sample_rain;
extern s_sample_t *cg_sample_snow;
extern s_sample_t *cg_sample_underwater;
extern s_sample_t *cg_sample_hits[2];
extern s_sample_t *cg_sample_gib;

s_sample_t *Cg_GetFootstepSample(const char *footsteps);

extern cg_particles_t *cg_particles_default;
extern cg_particles_t *cg_particles_corona;
extern cg_particles_t *cg_particles_explosion;
extern cg_particles_t *cg_particles_debris[4];
extern cg_particles_t *cg_particles_teleporter;
extern cg_particles_t *cg_particles_smoke;
extern cg_particles_t *cg_particles_steam;
extern cg_particles_t *cg_particles_bubble;
extern cg_particles_t *cg_particles_rain;
extern cg_particles_t *cg_particles_snow;
extern cg_particles_t *cg_particles_beam;
extern cg_particles_t *cg_particles_rail_wake;
extern cg_particles_t *cg_particles_tracer;
extern cg_particles_t *cg_particles_blood;
extern cg_particles_t *cg_particles_lightning;
extern cg_particles_t *cg_particles_rope;
extern cg_particles_t *cg_particles_flame;
extern cg_particles_t *cg_particles_spark;
extern cg_particles_t *cg_particles_inactive;
extern cg_particles_t *cg_particles_ripple[3];

extern cg_particles_t *cg_particles_stain_burn;
extern cg_particles_t *cg_particles_lightning_burn;
extern cg_particles_t *cg_particles_blood_burn;

void Cg_UpdateMedia(void);
#endif /* __CG_LOCAL_H__ */
