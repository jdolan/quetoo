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

#include "qvis.h"

map_vis_t map_vis;

_Bool fast_vis = false;
_Bool no_sort = false;

static int32_t visibility_count;

/**
 * @brief
 */
static void PlaneFromWinding(const winding_t *w, plane_t *plane) {
	vec3_t v1, v2;

	// calc plane
	VectorSubtract(w->points[2], w->points[1], v1);
	VectorSubtract(w->points[0], w->points[1], v2);
	CrossProduct(v2, v1, plane->normal);
	VectorNormalize(plane->normal);
	plane->dist = DotProduct(w->points[0], plane->normal);
}

/**
 * @brief
 */
static winding_t *NewWinding(uint16_t points) {
	winding_t *w;
	size_t size;

	if (points > MAX_POINTS_ON_WINDING) {
		Com_Error(ERROR_FATAL, "MAX_POINTS_ON_WINDING\n");
	}

	size = (size_t) ((winding_t *) 0)->points[points];
	w = Mem_TagMalloc(size, MEM_TAG_WINDING);

	return w;
}

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
 * @brief Sorts the portals from the least complex, so the later ones can reuse
 * the earlier information.
 */
static void SortPortals(void) {
	uint32_t i;

	for (i = 0; i < map_vis.num_portals * 2; i++) {
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
static size_t LeafVectorFromPortalVector(byte *portalbits, byte *leafbits) {
	uint32_t i;

	memset(leafbits, 0, map_vis.leaf_bytes);

	for (i = 0; i < map_vis.num_portals * 2; i++) {
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
	leaf_t *leaf;
	byte portalvector[MAX_BSP_PORTALS / 8];
	byte uncompressed[MAX_BSP_LEAFS / 8];
	byte compressed[MAX_BSP_LEAFS / 8];
	uint32_t i, j;
	size_t numvis;
	byte *dest;
	portal_t *p;
	ptrdiff_t pnum;

	if (map_vis.portal_bytes > sizeof(portalvector)) {
		Com_Error(ERROR_FATAL, "VIS overflow. Try making more brushes CONTENTS_DETAIL.\n");
	}

	// OR together all the portal vis bits
	memset(portalvector, 0, map_vis.portal_bytes);
	leaf = &map_vis.leafs[leaf_num];
	for (i = 0; i < leaf->num_portals; i++) {
		p = leaf->portals[i];
		if (p->status != stat_done) {
			Com_Error(ERROR_FATAL, "Portal not done\n");
		}
		for (j = 0; j < map_vis.portal_bytes; j++) {
			portalvector[j] |= p->vis[j];
		}
		pnum = p - map_vis.portals;
		portalvector[pnum >> 3] |= 1 << (pnum & 7);
	}

	// convert portal bits to leaf bits
	numvis = LeafVectorFromPortalVector(portalvector, uncompressed);

	if (uncompressed[leaf_num >> 3] & (1 << (leaf_num & 7))) {
		Com_Warn("Leaf portals saw into leaf\n");
	}

	uncompressed[leaf_num >> 3] |= (1 << (leaf_num & 7));
	numvis++; // count the leaf itself

	// save uncompressed for PHS calculation
	memcpy(map_vis.uncompressed + leaf_num * map_vis.leaf_bytes, uncompressed, map_vis.leaf_bytes);

	// compress the bit string
	Com_Debug(DEBUG_ALL, "Cluster %4i : %4" PRIuPTR " visible\n", leaf_num, numvis);
	visibility_count += numvis;

	i = Bsp_CompressVis(&bsp_file, uncompressed, compressed);

	dest = map_vis.pointer;
	map_vis.pointer += i;

	if (map_vis.pointer > map_vis.end) {
		Com_Error(ERROR_FATAL, "VIS expansion overflow\n");
	}

	bsp_file.vis_data.vis->bit_offsets[leaf_num][DVIS_PVS] = (int32_t) (ptrdiff_t) (dest - map_vis.base);

	memcpy(dest, compressed, i);
}

/**
 * @brief
 */
static void CalcVis(void) {
	uint32_t i;

	RunThreadsOn(map_vis.num_portals * 2, true, BaseVis);

	SortPortals();

	// fast vis just uses the flood result for a very loose bound
	if (fast_vis) {
		for (i = 0; i < map_vis.num_portals * 2; i++) {
			map_vis.portals[i].vis = map_vis.portals[i].flood;
			map_vis.portals[i].status = stat_done;
		}
	} else {
		RunThreadsOn(map_vis.num_portals * 2, true, FinalVis);
	}

	// assemble the leaf vis lists by OR-ing and compressing the portal lists
	for (i = 0; i < map_vis.portal_clusters; i++) {
		ClusterMerge(i);
	}

	if (map_vis.portal_clusters) {
		Com_Print("Average clusters visible: %i\n", visibility_count / map_vis.portal_clusters);
	} else {
		Com_Print("Average clusters visible: 0\n");
	}
}

/**
 * @brief
 */
static void SetPortalSphere(portal_t *p) {
	int32_t i;
	vec3_t total, dist;
	winding_t *w;
	vec_t r, bestr;

	w = p->winding;
	VectorClear(total);
	for (i = 0; i < w->num_points; i++) {
		VectorAdd(total, w->points[i], total);
	}

	for (i = 0; i < 3; i++) {
		total[i] /= w->num_points;
	}

	bestr = 0;
	for (i = 0; i < w->num_points; i++) {
		VectorSubtract(w->points[i], total, dist);
		r = VectorLength(dist);
		if (r > bestr) {
			bestr = r;
		}
	}
	VectorCopy(total, p->origin);
	p->radius = bestr;
}

/**
 * @brief
 */
static void LoadPortals(const char *filename) {
	uint32_t i;
	portal_t *p;
	leaf_t *l;
	char magic[80];
	char *buffer, *s;
	int32_t len;
	int32_t num_points;
	winding_t *w;
	int32_t leaf_nums[2];
	plane_t plane;

	if (Fs_Load(filename, (void **) &buffer) == -1) {
		Com_Error(ERROR_FATAL, "Could not open %s\n", filename);
	}

	s = buffer;

	memset(&map_vis, 0, sizeof(map_vis));

	if (sscanf(s, "%79s\n%u\n%u\n%n", magic, &map_vis.portal_clusters, &map_vis.num_portals, &len)
	        != 3) {
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

	map_vis.base = map_vis.pointer = bsp_file.vis_data.raw;
	bsp_file.vis_data.vis->num_clusters = map_vis.portal_clusters;
	map_vis.pointer = (byte *) &bsp_file.vis_data.vis->bit_offsets[map_vis.portal_clusters];

	map_vis.end = map_vis.base + MAX_BSP_VISIBILITY;

	for (i = 0, p = map_vis.portals; i < map_vis.num_portals; i++) {
		int32_t j;

		if (sscanf(s, "%i %i %i %n", &num_points, &leaf_nums[0], &leaf_nums[1], &len) != 3) {
			Com_Error(ERROR_FATAL, "Failed to read portal %i\n", i);
		}
		s += len;

		if (num_points > MAX_POINTS_ON_WINDING) {
			Com_Error(ERROR_FATAL, "Portal %i has too many points\n", i);
		}

		if ((uint32_t) leaf_nums[0] > map_vis.portal_clusters || (uint32_t) leaf_nums[1]
		        > map_vis.portal_clusters) {
			Com_Error(ERROR_FATAL, "Portal %i has invalid leafs\n", i);
		}

		w = p->winding = NewWinding(num_points);
		w->original = true;
		w->num_points = num_points;

		for (j = 0; j < num_points; j++) {
			double v[3];
			int32_t k;

			// scanf into double, then assign to vec_t
			// so we don't care what size vec_t is
			if (sscanf(s, "(%lf %lf %lf ) %n", &v[0], &v[1], &v[2], &len) != 3) {
				Com_Error(ERROR_FATAL, "Failed to read portal vertex definition %i:%i\n", i, j);
			}
			s += len;

			for (k = 0; k < 3; k++) {
				w->points[j][k] = v[k];
			}
		}
		if (sscanf(s, "\n%n", &len)) {
			s += len;
		}

		// calc plane
		PlaneFromWinding(w, &plane);

		// create forward portal
		l = &map_vis.leafs[leaf_nums[0]];
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

		p->winding = NewWinding(w->num_points);
		p->winding->num_points = w->num_points;
		for (j = 0; j < w->num_points; j++) {
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
	uint32_t i, j, k, l, index;
	int32_t bitbyte;
	byte *dest, *src;
	byte *scan;
	int32_t count;
	byte uncompressed[MAX_BSP_LEAFS / 8];
	byte compressed[MAX_BSP_LEAFS / 8];

	Com_Verbose("Building PHS...\n");

	count = 0;
	for (i = 0; i < map_vis.portal_clusters; i++) {
		scan = map_vis.uncompressed + i * map_vis.leaf_bytes;
		memcpy(uncompressed, scan, map_vis.leaf_bytes);
		for (j = 0; j < map_vis.leaf_bytes; j++) {
			bitbyte = scan[j];
			if (!bitbyte) {
				continue;
			}
			for (k = 0; k < 8; k++) {
				if (!(bitbyte & (1 << k))) {
					continue;
				}
				// OR this pvs row into the phs
				index = ((j << 3) + k);
				if (index >= map_vis.portal_clusters) {
					Com_Error(ERROR_FATAL, "Bad bit vector in PVS\n");    // pad bits should be 0
				}
				src = (map_vis.uncompressed + index * map_vis.leaf_bytes);
				for (l = 0; l < map_vis.leaf_bytes; l++) {
					uncompressed[l] |= src[l];
				}
			}
		}
		for (j = 0; j < map_vis.portal_clusters; j++)
			if (uncompressed[j >> 3] & (1 << (j & 7))) {
				count++;
			}

		// compress the bit string
		j = Bsp_CompressVis(&bsp_file, uncompressed, compressed);

		dest = map_vis.pointer;
		map_vis.pointer += j;

		if (map_vis.pointer > map_vis.end) {
			Com_Error(ERROR_FATAL, "Overflow\n");
		}

		bsp_file.vis_data.vis->bit_offsets[i][DVIS_PHS] = (int32_t) (ptrdiff_t) (dest - map_vis.base);

		memcpy(dest, compressed, j);
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

	Com_Print("\n----- VIS -----\n\n");

	const time_t start = time(NULL);

	const int32_t version = LoadBSPFile(bsp_name, BSP_LUMPS_ALL);

	if (bsp_file.num_nodes == 0 || bsp_file.num_faces == 0) {
		Com_Error(ERROR_FATAL, "Empty map\n");
	}

	LoadPortals(va("maps/%s.prt", map_base));

	CalcVis();

	CalcPHS();

	bsp_file.vis_data_size = (int32_t) (ptrdiff_t) (map_vis.pointer - bsp_file.vis_data.raw);
	Com_Print("VIS data: %d bytes (compressed from %u bytes)\n", bsp_file.vis_data_size,
	          (uint32_t) (map_vis.uncompressed_size * 2));

	WriteBSPFile(va("maps/%s.bsp", map_base), version);

	const time_t end = time(NULL);
	const time_t duration = end - start;
	Com_Print("\nVIS Time: ");
	if (duration > 59) {
		Com_Print("%d Minutes ", (int32_t) (duration / 60));
	}
	Com_Print("%d Seconds\n", (int32_t) (duration % 60));

	return 0;
}
