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

/*
 *
 *   each portal will have a list of all possible to see from first portal
 *
 *   if(!thread->portalmightsee[portal_num])
 *
 *   portal mightsee
 *
 *   for p2 = all other portals in leaf
 * 	get separating planes
 * 	for all portals that might be seen by p2
 * 		mark as unseen if not present in separating plane
 * 	flood fill a new mightsee
 * 	save as passagemightsee
 *
 *
 *   void CalcMightSee (leaf_t *leaf,
 */

size_t CountBits(const byte * bits, size_t max) {
	size_t i, c;

	c = 0;
	for (i = 0; i < max; i++)
		if (bits[i >> 3] & (1 << (i & 7)))
			c++;

	return c;
}

static winding_t *AllocStackWinding(pstack_t * stack) {
	int i;

	for (i = 0; i < 3; i++) {
		if (stack->freewindings[i]) {
			stack->freewindings[i] = 0;
			return &stack->windings[i];
		}
	}

	Com_Error(ERR_FATAL, "AllocStackWinding: failed\n");

	return NULL;
}

static void FreeStackWinding(const winding_t * w, pstack_t * stack) {
	const int i = w - stack->windings;

	if (i < 0 || i > 2)
		return; // not from local

	if (stack->freewindings[i])
		Com_Error(ERR_FATAL, "FreeStackWinding: already free\n");
	stack->freewindings[i] = 1;
}

/*
 * Vis_ChopWinding
 */
static winding_t *Vis_ChopWinding(winding_t *in, pstack_t *stack,
		plane_t *split) {
	vec_t dists[128];
	int sides[128];
	int counts[SIDE_BOTH + 1];
	vec_t dot;
	int i, j;
	vec_t *p1, *p2;
	vec3_t mid;
	winding_t *neww;

	memset(counts, 0, sizeof(counts));

	// determine sides for each point
	for (i = 0; i < in->num_points; i++) {
		dot = DotProduct(in->points[i], split->normal);
		dot -= split->dist;
		dists[i] = dot;
		if (dot > ON_EPSILON)
			sides[i] = SIDE_FRONT;
		else if (dot < -ON_EPSILON)
			sides[i] = SIDE_BACK;
		else {
			sides[i] = SIDE_BOTH;
		}
		counts[sides[i]]++;
	}

	if (!counts[SIDE_BACK])
		return in; // completely on front side

	if (!counts[SIDE_FRONT]) {
		FreeStackWinding(in, stack);
		return NULL;
	}

	sides[i] = sides[0];
	dists[i] = dists[0];

	neww = AllocStackWinding(stack);

	neww->num_points = 0;

	for (i = 0; i < in->num_points; i++) {
		p1 = in->points[i];

		if (neww->num_points == MAX_POINTS_ON_FIXED_WINDING) {
			FreeStackWinding(neww, stack);
			return in; // can't chop -- fall back to original
		}

		if (sides[i] == SIDE_BOTH) {
			VectorCopy(p1, neww->points[neww->num_points]);
			neww->num_points++;
			continue;
		}

		if (sides[i] == SIDE_FRONT) {
			VectorCopy(p1, neww->points[neww->num_points]);
			neww->num_points++;
		}

		if (sides[i + 1] == SIDE_BOTH || sides[i + 1] == sides[i])
			continue;

		if (neww->num_points == MAX_POINTS_ON_FIXED_WINDING) {
			FreeStackWinding(neww, stack);
			return in; // can't chop -- fall back to original
		}
		// generate a split point
		p2 = in->points[(i + 1) % in->num_points];

		dot = dists[i] / (dists[i] - dists[i + 1]);
		for (j = 0; j < 3; j++) { // avoid round off error when possible
			if (split->normal[j] == 1)
				mid[j] = split->dist;
			else if (split->normal[j] == -1)
				mid[j] = -split->dist;
			else
				mid[j] = p1[j] + dot * (p2[j] - p1[j]);
		}

		VectorCopy(mid, neww->points[neww->num_points]);
		neww->num_points++;
	}

	// free the original winding
	FreeStackWinding(in, stack);

	return neww;
}

/*
 * ==============
 * ClipToSeperators
 *
 * Source, pass, and target are an ordering of portals.
 *
 * Generates seperating planes canidates by taking two points from source and one
 * point from pass, and clips target by them.
 *
 * If target is totally clipped away, that portal can not be seen through.
 *
 * Normal clip keeps target on the same side as pass, which is correct if the
 * order goes source, pass, target.  If the order goes pass, source, target then
 * flipclip should be set.
 * ==============
 */
static winding_t *ClipToSeperators(winding_t * source, winding_t * pass,
		winding_t * target, boolean_t flipclip, pstack_t * stack) {
	int i, j, k, l;
	plane_t plane;
	vec3_t v1, v2;
	float d;
	vec_t length;
	int counts[3];
	boolean_t fliptest;

	// check all combinations
	for (i = 0; i < source->num_points; i++) {
		l = (i + 1) % source->num_points;
		VectorSubtract(source->points[l], source->points[i], v1);

		// fing a vertex of pass that makes a plane that puts all of the
		// vertexes of pass on the front side and all of the vertexes of
		// source on the back side
		for (j = 0; j < pass->num_points; j++) {
			VectorSubtract(pass->points[j], source->points[i], v2);

			plane.normal[0] = v1[1] * v2[2] - v1[2] * v2[1];
			plane.normal[1] = v1[2] * v2[0] - v1[0] * v2[2];
			plane.normal[2] = v1[0] * v2[1] - v1[1] * v2[0];

			// if points don't make a valid plane, skip it

			length = plane.normal[0] * plane.normal[0] + plane.normal[1]
					* plane.normal[1] + plane.normal[2] * plane.normal[2];

			if (length < ON_EPSILON)
				continue;

			length = 1 / sqrt(length);

			plane.normal[0] *= length;
			plane.normal[1] *= length;
			plane.normal[2] *= length;

			plane.dist = DotProduct(pass->points[j], plane.normal);

			//
			// find out which side of the generated seperating plane has the
			// source portal
			//
#if 1
			fliptest = false;
			for (k = 0; k < source->num_points; k++) {
				if (k == i || k == l)
					continue;
				d = DotProduct(source->points[k], plane.normal) - plane.dist;
				if (d < -ON_EPSILON) { // source is on the negative side, so we want all
					// pass and target on the positive side
					fliptest = false;
					break;
				} else if (d > ON_EPSILON) { // source is on the positive side, so we want all
					// pass and target on the negative side
					fliptest = true;
					break;
				}
			}
			if (k == source->num_points)
				continue; // planar with source portal
#else
			fliptest = flipclip;
#endif
			//
			// flip the normal if the source portal is backwards
			//
			if (fliptest) {
				VectorSubtract(vec3_origin, plane.normal, plane.normal);
				plane.dist = -plane.dist;
			}
#if 1
			//
			// if all of the pass portal points are now on the positive side,
			// this is the seperating plane
			//
			counts[0] = counts[1] = counts[2] = 0;
			for (k = 0; k < pass->num_points; k++) {
				if (k == j)
					continue;
				d = DotProduct(pass->points[k], plane.normal) - plane.dist;
				if (d < -ON_EPSILON)
					break;
				else if (d > ON_EPSILON)
					counts[0]++;
				else
					counts[2]++;
			}
			if (k != pass->num_points)
				continue; // points on negative side, not a seperating plane

			if (!counts[0])
				continue; // planar with seperating plane
#else
			k = (j + 1) % pass->num_points;
			d = DotProduct(pass->points[k], plane.normal) - plane.dist;
			if(d < -ON_EPSILON)
			continue;
			k = (j + pass->num_points - 1) % pass->num_points;
			d = DotProduct(pass->points[k], plane.normal) - plane.dist;
			if(d < -ON_EPSILON)
			continue;
#endif
			//
			// flip the normal if we want the back side
			//
			if (flipclip) {
				VectorSubtract(vec3_origin, plane.normal, plane.normal);
				plane.dist = -plane.dist;
			}
			//
			// clip target by the seperating plane
			//
			target = Vis_ChopWinding(target, stack, &plane);
			if (!target)
				return NULL; // target is not visible
		}
	}

	return target;
}

/*
 * ==================
 * RecursiveLeafFlow
 *
 * Flood fill through the leafs
 * If src_portal is NULL, this is the originating leaf
 * ==================
 */
static void RecursiveLeafFlow(int leaf_num, thread_data_t * thread,
		pstack_t * prevstack) {
	pstack_t stack;
	portal_t *p;
	plane_t back_plane;
	leaf_t *leaf;
	unsigned int i, j;
	long *test, *might, *vis, more;
	int pnum;

	thread->c_chains++;

	leaf = &map_vis.leafs[leaf_num];

	prevstack->next = &stack;

	stack.next = NULL;
	stack.leaf = leaf;
	stack.portal = NULL;

	might = (long *) stack.mightsee;
	vis = (long *) thread->base->vis;

	// check all portals for flowing into other leafs
	for (i = 0; i < leaf->num_portals; i++) {
		p = leaf->portals[i];
		pnum = p - map_vis.portals;

		if (!(prevstack->mightsee[pnum >> 3] & (1 << (pnum & 7)))) {
			continue; // can't possibly see it
		}
		// if the portal can't see anything we haven't already seen, skip it
		if (p->status == stat_done) {
			test = (long *) p->vis;
		} else {
			test = (long *) p->flood;
		}

		more = 0;
		for (j = 0; j < map_vis.portal_longs; j++) {
			might[j] = ((long *) prevstack->mightsee)[j] & test[j];
			more |= (might[j] & ~vis[j]);
		}

		if (!more && (thread->base->vis[pnum >> 3] & (1 << (pnum & 7)))) { // can't see anything new
			continue;
		}
		// get plane of portal, point normal into the neighbor leaf
		stack.portalplane = p->plane;
		VectorSubtract(vec3_origin, p->plane.normal, back_plane.normal);
		back_plane.dist = -p->plane.dist;

		stack.portal = p;
		stack.next = NULL;
		stack.freewindings[0] = 1;
		stack.freewindings[1] = 1;
		stack.freewindings[2] = 1;

		{
			const float
					d =
							DotProduct(p->origin, thread->pstack_head.portalplane.normal)
									- thread->pstack_head.portalplane.dist;
			if (d < -p->radius) {
				continue;
			} else if (d > p->radius) {
				stack.pass = p->winding;
			} else {
				stack.pass = Vis_ChopWinding(p->winding, &stack,
						&thread->pstack_head.portalplane);
				if (!stack.pass)
					continue;
			}
		}

		// the following block contains a fix from Geoffrey DeWan's qvis3 which
		// fixes the dreaded Quake2 'Hall of Mirrors' effect (insufficient PVS)
		{
			const float d = DotProduct(thread->base->origin, p->plane.normal)
					- p->plane.dist;
			if (d > thread->base->radius) {
				//if(d > p->radius){
				continue;
			} else if (d < -thread->base->radius) {
				//} else if(d < -p->radius){
				stack.source = prevstack->source;
			} else {
				stack.source = Vis_ChopWinding(prevstack->source, &stack,
						&back_plane);
				if (!stack.source)
					continue;
			}
		}

		if (!prevstack->pass) { // the second leaf can only be blocked if coplanar

			// mark the portal as visible
			thread->base->vis[pnum >> 3] |= (1 << (pnum & 7));

			RecursiveLeafFlow(p->leaf, thread, &stack);
			continue;
		}

		stack.pass = ClipToSeperators(stack.source, prevstack->pass,
				stack.pass, false, &stack);
		if (!stack.pass)
			continue;

		stack.pass = ClipToSeperators(prevstack->pass, stack.source,
				stack.pass, true, &stack);

		if (!stack.pass)
			continue;

		// mark the portal as visible
		thread->base->vis[pnum >> 3] |= (1 << (pnum & 7));

		// flow through it for real
		RecursiveLeafFlow(p->leaf, thread, &stack);
	}
}

/*
 * FinalVis
 *
 * Generates the vis bit vector.
 */
void FinalVis(int portal_num) {
	thread_data_t data;
	unsigned int i;
	portal_t *p;
	size_t c_might, c_can;

	p = map_vis.sorted_portals[portal_num];
	p->status = stat_working;

	c_might = CountBits(p->flood, map_vis.num_portals * 2);

	memset(&data, 0, sizeof(data));
	data.base = p;

	data.pstack_head.portal = p;
	data.pstack_head.source = p->winding;
	data.pstack_head.portalplane = p->plane;
	for (i = 0; i < map_vis.portal_longs; i++)
		((long *) data.pstack_head.mightsee)[i] = ((long *) p->flood)[i];
	RecursiveLeafFlow(p->leaf, &data, &data.pstack_head);

	p->status = stat_done;

	c_can = CountBits(p->vis, map_vis.num_portals * 2);

	Com_Debug("portal:%4i  mightsee:%4i  cansee:%4i (%i chains)\n",
			(int) (p - map_vis.portals), (int)c_might, (int)c_can, data.c_chains);
}

/*
 * SimpleFlood
 */
static void SimpleFlood(portal_t *srcportal, int leaf_num) {
	unsigned int i;
	leaf_t *leaf;
	portal_t *p;
	int pnum;

	leaf = &map_vis.leafs[leaf_num];

	for (i = 0; i < leaf->num_portals; i++) {
		p = leaf->portals[i];
		pnum = p - map_vis.portals;
		if (!(srcportal->front[pnum >> 3] & (1 << (pnum & 7))))
			continue;

		if (srcportal->flood[pnum >> 3] & (1 << (pnum & 7)))
			continue;

		srcportal->flood[pnum >> 3] |= (1 << (pnum & 7));

		SimpleFlood(srcportal, p->leaf);
	}
}

/*
 * BaseVis
 */
void BaseVis(int portal_num) {
	unsigned int j, k;
	portal_t *tp, *p;
	float d;
	const winding_t *w;

	p = map_vis.portals + portal_num;

	p->front = Z_Malloc(map_vis.portal_bytes);
	p->flood = Z_Malloc(map_vis.portal_bytes);
	p->vis = Z_Malloc(map_vis.portal_bytes);

	for (j = 0, tp = map_vis.portals; j < map_vis.num_portals * 2; j++, tp++) {

		if (j == portal_num)
			continue;

		w = tp->winding;
		for (k = 0; k < w->num_points; k++) {
			d = DotProduct(w->points[k], p->plane.normal) - p->plane.dist;
			if (d > ON_EPSILON)
				break;
		}

		if (k == w->num_points)
			continue; // no points on front

		w = p->winding;
		for (k = 0; k < w->num_points; k++) {
			d = DotProduct(w->points[k], tp->plane.normal) - tp->plane.dist;
			if (d < -ON_EPSILON)
				break;
		}

		if (k == w->num_points)
			continue; // no points on front

		p->front[j >> 3] |= (1 << (j & 7));
	}

	SimpleFlood(p, p->leaf);

	p->num_might_see = CountBits(p->flood, map_vis.num_portals * 2);
}
