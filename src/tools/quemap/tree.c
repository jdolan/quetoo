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

#include "tree.h"
#include "portal.h"
#include "qbsp.h"

static SDL_atomic_t c_active_nodes;

/**
 * @brief
 */
node_t *AllocNode(void) {

	SDL_AtomicAdd(&c_active_nodes, 1);

	return Mem_TagMalloc(sizeof(node_t), MEM_TAG_NODE);
}

/**
 * @brief
 */
void FreeNode(node_t *node) {

	SDL_AtomicAdd(&c_active_nodes, -1);

	Mem_Free(node);
}

/**
 * @brief
 */
tree_t *AllocTree(void) {

	return Mem_TagMalloc(sizeof(tree_t), MEM_TAG_TREE);
}

/**
 * @brief
 */
void FreeTreePortals_r(node_t *node) {

	// free children
	if (node->plane != PLANE_LEAF) {
		FreeTreePortals_r(node->children[0]);
		FreeTreePortals_r(node->children[1]);
	}
	// free portals
	portal_t *next;
	for (portal_t *p = node->portals; p; p = next) {
		const int32_t s = (p->nodes[1] == node);
		next = p->next[s];

		RemovePortalFromNode(p, p->nodes[!s]);
		FreePortal(p);
	}
	node->portals = NULL;
}

/**
 * @brief
 */
void FreeTree_r(node_t *node) {

	// free children
	if (node->plane != PLANE_LEAF) {
		FreeTree_r(node->children[0]);
		FreeTree_r(node->children[1]);
	}

	// free brushes
	FreeBrushes(node->brushes);

	// free faces
	face_t *nextf;
	for (face_t *f = node->faces; f; f = nextf) {
		nextf = f->next;
		FreeFace(f);
	}

	// free the node
	if (node->volume) {
		FreeBrush(node->volume);
	}

	FreeNode(node);
}

/**
 * @brief
 */
void FreeTree(tree_t *tree) {

	Com_Verbose("--- FreeTree ---\n");
	FreeTreePortals_r(tree->head_node);
	FreeTree_r(tree->head_node);
	Mem_Free(tree);
	Com_Verbose("--- FreeTree complete ---\n");
}

/**
 * @brief
 */
static void LeafNode(node_t *node, csg_brush_t *brushes) {

	node->plane = PLANE_LEAF;
	node->contents = 0;

	for (csg_brush_t *b = brushes; b; b = b->next) {
		// if the brush is solid and all of its sides are on nodes, it eats everything
		if (b->original->contents & CONTENTS_SOLID) {
			int32_t i;
			for (i = 0; i < b->num_brush_sides; i++) {
				if (b->brush_sides[i].material != BSP_MATERIAL_NODE) {
					break;
				}
			}
			if (i == b->num_brush_sides) {
				node->contents = CONTENTS_SOLID;
				break;
			}
		}
		node->contents |= b->original->contents;
	}

	node->brushes = brushes;

	Progress("Building tree", -1);
}

/**
 * @return A heuristic value for splitting the
 */
static int32_t SelectSplitSideHeuristic(const brush_side_t *side, const csg_brush_t *brushes) {

	if (side->surface & SURF_HINT) {
		return INT32_MAX;
	}

	const int32_t plane = side->plane & ~1;

	int32_t front = 0, back = 0, on = 0, num_split_sides = 0;

	for (const csg_brush_t *brush = brushes; brush; brush = brush->next) {

		int32_t i;
		const int32_t s = BrushOnPlaneSideSplits(brush, plane, &i);

		if (s & SIDE_FRONT) {
			front++;
		}
		if (s & SIDE_BACK) {
			back++;
		}
		if (s & SIDE_ON) {
			on++;
		}

		num_split_sides += i;
	}

	// give a value estimate for using this plane

	int32_t value = 5 * on - 5 * num_split_sides - abs(front - back);

	if (AXIAL(&planes[plane])) {
		value += 5;
	}

	return value;
}

/**
 * @return The original brush side from brushes with the highest heuristic value.
 */
static const brush_side_t *SelectSplitSide(node_t *node, csg_brush_t *brushes) {

	const brush_side_t *best_side = NULL;
	int32_t best_value = INT32_MIN;

	GPtrArray *cache = g_ptr_array_new();

	_Bool have_structural = false;
	for (const csg_brush_t *brush = brushes; brush; brush = brush->next) {
		if (!(brush->original->contents & CONTENTS_DETAIL)) {
			if (brush->original->contents & CONTENTS_MASK_VISIBLE) {
				have_structural = true;
				break;
			}
		}
	}

	for (const csg_brush_t *brush = brushes; brush; brush = brush->next) {

		if (brush->original->contents & CONTENTS_DETAIL) {
			if (have_structural) {
				continue;
			}
		}

		const brush_side_t *side = brush->brush_sides;
		for (int32_t i = 0; i < brush->num_brush_sides; i++, side++) {

			if (side->material == BSP_MATERIAL_BEVEL) {
				continue;
			}
			if (side->material == BSP_MATERIAL_NODE) {
				continue;
			}

			assert(side->winding);

			const int32_t plane = side->plane ^ 1;
			if (g_ptr_array_find(cache, (gconstpointer) (intptr_t) plane, NULL)) {
				continue;
			}

			csg_brush_t *front, *back;
			SplitBrush(node->volume, plane, &front, &back);
			const _Bool valid_split = (front && back);
			if (front) {
				FreeBrush(front);
			}
			if (back) {
				FreeBrush(back);
			}
			if (!valid_split) {
				continue;
			}

			const int32_t value = SelectSplitSideHeuristic(side, brushes);
			if (value > best_value) {
				best_side = side->original;
				best_value = value;
			}

			g_ptr_array_add(cache, (gpointer) (intptr_t) plane);
		}
	}

	g_ptr_array_free(cache, true);

	return best_side;
}

/**
 * @brief
 */
static void SplitBrushes(csg_brush_t *brushes, const node_t *node, csg_brush_t **front, csg_brush_t **back) {

	*front = *back = NULL;

	for (const csg_brush_t *brush = brushes; brush; brush = brush->next) {

		const int32_t s = BrushOnPlaneSide(brush, node->plane);
		if (s == SIDE_BOTH) {
			csg_brush_t *front_brush, *back_brush;
			SplitBrush(brush, node->plane, &front_brush, &back_brush);
			if (front_brush) {
				front_brush->next = *front;
				*front = front_brush;
			}
			if (back_brush) {
				back_brush->next = *back;
				*back = back_brush;
			}
			continue;
		}

		csg_brush_t *new_brush = CopyBrush(brush);

		// if the plane is actualy a part of the brush
		// find the plane and flag it as used so it won't be tried as a splitter again
		if (s & SIDE_ON) {
			for (int32_t i = 0; i < new_brush->num_brush_sides; i++) {
				brush_side_t *side = new_brush->brush_sides + i;
				if ((side->plane & ~1) == node->plane) {
					side->material = BSP_MATERIAL_NODE;
				}
			}
		}

		if (s & SIDE_FRONT) {
			new_brush->next = *front;
			*front = new_brush;
			continue;
		}
		if (s & SIDE_BACK) {
			new_brush->next = *back;
			*back = new_brush;
			continue;
		}
	}
}

/**
 * @brief
 */
static node_t *BuildTree_r(node_t *node, csg_brush_t *brushes) {
	csg_brush_t *children[2];

	node->split_side = SelectSplitSide(node, brushes);
	if (!node->split_side) {
		node->plane = PLANE_LEAF;
		LeafNode(node, brushes);
		return node;
	}

	// this is a decision node, reference the positive plane
	node->plane = node->split_side->plane & ~1;

	SplitBrushes(brushes, node, &children[0], &children[1]);

	FreeBrushes(brushes);

	// allocate children before recursing
	for (int32_t i = 0; i < 2; i++) {
		node->children[i] = AllocNode();
		node->children[i]->parent = node;
	}

	SplitBrush(node->volume, node->plane, &node->children[0]->volume, &node->children[1]->volume);

	// recursively process children
	for (int32_t i = 0; i < 2; i++) {
		node->children[i] = BuildTree_r(node->children[i], children[i]);
	}

	return node;
}

/**
 * @brief
 * @remark The incoming list will be freed before exiting
 */
tree_t *BuildTree(csg_brush_t *brushes) {

	assert(brushes);

	Com_Debug(DEBUG_ALL, "--- BuildTree ---\n");

	const uint32_t start = SDL_GetTicks();

	tree_t *tree = AllocTree();

	tree->bounds = Box3_Null();

	int32_t num_brushes = 0;
	int32_t num_brush_sides = 0;

	for (csg_brush_t *b = brushes; b; b = b->next) {
		num_brushes++;

		const float volume = BrushVolume(b);
		if (volume < micro_volume) {
			Mon_SendSelect(MON_WARN, b->original->entity, b->original->brush, "Micro volume");
		}

		for (int32_t i = 0; i < b->num_brush_sides; i++) {
			if (b->brush_sides[i].material == BSP_MATERIAL_BEVEL) {
				continue;
			}
			if (b->brush_sides[i].material == BSP_MATERIAL_NODE) {
				continue;
			}
			num_brush_sides++;
		}

		tree->bounds = Box3_Union(tree->bounds, b->bounds);
	}

	Com_Debug(DEBUG_ALL, "%5i brushes\n", num_brushes);
	Com_Debug(DEBUG_ALL, "%5i brush sides\n", num_brush_sides);

	tree->head_node = AllocNode();
	tree->head_node->volume = BrushFromBounds(Box3f(MAX_WORLD_AXIAL, MAX_WORLD_AXIAL, MAX_WORLD_AXIAL));

	BuildTree_r(tree->head_node, brushes);

	Com_Print("\r%-24s [100%%] %d ms\n", "Building tree", SDL_GetTicks() - start);

	return tree;
}

static int32_t c_pruned;

/**
 * @brief
 */
void PruneNodes_r(node_t *node) {
	csg_brush_t *b, *next;

	if (node->plane == PLANE_LEAF) {
		return;
	}

	PruneNodes_r(node->children[0]);
	PruneNodes_r(node->children[1]);

	if ((node->children[0]->contents & CONTENTS_SOLID) &&
		(node->children[1]->contents & CONTENTS_SOLID)) {
		if (node->faces) {
			Com_Error(ERROR_FATAL, "Node faces separating CONTENTS_SOLID\n");
		}
		if (node->children[0]->faces || node->children[1]->faces) {
			Com_Error(ERROR_FATAL, "Node has no faces but children do\n");
		}

		// convert this node into a leaf, absorbing all brushes from its children

		node->plane = PLANE_LEAF;
		node->contents = CONTENTS_SOLID;
		node->split_side = NULL;

		if (node->brushes) {
			Com_Error(ERROR_FATAL, "Node still references brushes\n");
		}

		// combine brush lists
		node->brushes = node->children[1]->brushes;

		for (b = node->children[0]->brushes; b; b = next) {
			next = b->next;
			b->next = node->brushes;
			node->brushes = b;
		}

		// FIXME: Freeing child nodes here will cause downstream crashes when iterating
		// or removing portals from the tree. We need to handle the portals of child
		// nodes before we can safely free them.

//		FreeNode(node->children[0]);
//		node->children[0] = NULL;
//
//		FreeNode(node->children[1]);
//		node->children[1] = NULL;

		c_pruned++;
	}
}

/**
 * @brief Adjacent solids that do not separate contents can be pruned.
 * @remarks Depth-first recursion will merge the brushes of such nodes into their
 * parents, until an ancestor of different contents mask is found. The pruned
 * nodes become leafs.
 */
void PruneNodes(node_t *node) {
	Com_Verbose("--- PruneNodes ---\n");
	c_pruned = 0;
	PruneNodes_r(node);
	Com_Verbose("%5i pruned nodes\n", c_pruned);
}
