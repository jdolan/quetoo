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

int numportals;
int portalclusters;

portal_t *portals;
leaf_t *leafs;

byte *uncompressedvis;

static byte *vismap, *vismap_p, *vismap_end;	// past visfile
static int originalvismapsize;

static int leafbytes;						  // (portalclusters+63)>>3
static int leaflongs;

int portalbytes;
int portallongs;

qboolean fastvis;
qboolean nosort;

int testlevel = 2;

static int totalvis;

portal_t *sorted_portals[MAX_BSP_PORTALS * 2];


static void PlaneFromWinding(const winding_t * w, plane_t * plane){
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
static winding_t *NewWinding(int points){
	winding_t *w;
	size_t size;

	if(points > MAX_POINTS_ON_WINDING)
		Com_Error(ERR_FATAL, "NewWinding: %i points\n", points);

	size = (size_t)((winding_t *) 0)->points[points];
	w = Z_Malloc(size);
	memset(w, 0, size);

	return w;
}

/*
 * SortPortals
 *
 * Sorts the portals from the least complex, so the later ones can reuse
 * the earlier information.
 */
static int PComp(const void *a, const void *b){
	if((*(portal_t **) a)->nummightsee == (*(portal_t **) b)->nummightsee)
		return 0;
	if((*(portal_t **) a)->nummightsee < (*(portal_t **) b)->nummightsee)
		return -1;
	return 1;
}
static void SortPortals(void){
	int i;

	for(i = 0; i < numportals * 2; i++)
		sorted_portals[i] = &portals[i];

	if(nosort)
		return;
	qsort(sorted_portals, numportals * 2, sizeof(sorted_portals[0]), PComp);
}


/*
 * LeafVectorFromPortalVector
 */
static int LeafVectorFromPortalVector(byte * portalbits, byte * leafbits){
	int i;
	int c_leafs;

	memset(leafbits, 0, leafbytes);

	for(i = 0; i < numportals * 2; i++){
		if(portalbits[i >> 3] & (1 << (i & 7))){
			const portal_t *p = portals + i;
			leafbits[p->leaf >> 3] |= (1 << (p->leaf & 7));
		}
	}

	c_leafs = CountBits(leafbits, portalclusters);

	return c_leafs;
}


/*
 * ClusterMerge
 *
 * Merges the portal visibility for a leaf
 */
static void ClusterMerge(int leafnum){
	leaf_t *leaf;
	byte portalvector[MAX_PORTALS / 8];
	byte uncompressed[MAX_BSP_LEAFS / 8];
	byte compressed[MAX_BSP_LEAFS / 8];
	int i, j;
	int numvis;
	byte *dest;
	portal_t *p;
	int pnum;

	// OR together all the portalvis bits

	memset(portalvector, 0, portalbytes);
	leaf = &leafs[leafnum];
	for(i = 0; i < leaf->numportals; i++){
		p = leaf->portals[i];
		if(p->status != stat_done)
			Com_Error(ERR_FATAL, "portal not done\n");
		for(j = 0; j < portallongs; j++)
			((long *)portalvector)[j] |= ((long *)p->portalvis)[j];
		pnum = p - portals;
		portalvector[pnum >> 3] |= 1 << (pnum & 7);
	}

	// convert portal bits to leaf bits
	numvis = LeafVectorFromPortalVector(portalvector, uncompressed);

	if(uncompressed[leafnum >> 3] & (1 << (leafnum & 7)))
		Com_Warn("Leaf portals saw into leaf\n");

	uncompressed[leafnum >> 3] |= (1 << (leafnum & 7));
	numvis++;						  // count the leaf itself

	// save uncompressed for PHS calculation
	memcpy(uncompressedvis + leafnum * leafbytes, uncompressed, leafbytes);

	// compress the bit string
	Com_Debug("cluster %4i : %4i visible\n", leafnum, numvis);
	totalvis += numvis;

	i = CompressVis(uncompressed, compressed);

	dest = vismap_p;
	vismap_p += i;

	if(vismap_p > vismap_end)
		Com_Error(ERR_FATAL, "Vismap expansion overflow\n");

	dvis->bitofs[leafnum][DVIS_PVS] = dest - vismap;

	memcpy(dest, compressed, i);
}


/*
 * CalcPortalVis
 */
static void CalcPortalVis(void){
	int i;

	// fastvis just uses mightsee for a very loose bound
	if(fastvis){
		for(i = 0; i < numportals * 2; i++){
			portals[i].portalvis = portals[i].portalflood;
			portals[i].status = stat_done;
		}
		return;
	}

	RunThreadsOn(numportals * 2, true, PortalFlow);
}


/*
 * CalcVis
 */
static void CalcVis(void){
	int i;

	RunThreadsOn(numportals * 2, true, BasePortalVis);

	SortPortals();

	CalcPortalVis();

	// assemble the leaf vis lists by oring and compressing the portal lists
	for(i = 0; i < portalclusters; i++)
		ClusterMerge(i);

	Com_Print("Average clusters visible: %i\n", totalvis / portalclusters);
}


/*
 * SetPortalSphere
 */
static void SetPortalSphere(portal_t * p){
	int i;
	vec3_t total, dist;
	winding_t *w;
	float r, bestr;

	w = p->winding;
	VectorCopy(vec3_origin, total);
	for(i = 0; i < w->numpoints; i++){
		VectorAdd(total, w->points[i], total);
	}

	for(i = 0; i < 3; i++)
		total[i] /= w->numpoints;

	bestr = 0;
	for(i = 0; i < w->numpoints; i++){
		VectorSubtract(w->points[i], total, dist);
		r = VectorLength(dist);
		if(r > bestr)
			bestr = r;
	}
	VectorCopy(total, p->origin);
	p->radius = bestr;
}


/*
 * LoadPortals
 */
static void LoadPortals(const char *name){
	int i, j;
	portal_t *p;
	leaf_t *l;
	char magic[80];
	FILE *f;
	int numpoints;
	winding_t *w;
	int leafnums[2];
	plane_t plane;

	if(Fs_OpenFile(name, &f, FILE_READ) == -1)
		Com_Error(ERR_FATAL, "Could not open %s\n", name);

	if(fscanf(f, "%79s\n%i\n%i\n", magic, &portalclusters, &numportals) != 3)
		Com_Error(ERR_FATAL, "LoadPortals: failed to read header\n");
	if(strcmp(magic, PORTALFILE))
		Com_Error(ERR_FATAL, "LoadPortals: not a portal file\n");

	Com_Verbose("Loading %4i portals, %4i portalclusters..\n",
			numportals, portalclusters);

	// these counts should take advantage of 64 bit systems automatically
	leafbytes = ((portalclusters + 63) & ~63) >> 3;
	leaflongs = leafbytes / sizeof(long);

	portalbytes = ((numportals * 2 + 63) & ~63) >> 3;
	portallongs = portalbytes / sizeof(long);

	// each file portal is split into two memory portals
	portals = Z_Malloc(2 * numportals * sizeof(portal_t));
	memset(portals, 0, 2 * numportals * sizeof(portal_t));

	leafs = Z_Malloc(portalclusters * sizeof(leaf_t));
	memset(leafs, 0, portalclusters * sizeof(leaf_t));

	originalvismapsize = portalclusters * leafbytes;
	uncompressedvis = Z_Malloc(originalvismapsize);

	vismap = vismap_p = dvisdata;
	dvis->numclusters = portalclusters;
	vismap_p = (byte *) & dvis->bitofs[portalclusters];

	vismap_end = vismap + MAX_BSP_VISIBILITY;

	for(i = 0, p = portals; i < numportals; i++){
		if(fscanf(f, "%i %i %i ", &numpoints, &leafnums[0], &leafnums[1]) != 3)
			Com_Error(ERR_FATAL, "LoadPortals: reading portal %i\n", i);
		if(numpoints > MAX_POINTS_ON_WINDING)
			Com_Error(ERR_FATAL, "LoadPortals: portal %i has too many points\n", i);
		if((unsigned)leafnums[0] > portalclusters
		        || (unsigned)leafnums[1] > portalclusters)
			Com_Error(ERR_FATAL, "LoadPortals: reading portal %i\n", i);

		w = p->winding = NewWinding(numpoints);
		w->original = true;
		w->numpoints = numpoints;

		for(j = 0; j < numpoints; j++){
			double v[3];
			int k;

			// scanf into double, then assign to vec_t
			// so we don't care what size vec_t is
			if(fscanf(f, "(%lf %lf %lf ) ", &v[0], &v[1], &v[2]) != 3)
				Com_Error(ERR_FATAL, "LoadPortals: reading portal %i\n", i);
			for(k = 0; k < 3; k++)
				w->points[j][k] = v[k];
		}
		if(fscanf(f, "\n")) {}

		// calc plane
		PlaneFromWinding(w, &plane);

		// create forward portal
		l = &leafs[leafnums[0]];
		if(l->numportals == MAX_PORTALS_ON_LEAF)
			Com_Error(ERR_FATAL, "Leaf with too many portals\n");
		l->portals[l->numportals] = p;
		l->numportals++;

		p->winding = w;
		VectorSubtract(vec3_origin, plane.normal, p->plane.normal);
		p->plane.dist = -plane.dist;
		p->leaf = leafnums[1];
		SetPortalSphere(p);
		p++;

		// create backwards portal
		l = &leafs[leafnums[1]];
		if(l->numportals == MAX_PORTALS_ON_LEAF)
			Com_Error(ERR_FATAL, "Leaf with too many portals\n");
		l->portals[l->numportals] = p;
		l->numportals++;

		p->winding = NewWinding(w->numpoints);
		p->winding->numpoints = w->numpoints;
		for(j = 0; j < w->numpoints; j++){
			VectorCopy(w->points[w->numpoints - 1 - j], p->winding->points[j]);
		}

		p->plane = plane;
		p->leaf = leafnums[0];
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
static void CalcPHS(void){
	int i, j, k, l, index;
	int bitbyte;
	long *dest, *src;
	byte *scan;
	int count;
	byte uncompressed[MAX_BSP_LEAFS / 8];
	byte compressed[MAX_BSP_LEAFS / 8];

	Com_Verbose("Building PHS...\n");

	count = 0;
	for(i = 0; i < portalclusters; i++){
		scan = uncompressedvis + i * leafbytes;
		memcpy(uncompressed, scan, leafbytes);
		for(j = 0; j < leafbytes; j++){
			bitbyte = scan[j];
			if(!bitbyte)
				continue;
			for(k = 0; k < 8; k++){
				if(!(bitbyte & (1 << k)))
					continue;
				// OR this pvs row into the phs
				index = ((j << 3) + k);
				if(index >= portalclusters)
					Com_Error(ERR_FATAL, "Bad bit in PVS\n");	// pad bits should be 0
				src = (long *)(uncompressedvis + index * leafbytes);
				dest = (long *)uncompressed;
				for(l = 0; l < leaflongs; l++)
					((long *)uncompressed)[l] |= src[l];
			}
		}
		for(j = 0; j < portalclusters; j++)
			if(uncompressed[j >> 3] & (1 << (j & 7)))
				count++;

		// compress the bit string
		j = CompressVis(uncompressed, compressed);

		dest = (long *)vismap_p;
		vismap_p += j;

		if(vismap_p > vismap_end)
			Com_Error(ERR_FATAL, "Vismap expansion overflow\n");

		dvis->bitofs[i][DVIS_PHS] = (byte *) dest - vismap;

		memcpy(dest, compressed, j);
	}

	Com_Print("Average clusters hearable: %i\n", count / portalclusters);
}

/*
 * VIS_Main
 */
int VIS_Main(void){
	char portalfile[MAX_OSPATH];
	double start, end;
	int total_vis_time;

	#ifdef _WIN32
		char title[MAX_OSPATH];
		sprintf(title, "Q2WMap [Compiling VIS]");
		SetConsoleTitle(title);
	#endif

	Com_Print("\n----- VIS -----\n\n");

	start = time(NULL);

	LoadBSPFile(bspname);
	if(numnodes == 0 || numfaces == 0)
		Com_Error(ERR_FATAL, "Empty map\n");

	Com_StripExtension(mapname, portalfile);
	strcat(portalfile, ".prt");

	LoadPortals(portalfile);

	CalcVis();

	CalcPHS();

	visdatasize = vismap_p - dvisdata;
	Com_Print("VIS data: %i bytes (compressed from %i bytes)\n", visdatasize,
			originalvismapsize * 2);

	WriteBSPFile(bspname);

	end = time(NULL);
	total_vis_time = (int)(end - start);
	Com_Print("\nVIS Time: ");
	if(total_vis_time > 59)
		Com_Print("%d Minutes ", total_vis_time / 60);
	Com_Print("%d Seconds\n", total_vis_time % 60);

	return 0;
}
