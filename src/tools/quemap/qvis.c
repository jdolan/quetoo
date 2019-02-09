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

#include <SDL_timer.h>

#include "qvis.h"
#include "vis.h"

vis_t map_vis;

_Bool fast_vis = false;
_Bool no_sort = false;

static int32_t c_vis_clusters;

/**
 * @brief
 */
static int32_t SortPortals_Compare(const void *a, const void *b) {
	if ((*(portal_t **) a)->num_might_see == (*(portal_t **) b)->num_might_see) {
		return 0;
	}
	if ((*(portal_t **) a)->num_might_see < (*(portal_t **) b)->num_might_see) {
		return -1;
	}
	return 1;
}

/**
 * @brief Sorts the portals by flood result in ascending order. This way, portals
 * with simpler visibility information are processed first, and their results
 * can be reused when they are visited by more complex portals later on.
 */
static void SortPortals(void) {

	for (int32_t i = 0; i < map_vis.num_portals * 2; i++) {
		map_vis.sorted_portals[i] = &map_vis.portals[i];
	}

	if (no_sort) {
		return;
	}

	qsort(map_vis.sorted_portals, map_vis.num_portals * 2, sizeof(portal_t *), SortPortals_Compare);
}

/**
 * @brief
 */
static int32_t LeafVectorFromPortalVector(byte *portalbits, byte *leafbits) {

	memset(leafbits, 0, map_vis.leaf_bytes);

	for (int32_t i = 0; i < map_vis.num_portals * 2; i++) {
		if (portalbits[i >> 3] & (1 << (i & 7))) {
			const portal_t *p = map_vis.portals + i;
			leafbits[p->leaf >> 3] |= (1 << (p->leaf & 7));
		}
	}

	return CountBits(leafbits, map_vis.portal_clusters);
}

/**
 * @brief Merges the portal visibility for a leaf.
 */
static void ClusterMerge(uint32_t leaf_num) {
	byte uncompressed[MAX_BSP_LEAFS / 8];
	byte compressed[MAX_BSP_LEAFS / 8];
	byte vector[MAX_BSP_PORTALS / 8];

	if ((size_t) map_vis.portal_bytes > sizeof(vector)) {
		Com_Error(ERROR_FATAL, "VIS overflow. Try making more brushes CONTENTS_DETAIL.\n");
	}

	// OR together all the portal vis bits
	memset(vector, 0, map_vis.portal_bytes);
	const leaf_t *leaf = &map_vis.leafs[leaf_num];
	for (int32_t i = 0; i < leaf->num_portals; i++) {
		portal_t *p = leaf->portals[i];
		if (p->status != STATUS_DONE) {
			Com_Error(ERROR_FATAL, "Portal not done\n");
		}
		for (int32_t j = 0; j < map_vis.portal_bytes; j++) {
			vector[j] |= p->vis[j];
		}
		const ptrdiff_t portal_num = p - map_vis.portals;
		vector[portal_num >> 3] |= 1 << (portal_num & 7);
	}

	// convert portal bits to leaf bits
	int32_t visible = LeafVectorFromPortalVector(vector, uncompressed);

	if (uncompressed[leaf_num >> 3] & (1 << (leaf_num & 7))) {
		Com_Warn("Leaf portals saw into leaf\n");
	}

	uncompressed[leaf_num >> 3] |= (1 << (leaf_num & 7));
	visible++; // count the leaf itself

	// save uncompressed for PHS calculation
	memcpy(map_vis.uncompressed + leaf_num * map_vis.leaf_bytes, uncompressed, map_vis.leaf_bytes);

	// compress the bit string
	Com_Debug(DEBUG_ALL, "Cluster %4d : %4d visible\n", leaf_num, visible);
	c_vis_clusters += visible;

	const int32_t len = Bsp_CompressVis(&bsp_file, uncompressed, compressed);

	byte *dest = map_vis.pointer;
	map_vis.pointer += len;

	if (map_vis.pointer > map_vis.end) {
		Com_Error(ERROR_FATAL, "VIS expansion overflow\n");
	}

	bsp_file.vis->bit_offsets[leaf_num][VIS_PVS] = (int32_t) (ptrdiff_t) (dest - map_vis.base);

	memcpy(dest, compressed, len);
}

/**
 * @brief
 */
static void CalcPVS(void) {

	Work("Base visibility", BaseVis, map_vis.num_portals * 2);

	SortPortals();

	// fast vis just uses the flood result for a very loose bound
	if (fast_vis) {
		for (int32_t i = 0; i < map_vis.num_portals * 2; i++) {
			map_vis.portals[i].vis = map_vis.portals[i].flood;
			map_vis.portals[i].status = STATUS_DONE;
		}
	} else {
		Work("Final visibility", FinalVis, map_vis.num_portals * 2);
	}

	Com_Print("\n");

	// assemble the leaf vis lists by OR-ing and compressing the portal lists
	for (int32_t i = 0; i < map_vis.portal_clusters; i++) {
		ClusterMerge(i);
	}

	if (map_vis.portal_clusters) {
		Com_Print("Average clusters visible: %i\n", c_vis_clusters / map_vis.portal_clusters);
	} else {
		Com_Print("Average clusters visible: 0\n");
	}
}

/**
 * @brief
 */
static void SetPortalSphere(portal_t *p) {
	vec3_t origin;

	const cm_winding_t *w = p->winding;
	VectorClear(origin);

	for (int32_t i = 0; i < w->num_points; i++) {
		VectorAdd(origin, w->points[i], origin);
	}

	for (int32_t i = 0; i < 3; i++) {
		origin[i] /= w->num_points;
	}

	vec_t radius = 0;
	for (int32_t i = 0; i < w->num_points; i++) {
		vec3_t delta;
		VectorSubtract(w->points[i], origin, delta);
		const vec_t r = VectorLength(delta);
		if (r > radius) {
			radius = r;
		}
	}
	
	VectorCopy(origin, p->origin);
	p->radius = radius;
}

/**
 * @brief
 */
static void LoadPortals(const char *filename) {
	char magic[80];
	int32_t len;
	int32_t num_points;
	int32_t leaf_nums[2];

	void *buffer;
	if (Fs_Load(filename, &buffer) == -1) {
		Com_Error(ERROR_FATAL, "Could not open %s\n", filename);
	}

	char *s = buffer;

	memset(&map_vis, 0, sizeof(map_vis));

	if (sscanf(s, "%79s\n%u\n%u\n%n", magic, &map_vis.portal_clusters, &map_vis.num_portals, &len) != 3) {
		Com_Error(ERROR_FATAL, "Failed to read header: %s\n", filename);
	}
	s += len;

	if (g_strcmp0(magic, PORTALFILE)) {
		Com_Error(ERROR_FATAL, "Not a portal file: %s\n", filename);
	}

	Com_Verbose("Loading %4u portals, %4u clusters from %s...\n", map_vis.num_portals,
	            map_vis.portal_clusters, filename);

	// determine the size, in bytes, of the leafs and portals
	map_vis.leaf_bytes = ((map_vis.portal_clusters + 63) & ~63) >> 3;
	map_vis.portal_bytes = ((map_vis.num_portals * 2 + 63) & ~63) >> 3;

	// each file portal is split into two memory portals
	map_vis.portals = Mem_TagMalloc(2 * map_vis.num_portals * sizeof(portal_t), MEM_TAG_PORTAL);

	// allocate the leafs
	map_vis.leafs = Mem_TagMalloc(map_vis.portal_clusters * sizeof(leaf_t), MEM_TAG_PORTAL);

	map_vis.uncompressed_size = map_vis.portal_clusters * map_vis.leaf_bytes;
	map_vis.uncompressed = Mem_TagMalloc(map_vis.uncompressed_size, MEM_TAG_PORTAL);

	// allocate vis data
	Bsp_AllocLump(&bsp_file, BSP_LUMP_VISIBILITY, MAX_BSP_VISIBILITY);

	map_vis.base = (byte *)  bsp_file.vis;
	bsp_file.vis->num_clusters = map_vis.portal_clusters;
	map_vis.pointer = (byte *) &bsp_file.vis->bit_offsets[map_vis.portal_clusters];

	map_vis.end = map_vis.base + MAX_BSP_VISIBILITY;

	portal_t *p = map_vis.portals;
	for (int32_t i = 0; i < map_vis.num_portals; i++) {

		if (sscanf(s, "%i %i %i %n", &num_points, &leaf_nums[0], &leaf_nums[1], &len) != 3) {
			Com_Error(ERROR_FATAL, "Failed to read portal %i\n", i);
		}
		s += len;

		if (leaf_nums[0] > map_vis.portal_clusters || leaf_nums[1] > map_vis.portal_clusters) {
			Com_Error(ERROR_FATAL, "Portal %i has invalid leafs\n", i);
		}

		cm_winding_t *w = p->winding = Cm_AllocWinding(num_points);
		w->num_points = num_points;

		for (int32_t j = 0; j < w->num_points; j++) {
			dvec3_t v;

			// scanf into double, then assign to vec_t so we don't care what size vec_t is
			if (sscanf(s, "(%lf %lf %lf ) %n", &v[0], &v[1], &v[2], &len) != 3) {
				Com_Error(ERROR_FATAL, "Failed to read portal vertex definition %i:%i\n", i, j);
			}
			s += len;

			VectorCopy(v, w->points[j]);
		}
		if (sscanf(s, "\n%n", &len)) {
			s += len;
		}

		// calc plane
		plane_t plane;
		Cm_PlaneForWinding(w, plane.normal, &plane.dist);

		// create forward portal
		leaf_t *l = &map_vis.leafs[leaf_nums[0]];
		if (l->num_portals == MAX_PORTALS_ON_LEAF) {
			Com_Error(ERROR_FATAL, "MAX_PORTALS_ON_LEAF\n");
		}
		l->portals[l->num_portals] = p;
		l->num_portals++;

		p->winding = w;
		VectorSubtract(vec3_origin, plane.normal, p->plane.normal);
		p->plane.dist = -plane.dist;
		p->leaf = leaf_nums[1];
		SetPortalSphere(p);
		p++;

		// create backwards portal
		l = &map_vis.leafs[leaf_nums[1]];
		if (l->num_portals == MAX_PORTALS_ON_LEAF) {
			Com_Error(ERROR_FATAL, "MAX_PORTALS_ON_LEAF\n");
		}
		l->portals[l->num_portals] = p;
		l->num_portals++;

		p->winding = Cm_AllocWinding(w->num_points);
		p->winding->num_points = w->num_points;
		for (int32_t j = 0; j < w->num_points; j++) {
			VectorCopy(w->points[w->num_points - 1 - j], p->winding->points[j]);
		}

		p->plane = plane;
		p->leaf = leaf_nums[0];
		SetPortalSphere(p);
		p++;
	}

	Fs_Free(buffer);
}

/**
 * @brief Calculate the PHS (Potentially Hearable Set)
 * by ORing together all the PVS visible from a leaf
 */
static void CalcPHS(void) {
	byte uncompressed[MAX_BSP_LEAFS / 8];
	byte compressed[MAX_BSP_LEAFS / 8];

	Com_Verbose("Building PHS...\n");

	int32_t count = 0;
	for (int32_t i = 0; i < map_vis.portal_clusters; i++) {
		const byte *scan = map_vis.uncompressed + i * map_vis.leaf_bytes;
		memcpy(uncompressed, scan, map_vis.leaf_bytes);
		for (int32_t j = 0; j < map_vis.leaf_bytes; j++) {
			if (!scan[j]) {
				continue;
			}
			for (int32_t k = 0; k < 8; k++) {
				if (!(scan[j] & (1 << k))) {
					continue;
				}
				// OR this pvs row into the phs
				int32_t index = ((j << 3) + k);
				if (index >= map_vis.portal_clusters) {
					Com_Error(ERROR_FATAL, "Bad bit vector in PVS\n"); // pad bits should be 0
				}
				const byte *src = (map_vis.uncompressed + index * map_vis.leaf_bytes);
				for (int32_t l = 0; l < map_vis.leaf_bytes; l++) {
					uncompressed[l] |= src[l];
				}
			}
		}
		for (int32_t j = 0; j < map_vis.portal_clusters; j++)
			if (uncompressed[j >> 3] & (1 << (j & 7))) {
				count++;
			}

		// compress the bit string
		const int32_t len = Bsp_CompressVis(&bsp_file, uncompressed, compressed);

		byte *dest = map_vis.pointer;
		map_vis.pointer += len;

		if (map_vis.pointer > map_vis.end) {
			Com_Error(ERROR_FATAL, "Overflow\n");
		}

		bsp_file.vis->bit_offsets[i][VIS_PHS] = (int32_t) (ptrdiff_t) (dest - map_vis.base);
		memcpy(dest, compressed, len);
	}

	if (map_vis.portal_clusters) {
		Com_Print("Average clusters hearable: %i\n", count / map_vis.portal_clusters);
	} else {
		Com_Print("Average clusters hearable: 0\n");
	}
}

/**
 * @brief
 */
int32_t VIS_Main(void) {

	Com_Print("\n------------------------------------------\n");
	Com_Print("\nCalculating PVS for %s\n\n", bsp_name);

	const uint32_t start = SDL_GetTicks();

	LoadBSPFile(bsp_name, BSP_LUMPS_ALL);

	if (bsp_file.num_nodes == 0 || bsp_file.num_faces == 0) {
		Com_Error(ERROR_FATAL, "Empty map\n");
	}

	LoadPortals(va("maps/%s.prt", map_base));

	CalcPVS();

	CalcPHS();
	
	Com_Print("\n");

	bsp_file.vis_size = (int32_t) (ptrdiff_t) (map_vis.pointer - map_vis.base);

	Com_Print("Generated %d bytes of PVS data\n", map_vis.uncompressed_size * 2);
	Com_Print("Compressed size is %d bytes\n", bsp_file.vis_size);

	WriteBSPFile(va("maps/%s.bsp", map_base));

	FreeWindings();

	for (int32_t tag = MEM_TAG_QVIS; tag < MEM_TAG_QLIGHT; tag++) {
		Mem_FreeTag(tag);
	}

	const uint32_t end = SDL_GetTicks();
	Com_Print("\nCalculated PVS for %s in %d ms\n", bsp_name, (end - start));

	return 0;
}
