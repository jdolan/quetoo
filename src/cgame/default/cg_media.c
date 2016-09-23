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

s_sample_t *cg_sample_explosion;
s_sample_t *cg_sample_teleport;
s_sample_t *cg_sample_respawn;
s_sample_t *cg_sample_sparks;
s_sample_t *cg_sample_footsteps[4];
s_sample_t *cg_sample_rain;
s_sample_t *cg_sample_snow;
s_sample_t *cg_sample_underwater;
s_sample_t *cg_sample_hits[2];
s_sample_t *cg_sample_gib;

cg_particles_t *cg_particles_normal;
cg_particles_t *cg_particles_explosion;
cg_particles_t *cg_particles_teleporter;
cg_particles_t *cg_particles_smoke;
cg_particles_t *cg_particles_steam;
cg_particles_t *cg_particles_bubble;
cg_particles_t *cg_particles_rain;
cg_particles_t *cg_particles_snow;
cg_particles_t *cg_particles_beam;
cg_particles_t *cg_particles_burn;
cg_particles_t *cg_particles_blood;
cg_particles_t *cg_particles_lightning;
cg_particles_t *cg_particles_flame;
cg_particles_t *cg_particles_spark;
cg_particles_t *cg_particles_inactive;
cg_particles_t *cg_particles_bullet[3];

/**
 * @brief Updates all media references for the client game.
 */
void Cg_UpdateMedia(void) {
	char name[MAX_QPATH];

	Cg_FreeParticles();

	cgi.FreeTag(MEM_TAG_CGAME);
	cgi.FreeTag(MEM_TAG_CGAME_LEVEL);

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

	cg_sample_explosion = cgi.LoadSample("weapons/common/explosion");
	cg_sample_teleport = cgi.LoadSample("world/teleport");
	cg_sample_respawn = cgi.LoadSample("world/respawn");
	cg_sample_sparks = cgi.LoadSample("world/sparks");
	cg_sample_rain = cgi.LoadSample("world/rain");
	cg_sample_snow = cgi.LoadSample("world/snow");
	cg_sample_underwater = cgi.LoadSample("world/underwater");
	cg_sample_gib = cgi.LoadSample("gibs/common/gib");

	for (size_t i = 0; i < lengthof(cg_sample_hits); i++) {
		g_snprintf(name, sizeof(name), "misc/hit_%zd", i + 1);
		cg_sample_hits[i] = cgi.LoadSample(name);
	}

	for (size_t i = 0; i < lengthof(cg_sample_machinegun_fire); i++) {
		g_snprintf(name, sizeof(name), "weapons/machinegun/fire_%zd", i + 1);
		cg_sample_machinegun_fire[i] = cgi.LoadSample(name);
	}

	for (size_t i = 0; i < lengthof(cg_sample_machinegun_hit); i++) {
		g_snprintf(name, sizeof(name), "weapons/machinegun/hit_%zd", i + 1);
		cg_sample_machinegun_hit[i] = cgi.LoadSample(name);
	}

	for (size_t i = 0; i < lengthof(cg_sample_footsteps); i++) {
		g_snprintf(name, sizeof(name), "#players/common/step_%zd", i + 1);
		cg_sample_footsteps[i] = cgi.LoadSample(name);
	}

	cg_particles_normal = Cg_AllocParticles(cgi.LoadImage("particles/particle.tga", IT_EFFECT));
	cg_particles_explosion = Cg_AllocParticles(cgi.LoadImage("particles/explosion.tga", IT_EFFECT));
	cg_particles_teleporter = Cg_AllocParticles(cgi.LoadImage("particles/teleport.tga", IT_EFFECT));
	cg_particles_smoke = Cg_AllocParticles(cgi.LoadImage("particles/smoke.tga", IT_EFFECT));
	cg_particles_steam = Cg_AllocParticles(cgi.LoadImage("particles/steam.tga", IT_EFFECT));
	cg_particles_bubble = Cg_AllocParticles(cgi.LoadImage("particles/bubble.tga", IT_EFFECT));
	cg_particles_rain = Cg_AllocParticles(cgi.LoadImage("particles/rain.tga", IT_EFFECT));
	cg_particles_snow = Cg_AllocParticles(cgi.LoadImage("particles/snow.tga", IT_EFFECT));
	cg_particles_beam = Cg_AllocParticles(cgi.LoadImage("particles/beam.tga", IT_EFFECT));
	cg_particles_burn = Cg_AllocParticles(cgi.LoadImage("particles/burn.tga", IT_EFFECT));
	cg_particles_blood = Cg_AllocParticles(cgi.LoadImage("particles/blood.tga", IT_EFFECT));
	cg_particles_lightning = Cg_AllocParticles(cgi.LoadImage("particles/lightning.tga", IT_EFFECT));
	cg_particles_flame = Cg_AllocParticles(cgi.LoadImage("particles/flame.tga", IT_EFFECT));
	cg_particles_spark = Cg_AllocParticles(cgi.LoadImage("particles/spark.tga", IT_EFFECT));
	cg_particles_inactive = Cg_AllocParticles(cgi.LoadImage("particles/inactive.tga", IT_EFFECT));

	for (size_t i = 0; i < lengthof(cg_particles_bullet); i++) {
		g_snprintf(name, sizeof(name), "particles/bullet_%zd", i);
		cg_particles_bullet[i] = Cg_AllocParticles(cgi.LoadImage(name, IT_EFFECT));
	}

	cg_draw_crosshair->modified = true;
	cg_draw_crosshair_color->modified = true;

	Cg_LoadEmits();

	Cg_LoadEffects();

	Cg_LoadClients();

	cgi.Debug("Complete\n");
}
