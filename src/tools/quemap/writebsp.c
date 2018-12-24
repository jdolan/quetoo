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

#include "bsp.h"
#include "map.h"
#include "portal.h"
#include "qbsp.h"

static int32_t c_nofaces;
static int32_t c_facenodes;

/**
 * @brief There is no opportunity to discard planes, because all of the original
 * brushes will be saved in the map.
 */
static void EmitPlanes(void) {
	int32_t i;
	bsp_plane_t *dp;
	plane_t *mp;

	mp = planes;
	for (i = 0; i < num_planes; i++, mp++) {
		dp = &bsp_file.planes[bsp_file.num_planes];
		VectorCopy(mp->normal, dp->normal);
		dp->dist = mp->dist;
		dp->type = mp->type;
		bsp_file.num_planes++;
	}
}

/**
 * @brief
 */
static void EmitLeafFace(bsp_leaf_t *leaf_p, face_t *f) {
	int32_t i;
	int32_t face_num;

	while (f->merged) {
		f = f->merged;
	}

	face_num = f->output_number;
	if (face_num == -1) {
		return;    // degenerate face
	}

	if (face_num < 0 || face_num >= bsp_file.num_faces) {
		Com_Error(ERROR_FATAL, "Bad leaf face\n");
	}

	for (i = leaf_p->first_leaf_face; i < bsp_file.num_leaf_faces; i++)
		if (bsp_file.leaf_faces[i] == face_num) {
			break;    // merged out face
		}

	if (i == bsp_file.num_leaf_faces) {
		if (bsp_file.num_leaf_faces >= MAX_BSP_LEAF_FACES) {
			Com_Error(ERROR_FATAL, "MAX_BSP_LEAF_FACES\n");
		}

		bsp_file.leaf_faces[bsp_file.num_leaf_faces] = face_num;
		bsp_file.num_leaf_faces++;
	}
}

/**
 * @brief
 */
static void EmitLeaf(node_t *node) {
	bsp_leaf_t *leaf_p;
	portal_t *p;
	int32_t s;
	face_t *f;
	csg_brush_t *b;
	int32_t i;
	ptrdiff_t brush_num;

	// emit a leaf
	if (bsp_file.num_leafs >= MAX_BSP_LEAFS) {
		Com_Error(ERROR_FATAL, "MAX_BSP_LEAFS\n");
	}

	leaf_p = &bsp_file.leafs[bsp_file.num_leafs];
	bsp_file.num_leafs++;

	leaf_p->contents = node->contents;
	leaf_p->cluster = node->cluster;
	leaf_p->area = node->area;

	// write bounding box info
	VectorCopy(node->mins, leaf_p->mins);
	VectorCopy(node->maxs, leaf_p->maxs);

	// write the leaf_brushes
	leaf_p->first_leaf_brush = bsp_file.num_leaf_brushes;
	for (b = node->brushes; b; b = b->next) {

		if (bsp_file.num_leaf_brushes >= MAX_BSP_LEAF_BRUSHES) {
			Com_Error(ERROR_FATAL, "MAX_BSP_LEAF_BRUSHES\n");
		}

		brush_num = b->original - brushes;

		for (i = leaf_p->first_leaf_brush; i < bsp_file.num_leaf_brushes; i++) {
			if (bsp_file.leaf_brushes[i] == brush_num) {
				break;
			}
		}

		if (i == bsp_file.num_leaf_brushes) {
			bsp_file.leaf_brushes[bsp_file.num_leaf_brushes] = brush_num;
			bsp_file.num_leaf_brushes++;
		}
	}
	leaf_p->num_leaf_brushes = bsp_file.num_leaf_brushes - leaf_p->first_leaf_brush;

	// write the leaf_faces
	if (leaf_p->contents & CONTENTS_SOLID) {
		return;    // no leaf_faces in solids
	}

	leaf_p->first_leaf_face = bsp_file.num_leaf_faces;

	for (p = node->portals; p; p = p->next[s]) {

		s = (p->nodes[1] == node);
		f = p->face[s];

		if (!f) {
			continue;    // not a visible portal
		}

		EmitLeafFace(leaf_p, f);
	}

	leaf_p->num_leaf_faces = bsp_file.num_leaf_faces - leaf_p->first_leaf_face;
}

/**
 * @brief
 */
static void EmitFace(face_t *f) {

	f->output_number = -1;

	if (f->w->num_points < 3) {
		return; // degenerated
	}
	if (f->merged) {
		return; // not a final face
	}
	// save output number so leaffaces can use
	f->output_number = bsp_file.num_faces;

	if (bsp_file.num_faces >= MAX_BSP_FACES) {
		Com_Error(ERROR_FATAL, "MAX_BSP_FACES\n");
	}

	bsp_face_t *df = &bsp_file.faces[bsp_file.num_faces];
	bsp_file.num_faces++;

	// plane_num is used by qlight, but not quake
	df->plane_num = f->plane_num & (~1);
	df->side = f->plane_num & 1;

	df->first_face_edge = bsp_file.num_face_edges;
	df->num_face_edges = f->w->num_points;
	df->texinfo = f->texinfo;

	const bsp_texinfo_t *tex = &bsp_file.texinfo[f->texinfo];

	int32_t vertex_nums[f->w->num_points];

	for (int32_t i = 0; i < f->w->num_points; i++) {
		if (tex->flags & SURF_NO_WELD) {
			VectorCopy(f->w->points[i], bsp_file.vertexes[bsp_file.num_vertexes].point);
			vertex_nums[i] = bsp_file.num_vertexes++;
		} else {
			vertex_nums[i] = EmitVertex(f->w->points[i]);
		}
	}

	for (int32_t i = 0; i < f->w->num_points; i++) {
		const int32_t j = (i + 1) % f->w->num_points;
		const int32_t e = EmitEdge(vertex_nums[i], vertex_nums[j], f);

		if (bsp_file.num_face_edges >= MAX_BSP_FACE_EDGES) {
			Com_Error(ERROR_FATAL, "MAX_BSP_FACE_EDGES\n");
		}

		bsp_file.face_edges[bsp_file.num_face_edges] = e;
		bsp_file.num_face_edges++;
	}
	df->lightmap_ofs = -1;
}

/**
 * @brief
 */
static int32_t EmitDrawNode_r(node_t *node) {
	bsp_node_t *n;
	face_t *f;
	int32_t i;

	if (node->plane_num == PLANENUM_LEAF) {
		EmitLeaf(node);
		return -bsp_file.num_leafs;
	}

	// emit a node
	if (bsp_file.num_nodes == MAX_BSP_NODES) {
		Com_Error(ERROR_FATAL, "MAX_BSP_NODES\n");
	}

	n = &bsp_file.nodes[bsp_file.num_nodes];
	bsp_file.num_nodes++;

	VectorCopy(node->mins, n->mins);
	VectorCopy(node->maxs, n->maxs);

	if (node->plane_num & 1) {
		Com_Error(ERROR_FATAL, "Odd plane number\n");
	}

	n->plane_num = node->plane_num;
	n->first_face = bsp_file.num_faces;

	if (!node->faces) {
		c_nofaces++;
	} else {
		c_facenodes++;
	}

	for (f = node->faces; f; f = f->next) {
		EmitFace(f);
	}

	n->num_faces = bsp_file.num_faces - n->first_face;

	// recursively output the other nodes
	for (i = 0; i < 2; i++) {
		if (node->children[i]->plane_num == PLANENUM_LEAF) {
			n->children[i] = -(bsp_file.num_leafs + 1);
			EmitLeaf(node->children[i]);
		} else {
			n->children[i] = bsp_file.num_nodes;
			EmitDrawNode_r(node->children[i]);
		}
	}

	return (int32_t) (ptrdiff_t) (n - bsp_file.nodes);
}

/**
 * @brief
 */
void WriteBSP(node_t *head_node) {

	c_nofaces = 0;
	c_facenodes = 0;

	Com_Verbose("--- WriteBSP ---\n");

	const int32_t old_faces = bsp_file.num_faces;

	bsp_file.models[bsp_file.num_models].head_node = EmitDrawNode_r(head_node);

	Com_Verbose("%5i nodes with faces\n", c_facenodes);
	Com_Verbose("%5i nodes without faces\n", c_nofaces);
	Com_Verbose("%5i faces\n", bsp_file.num_faces - old_faces);
}

/**
 * @brief
 */
static void EmitBrushes(void) {
	int32_t i, j, bnum, s, x;
	bsp_brush_t *db;
	brush_t *b;
	bsp_brush_side_t *cp;
	vec3_t normal;
	vec_t dist;
	int32_t plane_num;

	bsp_file.num_brush_sides = 0;
	bsp_file.num_brushes = num_brushes;

	for (bnum = 0; bnum < num_brushes; bnum++) {
		b = &brushes[bnum];
		db = &bsp_file.brushes[bnum];

		db->contents = b->contents;
		db->first_brush_side = bsp_file.num_brush_sides;
		db->num_sides = b->num_sides;

		for (j = 0; j < b->num_sides; j++) {

			if (bsp_file.num_brush_sides == MAX_BSP_BRUSH_SIDES) {
				Com_Error(ERROR_FATAL, "MAX_BSP_BRUSH_SIDES\n");
			}

			cp = &bsp_file.brush_sides[bsp_file.num_brush_sides];
			bsp_file.num_brush_sides++;

			cp->plane_num = b->original_sides[j].plane_num;
			cp->surf_num = b->original_sides[j].texinfo;
		}

		// add any axis planes not contained in the brush to bevel off corners
		for (x = 0; x < 3; x++) {
			for (s = -1; s <= 1; s += 2) {
				// add the plane
				VectorClear(normal);
				normal[x] = (vec_t) s;

				if (s == -1) {
					dist = -b->mins[x];
				} else {
					dist = b->maxs[x];
				}

				plane_num = FindPlane(normal, dist);

				for (i = 0; i < b->num_sides; i++)
					if (b->original_sides[i].plane_num == plane_num) {
						break;
					}

				if (i == b->num_sides) {
					if (bsp_file.num_brush_sides >= MAX_BSP_BRUSH_SIDES) {
						Com_Error(ERROR_FATAL, "MAX_BSP_BRUSH_SIDES\n");
					}

					bsp_file.brush_sides[bsp_file.num_brush_sides].plane_num = plane_num;
					bsp_file.brush_sides[bsp_file.num_brush_sides].surf_num
					    = bsp_file.brush_sides[bsp_file.num_brush_sides - 1].surf_num;
					bsp_file.num_brush_sides++;
					db->num_sides++;
				}
			}
		}
	}
}

/**
 * @brief Generates the entity string from all retained entities.
 */
void EmitEntities(void) {

	Bsp_AllocLump(&bsp_file, BSP_LUMP_ENTITIES, MAX_BSP_ENT_STRING);

	char *out = bsp_file.entity_string;

	for (int32_t i = 0; i < num_entities; i++) {
		const entity_key_value_t *e = entities[i].values;
		if (e) {
			g_strlcat(out, "{\n", MAX_BSP_ENT_STRING);
			while (e) {
				g_strlcat(out, va(" \"%s\" \"%s\"\n", e->key, e->value), MAX_BSP_ENT_STRING);
				e = e->next;
			}
			g_strlcat(out, "}\n", MAX_BSP_ENT_STRING);
		}
	}

	const size_t len = strlen(out);

	if (len == MAX_BSP_ENT_STRING - 1) {
		Com_Error(ERROR_FATAL, "MAX_BSP_ENT_STRING\n");
	}

	bsp_file.entity_string_size = (int32_t) len + 1;
}

/**
 * @brief
 */
void BeginBSPFile(void) {

	// edge 0 is not used, because 0 can't be negated
	bsp_file.num_edges = 1;

	// leave vertex 0 as an error
	bsp_file.num_vertexes = 1;

	// leave leaf 0 as an error
	bsp_file.num_leafs = 1;
	bsp_file.leafs[0].contents = CONTENTS_SOLID;
}

/**
 * @brief
 */
void EndBSPFile(void) {

	EmitBrushes();
	EmitPlanes();
	EmitAreaPortals();
	EmitEntities();

	WriteBSPFile(va("maps/%s.bsp", map_base));
}

/**
 * @brief
 */
void BeginModel(void) {
	bsp_model_t *mod;
	int32_t start, end;
	brush_t *b;
	int32_t j;
	entity_t *e;
	vec3_t mins, maxs;

	if (bsp_file.num_models == MAX_BSP_MODELS) {
		Com_Error(ERROR_FATAL, "MAX_BSP_MODELS\n");
	}
	mod = &bsp_file.models[bsp_file.num_models];

	mod->first_face = bsp_file.num_faces;

	first_bsp_model_edge = bsp_file.num_edges;

	// bound the brushes
	e = &entities[entity_num];

	start = e->first_brush;
	end = start + e->num_brushes;
	ClearBounds(mins, maxs);

	for (j = start; j < end; j++) {
		b = &brushes[j];
		if (!b->num_sides) {
			continue;    // not a real brush (origin brush)
		}
		AddPointToBounds(b->mins, mins, maxs);
		AddPointToBounds(b->maxs, mins, maxs);
	}

	VectorCopy(mins, mod->mins);
	VectorCopy(maxs, mod->maxs);
}

/**
 * @brief
 */
void EndModel(void) {
	bsp_model_t *mod;

	mod = &bsp_file.models[bsp_file.num_models];

	mod->num_faces = bsp_file.num_faces - mod->first_face;

	bsp_file.num_models++;
}
