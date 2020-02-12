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

#define TEXTURE_DIFFUSE                  0
#define TEXTURE_NORMALMAP                1
#define TEXTURE_GLOSSMAP                 2
#define TEXTURE_LIGHTMAP                 3
#define TEXTURE_STAINMAP                 4

#define TEXTURE_MASK_DIFFUSE            (1 << TEXTURE_DIFFUSE)
#define TEXTURE_MASK_NORMALMAP          (1 << TEXTURE_NORMALMAP)
#define TEXTURE_MASK_GLOSSMAP           (1 << TEXTURE_GLOSSMAP)
#define TEXTURE_MASK_LIGHTMAP           (1 << TEXTURE_LIGHTMAP)
#define TEXTURE_MASK_STAINMAP           (1 << TEXTURE_STAINMAP)
#define TEXTURE_MASK_ALL                0xff

/**
 * @brief The program.
 */
static struct {
	GLuint name;

	GLint in_position;
	GLint in_normal;
	GLint in_tangent;
	GLint in_bitangent;
	GLint in_diffuse;
	GLint in_lightmap;
	GLint in_color;

	GLint projection;
	GLint view;
	GLint model;
	GLint normal;

	GLint textures;

	GLint texture_diffuse;
	GLint texture_normalmap;
	GLint texture_glossmap;
	GLint texture_lightmap;

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

	GLint lights_block;
	GLuint lights_buffer;

	GLint fog_parameters;
	GLint fog_color;

	GLint caustics;
} r_bsp_program;

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
					.origin = { s + 0.5, t + 0.5, u + 0.5 },
					.size = BSP_LIGHTGRID_LUXEL_SIZE,
					.color = { MIN(r, 255), MIN(g, 255), MIN(b, 255), 255 },
				};

				p.origin = vec3_add(r_model_state.world->bsp->lightgrid->mins, vec3_scale(p.origin, BSP_LIGHTGRID_LUXEL_SIZE));

				R_AddParticle(&p);
			}
		}
	}
}

/**
 * @brief
 */
static void R_DrawBspDrawElements(const r_bsp_inline_model_t *in) {

	const r_bsp_model_t *bsp = r_model_state.world->bsp;
	assert(bsp);

	const r_material_t *material = NULL;
	GLint textures = 0;

	r_bsp_draw_elements_t **sorted = bsp->draw_elements_sorted;
	for (int32_t i = 0; i < bsp->num_draw_elements; i++, sorted++) {

		const r_bsp_draw_elements_t *draw = *sorted;

		if (draw->node->model != in) {
			continue;
		}

		if (draw->node->vis_frame != r_locals.vis_frame) {
			continue;
		}

		if (draw->texinfo->flags & SURF_SKY) {
			continue;
		}

		if (draw->texinfo->flags & SURF_MATERIAL) {
			continue;
		}

		if (draw->texinfo->flags & (SURF_BLEND_33 | SURF_BLEND_66)) {
			continue;
		}

		GLint tex = textures;

		if (draw->texinfo->material != material) {
			material = draw->texinfo->material;

			tex |= TEXTURE_MASK_DIFFUSE;
			glActiveTexture(GL_TEXTURE0 + TEXTURE_DIFFUSE);
			glBindTexture(GL_TEXTURE_2D, material->diffuse->texnum);

			if (material->normalmap) {
				tex |= TEXTURE_MASK_NORMALMAP;
				glActiveTexture(GL_TEXTURE0 + TEXTURE_NORMALMAP);
				glBindTexture(GL_TEXTURE_2D, material->normalmap->texnum);
			} else {
				tex &= ~TEXTURE_MASK_NORMALMAP;
			}

			if (material->glossmap) {
				tex |= TEXTURE_MASK_GLOSSMAP;
				glActiveTexture(GL_TEXTURE0 + TEXTURE_GLOSSMAP);
				glBindTexture(GL_TEXTURE_2D, material->glossmap->texnum);
			} else {
				tex &= ~TEXTURE_MASK_GLOSSMAP;
			}

			glUniform1f(r_bsp_program.bump, material->cm->bump);
			glUniform1f(r_bsp_program.parallax, material->cm->parallax);
			glUniform1f(r_bsp_program.hardness, material->cm->hardness);
			glUniform1f(r_bsp_program.specular, material->cm->specular);
		}

		if (bsp->lightmap) {
			tex |= TEXTURE_MASK_LIGHTMAP;
		} else {
			tex &= ~TEXTURE_MASK_LIGHTMAP;
		}

		if (tex != textures) {
			textures = tex;
			glUniform1i(r_bsp_program.textures, textures);
		}

		glDrawElements(GL_TRIANGLES, draw->num_elements, GL_UNSIGNED_INT, draw->elements);

		r_view.count_bsp_draw_elements++;
	}
}

/**
 * @brief
 */
static void R_DrawBspEntity(const r_entity_t *e) {

	glUniformMatrix4fv(r_bsp_program.model, 1, GL_FALSE, (GLfloat *) e->matrix.m);

	mat4_t normal;
	Matrix4x4_Concat(&normal, &r_locals.view, &e->matrix);
	Matrix4x4_Invert_Full(&normal, &normal);
	Matrix4x4_Transpose(&normal, &normal);

	glUniformMatrix4fv(r_bsp_program.normal, 1, GL_FALSE, (GLfloat *) normal.m);

	R_DrawBspDrawElements(e->model->bsp_inline);
}

/**
 * @brief
 */
void R_DrawWorld(void) {

	glEnable(GL_DEPTH_TEST);

	glEnable(GL_CULL_FACE);
	glCullFace(GL_FRONT);

	if (r_draw_wireframe->value) {
		glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
	}

	glUseProgram(r_bsp_program.name);

	glUniformMatrix4fv(r_bsp_program.projection, 1, GL_FALSE, (GLfloat *) r_locals.projection3D.m);
	glUniformMatrix4fv(r_bsp_program.view, 1, GL_FALSE, (GLfloat *) r_locals.view.m);
	glUniformMatrix4fv(r_bsp_program.model, 1, GL_FALSE, (GLfloat *) matrix4x4_identity.m);

	mat4_t normal;
	Matrix4x4_Invert_Full(&normal, &r_locals.view);
	Matrix4x4_Transpose(&normal, &normal);

	glUniformMatrix4fv(r_bsp_program.normal, 1, GL_FALSE, (GLfloat *) normal.m);

	glUniform1f(r_bsp_program.alpha_threshold, 0.f);

	glUniform1f(r_bsp_program.brightness, r_brightness->value);
	glUniform1f(r_bsp_program.contrast, r_contrast->value);
	glUniform1f(r_bsp_program.saturation, r_saturation->value);
	glUniform1f(r_bsp_program.gamma, r_gamma->value);
	glUniform1f(r_bsp_program.modulate, r_modulate->value);

	glBindBuffer(GL_UNIFORM_BUFFER, r_bsp_program.lights_buffer);
	glBufferData(GL_UNIFORM_BUFFER, sizeof(r_locals.view_lights), r_locals.view_lights, GL_DYNAMIC_DRAW);
	glBindBufferBase(GL_UNIFORM_BUFFER, 0, r_bsp_program.lights_buffer);

	const r_bsp_model_t *bsp = R_WorldModel()->bsp;

	glBindVertexArray(bsp->vertex_array);

	glBindBuffer(GL_ARRAY_BUFFER, bsp->vertex_buffer);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, bsp->elements_buffer);

	glEnableVertexAttribArray(r_bsp_program.in_position);
	glEnableVertexAttribArray(r_bsp_program.in_normal);
	glEnableVertexAttribArray(r_bsp_program.in_tangent);
	glEnableVertexAttribArray(r_bsp_program.in_bitangent);
	glEnableVertexAttribArray(r_bsp_program.in_diffuse);
	glEnableVertexAttribArray(r_bsp_program.in_lightmap);
	glEnableVertexAttribArray(r_bsp_program.in_color);

	if (bsp->lightmap) {
		glActiveTexture(GL_TEXTURE0 + TEXTURE_LIGHTMAP);
		glBindTexture(GL_TEXTURE_2D_ARRAY, bsp->lightmap->atlas->texnum);
	}

	R_DrawBspDrawElements(bsp->inline_models);

	const r_entity_t *e = r_view.entities;
	for (int32_t i = 0; i < r_view.num_entities; i++, e++) {
		if (e->model && e->model->type == MOD_BSP_INLINE) {
			R_DrawBspEntity(e);
		}
	}

	glActiveTexture(GL_TEXTURE0);

	glUseProgram(0);

	if (r_draw_wireframe->value) {
		glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
	}

	glCullFace(GL_FRONT);
	glDisable(GL_CULL_FACE);

	glDisable(GL_DEPTH_TEST);

	R_GetError(NULL);

	R_DrawBspLightgrid();
}

/**
 * @brief
 */
void R_InitBspProgram(void) {

	memset(&r_bsp_program, 0, sizeof(r_bsp_program));

	r_bsp_program.name = R_LoadProgram(
			&MakeShaderDescriptor(GL_VERTEX_SHADER, "bsp_vs.glsl"),
			&MakeShaderDescriptor(GL_FRAGMENT_SHADER, "color_filter.glsl", "lights.glsl", "bsp_fs.glsl"),
			NULL);

	glUseProgram(r_bsp_program.name);

	r_bsp_program.in_position = glGetAttribLocation(r_bsp_program.name, "in_position");
	r_bsp_program.in_normal = glGetAttribLocation(r_bsp_program.name, "in_normal");
	r_bsp_program.in_tangent = glGetAttribLocation(r_bsp_program.name, "in_tangent");
	r_bsp_program.in_bitangent = glGetAttribLocation(r_bsp_program.name, "in_bitangent");
	r_bsp_program.in_diffuse = glGetAttribLocation(r_bsp_program.name, "in_diffuse");
	r_bsp_program.in_lightmap = glGetAttribLocation(r_bsp_program.name, "in_lightmap");
	r_bsp_program.in_color = glGetAttribLocation(r_bsp_program.name, "in_color");

	r_bsp_program.projection = glGetUniformLocation(r_bsp_program.name, "projection");
	r_bsp_program.view = glGetUniformLocation(r_bsp_program.name, "view");
	r_bsp_program.model = glGetUniformLocation(r_bsp_program.name, "model");
	r_bsp_program.normal = glGetUniformLocation(r_bsp_program.name, "normal");

	r_bsp_program.textures = glGetUniformLocation(r_bsp_program.name, "textures");
	r_bsp_program.texture_diffuse = glGetUniformLocation(r_bsp_program.name, "texture_diffuse");
	r_bsp_program.texture_normalmap = glGetUniformLocation(r_bsp_program.name, "texture_normalmap");
	r_bsp_program.texture_glossmap = glGetUniformLocation(r_bsp_program.name, "texture_glossmap");
	r_bsp_program.texture_lightmap = glGetUniformLocation(r_bsp_program.name, "texture_lightmap");

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

	r_bsp_program.lights_block = glGetUniformBlockIndex(r_bsp_program.name, "lights_block");
	glUniformBlockBinding(r_bsp_program.name, r_bsp_program.lights_block, 0);
	glGenBuffers(1, &r_bsp_program.lights_buffer);

	r_bsp_program.fog_parameters = glGetUniformLocation(r_bsp_program.name, "fog_parameters");
	r_bsp_program.fog_color = glGetUniformLocation(r_bsp_program.name, "fog_color");

	r_bsp_program.caustics = glGetUniformLocation(r_bsp_program.name, "caustics");

	glUniform1i(r_bsp_program.texture_diffuse, TEXTURE_DIFFUSE);
	glUniform1i(r_bsp_program.texture_normalmap, TEXTURE_NORMALMAP);
	glUniform1i(r_bsp_program.texture_glossmap, TEXTURE_GLOSSMAP);
	glUniform1i(r_bsp_program.texture_lightmap, TEXTURE_LIGHTMAP);

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
