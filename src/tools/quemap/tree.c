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

	tree_t *tree = Mem_TagMalloc(sizeof(*tree), MEM_TAG_TREE);
	ClearBounds(tree->mins, tree->maxs);

	return tree;
}

/**
 * @brief
 */
void FreeTreePortals_r(node_t *node) {

	// free children
	if (node->plane_num != PLANE_NUM_LEAF) {
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
	if (node->plane_num != PLANE_NUM_LEAF) {
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

	node->plane_num = PLANE_NUM_LEAF;
	node->contents = 0;

	for (csg_brush_t *b = brushes; b; b = b->next) {
		// if the brush is solid and all of its sides are on nodes, it eats everything
		if (b->original->contents & CONTENTS_SOLID) {
			int32_t i;
			for (i = 0; i < b->num_sides; i++)
				if (b->sides[i].texinfo != TEXINFO_NODE) {
					break;
				}
			if (i == b->num_sides) {
				node->contents = CONTENTS_SOLID;
				break;
			}
		}
		node->contents |= b->original->contents;
	}

	node->brushes = brushes;
}

/**
 * @brief
 */
static void CheckPlaneAgainstParents(int32_t plane_num, node_t *node) {

	for (node_t *p = node->parent; p; p = p->parent) {
		if (p->plane_num == plane_num) {
			Com_Error(ERROR_FATAL, "Tried parent\n");
		}
	}
}

/**
 * @brief
 */
static _Bool CheckPlaneAgainstVolume(int32_t plane_num, node_t *node) {
	csg_brush_t *front, *back;

	SplitBrush(node->volume, plane_num, &front, &back);

	const _Bool good = (front && back);
	if (front) {
		FreeBrush(front);
	}
	if (back) {
		FreeBrush(back);
	}

	return good;
}

/**
 * @brief Using a heuristic, chooses one of the sides out of the brush list
 * to partition the brushes with.
 * Returns NULL if there are no valid planes to split with..
 */
static brush_side_t *SelectSplitSide(csg_brush_t *brushes, node_t *node) {
	int32_t value, best_value;
	csg_brush_t *brush, *test;
	brush_side_t *side, *best_side;
	int32_t i, j, pass, num_passes;
	int32_t plane_num;
	int32_t s;
	int32_t front, back, both, facing, splits;
	int32_t bsplits;
	int32_t epsilon_brush;
	_Bool hint_split;

	best_side = NULL;
	best_value = -99999;

	// the search order goes: visible-structural, visible-detail,
	// nonvisible-structural, nonvisible-detail.
	// If any valid plane is available in a pass, no further
	// passes will be tried.
	num_passes = 4;
	for (pass = 0; pass < num_passes; pass++) {
		for (brush = brushes; brush; brush = brush->next) {
			if ((pass & 1) && !(brush->original->contents & CONTENTS_DETAIL)) {
				continue;
			}
			if (!(pass & 1) && (brush->original->contents & CONTENTS_DETAIL)) {
				continue;
			}
			for (i = 0; i < brush->num_sides; i++) {
				side = brush->sides + i;
				if (side->bevel) {
					continue;    // never use a bevel as a splitter
				}
				if (!side->winding) {
					continue;    // nothing visible, so it can't split
				}
				if (side->texinfo == TEXINFO_NODE) {
					continue;    // already a node splitter
				}
				if (side->tested) {
					continue;    // we already have metrics for this plane
				}
				if (side->surf & SURF_SKIP) {
					continue;    // skip surfaces are never chosen
				}
				if (side->visible ^ (pass < 2)) {
					continue;    // only check visible faces on first pass
				}

				plane_num = side->plane_num;
				plane_num &= ~1; // always use positive facing plane

				CheckPlaneAgainstParents(plane_num, node);

				if (!CheckPlaneAgainstVolume(plane_num, node)) {
					continue; // would produce a tiny volume
				}

				front = 0;
				back = 0;
				both = 0;
				facing = 0;
				splits = 0;
				epsilon_brush = 0;
				hint_split = false;

				for (test = brushes; test; test = test->next) {
					s = TestBrushToPlane(test, plane_num, &bsplits, &hint_split, &epsilon_brush);

					splits += bsplits;
					if (bsplits && (s & SIDE_FACING)) {
						Com_Error(ERROR_FATAL, "SIDE_FACING with splits\n");
					}

					test->test_side = s;
					// if the brush shares this face, don't bother
					// testing that facenum as a splitter again
					if (s & SIDE_FACING) {
						facing++;
						for (j = 0; j < test->num_sides; j++) {
							if ((test->sides[j].plane_num & ~1) == plane_num) {
								test->sides[j].tested = true;
							}
						}
					}
					if (s & SIDE_FRONT) {
						front++;
					}
					if (s & SIDE_BACK) {
						back++;
					}
					if (s == SIDE_BOTH) {
						both++;
					}
				}

				// give a value estimate for using this plane

				value = 5 * facing - 5 * splits - abs(front - back);
				if (AXIAL(&planes[plane_num])) {
					value += 5;    // axial is better
				}
				value -= epsilon_brush * 1000; // avoid!

				if (side->contents & CONTENTS_DETAIL) {
					//value -= 5; // try to avoid splitting on details
				}

				// never split a hint side except with another hint
				if (hint_split && !(side->surf & SURF_HINT)) {
					value = -9999999;
				}

				// save off the side test so we don't need
				// to recalculate it when we actually separate
				// the brushes
				if (value > best_value) {
					best_value = value;
					best_side = side;
					for (test = brushes; test; test = test->next) {
						test->side = test->test_side;
					}
				}
			}
		}

		// if we found a good plane, don't bother trying any other passes
		if (best_side) {
			if (pass > 0) {
				node->detail_separator = true;    // not needed for vis
			}
			break;
		}
	}

	// clear all the tested flags we set
	for (brush = brushes; brush; brush = brush->next) {
		for (i = 0; i < brush->num_sides; i++) {
			brush->sides[i].tested = false;
		}
	}

	return best_side;
}

/**
 * @brief
 */
static void SplitBrushes(csg_brush_t *brushes, node_t *node, csg_brush_t **front, csg_brush_t **back) {
	csg_brush_t *brush, *front_brush, *back_brush;
	brush_side_t *side;

	*front = *back = NULL;

	for (brush = brushes; brush; brush = brush->next) {

		const int32_t sides = brush->side;
		if (sides == SIDE_BOTH) { // split into two brushes
			SplitBrush(brush, node->plane_num, &front_brush, &back_brush);
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

		// if the plane_num is actualy a part of the brush
		// find the plane and flag it as used so it won't be tried as a splitter again
		if (sides & SIDE_FACING) {
			for (int32_t i = 0; i < new_brush->num_sides; i++) {
				side = new_brush->sides + i;
				if ((side->plane_num & ~1) == node->plane_num) {
					side->texinfo = TEXINFO_NODE;
				}
			}
		}

		if (sides & SIDE_FRONT) {
			new_brush->next = *front;
			*front = new_brush;
			continue;
		}
		if (sides & SIDE_BACK) {
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

	// find the best plane to use as a splitter
	brush_side_t *split_side = SelectSplitSide(brushes, node);
	if (!split_side) {
		// leaf node
		node->side = NULL;
		node->plane_num = PLANE_NUM_LEAF;
		LeafNode(node, brushes);
		return node;
	}

	// this is a split-plane node
	node->side = split_side;
	node->plane_num = split_side->plane_num & ~1; // always use positive facing

	SplitBrushes(brushes, node, &children[0], &children[1]);
	FreeBrushes(brushes);

	// allocate children before recursing
	for (int32_t i = 0; i < 2; i++) {
		node_t *child = AllocNode();
		child->parent = node;
		node->children[i] = child;
	}

	SplitBrush(node->volume, node->plane_num, &node->children[0]->volume, &node->children[1]->volume);

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
tree_t *BuildTree(csg_brush_t *brushes, const vec3_t mins, const vec3_t maxs) {

	Com_Debug(DEBUG_ALL, "--- BuildTree ---\n");

	tree_t *tree = AllocTree();

	int32_t c_brushes = 0;
	int32_t c_vis_faces = 0;
	int32_t c_non_vis_faces = 0;

	for (csg_brush_t *b = brushes; b; b = b->next) {
		c_brushes++;

		const vec_t volume = BrushVolume(b);
		if (volume < micro_volume) {
			Mon_SendSelect(MON_WARN, b->original->entity_num, b->original->brush_num, "Microbrush");
		}

		for (int32_t i = 0; i < b->num_sides; i++) {
			if (b->sides[i].bevel) {
				continue;
			}
			if (!b->sides[i].winding) {
				continue;
			}
			if (b->sides[i].texinfo == TEXINFO_NODE) {
				continue;
			}
			if (b->sides[i].visible) {
				c_vis_faces++;
			} else {
				c_non_vis_faces++;
			}
		}

		AddPointToBounds(b->mins, tree->mins, tree->maxs);
		AddPointToBounds(b->maxs, tree->mins, tree->maxs);
	}

	Com_Debug(DEBUG_ALL, "%5i brushes\n", c_brushes);
	Com_Debug(DEBUG_ALL, "%5i visible faces\n", c_vis_faces);
	Com_Debug(DEBUG_ALL, "%5i nonvisible faces\n", c_non_vis_faces);

	node_t *node = AllocNode();

	node->volume = BrushFromBounds(mins, maxs);

	tree->head_node = node;

	node = BuildTree_r(node, brushes);

	return tree;
}

static int32_t c_pruned;

/**
 * @brief
 */
void PruneNodes_r(node_t *node) {
	csg_brush_t *b, *next;

	if (node->plane_num == PLANE_NUM_LEAF) {
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

		node->plane_num = PLANE_NUM_LEAF;
		node->contents = CONTENTS_SOLID;
		node->detail_separator = false;

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

/**
 * @brief
 */
void MergeNodeFaces(node_t *node) {

	for (face_t *f1 = node->faces; f1; f1 = f1->next) {
		if (f1->merged) {
			continue;
		}
		for (face_t *f2 = node->faces; f2 != f1; f2 = f2->next) {
			if (f2->merged) {
				continue;
			}

			//IDBUG: always passes the face's node's normal to TryMerge()
			//regardless of which side the face is on. Approximately 50% of
			//the time the face will be on the other side of node, and thus
			//the result of the convex/concave test in TryMergeWinding(),
			//which depends on the normal, is flipped. This causes faces
			//that shouldn't be merged to be merged and faces that
			//should be merged to not be merged.
			//the following added line fixes this bug
			//thanks to: Alexander Malmberg <alexander@malmberg.org>
			const plane_t *plane = &planes[f1->plane_num];

			face_t *merged = MergeFaces(f1, f2, plane->normal);
			if (!merged) {
				continue;
			}

			// add merged to the end of the node face list
			// so it will be checked against all the faces again
			face_t *last = node->faces;
			while (last->next) {
				last = last->next;
			}

			merged->next = NULL;
			last->next = merged;
			break;
		}
	}
}
