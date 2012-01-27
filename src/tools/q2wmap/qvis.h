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

#ifndef __QVIS_H__
#define __QVIS_H__

#include "bspfile.h"

#define	MAX_PORTALS	32768

#define	PORTALFILE	"PRT1"

#define	ON_EPSILON	0.1

typedef struct {
	vec3_t normal;
	float dist;
} plane_t;

#define MAX_POINTS_ON_WINDING	64
#define	MAX_POINTS_ON_FIXED_WINDING	12

typedef struct {
	boolean_t original;  // don't free, it's part of the portal
	unsigned short num_points;
	vec3_t points[MAX_POINTS_ON_FIXED_WINDING];  // variable sized
} winding_t;

void FreeWinding(winding_t * w);
winding_t *CopyWinding(winding_t * w);

typedef enum {
	stat_none, stat_working, stat_done
} status_t;

typedef struct {
	plane_t plane;			// normal pointing into neighbor
	int leaf;				// neighbor

	vec3_t origin;			// for fast clip testing
	float radius;

	winding_t *winding;
	status_t status;
	byte *front;			// [portals], preliminary
	byte *flood;			// [portals], intermediate
	byte *vis;				// [portals], final

	int num_might_see;		// bit count on flood for sort
} portal_t;

typedef struct separating_plane_s {
	struct separating_plane_s *next;
	plane_t plane;				// from portal is on positive side
} separating_plane_t;

typedef struct passage_s {
	struct passage_s *next;
	unsigned int from, to;		// leaf numbers
	separating_plane_t *planes;
} passage_t;

#define	MAX_PORTALS_ON_LEAF		128
typedef struct leaf_s {
	unsigned int num_portals;
	passage_t *passages;
	portal_t *portals[MAX_PORTALS_ON_LEAF];
} leaf_t;

typedef struct pstack_s {
	byte mightsee[MAX_PORTALS / 8];	// bit string
	struct pstack_s *next;
	leaf_t *leaf;
	portal_t *portal;				  // portal exiting
	winding_t *source;
	winding_t *pass;

	winding_t windings[3];		  // source, pass, temp in any order
	int freewindings[3];

	plane_t portalplane;
} pstack_t;

typedef struct {
	portal_t *base;
	int c_chains;
	pstack_t pstack_head;
} thread_data_t;

typedef struct map_vis_s {
	unsigned int num_portals;
	unsigned int portal_clusters;

	portal_t *portals;
	portal_t *sorted_portals[MAX_BSP_PORTALS * 2];

	leaf_t *leafs;

	size_t leaf_bytes; // (portal_clusters + 63) >> 3
	size_t leaf_longs; // / sizeof(long)

	size_t portal_bytes; // (portal_clusters + 63) >> 3
	size_t portal_longs; // / sizeof(long)

	size_t uncompressed_size;
	byte *uncompressed;

	byte *base;
	byte *pointer;
	byte *end;
} map_vis_t;

extern map_vis_t map_vis;

void BaseVis(int portal_num);
void FinalVis(int portal_num);

size_t CountBits(const byte *bits, size_t max);

#endif /* __QVIS_H__ */
