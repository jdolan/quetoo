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
#include "material.h"
#include "portal.h"
#include "qbsp.h"
#include "writebsp.h"

/**
 * @brief
 */
void EmitPlanes(void) {

	const plane_t *p = planes;
	for (int32_t i = 0; i < num_planes; i++, p++) {
		bsp_plane_t *out = &bsp_file.planes[bsp_file.num_planes];

		out->normal = p->normal;
		out->dist = (float) p->dist;

		bsp_file.num_planes++;

		Progress("Emitting planes", 100.f * i / num_planes);
	}
}

/**
 * @brief
 */
void EmitMaterials(void) {

	const material_t *m = materials;
	for (int32_t i = 0; i < num_materials; i++, m++) {
		bsp_material_t *out = &bsp_file.materials[bsp_file.num_materials];

		g_strlcpy(out->name, m->cm->name, sizeof(out->name));

		bsp_file.num_materials++;

		Progress("Emitting materials", 100.f * i / num_materials);
	}
}

/**
 * @brief
 */
static int32_t EmitFaces(const node_t *node) {

	const int32_t num_faces = bsp_file.num_faces;

	for (face_t *face = node->faces; face; face = face->next) {

		if (face->merged) {
			continue;
		}

		face->out = EmitFace(face);
	}

	return bsp_file.num_faces - num_faces;
}

/**
 * @brief
 */
static int32_t EmitLeaf(node_t *node) {

	if (bsp_file.num_leafs == MAX_BSP_LEAFS) {
		Com_Error(ERROR_FATAL, "MAX_BSP_LEAFS\n");
	}

	bsp_leaf_t *out = &bsp_file.leafs[bsp_file.num_leafs];
	bsp_file.num_leafs++;

	out->contents = node->contents;
	out->cluster = node->cluster;
	out->bounds = node->bounds;

	// write the leaf_brushes
	out->first_leaf_brush = bsp_file.num_leaf_brushes;

	for (const csg_brush_t *brush = node->brushes; brush; brush = brush->next) {

		if (bsp_file.num_leaf_brushes >= MAX_BSP_LEAF_BRUSHES) {
			Com_Error(ERROR_FATAL, "MAX_BSP_LEAF_BRUSHES\n");
		}

		assert(brush->original);
		assert(brush->original->out);
		
		const int32_t brush_num = (int32_t) (ptrdiff_t) (brush->original->out - bsp_file.brushes);

		int32_t i;
		for (i = out->first_leaf_brush; i < bsp_file.num_leaf_brushes; i++) {
			if (bsp_file.leaf_brushes[i] == brush_num) {
				break;
			}
		}

		if (i == bsp_file.num_leaf_brushes) {
			bsp_file.leaf_brushes[bsp_file.num_leaf_brushes] = brush_num;
			bsp_file.num_leaf_brushes++;
		}
	}

	out->num_leaf_brushes = bsp_file.num_leaf_brushes - out->first_leaf_brush;

	return (int32_t) (ptrdiff_t) (out - bsp_file.leafs);
}

/**
 * @brief
 */
static int32_t EmitNode(const node_t *node) {

	if (node->plane == PLANE_LEAF) {
		Com_Error(ERROR_FATAL, "Node does not reference a plane\n");
	}

	if (node->plane & 1) {
		Com_Error(ERROR_FATAL, "Node referencing negative plane\n");
	}

	if (bsp_file.num_nodes == MAX_BSP_NODES) {
		Com_Error(ERROR_FATAL, "MAX_BSP_NODES\n");
	}

	Progress("Emitting nodes", -1);

	bsp_node_t *out = &bsp_file.nodes[bsp_file.num_nodes];
	bsp_file.num_nodes++;

	out->plane = node->plane;
	out->bounds = node->bounds;
	out->visible_bounds = node->visible_bounds;

	if (node->faces) {
		out->first_face = bsp_file.num_faces;
		out->num_faces = EmitFaces(node);
	}

	// recursively output the other nodes
	for (int32_t i = 0; i < 2; i++) {
		if (node->children[i]->plane == PLANE_LEAF) {
			out->children[i] = -(bsp_file.num_leafs + 1);
			EmitLeaf(node->children[i]);
		} else {
			out->children[i] = bsp_file.num_nodes;
			EmitNode(node->children[i]);
		}
	}

	return (int32_t) (ptrdiff_t) (out - bsp_file.nodes);
}

/**
 * @brief
 */
int32_t EmitNodes(const tree_t *tree) {

	const uint32_t start = SDL_GetTicks();

	num_welds = 0;
	ClearWeldingSpatialHash();

	const int32_t node = EmitNode(tree->head_node);

	Com_Verbose("%5i welded vertices\n", num_welds);

	Com_Print("\r%-24s [100%%] %d ms\n", "Emitting nodes", SDL_GetTicks() - start);

	return node;
}

/**
 * @brief Draw elements comparator to sort faces by surface mask.
 */
static int32_t SurfaceCmp(const bsp_brush_side_t *a, const bsp_brush_side_t *b) {

	const int32_t a_surface = (a->surface & SURF_MASK_DRAW_ELEMENTS_CMP);
	const int32_t b_surface = (b->surface & SURF_MASK_DRAW_ELEMENTS_CMP);

	return a_surface - b_surface;
}

/**
 * @brief Draw elements comparator to sort model faces by material.
 * @details Opaque faces are equal if they share material and contents.
 * @details Blend faces are equal if they share opaque equality and plane.
 * @details Material faces equal if they share blend equality and brush side.
 */
static int32_t FaceCmp(const void *a, const void *b) {

	const bsp_face_t *a_face = a;
	const bsp_face_t *b_face = b;

	const bsp_brush_side_t *a_side = bsp_file.brush_sides + a_face->brush_side;
	const bsp_brush_side_t *b_side = bsp_file.brush_sides + b_face->brush_side;

	int32_t order = a_side->material - b_side->material;
	if (order == 0) {

		order = SurfaceCmp(a_side, b_side);
		if (order == 0) {

			if (a_side->surface & SURF_MATERIAL) {
				return (int32_t) (ptrdiff_t) (a_side - b_side);
			}

			if (a_side->surface & SURF_MASK_BLEND) {
				return a_side->plane - b_side->plane;
			}
		}
	}

	return order;
}

/**
 * @brief Sorts opaque and alpha test faces in the given model by material, and emits
 * glDrawElements commands for each unique material. The BSP face ordering is not modified,
 * as this would break the references that the nodes hold to them.
 * @return The number of draw elements commands emitted for the model.
 */
static int32_t EmitDrawElements(const bsp_model_t *mod) {

	const int32_t num_draw_elements = bsp_file.num_draw_elements;

	bsp_face_t *model_faces = calloc(mod->num_faces, sizeof(bsp_face_t));
	memcpy(model_faces, bsp_file.faces + mod->first_face, mod->num_faces * sizeof(bsp_face_t));

	qsort(model_faces, mod->num_faces, sizeof(bsp_face_t), FaceCmp);

	for (int32_t i = 0; i < mod->num_faces; i++) {

		if (bsp_file.num_draw_elements == MAX_BSP_DRAW_ELEMENTS) {
			Com_Error(ERROR_FATAL, "MAX_BSP_LEAF_ELEMENTS\n");
		}

		const bsp_face_t *a = model_faces + i;
		const bsp_brush_side_t *a_side = bsp_file.brush_sides + a->brush_side;

		if (a_side->surface & SURF_MASK_NO_DRAW_ELEMENTS) {
			continue;
		}

		bsp_draw_elements_t *out = bsp_file.draw_elements + bsp_file.num_draw_elements;
		bsp_file.num_draw_elements++;

		out->plane = a_side->plane;
		out->material = a_side->material;
		out->surface = a_side->surface & SURF_MASK_DRAW_ELEMENTS_CMP;

		out->bounds = Box3_Null();

		out->first_element = bsp_file.num_elements;

		for (int32_t j = i; j < mod->num_faces; j++) {

			const bsp_face_t *b = model_faces + j;

			if (FaceCmp(a, b)) {
				break;
			}

			if (bsp_file.num_elements + b->num_elements >= MAX_BSP_ELEMENTS) {
				Com_Error(ERROR_FATAL, "MAX_BSP_ELEMENTS\n");
			}

			memcpy(bsp_file.elements + bsp_file.num_elements,
				   bsp_file.elements + b->first_element,
				   sizeof(int32_t) * b->num_elements);

			bsp_file.num_elements += b->num_elements;
			out->num_elements += b->num_elements;

			out->bounds = Box3_Union(out->bounds, b->bounds);
			
			i = j;
		}

		assert(out->num_elements);
	}

	free(model_faces);

	return bsp_file.num_draw_elements - num_draw_elements;
}

/**
 * @brief
 */
static bsp_brush_side_t *EmitBrushSide(const brush_side_t *side) {

	bsp_brush_side_t *out = bsp_file.brush_sides + bsp_file.num_brush_sides;

	out->plane = side->plane;
	out->material = side->material;

	for (size_t i = 0; i < lengthof(out->axis); i++) {
		out->axis[i] = side->axis[i];
	}

	out->contents = side->contents;
	out->surface = side->surface;
	out->value = side->value;

	return out;
}

/**
 * @brief
 */
static int32_t EmitBrushSides(const brush_t *brush) {

	brush_side_t *side = brush->brush_sides;
	for (int32_t i = 0; i < brush->num_brush_sides; i++, side++) {

		side->out = EmitBrushSide(side);
		bsp_file.num_brush_sides++;
	}

	return brush->num_brush_sides;
}

/**
 * @brief
 */
static bsp_brush_t *EmitBrush(const brush_t *brush) {

	bsp_brush_t *out = bsp_file.brushes + bsp_file.num_brushes;

	out->entity = brush->entity;
	out->contents = brush->contents;

	out->first_brush_side = bsp_file.num_brush_sides;
	out->num_brush_sides = EmitBrushSides(brush);

	out->bounds = brush->bounds;

	return out;
}

/**
 * @brief
 */
void EmitBrushes(void) {

	brush_t *brush = brushes;
	for (int32_t i = 0; i < num_brushes; i++, brush++) {

		if (!brush->num_brush_sides) {
			continue;
		}

		brush->out = EmitBrush(brush);
		bsp_file.num_brushes++;

		Progress("Emitting brushes", 100.f * i / num_brushes);
	}
}

/**
 * @brief Generates the entity string from all retained entities.
 */
void EmitEntities(void) {

	const uint32_t start = SDL_GetTicks();

	Bsp_AllocLump(&bsp_file, BSP_LUMP_ENTITIES, MAX_BSP_ENTITIES_SIZE);

	char *out = bsp_file.entity_string;
	*out = '\0';

	for (int32_t i = 0; i < num_entities; i++) {
		const entity_key_value_t *e = entities[i].values;
		if (e) {
			g_strlcat(out, "{\n", MAX_BSP_ENTITIES_SIZE);
			while (e) {
				g_strlcat(out, va(" \"%s\" \"%s\"\n", e->key, e->value), MAX_BSP_ENTITIES_SIZE);
				e = e->next;
			}
			g_strlcat(out, "}\n", MAX_BSP_ENTITIES_SIZE);
		}

		Progress("Emitting entities", 100.f * i / num_entities);
	}

	const size_t len = strlen(out);

	if (len == MAX_BSP_ENTITIES_SIZE - 1) {
		Com_Error(ERROR_FATAL, "MAX_BSP_ENTITIES_SIZE\n");
	}

	bsp_file.entity_string_size = (int32_t) len + 1;

	Com_Print("\r%-24s [100%%] %d ms\n\n", "Emitting entities", SDL_GetTicks() - start);
}

/**
 * @brief
 */
void BeginBSPFile(void) {

	memset(&bsp_file, 0, sizeof(bsp_file));

	Bsp_AllocLump(&bsp_file, BSP_LUMP_MATERIALS, MAX_BSP_MATERIALS);
	Bsp_AllocLump(&bsp_file, BSP_LUMP_PLANES, MAX_BSP_PLANES);
	Bsp_AllocLump(&bsp_file, BSP_LUMP_BRUSH_SIDES, MAX_BSP_BRUSH_SIDES);
	Bsp_AllocLump(&bsp_file, BSP_LUMP_BRUSHES, MAX_BSP_BRUSHES);
	Bsp_AllocLump(&bsp_file, BSP_LUMP_VERTEXES, MAX_BSP_VERTEXES);
	Bsp_AllocLump(&bsp_file, BSP_LUMP_ELEMENTS, MAX_BSP_ELEMENTS);
	Bsp_AllocLump(&bsp_file, BSP_LUMP_FACES, MAX_BSP_FACES);
	Bsp_AllocLump(&bsp_file, BSP_LUMP_DRAW_ELEMENTS, MAX_BSP_DRAW_ELEMENTS);
	Bsp_AllocLump(&bsp_file, BSP_LUMP_NODES, MAX_BSP_NODES);
	Bsp_AllocLump(&bsp_file, BSP_LUMP_LEAF_BRUSHES, MAX_BSP_LEAF_BRUSHES);
	Bsp_AllocLump(&bsp_file, BSP_LUMP_LEAFS, MAX_BSP_LEAFS);
	Bsp_AllocLump(&bsp_file, BSP_LUMP_MODELS, MAX_BSP_MODELS);

	/*
	 * jdolan 2019-01-01
	 *
	 * Leafs are referenced by their parents as child nodes, but with negative indices.
	 * Because zero can not be negated, the first leaf in the map must be padded here.
	 * You can choose to ignore this comment if you want to lose 3 days of your life
	 * to debugging PVS, like I did.
	 */
	bsp_file.num_leafs = 1;
	bsp_file.leafs[0].contents = CONTENTS_SOLID;
}

/**
 * @brief
 */
bsp_model_t *BeginModel(const entity_t *e) {

	if (bsp_file.num_models == MAX_BSP_MODELS) {
		Com_Error(ERROR_FATAL, "MAX_BSP_MODELS\n");
	}

	bsp_model_t *mod = &bsp_file.models[bsp_file.num_models];
	bsp_file.num_models++;

	mod->entity = (int32_t) (ptrdiff_t) (e - entities);

	mod->first_face = bsp_file.num_faces;
	mod->first_draw_elements = bsp_file.num_draw_elements;

	// bound the brushes
	const int32_t start = e->first_brush;
	const int32_t end = start + e->num_brushes;

	mod->bounds = Box3_Null();

	const brush_t *brush = &brushes[start];
	for (int32_t j = start; j < end; j++, brush++) {

		if (brush->num_brush_sides) {
			mod->bounds = Box3_Union(mod->bounds, brush->bounds);
		}
	}

	return mod;
}

/**
 * @brief
 */
void EndModel(bsp_model_t *mod) {

	mod->num_faces = bsp_file.num_faces - mod->first_face;
	mod->num_draw_elements = EmitDrawElements(mod);
}
