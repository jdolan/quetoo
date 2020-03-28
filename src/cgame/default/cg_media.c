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
#include "collision/collision.h"

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
s_sample_t *cg_sample_rain;
s_sample_t *cg_sample_snow;
s_sample_t *cg_sample_underwater;
s_sample_t *cg_sample_hits[2];
s_sample_t *cg_sample_gib;

r_image_t *cg_sprite_smoke;
r_image_t *cg_beam_hook;
r_image_t *cg_beam_rail;
r_image_t *cg_beam_lightning;
r_animation_t *cg_fire_1;
r_animation_t *cg_flame_1;
r_animation_t *cg_smoke_1;
r_animation_t *cg_smoke_2;
r_animation_t *cg_blast_01_ring;

static GHashTable *cg_footstep_table;

/**
 * @brief Free callback for footstep table
 */
static void Cg_FootstepsTable_Destroy(gpointer value) {
	g_array_free((GArray *) value, true);
}

/**
 * @brief
 */
static void Cg_FootstepsTable_EnumerateFiles(const char *file, void *data) {
	(*((size_t *) data))++;
}

/**
 * @brief
 */
static void Cg_FootstepsTable_Load(const char *footsteps) {
	GArray *sounds = (GArray *) g_hash_table_lookup(cg_footstep_table, footsteps);

	if (sounds) { // already loaded
		return;
	}

	size_t count = 0;
	cgi.EnumerateFiles(va("players/common/step_%s_*", footsteps), Cg_FootstepsTable_EnumerateFiles, &count);

	if (!count) {
		cgi.Warn("Map has footsteps material %s but no sound files exist\n", footsteps);
		return;
	}

	sounds = g_array_new(false, false, sizeof(s_sample_t *));

	for (uint32_t i = 0; i < count; i++) {

		char name[MAX_QPATH];
		g_snprintf(name, sizeof(name), "#players/common/step_%s_%" PRIu32, footsteps, i + 1);

		s_sample_t *sample = cgi.LoadSample(name);
		sounds = g_array_append_val(sounds, sample);
	}

	g_hash_table_insert(cg_footstep_table, (gpointer) footsteps, sounds);
}

/**
 * @brief Return a sample for the specified footstep type.
 */
s_sample_t *Cg_GetFootstepSample(const char *footsteps) {

	if (!footsteps || !*footsteps) {
		footsteps = "default";
	}

	GArray *sounds = (GArray *) g_hash_table_lookup(cg_footstep_table, footsteps);

	if (!sounds) { // bad mapper!
		cgi.Warn("Footstep sound %s not valid\n", footsteps);
		return Cg_GetFootstepSample("default"); // this should never recurse
	}

	static uint32_t last_index = -1;
	uint32_t index = RandomRangeu(0, sounds->len);

	if (last_index == index) {
		index = (index ^ 1) % sounds->len;
	}

	last_index = index;

	return g_array_index(sounds, s_sample_t *, index);
}

/**
 * @brief Loads all of the footstep sounds for this map.
 */
static void Cg_InitFootsteps(void) {

	if (cg_footstep_table) {
		g_hash_table_destroy(cg_footstep_table);
	}

	cg_footstep_table = g_hash_table_new_full(g_str_hash, g_str_equal, NULL, Cg_FootstepsTable_Destroy);

	// load the hardcoded default set
	GArray *default_samples = g_array_new(false, false, sizeof(s_sample_t *));

	default_samples = g_array_append_vals(default_samples, (const s_sample_t *[]) {
		cgi.LoadSample("#players/common/step_1"),
		cgi.LoadSample("#players/common/step_2"),
		cgi.LoadSample("#players/common/step_3"),
		cgi.LoadSample("#players/common/step_4")
	}, 4);

	g_hash_table_insert(cg_footstep_table, "default", default_samples);

	r_material_t **material = cgi.WorldModel()->materials;
	for (size_t i = 0; i < cgi.WorldModel()->num_materials; i++, material++) {

		if (strlen((*material)->cm->footsteps)) {
			Cg_FootstepsTable_Load((*material)->cm->footsteps);
		}
	}
}

/**
 * @brief Updates all media references for the client game.
 */
void Cg_UpdateMedia(void) {
	char name[MAX_QPATH];
	
	Cg_FreeParticles();
	Cg_FreeSprites();

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

	Cg_InitFootsteps();
	
	Cg_FreeParticles();

	Cg_FreeSprites();

	cg_draw_crosshair->modified = true;
	cg_draw_crosshair_color->modified = true;

	Cg_LoadEmits();

	cg_sprite_smoke = cgi.LoadImage("particles/smoke", IT_EFFECT);
	cg_beam_hook = cgi.LoadImage("particles/rope", IT_EFFECT);
	cg_beam_rail = cgi.LoadImage("particles/beam", IT_EFFECT | IT_MASK_CLAMP_EDGE);
	cg_beam_lightning = cgi.LoadImage("particles/lightning", IT_EFFECT);

	// explosion
	const r_image_t *cg_fire_1_images[36];
	for (uint32_t i = 0; i < lengthof(cg_fire_1_images); i++) {
		g_snprintf(name, sizeof(name), "particles/explosion_01/explosion_01_%02" PRIu32, i + 1);
		cg_fire_1_images[i] = cgi.LoadImage(name, IT_EFFECT);
	}
	cg_fire_1 = cgi.CreateAnimation("particles/flame1/flame_1", lengthof(cg_fire_1_images), cg_fire_1_images);

	// flame 1
	const r_image_t *cg_flame_1_images[29];
	for (uint32_t i = 0; i < lengthof(cg_flame_1_images); i++) {
		g_snprintf(name, sizeof(name), "particles/flame_03/flame_03_%02" PRIu32, i + 1);
		cg_flame_1_images[i] = cgi.LoadImage(name, IT_EFFECT);
	}
	cg_flame_1 = cgi.CreateAnimation("particles/flame_01/flame_01", lengthof(cg_flame_1_images), cg_flame_1_images);
	
	// smoke 1
	const r_image_t *cg_smoke_1_images[90];
	for (uint32_t i = 0; i < lengthof(cg_smoke_1_images); i++) {
		g_snprintf(name, sizeof(name), "particles/smoke_04/smoke_04_%02" PRIu32, i + 1);
		cg_smoke_1_images[i] = cgi.LoadImage(name, IT_EFFECT);
	}
	cg_smoke_1 = cgi.CreateAnimation("particles/smoke_1", lengthof(cg_smoke_1_images), cg_smoke_1_images);
	
	// smoke 2
	const r_image_t *cg_smoke_2_images[99];
	for (uint32_t i = 0; i < lengthof(cg_smoke_2_images); i++) {
		g_snprintf(name, sizeof(name), "particles/smoke_05/smoke_05_%02" PRIu32, i + 1);
		cg_smoke_2_images[i] = cgi.LoadImage(name, IT_EFFECT);
	}
	cg_smoke_2 = cgi.CreateAnimation("particles/smoke_2", lengthof(cg_smoke_2_images), cg_smoke_2_images);

	// blast ring
	const r_image_t *cg_blast_01_ring_images[7];
	for (uint32_t i = 0; i < lengthof(cg_blast_01_ring_images); i++) {
		g_snprintf(name, sizeof(name), "particles/blast_01/blast_01_ring_%02" PRIu32, i + 1);
		cg_blast_01_ring_images[i] = cgi.LoadImage(name, IT_EFFECT);
	}
	cg_blast_01_ring = cgi.CreateAnimation("particles/blast_01/blast_01_ring", lengthof(cg_blast_01_ring_images), cg_blast_01_ring_images);
	
	Cg_LoadEffects();

	Cg_LoadClients();

	Cg_LoadHudMedia();

	cgi.Debug("Complete\n");
}
