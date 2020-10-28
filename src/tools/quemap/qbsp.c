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
_Bool no_liquid = false;
_Bool no_csg = false;
_Bool no_weld = false;
_Bool no_share = false;
_Bool no_tjunc = false;
_Bool only_ents = false;
_Bool leaked = false;

/**
 * @brief
 */
static void ProcessWorldModel(const entity_t *e) {

	csg_brush_t *brushes = MakeBrushes(e->first_brush, e->num_brushes);
	if (!no_csg) {
		brushes = SubtractBrushes(brushes);
	}

	tree_t *tree = BuildTree(brushes);

	MakeTreePortals(tree);

	if (FloodEntities(tree)) {
		FillOutside(tree);
	} else {
		Com_Warn("Map leaked, writing maps/%s.lin\n", map_base);
		leaked = true;

		WriteLeakFile(tree);
	}

	MarkVisibleSides(tree, e->first_brush, e->num_brushes);

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
static void ProcessInlineModel(const entity_t *e) {

	csg_brush_t *brushes = MakeBrushes(e->first_brush, e->num_brushes);
	if (!no_csg) {
		brushes = SubtractBrushes(brushes);
	}

	tree_t *tree = BuildTree(brushes);

	MakeTreePortals(tree);

	MarkVisibleSides(tree, e->first_brush, e->num_brushes);

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

	for (int32_t i = 0; i < num_entities; i++) {
		const entity_t *e = entities + i;

		if (!e->num_brushes) {
			continue;
		}

		const vec3_t origin = VectorForKey(e, "origin", Vec3_Zero());
		Com_Print("%s @ %s\n", ValueForKey(e, "classname", "Unknown"), vtos(origin));

		BeginModel(e);
		if (i == 0) {
			ProcessWorldModel(e);
		} else {
			ProcessInlineModel(e);
		}
		EndModel();

		Com_Print("\n");
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
