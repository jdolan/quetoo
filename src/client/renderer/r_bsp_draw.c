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

#define TEXTURE_MATERIAL 0
#define TEXTURE_LIGHTMAP 1
#define TEXTURE_WARP     2

/**
 * @brief The program.
 */
static struct {
	GLuint name;

	GLint in_position;
	GLint in_normal;
	GLint in_tangent;
	GLint in_bitangent;
	GLint in_diffusemap;
	GLint in_lightmap;
	GLint in_color;

	GLint projection;
	GLint view;
	GLint model;

	GLint texture_material;
	GLint texture_lightmap;
	GLint texture_warp;

	GLint alpha_threshold;

	GLint brightness;
	GLint contrast;
	GLint saturation;
	GLint gamma;
	
	GLint modulate;

	GLint bump;
	GLint parallax;
	GLint hardness;
	GLint specular;
	GLint warp;

	GLint lights_block;
	GLuint lights_buffer;
	GLint lights_mask;

	GLint fog_parameters;
	GLint fog_color;

	GLint ticks;

	r_image_t *warp_image;
} r_bsp_program;

/**
 * @brief
 */
static void R_DrawBspNormals(void) {

	if (!r_draw_bsp_normals->value) {
		return;
	}

	const r_bsp_model_t *bsp = r_model_state.world->bsp;

	const r_bsp_vertex_t *v = bsp->vertexes;
	for (int32_t i = 0; i < bsp->num_vertexes; i++, v++) {
		
		const vec3_t pos = v->position;
		if (Vec3_Distance(pos, r_view.origin) > 256.f) {
			continue;
		}

		const vec3_t normal[] = { pos, Vec3_Add(pos, Vec3_Scale(v->normal, 8.f)) };
		const vec3_t tangent[] = { pos, Vec3_Add(pos, Vec3_Scale(v->tangent, 8.f)) };
		const vec3_t bitangent[] = { pos, Vec3_Add(pos, Vec3_Scale(v->bitangent, 8.f)) };

		R_Draw3DLines(normal, 2, color_red);

		if (r_draw_bsp_normals->integer > 1) {
			R_Draw3DLines(tangent, 2, color_green);

			if (r_draw_bsp_normals->integer > 2) {
				R_Draw3DLines(bitangent, 2, color_blue);
			}
		}
	}
}

/**
 * @brief
 */
static void R_DrawBspLightgrid(void) {

	if (!r_draw_bsp_lightgrid->value) {
		return;
	}

	const bsp_lightgrid_t *lg = r_model_state.world->bsp->cm->file.lightgrid;
	if (!lg) {
		return;
	}

	const size_t texture_size = lg->size.x * lg->size.y * lg->size.z * BSP_LIGHTGRID_BPP;

	const byte *textures = (byte *) lg + sizeof(bsp_lightgrid_t);
	int32_t luxel = 0;

	for (int32_t u = 0; u < lg->size.z; u++) {
		for (int32_t t = 0; t < lg->size.y; t++) {
			for (int32_t s = 0; s < lg->size.x; s++, luxel++) {

				byte r = 0, g = 0, b = 0;

				for (int32_t i = 0; i < BSP_LIGHTGRID_TEXTURES; i++) {
					if (r_draw_bsp_lightgrid->integer & (1 << i)) {

						const byte *texture = textures + i * texture_size;
						const byte *color = texture + luxel * BSP_LIGHTGRID_BPP;

						r += color[0];
						g += color[1];
						b += color[2];
					}
				}

				r_particle_t p = {
					.origin = Vec3(s + 0.5, t + 0.5, u + 0.5),
					.size = 4.f,
					.color = Color3b(r, g, b),
				};

				p.origin = Vec3_Add(r_model_state.world->bsp->lightgrid->mins, Vec3_Scale(p.origin, BSP_LIGHTGRID_LUXEL_SIZE));

				R_AddParticle(&p);
			}
		}
	}
}

/**
 *@brief Draws alpha blended objects positioned behind the specified node.
 */
static void R_DrawBspNodeBlendContents(const r_bsp_inline_model_t *in, r_bsp_node_t *node) {

	const r_bsp_model_t *bsp = R_WorldModel()->bsp;
	if (in != bsp->inline_models) {
		return;
	}

	if (!node->num_particles && !node->num_sprites && !node->num_entities) {
		return;
	}

	glDisable(GL_BLEND);
	glBlendFunc(GL_ONE, GL_ZERO);

	glCullFace(GL_BACK);
	glDisable(GL_CULL_FACE);

	glDisable(GL_DEPTH_TEST);
	glDepthMask(GL_TRUE);

	if (node->num_entities) {
		R_DrawEntities(node);
	}

	if (node->num_particles) {
		R_DrawParticles(node);
	}
	
	if (node->num_sprites) {
		R_DrawSprites(node);
	}

	R_GetError(NULL);

	glEnable(GL_DEPTH_TEST);
	glDepthMask(GL_FALSE);

	glEnable(GL_CULL_FACE);
	glCullFace(GL_FRONT);

	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	glUseProgram(r_bsp_program.name);

	glBindVertexArray(bsp->vertex_array);

	glBindBuffer(GL_ARRAY_BUFFER, bsp->vertex_buffer);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, bsp->elements_buffer);

	R_GetError(NULL);

	node->num_particles = 0;
	node->num_sprites = 0;
	node->num_entities = 0;
}

/**
 * @brief Recurses the specified node, drawing alpha blended elements from back to front.
 */
static void R_DrawBspNodeBlend(const r_bsp_inline_model_t *in, r_bsp_node_t *node) {

	if (node->contents != CONTENTS_NODE) {
		return;
	}

	if (node->vis_frame != r_locals.vis_frame) {
		return;
	}

	const float dist = Cm_DistanceToPlane(r_view.origin, node->plane);
	const int32_t side = dist > 0.f ? 1 : 0;

	R_DrawBspNodeBlend(in, node->children[side]);

	if (node->surface_mask & SURF_MASK_BLEND) {

		R_DrawBspNodeBlendContents(in, node);

		glUniform1i(r_bsp_program.lights_mask, node->lights);

		const r_material_t *material = NULL;

		const r_bsp_draw_elements_t *draw = node->draw_elements;
		for (int32_t i = 0; i < node->num_draw_elements; i++, draw++) {

			if (draw->texinfo->flags & SURF_MASK_BLEND) {

				if (draw->texinfo->material != material) {
					material = draw->texinfo->material;

					glBindTexture(GL_TEXTURE_2D_ARRAY, material->texture->texnum);

					glUniform1f(r_bsp_program.bump, material->cm->bump * r_bumpmap->value);
					glUniform1f(r_bsp_program.parallax, material->cm->parallax * r_parallax->value);
					glUniform1f(r_bsp_program.hardness, material->cm->hardness * r_hardness->value);
					glUniform1f(r_bsp_program.specular, material->cm->specular * r_specular->value);
					glUniform1f(r_bsp_program.warp, material->cm->warp * r_warp->value);
				}

				glDrawElements(GL_TRIANGLES, draw->num_elements, GL_UNSIGNED_INT, draw->elements);

				r_view.count_bsp_draw_elements_blend++;
			}
		}

		R_GetError(NULL);
	}

	R_DrawBspNodeBlend(in, node->children[!side]);
}

/**
 * @brief
 */
static void R_DrawBspDrawElements(const r_bsp_inline_model_t *in, const GPtrArray *draw_elements) {

	const r_bsp_node_t *node = NULL;
	const r_material_t *material = NULL;

	for (guint i = 0; i < draw_elements->len; i++) {

		const r_bsp_draw_elements_t *draw = g_ptr_array_index(draw_elements, i);

		if (draw->node->model != in) {
			continue;
		}

		if (draw->node->vis_frame != r_locals.vis_frame) {
			continue;
		}

		if (draw->node != node) {
			node = draw->node;

			glUniform1i(r_bsp_program.lights_mask, node->lights);
		}

		if (draw->texinfo->material != material) {
			material = draw->texinfo->material;

			glBindTexture(GL_TEXTURE_2D_ARRAY, material->texture->texnum);

			glUniform1f(r_bsp_program.bump, material->cm->bump * r_bumpmap->value);
			glUniform1f(r_bsp_program.parallax, material->cm->parallax * r_parallax->value);
			glUniform1f(r_bsp_program.hardness, material->cm->hardness * r_hardness->value);
			glUniform1f(r_bsp_program.specular, material->cm->specular * r_specular->value);
			glUniform1f(r_bsp_program.warp, material->cm->warp * r_warp->value);
		}

		glDrawElements(GL_TRIANGLES, draw->num_elements, GL_UNSIGNED_INT, draw->elements);

		r_view.count_bsp_draw_elements++;
	}
}

/**
 * @brief
 */
static void R_DrawBspInlineModel(const r_bsp_inline_model_t *in) {

	R_DrawBspDrawElements(in, r_model_state.world->bsp->draw_elements_opaque);

	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	glDepthMask(GL_FALSE);

	R_DrawBspNodeBlend(in, in->head_node);

	glDepthMask(GL_TRUE);

	glBlendFunc(GL_ONE, GL_ZERO);
	glDisable(GL_BLEND);

	R_GetError(NULL);
}

/**
 * @brief
 */
static void R_DrawBspEntity(const r_entity_t *e) {

	glUniformMatrix4fv(r_bsp_program.model, 1, GL_FALSE, (GLfloat *) e->matrix.m);

	R_DrawBspInlineModel(e->model->bsp_inline);
}

/**
 * @brief
 */
void R_DrawWorld(void) {

	glEnable(GL_DEPTH_TEST);
	glDepthMask(GL_TRUE);

	glEnable(GL_CULL_FACE);
	glCullFace(GL_FRONT);

	if (r_draw_wireframe->value) {
		glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
	}

	glUseProgram(r_bsp_program.name);

	glUniformMatrix4fv(r_bsp_program.projection, 1, GL_FALSE, (GLfloat *) r_locals.projection3D.m);
	glUniformMatrix4fv(r_bsp_program.view, 1, GL_FALSE, (GLfloat *) r_locals.view.m);
	glUniformMatrix4fv(r_bsp_program.model, 1, GL_FALSE, (GLfloat *) matrix4x4_identity.m);

	glUniform1f(r_bsp_program.alpha_threshold, .125f);

	glUniform1f(r_bsp_program.brightness, r_brightness->value);
	glUniform1f(r_bsp_program.contrast, r_contrast->value);
	glUniform1f(r_bsp_program.saturation, r_saturation->value);
	glUniform1f(r_bsp_program.gamma, r_gamma->value);
	glUniform1f(r_bsp_program.modulate, r_modulate->value);

	glBindBuffer(GL_UNIFORM_BUFFER, r_bsp_program.lights_buffer);
	glBufferData(GL_UNIFORM_BUFFER, sizeof(r_locals.view_lights), r_locals.view_lights, GL_DYNAMIC_DRAW);
	glBindBufferBase(GL_UNIFORM_BUFFER, 0, r_bsp_program.lights_buffer);

	glUniform3fv(r_bsp_program.fog_parameters, 1, r_locals.fog_parameters.xyz);
	glUniform3fv(r_bsp_program.fog_color, 1, r_view.fog_color.xyz);

	glUniform1i(r_bsp_program.ticks, r_view.ticks);

	const r_bsp_model_t *bsp = R_WorldModel()->bsp;

	glBindVertexArray(bsp->vertex_array);

	glBindBuffer(GL_ARRAY_BUFFER, bsp->vertex_buffer);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, bsp->elements_buffer);

	glEnableVertexAttribArray(r_bsp_program.in_position);
	glEnableVertexAttribArray(r_bsp_program.in_normal);
	glEnableVertexAttribArray(r_bsp_program.in_tangent);
	glEnableVertexAttribArray(r_bsp_program.in_bitangent);
	glEnableVertexAttribArray(r_bsp_program.in_diffusemap);
	glEnableVertexAttribArray(r_bsp_program.in_lightmap);
	glEnableVertexAttribArray(r_bsp_program.in_color);

	glActiveTexture(GL_TEXTURE0 + TEXTURE_LIGHTMAP);
	glBindTexture(GL_TEXTURE_2D_ARRAY, bsp->lightmap ? bsp->lightmap->atlas->texnum : 0);

	glActiveTexture(GL_TEXTURE0 + TEXTURE_WARP);
	glBindTexture(GL_TEXTURE_2D, r_bsp_program.warp_image->texnum);

	glActiveTexture(GL_TEXTURE0 + TEXTURE_MATERIAL);

	R_DrawBspInlineModel(bsp->inline_models);

	const r_entity_t *e = r_view.entities;
	for (int32_t i = 0; i < r_view.num_entities; i++, e++) {
		if (e->model && e->model->type == MOD_BSP_INLINE) {
			R_DrawBspEntity(e);
		}
	}

	glUseProgram(0);

	if (r_draw_wireframe->value) {
		glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
	}

	glCullFace(GL_FRONT);
	glDisable(GL_CULL_FACE);
	
	glDepthMask(GL_FALSE);
	glDisable(GL_DEPTH_TEST);

	glBindVertexArray(0);

	R_GetError(NULL);

	R_DrawBspNormals();
	R_DrawBspLightgrid();
}

#define WARP_IMAGE_SIZE 128

/**
 * @brief
 */
void R_InitBspProgram(void) {

	memset(&r_bsp_program, 0, sizeof(r_bsp_program));

	r_bsp_program.name = R_LoadProgram(
			&MakeShaderDescriptor(GL_VERTEX_SHADER, "bsp_vs.glsl"),
			&MakeShaderDescriptor(GL_FRAGMENT_SHADER, "common_fs.glsl", "lights_fs.glsl", "bsp_fs.glsl"),
			NULL);

	glUseProgram(r_bsp_program.name);

	r_bsp_program.in_position = glGetAttribLocation(r_bsp_program.name, "in_position");
	r_bsp_program.in_normal = glGetAttribLocation(r_bsp_program.name, "in_normal");
	r_bsp_program.in_tangent = glGetAttribLocation(r_bsp_program.name, "in_tangent");
	r_bsp_program.in_bitangent = glGetAttribLocation(r_bsp_program.name, "in_bitangent");
	r_bsp_program.in_diffusemap = glGetAttribLocation(r_bsp_program.name, "in_diffusemap");
	r_bsp_program.in_lightmap = glGetAttribLocation(r_bsp_program.name, "in_lightmap");
	r_bsp_program.in_color = glGetAttribLocation(r_bsp_program.name, "in_color");

	r_bsp_program.projection = glGetUniformLocation(r_bsp_program.name, "projection");
	r_bsp_program.view = glGetUniformLocation(r_bsp_program.name, "view");
	r_bsp_program.model = glGetUniformLocation(r_bsp_program.name, "model");

	r_bsp_program.texture_material = glGetUniformLocation(r_bsp_program.name, "texture_material");
	r_bsp_program.texture_lightmap = glGetUniformLocation(r_bsp_program.name, "texture_lightmap");
	r_bsp_program.texture_warp = glGetUniformLocation(r_bsp_program.name, "texture_warp");

	r_bsp_program.alpha_threshold = glGetUniformLocation(r_bsp_program.name, "alpha_threshold");

	r_bsp_program.brightness = glGetUniformLocation(r_bsp_program.name, "brightness");
	r_bsp_program.contrast = glGetUniformLocation(r_bsp_program.name, "contrast");
	r_bsp_program.saturation = glGetUniformLocation(r_bsp_program.name, "saturation");
	r_bsp_program.gamma = glGetUniformLocation(r_bsp_program.name, "gamma");
	r_bsp_program.modulate = glGetUniformLocation(r_bsp_program.name, "modulate");

	r_bsp_program.bump = glGetUniformLocation(r_bsp_program.name, "bump");
	r_bsp_program.parallax = glGetUniformLocation(r_bsp_program.name, "parallax");
	r_bsp_program.hardness = glGetUniformLocation(r_bsp_program.name, "hardness");
	r_bsp_program.specular = glGetUniformLocation(r_bsp_program.name, "specular");
	r_bsp_program.warp = glGetUniformLocation(r_bsp_program.name, "warp");

	r_bsp_program.lights_block = glGetUniformBlockIndex(r_bsp_program.name, "lights_block");
	glUniformBlockBinding(r_bsp_program.name, r_bsp_program.lights_block, 0);
	glGenBuffers(1, &r_bsp_program.lights_buffer);
	r_bsp_program.lights_mask = glGetUniformLocation(r_bsp_program.name, "lights_mask");

	r_bsp_program.fog_parameters = glGetUniformLocation(r_bsp_program.name, "fog_parameters");
	r_bsp_program.fog_color = glGetUniformLocation(r_bsp_program.name, "fog_color");

	r_bsp_program.ticks = glGetUniformLocation(r_bsp_program.name, "ticks");

	glUniform1i(r_bsp_program.texture_material, TEXTURE_MATERIAL);
	glUniform1i(r_bsp_program.texture_lightmap, TEXTURE_LIGHTMAP);
	glUniform1i(r_bsp_program.texture_warp, TEXTURE_WARP);

	r_bsp_program.warp_image = (r_image_t *) R_AllocMedia("r_warp_image", sizeof(r_image_t), MEDIA_IMAGE);
	r_bsp_program.warp_image->media.Retain = R_RetainImage;
	r_bsp_program.warp_image->media.Free = R_FreeImage;

	r_bsp_program.warp_image->width = r_bsp_program.warp_image->height = WARP_IMAGE_SIZE;
	r_bsp_program.warp_image->type = IT_PROGRAM;

	byte data[WARP_IMAGE_SIZE][WARP_IMAGE_SIZE][4];

	for (int32_t i = 0; i < WARP_IMAGE_SIZE; i++) {
		for (int32_t j = 0; j < WARP_IMAGE_SIZE; j++) {
			data[i][j][0] = 255;
			data[i][j][1] = 255;
			data[i][j][2] = RandomRangeu(0, 48);
			data[i][j][3] = RandomRangeu(0, 48);
		}
	}

	R_UploadImage(r_bsp_program.warp_image, GL_RGBA, (byte *) data);

	R_GetError(NULL);
}

/**
 * @brief
 */
void R_ShutdownBspProgram(void) {

	glDeleteBuffers(1, &r_bsp_program.lights_buffer);

	glDeleteProgram(r_bsp_program.name);

	r_bsp_program.name = 0;
}
