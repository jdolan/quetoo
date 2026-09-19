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

BspFile bspFile;

/**
 * @brief Dumps info about current file
 */
static void PrintBSPFileSizes(void) {

  Com_Verbose("--- Wrote %s ---\n", bspName);

  Com_Verbose("%5i entities      %7i bytes\n", numEntities,
        bspFile.entityStringSize);

  Com_Verbose("%5i materials     %7i bytes\n", bspFile.numMaterials,
        (int32_t) (bspFile.numMaterials * sizeof(BspMaterial)));

  Com_Verbose("%5i planes        %7i bytes\n", bspFile.numPlanes,
        (int32_t) (bspFile.numPlanes * sizeof(BspPlane)));

  Com_Verbose("%5i brush_sides   %7i bytes\n", bspFile.numBrushSides,
        (int32_t) (bspFile.numBrushSides * sizeof(BspBrushSide)));

  Com_Verbose("%5i brushes       %7i bytes\n", bspFile.numBrushes,
        (int32_t) (bspFile.numBrushes * sizeof(BspBrush)));

  Com_Verbose("%5i vertexes      %7i bytes\n", bspFile.numVertexes,
        (int32_t) (bspFile.numVertexes * sizeof(BspVertex)));

  Com_Verbose("%5i elements      %7i bytes\n", bspFile.numElements,
        (int32_t) (bspFile.numElements * sizeof(int32_t)));

  Com_Verbose("%5i faces         %7i bytes\n", bspFile.numFaces,
        (int32_t) (bspFile.numFaces * sizeof(BspFace)));

  Com_Verbose("%5i nodes         %7i bytes\n", bspFile.numNodes,
        (int32_t) (bspFile.numNodes * sizeof(BspNode)));

  Com_Verbose("%5i leaf_brushes  %7i bytes\n", bspFile.numLeafBrushes,
        (int32_t) (bspFile.numLeafBrushes * sizeof(bspFile.leafBrushes[0])));

  Com_Verbose("%5i leafs         %7i bytes\n", bspFile.numLeafs,
        (int32_t) (bspFile.numLeafs * sizeof(BspLeaf)));

  Com_Verbose("%5i models        %7i bytes\n", bspFile.numModels,
        (int32_t) (bspFile.numModels * sizeof(BspModel)));

  Com_Verbose("%5i patches       %7i bytes\n", bspFile.numPatches,
        (int32_t) (bspFile.numPatches * sizeof(BspPatch)));

  Com_Verbose("      voxels        %7i bytes\n", bspFile.voxelsSize);
}

/**
 * @brief Loads the specified lumps from a BSP file into the global `bspFile` structure.
 */
void LoadBSPFile(const char *filename, const BspLumpId lumps) {

  memset(&bspFile, 0, sizeof(bspFile));

  BspHeader *file;

  if (Fs_Load(filename, (void **) &file) == -1) {
    Com_Error(ERROR_FATAL, "Failed to load %s\n", filename);
  }

  if (Bsp_Verify(file) == -1) {
    Fs_Free(file);
    Com_Error(ERROR_FATAL, "Failed to verify %s\n", filename);
  }

  Bsp_LoadLumps(file, &bspFile, lumps);
  Fs_Free(file);
}

/**
 * @brief Writes the global `bspFile` structure to the specified BSP file on disk.
 */
void WriteBSPFile(const char *filename) {

  File *file = Fs_OpenWrite(filename);

  Bsp_Write(file, &bspFile);

  Fs_Close(file);

  if (verbose) {
    PrintBSPFileSizes();
  }
}
