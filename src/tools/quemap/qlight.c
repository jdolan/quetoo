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

_Bool antialias = false;
_Bool indirect = false;

float brightness = 1.0;
float saturation = 1.0;
float contrast = 1.0;

int32_t luxel_size = BSP_LIGHTMAP_LUXEL_SIZE;
int32_t patch_size = DEFAULT_BSP_PATCH_SIZE;

float radiosity = LIGHT_RADIOSITY;
int32_t num_bounces = 1;
int32_t bounce = 0;

// we use the collision detection facilities for lighting
static cm_bsp_model_t *bsp_models[MAX_BSP_MODELS];

/**
 * @brief
 */
int32_t Light_ClusterPVS(const int32_t cluster, byte *pvs) {

	if (Cm_NumClusters() == 0) {
		return Cm_ClusterPVS(0, pvs);
	}

	return Cm_ClusterPVS(cluster, pvs);
}

/**
 * @brief
 */
int32_t Light_PointPVS(const vec3_t p, int32_t head_node, byte *pvs) {

	const int32_t leaf = Cm_PointLeafnum(p, head_node);

	return Cm_ClusterPVS(bsp_file.leafs[leaf].cluster, pvs);
}

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

	cm_trace_t trace = Cm_BoxTrace(start, end, Vec3_Zero(), Vec3_Zero(), 0, mask);

	if (head_node) {
		cm_trace_t tr = Cm_BoxTrace(start, end, Vec3_Zero(), Vec3_Zero(), head_node, mask);
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

	if (Cm_NumClusters() == 0) {
		Com_Warn("No VIS information, expect longer compile time\n");
	}

	// resolve global lighting parameters from worldspawn

	GList *entities = Cm_LoadEntities(bsp_file.entity_string);
	const cm_entity_t *e = entities->data;

	if (radiosity == LIGHT_RADIOSITY) {
		if (Cm_EntityVector(e, "radiosity", &radiosity, 1) == 1) {
			Com_Verbose("Radiosity: %g\n", radiosity);
		}
	}

	if (brightness == 1.0) {
		if (Cm_EntityVector(e, "brightness", &brightness, 1) == 1) {
			Com_Verbose("Brightness: %g\n", brightness);
		}
	}

	if (saturation == 1.0) {
		if (Cm_EntityVector(e, "saturation", &saturation, 1) == 1) {
			Com_Verbose("Saturation: %g\n", saturation);
		}
	}

	if (contrast == 1.0) {
		if (Cm_EntityVector(e, "contrast", &contrast, 1) == 1) {
			Com_Verbose("Contrast: %g\n", contrast);
		}
	}

	if (luxel_size == BSP_LIGHTMAP_LUXEL_SIZE) {
		float v;
		if (Cm_EntityVector(e, "luxel_size", &v, 1) == 1) {
			luxel_size = v;
			Com_Verbose("Luxel size: %d\n", luxel_size);
		}
	}

	if (patch_size == DEFAULT_BSP_PATCH_SIZE) {
		float v;
		if (Cm_EntityVector(e, "patch_size", &v, 1) == 1) {
			patch_size = v;
			Com_Verbose("Patch size: %d\n", patch_size);
		}
	}

	// build patches
	BuildPatches(entities);

	// subdivide patches to the desired resolution
	Work("Building patches", SubdividePatch, bsp_file.num_faces);

	// build lightmaps
	BuildLightmaps();

	// build lightgrid
	const size_t num_lightgrid = BuildLightgrid();

	// build lights out of patches and entities
	BuildDirectLights(entities);

	// ambient and diffuse lighting
	Work("Direct lightmaps", DirectLightmap, bsp_file.num_faces);
	Work("Direct lightgrid", DirectLightgrid, (int32_t) num_lightgrid);

	if (indirect) {
		for (bounce = 0; bounce < num_bounces; bounce++) {

			// build indirect lights from lightmapped patches
			BuildIndirectLights();

			// calculate indirect lighting
			Work("Indirect lightmaps", IndirectLightmap, bsp_file.num_faces);
			Work("Indirect lightgrid", IndirectLightgrid, (int32_t) num_lightgrid);
		}
	}

	// finalize it and write it to per-face textures
	Work("Finalizing lightmaps", FinalizeLightmap, bsp_file.num_faces);
	Work("Finalizing lightgrid", FinalizeLightgrid, (int32_t) num_lightgrid);
	
	if (lights) {
		g_array_free(lights, true);
		lights = NULL;
	}

	// generate atlased lightmaps
	EmitLightmap();

	// and vertex lightmap texcoords
	EmitLightmapTexcoords();

	// and lightgrid
	EmitLightgrid();

	g_list_free_full(entities, Mem_Free);

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

	BuildTextureColors();

	LightWorld();

	FreeTextureColors();

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
