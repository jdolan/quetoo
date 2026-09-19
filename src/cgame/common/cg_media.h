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

#if defined(__CG_LOCAL_H__)
extern SoundSample *cg_sample_blaster_fire;
extern SoundSample *cg_sample_blaster_hit;
extern SoundSample *cg_sample_shotgun_fire;
extern SoundSample *cg_sample_supershotgun_fire;
extern SoundSample *cg_sample_machinegun_fire[3];
extern SoundSample *cg_sample_machinegun_hit[3];
extern SoundSample *cg_sample_grenadelauncher_fire;
extern SoundSample *cg_sample_rocketlauncher_fire;
extern SoundSample *cg_sample_hyperblaster_fire;
extern SoundSample *cg_sample_hyperblaster_hit;
extern SoundSample *cg_sample_lightning_fire;
extern SoundSample *cg_sample_laser_fire;
extern SoundSample *cg_sample_lightning_discharge;
extern SoundSample *cg_sample_railgun_fire;
extern SoundSample *cg_sample_bfg_fire;
extern SoundSample *cg_sample_bfg_hit;

#if defined(G_HOOK)
extern SoundSample *cg_sample_hook_hit;
#endif

extern SoundSample *cg_sample_quake_shotgun_fire;
extern SoundSample *cg_sample_quake_supershotgun_fire;
extern SoundSample *cg_sample_quake_nailgun_fire;
extern SoundSample *cg_sample_quake_supernailgun_fire;
extern SoundSample *cg_sample_quake_nail_hit;
extern SoundSample *cg_sample_quake_grenadelauncher_fire;
extern SoundSample *cg_sample_quake_rocketlauncher_fire;

extern SoundSample *cg_sample_explosion;
extern SoundSample *cg_sample_teleport;
extern SoundSample *cg_sample_respawn;
extern SoundSample *cg_sample_sparks;
extern SoundSample *cg_sample_fire;
extern SoundSample *cg_sample_steam;

extern SoundSample *cg_sample_rain;
extern SoundSample *cg_sample_snow;
extern SoundSample *cg_sample_ash;
extern SoundSample *cg_sample_underwater;
extern SoundSample *cg_sample_hits[2];
extern SoundSample *cg_sample_gib;

extern RenderAtlasImage *cg_sprite_particle;
extern RenderAtlasImage *cg_sprite_particle2;
extern RenderAtlasImage *cg_sprite_particle3;
extern RenderAtlasImage *cg_sprite_flash;
extern RenderAtlasImage *cg_sprite_ring;
extern RenderAtlasImage *cg_sprite_blaster_flash;
extern RenderAtlasImage *cg_sprite_aniso_flare_01;
extern RenderAtlasImage *cg_sprite_rain;
extern RenderAtlasImage *cg_sprite_snow;
extern RenderAtlasImage *cg_sprite_ash;
extern RenderAtlasImage *cg_sprite_smoke;
extern RenderAtlasImage *cg_sprite_flame;
extern RenderAtlasImage *cg_sprite_spark;
extern RenderAtlasImage *cg_sprite_bubble;
extern RenderAtlasImage *cg_sprite_teleport;
extern RenderAtlasImage *cg_sprite_teleport_core;
extern RenderAtlasImage *cg_sprite_steam;
extern RenderAtlasImage *cg_sprite_inactive;
extern RenderAtlasImage *cg_sprite_plasma_var01;
extern RenderAtlasImage *cg_sprite_plasma_var02;
extern RenderAtlasImage *cg_sprite_plasma_var03;
extern RenderAtlasImage *cg_sprite_blob_01;
extern RenderAtlasImage *cg_sprite_electro_02;
extern RenderAtlasImage *cg_sprite_explosion_flash;
extern RenderAtlasImage *cg_sprite_explosion_glow;
extern RenderAtlasImage *cg_sprite_splash_02_03;
extern RenderAtlasImage *cg_sprite_impact_spark_01_dot;
extern RenderAtlasImage *cg_sprite_puff_cloud;
extern RenderAtlasImage *cg_sprite_water_circle;
extern RenderAtlasImage *cg_sprite_water_ring;
extern RenderAtlasImage *cg_sprite_water_ring2;
extern RenderAtlasImage *cg_sprite_abstract_01;
extern RenderAtlasImage *cg_sprite_node_wait;
extern RenderAtlasImage *cg_sprite_node_slow;

extern RenderImage *cg_beam_hook;
extern RenderImage *cg_beam_arrow;
extern RenderImage *cg_beam_line;
extern RenderImage *cg_beam_rail;
extern RenderImage *cg_beam_lightning;
extern RenderImage *cg_beam_tracer;
extern RenderImage *cg_beam_tail;

extern RenderAnimation *cg_sprite_explosion;
extern RenderAnimation *cg_sprite_explosion_ring_02;
extern RenderAnimation *cg_sprite_rocket_flame;
extern RenderAnimation *cg_sprite_blaster_flame;
extern RenderAnimation *cg_sprite_smoke_04;
extern RenderAnimation *cg_sprite_smoke_05;
extern RenderAnimation *cg_sprite_blaster_ring;
extern RenderAnimation *cg_bfg_explosion_1;
extern RenderAnimation *cg_sprite_bfg_explosion_2;
extern RenderAnimation *cg_sprite_bfg_explosion_3;
extern RenderAnimation *cg_sprite_poof_01;
extern RenderAnimation *cg_sprite_poof_02;
extern RenderAnimation *cg_sprite_blood_01;
extern RenderAnimation *cg_sprite_electro_01;
extern RenderAnimation *cg_sprite_fireball_01;
extern RenderAnimation *cg_sprite_impact_spark_01;
extern RenderAnimation *cg_sprite_hyperball_01;
extern RenderAnimation *cg_sprite_fizz_01;

extern RenderAtlasImage *cg_decal_bullet[3];
extern RenderAtlasImage *cg_decal_blood[4];
extern RenderAtlasImage *cg_decal_burn[4];
extern RenderAtlasImage *cg_decal_slug[4];

extern Framebuffer *cg_framebuffer;

void Cg_CreateFramebuffer(void);
void Cg_DestroyFramebuffer(void);
void Cg_LoadMedia(void);
void Cg_FreeMedia(void);
#endif
