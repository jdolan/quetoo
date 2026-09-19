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

#include "map.h"
#include "bsp.h"

BspFile bsp_file;

/**
 * @brief Dumps info about current file
 */
static void PrintBSPFileSizes(void) {

  Com_Verbose("--- Wrote %s ---\n", bsp_name);

  Com_Verbose("%5i entities      %7i bytes\n", num_entities,
        bsp_file.entityStringSize);

  Com_Verbose("%5i materials     %7i bytes\n", bsp_file.numMaterials,
        (int32_t) (bsp_file.numMaterials * sizeof(BspMaterial)));

  Com_Verbose("%5i planes        %7i bytes\n", bsp_file.numPlanes,
        (int32_t) (bsp_file.numPlanes * sizeof(BspPlane)));

  Com_Verbose("%5i brush_sides   %7i bytes\n", bsp_file.numBrushSides,
        (int32_t) (bsp_file.numBrushSides * sizeof(BspBrushSide)));

  Com_Verbose("%5i brushes       %7i bytes\n", bsp_file.numBrushes,
        (int32_t) (bsp_file.numBrushes * sizeof(BspBrush)));

  Com_Verbose("%5i vertexes      %7i bytes\n", bsp_file.numVertexes,
        (int32_t) (bsp_file.numVertexes * sizeof(BspVertex)));

  Com_Verbose("%5i elements      %7i bytes\n", bsp_file.numElements,
        (int32_t) (bsp_file.numElements * sizeof(int32_t)));

  Com_Verbose("%5i faces         %7i bytes\n", bsp_file.numFaces,
        (int32_t) (bsp_file.numFaces * sizeof(BspFace)));

  Com_Verbose("%5i nodes         %7i bytes\n", bsp_file.numNodes,
        (int32_t) (bsp_file.numNodes * sizeof(BspNode)));

  Com_Verbose("%5i leaf_brushes  %7i bytes\n", bsp_file.numLeafBrushes,
        (int32_t) (bsp_file.numLeafBrushes * sizeof(bsp_file.leafBrushes[0])));

  Com_Verbose("%5i leafs         %7i bytes\n", bsp_file.numLeafs,
        (int32_t) (bsp_file.numLeafs * sizeof(BspLeaf)));

  Com_Verbose("%5i models        %7i bytes\n", bsp_file.numModels,
        (int32_t) (bsp_file.numModels * sizeof(BspModel)));

  Com_Verbose("%5i patches       %7i bytes\n", bsp_file.numPatches,
        (int32_t) (bsp_file.numPatches * sizeof(BspPatch)));

  Com_Verbose("      voxels        %7i bytes\n", bsp_file.voxelsSize);
}

/**
 * @brief Loads the specified lumps from a BSP file into the global `bsp_file` structure.
 */
void LoadBSPFile(const char *filename, const BspLumpId lumps) {

  memset(&bsp_file, 0, sizeof(bsp_file));

  BspHeader *file;

  if (Fs_Load(filename, (void **) &file) == -1) {
    Com_Error(ERROR_FATAL, "Failed to load %s\n", filename);
  }

  if (Bsp_Verify(file) == -1) {
    Fs_Free(file);
    Com_Error(ERROR_FATAL, "Failed to verify %s\n", filename);
  }

  Bsp_LoadLumps(file, &bsp_file, lumps);
  Fs_Free(file);
}

/**
 * @brief Writes the global `bsp_file` structure to the specified BSP file on disk.
 */
void WriteBSPFile(const char *filename) {

  File *file = Fs_OpenWrite(filename);

  Bsp_Write(file, &bsp_file);

  Fs_Close(file);

  if (verbose) {
    PrintBSPFileSizes();
  }
}
