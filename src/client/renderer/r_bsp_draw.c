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

	GLint projection;
	GLint view;
	GLint model;

	GLint texture_material;
	GLint texture_lightmap;
	GLint texture_warp;

	GLint color;
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

	const r_bsp_model_t *bsp = r_world_model->bsp;

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

	const bsp_lightgrid_t *lg = r_world_model->bsp->cm->file.lightgrid;
	if (!lg) {
		return;
	}

	const size_t texture_size = lg->size.x * lg->size.y * lg->size.z * BSP_LIGHTGRID_BPP;

	const byte *textures = (byte *) lg + sizeof(bsp_lightgrid_t);
	int32_t luxel = 0;

	r_image_t *particle = R_LoadImage("sprites/particle", IT_EFFECT);

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

				r_sprite_t sprite = {
					.origin = Vec3(s + 0.5, t + 0.5, u + 0.5),
					.size = 4.f * 8.f,
					.color = Color3b(r, g, b),
					.media = (r_media_t *) particle
				};

				sprite.origin = Vec3_Add(r_world_model->bsp->lightgrid->mins, Vec3_Scale(sprite.origin, BSP_LIGHTGRID_LUXEL_SIZE));

				R_AddSprite(&sprite);
			}
		}
	}
}

/**
 * @brief Draws opaque draw elements for the specified inline model, ordered by material.
 */
static void R_DrawBspInlineModelOpaqueDrawElements(const r_bsp_inline_model_t *in) {

	const r_bsp_node_t *node = NULL;
	const r_material_t *material = NULL;

	for (guint i = 0; i < in->opaque_draw_elements->len; i++) {

		const r_bsp_draw_elements_t *draw = g_ptr_array_index(in->opaque_draw_elements, i);

		if (draw->node->vis_frame != r_locals.vis_frame) {
			continue;
		}

		if (draw->node != node) {
			node = draw->node;
			glUniform1i(r_bsp_program.lights_mask, node->lights_mask);
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
 *@brief Draws alpha blended objects with the specified node depth.
 *
 *@see R_NodeDepthForPoint
 */
static void R_DrawBspInlineModelAlphaBlendDepth(int32_t blend_depth) {

	glBlendFunc(GL_ONE, GL_ZERO);
	glDisable(GL_BLEND);

	glDisable(GL_DEPTH_TEST);
	glDisable(GL_CULL_FACE);

	R_DrawEntities(blend_depth);

	R_DrawSprites(blend_depth);

	glEnable(GL_DEPTH_TEST);
	glEnable(GL_CULL_FACE);

	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	glUseProgram(r_bsp_program.name);
	glBindVertexArray(r_world_model->bsp->vertex_array);

	glBindBuffer(GL_ARRAY_BUFFER, r_world_model->bsp->vertex_buffer);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, r_world_model->bsp->elements_buffer);

	R_GetError(NULL);
}

/**
 * @brief Draws alpha blended draw elements for the specified inline model, ordered back to front.
 */
static void R_DrawBspInlineModelAlphaBlendDrawElements(const r_bsp_inline_model_t *in) {

	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	const r_bsp_node_t *node = NULL;
	const r_material_t *material = NULL;

	for (guint i = 0; i < in->alpha_blend_draw_elements->len; i++) {

		const r_bsp_draw_elements_t *draw = g_ptr_array_index(in->alpha_blend_draw_elements, i);

		if (draw->node->vis_frame != r_locals.vis_frame) {
			continue;
		}

		if (draw->node != node) {
			node = draw->node;

			R_DrawBspInlineModelAlphaBlendDepth(node->blend_depth);
			material = NULL;

			glUniform1i(r_bsp_program.lights_mask, node->lights_mask);
		}

		if (draw->texinfo->material != material) {
			material = draw->texinfo->material;

			glBindTexture(GL_TEXTURE_2D_ARRAY, material->texture->texnum);

			glUniform1f(r_bsp_program.bump, material->cm->bump * r_bumpmap->value);
			glUniform1f(r_bsp_program.parallax, material->cm->parallax * r_parallax->value);
			glUniform1f(r_bsp_program.hardness, material->cm->hardness * r_hardness->value);
			glUniform1f(r_bsp_program.specular, material->cm->specular * r_specular->value);
			glUniform1f(r_bsp_program.warp, material->cm->warp * r_warp->value);

			float alpha = 1.0;

			switch (draw->texinfo->flags & SURF_MASK_BLEND) {
				case SURF_BLEND_33:
					alpha = 0.333f;
					break;
				case SURF_BLEND_66:
					alpha = 0.666f;
				default:
					break;
			}

			glUniform4fv(r_bsp_program.color, 1, Color4f(1.f, 1.f, 1.f, alpha).rgba);
		}

		glDrawElements(GL_TRIANGLES, draw->num_elements, GL_UNSIGNED_INT, draw->elements);

		r_view.count_bsp_draw_elements_blend++;
	}

	glBlendFunc(GL_ONE, GL_ZERO);
	glDisable(GL_BLEND);
}

/**
 * @brief
 */
void R_DrawWorld(void) {

	glEnable(GL_DEPTH_TEST);
	glEnable(GL_CULL_FACE);

	glUseProgram(r_bsp_program.name);

	glUniformMatrix4fv(r_bsp_program.projection, 1, GL_FALSE, (GLfloat *) r_locals.projection3D.m);
	glUniformMatrix4fv(r_bsp_program.view, 1, GL_FALSE, (GLfloat *) r_locals.view.m);

	glUniform4fv(r_bsp_program.color, 1, color_white.rgba);
	glUniform1f(r_bsp_program.alpha_threshold, .125f);

	glUniform1f(r_bsp_program.brightness, r_brightness->value);
	glUniform1f(r_bsp_program.contrast, r_contrast->value);
	glUniform1f(r_bsp_program.saturation, r_saturation->value);
	glUniform1f(r_bsp_program.gamma, r_gamma->value);
	glUniform1f(r_bsp_program.modulate, r_modulate->value);

	glBindBufferBase(GL_UNIFORM_BUFFER, 0, r_lights.uniform_buffer);

	glUniform3fv(r_bsp_program.fog_parameters, 1, r_locals.fog_parameters.xyz);
	glUniform3fv(r_bsp_program.fog_color, 1, r_view.fog_color.xyz);

	glUniform1i(r_bsp_program.ticks, r_view.ticks);

	glBindVertexArray(r_world_model->bsp->vertex_array);

	glBindBuffer(GL_ARRAY_BUFFER, r_world_model->bsp->vertex_buffer);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, r_world_model->bsp->elements_buffer);

	glEnableVertexAttribArray(r_bsp_program.in_position);
	glEnableVertexAttribArray(r_bsp_program.in_normal);
	glEnableVertexAttribArray(r_bsp_program.in_tangent);
	glEnableVertexAttribArray(r_bsp_program.in_bitangent);
	glEnableVertexAttribArray(r_bsp_program.in_diffusemap);
	glEnableVertexAttribArray(r_bsp_program.in_lightmap);

	glActiveTexture(GL_TEXTURE0 + TEXTURE_LIGHTMAP);
	glBindTexture(GL_TEXTURE_2D_ARRAY, r_world_model->bsp->lightmap->atlas->texnum);

	glActiveTexture(GL_TEXTURE0 + TEXTURE_WARP);
	glBindTexture(GL_TEXTURE_2D, r_bsp_program.warp_image->texnum);

	glActiveTexture(GL_TEXTURE0 + TEXTURE_MATERIAL);

	glUniformMatrix4fv(r_bsp_program.model, 1, GL_FALSE, (GLfloat *) matrix4x4_identity.m);
	R_DrawBspInlineModelOpaqueDrawElements(r_world_model->bsp->inline_models);

	{
		const r_entity_t *e = r_view.entities;
		for (int32_t i = 0; i < r_view.num_entities; i++, e++) {
			if (IS_BSP_INLINE_MODEL(e->model)) {

				glUniformMatrix4fv(r_bsp_program.model, 1, GL_FALSE, (GLfloat *) e->matrix.m);
				R_DrawBspInlineModelOpaqueDrawElements(e->model->bsp_inline);
			}
		}
	}

	glUniformMatrix4fv(r_bsp_program.model, 1, GL_FALSE, (GLfloat *) matrix4x4_identity.m);
	R_DrawBspInlineModelAlphaBlendDrawElements(r_world_model->bsp->inline_models);

	{
		const r_entity_t *e = r_view.entities;
		for (int32_t i = 0; i < r_view.num_entities; i++, e++) {
			if (IS_BSP_INLINE_MODEL(e->model)) {

				glUniformMatrix4fv(r_bsp_program.model, 1, GL_FALSE, (GLfloat *) e->matrix.m);
				//R_DrawBspInlineModelAlphaBlendDrawElements(e->model->bsp_inline);
			}
		}
	}

	glUseProgram(0);

	glDisable(GL_CULL_FACE);
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

	r_bsp_program.projection = glGetUniformLocation(r_bsp_program.name, "projection");
	r_bsp_program.view = glGetUniformLocation(r_bsp_program.name, "view");
	r_bsp_program.model = glGetUniformLocation(r_bsp_program.name, "model");

	r_bsp_program.texture_material = glGetUniformLocation(r_bsp_program.name, "texture_material");
	r_bsp_program.texture_lightmap = glGetUniformLocation(r_bsp_program.name, "texture_lightmap");
	r_bsp_program.texture_warp = glGetUniformLocation(r_bsp_program.name, "texture_warp");

	r_bsp_program.color = glGetUniformLocation(r_bsp_program.name, "color");
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

	glDeleteProgram(r_bsp_program.name);

	r_bsp_program.name = 0;
}
