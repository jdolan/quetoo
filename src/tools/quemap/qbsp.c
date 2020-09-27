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

#include "bsp.h"
#include "face.h"
#include "leakfile.h"
#include "map.h"
#include "material.h"
#include "portal.h"
#include "prtfile.h"
#include "tjunction.h"
#include "writebsp.h"
#include "qbsp.h"

float micro_volume = 0.125;

_Bool no_prune = false;
_Bool no_merge = false;
_Bool no_detail = false;
_Bool all_structural = false;
_Bool only_ents = false;
_Bool no_water = false;
_Bool no_csg = false;
_Bool no_weld = false;
_Bool no_share = false;
_Bool no_tjunc = false;
_Bool leak_test = false;
_Bool leaked = false;

int32_t entity_num;

/**
 * @brief
 */
static void ProcessWorldModel(void) {

	const entity_t *e = &entities[entity_num];

	const int32_t start = e->first_brush;
	const int32_t end = start + e->num_brushes;

	const vec3_t mins = Vec3(MIN_WORLD_COORD, MIN_WORLD_COORD, MIN_WORLD_COORD);
	const vec3_t maxs = Vec3(MAX_WORLD_COORD, MAX_WORLD_COORD, MAX_WORLD_COORD);

	csg_brush_t *brushes = MakeBrushes(start, end, mins, maxs);
	if (!no_csg) {
		brushes = SubtractBrushes(brushes);
	}

	tree_t *tree = BuildTree(brushes, mins, maxs);

	MakeTreePortals(tree);

	if (FloodEntities(tree)) {
		FillOutside(tree->head_node);
	} else {
		Com_Warn("Map leaked, writing maps/%s.lin\n", map_base);
		leaked = true;

		LeakFile(tree);

		if (leak_test) {
			Com_Error(ERROR_FATAL, "Leak test failed");
		}
	}

	MarkVisibleSides(tree, start, end);

	FloodAreas(tree);

	MakeTreeFaces(tree);

	if (!no_prune) {
		PruneNodes(tree->head_node);
	}

	if (!no_tjunc) {
		FixTJunctions(tree->head_node);
	}

	EmitNodes(tree->head_node);

	if (!leaked) {
		WritePortalFile(tree);
	}

	FreeTree(tree);
}

/**
 * @brief
 */
static void ProcessInlineModel(void) {

	const entity_t *e = &entities[entity_num];

	const int32_t start = e->first_brush;
	const int32_t end = start + e->num_brushes;

	const vec3_t mins = Vec3(MIN_WORLD_COORD, MIN_WORLD_COORD, MIN_WORLD_COORD);
	const vec3_t maxs = Vec3(MAX_WORLD_COORD, MAX_WORLD_COORD, MAX_WORLD_COORD);

	csg_brush_t *brushes = MakeBrushes(start, end, mins, maxs);
	if (!no_csg) {
		brushes = SubtractBrushes(brushes);
	}

	tree_t *tree = BuildTree(brushes, mins, maxs);

	MakeTreePortals(tree);
	MarkVisibleSides(tree, start, end);
	MakeTreeFaces(tree);

	if (!no_prune) {
		PruneNodes(tree->head_node);
	}

	if (!no_tjunc) {
		FixTJunctions(tree->head_node);
	}

	EmitNodes(tree->head_node);
	FreeTree(tree);
}

/**
 * @brief
 */
static void ProcessModels(void) {

	for (entity_num = 0; entity_num < num_entities; entity_num++) {

		if (!entities[entity_num].num_brushes) {
			continue;
		}

		Com_Verbose("############### model %i ###############\n", bsp_file.num_models);
		BeginModel();
		if (entity_num == 0) {
			ProcessWorldModel();
		} else {
			ProcessInlineModel();
		}
		EndModel();
	}
}

/**
 * @brief
 */
int32_t BSP_Main(void) {

	Com_Print("\n------------------------------------------\n");
	Com_Print("\nCompiling %s from %s\n\n", bsp_name, map_name);

	const uint32_t start = SDL_GetTicks();

	LoadMaterials(va("maps/%s.mat", map_base), ASSET_CONTEXT_TEXTURES, NULL);

	if (only_ents) {

		LoadBSPFile(bsp_name, BSP_LUMPS_ALL);

		LoadMapFile(map_name);

		EmitEntities();
	} else {

		Fs_Delete(va("maps/%s.prt", map_base));
		Fs_Delete(va("maps/%s.lin", map_base));

		BeginBSPFile();

		LoadMapFile(map_name);

		ProcessModels();

		EndBSPFile();
	}

	WriteBSPFile(va("maps/%s.bsp", map_base));

	FreeMaterials();

	FreeWindings();

	for (int32_t tag = MEM_TAG_QBSP; tag < MEM_TAG_QVIS; tag++) {
		Mem_FreeTag(tag);
	}

	const uint32_t end = SDL_GetTicks();
	Com_Print("\nCompiled %s in %d ms\n", bsp_name, (end - start));

	return 0;
}
