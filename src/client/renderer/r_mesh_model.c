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

#include "parse.h"

#include "r_local.h"

/**
 * @brief Resolves a material for the specified mesh model.
 * @remarks First, it will attempt to use the material explicitly designated on the mesh, if
 * one exists. If that material is not found, it will attempt to load a material based on the mesh's name.
 * Finally, if that fails, it will fall back to using model path + "skin".
 */
static r_material_t *R_ResolveModelMaterial(const r_model_t *mod, const r_model_mesh_t *mesh, const char *mesh_material) {
	char path[MAX_QPATH];
	r_material_t *material;

	// try explicit material
	if (mesh_material != NULL && mesh_material[0]) {
		material = R_FindMaterial(mesh_material, ASSET_CONTEXT_MODELS);

		if (material) {
			return material;
		}

		Com_Debug(DEBUG_RENDERER, "Couldn't resolve explicit material \"%s\"\n", mesh_material);
	}

	// try implicit mesh name
	if (mesh->name[0]) {
		Dirname(mod->media.name, path);
		g_strlcat(path, mesh->name, sizeof(path));

		material = R_FindMaterial(path, ASSET_CONTEXT_MODELS);

		if (material) {
			return material;
		}

		Com_Debug(DEBUG_RENDERER, "Couldn't resolve implicit mesh material \"%s\"\n", mesh->name);
	}

	// fall back to "skin", which will have been force-loaded by
	// R_LoadModelMaterials
	Dirname(mod->media.name, path);
	g_strlcat(path, "skin", sizeof(path));

	return R_FindMaterial(path, ASSET_CONTEXT_MODELS);
}

/**
 * @brief Parses animation.cfg, loading the frame specifications for the given model.
 */
static void R_LoadMd3Animations(r_model_t *mod, r_md3_t *md3) {
	char path[MAX_QPATH];
	void *buf;
	uint16_t skip = 0;
	char token[MAX_TOKEN_CHARS];
	parser_t parser;

	Dirname(mod->media.name, path);
	strcat(path, "animation.cfg");

	if (Fs_Load(path, &buf) == -1) {
		Com_Warn("No animation.cfg for %s\n", mod->media.name);
		return;
	}

	mod->mesh->animations = Mem_LinkMalloc(sizeof(r_model_animation_t) * MD3_MAX_ANIMATIONS, mod->mesh);

	Parse_Init(&parser, (const char *) buf, PARSER_DEFAULT);

	while (true) {

		if (!Parse_PeekToken(&parser, PARSE_DEFAULT, token, sizeof(token))) {
			break;
		}

		if (!g_strcmp0(token, "footsteps")) {
			Parse_SkipToken(&parser, PARSE_DEFAULT);
			Parse_SkipToken(&parser, PARSE_DEFAULT | PARSE_NO_WRAP);
			continue;
		}

		if (!g_strcmp0(token, "headoffset")) {
			Parse_SkipToken(&parser, PARSE_DEFAULT);
			Parse_SkipPrimitive(&parser, PARSE_DEFAULT | PARSE_NO_WRAP, PARSE_FLOAT, 3);
			continue;
		}

		if (!g_strcmp0(token, "sex")) {
			Parse_SkipToken(&parser, PARSE_DEFAULT);
			Parse_SkipToken(&parser, PARSE_DEFAULT | PARSE_NO_WRAP);
			continue;
		}

		if (*token >= '0' && *token <= '9') {
			r_model_animation_t *a = &mod->mesh->animations[mod->mesh->num_animations];

			if (!Parse_Primitive(&parser, PARSE_DEFAULT, PARSE_UINT16, &a->first_frame, 1)) {
				break;
			}

			if (!Parse_Primitive(&parser, PARSE_DEFAULT | PARSE_NO_WRAP, PARSE_UINT16, &a->num_frames, 1)) {
				break;
			}

			if (!Parse_Primitive(&parser, PARSE_DEFAULT | PARSE_NO_WRAP, PARSE_UINT16, &a->looped_frames, 1)) {
				break;
			}

			if (!Parse_Primitive(&parser, PARSE_DEFAULT | PARSE_NO_WRAP, PARSE_UINT16, &a->hz, 1)) {
				break;
			}

			if (mod->mesh->num_animations == ANIM_LEGS_WALKCR) {
				skip = a->first_frame - mod->mesh->animations[ANIM_TORSO_GESTURE].first_frame;
			}

			if (mod->mesh->num_animations >= ANIM_LEGS_WALKCR) {
				a->first_frame -= skip;
			}

			if (!a->num_frames) {
				Com_Warn("%s: No frames for %d\n", mod->media.name, mod->mesh->num_animations);
			}

			if (!a->hz) {
				Com_Warn("%s: No hz for %d\n", mod->media.name, mod->mesh->num_animations);
			}

			Com_Debug(DEBUG_RENDERER, "Parsed %d: %d %d %d %d\n", mod->mesh->num_animations,
			          a->first_frame, a->num_frames, a->looped_frames, a->hz);

			mod->mesh->num_animations++;

			if (mod->mesh->num_animations == MD3_MAX_ANIMATIONS) {
				Com_Warn("MD3_MAX_ANIMATIONS reached: %s\n", mod->media.name);
				break;
			}

			continue;
		}

		// skip unknown directives until we reach newline
		Parse_SkipToken(&parser, PARSE_DEFAULT);

		while (true) {
			if (!Parse_SkipToken(&parser, PARSE_DEFAULT | PARSE_NO_WRAP)) {
				break;
			}
		}
	}

	Fs_Free(buf);

	Com_Debug(DEBUG_RENDERER, "Loaded %d animations: %s\n", mod->mesh->num_animations, mod->media.name);
}

/**
 * @brief Loads the specified r_mesh_config_t from the file at path.
 */
static void R_LoadMeshConfig(r_mesh_config_t *config, const char *path) {
	void *buf;
	parser_t parser;
	char token[MAX_STRING_CHARS];

	if (Fs_Load(path, &buf) == -1) {
		return;
	}

	Parse_Init(&parser, (const char *) buf, PARSER_DEFAULT);

	while (true) {

		if (!Parse_Token(&parser, PARSE_DEFAULT, token, sizeof(token))) {
			break;
		}

		if (!g_strcmp0(token, "translate")) {

			if (Parse_Primitive(&parser, PARSE_DEFAULT | PARSE_WITHIN_QUOTES | PARSE_NO_WRAP, PARSE_FLOAT, config->translate, 3) != 3) {
				break;
			}

			continue;
		}

		if (!g_strcmp0(token, "rotate")) {

			if (Parse_Primitive(&parser, PARSE_DEFAULT | PARSE_WITHIN_QUOTES | PARSE_NO_WRAP, PARSE_FLOAT, config->rotate, 3) != 3) {
				break;
			}

			continue;
		}

		if (!g_strcmp0(token, "scale")) {

			if (!Parse_Primitive(&parser, PARSE_DEFAULT | PARSE_WITHIN_QUOTES | PARSE_NO_WRAP, PARSE_FLOAT, &config->scale, 1)) {
				break;
			}

			continue;
		}

		if (!g_strcmp0(token, "alpha_test")) {
			config->flags |= EF_ALPHATEST;
			continue;
		}

		if (!g_strcmp0(token, "blend")) {
			config->flags |= EF_BLEND;
			continue;
		}
	}

	Fs_Free(buf);
}

/**
 * @brief Loads all r_mesh_config_t for the specified r_model_t. These allow
 * models to be positioned and scaled relative to their own origins, which is
 * useful because artists contribute models in almost arbitrary dimensions at
 * times.
 */
static void R_LoadMeshConfigs(r_model_t *mod) {
	char path[MAX_QPATH];

	mod->mesh->world_config.scale = 1.0;

	Dirname(mod->media.name, path);

	R_LoadMeshConfig(&mod->mesh->world_config, va("%sworld.cfg", path));

	// by default, additional configs inherit from world
	mod->mesh->view_config = mod->mesh->link_config = mod->mesh->world_config;

	R_LoadMeshConfig(&mod->mesh->view_config, va("%sview.cfg", path));
	R_LoadMeshConfig(&mod->mesh->link_config, va("%slink.cfg", path));
}

/**
 * @brief Calculates tangent vectors for each MD3 vertex for per-pixel
 * lighting. See http://www.terathon.com/code/tangent.html.
 */
static void R_LoadMd3Tangents(r_model_mesh_t *mesh, r_md3_mesh_t *md3_mesh) {
	vec3_t *tan1 = (vec3_t *) Mem_Malloc(mesh->num_verts * sizeof(vec3_t));
	vec3_t *tan2 = (vec3_t *) Mem_Malloc(mesh->num_verts * sizeof(vec3_t));

	uint32_t *tri = md3_mesh->tris;

	// resolve the texture directional vectors

	for (uint16_t i = 0; i < mesh->num_tris; i++, tri += 3) {
		vec3_t sdir, tdir;

		const uint32_t i1 = tri[0];
		const uint32_t i2 = tri[1];
		const uint32_t i3 = tri[2];

		const vec_t *v1 = md3_mesh->verts[i1].position;
		const vec_t *v2 = md3_mesh->verts[i2].position;
		const vec_t *v3 = md3_mesh->verts[i3].position;

		const vec_t *w1 = md3_mesh->coords[i1].st;
		const vec_t *w2 = md3_mesh->coords[i2].st;
		const vec_t *w3 = md3_mesh->coords[i3].st;

		vec_t x1 = v2[0] - v1[0];
		vec_t x2 = v3[0] - v1[0];
		vec_t y1 = v2[1] - v1[1];
		vec_t y2 = v3[1] - v1[1];
		vec_t z1 = v2[2] - v1[2];
		vec_t z2 = v3[2] - v1[2];

		vec_t s1 = w2[0] - w1[0];
		vec_t s2 = w3[0] - w1[0];
		vec_t t1 = w2[1] - w1[1];
		vec_t t2 = w3[1] - w1[1];

		vec_t r = 1.0 / (s1 * t2 - s2 * t1);

		VectorSet(sdir,
		          (t2 * x1 - t1 * x2),
		          (t2 * y1 - t1 * y2),
		          (t2 * z1 - t1 * z2)
		         );

		VectorScale(sdir, r, sdir);

		VectorSet(tdir,
		          (s1 * x2 - s2 * x1),
		          (s1 * y2 - s2 * y1),
		          (s1 * z2 - s2 * z1)
		         );

		VectorScale(tdir, r, tdir);

		VectorAdd(tan1[i1], sdir, tan1[i1]);
		VectorAdd(tan1[i2], sdir, tan1[i2]);
		VectorAdd(tan1[i3], sdir, tan1[i3]);

		VectorAdd(tan2[i1], tdir, tan2[i1]);
		VectorAdd(tan2[i2], tdir, tan2[i2]);
		VectorAdd(tan2[i3], tdir, tan2[i3]);
	}

	// calculate the tangents

	for (uint16_t i = 0; i < mesh->num_verts; i++) {
		const vec_t *normal = md3_mesh->verts[i].normal;
		vec_t *tangent = md3_mesh->verts[i].tangent;
		vec_t *bitangent = md3_mesh->verts[i].bitangent;

		TangentVectors(normal, tan1[i], tan2[i], tangent, bitangent);
	}

	Mem_Free(tan1);
	Mem_Free(tan2);
}

typedef struct {
	vec3_t vertex;
	vec3_t normal;
	vec3_t tangent;
	vec3_t bitangent;
} r_md3_interleave_vertex_t;

/**
 * @brief Loads and populates vertex array data for the specified MD3 model.
 *
 * @remarks Static MD3 meshes receive vertex, normal and tangent arrays in
 * addition to texture coordinate arrays. Animated models receive only texture
 * coordinates, because they must be interpolated at each frame.
 */
static void R_LoadMd3VertexArrays(r_model_t *mod, r_md3_t *md3) {
//	mod->num_verts = 0;
//
//	for (uint16_t i = 0; i < mod->mesh->num_meshes; i++) {
//		const r_model_mesh_t *mesh = &mod->mesh->meshes[i];
//		mod->num_verts += mesh->num_verts;
//		mod->num_elements += mesh->num_tris * 3;
//	}
//
//	// make the scratch space
//	const size_t vert_size = mod->num_verts * sizeof(r_md3_interleave_vertex_t);
//	const size_t texcoord_size = mod->num_verts * sizeof(vec2_t);
//	const size_t elem_size = mod->num_elements * sizeof(uint32_t);
//
//	r_md3_interleave_vertex_t *vertexes = Mem_Malloc(vert_size);
//	u16vec_t *texcoords = Mem_Malloc(texcoord_size);
//	uint32_t *tris = Mem_Malloc(elem_size);
//
//	// upload initial data
//	R_CreateInterleaveBuffer(&mod->mesh->vertex_buffer, &(const r_create_interleave_t) {
//		.struct_size = sizeof(r_md3_interleave_vertex_t),
//		.layout = r_md3_buffer_layout,
//		.hint = GL_STATIC_DRAW,
//		.size = vert_size * mod->mesh->num_frames
//	});
//
//	uint32_t *out_tri = tris;
//	u16vec_t *out_texcoord = texcoords;
//
//	for (uint16_t f = 0; f < mod->mesh->num_frames; ++f) {
//		const d_md3_frame_t *frame = &md3->frames[f];
//		uint32_t vert_offset = 0;
//
//		for (uint16_t i = 0; i < mod->mesh->num_meshes; i++) { // iterate the meshes
//			const r_model_mesh_t *mesh = &mod->mesh->meshes[i];
//			const r_md3_mesh_t *md3_mesh = &md3->meshes[i];
//
//			for (uint16_t j = 0; j < mesh->num_verts; j++) {
//				const r_model_vertex_t *vert = &(md3_mesh->verts[(f * mesh->num_verts) + j]);
//
//				VectorAdd(frame->translate, vert->point, vertexes[j + vert_offset].vertex);
//				VectorCopy(vert->normal, vertexes[j + vert_offset].normal);
//				VectorCopy(vert->tangent, vertexes[j + vert_offset].tangent);
//				VectorCopy(vert->bitangent, vertexes[j + vert_offset].bitangent);
//
//				// only copy st coords once
//				if (f == 0) {
//					const d_md3_texcoord_t *tc = &md3_mesh->coords[j];
//					PackTexcoords(tc->st, out_texcoord);
//					out_texcoord += 2;
//				}
//			}
//
//			// only copy elements once
//			if (f == 0) {
//				const uint32_t *tri = md3_mesh->tris;
//
//				for (uint16_t j = 0; j < mesh->num_tris; ++j) {
//					for (uint16_t k = 0; k < 3; ++k) {
//						*out_tri++ = *tri++ + vert_offset;
//					}
//				}
//			}
//
//			vert_offset += mesh->num_verts;
//		}
//
//		// upload each frame
//		R_UploadToSubBuffer(&mod->mesh->vertex_buffer, vert_size * f, vert_size, vertexes, false);
//	}
//
//	// upload texcoords
//	R_CreateDataBuffer(&mod->mesh->texcoord_buffer, &(const r_create_buffer_t) {
//		.element = {
//			.type = R_TYPE_UNSIGNED_SHORT,
//			.count = 2,
//			.normalized = true
//		},
//		.hint = GL_STATIC_DRAW,
//		.size = texcoord_size,
//		.data = texcoords
//	});
//
//	// upload elements
//	R_CreateElementBuffer(&mod->mesh->element_buffer, &(const r_create_element_t) {
//		.type = R_TYPE_UNSIGNED_INT,
//		.hint = GL_STATIC_DRAW,
//		.size = elem_size,
//		.data = tris
//	});
//
//	// get rid of these, we don't need them any more
//	Mem_Free(texcoords);
//	Mem_Free(tris);
//	Mem_Free(vertexes);
//
//	R_GetError(mod->media.name);
}

/**
 * @brief Loads the d_md3_t contents of buffer to the specified model.
 */
void R_LoadMd3Model(r_model_t *mod, void *buffer) {
	int32_t i, j, l;
	d_md3_t *in_md3;
	r_md3_t *out_md3;
	d_md3_frame_t *in_frame, *out_frame;
	d_md3_tag_t *in_tag;
	r_model_tag_t *out_tag;
	d_md3_orientation_t orient;
	d_md3_mesh_t *in_mesh;
	r_model_mesh_t *out_mesh;
	d_md3_texcoord_t *in_coord, *out_coord;
	d_md3_vertex_t *in_vert;
	r_model_vertex_t *out_vert;
	uint32_t *inindex, *out_index;
	vec_t lat, lng;
	size_t size;

	in_md3 = (d_md3_t *) buffer;

	const int32_t version = LittleLong(in_md3->version);
	if (version != MD3_VERSION) {
		Com_Error(ERROR_DROP, "%s has wrong version number "
		          "(%i should be %i)\n", mod->media.name, version, MD3_VERSION);
	}

	mod->mesh = Mem_LinkMalloc(sizeof(r_mesh_model_t), mod);
	out_md3 = Mem_LinkMalloc(sizeof(r_md3_t), mod->mesh);

	// byte swap the header fields and sanity check
	in_md3->ofs_frames = LittleLong(in_md3->ofs_frames);
	in_md3->ofs_tags = LittleLong(in_md3->ofs_tags);
	in_md3->ofs_meshes = LittleLong(in_md3->ofs_meshes);

	mod->mesh->num_frames = LittleLong(in_md3->num_frames);
	mod->mesh->num_tags = LittleLong(in_md3->num_tags);
	mod->mesh->num_meshes = LittleLong(in_md3->num_meshes);

	if (mod->mesh->num_frames < 1) {
		Com_Error(ERROR_DROP, "%s has no frames\n", mod->media.name);
	}

	if (mod->mesh->num_frames > MD3_MAX_FRAMES) {
		Com_Error(ERROR_DROP, "%s has too many frames\n", mod->media.name);
	}

	if (mod->mesh->num_tags > MD3_MAX_TAGS) {
		Com_Error(ERROR_DROP, "%s has too many tags\n", mod->media.name);
	}

	if (mod->mesh->num_meshes > MD3_MAX_MESHES) {
		Com_Error(ERROR_DROP, "%s has too many meshes\n", mod->media.name);
	}

	// load materials first, so meshes can resolve them
	R_LoadModelMaterials(mod);

	// load the frames
	in_frame = (d_md3_frame_t *) ((byte *) in_md3 + in_md3->ofs_frames);
	size = mod->mesh->num_frames * sizeof(d_md3_frame_t);
	out_md3->frames = out_frame = Mem_LinkMalloc(size, mod->mesh);

	ClearBounds(mod->mins, mod->maxs);

	for (i = 0; i < mod->mesh->num_frames; i++, in_frame++, out_frame++) {
		out_frame->radius = LittleFloat(in_frame->radius);

		for (j = 0; j < 3; j++) {
			out_frame->mins[j] = LittleFloat(in_frame->mins[j]);
			out_frame->maxs[j] = LittleFloat(in_frame->maxs[j]);
			out_frame->translate[j] = LittleFloat(in_frame->translate[j]);
		}

		AddPointToBounds(out_frame->mins, mod->mins, mod->maxs);
		AddPointToBounds(out_frame->maxs, mod->mins, mod->maxs);
	}

	// load the tags
	if (mod->mesh->num_tags) {

		in_tag = (d_md3_tag_t *) ((byte *) in_md3 + in_md3->ofs_tags);
		size = mod->mesh->num_tags * mod->mesh->num_frames * sizeof(r_model_tag_t);
		mod->mesh->tags = out_tag = Mem_LinkMalloc(size, mod->mesh);

		for (i = 0; i < mod->mesh->num_frames; i++) {
			for (l = 0; l < mod->mesh->num_tags; l++, in_tag++, out_tag++) {
				g_strlcpy(out_tag->name, in_tag->name, MD3_MAX_PATH);

				for (j = 0; j < 3; j++) {
					orient.origin[j] = LittleFloat(in_tag->orient.origin[j]);
					orient.axis[0][j] = LittleFloat(in_tag->orient.axis[0][j]);
					orient.axis[1][j] = LittleFloat(in_tag->orient.axis[1][j]);
					orient.axis[2][j] = LittleFloat(in_tag->orient.axis[2][j]);
				}

				Matrix4x4_FromVectors(&out_tag->matrix, orient.axis[0], orient.axis[1],
				                      orient.axis[2], orient.origin);
			}
		}
	}

	// load the meshes
	in_mesh = (d_md3_mesh_t *) ((byte *) in_md3 + in_md3->ofs_meshes);

	size = mod->mesh->num_meshes * sizeof(r_model_mesh_t);
	mod->mesh->meshes = out_mesh = Mem_LinkMalloc(size, mod->mesh);

	size = mod->mesh->num_meshes * sizeof(r_md3_mesh_t);
	out_md3->meshes = Mem_LinkMalloc(size, out_md3);

	for (i = 0; i < mod->mesh->num_meshes; i++, out_mesh++) {
		r_md3_mesh_t *md3_mesh = &out_md3->meshes[i];

		g_strlcpy(out_mesh->name, in_mesh->name, MD3_MAX_PATH);

		in_mesh->ofs_tris = LittleLong(in_mesh->ofs_tris);
		in_mesh->ofs_skins = LittleLong(in_mesh->ofs_skins);
		in_mesh->ofs_tcs = LittleLong(in_mesh->ofs_tcs);
		in_mesh->ofs_verts = LittleLong(in_mesh->ofs_verts);
		in_mesh->size = LittleLong(in_mesh->size);

		in_mesh->num_skins = LittleLong(in_mesh->num_skins);
		out_mesh->num_tris = LittleLong(in_mesh->num_tris);
		out_mesh->num_verts = LittleLong(in_mesh->num_verts);

		if (in_mesh->num_skins > MD3_MAX_SHADERS) {
			Com_Error(ERROR_DROP, "%s: %s has too many shaders", mod->media.name, out_mesh->name);
		}

		if (out_mesh->num_tris > MD3_MAX_TRIANGLES) {
			Com_Error(ERROR_DROP, "%s: %s has too many triangles\n", mod->media.name, out_mesh->name);
		}

		if (out_mesh->num_verts > MD3_MAX_VERTS) {
			Com_Error(ERROR_DROP, "%s: %s has too many vertexes\n", mod->media.name, out_mesh->name);
		}

		// load the first shader, if it exists
		char shader_name[MD3_MAX_PATH] = { '\0' };

		if (in_mesh->num_skins) {
			d_md3_skin_t *skin = (d_md3_skin_t *) ((byte *) in_mesh + in_mesh->ofs_skins);
			g_strlcpy(shader_name, skin->name, MD3_MAX_PATH);
		}

		out_mesh->material = R_ResolveModelMaterial(mod, out_mesh, shader_name);

		// load the triangle indexes
		inindex = (uint32_t *) ((byte *) in_mesh + in_mesh->ofs_tris);
		size = out_mesh->num_tris * sizeof(uint32_t) * 3;
		md3_mesh->tris = out_index = Mem_LinkMalloc(size, out_md3);

		for (j = 0; j < out_mesh->num_tris; j++, inindex += 3, out_index += 3) {
			out_index[0] = (uint32_t) LittleLong(inindex[0]);
			out_index[1] = (uint32_t) LittleLong(inindex[1]);
			out_index[2] = (uint32_t) LittleLong(inindex[2]);
		}

		// load the texcoords
		in_coord = (d_md3_texcoord_t *) ((byte *) in_mesh + in_mesh->ofs_tcs);
		size = out_mesh->num_verts * sizeof(d_md3_texcoord_t);
		md3_mesh->coords = out_coord = Mem_LinkMalloc(size, out_md3);

		for (j = 0; j < out_mesh->num_verts; j++, in_coord++, out_coord++) {
			out_coord->st[0] = LittleFloat(in_coord->st[0]);
			out_coord->st[1] = LittleFloat(in_coord->st[1]);
		}

		// load the verts and norms
		in_vert = (d_md3_vertex_t *) ((byte *) in_mesh + in_mesh->ofs_verts);
		size = mod->mesh->num_frames * out_mesh->num_verts * sizeof(r_model_vertex_t);
		md3_mesh->verts = out_vert = Mem_LinkMalloc(size, out_md3);

		for (l = 0; l < mod->mesh->num_frames; l++) {
			for (j = 0; j < out_mesh->num_verts; j++, in_vert++, out_vert++) {
				out_vert->position[0] = LittleShort(in_vert->point[0]) * MD3_XYZ_SCALE;
				out_vert->position[1] = LittleShort(in_vert->point[1]) * MD3_XYZ_SCALE;
				out_vert->position[2] = LittleShort(in_vert->point[2]) * MD3_XYZ_SCALE;

				lat = (in_vert->norm >> 8) & 0xff;
				lng = (in_vert->norm & 0xff);

				lat *= M_PI / 128.0;
				lng *= M_PI / 128.0;

				out_vert->normal[0] = cosf(lat) * sinf(lng);
				out_vert->normal[1] = sinf(lat) * sinf(lng);
				out_vert->normal[2] = cosf(lng);
			}
		}

		R_LoadMd3Tangents(out_mesh, md3_mesh);

		out_mesh->num_elements = out_mesh->num_tris * 3;

		Com_Debug(DEBUG_RENDERER, "%s: %s: %d triangles (%d elements)\n", mod->media.name, out_mesh->name, out_mesh->num_tris,
		          out_mesh->num_elements);

		in_mesh = (d_md3_mesh_t *) ((byte *) in_mesh + in_mesh->size);
	}

	// and animations for player models
	if (strstr(mod->media.name, "/upper")) {
		R_LoadMd3Animations(mod, out_md3);
	}

	// and the configs
	R_LoadMeshConfigs(mod);

	// and finally load the arrays
	R_LoadMd3VertexArrays(mod, out_md3);

	// get rid of temporary space
	Mem_Free(out_md3);

	Com_Debug(DEBUG_RENDERER, "%s\n  %d meshes\n  %d frames\n  %d tags\n  %d vertexes\n", mod->media.name,
	          mod->mesh->num_meshes, mod->mesh->num_frames, mod->mesh->num_tags, mod->mesh->num_verts);
}

/**
 * @brief Resolves the the vertex described by `indices`, allocating a new
 * vertex if `indices` is unique to the model.
 *
 * @remarks In Object file format, primitives are indexed starting at 1, not 0.
 */
static size_t R_ObjVertexForIndices(r_model_t *mod, r_obj_t *obj, const uint16_t *indices) {

	for (size_t i = 0; i < obj->verts->len; i++) {
		r_obj_vertex_t *v = &g_array_index(obj->verts, r_obj_vertex_t, i);

		if (memcmp(v->indices, indices, sizeof(v->indices)) == 0) {
			return i;
		}
	}

	r_obj_vertex_t v;

	memcpy(v.indices, indices, sizeof(v.indices));

	v.position = obj->verts->len;
	v.point = (vec_t *) &obj->points[indices[0] - 1];
	v.texcoords = (vec_t *) &obj->texcoords[indices[1] - 1];
	v.normal = (vec_t *) &obj->normals[indices[2] - 1];

	obj->verts = g_array_append_val(obj->verts, v);

	return v.position;
}

/**
 * @brief
 */
static void R_BeginObjGroup(r_obj_t *obj, const char *name) {
	r_obj_group_t *current = &obj->groups[obj->cur_group];
	
	if (!current->num_tris) {
		g_strlcpy(current->name, name, sizeof(current->name));
		return;
	}

	current++;
	g_strlcpy(current->name, name, sizeof(current->name));
	obj->cur_group++;
}

/**
 * @brief
 */
static void R_LoadObjGroups(r_model_t *mod, r_obj_t *obj) {

	mod->mesh->num_meshes = obj->num_groups;

	const size_t size = mod->mesh->num_meshes * sizeof(r_model_mesh_t);

	mod->mesh->meshes = Mem_LinkMalloc(size, mod->mesh);

	GHashTable *unique_hash = g_hash_table_new(g_int_hash, g_int_equal);

	size_t tri_offset = 0;

	for (size_t i = 0; i < mod->mesh->num_meshes; i++) {
		const r_obj_group_t *in_mesh = &obj->groups[i];
		r_model_mesh_t *out_mesh = &mod->mesh->meshes[i];

		g_snprintf(out_mesh->name, sizeof(out_mesh->name), "%s", in_mesh->name);

		out_mesh->num_tris = in_mesh->num_tris;
		out_mesh->num_elements = in_mesh->num_tris * 3;

		for (size_t t = 0; t < in_mesh->num_tris; t++) {
			uint32_t *tri = (uint32_t *) &obj->tris[tri_offset + t];

			g_hash_table_add(unique_hash, &tri[0]);
			g_hash_table_add(unique_hash, &tri[1]);
			g_hash_table_add(unique_hash, &tri[2]);
		}

		out_mesh->num_verts = g_hash_table_size(unique_hash);
		tri_offset += in_mesh->num_tris;

		out_mesh->material = R_ResolveModelMaterial(mod, out_mesh, in_mesh->material);
	}

	g_hash_table_destroy(unique_hash);
}

/**
 * @brief Parses a line of an Object file, allocating primitives accordingly.
 */
static void R_LoadObjPrimitive(r_model_t *mod, r_obj_t *obj, const char *line) {

	r_obj_group_t *current = &obj->groups[obj->cur_group];

	if (g_str_has_prefix(line, "g ")) { // group

		R_BeginObjGroup(obj, line + strlen("g "));

	} else if (g_str_has_prefix(line, "usemtl ")) { // material

		g_strlcpy(current->material, line + strlen("usemtl "), sizeof(current->material));

	} else if (g_str_has_prefix(line, "v ")) { // vertex

		vec3_t v;

		if (sscanf(line + 2, "%f %f %f", &v[0], &v[2], &v[1]) != 3) {
			Com_Error(ERROR_DROP, "Malformed vertex for %s: %s\n", mod->media.name, line);
		}

		AddPointToBounds(v, mod->mins, mod->maxs);

		memcpy(&obj->points[obj->cur_point], v, sizeof(v));
		obj->cur_point++;

	} else if (g_str_has_prefix(line, "vt ")) { // texcoord

		vec2_t vt;

		if (sscanf(line + 3, "%f %f", &vt[0], &vt[1]) != 2) {
			Com_Error(ERROR_DROP, "Malformed texcoord for %s: %s\n", mod->media.name, line);
		}

		vt[1] = -vt[1];

		memcpy(&obj->texcoords[obj->cur_texcoord], vt, sizeof(vt));
		obj->cur_texcoord++;

	} else if (g_str_has_prefix(line, "vn ")) { // normal

		vec3_t vn;

		if (sscanf(line + 3, "%f %f %f", &vn[0], &vn[2], &vn[1]) != 3) {
			Com_Error(ERROR_DROP, "Malformed normal for %s: %s\n", mod->media.name, line);
		}

		VectorNormalize(vn);

		memcpy(&obj->normals[obj->cur_normal], vn, sizeof(vn));
		obj->cur_normal++;

	} else if (g_str_has_prefix(line, "f ")) { // face

		static GArray *verts = NULL;

		if (verts == NULL)
			verts = g_array_sized_new(false, false, sizeof(uint32_t), 3);

		const char *c = line + 2;
		while (*c) {
			uint16_t indices[3];
			int32_t n;

			if (sscanf(c, "%hu/%hu/%hu%n", &indices[0], &indices[1], &indices[2], &n) != 3) {
				Com_Error(ERROR_DROP, "Malformed face for %s: %s\n", mod->media.name, line);
			}

			const size_t position = R_ObjVertexForIndices(mod, obj, indices);
			verts = g_array_append_val(verts, position);
			c += n;
		}

		// iterate the face, converting polygons into triangles

		const size_t len = verts->len;

		if (len < 3) {
			Com_Error(ERROR_DROP, "Malformed face for %s: %s\n", mod->media.name, line);
		}

		for (size_t i = 1; i < len - 1; i++) {
			r_obj_triangle_t tri;

			tri[0] = g_array_index(verts, uint32_t, 0);
			tri[1] = g_array_index(verts, uint32_t, i);
			tri[2] = g_array_index(verts, uint32_t, i + 1);

			memcpy(&obj->tris[obj->cur_tris], &tri, sizeof(tri));
			obj->cur_tris++;
			current->num_tris++;
		}

		g_array_set_size(verts, 0);
	}

	// else we just ignore it
}

/**
 * @brief Parses a line of an Object file, counting primitive accordingly.
 */
static void R_CountObjPrimitive(r_model_t *mod, r_obj_t *obj, const char *line) {

	if (g_str_has_prefix(line, "g ")) { // group

		obj->num_groups++;
	} else if (g_str_has_prefix(line, "v ")) { // vertex

		obj->num_points++;
	} else if (g_str_has_prefix(line, "vt ")) { // texcoord

		obj->num_texcoords++;
	} else if (g_str_has_prefix(line, "vn ")) { // normal

		obj->num_normals++;
	} else if (g_str_has_prefix(line, "f ")) { // face

		int32_t num_verts = 0;

		const char *c = line + 2;
		while (*c) {
			uint16_t indices[3];
			int32_t n;

			if (sscanf(c, "%hu/%hu/%hu%n", &indices[0], &indices[1], &indices[2], &n) != 3) {
				Com_Error(ERROR_DROP, "Malformed face for %s: %s\n", mod->media.name, line);
			}

			num_verts++;
			c += n;
		}

		// iterate the face, converting polygons into triangles

		if (num_verts < 3) {
			Com_Error(ERROR_DROP, "Malformed face for %s: %s\n", mod->media.name, line);
		}

		obj->num_tris += num_verts - 2;
	}

	// else we just ignore it
}

/**
 * @brief Parses the file and counts the number of primitives used; this is to make loading much easier to deal with.
 */
static void R_CountObjPrimitives(r_model_t *mod, r_obj_t *obj, const void *buffer) {
	char line[MAX_STRING_CHARS];
	char *l = line;
	const char *c = buffer;

	memset(line, 0, sizeof(line));

	while (true) {
		switch (*c) {

			case '\r':
			case '\n':
			case '\0':
				l = g_strstrip(line);
				R_CountObjPrimitive(mod, obj, l);

				if (*c == '\0') {
					goto done;
				}

				memset(line, 0, sizeof(line));
				l = line;

				c++;
				break;

			default:
				*l++ = *c++;
				break;
		}
	}

done:
	if (obj->num_groups == 0)
		obj->num_groups = 1;

	Com_Debug(DEBUG_RENDERER, "%s: %u tris in %u groups\n", mod->media.name, obj->num_tris, obj->num_groups);
}

/**
 * @brief Parses the Object primitives from the ASCII text in `buffer`.
 */
static void R_LoadObjPrimitives(r_model_t *mod, r_obj_t *obj, const void *buffer) {
	char line[MAX_STRING_CHARS];
	char *l = line;
	const char *c = buffer;

	memset(line, 0, sizeof(line));

	while (true) {
		switch (*c) {

			case '\r':
			case '\n':
			case '\0':
				l = g_strstrip(line);
				R_LoadObjPrimitive(mod, obj, l);

				if (*c == '\0') {
					goto done;
				}

				memset(line, 0, sizeof(line));
				l = line;

				c++;
				break;

			default:
				*l++ = *c++;
				break;
		}
	}

done:
	;
}

/**
 * @brief Calculate tangent vectors for the given Object model.
 *
 * http://www.terathon.com/code/tangent.html
 */
static void R_LoadObjTangents(r_model_t *mod, r_obj_t *obj) {
	vec3_t *tan1 = (vec3_t *) Mem_Malloc(obj->verts->len * sizeof(vec3_t));
	vec3_t *tan2 = (vec3_t *) Mem_Malloc(obj->verts->len * sizeof(vec3_t));

	uint32_t *tri = (uint32_t *) obj->tris;

	// resolve the texture directional vectors

	for (uint32_t i = 0; i < obj->num_tris; i++, tri += 3) {
		vec3_t sdir, tdir;

		const uint32_t i1 = tri[0];
		const uint32_t i2 = tri[1];
		const uint32_t i3 = tri[2];
		
		const r_obj_vertex_t *vert1 = &g_array_index(obj->verts, r_obj_vertex_t, i1);
		const r_obj_vertex_t *vert2 = &g_array_index(obj->verts, r_obj_vertex_t, i2);
		const r_obj_vertex_t *vert3 = &g_array_index(obj->verts, r_obj_vertex_t, i3);

		const vec_t *v1 = vert1->point;
		const vec_t *v2 = vert2->point;
		const vec_t *v3 = vert3->point;

		const vec_t *w1 = vert1->texcoords;
		const vec_t *w2 = vert2->texcoords;
		const vec_t *w3 = vert3->texcoords;

		vec_t x1 = v2[0] - v1[0];
		vec_t x2 = v3[0] - v1[0];
		vec_t y1 = v2[1] - v1[1];
		vec_t y2 = v3[1] - v1[1];
		vec_t z1 = v2[2] - v1[2];
		vec_t z2 = v3[2] - v1[2];

		vec_t s1 = w2[0] - w1[0];
		vec_t s2 = w3[0] - w1[0];
		vec_t t1 = w2[1] - w1[1];
		vec_t t2 = w3[1] - w1[1];

		vec_t r = 1.0 / (s1 * t2 - s2 * t1);

		VectorSet(sdir,
		          (t2 * x1 - t1 * x2),
		          (t2 * y1 - t1 * y2),
		          (t2 * z1 - t1 * z2)
		         );

		VectorScale(sdir, r, sdir);

		VectorSet(tdir,
		          (s1 * x2 - s2 * x1),
		          (s1 * y2 - s2 * y1),
		          (s1 * z2 - s2 * z1)
		         );

		VectorScale(tdir, r, tdir);

		VectorAdd(tan1[i1], sdir, tan1[i1]);
		VectorAdd(tan1[i2], sdir, tan1[i2]);
		VectorAdd(tan1[i3], sdir, tan1[i3]);

		VectorAdd(tan2[i1], tdir, tan2[i1]);
		VectorAdd(tan2[i2], tdir, tan2[i2]);
		VectorAdd(tan2[i3], tdir, tan2[i3]);
	}

	// calculate the tangents

	for (uint32_t i = 0; i < obj->verts->len; i++) {
		r_obj_vertex_t *v = &g_array_index(obj->verts, r_obj_vertex_t, i);
		const vec_t *normal = v->normal;
		TangentVectors(normal, tan1[i], tan2[i], v->tangent, v->bitangent);
	}

	Mem_Free(tan1);
	Mem_Free(tan2);
}

typedef struct {
	vec3_t vertex;
	vec3_t normal;
	vec3_t tangent;
	vec3_t bitangent;
	u16vec2_t diffuse;
} r_obj_interleave_vertex_t;

/**
 * @brief Populates the vertex arrays of `mod` with triangle data from `obj`.
 */
static void R_LoadObjVertexArrays(r_model_t *mod, r_obj_t *obj) {

	const GLsizei v = obj->verts->len * sizeof(r_obj_interleave_vertex_t);
	const GLsizei e = obj->num_tris * 3 * sizeof(GLuint);

	r_obj_interleave_vertex_t *verts = Mem_LinkMalloc(v, mod);
	GLuint *elements = Mem_LinkMalloc(e, mod);

	r_obj_interleave_vertex_t *vout = verts;
	GLuint *eout = elements;

	for (uint32_t i = 0; i < obj->verts->len; i++) {
		const r_obj_vertex_t *ve = &g_array_index(obj->verts, r_obj_vertex_t, i);

		VectorCopy(ve->point, vout->vertex);

		PackTexcoords(ve->texcoords, vout->diffuse);

		VectorCopy(ve->normal, vout->normal);

		VectorCopy(ve->tangent, vout->tangent);

		VectorCopy(ve->bitangent, vout->bitangent);

		vout++;
	}

	for (uint32_t i = 0; i < obj->num_tris; i++) {
		const uint32_t *te = (const uint32_t *) &obj->tris[i];

		for (int32_t i = 0; i < 3; ++i) {
			const r_obj_vertex_t *ve = &g_array_index(obj->verts, r_obj_vertex_t, te[i]);

			*eout++ = ve->position;
		}
	}

	// load the vertex buffer objects
//	R_CreateInterleaveBuffer(&mod->mesh->vertex_buffer, &(const r_create_interleave_t) {
//		.struct_size = sizeof(r_obj_interleave_vertex_t),
//		.layout = r_obj_buffer_layout,
//		.hint = GL_STATIC_DRAW,
//		.size = v,
//		.data = verts
//	});
//
//	R_CreateElementBuffer(&mod->mesh->element_buffer, &(const r_create_element_t) {
//		.type = R_TYPE_UNSIGNED_INT,
//		.hint = GL_STATIC_DRAW,
//		.size = e,
//		.data = elements
//	});

	// free our temporary buffers
	Mem_Free(verts);
	Mem_Free(elements);

	R_GetError(mod->media.name);
}

/**
 * @brief
 */
void R_LoadObjModel(r_model_t *mod, void *buffer) {

	// load materials first, so meshes can resolve them
	R_LoadModelMaterials(mod);

	mod->mesh = Mem_LinkMalloc(sizeof(r_mesh_model_t), mod);

	r_obj_t *obj = Mem_LinkMalloc(sizeof(r_obj_t), mod->mesh);

	mod->mesh->num_frames = 1;

	ClearBounds(mod->mins, mod->maxs);

	// parse the file, loading primitives
	R_CountObjPrimitives(mod, obj, buffer);

	obj->verts = g_array_new(false, false, sizeof(r_obj_vertex_t));
	obj->points = Mem_LinkMalloc(sizeof(vec3_t) * obj->num_points, obj);
	obj->texcoords = Mem_LinkMalloc(sizeof(vec2_t) * obj->num_texcoords, obj);
	obj->normals = Mem_LinkMalloc(sizeof(vec3_t) * obj->num_normals, obj);
	obj->tris = Mem_LinkMalloc(sizeof(r_obj_triangle_t) * obj->num_tris, obj);
	obj->groups = Mem_LinkMalloc(sizeof(r_obj_group_t) * obj->num_groups, obj);

	R_LoadObjPrimitives(mod, obj, buffer);

	if (obj->num_tris == 0) {
		Com_Error(ERROR_DROP, "Failed to load .obj: %s\n", mod->media.name);
	}

	// set up groups
	R_LoadObjGroups(mod, obj);

	// calculate tangents
	R_LoadObjTangents(mod, obj);

	// and configs
	R_LoadMeshConfigs(mod);

	// and finally the arrays
	R_LoadObjVertexArrays(mod, obj);

	g_array_free(obj->verts, true);

	Mem_Free(obj);
}
