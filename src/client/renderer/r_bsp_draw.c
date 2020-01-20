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
