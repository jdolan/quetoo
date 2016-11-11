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
 * @brief Resolves the skin for the specified model. By default, we simply load
 * "skin.tga" in the model's directory.
 */
static void R_LoadMeshMaterial(r_model_t *mod) {
	char skin[MAX_QPATH];

	Dirname(mod->media.name, skin);
	strcat(skin, "skin");

	mod->mesh->material = R_LoadMaterial(skin);
}

/**
 * @brief Parses animation.cfg, loading the frame specifications for the given model.
 */
static void R_LoadMd3Animations(r_model_t *mod) {
	r_md3_t *md3;
	char path[MAX_QPATH];
	const char *buffer, *c;
	void *buf;
	uint16_t skip;

	md3 = (r_md3_t *) mod->mesh->data;

	Dirname(mod->media.name, path);
	strcat(path, "animation.cfg");

	if (Fs_Load(path, &buf) == -1) {
		Com_Warn("No animation.cfg for %s\n", mod->media.name);
		return;
	}

	md3->animations = Mem_LinkMalloc(sizeof(r_md3_animation_t) * MD3_MAX_ANIMATIONS, mod->mesh);

	buffer = (char *) buf;
	skip = 0;

	while (true) {

		c = ParseToken(&buffer);

		if (*c == '\0') {
			break;
		}

		if (!g_strcmp0(c, "footsteps")) {
			ParseToken(&buffer);
			continue;
		}

		if (!g_strcmp0(c, "headoffset")) {
			ParseToken(&buffer);
			ParseToken(&buffer);
			ParseToken(&buffer);
			continue;
		}

		if (!g_strcmp0(c, "sex")) {
			ParseToken(&buffer);
			continue;
		}

		if (*c >= '0' && *c <= '9') {
			r_md3_animation_t *a = &md3->animations[md3->num_animations];

			a->first_frame = (uint16_t) strtoul(c, NULL, 0);
			c = ParseToken(&buffer);
			a->num_frames = (uint16_t) strtoul(c, NULL, 0);
			c = ParseToken(&buffer);
			a->looped_frames = (uint16_t) strtoul(c, NULL, 0);
			c = ParseToken(&buffer);
			a->hz = (uint16_t) strtoul(c, NULL, 0);

			if (md3->num_animations == ANIM_LEGS_WALKCR) {
				skip = a->first_frame - md3->animations[ANIM_TORSO_GESTURE].first_frame;
			}

			if (md3->num_animations >= ANIM_LEGS_WALKCR) {
				a->first_frame -= skip;
			}

			if (!a->num_frames) {
				Com_Warn("%s: No frames for %d\n", mod->media.name, md3->num_animations);
			}

			if (!a->hz) {
				Com_Warn("%s: No hz for %d\n", mod->media.name, md3->num_animations);
			}

			Com_Debug("Parsed %d: %d %d %d %d\n", md3->num_animations,
			          a->first_frame, a->num_frames, a->looped_frames, a->hz);

			md3->num_animations++;
			if (md3->num_animations == MD3_MAX_ANIMATIONS) {
				Com_Warn("MD3_MAX_ANIMATIONS reached: %s\n", mod->media.name);
				break;
			}
		}
	}

	Fs_Free(buf);

	Com_Debug("Loaded %d animations: %s\n", md3->num_animations, mod->media.name);
}

/**
 * @brief Loads the specified r_mesh_config_t from the file at path.
 */
static void R_LoadMeshConfig(r_mesh_config_t *config, const char *path) {
	const char *buffer, *c;
	void *buf;

	if (Fs_Load(path, &buf) == -1) {
		return;
	}

	buffer = (char *) buf;

	while (true) {

		c = ParseToken(&buffer);

		if (*c == '\0') {
			break;
		}

		if (!g_strcmp0(c, "translate")) {
			sscanf(ParseToken(&buffer), "%f %f %f", &config->translate[0], &config->translate[1],
			       &config->translate[2]);
			continue;
		}

		if (!g_strcmp0(c, "scale")) {
			sscanf(ParseToken(&buffer), "%f", &config->scale);
			continue;
		}

		if (!g_strcmp0(c, "alpha_test")) {
			config->flags |= EF_ALPHATEST;
			continue;
		}

		if (!g_strcmp0(c, "blend")) {
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

	mod->mesh->world_config = Mem_LinkMalloc(sizeof(r_mesh_config_t), mod->mesh);
	mod->mesh->view_config = Mem_LinkMalloc(sizeof(r_mesh_config_t), mod->mesh);
	mod->mesh->link_config = Mem_LinkMalloc(sizeof(r_mesh_config_t), mod->mesh);

	mod->mesh->world_config->scale = 1.0;

	Dirname(mod->media.name, path);

	R_LoadMeshConfig(mod->mesh->world_config, va("%sworld.cfg", path));

	// by default, additional configs inherit from world
	memcpy(mod->mesh->view_config, mod->mesh->world_config, sizeof(r_mesh_config_t));
	memcpy(mod->mesh->link_config, mod->mesh->world_config, sizeof(r_mesh_config_t));

	R_LoadMeshConfig(mod->mesh->view_config, va("%sview.cfg", path));
	R_LoadMeshConfig(mod->mesh->link_config, va("%slink.cfg", path));
}

/**
 * @brief Calculates tangent vectors for each MD3 vertex for per-pixel
 * lighting. See http://www.terathon.com/code/tangent.html.
 */
static void R_LoadMd3Tangents(r_md3_mesh_t *mesh) {
	vec3_t *tan1, *tan2;
	uint32_t *tri;
	int32_t i;

	tan1 = (vec3_t *) Mem_Malloc(mesh->num_verts * sizeof(vec3_t));
	tan2 = (vec3_t *) Mem_Malloc(mesh->num_verts * sizeof(vec3_t));

	tri = mesh->tris;

	// resolve the texture directional vectors

	for (i = 0; i < mesh->num_tris; i++, tri += 3) {
		vec3_t sdir, tdir;

		const uint32_t i1 = tri[0];
		const uint32_t i2 = tri[1];
		const uint32_t i3 = tri[2];

		const vec_t *v1 = mesh->verts[i1].point;
		const vec_t *v2 = mesh->verts[i2].point;
		const vec_t *v3 = mesh->verts[i3].point;

		const vec_t *w1 = mesh->coords[i1].st;
		const vec_t *w2 = mesh->coords[i2].st;
		const vec_t *w3 = mesh->coords[i3].st;

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

	for (i = 0; i < mesh->num_verts; i++) {
		vec3_t bitangent;

		const vec_t *normal = mesh->verts[i].normal;
		vec_t *tangent = mesh->verts[i].tangent;

		TangentVectors(normal, tan1[i], tan2[i], tangent, bitangent);
	}

	Mem_Free(tan1);
	Mem_Free(tan2);
}

typedef struct {
	vec3_t vertex;
	int32_t normal;
	int32_t tangent;
} r_md3_interleave_vertex_t;

r_buffer_layout_t r_md3_buffer_layout[] = {
	{ .attribute = R_ARRAY_POSITION, .type = GL_FLOAT, .count = 3, .size = sizeof(vec3_t), .offset = 0 },
	{ .attribute = R_ARRAY_NORMAL, .type = GL_INT_2_10_10_10_REV, .count = 4, .size = sizeof(int32_t), .offset = 12 },
	{ .attribute = R_ARRAY_TANGENT, .type = GL_INT_2_10_10_10_REV, .count = 4, .size = sizeof(int32_t), .offset = 16 },
	{ .attribute = -1 }
};

/**
 * @brief Loads and populates vertex array data for the specified MD3 model.
 *
 * @remarks Static MD3 meshes receive vertex, normal and tangent arrays in
 * addition to texture coordinate arrays. Animated models receive only texture
 * coordinates, because they must be interpolated at each frame.
 */
static void R_LoadMd3VertexArrays(r_model_t *mod) {

	const r_md3_t *md3 = (r_md3_t *) mod->mesh->data;
	const r_md3_mesh_t *mesh = md3->meshes;

	mod->num_verts = 0;

	for (uint16_t i = 0; i < md3->num_meshes; i++, mesh++) {
		mod->num_verts += mesh->num_verts;
		mod->num_elements += mesh->num_tris * 3;
	}

	// make the scratch space
	const GLsizei v = mod->num_verts * sizeof(r_md3_interleave_vertex_t);
	const GLsizei st = mod->num_verts * sizeof(vec2_t);
	const GLsizei e = mod->num_elements * sizeof(GLuint);

	vec_t *texcoords = Mem_LinkMalloc(st, mod);
	GLuint *tris = Mem_LinkMalloc(e, mod);

	const d_md3_frame_t *frame = md3->frames;
	GLuint *out_tri = tris;
	vec_t *out_texcoord = texcoords;

	r_md3_interleave_vertex_t *vertexes = Mem_LinkMalloc(v, mod);

	// upload initial data
	R_CreateInterleaveBuffer(&mod->vertex_buffer, sizeof(r_md3_interleave_vertex_t), r_md3_buffer_layout, GL_STATIC_DRAW,
	                         v * md3->num_frames, NULL);

	for (uint16_t f = 0; f < md3->num_frames; ++f, frame++) {

		mesh = md3->meshes;
		GLuint vert_offset = 0;

		for (uint16_t i = 0; i < md3->num_meshes; i++, mesh++) { // iterate the meshes

			const r_md3_vertex_t *v = mesh->verts + f * mesh->num_verts;
			const d_md3_texcoord_t *t = mesh->coords;

			for (uint16_t j = 0; j < mesh->num_verts; j++, v++, ++t) {
				VectorAdd(frame->translate, v->point, vertexes[j + vert_offset].vertex);
				NormalToGLNormal(v->normal, &vertexes[j + vert_offset].normal);
				TangentToGLTangent(v->tangent, &vertexes[j + vert_offset].tangent);

				// only copy st coords once
				if (f == 0) {
					Vector2Copy(t->st, out_texcoord);
					out_texcoord += 2;
				}
			}

			// only copy elements once
			if (f == 0) {
				const uint32_t *tri = mesh->tris;

				for (uint16_t j = 0; j < mesh->num_tris; ++j) {
					for (uint16_t k = 0; k < 3; ++k) {
						*out_tri++ = *tri++ + vert_offset;
					}
				}
			}

			vert_offset += mesh->num_verts;
		}

		// upload each frame
		R_UploadToSubBuffer(&mod->vertex_buffer, v * f, v, vertexes, false);
	}

	mod->normal_buffer = mod->vertex_buffer;
	mod->tangent_buffer = mod->vertex_buffer;

	// upload texcoords
	R_CreateDataBuffer(&mod->texcoord_buffer, GL_FLOAT, 2, GL_STATIC_DRAW, st, texcoords);

	// upload elements
	R_CreateElementBuffer(&mod->element_buffer, GL_UNSIGNED_INT, GL_STATIC_DRAW, e, tris);

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
	r_md3_tag_t *out_tag;
	d_md3_orientation_t orient;
	d_md3_mesh_t *in_mesh;
	r_md3_mesh_t *out_mesh;
	d_md3_texcoord_t *in_coord, *out_coord;
	d_md3_vertex_t *in_vert;
	r_md3_vertex_t *out_vert;
	uint32_t *inindex, *out_index;
	vec_t lat, lng;
	size_t size;

	in_md3 = (d_md3_t *) buffer;

	const int32_t version = LittleLong(in_md3->version);
	if (version != MD3_VERSION) {
		Com_Error(ERR_DROP, "%s has wrong version number "
		          "(%i should be %i)\n", mod->media.name, version, MD3_VERSION);
	}

	mod->mesh = Mem_LinkMalloc(sizeof(r_mesh_model_t), mod);
	mod->mesh->data = out_md3 = Mem_LinkMalloc(sizeof(r_md3_t), mod->mesh);

	// byte swap the header fields and sanity check
	in_md3->ofs_frames = LittleLong(in_md3->ofs_frames);
	in_md3->ofs_tags = LittleLong(in_md3->ofs_tags);
	in_md3->ofs_meshes = LittleLong(in_md3->ofs_meshes);

	mod->mesh->num_frames = out_md3->num_frames = LittleLong(in_md3->num_frames);
	out_md3->num_tags = LittleLong(in_md3->num_tags);
	out_md3->num_meshes = LittleLong(in_md3->num_meshes);

	if (out_md3->num_frames < 1) {
		Com_Error(ERR_DROP, "%s has no frames\n", mod->media.name);
	}

	if (out_md3->num_frames > MD3_MAX_FRAMES) {
		Com_Error(ERR_DROP, "%s has too many frames\n", mod->media.name);
	}

	if (out_md3->num_tags > MD3_MAX_TAGS) {
		Com_Error(ERR_DROP, "%s has too many tags\n", mod->media.name);
	}

	if (out_md3->num_meshes > MD3_MAX_MESHES) {
		Com_Error(ERR_DROP, "%s has too many meshes\n", mod->media.name);
	}

	// load the frames
	in_frame = (d_md3_frame_t *) ((byte *) in_md3 + in_md3->ofs_frames);
	size = out_md3->num_frames * sizeof(d_md3_frame_t);
	out_md3->frames = out_frame = Mem_LinkMalloc(size, mod->mesh);

	ClearBounds(mod->mins, mod->maxs);

	for (i = 0; i < out_md3->num_frames; i++, in_frame++, out_frame++) {
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
	if (out_md3->num_tags) {

		in_tag = (d_md3_tag_t *) ((byte *) in_md3 + in_md3->ofs_tags);
		size = out_md3->num_tags * out_md3->num_frames * sizeof(r_md3_tag_t);
		out_md3->tags = out_tag = Mem_LinkMalloc(size, mod->mesh);

		for (i = 0; i < out_md3->num_frames; i++) {
			for (l = 0; l < out_md3->num_tags; l++, in_tag++, out_tag++) {
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
	size = out_md3->num_meshes * sizeof(r_md3_mesh_t);
	out_md3->meshes = out_mesh = Mem_LinkMalloc(size, mod->mesh);

	for (i = 0; i < out_md3->num_meshes; i++, out_mesh++) {
		memcpy(out_mesh->name, in_mesh->name, MD3_MAX_PATH);

		in_mesh->ofs_tris = LittleLong(in_mesh->ofs_tris);
		in_mesh->ofs_skins = LittleLong(in_mesh->ofs_skins);
		in_mesh->ofs_tcs = LittleLong(in_mesh->ofs_tcs);
		in_mesh->ofs_verts = LittleLong(in_mesh->ofs_verts);
		in_mesh->size = LittleLong(in_mesh->size);

		out_mesh->flags = LittleLong(in_mesh->flags);
		out_mesh->num_skins = LittleLong(in_mesh->num_skins);
		out_mesh->num_tris = LittleLong(in_mesh->num_tris);
		out_mesh->num_verts = LittleLong(in_mesh->num_verts);

		if (out_mesh->num_skins > MD3_MAX_SHADERS) {
			Com_Error(ERR_DROP, "%s: %s has too many skins\n", mod->media.name, out_mesh->name);
		}

		if (out_mesh->num_tris > MD3_MAX_TRIANGLES) {
			Com_Error(ERR_DROP, "%s: %s has too many triangles\n", mod->media.name, out_mesh->name);
		}

		if (out_mesh->num_verts > MD3_MAX_VERTS) {
			Com_Error(ERR_DROP, "%s: %s has too many vertexes\n", mod->media.name, out_mesh->name);
		}

		// load the triangle indexes
		inindex = (uint32_t *) ((byte *) in_mesh + in_mesh->ofs_tris);
		size = out_mesh->num_tris * sizeof(uint32_t) * 3;
		out_mesh->tris = out_index = Mem_LinkMalloc(size, mod->mesh);

		for (j = 0; j < out_mesh->num_tris; j++, inindex += 3, out_index += 3) {
			out_index[0] = (uint32_t) LittleLong(inindex[0]);
			out_index[1] = (uint32_t) LittleLong(inindex[1]);
			out_index[2] = (uint32_t) LittleLong(inindex[2]);
		}

		// load the texcoords
		in_coord = (d_md3_texcoord_t *) ((byte *) in_mesh + in_mesh->ofs_tcs);
		size = out_mesh->num_verts * sizeof(d_md3_texcoord_t);
		out_mesh->coords = out_coord = Mem_LinkMalloc(size, mod->mesh);

		for (j = 0; j < out_mesh->num_verts; j++, in_coord++, out_coord++) {
			out_coord->st[0] = LittleFloat(in_coord->st[0]);
			out_coord->st[1] = LittleFloat(in_coord->st[1]);
		}

		// load the verts and norms
		in_vert = (d_md3_vertex_t *) ((byte *) in_mesh + in_mesh->ofs_verts);
		size = out_md3->num_frames * out_mesh->num_verts * sizeof(r_md3_vertex_t);
		out_mesh->verts = out_vert = Mem_LinkMalloc(size, mod->mesh);

		for (l = 0; l < out_md3->num_frames; l++) {
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

		R_LoadMd3Tangents(out_mesh);

		mod->num_tris += out_mesh->num_tris;
		out_mesh->num_elements = out_mesh->num_tris * 3;

		Com_Debug("%s: %s: %d triangles (%d elements)\n", mod->media.name, out_mesh->name, out_mesh->num_tris,
		          out_mesh->num_elements);

		in_mesh = (d_md3_mesh_t *) ((byte *) in_mesh + in_mesh->size);
	}

	// load the skin for objects, and the animations for players
	if (!strstr(mod->media.name, "players/")) {
		R_LoadMeshMaterial(mod);
	}

	else if (strstr(mod->media.name, "/upper")) {
		R_LoadMd3Animations(mod);
	}

	// and the configs
	R_LoadMeshConfigs(mod);

	// and finally load the arrays
	R_LoadMd3VertexArrays(mod);

	Com_Debug("%s\n  %d meshes\n  %d frames\n  %d tags\n  %d vertexes\n", mod->media.name,
	          out_md3->num_meshes, out_md3->num_frames, out_md3->num_tags, mod->num_verts);
}

/**
 * @brief Resolves the the vertex described by `indices`, allocating a new
 * vertex if `indices` is unique to the model.
 *
 * @remarks In Object file format, primitives are indexed starting at 1, not 0.
 */
static r_obj_vertex_t *R_ObjVertexForIndices(r_model_t *mod, r_obj_t *obj, const uint16_t *indices) {

	GList *vert = obj->verts;
	while (vert) {
		r_obj_vertex_t *v = vert->data;

		if (memcmp(v->indices, indices, sizeof(v->indices)) == 0) {
			return v;
		}

		vert = vert->next;
	}

	r_obj_vertex_t *v = g_new(r_obj_vertex_t, 1);
	memcpy(v->indices, indices, sizeof(v->indices));

	v->point = g_list_nth_data(obj->points, indices[0] - 1);
	v->texcoords = g_list_nth_data(obj->texcoords, indices[1] - 1);
	v->normal = g_list_nth_data(obj->normals, indices[2] - 1);

	if (!v->point || !v->texcoords || !v->normal) {
		Com_Error(ERR_DROP, "Invalid face indices for %s: %hu/%hu/%hu\n", mod->media.name,
		          indices[0], indices[1], indices[2]);
	}

	v->position = g_list_length(obj->verts);
	obj->verts = g_list_append(obj->verts, v);
	return v;
}

/**
 * @brief Parses a line of an Object file, allocating primitives accordingly.
 */
static void R_LoadObjPrimitive(r_model_t *mod, r_obj_t *obj, const char *line) {

	if (g_str_has_prefix(line, "v ")) { // vertex

		vec_t *v = g_new(vec_t, 3);

		if (sscanf(line + 2, "%f %f %f", &v[0], &v[2], &v[1]) != 3) {
			Com_Error(ERR_DROP, "Malformed vertex for %s: %s\n", mod->media.name, line);
		}

		AddPointToBounds(v, mod->mins, mod->maxs);

		obj->points = g_list_append(obj->points, v);

	} else if (g_str_has_prefix(line, "vt ")) { // texcoord

		vec_t *vt = g_new(vec_t, 2);

		if (sscanf(line + 3, "%f %f", &vt[0], &vt[1]) != 2) {
			Com_Error(ERR_DROP, "Malformed texcoord for %s: %s\n", mod->media.name, line);
		}

		vt[1] = -vt[1];

		obj->texcoords = g_list_append(obj->texcoords, vt);

	} else if (g_str_has_prefix(line, "vn ")) { // normal

		vec_t *vn = g_new(vec_t, 3);

		if (sscanf(line + 3, "%f %f %f", &vn[0], &vn[2], &vn[1]) != 3) {
			Com_Error(ERR_DROP, "Malformed normal for %s: %s\n", mod->media.name, line);
		}

		VectorNormalize(vn);

		obj->normals = g_list_append(obj->normals, vn);

	} else if (g_str_has_prefix(line, "f ")) { // face

		GList *verts = NULL;

		const char *c = line + 2;
		while (*c) {
			uint16_t indices[3];
			int32_t n;

			if (sscanf(c, "%hu/%hu/%hu%n", &indices[0], &indices[1], &indices[2], &n) != 3) {
				Com_Error(ERR_DROP, "Malformed face for %s: %s\n", mod->media.name, line);
			}

			verts = g_list_append(verts, R_ObjVertexForIndices(mod, obj, indices));
			c += n;
		}

		// iterate the face, converting polygons into triangles

		const size_t len = g_list_length(verts);

		if (len < 3) {
			Com_Error(ERR_DROP, "Malformed face for %s: %s\n", mod->media.name, line);
		}

		for (size_t i = 1; i < len - 1; i++) {

			r_obj_triangle_t *tri = g_new(r_obj_triangle_t, 1);

			tri->verts[0] = g_list_nth_data(verts, 0);
			tri->verts[1] = g_list_nth_data(verts, (guint) i);
			tri->verts[2] = g_list_nth_data(verts, (guint) (i + 1));

			obj->tris = g_list_append(obj->tris, tri);
		}

		g_list_free(verts);
	}

	// else we just ignore it
}

/**
 * @brief Parses the Object primitives from the ASCII text in `buffer`.
 */
static void R_LoadObjPrimitives(r_model_t *mod, r_obj_t *obj, const void *buffer) {
	char line[MAX_STRING_CHARS];

	memset(line, 0, sizeof(line));
	char *l = line;

	const char *c = buffer;

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
	Com_Debug("%s: %u tris\n", mod->media.name, g_list_length(obj->tris));
}

/**
 * @brief Calculate tangent vectors for the given Object model.
 *
 * http://www.terathon.com/code/tangent.html
 */
static void R_LoadObjTangents(r_model_t *mod, r_obj_t *obj) {

	const size_t num_verts = g_list_length(obj->verts);

	vec3_t *sdirs = Mem_Malloc(num_verts * sizeof(vec3_t));
	vec3_t *tdirs = Mem_Malloc(num_verts * sizeof(vec3_t));

	const GList *tris = obj->tris;
	while (tris) {

		const r_obj_triangle_t *tri = tris->data;

		const vec_t *v0 = tri->verts[0]->point;
		const vec_t *v1 = tri->verts[1]->point;
		const vec_t *v2 = tri->verts[2]->point;

		const vec_t *st0 = tri->verts[0]->texcoords;
		const vec_t *st1 = tri->verts[1]->texcoords;
		const vec_t *st2 = tri->verts[2]->texcoords;

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

		const uint16_t i0 = tri->verts[0]->indices[0] - 1;
		const uint16_t i1 = tri->verts[1]->indices[0] - 1;
		const uint16_t i2 = tri->verts[2]->indices[0] - 1;

		VectorAdd(sdirs[i0], sdir, sdirs[i0]);
		VectorAdd(sdirs[i1], sdir, sdirs[i1]);
		VectorAdd(sdirs[i2], sdir, sdirs[i2]);

		VectorAdd(tdirs[i0], tdir, tdirs[i0]);
		VectorAdd(tdirs[i1], tdir, tdirs[i1]);
		VectorAdd(tdirs[i2], tdir, tdirs[i2]);

		tris = tris->next;
	}

	GList *vert = obj->verts;
	while (vert) {
		r_obj_vertex_t *v = vert->data;

		vec_t *tangent = v->tangent = g_new(vec_t, 4);
		vec3_t bitangent;

		const vec_t *sdir = sdirs[v->indices[0] - 1];
		const vec_t *tdir = tdirs[v->indices[0] - 1];

		TangentVectors(v->normal, sdir, tdir, tangent, bitangent);

		obj->tangents = g_list_append(obj->tangents, tangent);
		vert = vert->next;
	}

	Mem_Free(sdirs);
	Mem_Free(tdirs);

	Com_Debug("%s: %u tangents\n", mod->media.name, g_list_length(obj->tangents));
}

typedef struct {
	vec3_t vertex;
	int32_t normal;
	int32_t tangent;
	vec2_t diffuse;
} r_obj_interleave_vertex_t;

r_buffer_layout_t r_obj_buffer_layout[] = {
	{ .attribute = R_ARRAY_POSITION, .type = GL_FLOAT, .count = 3, .size = sizeof(vec3_t), .offset = 0 },
	{ .attribute = R_ARRAY_NORMAL, .type = GL_INT_2_10_10_10_REV, .count = 4, .size = sizeof(int32_t), .offset = 12 },
	{ .attribute = R_ARRAY_TANGENT, .type = GL_INT_2_10_10_10_REV, .count = 4, .size = sizeof(int32_t), .offset = 16 },
	{ .attribute = R_ARRAY_TEX_DIFFUSE, .type = GL_FLOAT, .count = 2, .size = sizeof(vec2_t), .offset = 20 },
	{ .attribute = -1 }
};

/**
 * @brief Populates the vertex arrays of `mod` with triangle data from `obj`.
 */
static void R_LoadObjVertexArrays(r_model_t *mod, r_obj_t *obj) {

	mod->num_verts = g_list_length(obj->verts);
	mod->num_tris = g_list_length(obj->tris);
	mod->num_elements = mod->num_tris * 3;

	const GLsizei v = mod->num_verts * sizeof(r_obj_interleave_vertex_t);
	const GLsizei e = mod->num_elements * sizeof(GLuint);

	r_obj_interleave_vertex_t *verts = Mem_LinkMalloc(v, mod);
	GLuint *elements = Mem_LinkMalloc(e, mod);

	r_obj_interleave_vertex_t *vout = verts;
	GLuint *eout = elements;

	const GList *vl = obj->verts;
	while (vl) {

		const r_obj_vertex_t *ve = vl->data;

		VectorCopy(ve->point, vout->vertex);

		Vector2Copy(ve->texcoords, vout->diffuse);

		NormalToGLNormal(ve->normal, &vout->normal);

		TangentToGLTangent(ve->tangent, &vout->tangent);

		vout++;

		vl = vl->next;
	}

	const GList *el = obj->tris;
	while (el) {

		const r_obj_triangle_t *te = el->data;

		for (int32_t i = 0; i < 3; ++i) {
			*eout++ = te->verts[i]->position;
		}

		el = el->next;
	}

	// load the vertex buffer objects
	R_CreateInterleaveBuffer(&mod->vertex_buffer, sizeof(*verts), r_obj_buffer_layout, GL_STATIC_DRAW, v, verts);

	mod->normal_buffer = mod->vertex_buffer;
	mod->tangent_buffer = mod->vertex_buffer;
	mod->texcoord_buffer = mod->vertex_buffer;

	R_CreateElementBuffer(&mod->element_buffer, GL_UNSIGNED_INT, GL_STATIC_DRAW, e, elements);

	// free our temporary buffers
	Mem_Free(verts);
	Mem_Free(elements);

	R_GetError(mod->media.name);
}

/**
 * @brief
 */
void R_LoadObjModel(r_model_t *mod, void *buffer) {
	r_obj_t *obj;

	mod->mesh = Mem_LinkMalloc(sizeof(r_mesh_model_t), mod);
	mod->mesh->data = obj = Mem_LinkMalloc(sizeof(r_obj_t), mod->mesh);

	mod->mesh->num_frames = 1;

	ClearBounds(mod->mins, mod->maxs);

	// parse the file, loading primitives
	R_LoadObjPrimitives(mod, obj, buffer);

	if (g_list_length(obj->tris) == 0) {
		Com_Error(ERR_DROP, "Failed to load .obj: %s\n", mod->media.name);
	}

	// calculate tangents
	R_LoadObjTangents(mod, obj);

	// load the material
	R_LoadMeshMaterial(mod);

	// and configs
	R_LoadMeshConfigs(mod);

	// and finally the arrays
	R_LoadObjVertexArrays(mod, obj);

	g_list_free_full(obj->points, g_free);
	obj->points = NULL;

	g_list_free_full(obj->texcoords, g_free);
	obj->texcoords = NULL;

	g_list_free_full(obj->normals, g_free);
	obj->normals = NULL;

	g_list_free_full(obj->tris, g_free);
	obj->tris = NULL;

	g_list_free_full(obj->tangents, g_free);
	obj->tangents = NULL;
}

