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
 * @brief Parses animation.cfg, loading the frame specifications for the given model.
 */
static void R_LoadMd3Animations(r_model_t *mod) {
	char path[MAX_QPATH];
	void *buf;
	int32_t skip = 0;
	char token[MAX_TOKEN_CHARS];
	parser_t parser;

	Dirname(mod->media.name, path);
	strcat(path, "animation.cfg");

	if (Fs_Load(path, &buf) == -1) {
		Com_Warn("No animation.cfg for %s\n", mod->media.name);
		return;
	}

	mod->mesh->animations = Mem_LinkMalloc(sizeof(r_mesh_animation_t) * MD3_MAX_ANIMATIONS, mod->mesh);

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
			r_mesh_animation_t *a = &mod->mesh->animations[mod->mesh->num_animations];

			if (!Parse_Primitive(&parser, PARSE_DEFAULT, PARSE_INT32, &a->first_frame, 1)) {
				break;
			}

			if (!Parse_Primitive(&parser, PARSE_DEFAULT | PARSE_NO_WRAP, PARSE_INT32, &a->num_frames, 1)) {
				break;
			}

			if (!Parse_Primitive(&parser, PARSE_DEFAULT | PARSE_NO_WRAP, PARSE_INT32, &a->looped_frames, 1)) {
				break;
			}

			if (!Parse_Primitive(&parser, PARSE_DEFAULT | PARSE_NO_WRAP, PARSE_INT32, &a->hz, 1)) {
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
 * @brief Swap function.
 */
static d_md3_texcoord_t R_SwapMd3Texcoord(const d_md3_texcoord_t *in) {

	d_md3_texcoord_t out = *in;

#if SDL_BYTEORDER != SDL_LIL_ENDIAN
	out.st = LittleVec2(out.st);
#endif

	return out;
}

/**
 * @brief Swap function/
 */
static d_md3_vertex_t R_SwapMd3Vertex(const d_md3_vertex_t *in) {

	d_md3_vertex_t out = *in;

#if SDL_BYTEORDER != SDL_LIL_ENDIAN
	out.point = LittleVec3s(out.point);
	out.norm = LittleShort(out.norm);
#endif

	return out;
}

/**
 * @brief Swap function/
 */
static d_md3_triangle_t R_SwapMd3Triangle(const d_md3_triangle_t *in) {

	d_md3_triangle_t out = *in;

#if SDL_BYTEORDER != SDL_LIL_ENDIAN
	for (int32_t i = 0; i < 3; i++) {
		out.indexes[i] = LittleLong(out.indexes[i]);
	}
#endif

	return out;
}

/**
 * @brief Swap function.
 */
static d_md3_frame_t R_SwapMd3Frame(const d_md3_frame_t *in) {

	d_md3_frame_t out = *in;

#if SDL_BYTEORDER != SDL_LIL_ENDIAN
	out.mins = LittleVec3(out.mins);
	out.maxs = LittleVec3(out.maxs);
	out.translate = LittleVec3(out.translate);
	out.radius = LittleFloat(out.radius);
#endif

	return out;
}

/**
 * @brief Swap function.
 */
static d_md3_tag_t R_SwapMd3Tag(const d_md3_tag_t *in) {

	d_md3_tag_t out = *in;

#if SDL_BYTEORDER != SDL_LIL_ENDIAN
	out.origin = LittleVec3(out.origin);
	out.axis[0] = LittleVec3(out.axis[0]);
	out.axis[1] = LittleVec3(out.axis[1]);
	out.axis[2] = LittleVec3(out.axis[2]);
#endif

	return out;
}

/**
 * @brief Swap function.
 */
static d_md3_shader_t R_SwapMd3Shader(const d_md3_shader_t *in) {

	d_md3_shader_t out = *in;

#if SDL_BYTEORDER != SDL_LIL_ENDIAN
	out.index = LittleLong(out.index);
#endif

	return out;
}

/**
 * @brief Swap function.
 */
static d_md3_surface_t R_SwapMd3Surface(const d_md3_surface_t *in) {

	d_md3_surface_t out = *in;

#if SDL_BYTEORDER != SDL_LIL_ENDIAN
	out.id = LittleLong(out.id);
	out.flags = LittleLong(out.flags);

	out.num_frames = LittleLong(out.num_frames);
	out.num_shaders = LittleLong(out.num_shaders);
	out.num_vertexes = LittleLong(out.num_vertexes);
	out.num_triangles = LittleLong(out.num_triangles);

	out.ofs_triangles = LittleLong(out.ofs_triangles);
	out.ofs_shaders = LittleLong(out.ofs_shaders);
	out.ofs_texcoords = LittleLong(out.ofs_texcoords);
	out.ofs_vertexes = LittleLong(out.ofs_vertexes);
	out.ofs_end = LittleLong(out.ofs_end);
#endif

	return out;
}

/**
 * @brief Swap function.
 */
static d_md3_t R_SwapMd3(const d_md3_t *in) {

	d_md3_t out = *in;

#if SDL_BYTEORDER != SDL_LIL_ENDIAN
	out.id = LittleLong(out.id);
	out.version = LittleLong(out.version);
	out.flags = LittleLong(out.flags);

	out.num_frames = LittleLong(out.num_frames);
	out.num_tags = LittleLong(out.num_tags);
	out.num_meshes = LittleLong(out.num_meshes);
	out.num_shaders = LittleLong(out.num_shaders);

	out.ofs_frames = LittleLong(out.ofs_frames);
	out.ofs_tags = LittleLong(out.ofs_tags);
	out.ofs_meshes = LittleLong(out.ofs_meshes);
	out.ofs_end = LittleLong(out.ofs_end);
#endif

	return out;
}

/**
 * @brief Loads the d_md3_t contents of buffer to the specified model.
 */
void R_LoadMd3Model(r_model_t *mod, void *buffer) {

	const byte *base = buffer;

	const d_md3_t md3 = R_SwapMd3((d_md3_t *) base);

	if (md3.id != MD3_ID) {
		Com_Error(ERROR_DROP, "%s MD3_ID is %d\n", mod->media.name, md3.id);
	}

	if (md3.version != MD3_VERSION) {
		Com_Error(ERROR_DROP, "%s MD3_VERSION is %d\n", mod->media.name, md3.version);
	}

	if (md3.num_frames < MD3_MIN_FRAMES) {
		Com_Error(ERROR_DROP, "%s MD3_MIN_FRAMES %d\n", mod->media.name, md3.num_frames);
	}

	if (md3.num_frames > MD3_MAX_FRAMES) {
		Com_Error(ERROR_DROP, "%s MD3_MAX_FRAMES %d\n", mod->media.name, md3.num_frames);
	}

	if (md3.num_tags > MD3_MAX_TAGS) {
		Com_Error(ERROR_DROP, "%s MD3_MAX_TAGS %d\n", mod->media.name, md3.num_tags);
	}

	if (md3.num_surfaces > MD3_MAX_SURFACES) {
		Com_Error(ERROR_DROP, "%s MD3_MAX_SURFACES %d\n", mod->media.name, md3.num_surfaces);
	}

	mod->mesh = Mem_LinkMalloc(sizeof(r_mesh_model_t), mod);

	R_LoadModelMaterials(mod);

	{
		mod->mesh->num_frames = md3.num_frames;
		mod->mesh->frames = Mem_LinkMalloc(mod->mesh->num_frames * sizeof(r_mesh_frame_t), mod->mesh);

		const d_md3_frame_t *in = (d_md3_frame_t *) (base + md3.ofs_frames);
		r_mesh_frame_t *out = mod->mesh->frames;

		for (int32_t i = 0; i < mod->mesh->num_frames; i++, in++, out++) {

			const d_md3_frame_t frame = R_SwapMd3Frame(in);

			out->mins = frame.mins;
			out->maxs = frame.maxs;
			
			out->translate = frame.translate;

			mod->mins = Vec3_Minf(mod->mins, out->mins);
			mod->maxs = Vec3_Maxf(mod->maxs, out->maxs);
		}
	}

	{
		mod->mesh->num_tags = md3.num_tags;
		mod->mesh->tags = Mem_LinkMalloc(mod->mesh->num_tags * mod->mesh->num_frames * sizeof(r_mesh_tag_t), mod->mesh);

		const d_md3_tag_t *in = (d_md3_tag_t *) (base + md3.ofs_tags);
		r_mesh_tag_t *out = mod->mesh->tags;

		for (int32_t i = 0; i < mod->mesh->num_frames; i++) {
			for (int32_t j = 0; j < mod->mesh->num_tags; j++, in++, out++) {

				const d_md3_tag_t tag = R_SwapMd3Tag(in);

				g_strlcpy(out->name, tag.name, MD3_MAX_PATH);
				Matrix4x4_FromVectors(&out->matrix, tag.axis[0].xyz, tag.axis[1].xyz, tag.axis[2].xyz, tag.origin.xyz);
			}
		}
	}

	{
		mod->mesh->num_faces = md3.num_surfaces;
		mod->mesh->faces = Mem_LinkMalloc(mod->mesh->num_faces * sizeof(r_mesh_face_t), mod->mesh);

		const d_md3_surface_t *in = (d_md3_surface_t *) (base + md3.ofs_surfaces);
		r_mesh_face_t *out = mod->mesh->faces;

		for (int32_t i = 0; i < mod->mesh->num_faces; i++, out++) {

			const d_md3_surface_t surface = R_SwapMd3Surface(in);

			if (surface.id != MD3_ID) {
				Com_Error(ERROR_DROP, "%s: %s: MD3_ID %d\n", mod->media.name, surface.name, surface.id);
			}

			if (surface.num_shaders > MD3_MAX_SHADERS) {
				Com_Error(ERROR_DROP, "%s: %s: MD3_MAX_SHADERS %d\n", mod->media.name, surface.name, surface.num_shaders);
			}

			if (in->num_triangles > MD3_MAX_TRIANGLES) {
				Com_Error(ERROR_DROP, "%s: %s: MD3_MAX_TRIANGLES %d\n", mod->media.name, surface.name, surface.num_triangles);
			}

			if (in->num_vertexes > MD3_MAX_VERTEXES) {
				Com_Error(ERROR_DROP, "%s: %s: MD3_MAX_VERTEXES %d\n", mod->media.name, surface.name, surface.num_vertexes);
			}

			g_strlcpy(out->name, surface.name, MD3_MAX_PATH);

			const byte *surface_base = (byte *) in;

			if (surface.num_shaders) {
				const d_md3_shader_t *in_shader = (d_md3_shader_t *) (surface_base + surface.ofs_shaders);
				for (int32_t j = 0; j < surface.num_shaders; j++, in_shader++) {
					const d_md3_shader_t skin = R_SwapMd3Shader(in_shader);
					out->material = R_ResolveMeshMaterial(mod, out, skin.name);
				}
			} else {
				out->material = R_ResolveMeshMaterial(mod, out, NULL);
			}

			{
				out->num_vertexes = surface.num_vertexes;
				out->vertexes = Mem_LinkMalloc(out->num_vertexes * mod->mesh->num_frames * sizeof(r_mesh_vertex_t), mod->mesh);

				const d_md3_vertex_t *in_vertex = (d_md3_vertex_t *) (surface_base + surface.ofs_vertexes);
				r_mesh_vertex_t *out_vertex = out->vertexes;

				for (int32_t j = 0; j < mod->mesh->num_frames; j++) {

					const d_md3_texcoord_t *in_texcoord = (d_md3_texcoord_t *) (surface_base + surface.ofs_texcoords);

					for (int32_t k = 0; k < out->num_vertexes; k++, in_vertex++, in_texcoord++, out_vertex++) {

						const d_md3_vertex_t vertex = R_SwapMd3Vertex(in_vertex);

						out_vertex->position = Vec3_Scale(Vec3s_CastVec3(vertex.point), MD3_XYZ_SCALE);

						float lat = (vertex.norm >> 8) & 0xff;
						float lon = (vertex.norm & 0xff);

						lat *= M_PI / 128.0;
						lon *= M_PI / 128.0;

						out_vertex->normal.x = cos(lat) * sin(lon);
						out_vertex->normal.y = sin(lat) * sin(lon);
						out_vertex->normal.z = cos(lon);

						out_vertex->normal = Vec3_Normalize(out_vertex->normal);

						const d_md3_texcoord_t texcoord = R_SwapMd3Texcoord(in_texcoord);

						out_vertex->diffuse = texcoord.st;
					}
				}
			}

			{
				out->num_elements = surface.num_triangles * 3;
				out->elements = Mem_LinkMalloc(out->num_elements * sizeof(GLuint), mod->mesh);

				const d_md3_triangle_t *in_triangle = (d_md3_triangle_t *) (surface_base + surface.ofs_triangles);
				GLuint *out_triangle = out->elements;

				for (int32_t j = 0; j < surface.num_triangles; j++, in_triangle++, out_triangle += 3) {

					const d_md3_triangle_t tri = R_SwapMd3Triangle(in_triangle);

					out_triangle[0] = tri.indexes[0];
					out_triangle[1] = tri.indexes[1];
					out_triangle[2] = tri.indexes[2];
				}
			}

			in = (d_md3_surface_t *) (surface_base + in->ofs_end);
		}
	}

	// and animations for player models
	if (strstr(mod->media.name, "/upper")) {
		R_LoadMd3Animations(mod);
	}

	// and the configs
	R_LoadMeshConfigs(mod);

	// and finally load the array
	R_LoadMeshVertexArray(mod);

	Com_Debug(DEBUG_RENDERER, "!================================\n");
	Com_Debug(DEBUG_RENDERER, "!R_LoadMd3Model:   %s\n", mod->media.name);
	Com_Debug(DEBUG_RENDERER, "!  Vertexes:       %d\n", mod->mesh->num_vertexes);
	Com_Debug(DEBUG_RENDERER, "!  Elements:       %d\n", mod->mesh->num_elements);
	Com_Debug(DEBUG_RENDERER, "!  Frames:         %d\n", mod->mesh->num_frames);
	Com_Debug(DEBUG_RENDERER, "!  Tags:           %d\n", mod->mesh->num_tags);
	Com_Debug(DEBUG_RENDERER, "!  Faces:          %d\n", mod->mesh->num_faces);
	Com_Debug(DEBUG_RENDERER, "!  Animations:     %d\n", mod->mesh->num_animations);
	Com_Debug(DEBUG_RENDERER, "!================================\n");
}
