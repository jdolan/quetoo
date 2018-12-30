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

#include "flow.h"

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

/**
 * @return The number of leafs visible (bits) in the given bit vector.
 */
int32_t CountBits(const byte *bits, int32_t max) {

	int32_t c = 0;
	for (int32_t i = 0; i < max; i++)
		if (bits[i >> 3] & (1 << (i & 7))) {
			c++;
		}

	return c;
}

/**
 * @brief
 */
static winding_t *AllocWinding(pstack_t *stack) {

	for (int32_t i = 0; i < 3; i++) {
		if (stack->freewindings[i]) {
			stack->freewindings[i] = 0;
			return &stack->windings[i];
		}
	}

	Com_Error(ERROR_FATAL, "Failed\n");
	return NULL;
}

/**
 * @brief
 */
static void FreeWinding(const winding_t *w, pstack_t *stack) {
	const ptrdiff_t i = w - stack->windings;

	if (i < 0 || i > 2) {
		return; // not from local
	}

	if (stack->freewindings[i]) {
		Com_Error(ERROR_FATAL, "Already free\n");
	}

	stack->freewindings[i] = 1;
}

/**
 * @brief
 */
static winding_t *ChopWinding(winding_t *in, pstack_t *stack, plane_t *split) {
	vec_t dists[128];
	int32_t sides[128];
	int32_t counts[SIDE_BOTH + 1];
	vec_t dot;
	int32_t i, j;
	vec_t *p1, *p2;
	vec3_t mid;
	winding_t *neww;

	memset(counts, 0, sizeof(counts));

	// determine sides for each point
	for (i = 0; i < in->num_points; i++) {
		dot = DotProduct(in->points[i], split->normal);
		dot -= split->dist;
		dists[i] = dot;
		if (dot > ON_EPSILON) {
			sides[i] = SIDE_FRONT;
		} else if (dot < -ON_EPSILON) {
			sides[i] = SIDE_BACK;
		} else {
			sides[i] = SIDE_BOTH;
		}
		counts[sides[i]]++;
	}

	if (!counts[SIDE_BACK]) {
		return in; // completely on front side
	}

	if (!counts[SIDE_FRONT]) {
		FreeWinding(in, stack);
		return NULL;
	}

	sides[i] = sides[0];
	dists[i] = dists[0];

	neww = AllocWinding(stack);

	neww->num_points = 0;

	for (i = 0; i < in->num_points; i++) {
		p1 = in->points[i];

		if (neww->num_points == MAX_POINTS_ON_FIXED_WINDING) {
			FreeWinding(neww, stack);
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

		if (sides[i + 1] == SIDE_BOTH || sides[i + 1] == sides[i]) {
			continue;
		}

		if (neww->num_points == MAX_POINTS_ON_FIXED_WINDING) {
			FreeWinding(neww, stack);
			return in; // can't chop -- fall back to original
		}
		// generate a split point
		p2 = in->points[(i + 1) % in->num_points];

		dot = dists[i] / (dists[i] - dists[i + 1]);
		for (j = 0; j < 3; j++) { // avoid round off error when possible
			if (split->normal[j] == 1) {
				mid[j] = split->dist;
			} else if (split->normal[j] == -1) {
				mid[j] = -split->dist;
			} else {
				mid[j] = p1[j] + dot * (p2[j] - p1[j]);
			}
		}

		VectorCopy(mid, neww->points[neww->num_points]);
		neww->num_points++;
	}

	// free the original winding
	FreeWinding(in, stack);

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
 * order goes source, pass, target. If the order goes pass, source, target then
 * flipclip should be set.
 * ==============
 */
static winding_t *ClipToSeperators(winding_t *source, winding_t *pass, winding_t *target,
                                   _Bool flipclip, pstack_t *stack) {
	int32_t i, j, k, l;
	plane_t plane;
	vec3_t v1, v2;
	vec_t d;
	vec_t length;
	int32_t counts[3];
	_Bool fliptest;

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

			length = plane.normal[0] * plane.normal[0] + plane.normal[1] * plane.normal[1]
			         + plane.normal[2] * plane.normal[2];

			if (length < ON_EPSILON) {
				continue;
			}

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
				if (k == i || k == l) {
					continue;
				}
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
			if (k == source->num_points) {
				continue;    // planar with source portal
			}
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
				if (k == j) {
					continue;
				}
				d = DotProduct(pass->points[k], plane.normal) - plane.dist;
				if (d < -ON_EPSILON) {
					break;
				} else if (d > ON_EPSILON) {
					counts[0]++;
				} else {
					counts[2]++;
				}
			}
			if (k != pass->num_points) {
				continue;    // points on negative side, not a seperating plane
			}

			if (!counts[0]) {
				continue;    // planar with seperating plane
			}
#else
			k = (j + 1) % pass->num_points;
			d = DotProduct(pass->points[k], plane.normal) - plane.dist;
			if (d < -ON_EPSILON) {
				continue;
			}
			k = (j + pass->num_points - 1) % pass->num_points;
			d = DotProduct(pass->points[k], plane.normal) - plane.dist;
			if (d < -ON_EPSILON) {
				continue;
			}
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
			target = ChopWinding(target, stack, &plane);
			if (!target) {
				return NULL;    // target is not visible
			}
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
static void RecursiveLeafFlow(int32_t leaf_num, thread_data_t *thread, pstack_t *prevstack) {
	plane_t back_plane;
	byte *test;

	thread->c_chains++;

	leaf_t *leaf = &map_vis.leafs[leaf_num];

	pstack_t stack;
	prevstack->next = &stack;

	stack.next = NULL;
	stack.leaf = leaf;
	stack.portal = NULL;

	// check all portals for flowing into other leafs
	for (int32_t i = 0; i < leaf->num_portals; i++) {
		portal_t *p = leaf->portals[i];
		const ptrdiff_t portal_num = p - map_vis.portals;

		if (!(prevstack->mightsee[portal_num >> 3] & (1 << (portal_num & 7)))) {
			continue; // can't possibly see it
		}

		// if the portal can't see anything we haven't already seen, skip it
		if (p->status == STATUS_DONE) {
			test = p->vis;
		} else {
			test = p->flood;
		}

		byte more = 0;
		for (int32_t j = 0; j < map_vis.portal_bytes; j++) {
			stack.mightsee[j] = prevstack->mightsee[j] & test[j];
			more |= (stack.mightsee[j] & ~thread->base->vis[j]);
		}

		if (!more && (thread->base->vis[portal_num >> 3] & (1 << (portal_num & 7)))) { // can't see anything new
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
			const vec_t d = DotProduct(p->origin, thread->pstack_head.portalplane.normal)
			                - thread->pstack_head.portalplane.dist;
			if (d < -p->radius) {
				continue;
			} else if (d > p->radius) {
				stack.pass = p->winding;
			} else {
				stack.pass = ChopWinding(p->winding, &stack, &thread->pstack_head.portalplane);
				if (!stack.pass) {
					continue;
				}
			}
		}

		// the following block contains a fix from Geoffrey DeWan's qvis3 which
		// fixes the dreaded Quake2 'Hall of Mirrors' effect (insufficient PVS)
		{
			const vec_t d = DotProduct(thread->base->origin, p->plane.normal) - p->plane.dist;
			if (d > thread->base->radius) {
				//if(d > p->radius){
				continue;
			} else if (d < -thread->base->radius) {
				//} else if(d < -p->radius){
				stack.source = prevstack->source;
			} else {
				stack.source = ChopWinding(prevstack->source, &stack, &back_plane);
				if (!stack.source) {
					continue;
				}
			}
		}

		if (!prevstack->pass) { // the second leaf can only be blocked if coplanar
			// mark the portal as visible
			thread->base->vis[portal_num >> 3] |= (1 << (portal_num & 7));
			RecursiveLeafFlow(p->leaf, thread, &stack);
			continue;
		}

		stack.pass = ClipToSeperators(stack.source, prevstack->pass, stack.pass, false, &stack);
		if (!stack.pass) {
			continue;
		}

		stack.pass = ClipToSeperators(prevstack->pass, stack.source, stack.pass, true, &stack);

		if (!stack.pass) {
			continue;
		}

		// mark the portal as visible
		thread->base->vis[portal_num >> 3] |= (1 << (portal_num & 7));

		// flow through it for real
		RecursiveLeafFlow(p->leaf, thread, &stack);
	}
}

/**
 * @brief Generates the vis bit vector.
 */
void FinalVis(int32_t portal_num) {

	portal_t *p = map_vis.sorted_portals[portal_num];
	p->status = STATUS_WORKING;

	const int32_t c_might = CountBits(p->flood, map_vis.num_portals * 2);

	thread_data_t data;
	memset(&data, 0, sizeof(data));
	data.base = p;

	data.pstack_head.portal = p;
	data.pstack_head.source = p->winding;
	data.pstack_head.portalplane = p->plane;

	for (int32_t i = 0; i < map_vis.portal_bytes; i++) {
		data.pstack_head.mightsee[i] = p->flood[i];
	}

	RecursiveLeafFlow(p->leaf, &data, &data.pstack_head);

	p->status = STATUS_DONE;

	const int32_t c_can = CountBits(p->vis, map_vis.num_portals * 2);

	Com_Debug(DEBUG_ALL, "portal:%4i mightsee:%4i cansee:%4i (%i chains)\n",
	          (int32_t) (p - map_vis.portals), c_might, c_can, data.c_chains);
}

/**
 * @brief
 */
static void SimpleFlood(portal_t *srcportal, int32_t leaf_num) {

	const leaf_t *leaf = &map_vis.leafs[leaf_num];

	for (int32_t i = 0; i < leaf->num_portals; i++) {
		portal_t *p = leaf->portals[i];
		const ptrdiff_t portal_num = p - map_vis.portals;
		if (!(srcportal->front[portal_num >> 3] & (1 << (portal_num & 7)))) {
			continue;
		}

		if (srcportal->flood[portal_num >> 3] & (1 << (portal_num & 7))) {
			continue;
		}

		srcportal->flood[portal_num >> 3] |= (1 << (portal_num & 7));

		SimpleFlood(srcportal, p->leaf);
	}
}

/**
 * @brief
 */
void BaseVis(int32_t portal_num) {
	int32_t j;

	portal_t *portal = map_vis.portals + portal_num;

	portal->front = Mem_TagMalloc(map_vis.portal_bytes, MEM_TAG_VIS);
	portal->flood = Mem_TagMalloc(map_vis.portal_bytes, MEM_TAG_VIS);
	portal->vis = Mem_TagMalloc(map_vis.portal_bytes, MEM_TAG_VIS);

	portal_t *p = map_vis.portals;
	for (int32_t i = 0; i < map_vis.num_portals * 2; i++, p++) {

		if (p == portal) {
			continue;
		}

		const winding_t *w = p->winding;
		for (j = 0; j < w->num_points; j++) {
			const vec_t d = DotProduct(w->points[j], portal->plane.normal) - portal->plane.dist;
			if (d > ON_EPSILON) {
				break;
			}
		}

		if (j == w->num_points) {
			continue; // no points on front
		}

		w = portal->winding;
		for (j = 0; j < w->num_points; j++) {
			const vec_t d = DotProduct(w->points[j], p->plane.normal) - p->plane.dist;
			if (d < -ON_EPSILON) {
				break;
			}
		}

		if (j == w->num_points) {
			continue; // no points on front
		}

		portal->front[i >> 3] |= (1 << (i & 7));
	}

	SimpleFlood(portal, portal->leaf);

	portal->num_might_see = CountBits(portal->flood, map_vis.num_portals * 2);
}
