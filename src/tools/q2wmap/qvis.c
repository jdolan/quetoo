/*
 * Copyright(c) 1997-2001 Id Software, Inc.
 * Copyright(c) 2002 The Quakeforge Project.
 * Copyright(c) 2006 Quake2World.
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

boolean_t fastvis;
boolean_t nosort;

static int visibility_count;

/*
 * PlaneFromWinding
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

/*
 * NewWinding
 */
static winding_t *NewWinding(unsigned short points) {
	winding_t *w;
	size_t size;

	if (points > MAX_POINTS_ON_WINDING)
		Com_Error(ERR_FATAL, "NewWinding: %i points\n", points);

	size = (size_t) ((winding_t *) 0)->points[points];
	w = Z_Malloc(size);
	memset(w, 0, size);

	return w;
}

/*
 * SortPortals_Compare
 */
static int SortPortals_Compare(const void *a, const void *b) {
	if ((*(portal_t **) a)->num_might_see == (*(portal_t **) b)->num_might_see)
		return 0;
	if ((*(portal_t **) a)->num_might_see < (*(portal_t **) b)->num_might_see)
		return -1;
	return 1;
}

/*
 * SortPortals
 *
 * Sorts the portals from the least complex, so the later ones can reuse
 * the earlier information.
 */
static void SortPortals(void) {
	unsigned int i;

	for (i = 0; i < map_vis.num_portals * 2; i++)
		map_vis.sorted_portals[i] = &map_vis.portals[i];

	if (nosort)
		return;

	qsort(map_vis.sorted_portals, map_vis.num_portals * 2, sizeof(portal_t *),
			SortPortals_Compare);
}

/*
 * LeafVectorFromPortalVector
 */
static size_t LeafVectorFromPortalVector(byte *portalbits, byte *leafbits) {
	unsigned int i;

	memset(leafbits, 0, map_vis.leaf_bytes);

	for (i = 0; i < map_vis.num_portals * 2; i++) {
		if (portalbits[i >> 3] & (1 << (i & 7))) {
			const portal_t *p = map_vis.portals + i;
			leafbits[p->leaf >> 3] |= (1 << (p->leaf & 7));
		}
	}

	return CountBits(leafbits, map_vis.portal_clusters);
}

/*
 * ClusterMerge
 *
 * Merges the portal visibility for a leaf.
 */
static void ClusterMerge(int leaf_num) {
	leaf_t *leaf;
	byte portalvector[MAX_PORTALS / 8];
	byte uncompressed[MAX_BSP_LEAFS / 8];
	byte compressed[MAX_BSP_LEAFS / 8];
	unsigned int i, j;
	int numvis;
	byte *dest;
	portal_t *p;
	int pnum;

	// OR together all the portal vis bits
	memset(portalvector, 0, map_vis.portal_bytes);
	leaf = &map_vis.leafs[leaf_num];
	for (i = 0; i < leaf->num_portals; i++) {
		p = leaf->portals[i];
		if (p->status != stat_done)
			Com_Error(ERR_FATAL, "portal not done\n");
		for (j = 0; j < map_vis.portal_longs; j++)
			((long *) portalvector)[j] |= ((long *) p->vis)[j];
		pnum = p - map_vis.portals;
		portalvector[pnum >> 3] |= 1 << (pnum & 7);
	}

	// convert portal bits to leaf bits
	numvis = LeafVectorFromPortalVector(portalvector, uncompressed);

	if (uncompressed[leaf_num >> 3] & (1 << (leaf_num & 7)))
		Com_Warn("Leaf portals saw into leaf\n");

	uncompressed[leaf_num >> 3] |= (1 << (leaf_num & 7));
	numvis++; // count the leaf itself

	// save uncompressed for PHS calculation
	memcpy(map_vis.uncompressed + leaf_num * map_vis.leaf_bytes, uncompressed, map_vis.leaf_bytes);

	// compress the bit string
	Com_Debug("cluster %4i : %4i visible\n", leaf_num, numvis);
	visibility_count += numvis;

	i = CompressVis(uncompressed, compressed);

	dest = map_vis.pointer;
	map_vis.pointer += i;

	if (map_vis.pointer > map_vis.end)
		Com_Error(ERR_FATAL, "Vismap expansion overflow\n");

	d_vis->bit_offsets[leaf_num][DVIS_PVS] = dest - map_vis.base;

	memcpy(dest, compressed, i);
}

/*
 * CalcVis
 */
static void CalcVis(void) {
	unsigned int i;

	RunThreadsOn(map_vis.num_portals * 2, true, BaseVis);

	SortPortals();

	// fast vis just uses migh_tsee for a very loose bound
	if (fastvis) {
		for (i = 0; i < map_vis.num_portals * 2; i++) {
			map_vis.portals[i].vis = map_vis.portals[i].flood;
			map_vis.portals[i].status = stat_done;
		}
	}
	else {
		RunThreadsOn(map_vis.num_portals * 2, true, FinalVis);
	}

	// assemble the leaf vis lists by oring and compressing the portal lists
	for (i = 0; i < map_vis.portal_clusters; i++)
		ClusterMerge(i);

	Com_Print("Average clusters visible: %i\n",
			visibility_count / map_vis.portal_clusters);
}

/*
 * SetPortalSphere
 */
static void SetPortalSphere(portal_t * p) {
	int i;
	vec3_t total, dist;
	winding_t *w;
	float r, bestr;

	w = p->winding;
	VectorCopy(vec3_origin, total);
	for (i = 0; i < w->num_points; i++) {
		VectorAdd(total, w->points[i], total);
	}

	for (i = 0; i < 3; i++)
		total[i] /= w->num_points;

	bestr = 0;
	for (i = 0; i < w->num_points; i++) {
		VectorSubtract(w->points[i], total, dist);
		r = VectorLength(dist);
		if (r > bestr)
			bestr = r;
	}
	VectorCopy(total, p->origin);
	p->radius = bestr;
}

/*
 * LoadPortals
 */
static void LoadPortals(const char *name) {
	unsigned int i;
	portal_t *p;
	leaf_t *l;
	char magic[80];
	FILE *f;
	int num_points;
	winding_t *w;
	int leaf_nums[2];
	plane_t plane;

	if (Fs_OpenFile(name, &f, FILE_READ) == -1)
		Com_Error(ERR_FATAL, "Could not open %s\n", name);

	memset(&map_vis, 0, sizeof(map_vis));

	if (fscanf(f, "%79s\n%u\n%u\n", magic, &map_vis.portal_clusters,
			&map_vis.num_portals) != 3)
		Com_Error(ERR_FATAL, "LoadPortals: failed to read header\n");

	if (strcmp(magic, PORTALFILE))
		Com_Error(ERR_FATAL, "LoadPortals: not a portal file\n");

	Com_Verbose("Loading %4u portals, %4u clusters...\n", map_vis.num_portals,
			map_vis.portal_clusters);

	// these counts should take advantage of 64 bit systems automatically
	map_vis.leaf_bytes = ((map_vis.portal_clusters + 63) & ~63) >> 3;
	map_vis.leaf_longs = map_vis.leaf_bytes / sizeof(long);

	map_vis.portal_bytes = ((map_vis.num_portals * 2 + 63) & ~63) >> 3;
	map_vis.portal_longs = map_vis.portal_bytes / sizeof(long);

	// each file portal is split into two memory portals
	map_vis.portals = Z_Malloc(2 * map_vis.num_portals * sizeof(portal_t));

	// allocate the leafs
	map_vis.leafs = Z_Malloc(map_vis.portal_clusters * sizeof(leaf_t));

	map_vis.uncompressed_size = map_vis.portal_clusters * map_vis.leaf_bytes;
	map_vis.uncompressed = Z_Malloc(map_vis.uncompressed_size);

	map_vis.base = map_vis.pointer = d_bsp.vis_data;
	d_vis->num_clusters = map_vis.portal_clusters;
	map_vis.pointer = (byte *) &d_vis->bit_offsets[map_vis.portal_clusters];

	map_vis.end = map_vis.base + MAX_BSP_VISIBILITY;

	for (i = 0, p = map_vis.portals; i < map_vis.num_portals; i++) {
		int j;

		if (fscanf(f, "%i %i %i ", &num_points, &leaf_nums[0], &leaf_nums[1]) != 3) {
			Com_Error(ERR_FATAL, "LoadPortals: reading portal %i\n", i);
		}

		if (num_points > MAX_POINTS_ON_WINDING) {
			Com_Error(ERR_FATAL, "LoadPortals: portal %i has too many points\n", i);
		}

		if ((unsigned) leaf_nums[0] > map_vis.portal_clusters
				|| (unsigned) leaf_nums[1] > map_vis.portal_clusters) {
			Com_Error(ERR_FATAL, "LoadPortals: reading portal %i\n", i);
		}

		w = p->winding = NewWinding(num_points);
		w->original = true;
		w->num_points = num_points;

		for (j = 0; j < num_points; j++) {
			double v[3];
			int k;

			// scanf into double, then assign to vec_t
			// so we don't care what size vec_t is
			if (fscanf(f, "(%lf %lf %lf ) ", &v[0], &v[1], &v[2]) != 3)
				Com_Error(ERR_FATAL, "LoadPortals: reading portal %i\n", i);
			for (k = 0; k < 3; k++)
				w->points[j][k] = v[k];
		}
		if (fscanf(f, "\n")) {
		}

		// calc plane
		PlaneFromWinding(w, &plane);

		// create forward portal
		l = &map_vis.leafs[leaf_nums[0]];
		if (l->num_portals == MAX_PORTALS_ON_LEAF)
			Com_Error(ERR_FATAL, "Leaf with too many portals\n");
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
		if (l->num_portals == MAX_PORTALS_ON_LEAF)
			Com_Error(ERR_FATAL, "Leaf with too many portals\n");
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

	Fs_CloseFile(f);
}

/*
 * CalcPHS
 *
 * Calculate the PHS (Potentially Hearable Set)
 * by ORing together all the PVS visible from a leaf
 */
static void CalcPHS(void) {
	unsigned int i, j, k, l, index;
	int bitbyte;
	long *dest, *src;
	byte *scan;
	int count;
	byte uncompressed[MAX_BSP_LEAFS / 8];
	byte compressed[MAX_BSP_LEAFS / 8];

	Com_Verbose("Building PHS...\n");

	count = 0;
	for (i = 0; i < map_vis.portal_clusters; i++) {
		scan = map_vis.uncompressed + i * map_vis.leaf_bytes;
		memcpy(uncompressed, scan, map_vis.leaf_bytes);
		for (j = 0; j < map_vis.leaf_bytes; j++) {
			bitbyte = scan[j];
			if (!bitbyte)
				continue;
			for (k = 0; k < 8; k++) {
				if (!(bitbyte & (1 << k)))
					continue;
				// OR this pvs row into the phs
				index = ((j << 3) + k);
				if (index >= map_vis.portal_clusters)
					Com_Error(ERR_FATAL, "Bad bit in PVS\n"); // pad bits should be 0
				src = (long *) (map_vis.uncompressed + index
						* map_vis.leaf_bytes);
				dest = (long *) uncompressed;
				for (l = 0; l < map_vis.leaf_longs; l++)
					((long *) uncompressed)[l] |= src[l];
			}
		}
		for (j = 0; j < map_vis.portal_clusters; j++)
			if (uncompressed[j >> 3] & (1 << (j & 7)))
				count++;

		// compress the bit string
		j = CompressVis(uncompressed, compressed);

		dest = (long *) map_vis.pointer;
		map_vis.pointer += j;

		if (map_vis.pointer > map_vis.end)
			Com_Error(ERR_FATAL, "CalcPHS: overflow");

		d_vis->bit_offsets[i][DVIS_PHS] = (byte *) dest - map_vis.base;

		memcpy(dest, compressed, j);
	}

	Com_Print("Average clusters hearable: %i\n",
			count / map_vis.portal_clusters);
}

/*
 * VIS_Main
 */
int VIS_Main(void) {
	char portal_file[MAX_OSPATH];
	double start, end;
	int total_vis_time;

#ifdef _WIN32
	char title[MAX_OSPATH];
	sprintf(title, "Q2WMap [Compiling VIS]");
	SetConsoleTitle(title);
#endif

	Com_Print("\n----- VIS -----\n\n");

	start = time(NULL);

	LoadBSPFile(bsp_name);

	if (d_bsp.num_nodes == 0 || d_bsp.num_faces == 0)
		Com_Error(ERR_FATAL, "Empty map\n");

	StripExtension(map_name, portal_file);
	strcat(portal_file, ".prt");

	LoadPortals(portal_file);

	CalcVis();

	CalcPHS();

	d_bsp.vis_data_size = map_vis.pointer - d_bsp.vis_data;
	Com_Print("VIS data: %d bytes (compressed from %z bytes)\n",
			d_bsp.vis_data_size, map_vis.uncompressed_size * 2);

	WriteBSPFile(bsp_name);

	end = time(NULL);
	total_vis_time = (int) (end - start);
	Com_Print("\nVIS Time: ");
	if (total_vis_time > 59)
		Com_Print("%d Minutes ", total_vis_time / 60);
	Com_Print("%d Seconds\n", total_vis_time % 60);

	return 0;
}
