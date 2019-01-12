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

#pragma once

#include "bsp.h"
#include "polylib.h"

typedef struct {
	vec3_t normal;
	vec_t dist;
} plane_t;

typedef enum {
	STATUS_NONE, STATUS_WORKING, STATUS_DONE
} status_t;

/**
 * @brief Portals output by the BSP stage are parsed into pairs, front and back.
 * @details Each BSP portal is a bridge between two leafs. For PVS calculation,
 * they are separated into two portals: one facing into each of the neighboring
 * leafs.
 */
typedef struct {
	/**
	 * @brief The plane facing into the neighboring leaf.
	 */
	plane_t plane;

	/**
	 * @brief The neighboring leaf that this portal points into.
	 */
	int32_t leaf;

	/**
	 * @brief The origin of the winding for this portal.
	 */
	vec3_t origin;

	/**
	 * @brief The radius of the winding for this portal.
	 */
	vec_t radius;

	/**
	 * @brief The winding for this portal.
	 */
	cm_winding_t *winding;

	/**
	 * @brief The VIS status, which is used to optimize later recursive
	 * iterations of chain culling.
	 */
	status_t status;

	/**
	 * @brief A vector of portals that are in front of this portal.
	 */
	byte *front; // [portals], preliminary

	/**
	 * @brief A vector of portals that are in front of this portal's leaf.
	 */
	byte *flood; // [portals], intermediate

	/**
	 * @brief A vector of portals that are in front of this portal's leaf and visible.
	 */
	byte *vis; // [portals], final

	/**
	 * @brief Portals are sorted by their flood result to improve performance.
	 * @details Portals with smaller flood results are processed first, so that their
	 * final visibility vector may speed up the processing of more complex portals.
	 */
	int32_t num_might_see; // bit count on flood for sort
} portal_t;

#define	MAX_PORTALS_ON_LEAF		128

/**
 * @brief For the purpose of PVS calculation, leafs are simply an aggregation
 * of portals that occupy them.
 */
typedef struct {
	int32_t num_portals;
	portal_t *portals[MAX_PORTALS_ON_LEAF];
} leaf_t;

#define	MAX_POINTS_ON_CHAIN_WINDING	16

/**
 * @brief Chain windings are windings that are statically allocated for
 * performance considerations.
 */
typedef struct {
	int32_t num_points;
	vec3_t points[MAX_POINTS_ON_CHAIN_WINDING];
} chain_winding_t;

/**
 * @brief Chains are formed recursively through leafs by traversing leaf portals.
 * @details Each link in the chain clips the incoming `source` winding by its
 * portal plane. If the winding is valid, the portal represented by that chain is
 * added to the potentially visible set of the originating portal, and vise versa.
 * When the winding becomes completely occluded, the chain is terminated.
 */
typedef struct chain_s {
	/**
	 * @brief The flood result, allowing fast culling of occluded neighbors.
	 */
	byte might_see[MAX_BSP_PORTALS / 8]; // bit string

	/**
	 * @brief The leaf that this chain
	 */
	const leaf_t *leaf;

	/**
	 * @brief The portal that this chain exits.
	 */
	const portal_t *portal;

	/**
	 * @brief The incoming visible winding from the head of the chain.
	 */
	chain_winding_t *source;

	/**
	 * @brief The `portal` winding, clipped by the head of the chain.
	 * @details This is the winding that will clip `source`.
	 */
	chain_winding_t *pass;

	/**
	 * @brief Statically allocated "scratch" windings for clipping operations.
	 */
	chain_winding_t windings[3];

	struct chain_s *next;
} chain_t;

/**
 * @brief A portal chain is constructed for each portal in the map.
 * @details All neighboring portals that the chain is able to visit before becoming
 * completely occluded are counted as visible to the originating portal.
 */
typedef struct {
	portal_t *portal;
	chain_t chain;
} portal_chain_t;

/**
 * @brief A bucket for accumulating PVS and PHS results.
 */
typedef struct {
	int32_t num_portals;
	int32_t portal_clusters;

	portal_t *portals;
	portal_t *sorted_portals[MAX_BSP_PORTALS * 2];

	leaf_t *leafs;

	int32_t leaf_bytes; // (portal_clusters + 63) >> 3
	int32_t portal_bytes; // (num_portals * 2 + 63) >> 3

	int32_t uncompressed_size;
	byte *uncompressed;

	byte *base;
	byte *pointer;
	byte *end;
} vis_t;

extern vis_t map_vis;

extern _Bool fast_vis;
extern _Bool no_sort;

void BaseVis(int32_t portal_num);
void FinalVis(int32_t portal_num);

int32_t CountBits(const byte *bits, int32_t max);
