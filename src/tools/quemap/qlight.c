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

#include "qlight.h"

_Bool indirect = true;
_Bool antialias = false;

float brightness = 1.0;
float saturation = 1.0;
float contrast = 1.0;

int32_t luxel_size = BSP_LIGHTMAP_LUXEL_SIZE;
int32_t patch_size = DEFAULT_PATCH_SIZE;

float radiosity = 1.0;
int32_t num_bounces = 1;
int32_t bounce = 0;

float lightscale_point = 1.0;
float lightscale_patch = 1.0;

// we use the collision detection facilities for lighting
static cm_bsp_model_t *bsp_models[MAX_BSP_MODELS];

/**
 * @brief
 */
int32_t Light_PointContents(const vec3_t p, int32_t head_node) {

	int32_t contents = Cm_PointContents(p, 0);

	if (head_node) {
		contents |= Cm_PointContents(p, head_node);
	}

	return contents;
}

/**
 * @brief Lighting collision detection.
 * @details Lighting traces are clipped to the world, and to the entity for the given lightmap.
 * This way, inline models will self-shadow, but will not cast shadows on each other or on the
 * world.
 * @param lm The lightmap.
 * @param start The starting point.
 * @param end The desired end point.
 * @param mask The contents mask to clip to.
 * @return The trace.
 */
cm_trace_t Light_Trace(const vec3_t start, const vec3_t end, int32_t head_node, int32_t mask) {

	cm_trace_t trace = Cm_BoxTrace(start, end, Box3_Zero(), 0, mask, NULL, NULL);

	if (head_node) {
		cm_trace_t tr = Cm_BoxTrace(start, end, Box3_Zero(), head_node, mask, NULL, NULL);
		if (tr.fraction < trace.fraction) {
			trace = tr;
		}
	}

	return trace;
}

/**
 * @brief
 */
static void LightWorld(void) {

	if (bsp_file.num_nodes == 0 || bsp_file.num_faces == 0) {
		Com_Error(ERROR_FATAL, "Empty map\n");
	}

	// load the bsp for tracing
	bsp_models[0] = Cm_LoadBspModel(bsp_name, NULL);
	for (int32_t i = 1; i < Cm_NumModels(); i++) {
		bsp_models[i] = Cm_Model(va("*%d", i));
	}

	// resolve global lighting parameters from worldspawn

	const cm_entity_t *e = Cm_Bsp()->entities[0];

	if (radiosity == 1.f) {
		radiosity = Cm_EntityValue(e, "radiosity")->value ?: radiosity;
		Com_Verbose("Radiosity: %g\n", radiosity);
	}

	if (brightness == 1.f) {
		brightness = Cm_EntityValue(e, "brightness")->value ?: brightness;
		Com_Verbose("Brightness: %g\n", brightness);
	}

	if (saturation == 1.f) {
		saturation = Cm_EntityValue(e, "saturation")->value ?: saturation;
		Com_Verbose("Saturation: %g\n", saturation);
	}

	if (contrast == 1.f) {
		contrast = Cm_EntityValue(e, "contrast")->value ?: contrast;
		Com_Verbose("Contrast: %g\n", contrast);
	}

	if (luxel_size == BSP_LIGHTMAP_LUXEL_SIZE) {
		luxel_size = Cm_EntityValue(e, "luxel_size")->integer ?: luxel_size;
		Com_Verbose("Luxel size: %d\n", luxel_size);
	}

	if (patch_size == DEFAULT_PATCH_SIZE) {
		patch_size = Cm_EntityValue(e, "patch_size")->integer ?: patch_size;
		Com_Verbose("Patch size: %d\n", patch_size);
	}

	if (lightscale_point == 1.f) {
		lightscale_point = Cm_EntityValue(e, "lightscale_point")->value ?: lightscale_point;
		Com_Verbose("Point light intensity scale: 1.0\n");
	}

	if (lightscale_patch == 1.f) {
		lightscale_patch = Cm_EntityValue(e, "lightscale_patch")->value ?: lightscale_patch;
		Com_Verbose("Patch light intensity scale: 1.0\n");
	}

	// build patches
	BuildPatches();

	// subdivide patches to the desired resolution
	Work("Building patches", SubdividePatch, bsp_file.num_faces);

	// build lightmaps
	BuildLightmaps();

	// build lightgrid
	const size_t num_lightgrid = BuildLightgrid();

	// build lights out of patches and entities
	BuildDirectLights();

	// ambient and diffuse lighting
	Work("Direct lightmaps", DirectLightmap, bsp_file.num_faces);
	Work("Direct lightgrid", DirectLightgrid, (int32_t) num_lightgrid);

	if (indirect && radiosity > 0.f) {
		for (bounce = 0; bounce < num_bounces; bounce++) {

			// build indirect lights from lightmapped patches
			BuildIndirectLights();

			// calculate indirect lighting
			Work("Indirect lightmaps", IndirectLightmap, bsp_file.num_faces);
			Work("Indirect lightgrid", IndirectLightgrid, (int32_t) num_lightgrid);
		}
	}

	// free the light sources
	FreeLights();

	// finalize it and write it to per-face textures
	Work("Finalizing lightmaps", FinalizeLightmap, bsp_file.num_faces);
	Work("Finalizing lightgrid", FinalizeLightgrid, (int32_t) num_lightgrid);

	// bake fog volumes into the lightgrid
	BuildFog();

	Work("Fog volumes", FogLightgrid, (int32_t) num_lightgrid);

	FreeFog();

	// generate atlased lightmaps
	EmitLightmap();

	// and vertex lightmap texcoords
	EmitLightmapTexcoords();

	// and lightgrid
	EmitLightgrid();

	// free the lightmaps
	Mem_FreeTag(MEM_TAG_LIGHTMAP);

	// free the patches
	Mem_FreeTag(MEM_TAG_PATCH);
}

/**
 * @brief
 */
int32_t LIGHT_Main(void) {

	Com_Print("\n------------------------------------------\n");
	Com_Print("\nLighting %s\n\n", bsp_name);

	const uint32_t start = SDL_GetTicks();

	LoadBSPFile(bsp_name, BSP_LUMPS_ALL);

	LoadMaterials(va("maps/%s.mat", map_base), ASSET_CONTEXT_TEXTURES, NULL);

	LightWorld();

	FreeMaterials();

	WriteBSPFile(va("maps/%s.bsp", map_base));

	FreeWindings();

	for (int32_t tag = MEM_TAG_QLIGHT; tag < MEM_TAG_QMAT; tag++) {
		Mem_FreeTag(tag);
	}

	const uint32_t end = SDL_GetTicks();
	Com_Print("\nLit %s in %d ms\n", bsp_name, (end - start));

	return 0;
}
