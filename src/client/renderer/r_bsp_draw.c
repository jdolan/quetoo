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

	GLint bicubic;
	GLint parallax_samples;

	struct {
		GLint roughness;
		GLint hardness;
		GLint specularity;
		GLint parallax;
	} material;

	struct {
		GLint flags;
		GLint ticks;
		GLint color;
		GLint pulse;
		GLint st_origin;
		GLint stretch;
		GLint rotate;
		GLint scroll;
		GLint scale;
		GLint terrain;
		GLint dirtmap;
		GLint warp;
	} stage;

	GLint lights_block;
	GLint lights_mask;

	GLint fog_parameters;
	GLint fog_color;

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
void R_DrawBspLightgrid(void) {

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

						r = Mini(r + color[0], 255);
						g = Mini(g + color[1], 255);
						b = Mini(b + color[2], 255);
					}
				}

				r_sprite_t sprite = {
					.origin = Vec3(s + 0.5, t + 0.5, u + 0.5),
					.size = 8.f,
					.color = Color32(r, g, b, 255),
					.media = (r_media_t *) particle
				};

				sprite.origin = Vec3_Add(r_world_model->bsp->lightgrid->mins, Vec3_Scale(sprite.origin, BSP_LIGHTGRID_LUXEL_SIZE));

				if (Vec3_DistanceSquared(r_view.origin, sprite.origin) > 512 * 512) {
					continue;
				}

				R_AddSprite(&sprite);
			}
		}
	}
}

/**
 * @brief
 */
static void R_DrawBspDrawElementsMaterialStage(const r_entity_t *e, const r_bsp_draw_elements_t *draw, const r_stage_t *stage) {

	glUniform1i(r_bsp_program.stage.flags, stage->cm->flags);

	if (stage->cm->flags & STAGE_COLOR) {
		glUniform4fv(r_bsp_program.stage.color, 1, stage->cm->color.rgba);
	}

	if (stage->cm->flags & STAGE_PULSE) {
		glUniform1f(r_bsp_program.stage.pulse, stage->cm->pulse.hz);
	}

	if (stage->cm->flags & (STAGE_STRETCH | STAGE_ROTATE)) {
		glUniform2fv(r_bsp_program.stage.st_origin, 1, draw->st_origin.xy);
	}

	if (stage->cm->flags & STAGE_STRETCH) {
		glUniform2f(r_bsp_program.stage.stretch, stage->cm->stretch.amp, stage->cm->stretch.hz);
	}

	if (stage->cm->flags & STAGE_ROTATE) {
		glUniform1f(r_bsp_program.stage.rotate, stage->cm->rotate.hz);
	}

	if (stage->cm->flags & (STAGE_SCROLL_S | STAGE_SCROLL_T)) {
		glUniform2f(r_bsp_program.stage.scroll, stage->cm->scroll.s, stage->cm->scroll.t);
	}

	if (stage->cm->flags & (STAGE_SCALE_S | STAGE_SCALE_T)) {
		glUniform2f(r_bsp_program.stage.scale, stage->cm->scale.s, stage->cm->scale.t);
	}

	if (stage->cm->flags & STAGE_TERRAIN) {
		glUniform2f(r_bsp_program.stage.terrain, stage->cm->terrain.floor, stage->cm->terrain.ceil);
	}

	if (stage->cm->flags & STAGE_DIRTMAP) {
		glUniform1f(r_bsp_program.stage.dirtmap, stage->cm->dirtmap.intensity);
	}

	if (stage->cm->flags & STAGE_WARP) {
		glUniform2f(r_bsp_program.stage.warp, stage->cm->warp.hz, stage->cm->warp.amplitude);
	}

	glBlendFunc(stage->cm->blend.src, stage->cm->blend.dest);

	if (stage->media) {
		switch (stage->media->type) {
			case R_MEDIA_IMAGE:
			case R_MEDIA_ATLAS_IMAGE: {
				const r_image_t *image = (r_image_t *) stage->media;
				glBindTexture(GL_TEXTURE_2D, image->texnum);
			}
				break;
			case R_MEDIA_ANIMATION: {
				const r_animation_t *animation = (r_animation_t *) stage->media;
				int32_t frame;
				if (stage->cm->animation.fps == 0.f && e != NULL) {
					frame = e->frame;
				} else {
					frame = r_view.ticks / 1000.f * stage->cm->animation.fps;
				}
				glBindTexture(GL_TEXTURE_2D, animation->frames[frame % animation->num_frames]->texnum);
			}
				break;
			case R_MEDIA_MATERIAL: {
				const r_material_t *material = (r_material_t *) stage->media;
				glActiveTexture(GL_TEXTURE0 + TEXTURE_MATERIAL);
				glBindTexture(GL_TEXTURE_2D_ARRAY, material->texture->texnum);
				glActiveTexture(GL_TEXTURE0 + TEXTURE_STAGE);
			}
				break;
			default:
				break;
		}
	}

	glDrawElements(GL_TRIANGLES, draw->num_elements, GL_UNSIGNED_INT, draw->elements);

	R_GetError(draw->texinfo->texture);
}

/**
 * @brief
 */
static void R_DrawBspDrawElementsMaterialStages(const r_entity_t *e, const r_bsp_draw_elements_t *draw, const r_material_t *material) {

	if (!r_materials->value) {
		return;
	}

	if (!(material->cm->flags & STAGE_DRAW)) {
		return;
	}

	glEnable(GL_BLEND);
	glEnable(GL_POLYGON_OFFSET_FILL);

	glActiveTexture(GL_TEXTURE0 + TEXTURE_STAGE);

	int32_t s = 1;
	for (r_stage_t *stage = material->stages; stage; stage = stage->next, s++) {

		if (!(stage->cm->flags & STAGE_DRAW)) {
			continue;
		}

		glPolygonOffset(-1.f, -s);

		R_DrawBspDrawElementsMaterialStage(e, draw, stage);
	}

	glUniform1i(r_bsp_program.stage.flags, STAGE_MATERIAL);

	glActiveTexture(GL_TEXTURE0 + TEXTURE_MATERIAL);

	glPolygonOffset(0.f, 0.f);
	glDisable(GL_POLYGON_OFFSET_FILL);

	glBlendFunc(GL_ONE, GL_ZERO);
	glDisable(GL_BLEND);

	R_GetError(NULL);
}

/**
 * @brief Draws opaque draw elements for the specified inline model, ordered by material.
 */
static void R_DrawBspInlineModelOpaqueDrawElements(const r_entity_t *e, const r_bsp_inline_model_t *in) {
	const r_material_t *material = NULL;

	for (guint i = 0; i < in->opaque_draw_elements->len; i++) {
		const r_bsp_draw_elements_t *draw = g_ptr_array_index(in->opaque_draw_elements, i);

		if (draw->node->vis_frame != r_locals.vis_frame) {
			continue;
		}

		glUniform1i(r_bsp_program.lights_mask, draw->node->lights_mask);

		if (!(draw->texinfo->flags & SURF_MATERIAL)) {

			if (material != draw->texinfo->material) {
				material = draw->texinfo->material;

				glBindTexture(GL_TEXTURE_2D_ARRAY, material->texture->texnum);

				glUniform1f(r_bsp_program.material.roughness, material->cm->roughness * r_roughness->value);
				glUniform1f(r_bsp_program.material.hardness, material->cm->hardness * r_hardness->value);
				glUniform1f(r_bsp_program.material.specularity, material->cm->specularity * r_specularity->value);
				glUniform1f(r_bsp_program.material.parallax, material->cm->parallax * r_parallax->value);
			}

			glDrawElements(GL_TRIANGLES, draw->num_elements, GL_UNSIGNED_INT, draw->elements);
			r_view.count_bsp_triangles += draw->num_elements / 3;
			r_view.count_bsp_draw_elements++;
		}

		R_DrawBspDrawElementsMaterialStages(e, draw, draw->texinfo->material);
	}
}

/**
 *@brief Draws alpha blended objects with the specified node depth.
 *
 *@see R_NodeDepthForPoint
 */
static int32_t R_DrawBspInlineModelAlphaBlendNode(r_bsp_node_t *node) {

	const int32_t blend_depth_count = node->blend_depth_count;
	if (blend_depth_count) {

		glBlendFunc(GL_ONE, GL_ZERO);
		glDisable(GL_BLEND);

		glDisable(GL_DEPTH_TEST);
		glDisable(GL_CULL_FACE);

		R_DrawEntities(node->blend_depth);

		R_DrawSprites(node->blend_depth);

		glEnable(GL_DEPTH_TEST);
		glEnable(GL_CULL_FACE);

		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

		glUseProgram(r_bsp_program.name);
		glBindVertexArray(r_world_model->bsp->vertex_array);

		glBindBuffer(GL_ARRAY_BUFFER, r_world_model->bsp->vertex_buffer);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, r_world_model->bsp->elements_buffer);

		R_GetError(NULL);

		node->blend_depth_count = 0;
	}

	return blend_depth_count;
}

/**
 * @brief Draws alpha blended draw elements for the specified inline model, ordered back to front.
 */
static void R_DrawBspInlineModelAlphaBlendDrawElements(const r_entity_t *e, const r_bsp_inline_model_t *in) {

	const r_material_t *material = NULL;

	glUniform1f(r_bsp_program.alpha_threshold, 0.f);

	for (guint i = 0; i < in->alpha_blend_draw_elements->len; i++) {
		r_bsp_draw_elements_t *draw = g_ptr_array_index(in->alpha_blend_draw_elements, i);

		if (draw->node->vis_frame != r_locals.vis_frame) {
			continue;
		}

		if (R_DrawBspInlineModelAlphaBlendNode(draw->node)) {
			material = NULL;
		}

		glUniform1i(r_bsp_program.lights_mask, draw->node->lights_mask);

		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

		if (!(draw->texinfo->flags & SURF_MATERIAL)) {

			if (material != draw->texinfo->material) {
				material = draw->texinfo->material;

				glBindTexture(GL_TEXTURE_2D_ARRAY, material->texture->texnum);

				glUniform1f(r_bsp_program.material.roughness, material->cm->roughness * r_roughness->value);
				glUniform1f(r_bsp_program.material.hardness, material->cm->hardness * r_hardness->value);
				glUniform1f(r_bsp_program.material.specularity, material->cm->specularity * r_specularity->value);
				glUniform1f(r_bsp_program.material.parallax, material->cm->parallax * r_parallax->value);
			}

			glDrawElements(GL_TRIANGLES, draw->num_elements, GL_UNSIGNED_INT, draw->elements);
			r_view.count_bsp_triangles += draw->num_elements / 3;
			r_view.count_bsp_draw_elements++;
		}

		R_DrawBspDrawElementsMaterialStages(e, draw, draw->texinfo->material);
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

	glUniform1i(r_bsp_program.bicubic, r_bicubic->value);
	glUniform1i(r_bsp_program.parallax_samples, r_parallax_samples->value);

	glBindBufferBase(GL_UNIFORM_BUFFER, 0, r_lights.uniform_buffer);

	glUniform3fv(r_bsp_program.fog_parameters, 1, r_locals.fog_parameters.xyz);
	glUniform3fv(r_bsp_program.fog_color, 1, r_view.fog_color.xyz);

	glUniform1i(r_bsp_program.stage.flags, STAGE_MATERIAL);
	glUniform1i(r_bsp_program.stage.ticks, r_view.ticks);

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
	R_DrawBspInlineModelOpaqueDrawElements(NULL, r_world_model->bsp->inline_models);

	{
		const r_entity_t *e = r_view.entities;
		for (int32_t i = 0; i < r_view.num_entities; i++, e++) {
			if (IS_BSP_INLINE_MODEL(e->model)) {

				glUniformMatrix4fv(r_bsp_program.model, 1, GL_FALSE, (GLfloat *) e->matrix.m);
				R_DrawBspInlineModelOpaqueDrawElements(e, e->model->bsp_inline);
			}
		}
	}

	glUniform1f(r_bsp_program.alpha_threshold, 0.f);

	glUniformMatrix4fv(r_bsp_program.model, 1, GL_FALSE, (GLfloat *) matrix4x4_identity.m);
	R_DrawBspInlineModelAlphaBlendDrawElements(NULL, r_world_model->bsp->inline_models);

	{
		const r_entity_t *e = r_view.entities;
		for (int32_t i = 0; i < r_view.num_entities; i++, e++) {
			if (IS_BSP_INLINE_MODEL(e->model)) {

				glUniformMatrix4fv(r_bsp_program.model, 1, GL_FALSE, (GLfloat *) e->matrix.m);
				R_DrawBspInlineModelAlphaBlendDrawElements(e, e->model->bsp_inline);
			}
		}
	}

	glUseProgram(0);

	glDisable(GL_CULL_FACE);
	glDisable(GL_DEPTH_TEST);

	glBindVertexArray(0);

	R_GetError(NULL);

	R_DrawBspNormals();
}

#define WARP_IMAGE_SIZE 16

/**
 * @brief
 */
void R_InitBspProgram(void) {

	memset(&r_bsp_program, 0, sizeof(r_bsp_program));

	r_bsp_program.name = R_LoadProgram(
			&MakeShaderDescriptor(GL_VERTEX_SHADER, "material.glsl", "bsp_vs.glsl"),
			&MakeShaderDescriptor(GL_FRAGMENT_SHADER, "material.glsl", "common_fs.glsl", "lights_fs.glsl", "bsp_fs.glsl"),
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
	r_bsp_program.bicubic = glGetUniformLocation(r_bsp_program.name, "bicubic");
	r_bsp_program.parallax_samples = glGetUniformLocation(r_bsp_program.name, "parallax_samples");

	r_bsp_program.material.roughness = glGetUniformLocation(r_bsp_program.name, "material.roughness");
	r_bsp_program.material.hardness = glGetUniformLocation(r_bsp_program.name, "material.hardness");
	r_bsp_program.material.specularity = glGetUniformLocation(r_bsp_program.name, "material.specularity");
	r_bsp_program.material.parallax = glGetUniformLocation(r_bsp_program.name, "material.parallax");

	r_bsp_program.stage.flags = glGetUniformLocation(r_bsp_program.name, "stage.flags");
	r_bsp_program.stage.ticks = glGetUniformLocation(r_bsp_program.name, "stage.ticks");
	r_bsp_program.stage.color = glGetUniformLocation(r_bsp_program.name, "stage.color");
	r_bsp_program.stage.pulse = glGetUniformLocation(r_bsp_program.name, "stage.pulse");
	r_bsp_program.stage.st_origin = glGetUniformLocation(r_bsp_program.name, "stage.st_origin");
	r_bsp_program.stage.stretch = glGetUniformLocation(r_bsp_program.name, "stage.stretch");
	r_bsp_program.stage.rotate = glGetUniformLocation(r_bsp_program.name, "stage.rotate");
	r_bsp_program.stage.scroll = glGetUniformLocation(r_bsp_program.name, "stage.scroll");
	r_bsp_program.stage.scale = glGetUniformLocation(r_bsp_program.name, "stage.scale");
	r_bsp_program.stage.terrain = glGetUniformLocation(r_bsp_program.name, "stage.terrain");
	r_bsp_program.stage.dirtmap = glGetUniformLocation(r_bsp_program.name, "stage.dirtmap");
	r_bsp_program.stage.warp = glGetUniformLocation(r_bsp_program.name, "stage.warp");

	r_bsp_program.lights_block = glGetUniformBlockIndex(r_bsp_program.name, "lights_block");
	glUniformBlockBinding(r_bsp_program.name, r_bsp_program.lights_block, 0);
	
	r_bsp_program.lights_mask = glGetUniformLocation(r_bsp_program.name, "lights_mask");

	r_bsp_program.fog_parameters = glGetUniformLocation(r_bsp_program.name, "fog_parameters");
	r_bsp_program.fog_color = glGetUniformLocation(r_bsp_program.name, "fog_color");

	glUniform1i(r_bsp_program.texture_material, TEXTURE_MATERIAL);
	glUniform1i(r_bsp_program.texture_lightmap, TEXTURE_LIGHTMAP);
	glUniform1i(r_bsp_program.texture_stage, TEXTURE_STAGE);
	glUniform1i(r_bsp_program.texture_warp, TEXTURE_WARP);

	r_bsp_program.warp_image = (r_image_t *) R_AllocMedia("r_warp_image", sizeof(r_image_t), R_MEDIA_IMAGE);
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

	r_bsp_program.name = 0;
}
