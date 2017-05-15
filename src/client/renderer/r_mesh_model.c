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
#include "parse.h"

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

		const vec_t *v1 = md3_mesh->verts[i1].point;
		const vec_t *v2 = md3_mesh->verts[i2].point;
		const vec_t *v3 = md3_mesh->verts[i3].point;

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
		static vec3_t bitangent;
		const vec_t *normal = md3_mesh->verts[i].normal;
		vec_t *tangent = md3_mesh->verts[i].tangent;

		TangentVectors(normal, tan1[i], tan2[i], tangent, bitangent);
	}

	Mem_Free(tan1);
	Mem_Free(tan2);
}

typedef struct {
	vec3_t vertex;
	vec3_t normal;
	vec4_t tangent;
} r_md3_interleave_vertex_t;

static r_buffer_layout_t r_md3_buffer_layout[] = {
	{ .attribute = R_ARRAY_POSITION, .type = R_ATTRIB_FLOAT, .count = 3, .size = sizeof(vec3_t) },
	{ .attribute = R_ARRAY_NORMAL, .type = R_ATTRIB_FLOAT, .count = 3, .size = sizeof(vec3_t), .offset = 12 },
	{ .attribute = R_ARRAY_TANGENT, .type = R_ATTRIB_FLOAT, .count = 4, .size = sizeof(vec4_t), .offset = 24 },
	{ .attribute = -1 }
};

/**
 * @brief Loads and populates vertex array data for the specified MD3 model.
 *
 * @remarks Static MD3 meshes receive vertex, normal and tangent arrays in
 * addition to texture coordinate arrays. Animated models receive only texture
 * coordinates, because they must be interpolated at each frame.
 */
static void R_LoadMd3VertexArrays(r_model_t *mod, r_md3_t *md3) {
	mod->num_verts = 0;

	for (uint16_t i = 0; i < mod->mesh->num_meshes; i++) {
		const r_model_mesh_t *mesh = &mod->mesh->meshes[i];
		mod->num_verts += mesh->num_verts;
		mod->num_elements += mesh->num_tris * 3;
	}

	// make the scratch space
	const size_t vert_size = mod->num_verts * sizeof(r_md3_interleave_vertex_t);
	const size_t texcoord_size = mod->num_verts * sizeof(vec2_t);
	const size_t elem_size = mod->num_elements * sizeof(uint32_t);
	
	r_md3_interleave_vertex_t *vertexes = Mem_Malloc(vert_size);
	u16vec_t *texcoords = Mem_Malloc(texcoord_size);
	uint32_t *tris = Mem_Malloc(elem_size);

	// upload initial data
	R_CreateInterleaveBuffer(&mod->mesh->vertex_buffer, sizeof(r_md3_interleave_vertex_t), r_md3_buffer_layout,
	                         GL_STATIC_DRAW,
	                         vert_size * mod->mesh->num_frames, NULL);

	uint32_t *out_tri = tris;
	u16vec_t *out_texcoord = texcoords;

	for (uint16_t f = 0; f < mod->mesh->num_frames; ++f) {
		const d_md3_frame_t *frame = &md3->frames[f];
		uint32_t vert_offset = 0;

		for (uint16_t i = 0; i < mod->mesh->num_meshes; i++) { // iterate the meshes
			const r_model_mesh_t *mesh = &mod->mesh->meshes[i];
			const r_md3_mesh_t *md3_mesh = &md3->meshes[i];

			for (uint16_t j = 0; j < mesh->num_verts; j++) {
				const r_model_vertex_t *vert = &(md3_mesh->verts[(f * mesh->num_verts) + j]);

				VectorAdd(frame->translate, vert->point, vertexes[j + vert_offset].vertex);
				VectorCopy(vert->normal, vertexes[j + vert_offset].normal);
				Vector4Copy(vert->tangent, vertexes[j + vert_offset].tangent);

				// only copy st coords once
				if (f == 0) {
					const d_md3_texcoord_t *tc = &md3_mesh->coords[j];
					PackTexcoords(tc->st, out_texcoord);
					out_texcoord += 2;
				}
			}

			// only copy elements once
			if (f == 0) {
				const uint32_t *tri = md3_mesh->tris;

				for (uint16_t j = 0; j < mesh->num_tris; ++j) {
					for (uint16_t k = 0; k < 3; ++k) {
						*out_tri++ = *tri++ + vert_offset;
					}
				}
			}

			vert_offset += mesh->num_verts;
		}

		// upload each frame
		R_UploadToSubBuffer(&mod->mesh->vertex_buffer, vert_size * f, vert_size, vertexes, false);
	}

	// upload texcoords
	R_CreateDataBuffer(&mod->mesh->texcoord_buffer, R_ATTRIB_UNSIGNED_SHORT, 2, true, GL_STATIC_DRAW, texcoord_size, texcoords);

	// upload elements
	R_CreateElementBuffer(&mod->mesh->element_buffer, R_ATTRIB_UNSIGNED_INT, GL_STATIC_DRAW, elem_size, tris);

	// get rid of these, we don't need them any more
	Mem_Free(texcoords);
	Mem_Free(tris);
	Mem_Free(vertexes);

	R_GetError(mod->media.name);
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

	mod->mesh->num_frames = mod->mesh->num_frames = LittleLong(in_md3->num_frames);
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
				memcpy(out_tag->name, in_tag->name, MD3_MAX_PATH);

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

		memcpy(out_mesh->name, in_mesh->name, MD3_MAX_PATH);

		in_mesh->ofs_tris = LittleLong(in_mesh->ofs_tris);
		in_mesh->ofs_skins = LittleLong(in_mesh->ofs_skins);
		in_mesh->ofs_tcs = LittleLong(in_mesh->ofs_tcs);
		in_mesh->ofs_verts = LittleLong(in_mesh->ofs_verts);
		in_mesh->size = LittleLong(in_mesh->size);

		out_mesh->num_tris = LittleLong(in_mesh->num_tris);
		out_mesh->num_verts = LittleLong(in_mesh->num_verts);

		if (out_mesh->num_tris > MD3_MAX_TRIANGLES) {
			Com_Error(ERROR_DROP, "%s: %s has too many triangles\n", mod->media.name, out_mesh->name);
		}

		if (out_mesh->num_verts > MD3_MAX_VERTS) {
			Com_Error(ERROR_DROP, "%s: %s has too many vertexes\n", mod->media.name, out_mesh->name);
		}

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
				out_vert->point[0] = LittleShort(in_vert->point[0]) * MD3_XYZ_SCALE;
				out_vert->point[1] = LittleShort(in_vert->point[1]) * MD3_XYZ_SCALE;
				out_vert->point[2] = LittleShort(in_vert->point[2]) * MD3_XYZ_SCALE;

				lat = (in_vert->norm >> 8) & 0xff;
				lng = (in_vert->norm & 0xff);

				lat *= M_PI / 128.0;
				lng *= M_PI / 128.0;

				out_vert->normal[0] = cos(lat) * sin(lng);
				out_vert->normal[1] = sin(lat) * sin(lng);
				out_vert->normal[2] = cos(lng);
			}
		}

		R_LoadMd3Tangents(out_mesh, md3_mesh);

		mod->num_tris += out_mesh->num_tris;
		out_mesh->num_elements = out_mesh->num_tris * 3;

		Com_Debug(DEBUG_RENDERER, "%s: %s: %d triangles (%d elements)\n", mod->media.name, out_mesh->name, out_mesh->num_tris,
		          out_mesh->num_elements);

		in_mesh = (d_md3_mesh_t *) ((byte *) in_mesh + in_mesh->size);
	}

	// load materials
	R_LoadModelMaterials(mod);

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
	          mod->mesh->num_meshes, mod->mesh->num_frames, mod->mesh->num_tags, mod->num_verts);
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
	
	obj->verts = g_array_append_val(obj->verts, v);

	return v.position;
}

/**
 * @brief
 */
static void R_BeginObjGroup(r_obj_t *obj, const char *name) {
	r_obj_group_t *current = &g_array_index(obj->groups, r_obj_group_t, obj->groups->len - 1);

	if (!current->num_tris) {
		current->name = name;
		return;
	}

	obj->groups = g_array_append_vals(obj->groups, &(const r_obj_group_t) {
		.name = name,
		.num_tris = 0
	}, 1);
}

/**
 * @brief
 */
static void R_EndObjGroup(r_obj_t *obj) {
	r_obj_group_t *current = &g_array_index(obj->groups, r_obj_group_t, obj->groups->len - 1);
	
	if (!current->num_tris) {
		obj->groups = g_array_set_size(obj->groups, obj->groups->len - 1);
	}
}

/**
 * @brief
 */
static void R_LoadObjGroups(r_model_t *mod, r_obj_t *obj) {

	R_EndObjGroup(obj);

	mod->mesh->num_meshes = obj->groups->len;
	
	const size_t size = mod->mesh->num_meshes * sizeof(r_model_mesh_t);

	mod->mesh->meshes = Mem_LinkMalloc(size, mod->mesh);

	GHashTable *unique_hash = g_hash_table_new(g_int_hash, g_int_equal);

	size_t tri_offset = 0;

	for (size_t i = 0; i < mod->mesh->num_meshes; i++) {
		const r_obj_group_t *in_mesh = &g_array_index(obj->groups, r_obj_group_t, i);
		r_model_mesh_t *out_mesh = &mod->mesh->meshes[i];

		g_snprintf(out_mesh->name, sizeof(out_mesh->name), "%s", in_mesh->name);

		out_mesh->num_tris = in_mesh->num_tris;
		out_mesh->num_elements = in_mesh->num_tris * 3;

		for (size_t t = 0; t < in_mesh->num_tris; t++) {
			r_obj_triangle_t *tri = &g_array_index(obj->tris, r_obj_triangle_t, tri_offset + t);
			
			g_hash_table_add(unique_hash, &tri->verts[0]);
			g_hash_table_add(unique_hash, &tri->verts[1]);
			g_hash_table_add(unique_hash, &tri->verts[2]);
		}

		out_mesh->num_verts = g_hash_table_size(unique_hash);
		tri_offset += in_mesh->num_tris;
	}

	g_hash_table_destroy(unique_hash);
}

/**
 * @brief Parses a line of an Object file, allocating primitives accordingly.
 */
static void R_LoadObjPrimitive(r_model_t *mod, r_obj_t *obj, const char *line) {

	if (g_str_has_prefix(line, "g ")) { // vertex

		R_BeginObjGroup(obj, line + 2);

	} else if (g_str_has_prefix(line, "v ")) { // vertex

		vec3_t v;

		if (sscanf(line + 2, "%f %f %f", &v[0], &v[2], &v[1]) != 3) {
			Com_Error(ERROR_DROP, "Malformed vertex for %s: %s\n", mod->media.name, line);
		}

		AddPointToBounds(v, mod->mins, mod->maxs);

		obj->points = g_array_append_vals(obj->points, v, 1);

	} else if (g_str_has_prefix(line, "vt ")) { // texcoord

		vec2_t vt;

		if (sscanf(line + 3, "%f %f", &vt[0], &vt[1]) != 2) {
			Com_Error(ERROR_DROP, "Malformed texcoord for %s: %s\n", mod->media.name, line);
		}

		vt[1] = -vt[1];

		obj->texcoords = g_array_append_vals(obj->texcoords, vt, 1);

	} else if (g_str_has_prefix(line, "vn ")) { // normal

		vec3_t vn;

		if (sscanf(line + 3, "%f %f %f", &vn[0], &vn[2], &vn[1]) != 3) {
			Com_Error(ERROR_DROP, "Malformed normal for %s: %s\n", mod->media.name, line);
		}

		VectorNormalize(vn);

		obj->normals = g_array_append_vals(obj->normals, vn, 1);

	} else if (g_str_has_prefix(line, "f ")) { // face

		static GArray *verts = NULL;
		r_obj_group_t *current = &g_array_index(obj->groups, r_obj_group_t, obj->groups->len - 1);

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

			tri.verts[0] = g_array_index(verts, uint32_t, 0);
			tri.verts[1] = g_array_index(verts, uint32_t, i);
			tri.verts[2] = g_array_index(verts, uint32_t, i + 1);

			obj->tris = g_array_append_val(obj->tris, tri);
			current->num_tris++;
		}

		g_array_set_size(verts, 0);
	}

	// else we just ignore it
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
	Com_Debug(DEBUG_RENDERER, "%s: %u tris\n", mod->media.name, obj->tris->len);
}

/**
 * @brief Calculate tangent vectors for the given Object model.
 *
 * http://www.terathon.com/code/tangent.html
 */
static void R_LoadObjTangents(r_model_t *mod, r_obj_t *obj) {

	const size_t num_verts = obj->verts->len;

	vec3_t *sdirs = Mem_Malloc(num_verts * sizeof(vec3_t));
	vec3_t *tdirs = Mem_Malloc(num_verts * sizeof(vec3_t));

	for (size_t i = 0; i < obj->tris->len; i++) {
		const r_obj_triangle_t *tri = &g_array_index(obj->tris, r_obj_triangle_t, i);

		const r_obj_vertex_t *rv0 = &g_array_index(obj->verts, r_obj_vertex_t, tri->verts[0]);
		const r_obj_vertex_t *rv1 = &g_array_index(obj->verts, r_obj_vertex_t, tri->verts[1]);
		const r_obj_vertex_t *rv2 = &g_array_index(obj->verts, r_obj_vertex_t, tri->verts[2]);
		
		const vec_t *v0 = g_array_index(obj->points, vec3_t, rv0->indices[0] - 1);
		const vec_t *v1 = g_array_index(obj->points, vec3_t, rv1->indices[0] - 1);
		const vec_t *v2 = g_array_index(obj->points, vec3_t, rv2->indices[0] - 1);
		
		const vec_t *st0 = g_array_index(obj->texcoords, vec2_t, rv0->indices[1] - 1);
		const vec_t *st1 = g_array_index(obj->texcoords, vec2_t, rv1->indices[1] - 1);
		const vec_t *st2 = g_array_index(obj->texcoords, vec2_t, rv2->indices[1] - 1);

		// accumulate the tangent vectors

		const vec_t x1 = v1[0] - v0[0];
		const vec_t x2 = v2[0] - v0[0];
		const vec_t y1 = v1[1] - v0[1];
		const vec_t y2 = v2[1] - v0[1];
		const vec_t z1 = v1[2] - v0[2];
		const vec_t z2 = v2[2] - v0[2];

		const vec_t s1 = st1[0] - st0[0];
		const vec_t s2 = st2[0] - st0[0];
		const vec_t t1 = st1[1] - st0[1];
		const vec_t t2 = st2[1] - st0[1];

		const vec_t r = 1.0 / (s1 * t2 - s2 * t1);

		vec3_t sdir;
		VectorSet(sdir,
		          (t2 * x1 - t1 * x2),
		          (t2 * y1 - t1 * y2),
		          (t2 * z1 - t1 * z2)
		         );

		VectorScale(sdir, r, sdir);

		vec3_t tdir;
		VectorSet(tdir,
		          (s1 * x2 - s2 * x1),
		          (s1 * y2 - s2 * y1),
		          (s1 * z2 - s2 * z1)
		         );

		VectorScale(tdir, r, tdir);

		// the tangents are coindexed with the vertices

		const uint16_t i0 = rv0->indices[0] - 1;
		const uint16_t i1 = rv1->indices[0] - 1;
		const uint16_t i2 = rv2->indices[0] - 1;

		VectorAdd(sdirs[i0], sdir, sdirs[i0]);
		VectorAdd(sdirs[i1], sdir, sdirs[i1]);
		VectorAdd(sdirs[i2], sdir, sdirs[i2]);

		VectorAdd(tdirs[i0], tdir, tdirs[i0]);
		VectorAdd(tdirs[i1], tdir, tdirs[i1]);
		VectorAdd(tdirs[i2], tdir, tdirs[i2]);
	}

	for (size_t i = 0; i < obj->verts->len; i++) {
		static vec3_t bitangent;
		vec4_t tangent;
		r_obj_vertex_t *v = &g_array_index(obj->verts, r_obj_vertex_t, i);
		const vec_t *sdir = sdirs[v->indices[0] - 1];
		const vec_t *tdir = tdirs[v->indices[0] - 1];
		const vec_t *normal = g_array_index(obj->normals, vec3_t, v->indices[2] - 1);

		TangentVectors(normal, sdir, tdir, tangent, bitangent);

		obj->tangents = g_array_append_vals(obj->tangents, tangent, 1);
	}

	Mem_Free(sdirs);
	Mem_Free(tdirs);

	Com_Debug(DEBUG_RENDERER, "%s: %u tangents\n", mod->media.name, obj->tangents->len);
}

typedef struct {
	vec3_t vertex;
	vec3_t normal;
	u16vec2_t diffuse;
} r_obj_shell_interleave_vertex_t;

static r_buffer_layout_t r_obj_shell_buffer_layout[] = {
	{ .attribute = R_ARRAY_POSITION, .type = R_ATTRIB_FLOAT, .count = 3, .size = sizeof(vec3_t) },
	{ .attribute = R_ARRAY_NORMAL, .type = R_ATTRIB_FLOAT, .count = 3, .size = sizeof(vec3_t), .offset = 12 },
	{ .attribute = R_ARRAY_DIFFUSE, .type = R_ATTRIB_UNSIGNED_SHORT, .count = 2, .size = sizeof(u16vec2_t), .offset = 24, .normalized = true },
	{ .attribute = -1 }
};

/**
 * @brief Populates the shell array of `mod` with data from `obj`. This is only used
 * if r_shell is 2.
 */
static void R_LoadObjShellVertexArrays(r_model_t *mod, r_obj_t *obj, GLuint *elements, const GLsizei e) {

	if (r_shell->value < 2) {
		return;
	}

	GHashTable *index_remap_table = g_hash_table_new(g_direct_hash, g_direct_equal);
	GArray *unique_vertex_list = g_array_new(false, false, sizeof(r_obj_shell_interleave_vertex_t));

	// compile list of unique vertices in the model
	for (uint32_t vi = 0; vi < obj->verts->len; vi++) {
		const r_obj_vertex_t *ve = &g_array_index(obj->verts, r_obj_vertex_t, vi);
		const vec_t *point = g_array_index(obj->points, vec3_t, ve->indices[0] - 1);
		const vec_t *texcoord = g_array_index(obj->texcoords, vec2_t, ve->indices[1] - 1);
		const vec_t *normal = g_array_index(obj->normals, vec3_t, ve->indices[2] - 1);
		uint32_t i;

		for (i = 0; i < unique_vertex_list->len; i++) {
			r_obj_shell_interleave_vertex_t *v = &g_array_index(unique_vertex_list, r_obj_shell_interleave_vertex_t, i);

			if (VectorCompare(point, v->vertex)) {
				VectorAdd(v->normal, normal, v->normal);
				break;
			}
		}

		if (i == unique_vertex_list->len) {

			unique_vertex_list = g_array_append_vals(unique_vertex_list, &(const r_obj_shell_interleave_vertex_t) {
				.vertex = { point[0], point[1], point[2] },
				 .normal = { normal[0], normal[1], normal[2] },
				  .diffuse = { PackTexcoord(texcoord[0]), PackTexcoord(texcoord[1]) }
			}, 1);
		}

		if (vi != i) {

			g_hash_table_insert(index_remap_table, (gpointer) (ptrdiff_t) vi, (gpointer) (ptrdiff_t) i);
		}
	}

	for (uint32_t i = 0; i < unique_vertex_list->len; i++) {
		r_obj_shell_interleave_vertex_t *v = &g_array_index(unique_vertex_list, r_obj_shell_interleave_vertex_t, i);

		VectorNormalize(v->normal);
	}

	// upload data
	R_CreateInterleaveBuffer(&mod->mesh->shell_vertex_buffer, sizeof(r_obj_shell_interleave_vertex_t),
	                         r_obj_shell_buffer_layout, GL_STATIC_DRAW, sizeof(r_obj_shell_interleave_vertex_t) * unique_vertex_list->len,
	                         unique_vertex_list->data);

	g_array_free(unique_vertex_list, true);

	// remap indices
	for (GLsizei i = 0; i < mod->num_elements; ++i) {
		GLuint *element = &elements[i];
		gpointer new_element;

		if (g_hash_table_lookup_extended(index_remap_table, (gconstpointer) (ptrdiff_t) *element, NULL, &new_element)) {

			*element = (GLuint) (ptrdiff_t) new_element;
		}
	}

	R_CreateElementBuffer(&mod->mesh->shell_element_buffer, R_ATTRIB_UNSIGNED_INT, GL_STATIC_DRAW, e, elements);

	g_hash_table_destroy(index_remap_table);
}

typedef struct {
	vec3_t vertex;
	vec3_t normal;
	vec4_t tangent;
	u16vec2_t diffuse;
} r_obj_interleave_vertex_t;

static r_buffer_layout_t r_obj_buffer_layout[] = {
	{ .attribute = R_ARRAY_POSITION, .type = R_ATTRIB_FLOAT, .count = 3, .size = sizeof(vec3_t) },
	{ .attribute = R_ARRAY_NORMAL, .type = R_ATTRIB_FLOAT, .count = 3, .size = sizeof(vec3_t), .offset = 12 },
	{ .attribute = R_ARRAY_TANGENT, .type = R_ATTRIB_FLOAT, .count = 4, .size = sizeof(vec4_t), .offset = 24 },
	{ .attribute = R_ARRAY_DIFFUSE, .type = R_ATTRIB_UNSIGNED_SHORT, .count = 2, .size = sizeof(u16vec2_t), .offset = 40, .normalized = true },
	{ .attribute = -1 }
};

/**
 * @brief Populates the vertex arrays of `mod` with triangle data from `obj`.
 */
static void R_LoadObjVertexArrays(r_model_t *mod, r_obj_t *obj) {

	mod->num_verts = obj->verts->len;
	mod->num_tris = obj->tris->len;
	mod->num_elements = mod->num_tris * 3;

	const GLsizei v = mod->num_verts * sizeof(r_obj_interleave_vertex_t);
	const GLsizei e = mod->num_elements * sizeof(GLuint);

	r_obj_interleave_vertex_t *verts = Mem_LinkMalloc(v, mod);
	GLuint *elements = Mem_LinkMalloc(e, mod);

	r_obj_interleave_vertex_t *vout = verts;
	GLuint *eout = elements;
	
	for (uint32_t i = 0; i < obj->verts->len; i++) {
		const r_obj_vertex_t *ve = &g_array_index(obj->verts, r_obj_vertex_t, i);

		const vec_t *point = g_array_index(obj->points, vec3_t, ve->indices[0] - 1);
		const vec_t *texcoord = g_array_index(obj->texcoords, vec2_t, ve->indices[1] - 1);
		const vec_t *normal = g_array_index(obj->normals, vec3_t, ve->indices[2] - 1);
		const vec_t *tangent = g_array_index(obj->tangents, vec4_t, ve->indices[0] - 1);

		VectorCopy(point, vout->vertex);

		PackTexcoords(texcoord, vout->diffuse);

		VectorCopy(normal, vout->normal);

		Vector4Copy(tangent, vout->tangent);

		vout++;
	}
	
	for (uint32_t i = 0; i < obj->tris->len; i++) {
		const r_obj_triangle_t *te = &g_array_index(obj->tris, r_obj_triangle_t, i);

		for (int32_t i = 0; i < 3; ++i) {
			const r_obj_vertex_t *ve = &g_array_index(obj->verts, r_obj_vertex_t, te->verts[i]);

			*eout++ = ve->position;
		}
	}

	// load the vertex buffer objects
	R_CreateInterleaveBuffer(&mod->mesh->vertex_buffer, sizeof(*verts), r_obj_buffer_layout, GL_STATIC_DRAW, v, verts);

	R_CreateElementBuffer(&mod->mesh->element_buffer, R_ATTRIB_UNSIGNED_INT, GL_STATIC_DRAW, e, elements);

	R_LoadObjShellVertexArrays(mod, obj, elements, e);

	// free our temporary buffers
	Mem_Free(verts);
	Mem_Free(elements);

	R_GetError(mod->media.name);
}

/**
 * @brief
 */
void R_LoadObjModel(r_model_t *mod, void *buffer) {
	
	mod->mesh = Mem_LinkMalloc(sizeof(r_mesh_model_t), mod);

	r_obj_t *obj = Mem_LinkMalloc(sizeof(r_obj_t), mod->mesh);

	mod->mesh->num_frames = 1;

	ClearBounds(mod->mins, mod->maxs);
	
	obj->verts = g_array_new(false, false, sizeof(r_obj_vertex_t));
	obj->points = g_array_new(false, false, sizeof(vec3_t));
	obj->texcoords = g_array_new(false, false, sizeof(vec2_t));
	obj->normals = g_array_new(false, false, sizeof(vec3_t));
	obj->tris = g_array_new(false, false, sizeof(r_obj_triangle_t));
	obj->tangents = g_array_new(false, false, sizeof(vec4_t));
	obj->groups = g_array_sized_new(false, false, sizeof(r_obj_group_t), 1);

	obj->groups = g_array_append_vals(obj->groups, &(const r_obj_group_t) {
		.name = "",
		.num_tris = 0
	}, 1);

	// parse the file, loading primitives
	R_LoadObjPrimitives(mod, obj, buffer);

	if (obj->tris->len == 0) {
		Com_Error(ERROR_DROP, "Failed to load .obj: %s\n", mod->media.name);
	}

	// set up groups
	R_LoadObjGroups(mod, obj);

	// calculate tangents
	R_LoadObjTangents(mod, obj);

	// load the material
	R_LoadModelMaterials(mod);

	// and configs
	R_LoadMeshConfigs(mod);

	// and finally the arrays
	R_LoadObjVertexArrays(mod, obj);
	
	g_array_free(obj->verts, true);
	g_array_free(obj->points, true);
	g_array_free(obj->texcoords, true);
	g_array_free(obj->normals, true);
	g_array_free(obj->tris, true);
	g_array_free(obj->tangents, true);
	g_array_free(obj->groups, true);

	Mem_Free(obj);
}

