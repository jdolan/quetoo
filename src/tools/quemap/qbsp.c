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
#include "csg.h"
#include "face.h"
#include "leakfile.h"
#include "map.h"
#include "material.h"
#include "portal.h"
#include "tjunction.h"
#include "writebsp.h"
#include "qbsp.h"

bool leaked = false;

float micro_volume = 0.125;

bool no_csg = false;
bool no_detail = false;
bool no_liquid = false;
bool no_merge = false;
bool no_phong = false;
bool no_tjunc = false;
bool no_weld = false;

/**
 * @brief
 */
static void ProcessWorldModel(const entity_t *e, bsp_model_t *out) {

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

  FindPortalBrushSides(tree);

  MakeTreeFaces(tree);

  if (!no_merge) {
    MergeTreeFaces(tree);
  }

  if (!no_tjunc) {
    FixTJunctions(tree);
  }

  out->head_node = EmitNodes(tree);

  FreeTree(tree);
}

/**
 * @brief
 */
static void ProcessInlineModel(const entity_t *e, bsp_model_t *out) {

  csg_brush_t *brushes = MakeBrushes(e->first_brush, e->num_brushes);
  if (!no_csg) {
    brushes = SubtractBrushes(brushes);
  }

  tree_t *tree = BuildTree(brushes);

  MakeTreePortals(tree);

  FindPortalBrushSides(tree);

  MakeTreeFaces(tree);

  if (!no_merge) {
    MergeTreeFaces(tree);
  }

  if (!no_tjunc) {
    FixTJunctions(tree);
  }

  out->head_node = EmitNodes(tree);

  FreeTree(tree);
}

/**
 * @brief
 */
static void ProcessModels(void) {

  for (int32_t i = 0; i < num_entities; i++) {
    const entity_t *e = entities + i;

    if (!e->num_brush_sides) {
      continue;
    }

    const vec3_t origin = VectorForKey(e, "origin", Vec3_Zero());
    Com_Print("%s @ %s\n", ValueForKey(e, "classname", "Unknown"), vtos(origin));

    bsp_model_t *mod = BeginModel(e);
    if (i == 0) {
      ProcessWorldModel(e, mod);
    } else {
      ProcessInlineModel(e, mod);
    }
    EndModel(mod);

    Com_Print("\n");
  }
}

/**
 * @brief
 */
int32_t BSP_Main(void) {

  Com_Print("\n------------------------------------------\n");
  Com_Print("\nCompiling %s from %s\n\n", bsp_name, map_name);

  const uint32_t start = (uint32_t) SDL_GetTicks();

  LoadMaterials();

  Fs_Delete(va("maps/%s.prt", map_base));
  Fs_Delete(va("maps/%s.lin", map_base));

  BeginBSPFile();

  LoadMapFile(map_name);

  EmitPlanes();
  EmitMaterials();
  EmitBrushes();
  EmitEntities();

  ProcessModels();

  EndBSPFile();

  PhongShading();
  TangentVectors();

  WriteBSPFile(bsp_name);

  FreeMaterials();

  FreeWindings();

  for (int32_t tag = MEM_TAG_QBSP; tag < MEM_TAG_QLIGHT; tag++) {
    Mem_FreeTag(tag);
  }

  const uint32_t end = (uint32_t) SDL_GetTicks();
  Com_Print("\nCompiled %s in %d ms\n", bsp_name, (end - start));

  return 0;
}
