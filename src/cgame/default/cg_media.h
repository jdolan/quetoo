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
extern s_sample_t *cg_sample_fire;
extern s_sample_t *cg_sample_steam;

extern s_sample_t *cg_sample_rain;
extern s_sample_t *cg_sample_snow;
extern s_sample_t *cg_sample_underwater;
extern s_sample_t *cg_sample_hits[2];
extern s_sample_t *cg_sample_gib;

s_sample_t *Cg_GetFootstepSample(const char *footsteps);

extern r_atlas_image_t *cg_sprite_particle;
extern r_atlas_image_t *cg_sprite_particle2;
extern r_atlas_image_t *cg_sprite_flash;
extern r_atlas_image_t *cg_sprite_ring;
extern r_atlas_image_t *cg_sprite_aniso_flare_01;
extern r_atlas_image_t *cg_sprite_rain;
extern r_atlas_image_t *cg_sprite_snow;
extern r_atlas_image_t *cg_sprite_smoke;
extern r_atlas_image_t *cg_sprite_flame;
extern r_atlas_image_t *cg_sprite_spark;
extern r_atlas_image_t *cg_sprite_bubble;
extern r_atlas_image_t *cg_sprite_teleport;
extern r_atlas_image_t *cg_sprite_steam;
extern r_atlas_image_t *cg_sprite_inactive;
extern r_atlas_image_t *cg_sprite_plasma_var01;
extern r_atlas_image_t *cg_sprite_plasma_var02;
extern r_atlas_image_t *cg_sprite_plasma_var03;
extern r_atlas_image_t *cg_sprite_blob_01;
extern r_atlas_image_t *cg_sprite_electro_02;
extern r_atlas_image_t *cg_sprite_explosion_flash;
extern r_atlas_image_t *cg_sprite_explosion_glow;
extern r_atlas_image_t *cg_sprite_splash_02_03;
extern r_image_t *cg_beam_hook;
extern r_image_t *cg_beam_arrow;
extern r_image_t *cg_beam_line;
extern r_image_t *cg_beam_rail;
extern r_image_t *cg_beam_lightning;
extern r_image_t *cg_beam_tracer;
extern r_image_t *cg_sprite_blaster_flash;
extern r_animation_t *cg_sprite_explosion;
extern r_animation_t *cg_sprite_explosion_ring_02;
extern r_animation_t *cg_sprite_rocket_flame;
extern r_animation_t *cg_sprite_blaster_flame;
extern r_animation_t *cg_sprite_smoke_04;
extern r_animation_t *cg_sprite_smoke_05;
extern r_animation_t *cg_sprite_blaster_ring;
extern r_animation_t *cg_sprite_hyperblaster;
extern r_animation_t *cg_bfg_explosion_1;
extern r_animation_t *cg_sprite_bfg_explosion_2;
extern r_animation_t *cg_sprite_bfg_explosion_3;
extern r_animation_t *cg_sprite_poof_01;
extern r_animation_t *cg_sprite_poof_02;
extern r_animation_t *cg_sprite_blood_01;
extern r_animation_t *cg_sprite_electro_01;
extern r_animation_t *cg_sprite_fireball_01;

extern r_atlas_image_t cg_sprite_font[16 * 8];

extern int32_t cg_sprite_font_width, cg_sprite_font_height;

void Cg_UpdateMedia(void);
#endif /* __CG_LOCAL_H__ */
