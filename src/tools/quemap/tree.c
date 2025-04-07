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
static void FreeTreePortals_r(node_t *node) {

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

void FreeTreePortals(tree_t *tree) {
	FreeTreePortals_r(tree->head_node);
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
static node_t *LeafNode(node_t *node, csg_brush_t *brushes) {

	node->plane = PLANE_LEAF;
	node->contents = CONTENTS_NONE;

	for (csg_brush_t *b = brushes; b; b = b->next) {
		// if the brush is solid and all of its sides are on nodes, it eats everything
		if (b->original->contents & CONTENTS_SOLID) {
			int32_t i;
			for (i = 0; i < b->num_brush_sides; i++) {
				if (!(b->brush_sides[i].surface & SURF_NODE)) {
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

	return node;
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

	bool have_structural = false;
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

			if (side->surface & SURF_BEVEL) {
				continue;
			}
			if (side->surface & SURF_NODE) {
				continue;
			}

			assert(side->winding);

			const int32_t plane = side->plane ^ 1;
			if (g_ptr_array_find(cache, (gconstpointer) (intptr_t) plane, NULL)) {
				continue;
			}

			csg_brush_t *front, *back;
			SplitBrush(node->volume, plane, &front, &back);
			const bool valid_split = (front && back);
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
					side->surface |= SURF_NODE;
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
 * @brief Recursively split the node and filter brushes into its children. Nodes larger
 * than `BSP_BLOCK_SIZE` are split in half on their longest axis to produce a balanced tree.
 * Smaller nodes are split using a brush side heuristic to produce more optimal geometry.
 */
static node_t *BuildTree_r(node_t *node, csg_brush_t *brushes) {

	const vec3_t size = Box3_Size(node->volume->bounds);

	int32_t axis = 0;
	float longest_side = 0.f;
	for (int32_t i = 0; i < 3; i++) {
		if (size.xyz[i] > longest_side) {
			longest_side = size.xyz[i];
			axis = i;
		}
	}

	if (longest_side > BSP_BLOCK_SIZE) {
		node->contents = CONTENTS_BLOCK;

		if (node->parent) {
			node->parent->contents = CONTENTS_NODE;
		}

		vec3_t normal = Vec3_Zero();
		normal.xyz[axis] = 1.f;

		const int32_t dist = Box3_Center(node->volume->bounds).xyz[axis];
		node->plane = FindPlane(normal, dist) & ~1;

	} else {

		if (node->parent == NULL) {
			node->contents = CONTENTS_BLOCK;
		} else {
			node->contents = CONTENTS_NODE;
		}

		node->split_side = SelectSplitSide(node, brushes);
		if (!node->split_side) {
			return LeafNode(node, brushes);
		}

		node->plane = node->split_side->plane & ~1;
	}

	node->children[0] = AllocNode();
	node->children[0]->parent = node;

	node->children[1] = AllocNode();
	node->children[1]->parent = node;

	SplitBrush(node->volume, node->plane, &node->children[0]->volume, &node->children[1]->volume);

	csg_brush_t *front, *back;
	SplitBrushes(brushes, node, &front, &back);

	FreeBrushes(brushes);

	BuildTree_r(node->children[0], front);
	BuildTree_r(node->children[1], back);

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

		const brush_side_t *s = b->brush_sides;
		for (int32_t i = 0; i < b->num_brush_sides; i++, s++) {
			if (s->surface & SURF_BEVEL) {
				continue;
			}
			if (s->surface & SURF_NODE) {
				continue;
			}
			num_brush_sides++;
		}

		tree->bounds = Box3_Union(tree->bounds, b->bounds);
	}

	assert(num_brushes);
	assert(num_brush_sides);

	Com_Debug(DEBUG_ALL, "%5i brushes\n", num_brushes);
	Com_Debug(DEBUG_ALL, "%5i brush sides\n", num_brush_sides);

	tree->head_node = AllocNode();
	tree->head_node->volume = BrushFromBounds(Box3_Expand(tree->bounds, 1.f));

	BuildTree_r(tree->head_node, brushes);

	Com_Print("\r%-24s [100%%] %d ms\n", "Building tree", SDL_GetTicks() - start);

	return tree;
}

static int32_t c_merged_faces;

/**
 * @brief
 */
static void MergeFaces_r(node_t *node) {

	if (node->plane == PLANE_LEAF) {
		return;
	}

again:
	for (face_t *a = node->faces; a; a = a->next) {
		if (a->merged) {
			continue;
		}
		for (face_t *b = node->faces; b; b = b->next) {
			if (a == b) {
				continue;
			}

			if (b->merged) {
				continue;
			}

			face_t *merged = MergeFaces(a, b);
			if (!merged) {
				continue;
			}

			c_merged_faces++;

			merged->next = node->faces;
			node->faces = merged;

			goto again;
		}
	}

	MergeFaces_r(node->children[0]);
	MergeFaces_r(node->children[1]);
}

/**
 * @brief
 */
static box3_t CalcNodeVisibleBounds_r(node_t *node) {

	if (node->plane == PLANE_LEAF) {
		return Box3_Null();
	}

	const box3_t a = CalcNodeVisibleBounds_r(node->children[0]);
	const box3_t b = CalcNodeVisibleBounds_r(node->children[1]);

	node->visible_bounds = Box3_Union(a, b);

	for (face_t *face = node->faces; face; face = face->next) {

		face_t *f = face;
		while (f->merged) {
			f = f->merged;
		}

		assert(f->w);
		node->visible_bounds = Box3_Union(node->visible_bounds, Cm_WindingBounds(f->w));
	}

	return node->visible_bounds;
}


/**
 * @brief
 */
void MergeTreeFaces(tree_t *tree) {
	Com_Verbose("--- MergeTreeFaces ---\n");
	c_merged_faces = 0;
	MergeFaces_r(tree->head_node);
	CalcNodeVisibleBounds_r(tree->head_node);
	Com_Verbose("%5i merged faces\n", c_merged_faces);
}
