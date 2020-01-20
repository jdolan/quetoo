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

#define MAX_ACTIVE_LIGHTS 10

typedef enum {
	BSP_TEXTURE_DIFFUSE    = 1 << 0,
	BSP_TEXTURE_NORMALMAP  = 1 << 1,
	BSP_TEXTURE_GLOSSMAP   = 1 << 2,

	BSP_TEXTURE_LIGHTMAP   = 1 << 3,
	BSP_TEXTURE_DELUXEMAP  = 1 << 4,
	BSP_TEXTURE_STAINMAP   = 1 << 5,

	BSP_TEXTURE_ALL        = 0xff
} r_bsp_program_texture_t;

typedef struct {
    GLuint name;

    struct {
        GLint in_position;
        GLint in_normal;
        GLint in_tangent;
        GLint in_bitangent;
        GLint in_diffuse;
        GLint in_lightmap;
		GLint in_color;
    } attributes;

    struct {
        GLint projection;
        GLint model_view;
        GLint normal;

		GLint textures;

        GLint texture_diffuse;
		GLint texture_normalmap;
		GLint texture_glossmap;
        GLint texture_lightmap;

		GLint contents;

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

		GLint light_positions[MAX_ACTIVE_LIGHTS];
		GLint light_colors[MAX_ACTIVE_LIGHTS];

		GLint fog_parameters;
		GLint fog_color;

        GLint caustics;
    } uniforms;
} r_bsp_program_t;

static r_bsp_program_t r_bsp_program;
static r_bsp_program_t *p = &r_bsp_program;

/**
 * @brief
 */
static void R_DrawBspLeaf(const r_bsp_leaf_t *leaf) {

	if (r_view.area_bits) { // check for door connected areas
		if (!(r_view.area_bits[leaf->area >> 3] & (1 << (leaf->area & 7)))) {
			return; // not visible
		}
	}

	glUniform1i(p->uniforms.contents, leaf->contents);

	const r_bsp_draw_elements_t *draw = leaf->draw_elements;
	for (int32_t i = 0; i < leaf->num_draw_elements; i++, draw++) {

		const r_material_t *material = draw->texinfo->material;

		r_bsp_program_texture_t textures = BSP_TEXTURE_DIFFUSE;

		glActiveTexture(GL_TEXTURE0 + BSP_TEXTURE_DIFFUSE);
		glBindTexture(GL_TEXTURE_2D, material->diffuse->texnum);

		if (material->normalmap) {
			textures |= BSP_TEXTURE_NORMALMAP;

			glActiveTexture(GL_TEXTURE0 + BSP_TEXTURE_NORMALMAP);
			glBindTexture(GL_TEXTURE_2D, material->normalmap->texnum);
		}

		if (material->glossmap) {
			textures |= BSP_TEXTURE_GLOSSMAP;

			glActiveTexture(GL_TEXTURE0 + BSP_TEXTURE_GLOSSMAP);
			glBindTexture(GL_TEXTURE_2D, material->glossmap->texnum);
		}

		if (draw->lightmap) {
			textures |= BSP_TEXTURE_LIGHTMAP;
			textures |= BSP_TEXTURE_DELUXEMAP;
			textures |= BSP_TEXTURE_STAINMAP;

			glActiveTexture(GL_TEXTURE0 + BSP_TEXTURE_LIGHTMAP);
			glBindTexture(GL_TEXTURE_2D_ARRAY, draw->lightmap->atlas->texnum);
		}

		switch (r_draw_bsp_lightmaps->integer) {
			case 1:
				textures = BSP_TEXTURE_LIGHTMAP;
				break;
			case 2:
				textures = BSP_TEXTURE_DELUXEMAP;
				break;
		}

		glUniform1i(p->uniforms.textures, textures);

		glDrawElements(GL_TRIANGLES,
					   draw->num_elements,
					   GL_UNSIGNED_INT,
					   (void *) (draw->first_element * sizeof(GLuint)));
	}
}

/**
 * @brief
 */
static void R_DrawBspNode(const r_bsp_node_t *node) {

	if (node->contents == CONTENTS_SOLID) {
		return; // solid
	}

	if (node->vis_frame != r_locals.vis_frame) {
		return; // not in pvs
	}

	if (R_CullBox(node->mins, node->maxs)) {
		return; // culled out
	}

	// draw leafs, or recurse nodes
	if (node->contents != CONTENTS_NODE) {
		R_DrawBspLeaf((r_bsp_leaf_t *) node);
	} else {

		const vec_t dist = Cm_DistanceToPlane(r_view.origin, node->plane);

		int32_t side;
		if (dist > SIDE_EPSILON) {
			side = 0;
		} else {
			side = 1;
		}

		R_DrawBspNode(node->children[side]);

		R_DrawBspNode(node->children[!side]);
	}
}

/**
 * @brief
 */
static void R_DrawBspModel(const r_bsp_model_t *model) {

	// model matrix?

	R_DrawBspNode(model->nodes);
}

/**
 * @brief
 */
void R_DrawWorld(void) {

	glEnable(GL_DEPTH_TEST);
	glDisable(GL_CULL_FACE);

	if (r_draw_wireframe->value) {
		glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
	} else {
		glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
	}

	glUseProgram(p->name);

	glUniformMatrix4fv(p->uniforms.projection, 1, GL_FALSE, (GLfloat *) r_view.projection.m);
	glUniformMatrix4fv(p->uniforms.model_view, 1, GL_FALSE, (GLfloat *) r_view.model_view.m);
	glUniformMatrix4fv(p->uniforms.normal, 1, GL_FALSE, (GLfloat *) r_view.normal.m);

	glUniform1i(p->uniforms.textures, BSP_TEXTURE_ALL);

	glUniform4f(p->uniforms.color, 1.f, 1.f, 1.f, 1.f);
	glUniform1f(p->uniforms.alpha_threshold, 0.f);

	glUniform1f(p->uniforms.brightness, r_brightness->value);
	glUniform1f(p->uniforms.contrast, r_contrast->value);
	glUniform1f(p->uniforms.saturation, r_saturation->value);
	glUniform1f(p->uniforms.gamma, r_gamma->value);
	glUniform1f(p->uniforms.modulate, r_modulate->value);

	const r_bsp_model_t *bsp = R_WorldModel()->bsp;

	glBindVertexArray(bsp->vertex_array);

	glBindBuffer(GL_ARRAY_BUFFER, bsp->vertex_buffer);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, bsp->elements_buffer);

	glEnableVertexAttribArray(p->attributes.in_position);
	glEnableVertexAttribArray(p->attributes.in_normal);
	glEnableVertexAttribArray(p->attributes.in_tangent);
	glEnableVertexAttribArray(p->attributes.in_bitangent);
	glEnableVertexAttribArray(p->attributes.in_diffuse);
	glEnableVertexAttribArray(p->attributes.in_lightmap);
	glEnableVertexAttribArray(p->attributes.in_color);

	R_GetError(NULL);
	
	R_DrawBspModel(bsp);

#if 0
	for (int32_t i = 0; i < bsp->num_faces; i++) {

		const r_bsp_face_t *face = bsp->faces + i;
		const r_material_t *material = face->texinfo->material;

		r_bsp_program_texture_t textures = BSP_TEXTURE_DIFFUSE;

		glActiveTexture(GL_TEXTURE0 + BSP_TEXTURE_DIFFUSE);
		glBindTexture(GL_TEXTURE_2D, material->diffuse->texnum);

		glUniform1i(p->uniforms.textures, textures);

		glDrawElements(GL_TRIANGLES,
					   face->num_elements,
					   GL_UNSIGNED_INT,
					   (void *) (face->first_element * sizeof(GLuint)));
	}
#endif
}

/**
 * @brief
 */
void R_InitBspProgram(void) {

	memset(p, 0, sizeof(*p));

	p->name = R_LoadProgram(
		&MakeShaderDescriptor(GL_VERTEX_SHADER, "bsp_vs.glsl"),
		&MakeShaderDescriptor(GL_FRAGMENT_SHADER, "color_filter.glsl", "bsp_fs.glsl"),
		NULL);

	glUseProgram(p->name);

	GetAttributeLocation(p, in_position);
	GetAttributeLocation(p, in_normal);
	GetAttributeLocation(p, in_tangent);
	GetAttributeLocation(p, in_bitangent);
	GetAttributeLocation(p, in_diffuse);
	GetAttributeLocation(p, in_lightmap);
	GetAttributeLocation(p, in_color);

	GetUniformLocation(p, projection);
	GetUniformLocation(p, model_view);
	GetUniformLocation(p, normal);

	GetUniformLocation(p, textures);
	GetUniformLocation(p, texture_diffuse);
	GetUniformLocation(p, texture_normalmap);
	GetUniformLocation(p, texture_glossmap);
	GetUniformLocation(p, texture_lightmap);

	GetUniformLocation(p, contents);

	GetUniformLocation(p, color);
	GetUniformLocation(p, alpha_threshold);

	GetUniformLocation(p, brightness);
	GetUniformLocation(p, contrast);
	GetUniformLocation(p, saturation);
	GetUniformLocation(p, gamma);
	GetUniformLocation(p, modulate);

	GetUniformLocation(p, bump);
	GetUniformLocation(p, parallax);
	GetUniformLocation(p, hardness);
	GetUniformLocation(p, specular);

	GetUniformLocation(p, light_positions[0]);
	GetUniformLocation(p, light_positions[1]);
	GetUniformLocation(p, light_positions[2]);
	GetUniformLocation(p, light_positions[3]);
	GetUniformLocation(p, light_positions[4]);
	GetUniformLocation(p, light_positions[5]);
	GetUniformLocation(p, light_positions[6]);
	GetUniformLocation(p, light_positions[7]);
	GetUniformLocation(p, light_positions[8]);
	GetUniformLocation(p, light_positions[9]);

	GetUniformLocation(p, light_colors[0]);
	GetUniformLocation(p, light_colors[1]);
	GetUniformLocation(p, light_colors[2]);
	GetUniformLocation(p, light_colors[3]);
	GetUniformLocation(p, light_colors[4]);
	GetUniformLocation(p, light_colors[5]);
	GetUniformLocation(p, light_colors[6]);
	GetUniformLocation(p, light_colors[7]);
	GetUniformLocation(p, light_colors[8]);
	GetUniformLocation(p, light_colors[9]);

	GetUniformLocation(p, fog_parameters);
	GetUniformLocation(p, fog_color);

	GetUniformLocation(p, caustics);

	R_GetError(NULL);

	glUniform1i(p->uniforms.texture_diffuse, BSP_TEXTURE_DIFFUSE);
	glUniform1i(p->uniforms.texture_normalmap, BSP_TEXTURE_NORMALMAP);
	glUniform1i(p->uniforms.texture_glossmap, BSP_TEXTURE_GLOSSMAP);
	glUniform1i(p->uniforms.texture_lightmap, BSP_TEXTURE_LIGHTMAP);

	glUniform4f(p->uniforms.color, 1.f, 1.f, 1.f, 1.f);

	R_GetError(NULL);
}

/**
 * @brief
 */
void R_ShutdownBspProgram(void) {

	glDeleteProgram(r_bsp_program.name);

	memset(&r_bsp_program, 0, sizeof(r_bsp_program));
}
