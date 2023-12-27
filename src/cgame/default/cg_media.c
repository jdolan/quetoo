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

s_sample_t *cg_sample_blaster_fire;
s_sample_t *cg_sample_blaster_hit;
s_sample_t *cg_sample_shotgun_fire;
s_sample_t *cg_sample_supershotgun_fire;
s_sample_t *cg_sample_machinegun_fire[4];
s_sample_t *cg_sample_machinegun_hit[3];
s_sample_t *cg_sample_grenadelauncher_fire;
s_sample_t *cg_sample_rocketlauncher_fire;
s_sample_t *cg_sample_hyperblaster_fire;
s_sample_t *cg_sample_hyperblaster_hit;
s_sample_t *cg_sample_lightning_fire;
s_sample_t *cg_sample_lightning_discharge;
s_sample_t *cg_sample_railgun_fire;
s_sample_t *cg_sample_bfg_fire;
s_sample_t *cg_sample_bfg_hit;
s_sample_t *cg_sample_hook_hit;

s_sample_t *cg_sample_explosion;
s_sample_t *cg_sample_teleport;
s_sample_t *cg_sample_respawn;
s_sample_t *cg_sample_sparks;
s_sample_t *cg_sample_fire;
s_sample_t *cg_sample_steam;

s_sample_t *cg_sample_rain;
s_sample_t *cg_sample_snow;
s_sample_t *cg_sample_underwater;
s_sample_t *cg_sample_hits[2];
s_sample_t *cg_sample_gib;

static r_atlas_t *cg_sprite_atlas;

r_atlas_image_t *cg_sprite_particle;
r_atlas_image_t *cg_sprite_particle2;
r_atlas_image_t *cg_sprite_particle3;
r_atlas_image_t *cg_sprite_flash;
r_atlas_image_t *cg_sprite_ring;
r_atlas_image_t *cg_sprite_aniso_flare_01;
r_atlas_image_t *cg_sprite_rain;
r_atlas_image_t *cg_sprite_snow;
r_atlas_image_t *cg_sprite_bubble;
r_atlas_image_t *cg_sprite_teleport;
r_atlas_image_t *cg_sprite_smoke;
r_atlas_image_t *cg_sprite_flame;
r_atlas_image_t *cg_sprite_explosion_glow;
r_atlas_image_t *cg_sprite_explosion_flash;
r_atlas_image_t *cg_sprite_spark;
r_atlas_image_t *cg_sprite_steam;
r_atlas_image_t *cg_sprite_inactive;
r_atlas_image_t *cg_sprite_plasma_var01;
r_atlas_image_t *cg_sprite_plasma_var02;
r_atlas_image_t *cg_sprite_plasma_var03;
r_atlas_image_t *cg_sprite_blob_01;
r_atlas_image_t *cg_sprite_electro_02;
r_atlas_image_t *cg_sprite_splash_02_03;
r_atlas_image_t *cg_sprite_impact_spark_01_dot;
r_atlas_image_t *cg_sprite_puff_cloud;
r_atlas_image_t *cg_sprite_water_circle;
r_atlas_image_t *cg_sprite_water_ring;
r_atlas_image_t *cg_sprite_water_ring2;
r_atlas_image_t *cg_sprite_abstract_01;
r_atlas_image_t *cg_sprite_node_wait;
r_atlas_image_t *cg_sprite_node_slow;
r_image_t *cg_beam_hook;
r_image_t *cg_beam_arrow;
r_image_t *cg_beam_line;
r_image_t *cg_beam_rail;
r_image_t *cg_beam_lightning;
r_image_t *cg_beam_tracer;
r_image_t *cg_beam_tail;
r_image_t *cg_sprite_blaster_flash;

r_animation_t *cg_sprite_explosion;
r_animation_t *cg_sprite_explosion_ring_02;
r_animation_t *cg_sprite_rocket_flame;
r_animation_t *cg_sprite_blaster_flame;
r_animation_t *cg_sprite_smoke_04;
r_animation_t *cg_sprite_smoke_05;
r_animation_t *cg_sprite_blaster_ring;
r_animation_t *cg_bfg_explosion_1;
r_animation_t *cg_sprite_bfg_explosion_2;
r_animation_t *cg_sprite_bfg_explosion_3;
r_animation_t *cg_sprite_poof_01;
r_animation_t *cg_sprite_poof_02;
r_animation_t *cg_sprite_blood_01;
r_animation_t *cg_sprite_electro_01;
r_animation_t *cg_sprite_fireball_01;
r_animation_t *cg_sprite_impact_spark_01;
r_animation_t *cg_sprite_hyperball_01;

r_framebuffer_t cg_framebuffer;

/**
 * @brief
 */
static r_animation_t *Cg_LoadAnimatedSprite(r_atlas_t *atlas, char *base_path, char *seq_num_fmt, uint32_t first_frame, uint32_t last_frame) {
	assert(last_frame > first_frame);

	// TODO: maybe first check if the paths actually exist?

	char format_path[MAX_QPATH];
	g_snprintf(format_path, sizeof(format_path), "%s%s", base_path, seq_num_fmt);

	char name[MAX_QPATH];
	const uint32_t length = (last_frame - first_frame) + 1;
	const r_image_t *images[length];
	for (uint32_t i = 0; i < length; i++) {
		g_snprintf(name, MAX_QPATH, format_path, i + first_frame);
		images[i] = (r_image_t *) cgi.LoadAtlasImage(atlas, name, IMG_SPRITE);
	}

	return cgi.CreateAnimation(base_path, length, images);
}

/**
 * @brief Updates all media references for the client game.
 */
void Cg_LoadMedia(void) {
	char name[MAX_QPATH];

	cgi.FreeTag(MEM_TAG_CGAME);
	cgi.FreeTag(MEM_TAG_CGAME_LEVEL);

	cgi.LoadingProgress(-1, "sounds");

	cg_sample_blaster_fire = cgi.LoadSample("weapons/blaster/fire");
	cg_sample_blaster_hit = cgi.LoadSample("weapons/blaster/hit");
	cg_sample_shotgun_fire = cgi.LoadSample("weapons/shotgun/fire");
	cg_sample_supershotgun_fire = cgi.LoadSample("weapons/supershotgun/fire");
	cg_sample_grenadelauncher_fire = cgi.LoadSample("weapons/grenadelauncher/fire");
	cg_sample_rocketlauncher_fire = cgi.LoadSample("weapons/rocketlauncher/fire");
	cg_sample_hyperblaster_fire = cgi.LoadSample("weapons/hyperblaster/fire");
	cg_sample_hyperblaster_hit = cgi.LoadSample("weapons/hyperblaster/hit");
	cg_sample_lightning_fire = cgi.LoadSample("weapons/lightning/fire");
	cg_sample_lightning_discharge = cgi.LoadSample("weapons/lightning/discharge");
	cg_sample_railgun_fire = cgi.LoadSample("weapons/railgun/fire");
	cg_sample_bfg_fire = cgi.LoadSample("weapons/bfg/fire");
	cg_sample_bfg_hit = cgi.LoadSample("weapons/bfg/hit");

	cg_sample_hook_hit = cgi.LoadSample("objects/hook/hit");

	cg_sample_explosion = cgi.LoadSample("weapons/common/explosion");
	cg_sample_teleport = cgi.LoadSample("world/teleport");
	cg_sample_respawn = cgi.LoadSample("world/respawn");
	cg_sample_sparks = cgi.LoadSample("world/sparks");
	cg_sample_fire = cgi.LoadSample("world/fire");
	cg_sample_steam = cgi.LoadSample("world/steam");
	cg_sample_rain = cgi.LoadSample("world/rain");
	cg_sample_snow = cgi.LoadSample("world/snow");
	cg_sample_underwater = cgi.LoadSample("world/underwater");
	cg_sample_gib = cgi.LoadSample("gibs/common/gib");

	for (uint32_t i = 0; i < lengthof(cg_sample_hits); i++) {
		g_snprintf(name, sizeof(name), "misc/hit_%" PRIu32, i + 1);
		cg_sample_hits[i] = cgi.LoadSample(name);
	}

	for (uint32_t i = 0; i < lengthof(cg_sample_machinegun_fire); i++) {
		g_snprintf(name, sizeof(name), "weapons/machinegun/fire_%" PRIu32, i + 1);
		cg_sample_machinegun_fire[i] = cgi.LoadSample(name);
	}

	for (uint32_t i = 0; i < lengthof(cg_sample_machinegun_hit); i++) {
		g_snprintf(name, sizeof(name), "weapons/machinegun/hit_%" PRIu32, i + 1);
		cg_sample_machinegun_hit[i] = cgi.LoadSample(name);
	}

	cgi.LoadingProgress(-1, "sprites");

	Cg_FreeSprites();

	cg_beam_hook = cgi.LoadImage("sprites/rope", IMG_SPRITE);
	cg_beam_arrow = cgi.LoadImage("sprites/arrow", IMG_SPRITE/* | IT_MASK_CLAMP_EDGE*/);
	cg_beam_line = cgi.LoadImage("sprites/line", IMG_SPRITE/* | IT_MASK_CLAMP_EDGE*/);
	cg_beam_rail = cgi.LoadImage("sprites/beam", IMG_SPRITE/* | IT_MASK_CLAMP_EDGE*/);
	cg_beam_lightning = cgi.LoadImage("sprites/lightning", IMG_SPRITE);
	cg_beam_tracer = cgi.LoadImage("sprites/tracer", IMG_SPRITE/* | IT_MASK_CLAMP_EDGE*/);
	cg_beam_tail = cgi.LoadImage("sprites/particle_tail", IMG_SPRITE/* | IT_MASK_CLAMP_EDGE*/);
	cg_sprite_blaster_flash = cgi.LoadImage("sprites/blast_01/blast_01_flash", IMG_SPRITE);

	cg_sprite_atlas = cgi.LoadAtlas("cg_sprite_atlas");
	cg_sprite_particle = cgi.LoadAtlasImage(cg_sprite_atlas, "sprites/particle", IMG_SPRITE);
	cg_sprite_particle2 = cgi.LoadAtlasImage(cg_sprite_atlas, "sprites/particle2", IMG_SPRITE);
	cg_sprite_particle3 = cgi.LoadAtlasImage(cg_sprite_atlas, "sprites/particle3", IMG_SPRITE);
	cg_sprite_flash = cgi.LoadAtlasImage(cg_sprite_atlas, "sprites/flash", IMG_SPRITE);
	cg_sprite_ring = cgi.LoadAtlasImage(cg_sprite_atlas, "sprites/ring", IMG_SPRITE);
	cg_sprite_aniso_flare_01 = cgi.LoadAtlasImage(cg_sprite_atlas, "sprites/aniso_flare_01", IMG_SPRITE);
	cg_sprite_smoke = cgi.LoadAtlasImage(cg_sprite_atlas, "sprites/smoke", IMG_SPRITE);
	cg_sprite_flame = cgi.LoadAtlasImage(cg_sprite_atlas, "sprites/flame", IMG_SPRITE);
	cg_sprite_explosion_glow = cgi.LoadAtlasImage(cg_sprite_atlas, "sprites/explosion_glow", IMG_SPRITE);
	cg_sprite_explosion_flash = cgi.LoadAtlasImage(cg_sprite_atlas, "sprites/explosion_flash", IMG_SPRITE);
	cg_sprite_spark = cgi.LoadAtlasImage(cg_sprite_atlas, "sprites/spark", IMG_SPRITE);
	cg_sprite_rain = cgi.LoadAtlasImage(cg_sprite_atlas, "sprites/rain", IMG_SPRITE);
	cg_sprite_snow = cgi.LoadAtlasImage(cg_sprite_atlas, "sprites/snow", IMG_SPRITE);
	cg_sprite_steam = cgi.LoadAtlasImage(cg_sprite_atlas, "sprites/steam", IMG_SPRITE);
	cg_sprite_bubble = cgi.LoadAtlasImage(cg_sprite_atlas, "sprites/bubble", IMG_SPRITE);
	cg_sprite_inactive = cgi.LoadAtlasImage(cg_sprite_atlas, "sprites/inactive", IMG_SPRITE);
	cg_sprite_plasma_var01 = cgi.LoadAtlasImage(cg_sprite_atlas, "sprites/plasma/plasma_var01", IMG_SPRITE);
	cg_sprite_plasma_var02 = cgi.LoadAtlasImage(cg_sprite_atlas, "sprites/plasma/plasma_var02", IMG_SPRITE);
	cg_sprite_plasma_var03 = cgi.LoadAtlasImage(cg_sprite_atlas, "sprites/plasma/plasma_var03", IMG_SPRITE);
	cg_sprite_blob_01 = cgi.LoadAtlasImage(cg_sprite_atlas, "sprites/blob_01", IMG_SPRITE);
	cg_sprite_electro_02 = cgi.LoadAtlasImage(cg_sprite_atlas, "sprites/electro_02/electro_02", IMG_SPRITE);
	cg_sprite_teleport = cgi.LoadAtlasImage(cg_sprite_atlas, "sprites/teleport", IMG_SPRITE);
	cg_sprite_splash_02_03 = cgi.LoadAtlasImage(cg_sprite_atlas, "sprites/splash_02/splash_02_03", IMG_SPRITE);
	cg_sprite_impact_spark_01_dot = cgi.LoadAtlasImage(cg_sprite_atlas, "sprites/impact_spark_01/impact_spark_01_dot", IMG_SPRITE);
	cg_sprite_puff_cloud = cgi.LoadAtlasImage(cg_sprite_atlas, "sprites/puff_cloud", IMG_SPRITE);
	cg_sprite_water_circle = cgi.LoadAtlasImage(cg_sprite_atlas, "sprites/water/splash_01_circle", IMG_SPRITE);
	cg_sprite_water_ring = cgi.LoadAtlasImage(cg_sprite_atlas, "sprites/water/splash_01_ring", IMG_SPRITE);
	cg_sprite_water_ring2 = cgi.LoadAtlasImage(cg_sprite_atlas, "sprites/water/splash_01_ring2", IMG_SPRITE);
	cg_sprite_abstract_01 = cgi.LoadAtlasImage(cg_sprite_atlas, "sprites/abstract/abstract_01", IMG_SPRITE);
	cg_sprite_node_wait = cgi.LoadAtlasImage(cg_sprite_atlas, "pics/emoji/teamkill", IMG_SPRITE);
	cg_sprite_node_slow = cgi.LoadAtlasImage(cg_sprite_atlas, "pics/emoji/crush", IMG_SPRITE);

	cgi.LoadingProgress(-1, "sprites");

	cg_sprite_blaster_ring = Cg_LoadAnimatedSprite(cg_sprite_atlas, "sprites/blast_01/blast_01_ring", "_%02" PRIu32, 1, 7);
	cg_sprite_explosion = Cg_LoadAnimatedSprite(cg_sprite_atlas, "sprites/explosion_01/explosion_01", "_%02" PRIu32, 1, 36);
	cg_sprite_explosion_ring_02 = Cg_LoadAnimatedSprite(cg_sprite_atlas, "sprites/explosion_ring_02/explosion_ring_02", "_%02" PRIu32, 1, 7);
	cg_sprite_rocket_flame = Cg_LoadAnimatedSprite(cg_sprite_atlas, "sprites/flame_03/flame_03", "_%02" PRIu32, 1, 29);
	cg_sprite_blaster_flame = Cg_LoadAnimatedSprite(cg_sprite_atlas, "sprites/flame_mono_01/flame_mono_01", "_%02" PRIu32, 1, 21);
	cg_sprite_smoke_04 = Cg_LoadAnimatedSprite(cg_sprite_atlas, "sprites/smoke_04/smoke_04", "_%02" PRIu32, 1, 90);
	cg_sprite_smoke_05 = Cg_LoadAnimatedSprite(cg_sprite_atlas, "sprites/smoke_05/smoke_05", "_%02" PRIu32, 1, 99);
	cg_sprite_bfg_explosion_2 = Cg_LoadAnimatedSprite(cg_sprite_atlas, "sprites/bfg_explosion_02/bfg_explosion_02", "_%02" PRIu32, 1, 23);
	cg_sprite_bfg_explosion_3 = Cg_LoadAnimatedSprite(cg_sprite_atlas, "sprites/bfg_explosion_03/bfg_explosion_03", "_%02" PRIu32, 1, 21);
	cg_sprite_poof_01 = Cg_LoadAnimatedSprite(cg_sprite_atlas, "sprites/poof_01/poof_01", "_%02" PRIu32, 1, 34);
	cg_sprite_poof_02 = Cg_LoadAnimatedSprite(cg_sprite_atlas, "sprites/poof_02/poof_02", "_%02" PRIu32, 1, 17);
	cg_sprite_blood_01 = Cg_LoadAnimatedSprite(cg_sprite_atlas, "sprites/blood_01/blood_01", "_%02" PRIu32, 1, 10);
	cg_sprite_electro_01 = Cg_LoadAnimatedSprite(cg_sprite_atlas, "sprites/electro_01/electro_01", "_%02" PRIu32, 1, 5);
	cg_sprite_fireball_01 = Cg_LoadAnimatedSprite(cg_sprite_atlas, "sprites/fireball_01/fireball_01", "_%02" PRIu32, 0, 63);
	cg_sprite_impact_spark_01 = Cg_LoadAnimatedSprite(cg_sprite_atlas, "sprites/impact_spark_01/impact_spark_01", "_%02" PRIu32, 0, 4);
	cg_sprite_hyperball_01 = Cg_LoadAnimatedSprite(cg_sprite_atlas, "sprites/hyperball_01/hyperball_01", "_%02" PRIu32, 1, 32);

	cgi.CompileAtlas(cg_sprite_atlas);

	const int32_t w = cgi.context->drawable_width, h = cgi.context->drawable_height;
	cg_framebuffer = cgi.CreateFramebuffer(w, h, ATTACHMENT_ALL);

	Cg_LoadFlares();

	cgi.LoadingProgress(-1, "entities");

	Cg_LoadEntities();

	Cg_LoadEffects();

	Cg_InitLights();

	cgi.LoadingProgress(-1, "clients");

	Cg_LoadClients();

	cgi.LoadingProgress(-1, "hud");

	cg_draw_crosshair->modified = true;
	cg_draw_crosshair_color->modified = true;

	Cg_LoadHudMedia();

	Cg_Debug("Complete\n");
}

/**
 * @brief
 */
void Cg_FreeMedia(void) {

	cgi.FreeTag(MEM_TAG_CGAME);
	cgi.FreeTag(MEM_TAG_CGAME_LEVEL);

	cgi.DestroyFramebuffer(&cg_framebuffer);

	Cg_FreeEntities();

	Cg_FreeFlares();

	Cg_FreeSprites();
}
