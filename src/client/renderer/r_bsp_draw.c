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

#define MAX_ACTIVE_LIGHTS        10

#define TEXTURE_DIFFUSE           0
#define TEXTURE_NORMALMAP         1
#define TEXTURE_GLOSSMAP          2
#define TEXTURE_LIGHTMAP          3
#define TEXTURE_DELUXEMAP         4
#define TEXTURE_STAINMAP          5

#define TEXTURE_MASK_DIFFUSE     (1 << TEXTURE_DIFFUSE)
#define TEXTURE_MASK_NORMALMAP   (1 << TEXTURE_NORMALMAP)
#define TEXTURE_MASK_GLOSSMAP    (1 << TEXTURE_GLOSSMAP)
#define TEXTURE_MASK_LIGHTMAP    (1 << TEXTURE_LIGHTMAP)
#define TEXTURE_MASK_DELUXEMAP   (1 << TEXTURE_DELUXEMAP)
#define TEXTURE_MASK_STAINMAP    (1 << TEXTURE_STAINMAP)
#define TEXTURE_MASK_ALL          0xff

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
} prog;

/**
 * @brief
 */
static void R_DrawBspLeaf(const r_bsp_leaf_t *leaf) {

	glUniform1i(prog.contents, leaf->contents);

	const r_bsp_draw_elements_t *draw = leaf->draw_elements;
	for (int32_t i = 0; i < leaf->num_draw_elements; i++, draw++) {

		const r_material_t *material = draw->texinfo->material;

		GLint textures = TEXTURE_MASK_DIFFUSE;

		glActiveTexture(GL_TEXTURE0 + TEXTURE_DIFFUSE);
		glBindTexture(GL_TEXTURE_2D, material->diffuse->texnum);

		if (material->normalmap) {
			textures |= TEXTURE_MASK_NORMALMAP;

			glActiveTexture(GL_TEXTURE0 + TEXTURE_NORMALMAP);
			glBindTexture(GL_TEXTURE_2D, material->normalmap->texnum);
		}

		if (material->glossmap) {
			textures |= TEXTURE_MASK_GLOSSMAP;

			glActiveTexture(GL_TEXTURE0 + TEXTURE_GLOSSMAP);
			glBindTexture(GL_TEXTURE_2D, material->glossmap->texnum);
		}

		if (draw->lightmap) {
			textures |= TEXTURE_MASK_LIGHTMAP;
			textures |= TEXTURE_MASK_DELUXEMAP;
			textures |= TEXTURE_MASK_STAINMAP;

			glActiveTexture(GL_TEXTURE0 + TEXTURE_LIGHTMAP);
			glBindTexture(GL_TEXTURE_2D_ARRAY, draw->lightmap->atlas->texnum);
		}

		switch (r_draw_bsp_lightmaps->integer) {
			case 1:
				textures = TEXTURE_MASK_LIGHTMAP;
				break;
			case 2:
				textures = TEXTURE_MASK_DELUXEMAP;
				break;
		}

		glUniform1i(prog.textures, textures);

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

		const r_bsp_leaf_t *leaf = (r_bsp_leaf_t *) node;

		if (r_view.area_bits) { // check for door connected areas
			if (!(r_view.area_bits[leaf->area >> 3] & (1 << (leaf->area & 7)))) {
				return; // not visible
			}
		}

		R_DrawBspLeaf(leaf);
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

	glEnable(GL_CULL_FACE);
	glCullFace(GL_FRONT); // okay Quake?

	if (r_draw_wireframe->value) {
		glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
	} else {
		glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
	}

	glUseProgram(prog.name);

	glUniformMatrix4fv(prog.projection, 1, GL_FALSE, (GLfloat *) r_view.projection3D.m);
	glUniformMatrix4fv(prog.model_view, 1, GL_FALSE, (GLfloat *) r_view.model_view.m);
	glUniformMatrix4fv(prog.normal, 1, GL_FALSE, (GLfloat *) r_view.normal.m);

	glUniform4f(prog.color, 1.f, 1.f, 1.f, 1.f);
	glUniform1f(prog.alpha_threshold, 0.f);

	glUniform1f(prog.brightness, r_brightness->value);
	glUniform1f(prog.contrast, r_contrast->value);
	glUniform1f(prog.saturation, r_saturation->value);
	glUniform1f(prog.gamma, r_gamma->value);
	glUniform1f(prog.modulate, r_modulate->value);

	const r_bsp_model_t *bsp = R_WorldModel()->bsp;

	glBindVertexArray(bsp->vertex_array);

	glBindBuffer(GL_ARRAY_BUFFER, bsp->vertex_buffer);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, bsp->elements_buffer);

	glEnableVertexAttribArray(prog.in_position);
	glEnableVertexAttribArray(prog.in_normal);
	glEnableVertexAttribArray(prog.in_tangent);
	glEnableVertexAttribArray(prog.in_bitangent);
	glEnableVertexAttribArray(prog.in_diffuse);
	glEnableVertexAttribArray(prog.in_lightmap);
	glEnableVertexAttribArray(prog.in_color);

	R_GetError(NULL);
	
	R_DrawBspModel(bsp);

#if 0

	glUniform1i(r_bsp_program.textures, TEXTURE_MASK_DIFFUSE);

	for (int32_t i = 0; i < bsp->num_faces; i++) {

		const r_bsp_face_t *face = bsp->faces + i;
		const r_material_t *material = face->texinfo->material;

		glActiveTexture(GL_TEXTURE0 + TEXTURE_DIFFUSE);
		glBindTexture(GL_TEXTURE_2D, material->diffuse->texnum);

		glDrawElements(GL_TRIANGLES,
					   face->num_elements,
					   GL_UNSIGNED_INT,
					   (void *) (face->first_element * sizeof(GLuint)));
	}
#endif

	glActiveTexture(GL_TEXTURE0);

	glCullFace(GL_BACK);
	glDisable(GL_CULL_FACE);
	glDisable(GL_DEPTH_TEST);
}

/**
 * @brief
 */
void R_InitBspProgram(void) {

	memset(&prog, 0, sizeof(prog));

	prog.name = R_LoadProgram(
			&MakeShaderDescriptor(GL_VERTEX_SHADER, "bsp_vs.glsl"),
			&MakeShaderDescriptor(GL_FRAGMENT_SHADER, "color_filter.glsl", "bsp_fs.glsl"),
			NULL);

	glUseProgram(prog.name);

	prog.in_position = glGetAttribLocation(prog.name, "in_position");
	prog.in_normal = glGetAttribLocation(prog.name, "in_normal");
	prog.in_tangent = glGetAttribLocation(prog.name, "in_tangent");
	prog.in_bitangent = glGetAttribLocation(prog.name, "in_bitangent");
	prog.in_diffuse = glGetAttribLocation(prog.name, "in_diffuse");
	prog.in_lightmap = glGetAttribLocation(prog.name, "in_lightmap");
	prog.in_color = glGetAttribLocation(prog.name, "in_color");

	prog.projection = glGetUniformLocation(prog.name, "projection");
	prog.model_view = glGetUniformLocation(prog.name, "model_view");
	prog.normal = glGetUniformLocation(prog.name, "normal");

	prog.textures = glGetUniformLocation(prog.name, "textures");
	prog.texture_diffuse = glGetUniformLocation(prog.name, "texture_diffuse");
	prog.texture_normalmap = glGetUniformLocation(prog.name, "texture_normalmap");
	prog.texture_glossmap = glGetUniformLocation(prog.name, "texture_glossmap");
	prog.texture_lightmap = glGetUniformLocation(prog.name, "texture_lightmap");

	prog.contents = glGetUniformLocation(prog.name, "contents");

	prog.color = glGetUniformLocation(prog.name, "color");
	prog.alpha_threshold = glGetUniformLocation(prog.name, "alpha_threshold");

	prog.brightness = glGetUniformLocation(prog.name, "brightness");
	prog.contrast = glGetUniformLocation(prog.name, "contrast");
	prog.saturation = glGetUniformLocation(prog.name, "saturation");
	prog.gamma = glGetUniformLocation(prog.name, "gamma");
	prog.modulate = glGetUniformLocation(prog.name, "modulate");

	prog.bump = glGetUniformLocation(prog.name, "bump");
	prog.parallax = glGetUniformLocation(prog.name, "parallax");
	prog.hardness = glGetUniformLocation(prog.name, "hardness");
	prog.specular = glGetUniformLocation(prog.name, "specular");

	for (size_t i = 0; i < lengthof(prog.light_positions); i++) {
		prog.light_positions[i] = glGetUniformLocation(prog.name, va("light_positions[%zd]", i));
		prog.light_colors[i] = glGetUniformLocation(prog.name, va("light_colors[%zd]", i));
	}

	prog.fog_parameters = glGetUniformLocation(prog.name, "fog_parameters");
	prog.fog_color = glGetUniformLocation(prog.name, "fog_color");

	prog.caustics = glGetUniformLocation(prog.name, "caustics");

	glUniform1i(prog.texture_diffuse, TEXTURE_DIFFUSE);
	glUniform1i(prog.texture_normalmap, TEXTURE_NORMALMAP);
	glUniform1i(prog.texture_glossmap, TEXTURE_GLOSSMAP);
	glUniform1i(prog.texture_lightmap, TEXTURE_LIGHTMAP);

	glUniform4f(prog.color, 1.f, 1.f, 1.f, 1.f);

	R_GetError(NULL);
}

/**
 * @brief
 */
void R_ShutdownBspProgram(void) {

	glDeleteProgram(prog.name);

	prog.name = 0;
}
