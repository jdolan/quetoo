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
 * @brief The BSP program.
 */
static struct {
	GLuint name;

	GLuint uniforms_block;
	GLuint lights_block;

	GLint in_position;
	GLint in_normal;
	GLint in_tangent;
	GLint in_bitangent;
	GLint in_diffusemap;
	GLint in_lightmap;
	GLint in_color;

	GLint model;

	GLint texture_material;
	GLint texture_lightmap;
	GLint texture_stage;
	GLint texture_warp;
	GLint texture_lightgrid_ambient;
	GLint texture_lightgrid_diffuse;
	GLint texture_lightgrid_direction;
	GLint texture_lightgrid_caustics;
	GLint texture_lightgrid_fog;
	GLint texture_shadowmap;
	GLint texture_shadowmap_cube;

	GLint entity;
	GLint alpha_test;

	struct {
		GLint alpha_test;
		GLint roughness;
		GLint hardness;
		GLint specularity;
		GLint bloom;
	} material;

	struct {
		GLint flags;
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

	r_image_t *warp_image;
} r_bsp_program;

/**
 * @brief
 */
static void R_DrawBspNormals(const r_view_t *view) {

	if (!r_draw_bsp_normals->value) {
		return;
	}

	const r_bsp_model_t *bsp = r_world_model->bsp;

	const r_bsp_vertex_t *v = bsp->vertexes;
	for (int32_t i = 0; i < bsp->num_vertexes; i++, v++) {
		
		const vec3_t pos = v->position;
		if (Vec3_Distance(pos, view->origin) > 256.f) {
			continue;
		}

		const vec3_t normal[] = { pos, Vec3_Fmaf(pos, 8.f, v->normal) };
		const vec3_t tangent[] = { pos, Vec3_Fmaf(pos, 8.f, v->tangent) };
		const vec3_t bitangent[] = { pos, Vec3_Fmaf(pos, 8.f, v->bitangent) };

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
void R_AddBspLightgridSprites(r_view_t *view) {

	if (!r_draw_bsp_lightgrid->value) {
		return;
	}

	const byte *in = (byte *) r_world_model->bsp->cm->file->lightgrid;
	if (!in) {
		return;
	}

	const r_bsp_lightgrid_t *lg = r_world_model->bsp->lightgrid;

	const size_t luxels = lg->size.x * lg->size.y * lg->size.z;

	const byte *ambient = in + sizeof(bsp_lightgrid_t);
	const byte *diffuse = ambient + luxels * BSP_LIGHTGRID_BPP;
	const byte *direction = diffuse + luxels * BSP_LIGHTGRID_BPP;
	const byte *caustics = direction + luxels * BSP_LIGHTGRID_BPP;
	const byte *fog = caustics+ luxels * BSP_LIGHTGRID_BPP;

	r_image_t *particle = R_LoadImage("sprites/particle", IT_SPRITE);

	for (int32_t u = 0; u < lg->size.z; u++) {
		for (int32_t t = 0; t < lg->size.y; t++) {
			for (int32_t s = 0; s < lg->size.x; s++, ambient += 3, diffuse += 3, direction += 3, caustics += 3, fog += 4) {

				if (s & 1 || t & 1 || u & 1) {
					continue;
				}

				const vec3_t position = Vec3(s + 0.5f, t + 0.5f, u + 0.5f);
				const vec3_t origin = Vec3_Fmaf(lg->bounds.mins, BSP_LIGHTGRID_LUXEL_SIZE, position);

				if (Vec3_DistanceSquared(view->origin, origin) > 512.f * 512.f) {
					continue;
				}

				if (r_draw_bsp_lightgrid->integer == 1) {

					const byte r = Mini(ambient[0] + diffuse[0], 255);
					const byte g = Mini(ambient[1] + diffuse[1], 255);
					const byte b = Mini(ambient[2] + diffuse[2], 255);

					R_AddSprite(view, &(r_sprite_t) {
						.origin = origin,
						.size = 8.f,
						.color = Color32(r, g, b, 255),
						.media = (r_media_t *) particle,
						.flags = SPRITE_NO_BLEND_DEPTH
					});

					const float x = direction[0] / 255.f * 2.f - 1.f;
					const float y = direction[1] / 255.f * 2.f - 1.f;
					const float z = direction[2] / 255.f * 2.f - 1.f;

					const vec3_t dir = Vec3_Normalize(Vec3(x, y, z));
					const vec3_t end = Vec3_Fmaf(origin, 16.f, dir);

					R_Draw3DLines((vec3_t []) { origin, end }, 2, Color3b(r, g, b));

				} else if (r_draw_bsp_lightgrid->integer == 2) {

					const byte r = caustics[0];
					const byte g = caustics[1];
					const byte b = caustics[2];

					if (r || g || b) {
						const float a = Clampf((r + g + b) / 255.f, 0.f, 255.f);

						R_AddSprite(view, &(r_sprite_t) {
							.origin = origin,
							.size = 8.f,
							.color = Color32(r, g, b, a),
							.media = (r_media_t *) particle
						});
					}
				} else if (r_draw_bsp_lightgrid->integer == 3) {

					const byte a = Mini(fog[3], 255);

					if (a) {
						const byte r = fog[0];
						const byte g = fog[1];
						const byte b = fog[2];
						const float af = a / 255.f;

						R_AddSprite(view, &(r_sprite_t) {
							.origin = origin,
							.size = 8.f,
							.color = Color32(r * af, g * af, b * af, 0),
							.media = (r_media_t *) particle
						});
					}
				}
			}
		}
	}
}

/**
 * @brief
 */
static void R_DrawBspDrawElementsMaterialStage(const r_view_t *view,
											   const r_entity_t *entity,
											   const r_bsp_draw_elements_t *draw,
											   const r_stage_t *stage) {

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
				if (stage->cm->animation.fps == 0.f && entity != NULL) {
					frame = entity->frame;
				} else {
					frame = view->ticks / 1000.f * stage->cm->animation.fps;
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

	R_GetError(draw->material->media.name);
}

/**
 * @brief
 */
static void R_DrawBspDrawElementsMaterialStages(const r_view_t *view,
												const r_entity_t *entity,
												const r_bsp_draw_elements_t *draw,
												const r_material_t *material) {

	if (!r_draw_material_stages->value) {
		return;
	}

	if (!(material->cm->stage_flags & STAGE_DRAW)) {
		return;
	}

	if (draw->surface & SURF_MASK_BLEND) {
		glBlendFunc(GL_ONE, GL_ZERO);
	} else {
		glEnable(GL_BLEND);
	}

	glActiveTexture(GL_TEXTURE0 + TEXTURE_STAGE);

	for (r_stage_t *stage = material->stages; stage; stage = stage->next) {

		if (!(stage->cm->flags & STAGE_DRAW)) {
			continue;
		}

		R_DrawBspDrawElementsMaterialStage(view, entity, draw, stage);
	}

	glUniform1i(r_bsp_program.stage.flags, STAGE_MATERIAL);

	glActiveTexture(GL_TEXTURE0 + TEXTURE_MATERIAL);

	if (draw->surface & SURF_MASK_BLEND) {
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	} else {
		glDisable(GL_BLEND);
	}

	R_GetError(NULL);
}

/**
 * @brief Interrupt BSP drawing to draw depth sorted objects.
 */
static void R_DrawBlendDepthTypes(const r_view_t *view, int32_t blend_depth, r_blend_depth_type_t types) {

	glDisable(GL_DEPTH_TEST);
	glDisable(GL_CULL_FACE);

	glBlendFunc(GL_ONE, GL_ZERO);
	glDisable(GL_BLEND);

	if (types & BLEND_DEPTH_ENTITY) {
		R_DrawEntities(view, blend_depth);
	}

	if (types & BLEND_DEPTH_SPRITE) {
		R_DrawSprites(view, blend_depth);
	}

	glEnable(GL_DEPTH_TEST);
	glEnable(GL_CULL_FACE);

	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	glUseProgram(r_bsp_program.name);
	glBindVertexArray(r_world_model->bsp->vertex_array);

	glBindBuffer(GL_ARRAY_BUFFER, r_world_model->bsp->vertex_buffer);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, r_world_model->bsp->elements_buffer);
}

/**
 * @brief Draws the specified draw elements for the given entity.
 * @param entity The entity, or NULL for the world model.
 * @param draw The draw elements command.
 * @param material The currently bound material.
 */
static inline void R_DrawBspDrawElements(const r_view_t *view,
										 const r_entity_t *entity,
										 const r_bsp_draw_elements_t *draw,
										 const r_material_t **material) {

	if (!(draw->surface & SURF_MATERIAL)) {

		if (*material != draw->material) {
			*material = draw->material;

			glBindTexture(GL_TEXTURE_2D_ARRAY, (*material)->texture->texnum);

			glUniform1f(r_bsp_program.material.alpha_test, (*material)->cm->alpha_test * r_alpha_test->value);
			glUniform1f(r_bsp_program.material.roughness, (*material)->cm->roughness * r_roughness->value);
			glUniform1f(r_bsp_program.material.hardness, (*material)->cm->hardness * r_hardness->value);
			glUniform1f(r_bsp_program.material.specularity, (*material)->cm->specularity * r_specularity->value);
			glUniform1f(r_bsp_program.material.bloom, (*material)->cm->bloom * r_bloom->value);
		}

		glDrawElements(GL_TRIANGLES, draw->num_elements, GL_UNSIGNED_INT, draw->elements);
		r_stats.bsp_triangles += draw->num_elements / 3;

		R_GetError(draw->material->media.name);
	}

	R_DrawBspDrawElementsMaterialStages(view, entity, draw, draw->material);
}

/**
 * @brief Draws opaque draw elements for the specified inline model.
 */
static void R_DrawBspInlineOpaqueDrawElements(const r_view_t *view,
											  const r_entity_t *entity,
											  const r_bsp_inline_model_t *in) {

	const r_material_t *material = NULL;

	const r_bsp_draw_elements_t *draw = in->draw_elements;
	for (int32_t i = 0; i < in->num_draw_elements; i++, draw++) {

		if (draw->surface & SURF_MASK_TRANSLUCENT) {
			continue;
		}

		if (draw->surface & SURF_SKY) {
			continue;
		}

		R_DrawBspDrawElements(view, entity, draw, &material);

		r_stats.bsp_draw_elements++;
	}
}

/**
 * @brief Draws alpha test draw elements for the specified inline model.
 */
static void R_DrawBspInlineAlphaTestDrawElements(const r_view_t *view,
												 const r_entity_t *entity,
												 const r_bsp_inline_model_t *in) {

	const r_material_t *material = NULL;

	const r_bsp_draw_elements_t *draw = in->draw_elements;
	for (int32_t i = 0; i < in->num_draw_elements; i++, draw++) {

		if (!(draw->surface & SURF_ALPHA_TEST)) {
			continue;
		}
		
		R_DrawBspDrawElements(view, entity, draw, &material);

		r_stats.bsp_draw_elements++;
	}
}

/**
 * @brief Draws alpha blended faces for the specified inline model, ordered back to front.
 * @details In order to ensure correct blend ordering, sprites and mesh entities may be dispatched
 * here, to be drawn immediately behind (before) any draw elements that may occlude them.
 */
static void R_DrawBspInlineBlendDrawElements(const r_view_t *view,
											 const r_entity_t *entity,
											 const r_bsp_inline_model_t *in) {

	const r_material_t *material = NULL;

	for (guint i = 0; i < in->blend_elements->len; i++) {

		const r_bsp_draw_elements_t *draw = g_ptr_array_index(in->blend_elements, i);

		if (draw->blend_depth_types) {

			assert(entity == NULL);

			const int32_t blend_depth = (int32_t) (draw - r_world_model->bsp->draw_elements);

			R_DrawBlendDepthTypes(view, blend_depth, draw->blend_depth_types);

			material = NULL;
		}

		R_DrawBspDrawElements(view, entity, draw, &material);

		r_stats.bsp_draw_elements++;
		r_stats.bsp_blend_draw_elements++;
	}

#if 0
	const r_bsp_face_t *face = in->faces;
	for (int32_t i = 0; i < in->num_faces; i++, face++) {

		const r_material_t *material = face->brush_side->material;

		if (!(material->cm->surface & SURF_MASK_BLEND)) {
			continue;
		}

		glBindTexture(GL_TEXTURE_2D_ARRAY, material->texture->texnum);

		glUniform1f(r_bsp_program.material.alpha_test, material->cm->alpha_test * r_alpha_test->value);
		glUniform1f(r_bsp_program.material.roughness, material->cm->roughness * r_roughness->value);
		glUniform1f(r_bsp_program.material.hardness, material->cm->hardness * r_hardness->value);
		glUniform1f(r_bsp_program.material.specularity, material->cm->specularity * r_specularity->value);
		glUniform1f(r_bsp_program.material.bloom, material->cm->bloom* r_bloom->value);

		glDrawElements(GL_TRIANGLES, face->num_elements, GL_UNSIGNED_INT, face->elements);
	}
#endif

	R_GetError(NULL);
}

/**
 * @brief
 */
void R_UpdateBspInlineEntities(r_view_t *view) {

	r_entity_t *e = view->entities;
	for (int32_t i = 0; i < view->num_entities; i++, e++) {
		if (IS_BSP_INLINE_MODEL(e->model)) {
			const r_bsp_inline_model_t *in = e->model->bsp_inline;
			if (in->blend_elements->len) {
				e->blend_depth = R_BlendDepthForPoint(view, Box3_Center(e->abs_model_bounds), BLEND_DEPTH_ENTITY);
			} else {
				e->blend_depth = INT32_MIN;
			}
		}
	}
}

/**
 * @brief
 */
static void R_DrawBspInlineEntity(const r_view_t *view, const r_entity_t *e) {

	glUniformMatrix4fv(r_bsp_program.model, 1, GL_FALSE, e->matrix.array);

	R_DrawBspInlineOpaqueDrawElements(view, e, e->model->bsp_inline);

	R_DrawBspInlineAlphaTestDrawElements(view, e, e->model->bsp_inline);

	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	R_DrawBspInlineBlendDrawElements(view, e, e->model->bsp_inline);

	glBlendFunc(GL_ONE, GL_ZERO);
	glDisable(GL_BLEND);

	glUniformMatrix4fv(r_bsp_program.model, 1, GL_FALSE, Mat4_Identity().array);

	r_stats.bsp_inline_models++;
}

/**
 * @brief
 */
void R_DrawBspInlineEntities(const r_view_t *view, int32_t blend_depth) {

	if (view->type == VIEW_PLAYER_MODEL) {
		return;
	}
	
	glUseProgram(r_bsp_program.name);

	glBindVertexArray(r_world_model->bsp->vertex_array);

	glBindBuffer(GL_ARRAY_BUFFER, r_world_model->bsp->vertex_buffer);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, r_world_model->bsp->elements_buffer);

	glUniform1i(r_bsp_program.entity, 1);

	glEnable(GL_DEPTH_TEST);
	glEnable(GL_CULL_FACE);

	const r_entity_t *e = view->entities;
	for (int32_t i = 0; i < view->num_entities; i++, e++) {
		if (IS_BSP_INLINE_MODEL(e->model)) {

			if (e->effects & EF_NO_DRAW) {
				continue;
			}

			if (e->blend_depth != blend_depth) {
				continue;
			}

			if (R_OccludeBox(view, e->abs_model_bounds)) {
				continue;
			}

			R_DrawBspInlineEntity(view, e);
		}
	}

	glDisable(GL_CULL_FACE);
	glDisable(GL_DEPTH_TEST);

	glUniform1i(r_bsp_program.entity, 0);

	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

	glBindVertexArray(0);

	glUseProgram(0);

	R_GetError(NULL);
}

/**
 * @brief
 */
void R_DrawWorld(const r_view_t *view) {

	R_DrawSky(view);

	glUseProgram(r_bsp_program.name);

	glUniform1i(r_bsp_program.stage.flags, STAGE_MATERIAL);

	glBindVertexArray(r_world_model->bsp->vertex_array);

	glEnableVertexAttribArray(r_bsp_program.in_position);
	glEnableVertexAttribArray(r_bsp_program.in_normal);
	glEnableVertexAttribArray(r_bsp_program.in_tangent);
	glEnableVertexAttribArray(r_bsp_program.in_bitangent);
	glEnableVertexAttribArray(r_bsp_program.in_diffusemap);
	glEnableVertexAttribArray(r_bsp_program.in_lightmap);
	glEnableVertexAttribArray(r_bsp_program.in_color);

	glBindBuffer(GL_ARRAY_BUFFER, r_world_model->bsp->vertex_buffer);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, r_world_model->bsp->elements_buffer);

	for (bsp_lightgrid_texture_t i = BSP_LIGHTGRID_FIRST; i < BSP_LIGHTGRID_LAST; i++) {
		glActiveTexture(GL_TEXTURE0 + TEXTURE_LIGHTGRID + i);
		glBindTexture(GL_TEXTURE_3D, r_world_model->bsp->lightgrid->textures[i]->texnum);
	}

	glActiveTexture(GL_TEXTURE0 + TEXTURE_LIGHTMAP);
	glBindTexture(GL_TEXTURE_2D_ARRAY, r_world_model->bsp->lightmap->atlas->texnum);

	glActiveTexture(GL_TEXTURE0 + TEXTURE_WARP);
	glBindTexture(GL_TEXTURE_2D, r_bsp_program.warp_image->texnum);

	glActiveTexture(GL_TEXTURE0 + TEXTURE_MATERIAL);

	glEnable(GL_DEPTH_TEST);
	glEnable(GL_CULL_FACE);

	if (r_depth_pass->value) {
		glDepthMask(GL_FALSE);
	}

	glUniformMatrix4fv(r_bsp_program.model, 1, GL_FALSE, Mat4_Identity().array);

	R_DrawBspInlineOpaqueDrawElements(view, NULL, r_world_model->bsp->inline_models);

	if (r_depth_pass->value) {
		glDepthMask(GL_TRUE);
	}

	R_DrawBspInlineAlphaTestDrawElements(view, NULL, r_world_model->bsp->inline_models);

	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	R_DrawBlendDepthTypes(view, INT32_MIN, BLEND_DEPTH_ALL);

	R_DrawBspInlineBlendDrawElements(view, NULL, r_world_model->bsp->inline_models);

	R_DrawBlendDepthTypes(view, INT32_MAX, BLEND_DEPTH_ALL);

	glBlendFunc(GL_ONE, GL_ZERO);
	glDisable(GL_BLEND);

	glDisable(GL_DEPTH_TEST);
	glDisable(GL_CULL_FACE);

	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
	
	glBindVertexArray(0);

	glUseProgram(0);

	R_GetError(NULL);

	R_DrawBspNormals(view);
}

#define WARP_IMAGE_SIZE 16

/**
 * @brief
 */
void R_InitBspProgram(void) {

	memset(&r_bsp_program, 0, sizeof(r_bsp_program));

	r_bsp_program.name = R_LoadProgram(
			R_ShaderDescriptor(GL_VERTEX_SHADER, "lightgrid.glsl", "material.glsl", "bsp_vs.glsl", NULL),
		    R_ShaderDescriptor(GL_GEOMETRY_SHADER, "polylib.glsl", "bsp_gs.glsl", NULL),
			R_ShaderDescriptor(GL_FRAGMENT_SHADER, "lightgrid.glsl", "material.glsl", "bsp_fs.glsl", NULL),
			NULL);

	glUseProgram(r_bsp_program.name);

	r_bsp_program.uniforms_block = glGetUniformBlockIndex(r_bsp_program.name, "uniforms_block");
	glUniformBlockBinding(r_bsp_program.name, r_bsp_program.uniforms_block, 0);

	r_bsp_program.lights_block = glGetUniformBlockIndex(r_bsp_program.name, "lights_block");
	glUniformBlockBinding(r_bsp_program.name, r_bsp_program.lights_block, 1);

	r_bsp_program.in_position = glGetAttribLocation(r_bsp_program.name, "in_position");
	r_bsp_program.in_normal = glGetAttribLocation(r_bsp_program.name, "in_normal");
	r_bsp_program.in_tangent = glGetAttribLocation(r_bsp_program.name, "in_tangent");
	r_bsp_program.in_bitangent = glGetAttribLocation(r_bsp_program.name, "in_bitangent");
	r_bsp_program.in_diffusemap = glGetAttribLocation(r_bsp_program.name, "in_diffusemap");
	r_bsp_program.in_lightmap = glGetAttribLocation(r_bsp_program.name, "in_lightmap");
	r_bsp_program.in_color = glGetAttribLocation(r_bsp_program.name, "in_color");

	r_bsp_program.model = glGetUniformLocation(r_bsp_program.name, "model");

	r_bsp_program.texture_material = glGetUniformLocation(r_bsp_program.name, "texture_material");
	r_bsp_program.texture_lightmap = glGetUniformLocation(r_bsp_program.name, "texture_lightmap");
	r_bsp_program.texture_stage = glGetUniformLocation(r_bsp_program.name, "texture_stage");
	r_bsp_program.texture_warp = glGetUniformLocation(r_bsp_program.name, "texture_warp");
	r_bsp_program.texture_lightgrid_ambient = glGetUniformLocation(r_bsp_program.name, "texture_lightgrid_ambient");
	r_bsp_program.texture_lightgrid_diffuse = glGetUniformLocation(r_bsp_program.name, "texture_lightgrid_diffuse");
	r_bsp_program.texture_lightgrid_direction = glGetUniformLocation(r_bsp_program.name, "texture_lightgrid_direction");
	r_bsp_program.texture_lightgrid_caustics = glGetUniformLocation(r_bsp_program.name, "texture_lightgrid_caustics");
	r_bsp_program.texture_lightgrid_fog = glGetUniformLocation(r_bsp_program.name, "texture_lightgrid_fog");
	r_bsp_program.texture_shadowmap = glGetUniformLocation(r_bsp_program.name, "texture_shadowmap");
	r_bsp_program.texture_shadowmap_cube = glGetUniformLocation(r_bsp_program.name, "texture_shadowmap_cube");

	r_bsp_program.entity = glGetUniformLocation(r_bsp_program.name, "entity");

	r_bsp_program.material.alpha_test = glGetUniformLocation(r_bsp_program.name, "material.alpha_test");
	r_bsp_program.material.roughness = glGetUniformLocation(r_bsp_program.name, "material.roughness");
	r_bsp_program.material.hardness = glGetUniformLocation(r_bsp_program.name, "material.hardness");
	r_bsp_program.material.specularity = glGetUniformLocation(r_bsp_program.name, "material.specularity");
	r_bsp_program.material.bloom = glGetUniformLocation(r_bsp_program.name, "material.bloom");

	r_bsp_program.stage.flags = glGetUniformLocation(r_bsp_program.name, "stage.flags");
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

	glUniform1i(r_bsp_program.texture_material, TEXTURE_MATERIAL);
	glUniform1i(r_bsp_program.texture_lightmap, TEXTURE_LIGHTMAP);
	glUniform1i(r_bsp_program.texture_stage, TEXTURE_STAGE);
	glUniform1i(r_bsp_program.texture_warp, TEXTURE_WARP);
	glUniform1i(r_bsp_program.texture_lightgrid_ambient, TEXTURE_LIGHTGRID_AMBIENT);
	glUniform1i(r_bsp_program.texture_lightgrid_diffuse, TEXTURE_LIGHTGRID_DIFFUSE);
	glUniform1i(r_bsp_program.texture_lightgrid_direction, TEXTURE_LIGHTGRID_DIRECTION);
	glUniform1i(r_bsp_program.texture_lightgrid_caustics, TEXTURE_LIGHTGRID_CAUSTICS);
	glUniform1i(r_bsp_program.texture_lightgrid_fog, TEXTURE_LIGHTGRID_FOG);
	glUniform1i(r_bsp_program.texture_shadowmap, TEXTURE_SHADOWMAP);
	glUniform1i(r_bsp_program.texture_shadowmap_cube, TEXTURE_SHADOWMAP_CUBE);

	r_bsp_program.warp_image = (r_image_t *) R_AllocMedia("r_warp_image", sizeof(r_image_t), R_MEDIA_IMAGE);
	r_bsp_program.warp_image->media.Retain = R_RetainImage;
	r_bsp_program.warp_image->media.Free = R_FreeImage;

	r_bsp_program.warp_image->width = r_bsp_program.warp_image->height = WARP_IMAGE_SIZE;
	r_bsp_program.warp_image->type = IT_PROGRAM;
	r_bsp_program.warp_image->target = GL_TEXTURE_2D;
	r_bsp_program.warp_image->format = GL_RGBA;

	byte data[WARP_IMAGE_SIZE][WARP_IMAGE_SIZE][4];

	for (int32_t i = 0; i < WARP_IMAGE_SIZE; i++) {
		for (int32_t j = 0; j < WARP_IMAGE_SIZE; j++) {
			data[i][j][0] = RandomRangeu(0, 48);
			data[i][j][1] = RandomRangeu(0, 48);
			data[i][j][2] = 0;
			data[i][j][3] = 255;
		}
	}

	R_UploadImage(r_bsp_program.warp_image, GL_TEXTURE_2D, (byte *) data);

	R_GetError(NULL);
}

/**
 * @brief
 */
void R_ShutdownBspProgram(void) {

	glDeleteProgram(r_bsp_program.name);

	r_bsp_program.name = 0;
}
