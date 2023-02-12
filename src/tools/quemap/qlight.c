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

float brightness = 1.f;
float saturation = 1.f;
float contrast = 1.f;

float ambient_intensity = LIGHT_INTENSITY;
float sun_intensity = LIGHT_INTENSITY;
float light_intensity = LIGHT_INTENSITY;
float patch_intensity = LIGHT_INTENSITY;
float indirect_intensity = LIGHT_INTENSITY;

int32_t luxel_size = BSP_LIGHTMAP_LUXEL_SIZE;
int32_t patch_size = DEFAULT_PATCH_SIZE;

float caustics = 1.f;

// we use the collision detection facilities for lighting
static cm_bsp_model_t *bsp_models[MAX_BSP_MODELS];

/**
 * @brief Box trace data encapsulation and context management.
 */
typedef struct {
	/**
	 * @brief The trace start and end points, as provided by the user.
	 */
	vec3_t start, end;

	/**
	 * @brief The absolute bounds of the trace, spanning the start and end points.
	 */
	box3_t abs_bounds;

	/**
	 * @brief The contents mask to collide with, as provided by the user.
	 */
	int32_t contents;

	/**
	 * @brief The brush cache, to avoid multiple tests against the same brush.
	 */
	int32_t brush_cache[128];

	/**
	 * @brief The trace result.
	 */
	cm_trace_t trace;

	/**
	 * @brief The trace fraction not taking any epsilon nudging into account.
	 */
	float unnudged_fraction;
} cm_trace_data_t;

/**
 * @brief
 */
static inline _Bool Light_BrushAlreadyTested(cm_trace_data_t *data, int32_t brush_num) {
	const int32_t hash = brush_num & (lengthof(data->brush_cache) - 1);

	const _Bool skip = (data->brush_cache[hash] == brush_num);

	data->brush_cache[hash] = brush_num;

	return skip;
}

/**
 * @brief Clips the bounded box to all brush sides for the given brush.
 */
static inline void Light_TraceToBrush(cm_trace_data_t *data, const cm_bsp_brush_t *brush) {

	if (!brush->num_brush_sides) {
		return;
	}

	if (!Box3_Intersects(data->abs_bounds, brush->bounds)) {
		return;
	}

	float enter_fraction = -1.f;
	float leave_fraction = 1.f;
	float nudged_enter_fraction = -1.f;

	cm_bsp_plane_t plane = { };
	const cm_bsp_brush_side_t *side = NULL;

	_Bool start_outside = false, end_outside = false;

	const cm_bsp_brush_side_t *s = brush->brush_sides + brush->num_brush_sides - 1;
	for (int32_t i = brush->num_brush_sides - 1; i >= 0; i--, s--) {

		cm_bsp_plane_t *p = s->plane;

		const float dist = p->dist;

		const float d1 = Vec3_Dot(data->start, p->normal) - dist;
		const float d2 = Vec3_Dot(data->end, p->normal) - dist;

		if (d1 > 0.f) {
			start_outside = true;
		}
		if (d2 > 0.f) {
			end_outside = true;
		}

		// if completely in front of plane, the trace does not intersect with the brush
		if (d1 > 0.f && d2 >= d1) {
			return;
		}

		// if completely behind plane, the trace does not intersect with this side
		if (d1 <= 0.f && d2 <= d1) {
			continue;
		}

		// the trace intersects this side
		const float d2d1_dist = (d1 - d2);

		if (d1 > d2) { // enter
			const float f = d1 / d2d1_dist;
			if (f > enter_fraction) {
				enter_fraction = f;
				plane = *p;
				side = s;
				nudged_enter_fraction = (d1 - TRACE_EPSILON) / d2d1_dist;
			}
		} else { // leave
			const float f = d1 / d2d1_dist;
			if (f < leave_fraction) {
				leave_fraction = f;
			}
		}
	}

	// some sort of collision has occurred

	if (!start_outside) { // original point was inside brush
		data->trace.start_solid = true;
		if (!end_outside) {
			data->trace.all_solid = true;
			data->trace.contents = brush->contents;
			data->trace.fraction = 0.f;
			data->unnudged_fraction = 0.f;
		}
	} else if (enter_fraction < leave_fraction) { // pierced brush
		if (enter_fraction > -1.f && enter_fraction < data->unnudged_fraction && nudged_enter_fraction < data->trace.fraction) {
			data->unnudged_fraction = enter_fraction;
			data->trace.fraction = nudged_enter_fraction;
			data->trace.brush_side = side;
			data->trace.plane = plane;
			data->trace.contents = side->contents;
			data->trace.surface = side->surface;
			data->trace.material = side->material;
		}
	}
}

/**
 * @brief
 */
static inline void Light_TraceToLeaf(cm_trace_data_t *data, int32_t leaf_num) {

	const cm_bsp_leaf_t *leaf = &Cm_Bsp()->leafs[leaf_num];

	if (!(leaf->contents & data->contents)) {
		return;
	}

	// trace line against all brushes in the leaf
	for (int32_t i = 0; i < leaf->num_leaf_brushes; i++) {
		const int32_t brush_num = Cm_Bsp()->leaf_brushes[leaf->first_leaf_brush + i];

		if (Light_BrushAlreadyTested(data, brush_num)) {
			continue; // already checked this brush in another leaf
		}

		const cm_bsp_brush_t *b = &Cm_Bsp()->brushes[brush_num];

		if (!(b->contents & data->contents)) {
			continue;
		}

		Light_TraceToBrush(data, b);

		if (data->trace.all_solid) {
			return;
		}
	}
}

/**
 * @brief
 */
static inline void Light_TraceToNode(cm_trace_data_t *data, int32_t num, float p1f, float p2f,
									 const vec3_t p1, const vec3_t p2) {

next:;
	// find the point distances to the separating plane
	// and the offset for the size of the box
	const cm_bsp_node_t *node = Cm_Bsp()->nodes + num;
	const cm_bsp_plane_t plane = *node->plane;

	float d1, d2;
	if (AXIAL(&plane)) {
		d1 = p1.xyz[plane.type] - plane.dist;
		d2 = p2.xyz[plane.type] - plane.dist;
	} else {
		d1 = Vec3_Dot(plane.normal, p1) - plane.dist;
		d2 = Vec3_Dot(plane.normal, p2) - plane.dist;
	}

	// see which sides we need to consider
	if (d1 >= 0 && d2 >= 0) {
		num = node->children[0];

		// if < 0, we are in a leaf node
		if (num < 0) {
			Light_TraceToLeaf(data, -1 - num);
			return;
		}

		goto next;
	}
	if (d1 < -0 && d2 < -0) {
		num = node->children[1];

		// if < 0, we are in a leaf node
		if (num < 0) {
			Light_TraceToLeaf(data, -1 - num);
			return;
		}

		goto next;
	}

	int32_t side;
	float frac1, frac2;

	if (d1 < d2) {
		const float idist = 1.f / (d1 - d2);
		side = 1;
		frac2 = d1 * idist;
		frac1 = d1 * idist;
	} else if (d1 > d2) {
		const float idist = 1.f / (d1 - d2);
		side = 0;
		frac2 = d1 * idist;
		frac1 = d1 * idist;
	} else {
		side = 0;
		frac1 = 1.f;
		frac2 = 0.f;
	}

	// move up to the node if we can potentially hit it
	if (p1f < data->unnudged_fraction) {
		frac1 = Clampf(frac1, 0.f, 1.f);

		const float midf1 = p1f + (p2f - p1f) * frac1;

		const vec3_t mid = Vec3_Mix(p1, p2, frac1);
		
		num = node->children[side];

		// if < 0, we are in a leaf node
		if (num < 0) {
			Light_TraceToLeaf(data, -1 - num);
		} else {
			Light_TraceToNode(data, num, p1f, midf1, p1, mid);
		}
	}

	// go past the node
	frac2 = Clampf(frac2, 0.f, 1.f);

	const float midf2 = p1f + (p2f - p1f) * frac2;

	if (midf2 < data->unnudged_fraction) {
		const vec3_t mid = Vec3_Mix(p1, p2, frac2);
		
		num = node->children[side ^ 1];

		// if < 0, we are in a leaf node
		if (num < 0) {
			Light_TraceToLeaf(data, -1 - num);
		} else {
			Light_TraceToNode(data, num, midf2, p2f, mid, p2);
		}
	}
}

/**
 * @brief Primary collision detection entry point. This function recurses down
 * the BSP tree from the specified head node, clipping the desired movement to
 * brushes that match the specified contents mask.
 *
 * @param start The starting point.
 * @param end The desired end point.
 * @param bounds The bounding box, in model space.
 * @param head_node The BSP head node to recurse down. For inline BSP models,
 * the head node is the root of the model's subtree. For mesh models, a
 * special reserved box hull and head node are used.
 * @param contents The contents mask to clip to.
 * @param matrix The matrix to adjust tested planes by.
 *
 * @return The trace.
 */
static inline cm_trace_t Light_Trace_(vec3_t start, vec3_t end, int32_t head_node, int32_t contents) {

	cm_trace_data_t data;

	data.trace = (cm_trace_t) {
		.fraction = 1.f
	};
	
	data.start = start;
	data.end = end;
	data.abs_bounds = Box3_FromPoints((const vec3_t []) { start, end }, 2);
	data.contents = contents;
	data.unnudged_fraction = 1.f + TRACE_EPSILON;

	memset(data.brush_cache, 0xff, sizeof(data.brush_cache));

	Light_TraceToNode(&data, head_node, 0.f, 1.f, data.start, data.end);

	data.trace.fraction = Maxf(0.f, data.trace.fraction);

	if (data.trace.fraction == 0.f) {
		data.trace.end = data.start;
	} else if (data.trace.fraction == 1.f) {
		data.trace.end = data.end;
	} else {
		data.trace.end = Vec3_Mix(data.start, data.end, data.trace.fraction);
	}

	return data.trace;
}


/**
 * @brief
 */
int32_t Light_PointContents(const vec3_t p, int32_t head_node) {

	int32_t contents = Cm_PointContents(p, 0, Mat4_Identity());

	if (head_node) {
		contents |= Cm_PointContents(p, head_node, Mat4_Identity());
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
	cm_trace_t trace = Light_Trace_(start, end, 0, mask);
	if (trace.start_solid) {
		trace.fraction = 0.f;
	}

	if (head_node) {
		cm_trace_t tr = Light_Trace_(start, end, head_node, mask);
		if (tr.start_solid) {
			tr.fraction = 0.f;
		}
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

	if (brightness == 1.f) {
		brightness = Cm_EntityValue(e, "brightness")->value ?: brightness;
	}

	if (saturation == 1.f) {
		saturation = Cm_EntityValue(e, "saturation")->value ?: saturation;
	}

	if (contrast == 1.f) {
		contrast = Cm_EntityValue(e, "contrast")->value ?: contrast;
	}

	if (ambient_intensity == 1.f) {
		ambient_intensity = Cm_EntityValue(e, "ambient_intensity")->value ?: ambient_intensity;
	}

	if (sun_intensity == 1.f) {
		sun_intensity = Cm_EntityValue(e, "sun_intensity")->value ?: sun_intensity;
	}

	if (light_intensity == 1.f) {
		light_intensity = Cm_EntityValue(e, "light_intensity")->value ?: light_intensity;
	}

	if (patch_intensity == 1.f) {
		patch_intensity = Cm_EntityValue(e, "patch_intensity")->value ?: patch_intensity;
	}

	if (indirect_intensity == 1.f) {
		indirect_intensity = Cm_EntityValue(e, "indirect_intensity")->value ?: indirect_intensity;
	}

	if (luxel_size == BSP_LIGHTMAP_LUXEL_SIZE) {
		luxel_size = Cm_EntityValue(e, "luxel_size")->integer ?: luxel_size;
	}

	if (patch_size == DEFAULT_PATCH_SIZE) {
		patch_size = Cm_EntityValue(e, "patch_size")->integer ?: patch_size;
	}

	if (caustics == 1.f) {
		caustics = Cm_EntityValue(e, "caustics")->value ?: caustics;
	}

	Com_Print("\n");
	Com_Print("Lighting parameters\n");
	Com_Print("  Brightness: %g\n", brightness);
	Com_Print("  Saturation: %g\n", saturation);
	Com_Print("  Contrast: %g\n", contrast);
	Com_Print("  Ambient intensity: %g\n", ambient_intensity);
	Com_Print("  Sun intensity: %g\n", sun_intensity);
	Com_Print("  Light intensity: %g\n", light_intensity);
	Com_Print("  Patch intensity: %g\n", patch_intensity);
	Com_Print("  Indirect intensity: %g\n", indirect_intensity);
	Com_Print("  Caustics intensity: %g\n", caustics);
	Com_Print("  Luxel size: %d\n", luxel_size);
	Com_Print("  Patch size: %d\n", patch_size);
	Com_Print("\n");

	// build patches
	BuildPatches();

	// subdivide patches to the desired resolution
	Work("Building patches", SubdividePatch, bsp_file.num_faces);

	// build lightmaps
	BuildLightmaps();

	// build lightgrid
	const size_t num_lightgrid = BuildLightgrid();

	// build lights out of entities and emissive patches
	BuildDirectLights();

	// calculate direct lighting
	Work("Direct lightmaps", DirectLightmap, bsp_file.num_faces);
	Work("Direct lightgrid", DirectLightgrid, (int32_t) num_lightgrid);

	// indirect lighting
	if (indirect_intensity > 0.f) {

		// build lights out of lightmapped patches
		BuildIndirectLights();

		// calculate indirect lighting
		Work("Indirect lightmaps", IndirectLightmap, bsp_file.num_faces);
		Work("Indirect lightgrid", IndirectLightgrid, (int32_t) num_lightgrid);
	}

	// caustic effects
	Work("Caustics lightmap", CausticsLightmap, bsp_file.num_faces);
	Work("Caustics lightgrid", CausticsLightgrid, (int32_t) num_lightgrid);

	// save the light sources to the BSP
	EmitLights();

	// free the light sources
	FreeLights();

	// free the patches
	Mem_FreeTag(MEM_TAG_PATCH);

	// build fog volumes out of brush entities
	BuildFog();

	// bake fog into the lightgrid
	Work("Fog volumes", FogLightgrid, (int32_t) num_lightgrid);

	// free the fog volumes
	FreeFog();

	// finalize it and write it to per-face textures
	Work("Finalizing lightmaps", FinalizeLightmap, bsp_file.num_faces);

	// generate atlased lightmaps
	EmitLightmap();

	// and vertex lightmap texcoords
	EmitLightmapTexcoords();

	// free the lightmaps
	Mem_FreeTag(MEM_TAG_LIGHTMAP);

	// finalize the lightgrid
	Work("Finalizing lightgrid", FinalizeLightgrid, (int32_t) num_lightgrid);

	// write it to the bsp
	EmitLightgrid();

	// free the lightgrid
	Mem_FreeTag(MEM_TAG_LIGHTGRID);
}

/**
 * @brief
 */
int32_t LIGHT_Main(void) {

	Com_Print("\n------------------------------------------\n");
	Com_Print("\nLighting %s\n\n", bsp_name);

	const uint32_t start = SDL_GetTicks();

	LoadBSPFile(bsp_name, BSP_LUMPS_ALL);

	LoadMaterials(va("maps/%s.mat", map_base));

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
