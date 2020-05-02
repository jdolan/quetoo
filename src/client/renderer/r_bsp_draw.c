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
#define TEXTURE_STAGE    2
#define TEXTURE_WARP     3

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
	GLint texture_stage;
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

	GLint stage;

	GLint lights_block;
	GLint lights_mask;

	GLint fog_parameters;
	GLint fog_color;

	GLint ticks;

	GLuint stage_vertex_array;
	GLuint stage_vertex_buffer;

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
 * @return A texture transform matrix for the specified draw elements and stage.
 */
static mat4_t R_DrawBspDrawElementsMaterialStageTextureMatrix(const r_bsp_draw_elements_t *draw, r_stage_t *stage) {

	mat4_t m = matrix4x4_identity;

	if (stage->cm->flags & (STAGE_STRETCH | STAGE_ROTATE)) {

		vec2_t st_mins = Vec2_Mins();
		vec2_t st_maxs = Vec2_Maxs();

		const GLuint *e = ((GLvoid *) r_world_model->bsp->elements) + (ptrdiff_t) draw->elements;
		for (int32_t i = 0; i < draw->num_elements; i++, e++) {
			const r_bsp_vertex_t *v = &r_world_model->bsp->vertexes[*e];

			st_mins = Vec2_Minf(st_mins, v->diffusemap);
			st_maxs = Vec2_Maxf(st_maxs, v->diffusemap);
		}

		const float s = (st_mins.x + st_maxs.x) * .5f;
		const float t = (st_mins.y + st_maxs.y) * .5f;

		if (stage->cm->flags & STAGE_STRETCH) {
			const float mod = (sinf(r_view.ticks * stage->cm->stretch.hz * 0.00628f) + 1.f) / 2.f;
			const float amp = 1.5f - mod * stage->cm->stretch.amp;
			Matrix4x4_ConcatTranslate(&m, -s, -t, 0.f);
			Matrix4x4_ConcatScale3(&m, amp, amp, 1.f);
			Matrix4x4_ConcatTranslate(&m, -s, -t, 0.f);
		}

		if (stage->cm->flags & STAGE_ROTATE) {
			const float angle = r_view.ticks * stage->cm->rotate.hz * 0.36f;
			Matrix4x4_ConcatTranslate(&m, -s, -t, 0.f);
			Matrix4x4_ConcatRotate(&m, angle, 0.f, 0.f, 1.f);
			Matrix4x4_ConcatTranslate(&m, -s, -t, 0.f);
		}
	}

	if (stage->cm->flags & STAGE_SCALE_S) {
		Matrix4x4_ConcatScale3(&m, stage->cm->scale.s, 1.f, 1.f);
	}

	if (stage->cm->flags & STAGE_SCALE_T) {
		Matrix4x4_ConcatScale3(&m, 1.f, stage->cm->scale.t, 1.f);
	}

	if (stage->cm->flags & STAGE_SCROLL_S) {
		Matrix4x4_ConcatTranslate(&m, stage->cm->scroll.s * r_view.ticks / 1000.f, 0.f, 0.f);
	}

	if (stage->cm->flags & STAGE_SCROLL_T) {
		Matrix4x4_ConcatTranslate(&m, 0.f, stage->cm->scroll.t * r_view.ticks / 1000.f, 0.f);
	}

	return m;
}

/**
 * @brief
 */
static void R_DrawBspDrawElementsMaterialStage(const r_bsp_draw_elements_t *draw, r_stage_t *stage) {

	const mat4_t m = R_DrawBspDrawElementsMaterialStageTextureMatrix(draw, stage);

	r_bsp_vertex_t vertexes[draw->num_elements];
	r_bsp_vertex_t *v = vertexes;

	const GLuint *e = r_world_model->bsp->elements + ((ptrdiff_t) draw->elements) / sizeof(GLuint);
	for (int32_t i = 0; i < draw->num_elements; i++, e++, v++) {

		*v = r_world_model->bsp->vertexes[*e];

		color_t color = Color32_Color(v->color);

		if (stage->cm->flags & STAGE_COLOR) {
			color = Color_Multiply(color, Color3fv(stage->cm->color));
		}

		if (stage->cm->flags & STAGE_PULSE) {
			color.a *= (sinf(r_view.ticks * stage->cm->pulse.hz * 0.00628f) + 1.f) / 2.f;
		}

		v->color = Color_Color32(color);

		Matrix4x4_Transform2(&m, v->diffusemap.xy, v->diffusemap.xy);

		if (stage->cm->flags & STAGE_ANIM) {
			if (r_view.ticks >= stage->anim.time) {
				stage->anim.time = r_view.ticks + (1000 / stage->cm->anim.fps);
				stage->texture = stage->anim.frames[++stage->anim.frame % stage->cm->anim.num_frames];
			}
		}
	}

	glBufferData(GL_ARRAY_BUFFER, draw->num_elements * sizeof(r_bsp_vertex_t), vertexes, GL_DYNAMIC_DRAW);

	glBlendFunc(stage->cm->blend.src, stage->cm->blend.dest);

	glBindTexture(GL_TEXTURE_2D, stage->texture->texnum);

	glDrawArrays(GL_TRIANGLES, 0, draw->num_elements);

	R_GetError(draw->texinfo->texture);
}

/**
 * @brief
 */
static void R_DrawBspDrawElementsMaterialStages(const r_bsp_draw_elements_t *draw) {

	if (!r_materials->value) {
		return;
	}

	if (!(draw->texinfo->material->cm->flags & STAGE_TEXTURE)) {
		return;
	}

	glDepthMask(GL_FALSE);

	glEnable(GL_BLEND);
	glEnable(GL_POLYGON_OFFSET_FILL);

	glBindVertexArray(r_bsp_program.stage_vertex_array);
	glBindBuffer(GL_ARRAY_BUFFER, r_bsp_program.stage_vertex_buffer);

	glActiveTexture(GL_TEXTURE0 + TEXTURE_STAGE);

	int32_t s = 1;
	for (r_stage_t *stage = draw->texinfo->material->stages; stage; stage = stage->next, s++) {

		if (!(stage->cm->flags & STAGE_TEXTURE)) {
			continue;
		}

		glUniform1i(r_bsp_program.stage, s);

		glPolygonOffset(-1.f, -s);

		R_DrawBspDrawElementsMaterialStage(draw, stage);
	}

	glUniform1i(r_bsp_program.stage, 0);

	glBindVertexArray(r_world_model->bsp->vertex_array);
	glBindBuffer(GL_ARRAY_BUFFER, r_world_model->bsp->vertex_buffer);

	glActiveTexture(GL_TEXTURE0 + TEXTURE_MATERIAL);

	glPolygonOffset(0.f, 0.f);
	glDisable(GL_POLYGON_OFFSET_FILL);

	glBlendFunc(GL_ONE, GL_ZERO);
	glDisable(GL_BLEND);

	glDepthMask(GL_TRUE);

	R_GetError(NULL);
}

/**
 * @brief Draws opaque draw elements for the specified inline model, ordered by material.
 */
static void R_DrawBspInlineModelOpaqueDrawElements(const r_bsp_inline_model_t *in) {

	for (guint i = 0; i < in->opaque_draw_elements->len; i++) {
		const r_bsp_draw_elements_t *draw = g_ptr_array_index(in->opaque_draw_elements, i);

		if (draw->node->vis_frame != r_locals.vis_frame) {
			continue;
		}

		glUniform1i(r_bsp_program.lights_mask, draw->node->lights_mask);

		if (!(draw->texinfo->flags & SURF_MATERIAL)) {
			const r_material_t *material = draw->texinfo->material;

			glBindTexture(GL_TEXTURE_2D_ARRAY, material->texture->texnum);

			glUniform1f(r_bsp_program.bump, material->cm->bump * r_bumpmap->value);
			glUniform1f(r_bsp_program.parallax, material->cm->parallax * r_parallax->value);
			glUniform1f(r_bsp_program.hardness, material->cm->hardness * r_hardness->value);
			glUniform1f(r_bsp_program.specular, material->cm->specular * r_specular->value);
			glUniform1f(r_bsp_program.warp, material->cm->warp * r_warp->value);

			glDrawElements(GL_TRIANGLES, draw->num_elements, GL_UNSIGNED_INT, draw->elements);
			r_view.count_bsp_triangles += draw->num_elements / 3;
		}

		R_DrawBspDrawElementsMaterialStages(draw);
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

	glUniform1f(r_bsp_program.alpha_threshold, 0.f);

	for (guint i = 0; i < in->alpha_blend_draw_elements->len; i++) {
		r_bsp_draw_elements_t *draw = g_ptr_array_index(in->alpha_blend_draw_elements, i);

		if (draw->node->vis_frame != r_locals.vis_frame) {
			continue;
		}

		if (draw->node->blend_depth) {
			R_DrawBspInlineModelAlphaBlendDepth(draw->node->blend_depth);
			draw->node->blend_depth = 0;
		}

		glUniform1i(r_bsp_program.lights_mask, draw->node->lights_mask);

		if (!(draw->texinfo->flags & SURF_MATERIAL)) {
			const r_material_t *material = draw->texinfo->material;

			glEnable(GL_BLEND);
			glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

			glBindTexture(GL_TEXTURE_2D_ARRAY, material->texture->texnum);

			glUniform1f(r_bsp_program.bump, material->cm->bump * r_bumpmap->value);
			glUniform1f(r_bsp_program.parallax, material->cm->parallax * r_parallax->value);
			glUniform1f(r_bsp_program.hardness, material->cm->hardness * r_hardness->value);
			glUniform1f(r_bsp_program.specular, material->cm->specular * r_specular->value);
			glUniform1f(r_bsp_program.warp, material->cm->warp * r_warp->value);

			glDrawElements(GL_TRIANGLES, draw->num_elements, GL_UNSIGNED_INT, draw->elements);
			r_view.count_bsp_triangles += draw->num_elements / 3;
		}

		R_DrawBspDrawElementsMaterialStages(draw);
	}

	glBlendFunc(GL_ONE, GL_ZERO);
	glDisable(GL_BLEND);

	R_GetError(NULL);
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
	glEnableVertexAttribArray(r_bsp_program.in_color);

	glActiveTexture(GL_TEXTURE0 + TEXTURE_LIGHTMAP);
	glBindTexture(GL_TEXTURE_2D_ARRAY, r_world_model->bsp->lightmap->atlas->texnum);

	glActiveTexture(GL_TEXTURE0 + TEXTURE_WARP);
	glBindTexture(GL_TEXTURE_2D, r_bsp_program.warp_image->texnum);

	glActiveTexture(GL_TEXTURE0 + TEXTURE_MATERIAL);

	glUniform1f(r_bsp_program.alpha_threshold, .125f);

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

	glUniform1f(r_bsp_program.alpha_threshold, 0.f);

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

#define WARP_IMAGE_SIZE 16

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
	r_bsp_program.texture_stage = glGetUniformLocation(r_bsp_program.name, "texture_stage");
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

	r_bsp_program.stage = glGetUniformLocation(r_bsp_program.name, "stage");

	r_bsp_program.lights_block = glGetUniformBlockIndex(r_bsp_program.name, "lights_block");
	glUniformBlockBinding(r_bsp_program.name, r_bsp_program.lights_block, 0);
	
	r_bsp_program.lights_mask = glGetUniformLocation(r_bsp_program.name, "lights_mask");

	r_bsp_program.fog_parameters = glGetUniformLocation(r_bsp_program.name, "fog_parameters");
	r_bsp_program.fog_color = glGetUniformLocation(r_bsp_program.name, "fog_color");

	r_bsp_program.ticks = glGetUniformLocation(r_bsp_program.name, "ticks");

	glUniform1i(r_bsp_program.texture_material, TEXTURE_MATERIAL);
	glUniform1i(r_bsp_program.texture_lightmap, TEXTURE_LIGHTMAP);
	glUniform1i(r_bsp_program.texture_stage, TEXTURE_STAGE);
	glUniform1i(r_bsp_program.texture_warp, TEXTURE_WARP);

	glGenVertexArrays(1, &r_bsp_program.stage_vertex_array);
	glBindVertexArray(r_bsp_program.stage_vertex_array);

	glGenBuffers(1, &r_bsp_program.stage_vertex_buffer);
	glBindBuffer(GL_ARRAY_BUFFER, r_bsp_program.stage_vertex_buffer);

	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(r_bsp_vertex_t), (void *) offsetof(r_bsp_vertex_t, position));
	glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(r_bsp_vertex_t), (void *) offsetof(r_bsp_vertex_t, normal));
	glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, sizeof(r_bsp_vertex_t), (void *) offsetof(r_bsp_vertex_t, tangent));
	glVertexAttribPointer(3, 3, GL_FLOAT, GL_FALSE, sizeof(r_bsp_vertex_t), (void *) offsetof(r_bsp_vertex_t, bitangent));
	glVertexAttribPointer(4, 2, GL_FLOAT, GL_FALSE, sizeof(r_bsp_vertex_t), (void *) offsetof(r_bsp_vertex_t, diffusemap));
	glVertexAttribPointer(5, 2, GL_FLOAT, GL_FALSE, sizeof(r_bsp_vertex_t), (void *) offsetof(r_bsp_vertex_t, lightmap));
	glVertexAttribPointer(6, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(r_bsp_vertex_t), (void *) offsetof(r_bsp_vertex_t, color));

	glEnableVertexAttribArray(r_bsp_program.in_position);
	glEnableVertexAttribArray(r_bsp_program.in_normal);
	glEnableVertexAttribArray(r_bsp_program.in_tangent);
	glEnableVertexAttribArray(r_bsp_program.in_bitangent);
	glEnableVertexAttribArray(r_bsp_program.in_diffusemap);
	glEnableVertexAttribArray(r_bsp_program.in_lightmap);
	glEnableVertexAttribArray(r_bsp_program.in_color);

	glBindVertexArray(0);
	glBindBuffer(GL_ARRAY_BUFFER, 0);

	r_bsp_program.warp_image = (r_image_t *) R_AllocMedia("r_warp_image", sizeof(r_image_t), MEDIA_IMAGE);
	r_bsp_program.warp_image->media.Retain = R_RetainImage;
	r_bsp_program.warp_image->media.Free = R_FreeImage;

	r_bsp_program.warp_image->width = r_bsp_program.warp_image->height = WARP_IMAGE_SIZE;
	r_bsp_program.warp_image->type = IT_PROGRAM;

	byte data[WARP_IMAGE_SIZE][WARP_IMAGE_SIZE][4];

	for (int32_t i = 0; i < WARP_IMAGE_SIZE; i++) {
		for (int32_t j = 0; j < WARP_IMAGE_SIZE; j++) {
			data[i][j][0] = RandomRangeu(0, 48);
			data[i][j][1] = RandomRangeu(0, 48);
			data[i][j][2] = 0;
			data[i][j][3] = 255;
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

	glDeleteVertexArrays(1, &r_bsp_program.stage_vertex_array);

	glDeleteBuffers(1, &r_bsp_program.stage_vertex_buffer);

	r_bsp_program.name = 0;
}
