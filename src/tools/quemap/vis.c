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

#include "vis.h"

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
static winding_t *AllocStackWinding(pstack_t *stack) {

	for (int32_t i = 0; i < 3; i++) {
		if (stack->free_windings[i]) {
			stack->free_windings[i] = false;
			stack->windings[i].num_points = 0;
			return &stack->windings[i];
		}
	}

	Com_Error(ERROR_FATAL, "Failed\n");
	return NULL;
}

/**
 * @brief
 */
static void FreeStackWinding(pstack_t *stack, const winding_t *w) {
	const ptrdiff_t i = w - stack->windings;

	if (i < 0 || i > 2) {
		return; // the winding does not belong to the stack, it's a portal winding
	}

	if (stack->free_windings[i] == true) {
		Com_Error(ERROR_FATAL, "Already free\n");
	}

	stack->free_windings[i] = true;
}

/**
 * @brief
 */
static winding_t *ClipStackWinding(pstack_t *stack, winding_t *in, const plane_t *plane) {

	const int32_t max_points = in->num_points + 4;

	vec_t dists[max_points];
	int32_t sides[max_points];

	int32_t counts[SIDE_BOTH + 1];
	memset(counts, 0, sizeof(counts));

	// determine sides for each point
	for (int32_t i = 0; i < in->num_points; i++) {
		const vec_t dot = DotProduct(in->points[i], plane->normal) - plane->dist;
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

	sides[in->num_points] = sides[0];
	dists[in->num_points] = dists[0];

	if (!counts[SIDE_FRONT]) {
		FreeStackWinding(stack, in);
		return NULL;
	}

	if (!counts[SIDE_BACK]) {
		return in;
	}

	winding_t *out = AllocStackWinding(stack);

	for (int32_t i = 0; i < in->num_points; i++) {

		vec3_t p1, p2, mid;
		VectorCopy(in->points[i], p1);

		if (out->num_points == MAX_POINTS_ON_STACK_WINDING) {
			FreeStackWinding(stack, out);
			return in; // can't chop -- fall back to original
		}

		if (sides[i] == SIDE_BOTH) {
			VectorCopy(p1, out->points[out->num_points]);
			out->num_points++;
			continue;
		}

		if (sides[i] == SIDE_FRONT) {
			VectorCopy(p1, out->points[out->num_points]);
			out->num_points++;
		}

		if (sides[i + 1] == SIDE_BOTH || sides[i + 1] == sides[i]) {
			continue;
		}

		if (out->num_points == MAX_POINTS_ON_STACK_WINDING) {
			FreeStackWinding(stack, out);
			return in; // can't chop -- fall back to original
		}
		// generate a split point
		VectorCopy(in->points[(i + 1) % in->num_points], p2);

		const vec_t dot = dists[i] / (dists[i] - dists[i + 1]);
		for (int32_t j = 0; j < 3; j++) { // avoid round off error when possible
			if (plane->normal[j] == 1) {
				mid[j] = plane->dist;
			} else if (plane->normal[j] == -1) {
				mid[j] = -plane->dist;
			} else {
				mid[j] = p1[j] + dot * (p2[j] - p1[j]);
			}
		}

		VectorCopy(mid, out->points[out->num_points]);
		out->num_points++;
	}

	// free the original winding
	FreeStackWinding(stack, in);
	return out;
}

/*
 * @brief
 *
 * Source, pass, and target are an ordering of portals.
 *
 * Generates separating planes canidates by taking two points from source and one
 * point from pass, and clips target by them.
 *
 * If target is totally clipped away, that portal can not be seen through.
 *
 * Normal clip keeps target on the same side as pass, which is correct if the
 * order goes source, pass, target. If the order goes pass, source, target then
 * flip_clip should be set.
 */
static winding_t *ClipToSeparators(pstack_t *stack, winding_t *source, winding_t *pass, winding_t *target,
                                   _Bool flip_clip) {
	int32_t k;

	// check all combinations
	for (int32_t i = 0; i < source->num_points; i++) {
		const int32_t l = (i + 1) % source->num_points;

		vec3_t v1;
		VectorSubtract(source->points[l], source->points[i], v1);

		// find a vertex of pass that makes a plane that puts all of the
		// vertexes of pass on the front side and all of the vertexes of
		// source on the back side
		for (int32_t j = 0; j < pass->num_points; j++) {

			vec3_t v2;
			VectorSubtract(pass->points[j], source->points[i], v2);

			plane_t plane;
			plane.normal[0] = v1[1] * v2[2] - v1[2] * v2[1];
			plane.normal[1] = v1[2] * v2[0] - v1[0] * v2[2];
			plane.normal[2] = v1[0] * v2[1] - v1[1] * v2[0];

			// if points don't make a valid plane, skip it

			vec_t length = plane.normal[0] * plane.normal[0] +
						   plane.normal[1] * plane.normal[1] +
						   plane.normal[2] * plane.normal[2];

			if (length < ON_EPSILON) {
				continue;
			}

			length = 1 / sqrt(length);

			plane.normal[0] *= length;
			plane.normal[1] *= length;
			plane.normal[2] *= length;

			plane.dist = DotProduct(pass->points[j], plane.normal);

			//
			// find out which side of the generated separating plane has the
			// source portal
			//
#if 1
			_Bool flip_test = false;
			for (k = 0; k < source->num_points; k++) {
				if (k == i || k == l) {
					continue;
				}
				const vec_t d = DotProduct(source->points[k], plane.normal) - plane.dist;
				if (d < -ON_EPSILON) { // source is on the negative side, so we want all
					// pass and target on the positive side
					flip_test = false;
					break;
				} else if (d > ON_EPSILON) { // source is on the positive side, so we want all
					// pass and target on the negative side
					flip_test = true;
					break;
				}
			}
			if (k == source->num_points) {
				continue;    // planar with source portal
			}
#else
			flip_test = flip_clip;
#endif
			//
			// flip the normal if the source portal is backwards
			//
			if (flip_test) {
				VectorSubtract(vec3_origin, plane.normal, plane.normal);
				plane.dist = -plane.dist;
			}
#if 1
			//
			// if all of the pass portal points are now on the positive side,
			// this is the separating plane
			//
			int32_t counts[3] = { 0, 0, 0 };
			for (k = 0; k < pass->num_points; k++) {
				if (k == j) {
					continue;
				}
				const vec_t d = DotProduct(pass->points[k], plane.normal) - plane.dist;
				if (d < -ON_EPSILON) {
					break;
				} else if (d > ON_EPSILON) {
					counts[0]++;
				} else {
					counts[2]++;
				}
			}
			if (k != pass->num_points) {
				continue;    // points on negative side, not a separating plane
			}

			if (!counts[0]) {
				continue;    // planar with separating plane
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
			if (flip_clip) {
				VectorSubtract(vec3_origin, plane.normal, plane.normal);
				plane.dist = -plane.dist;
			}
			// MrE: fast check first
			const vec_t d = DotProduct(stack->portal->origin, plane.normal) - plane.dist;
			//if completely at the back of the separator plane
			if (d < -stack->portal->radius) {
				return NULL;
			}
			// if completely on the front of the separator plane
			if (d > stack->portal->radius) {
				break;
			}
			//
			// clip target by the separating plane
			//
			target = ClipStackWinding(stack, target, &plane);
			if (!target) {
				return NULL;    // target is not visible
			}
			break; // optimization by Antony Suter
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

	thread->c_chains++;

	// jdolan: This memory used to be allocated on the stack. Unfortunately, for very
	// complex maps, this would fail and cause crashes when > 1 threads were running.
	// So, we incur a small penalty by dynamically allocating the memory, but we're
	// free to throw all of the threads we want at it.
	pstack_t *stack = Mem_TagMalloc(sizeof(*stack), MEM_TAG_VIS);

	leaf_t *leaf = stack->leaf = &map_vis.leafs[leaf_num];
	prevstack->next = stack;

	// check all portals for flowing into other leafs
	for (int32_t i = 0; i < leaf->num_portals; i++) {
		portal_t *p = leaf->portals[i];
		const ptrdiff_t portal_num = p - map_vis.portals;

		if (!(prevstack->might_see[portal_num >> 3] & (1 << (portal_num & 7)))) {
			continue; // can't possibly see it
		}

		const byte *test;

		// if the portal can't see anything we haven't already seen, skip it
		if (p->status == STATUS_DONE) {
			test = p->vis;
		} else {
			test = p->flood;
		}

		_Bool more = false;
		for (int32_t j = 0; j < map_vis.portal_bytes; j++) {
			stack->might_see[j] = prevstack->might_see[j] & test[j];
			more |= (stack->might_see[j] & ~thread->base->vis[j]);
		}

		if (!more && (thread->base->vis[portal_num >> 3] & (1 << (portal_num & 7)))) { // can't see anything new
			continue;
		}

		// get plane of portal, point normal into the neighbor leaf
		stack->portalplane = p->plane;
		plane_t back_plane;
		VectorSubtract(vec3_origin, p->plane.normal, back_plane.normal);
		back_plane.dist = -p->plane.dist;

		stack->portal = p;
		stack->next = NULL;
		stack->free_windings[0] = 1;
		stack->free_windings[1] = 1;
		stack->free_windings[2] = 1;

		{
			const plane_t *plane = &thread->pstack_head.portalplane;
			const vec_t d = DotProduct(p->origin, plane->normal) - plane->dist;
			if (d < -p->radius) {
				continue;
			} else if (d > p->radius) {
				stack->pass = p->winding;
			} else {
				stack->pass = ClipStackWinding(stack, p->winding, plane);
				if (!stack->pass) {
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
				stack->source = prevstack->source;
			} else {
				stack->source = ClipStackWinding(stack, prevstack->source, &back_plane);
				if (!stack->source) {
					continue;
				}
			}
		}

		if (!prevstack->pass) { // the second leaf can only be blocked if coplanar
			// mark the portal as visible
			thread->base->vis[portal_num >> 3] |= (1 << (portal_num & 7));
			RecursiveLeafFlow(p->leaf, thread, stack);
			continue;
		}

		stack->pass = ClipToSeparators(stack, stack->source, prevstack->pass, stack->pass, false);
		if (!stack->pass) {
			continue;
		}

		stack->pass = ClipToSeparators(stack, prevstack->pass, stack->source, stack->pass, true);
		if (!stack->pass) {
			continue;
		}

		// mark the portal as visible
		thread->base->vis[portal_num >> 3] |= (1 << (portal_num & 7));

		// flow through it for real
		RecursiveLeafFlow(p->leaf, thread, stack);
	}

	Mem_Free(stack);
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
		data.pstack_head.might_see[i] = p->flood[i];
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
