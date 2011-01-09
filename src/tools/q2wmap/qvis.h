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
	qboolean original;  // don't free, it's part of the portal
	int numpoints;
	vec3_t points[MAX_POINTS_ON_FIXED_WINDING];  // variable sized
} winding_t;

void FreeWinding(winding_t * w);
winding_t *CopyWinding(winding_t * w);

typedef enum {
	stat_none, stat_working, stat_done
} vstatus_t;

typedef struct {
	plane_t plane;				// normal pointing into neighbor
	int leaf;					// neighbor

	vec3_t origin;				// for fast clip testing
	float radius;

	winding_t *winding;
	vstatus_t status;
	byte *portalfront;			// [portals], preliminary
	byte *portalflood;			// [portals], intermediate
	byte *portalvis;			// [portals], final

	int nummightsee;			// bit count on portalflood for sort
} portal_t;

typedef struct seperating_plane_s {
	struct seperating_plane_s *next;
	plane_t plane;				// from portal is on positive side
} sep_t;


typedef struct passage_s {
	struct passage_s *next;
	int from, to;				// leaf numbers
	sep_t *planes;
}
passage_t;

#define	MAX_PORTALS_ON_LEAF		128
typedef struct leaf_s {
	int numportals;
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
} threaddata_t;


extern int numportals;
extern int portalclusters;

extern portal_t *portals;
extern leaf_t *leafs;

extern int portallongs;
extern int portalbytes;

extern int testlevel;

extern byte *uncompressed;

void LeafFlow(int leaf_num);

void BasePortalVis(int portal_num);
void BetterPortalVis(int portal_num);
void PortalFlow(int portal_num);

extern portal_t *sorted_portals[MAX_BSP_PORTALS * 2];

int CountBits(const byte * bits, int numbits);

#endif /* __QVIS_H__ */
