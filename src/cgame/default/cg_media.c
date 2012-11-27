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
s_sample_t *cg_sample_lightning_fly;
s_sample_t *cg_sample_lightning_discharge;
s_sample_t *cg_sample_railgun_fire;
s_sample_t *cg_sample_bfg_fire;
s_sample_t *cg_sample_bfg_hit;

s_sample_t *cg_sample_explosion;
s_sample_t *cg_sample_teleport;
s_sample_t *cg_sample_respawn;
s_sample_t *cg_sample_sparks;
s_sample_t *cg_sample_footsteps[4];
s_sample_t *cg_sample_rain;
s_sample_t *cg_sample_snow;
s_sample_t *cg_sample_hit;

r_image_t *cg_particle_normal;
r_image_t *cg_particle_explosion;
r_image_t *cg_particle_teleporter;
r_image_t *cg_particle_smoke;
r_image_t *cg_particle_steam;
r_image_t *cg_particle_bubble;
r_image_t *cg_particle_rain;
r_image_t *cg_particle_snow;
r_image_t *cg_particle_beam;
r_image_t *cg_particle_burn;
r_image_t *cg_particle_blood;
r_image_t *cg_particle_lightning;
r_image_t *cg_particle_flame;
r_image_t *cg_particle_spark;
r_image_t *cg_particle_inactive;
r_image_t *cg_particle_bullet[3];

/*
 * @brief Updates all media references for the client game.
 */
void Cg_UpdateMedia(void) {
	int32_t i;
	char name[MAX_QPATH];

	cgi.FreeTag(Z_TAG_CGAME);

	cg_sample_blaster_fire = cgi.LoadSample("weapons/blaster/fire");
	cg_sample_blaster_hit = cgi.LoadSample("weapons/blaster/hit");
	cg_sample_shotgun_fire = cgi.LoadSample("weapons/shotgun/fire");
	cg_sample_supershotgun_fire = cgi.LoadSample("weapons/supershotgun/fire");
	cg_sample_grenadelauncher_fire = cgi.LoadSample("weapons/grenadelauncher/fire");
	cg_sample_rocketlauncher_fire = cgi.LoadSample("weapons/rocketlauncher/fire");
	cg_sample_hyperblaster_fire = cgi.LoadSample("weapons/hyperblaster/fire");
	cg_sample_hyperblaster_hit = cgi.LoadSample("weapons/hyperblaster/hit");
	cg_sample_lightning_fire = cgi.LoadSample("weapons/lightning/fire");
	cg_sample_lightning_fly = cgi.LoadSample("weapons/lightning/fly");
	cg_sample_lightning_discharge = cgi.LoadSample("weapons/lightning/discharge");
	cg_sample_railgun_fire = cgi.LoadSample("weapons/railgun/fire");
	cg_sample_bfg_fire = cgi.LoadSample("weapons/bfg/fire");
	cg_sample_bfg_hit = cgi.LoadSample("weapons/bfg/hit");

	cg_sample_explosion = cgi.LoadSample("weapons/common/explosion");
	cg_sample_teleport = cgi.LoadSample("world/teleport");
	cg_sample_respawn = cgi.LoadSample("world/respawn");
	cg_sample_sparks = cgi.LoadSample("world/sparks");
	cg_sample_rain = cgi.LoadSample("world/rain");
	cg_sample_snow = cgi.LoadSample("world/snow");
	cg_sample_hit = cgi.LoadSample("misc/hit");

	for (i = 0; i < 4; i++) {
		snprintf(name, sizeof(name), "weapons/machinegun/fire_%i", i + 1);
		cg_sample_machinegun_fire[i] = cgi.LoadSample(name);
	}

	for (i = 0; i < 3; i++) {
		snprintf(name, sizeof(name), "weapons/machinegun/hit_%i", i + 1);
		cg_sample_machinegun_hit[i] = cgi.LoadSample(name);
	}

	for (i = 0; i < 4; i++) {
		snprintf(name, sizeof(name), "#players/common/step_%i", i + 1);
		cg_sample_footsteps[i] = cgi.LoadSample(name);
	}

	cg_particle_normal = cgi.LoadImage("particles/particle.tga", IT_EFFECT);
	cg_particle_explosion = cgi.LoadImage("particles/explosion.tga", IT_EFFECT);
	cg_particle_teleporter = cgi.LoadImage("particles/teleport.tga", IT_EFFECT);
	cg_particle_smoke = cgi.LoadImage("particles/smoke.tga", IT_EFFECT);
	cg_particle_steam = cgi.LoadImage("particles/steam.tga", IT_EFFECT);
	cg_particle_bubble = cgi.LoadImage("particles/bubble.tga", IT_EFFECT);
	cg_particle_rain = cgi.LoadImage("particles/rain.tga", IT_EFFECT);
	cg_particle_snow = cgi.LoadImage("particles/snow.tga", IT_EFFECT);
	cg_particle_beam = cgi.LoadImage("particles/beam.tga", IT_EFFECT);
	cg_particle_burn = cgi.LoadImage("particles/burn.tga", IT_EFFECT);
	cg_particle_blood = cgi.LoadImage("particles/blood.tga", IT_EFFECT);
	cg_particle_lightning = cgi.LoadImage("particles/lightning.tga", IT_EFFECT);
	cg_particle_flame = cgi.LoadImage("particles/flame.tga", IT_EFFECT);
	cg_particle_spark = cgi.LoadImage("particles/spark.tga", IT_EFFECT);
	cg_particle_inactive = cgi.LoadImage("particles/inactive.tga", IT_EFFECT);

	for (i = 0; i < 3; i++) {
		snprintf(name, sizeof(name), "particles/bullet_%i", i);
		cg_particle_bullet[i] = cgi.LoadImage(name, IT_EFFECT);
	}

	cg_draw_crosshair->modified = true;
	cg_draw_crosshair_color->modified = true;

	Cg_LoadEmits();

	Cg_LoadEffects();

	Cg_LoadClients();

	Cg_FreeParticles();

	cgi.Debug("Cg_LoadMedia: Complete\n");
}
