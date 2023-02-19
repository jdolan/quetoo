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

#include "r_local.h"

/**
 * @brief
 */
static void R_LoadBspEntities(r_bsp_model_t *bsp) {

	bsp->luxel_size = Cm_EntityValue(Cm_Worldspawn(), "luxel_size")->integer;
	if (bsp->luxel_size <= 0) {
		bsp->luxel_size = BSP_LIGHTMAP_LUXEL_SIZE;
	}

	Com_Debug(DEBUG_RENDERER, "Resolved luxel_size: %d\n", bsp->luxel_size);
}

/**
 * @brief
 */
static void R_LoadBspPlanes(r_bsp_model_t *bsp) {
	r_bsp_plane_t *out;

	const cm_bsp_plane_t *in = bsp->cm->planes;

	bsp->num_planes = bsp->cm->num_planes;
	bsp->planes = out = Mem_LinkMalloc(bsp->num_planes * sizeof(*out), bsp);

	for (int32_t i = 0; i < bsp->num_planes; i++, out++, in++) {
		out->cm = in;
	}
}

/**
 * @brief
 */
static void R_LoadBspMaterials(r_model_t *mod) {
	char path[MAX_QPATH];

	g_snprintf(path, sizeof(path), "%s.mat", mod->media.name);

	GList *materials = NULL;

	R_LoadMaterials(path, ASSET_CONTEXT_TEXTURES, &materials);

	g_list_free(materials);

	r_material_t **out;
	const bsp_material_t *in = mod->bsp->cm->file->materials;

	mod->bsp->num_materials = mod->bsp->cm->file->num_materials;
	mod->bsp->materials = out = Mem_LinkMalloc(mod->bsp->num_materials * sizeof(*out), mod->bsp);

	for (int32_t i = 0; i < mod->bsp->num_materials; i++, in++, out++) {
		*out = R_LoadMaterial(in->name, ASSET_CONTEXT_TEXTURES);
		R_RegisterDependency((r_media_t *) mod, (r_media_t *) *out);
	}
}

/**
 * @brief
 */
static void R_LoadBspBrushSides(r_bsp_model_t *bsp) {
	r_bsp_brush_side_t *out;

	const bsp_brush_side_t *in = bsp->cm->file->brush_sides;

	bsp->num_brush_sides = bsp->cm->file->num_brush_sides;
	bsp->brush_sides = out = Mem_LinkMalloc(bsp->num_brush_sides * sizeof(*out), bsp);

	for (int32_t i = 0; i < bsp->num_brush_sides; i++, in++, out++) {

		out->plane = bsp->planes + in->plane;

		if (in->material > -1) {
			out->material = bsp->materials[in->material];
		}

		out->axis[0] = in->axis[0];
		out->axis[1] = in->axis[1];

		out->contents = in->contents;
		out->surface = in->surface;
		out->value = in->value;
	}
}

/**
 * @brief
 */
static void R_LoadBspVertexes(r_bsp_model_t *bsp) {

	bsp->num_vertexes = bsp->cm->file->num_vertexes;
	r_bsp_vertex_t *out = bsp->vertexes = Mem_LinkMalloc(bsp->num_vertexes * sizeof(*out), bsp);

	const bsp_vertex_t *in = bsp->cm->file->vertexes;
	for (int32_t i = 0; i < bsp->num_vertexes; i++, in++, out++) {

		out->position = in->position;
		out->normal = in->normal;
		out->tangent = in->tangent;
		out->bitangent = in->bitangent;
		out->diffusemap = in->diffusemap;
		out->lightmap = in->lightmap;
		out->color = in->color;
	}
}

/**
 * @brief
 */
static void R_LoadBspElements(r_bsp_model_t *bsp) {

	bsp->num_elements = bsp->cm->file->num_elements;
	GLuint *out = bsp->elements = Mem_LinkMalloc(bsp->num_elements * sizeof(*out), bsp);

	const int32_t *in = bsp->cm->file->elements;
	for (int32_t i = 0; i < bsp->num_elements; i++, in++, out++) {
		*out = *in;
	}
}

/**
 * @brief Loads all r_bsp_face_t for the specified BSP model.
 */
static void R_LoadBspFaces(r_bsp_model_t *bsp) {

	const bsp_face_t *in = bsp->cm->file->faces;
	r_bsp_face_t *out;

	bsp->num_faces = bsp->cm->file->num_faces;
	bsp->faces = out = Mem_LinkMalloc(bsp->num_faces * sizeof(*out), bsp);

	for (int32_t i = 0; i < bsp->num_faces; i++, in++, out++) {

		out->brush_side = bsp->brush_sides + in->brush_side;
		out->plane = bsp->planes + in->plane;

		out->bounds = in->bounds;

		out->vertexes = bsp->vertexes + in->first_vertex;
		out->num_vertexes = in->num_vertexes;

		out->elements = (GLvoid *) (in->first_element * sizeof(GLuint));
		out->num_elements = in->num_elements;

		if (out->brush_side->surface & SURF_MASK_NO_LIGHTMAP) {
			continue;
		}

		r_bsp_face_lightmap_t *lm = &out->lightmap;

		out->lightmap.s = in->lightmap.s;
		out->lightmap.t = in->lightmap.t;
		out->lightmap.w = in->lightmap.w;
		out->lightmap.h = in->lightmap.h;

		out->lightmap.matrix = in->lightmap.matrix;

		out->lightmap.st_mins = in->lightmap.st_mins;
		out->lightmap.st_maxs = in->lightmap.st_maxs;

		lm->stainmap = Mem_LinkMalloc(lm->w * lm->h * BSP_LIGHTMAP_BPP, bsp->faces);
		memset(lm->stainmap, 0xff, lm->w * lm->h * BSP_LIGHTMAP_BPP);
	}
}

/**
 * @brief
 */
static void R_LoadBspDrawElements(r_bsp_model_t *bsp) {
	r_bsp_draw_elements_t *out;

	bsp->num_draw_elements = bsp->cm->file->num_draw_elements;
	bsp->draw_elements = out = Mem_LinkMalloc(bsp->num_draw_elements * sizeof(*out), bsp);

	const bsp_draw_elements_t *in = bsp->cm->file->draw_elements;
	for (int32_t i = 0; i < bsp->num_draw_elements; i++, in++, out++) {

		out->plane = bsp->planes + in->plane;
		out->material = bsp->materials[in->material];
		out->surface = in->surface;

		out->bounds = in->bounds;

		out->num_elements = in->num_elements;
		out->elements = (GLvoid *) (in->first_element * sizeof(GLuint));

		if ((out->surface & SURF_ALPHA_TEST) && out->material->cm->alpha_test == 0.f) {
			out->material->cm->alpha_test = DEFAULT_ALPHA_TEST;
		}

		if (out->material->cm->stage_flags & (STAGE_STRETCH | STAGE_ROTATE)) {

			vec2_t st_mins = Vec2_Mins();
			vec2_t st_maxs = Vec2_Maxs();

			const GLuint *e = bsp->elements + in->first_element;
			for (int32_t j = 0; j < out->num_elements; j++, e++) {
				const r_bsp_vertex_t *v = &bsp->vertexes[*e];

				st_mins = Vec2_Minf(st_mins, v->diffusemap);
				st_maxs = Vec2_Maxf(st_maxs, v->diffusemap);
			}

			out->st_origin = Vec2_Scale(Vec2_Add(st_mins, st_maxs), .5f);
		}

		if (out->surface & SURF_SKY) {
			if (bsp->sky) {
				Com_Warn("Model contains multiple sky elements\n");
			} else {
				bsp->sky = out;
			}
		}
	}
}

/**
 * @brief Loads all r_bsp_leaf_t for the specified BSP model.
 */
static void R_LoadBspLeafs(r_bsp_model_t *bsp) {
	r_bsp_leaf_t *out;

	const bsp_leaf_t *in = bsp->cm->file->leafs;

	bsp->num_leafs = bsp->cm->file->num_leafs;
	bsp->leafs = out = Mem_LinkMalloc(bsp->num_leafs * sizeof(*out), bsp);

	for (int32_t i = 0; i < bsp->num_leafs; i++, in++, out++) {

		out->contents = in->contents;
		out->bounds = in->bounds;
		out->visible_bounds = in->visible_bounds;
	}
}

/**
 * @brief Loads all r_bsp_node_t for the specified BSP model.
 */
static void R_LoadBspNodes(r_bsp_model_t *bsp) {
	r_bsp_node_t *out;

	const bsp_node_t *in = bsp->cm->file->nodes;

	bsp->num_nodes = bsp->cm->file->num_nodes;
	bsp->nodes = out = Mem_LinkMalloc(bsp->num_nodes * sizeof(*out), bsp);

	for (int32_t i = 0; i < bsp->num_nodes; i++, in++, out++) {

		out->contents = CONTENTS_NODE; // differentiate from leafs
		out->bounds = in->bounds;
		out->visible_bounds = in->visible_bounds;

		out->plane = bsp->planes + in->plane;

		out->faces = bsp->faces + in->first_face;
		out->num_faces = in->num_faces;

		for (int32_t j = 0; j < 2; j++) {
			const int32_t c = in->children[j];
			if (c >= 0) {
				out->children[j] = bsp->nodes + c;
			} else {
				out->children[j] = (r_bsp_node_t *) (bsp->leafs + (-1 - c));
			}
		}
	}
}

/**
 * @brief
 */
static void R_DestroyNodeOcclusionQueries(r_bsp_node_t *node) {

	if (node->contents != CONTENTS_NODE) {
		return;
	}

	R_DestroyOcclusionQuery(&node->query);

	R_DestroyNodeOcclusionQueries(node->children[0]);
	R_DestroyNodeOcclusionQueries(node->children[1]);
}

/**
 * @brief Sets up in-memory relationships between node, parent and model.
 * @remarks Additionally, occlusion queries are generated from the visible bounds of the node.
 * The desired occlusion query size is controlled by `r_occlusion_query_size`. Nodes of this size
 * or greater are selected from the tree using bottom-up recursion.
 * @return True if an occlusion query was generated for the node.
 */
static _Bool R_SetupBspNode(r_bsp_inline_model_t *model, r_bsp_node_t *parent, r_bsp_node_t *node) {

	node->model = model;
	node->parent = parent;

	if (node->contents != CONTENTS_NODE) {
		return false;
	}

	r_bsp_face_t *face = node->faces;
	for (int32_t i = 0; i < node->num_faces; i++, face++) {
		face->node = node;
	}

	const _Bool a = R_SetupBspNode(model, node, node->children[0]);
	const _Bool b = R_SetupBspNode(model, node, node->children[1]);

	if (!a || !b) {

		const float size = r_occlusion_query_size->value;
		if (Box3_Volume(node->visible_bounds) > size * size * size) {
			if (a) {
				R_DestroyNodeOcclusionQueries(node->children[0]);
			}
			if (b) {
				R_DestroyNodeOcclusionQueries(node->children[1]);
			}
			node->query = R_CreateOcclusionQuery(Box3_Expand(node->visible_bounds, 1.f));
			return true;
		}
	}

	return a || b;
}

/**
 * @brief
 */
static void R_LoadBspInlineModels(r_bsp_model_t *bsp) {
	r_bsp_inline_model_t *out;

	const bsp_model_t *in = bsp->cm->file->models;

	bsp->num_inline_models = bsp->cm->file->num_models;
	bsp->inline_models = out = Mem_LinkMalloc(bsp->num_inline_models * sizeof(*out), bsp);

	for (int32_t i = 0; i < bsp->num_inline_models; i++, in++, out++) {

		out->def = bsp->cm->entities[in->entity];
		out->head_node = bsp->nodes + in->head_node;

		out->bounds = in->bounds;

		out->faces = bsp->faces + in->first_face;
		out->num_faces = in->num_faces;

		out->blend_elements = g_ptr_array_new();

		out->draw_elements = bsp->draw_elements + in->first_draw_elements;
		out->num_draw_elements = in->num_draw_elements;

		R_SetupBspNode(out, NULL, out->head_node);
	}
}

/**
 * @brief
 */
static void R_LoadBspLights(r_bsp_model_t *bsp) {

	const bsp_light_t *in = bsp->cm->file->lights;

	bsp->num_lights = bsp->cm->file->num_lights;
	r_bsp_light_t *out = bsp->lights = Mem_LinkMalloc(sizeof(*out) * bsp->num_lights, bsp);

	for (int32_t i = 0; i < bsp->num_lights; i++, in++, out++) {

		out->type = in->type;
		out->atten = in->atten;
		out->origin = in->origin;
		out->color = in->color;
		out->normal = in->normal;
		out->radius = in->radius;
		out->size = in->size;
		out->intensity = in->intensity;
		out->shadow = in->shadow;
		out->cone = in->cone;
		out->falloff = in->falloff;
		out->bounds = in->bounds;
	}
}

/**
 * @brief Loads the lightmap layers to a 2D array texture, appending a layer for the stainmap.
 */
static void R_LoadBspLightmap(r_bsp_model_t *bsp) {

	const bsp_lightmap_t *in = bsp->cm->file->lightmap;

	r_bsp_lightmap_t *out = bsp->lightmap = Mem_LinkMalloc(sizeof(*out), bsp);

	if (in) {
		out->width = in->width;
	} else {
		out->width = 1;
	}

	out->atlas = (r_image_t *) R_AllocMedia("lightmap", sizeof(r_image_t), R_MEDIA_IMAGE);
	out->atlas->media.Free = R_FreeImage;
	out->atlas->type = IT_LIGHTMAP;
	out->atlas->width = out->width;
	out->atlas->height = out->width;
	out->atlas->depth = BSP_LIGHTMAP_LAST;
	out->atlas->target = GL_TEXTURE_2D_ARRAY;
	out->atlas->internal_format = GL_RGBA32F;
	out->atlas->format = GL_RGBA;
	out->atlas->pixel_type = GL_FLOAT;

	const size_t layer_size = out->width * out->width * sizeof(color_t);

	const size_t in_size = layer_size * BSP_LIGHTMAP_LAYERS;
	const size_t out_size = in_size + layer_size * BSP_STAINMAP_LAYERS;

	const byte *in_data = (byte *) in + sizeof(bsp_lightmap_t);
	color32_t *in_color = (color32_t *) in_data;

	byte *out_data = Mem_Malloc(out_size);
	color_t *out_color = (color_t *) out_data;

	for (bsp_lightmap_texture_t i = BSP_LIGHTMAP_FIRST; i < BSP_LIGHTMAP_LAST; i++) {
		for (int32_t x = 0; x < out->width; x++) {
			for (int32_t y = 0; y < out->width; y++, out_color++, in_color++) {
				if (in) {
					switch (i) {
						case BSP_LIGHTMAP_CAUSTICS:
							*out_color = Color32_Color(*in_color);
							break;
						case BSP_LIGHTMAP_DIRECTION_0:
						case BSP_LIGHTMAP_DIRECTION_1: {
							const vec4_t unclamped = Vec4bv(in_color->rgba);
							out_color->r = unclamped.x;
							out_color->g = unclamped.y;
							out_color->b = unclamped.z;
							out_color->a = unclamped.w;
						}
							break;
						default:
							*out_color = Color32_RGBE(*in_color);
							break;
					}
				} else {
					switch (i) {
						case BSP_LIGHTMAP_CAUSTICS:
							*out_color = Color4f(0.f, 0.f, 0.f, 0.f);
							break;
						case BSP_LIGHTMAP_DIRECTION_0:
						case BSP_LIGHTMAP_DIRECTION_1:
							*out_color = Color4f(0.f, 0.f, 1.f, 0.f);
							break;
						default:
							*out_color = Color4f(1.f, 1.f, 1.f, 1.f);
							break;
					}
				}
			}
		}
	}

	R_UploadImage(out->atlas, out_data);

	Mem_Free(out_data);
}

/**
 * @brief Resets all face stainmaps in the event that the map is reloaded.
 */
static void R_ResetBspLightmap(r_bsp_model_t *bsp) {

	r_bsp_lightmap_t *out = bsp->lightmap;

	glBindTexture(GL_TEXTURE_2D_ARRAY, out->atlas->texnum);

	r_bsp_face_t *face = bsp->faces;
	for (int32_t i = 0; i < bsp->num_faces; i++, face++) {

		memset(face->lightmap.stainmap, 0xff, face->lightmap.w * face->lightmap.h * BSP_LIGHTMAP_BPP);

		glTexSubImage3D(GL_TEXTURE_2D_ARRAY,
				0,
				face->lightmap.s,
				face->lightmap.t,
				BSP_LIGHTMAP_LAYERS,
				face->lightmap.w,
				face->lightmap.h,
				1,
				GL_RGB,
				GL_UNSIGNED_BYTE,
				face->lightmap.stainmap);
	}
}

/**
 * @brief
 */
static void R_LoadBspLightgrid(r_model_t *mod) {

	const bsp_lightgrid_t *in = mod->bsp->cm->file->lightgrid;

	r_bsp_lightgrid_t *out = mod->bsp->lightgrid = Mem_LinkMalloc(sizeof(*out), mod->bsp);

	if (in) {
		out->size = in->size;
	} else {
		out->size = Vec3i(1, 1, 1);
	}

	const vec3_t grid_size = Vec3_Scale(Vec3i_CastVec3(out->size), BSP_LIGHTGRID_LUXEL_SIZE);
	const vec3_t world_size = Box3_Size(mod->bounds);
	const vec3_t padding = Vec3_Scale(Vec3_Subtract(grid_size, world_size), 0.5);

	out->bounds = Box3_Expand3(mod->bounds, padding);

	const size_t luxels = out->size.x * out->size.y * out->size.z;
	const size_t out_size = luxels * sizeof(color_t) * BSP_LIGHTGRID_TEXTURES;

	const byte *in_data = (byte *) in + sizeof(bsp_lightgrid_t);
	byte *out_data = Mem_Malloc(out_size);

	for (bsp_lightgrid_texture_t i = BSP_LIGHTGRID_FIRST; i < BSP_LIGHTGRID_LAST; i++) {

		r_image_t *texture = (r_image_t *) R_AllocMedia(va("lightgrid[%d]", i), sizeof(r_image_t), R_MEDIA_IMAGE);
		texture->media.Free = R_FreeImage;
		texture->type = IT_LIGHTGRID;
		texture->width = out->size.x;
		texture->height = out->size.y;
		texture->depth = out->size.z;
		texture->target = GL_TEXTURE_3D;
		texture->internal_format = GL_RGBA32F;
		texture->format = GL_RGBA;
		texture->pixel_type = GL_FLOAT;

		const byte *in_layer = in_data + i * luxels * sizeof(color32_t);
		byte *out_layer = out_data + i * luxels * sizeof(color_t);

		const color32_t *in_color = (color32_t *) in_layer;
		color_t *out_color = (color_t *) out_layer;

		for (int32_t x = 0; x < out->size.x; x++) {
			for (int32_t y = 0; y < out->size.y; y++) {
				for (int32_t z = 0; z < out->size.z; z++, out_color++, in_color++) {
					if (in) {
						switch (i) {
							case BSP_LIGHTGRID_CAUSTICS:
							case BSP_LIGHTGRID_FOG:
								*out_color = Color32_Color(*in_color);
								break;
							case BSP_LIGHTGRID_DIRECTION: {
								const vec4_t unclamped = Vec4bv(in_color->rgba);
								out_color->r = unclamped.x;
								out_color->g = unclamped.y;
								out_color->b = unclamped.z;
								out_color->a = unclamped.w;
							}
								break;
							default:
								*out_color = Color32_RGBE(*in_color);
								break;
						}
					} else {
						switch (i) {
							case BSP_LIGHTGRID_CAUSTICS:
							case BSP_LIGHTGRID_FOG:
								*out_color = Color4f(0.f, 0.f, 0.f, 0.f);
								break;
							case BSP_LIGHTGRID_DIRECTION:
								*out_color = Color4f(0.f, 0.f, 1.f, 0.f);
								break;
							default:
								*out_color = Color4f(1.f, 1.f, 1.f, 1.f);
								break;
						}
					}
				}
			}
		}

		R_UploadImage(texture, out_layer);

		out->textures[i] = texture;
	}
}

/**
 * @brief
 */
static void R_LoadBspVertexArray(r_model_t *mod) {

	glGenVertexArrays(1, &mod->bsp->vertex_array);
	glBindVertexArray(mod->bsp->vertex_array);

	glGenBuffers(1, &mod->bsp->vertex_buffer);
	glBindBuffer(GL_ARRAY_BUFFER, mod->bsp->vertex_buffer);
	glBufferData(GL_ARRAY_BUFFER, mod->bsp->num_vertexes * sizeof(r_bsp_vertex_t), mod->bsp->vertexes, GL_STATIC_DRAW);

	glGenBuffers(1, &mod->bsp->elements_buffer);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mod->bsp->elements_buffer);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, mod->bsp->num_elements * sizeof(GLuint), mod->bsp->elements, GL_STATIC_DRAW);

	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(r_bsp_vertex_t), (void *) offsetof(r_bsp_vertex_t, position));
	glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(r_bsp_vertex_t), (void *) offsetof(r_bsp_vertex_t, normal));
	glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, sizeof(r_bsp_vertex_t), (void *) offsetof(r_bsp_vertex_t, tangent));
	glVertexAttribPointer(3, 3, GL_FLOAT, GL_FALSE, sizeof(r_bsp_vertex_t), (void *) offsetof(r_bsp_vertex_t, bitangent));
	glVertexAttribPointer(4, 2, GL_FLOAT, GL_FALSE, sizeof(r_bsp_vertex_t), (void *) offsetof(r_bsp_vertex_t, diffusemap));
	glVertexAttribPointer(5, 2, GL_FLOAT, GL_FALSE, sizeof(r_bsp_vertex_t), (void *) offsetof(r_bsp_vertex_t, lightmap));
	glVertexAttribPointer(6, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(r_bsp_vertex_t), (void *) offsetof(r_bsp_vertex_t, color));

	R_GetError(mod->media.name);

	glBindVertexArray(0);
	
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
}

/**
 * @brief Create the depth elements buffer for the given inline model.
 */
static void R_LoadBspInlineModelDepthPassElements(const r_bsp_model_t *bsp, r_bsp_inline_model_t *in) {

	glGenBuffers(1, &in->depth_pass_elements_buffer);

	const r_bsp_draw_elements_t *draw = in->draw_elements;
	for (int32_t i = 0; i < in->num_draw_elements; i++, draw++) {

		if (!(draw->surface & SURF_MASK_TRANSLUCENT)) {
			in->num_depth_pass_elements += draw->num_elements;
		}
	}

	glBindBuffer(GL_COPY_READ_BUFFER, bsp->elements_buffer);
	glBindBuffer(GL_COPY_WRITE_BUFFER, in->depth_pass_elements_buffer);

	glBufferData(GL_COPY_WRITE_BUFFER, in->num_depth_pass_elements * sizeof(GLuint), NULL, GL_STATIC_DRAW);

	draw = in->draw_elements;

	GLintptr offset = 0;

	for (int32_t i = 0; i < in->num_draw_elements; i++, draw++) {

		if (draw->surface & SURF_MASK_TRANSLUCENT) {
			continue;
		}

		glCopyBufferSubData(GL_COPY_READ_BUFFER,
							GL_COPY_WRITE_BUFFER,
							(GLintptr) draw->elements,
							(GLintptr) offset,
							draw->num_elements * sizeof(GLuint));

		offset += draw->num_elements * sizeof(GLuint);
	}

	glBindBuffer(GL_COPY_READ_BUFFER, 0);
	glBindBuffer(GL_COPY_WRITE_BUFFER, 0);
}

/**
 * @brief Creates an r_model_t for each inline model so that entities may reference them.
 */
static void R_SetupBspInlineModels(r_model_t *mod) {

	r_bsp_inline_model_t *in = mod->bsp->inline_models;
	for (int32_t i = 0; i < mod->bsp->num_inline_models; i++, in++) {

		R_LoadBspInlineModelDepthPassElements(mod->bsp, in);

		char name[MAX_QPATH];
		g_snprintf(name, sizeof(name), "%s#%d", mod->media.name, i);

		r_model_t *out = (r_model_t *) R_AllocMedia(name, sizeof(r_model_t), R_MEDIA_MODEL);

		out->type = MOD_BSP_INLINE;
		out->bsp_inline = in;

		out->bounds = in->bounds;

		mod->bounds = Box3_Union(mod->bounds, out->bounds);

		R_RegisterDependency(&mod->media, &out->media);
	}
}

/**
 * @brief Extra lumps we need to load for the renderer.
 */
#define R_BSP_LUMPS ( \
	(1 << BSP_LUMP_VERTEXES) | \
	(1 << BSP_LUMP_ELEMENTS) | \
	(1 << BSP_LUMP_FACES) | \
	(1 << BSP_LUMP_DRAW_ELEMENTS) | \
	(1 << BSP_LUMP_LIGHTS) | \
	(1 << BSP_LUMP_LIGHTMAP) | \
	(1 << BSP_LUMP_LIGHTGRID) \
)

/**
 * @brief
 */
static void R_LoadBspModel(r_model_t *mod, void *buffer) {

	bsp_header_t *header = (bsp_header_t *) buffer;

	mod->bsp = Mem_LinkMalloc(sizeof(r_bsp_model_t), mod);
	mod->bsp->cm = Cm_Bsp();

	// load in lumps that the renderer needs
	Bsp_LoadLumps(header, mod->bsp->cm->file, R_BSP_LUMPS);

	R_LoadBspEntities(mod->bsp);
	R_LoadBspPlanes(mod->bsp);
	R_LoadBspMaterials(mod);
	R_LoadBspBrushSides(mod->bsp);
	R_LoadBspVertexes(mod->bsp);
	R_LoadBspElements(mod->bsp);
	R_LoadBspFaces(mod->bsp);
	R_LoadBspDrawElements(mod->bsp);
	R_LoadBspLeafs(mod->bsp);
	R_LoadBspNodes(mod->bsp);
	R_LoadBspInlineModels(mod->bsp);
	R_LoadBspVertexArray(mod);
	R_SetupBspInlineModels(mod);
	R_LoadBspLights(mod->bsp);
	R_LoadBspLightmap(mod->bsp);
	R_LoadBspLightgrid(mod);

	if (r_draw_bsp_lightgrid->value) {
		Bsp_UnloadLumps(mod->bsp->cm->file, R_BSP_LUMPS & ~(1 << BSP_LUMP_LIGHTGRID));
	} else {
		Bsp_UnloadLumps(mod->bsp->cm->file, R_BSP_LUMPS);
	}

	Com_Debug(DEBUG_RENDERER, "!================================\n");
	Com_Debug(DEBUG_RENDERER, "!R_LoadBspModel:   %s\n", mod->media.name);
	Com_Debug(DEBUG_RENDERER, "!  Vertexes:       %d\n", mod->bsp->num_vertexes);
	Com_Debug(DEBUG_RENDERER, "!  Elements:       %d\n", mod->bsp->num_elements);
	Com_Debug(DEBUG_RENDERER, "!  Faces:          %d\n", mod->bsp->num_faces);
	Com_Debug(DEBUG_RENDERER, "!  Draw elements:  %d\n", mod->bsp->num_draw_elements);
	Com_Debug(DEBUG_RENDERER, "!================================\n");
}

/**
 * @brief
 */
static void R_RegisterBspModel(r_media_t *self) {

	r_model_t *mod = (r_model_t *) self;

	R_RegisterDependency(self, (r_media_t *) mod->bsp->lightmap->atlas);

	R_ResetBspLightmap(mod->bsp);

	for (size_t i = 0; i < lengthof(mod->bsp->lightgrid->textures); i++) {
		R_RegisterDependency(self, (r_media_t *) mod->bsp->lightgrid->textures[i]);
	}

	r_world_model = mod;
}

/**
 * @brief
 */
static void R_FreeBspModel(r_media_t *self) {
	r_model_t *mod = (r_model_t *) self;

	glDeleteBuffers(1, &mod->bsp->vertex_buffer);
	glDeleteBuffers(1, &mod->bsp->elements_buffer);

	glDeleteVertexArrays(1, &mod->bsp->vertex_array);

	r_bsp_inline_model_t *in = mod->bsp->inline_models;
	for (int32_t i = 0; i < mod->bsp->num_inline_models; i++, in++) {
		glDeleteBuffers(1, &in->depth_pass_elements_buffer);
		g_ptr_array_free(in->blend_elements, 1);
	}

	r_bsp_node_t *node = mod->bsp->nodes;
	for (int32_t i = 0; i < mod->bsp->num_nodes; i++, node++) {
		R_DestroyOcclusionQuery(&node->query);
	}
}

/**
 * @brief
 */
const r_model_format_t r_bsp_model_format = {
	.extension = "bsp",
	.type = MOD_BSP,
	.Load = R_LoadBspModel,
	.Register = R_RegisterBspModel,
	.Free = R_FreeBspModel
};
