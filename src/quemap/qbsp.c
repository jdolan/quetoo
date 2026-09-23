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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#include "bsp.h"
#include "csg.h"
#include "face.h"
#include "leakfile.h"
#include "map.h"
#include "material.h"
#include "patch.h"
#include "portal.h"
#include "tjunction.h"
#include "writebsp.h"
#include "qbsp.h"

bool leaked = false;

float microVolume = 0.125;

bool noCsg = false;
bool noDetail = false;
bool noLiquid = false;
bool noMerge = false;
bool noPhong = false;
bool noTjunc = false;
bool noWeld = false;

/**
 * @brief Compiles the world model entity, performing CSG, BSP, portal, and face generation.
 */
static void ProcessWorldModel(const Entity *e, BspModel *out) {

  CsgBrush *brushes = MakeBrushes(e->firstBrush, e->numBrushes);

  if (!noCsg) {
    brushes = SubtractBrushes(brushes);
  }

  CsgBrush *faceBrushes = CopyBrushes(brushes);

  Tree *tree = BuildTree(brushes);

  MakeTreePortals(tree);

  if (FloodEntities(tree)) {
    FillOutside(tree);
  } else {
    Com_Warn("Map leaked, writing maps/%s.lin\n", mapBase);
    leaked = true;

    WriteLeakFile(tree);
  }

  MakeTreeFaces(tree, faceBrushes);

  FreeBrushes(faceBrushes);

  if (!noMerge) {
    MergeTreeFaces(tree);
  }

  if (!noTjunc) {
    FixTJunctions(tree);
  }

  TessellatePatches(out->entity);

  AssignPatchFacesToNodes(tree->headNode, out->entity);

  out->headNode = EmitNodes(tree);

  FreeTree(tree);
}

/**
 * @brief Compiles a brush entity as an inline BSP model (e.g. `func_door`, `func_plat`).
 */
static void ProcessInlineModel(const Entity *e, BspModel *out) {

  CsgBrush *brushes = MakeBrushes(e->firstBrush, e->numBrushes);
  if (!noCsg) {
    brushes = SubtractBrushes(brushes);
  }

  CsgBrush *faceBrushes = CopyBrushes(brushes);

  Tree *tree = BuildTree(brushes);

  MakeTreePortals(tree);

  MakeTreeFaces(tree, faceBrushes);

  FreeBrushes(faceBrushes);

  if (!noMerge) {
    MergeTreeFaces(tree);
  }

  if (!noTjunc) {
    FixTJunctions(tree);
  }

  TessellatePatches(out->entity);

  AssignPatchFacesToNodes(tree->headNode, out->entity);

  out->headNode = EmitNodes(tree);

  FreeTree(tree);
}

/**
 * @brief Iterates all brush entities and compiles each as either a world model or an inline model.
 */
static void ProcessModels(void) {

  for (int32_t i = 0; i < numEntities; i++) {
    const Entity *e = entities + i;

    if (!e->numBrushSides) {
      continue;
    }

    const Vec3 origin = VectorForKey(e, "origin", Vec3_Zero());
    Com_Print("%s @ %s\n", ValueForKey(e, "classname", "Unknown"), vtos(origin));

    BspModel *mod = BeginModel(e);
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
 * @brief Entry point for the BSP compilation stage; loads the map, builds the BSP tree, and writes the .bsp file.
 * @return The exit code for the BSP stage.
 */
int32_t BSP_Main(void) {

  Com_Print("\n------------------------------------------\n");
  Com_Print("\nCompiling %s from %s\n\n", bspName, mapName);

  const uint32_t start = (uint32_t) SDL_GetTicks();

  Fs_Delete(va("maps/%s.prt", mapBase));
  Fs_Delete(va("maps/%s.lin", mapBase));

  BeginBSPFile();

  mapFormat = LoadMapFile(mapName);

  Com_Verbose("Map format: %s\n", mapFormat == MAP_FORMAT_VALVE ? "Quake3 (Valve)" : "Quake3");

  EmitPlanes();
  EmitMaterials();
  EmitBrushes();
  EmitEntities();

  ProcessModels();

  EndBSPFile();

  TangentVectors();

  WriteBSPFile(bspName);

  FreeWindings();

  for (int32_t tag = MEM_TAG_QBSP; tag < MEM_TAG_QLIGHT; tag++) {
    Mem_FreeTag(tag);
  }

  const uint32_t end = (uint32_t) SDL_GetTicks();
  Com_Print("\nCompiled %s in %d ms\n", bspName, (end - start));

  return 0;
}
