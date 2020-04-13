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

#define TEXTURE_DIFFUSEMAP 0
#define TEXTURE_DEPTH_STENCIL_ATTACHMENT 1

/**
 * @brief The particle vertex structure.
 */
typedef struct {
	/**
	 * @brief The particle position. The W component is the particle size.
	 */
	vec4_t position;

	/**
	 * @brief The particle color.
	 */
	color32_t color;

	/**
	 * @brief The alpha blended node in which this particle should be rendered, or -1.
	 */
	int32_t node;

} r_particle_vertex_t;

/**
 * @brief
 */
typedef struct {

	r_particle_vertex_t particles[MAX_PARTICLES];

	GLuint vertex_array;
	GLuint vertex_buffer;

	r_image_t *diffusemap;

	_Bool dirty;

} r_particles_t;

static r_particles_t r_particles;

/**
 * @brief The draw program.
 */
static struct {
	GLuint name;

	GLint in_position;
	GLint in_color;
	GLint in_node;

	GLint projection;
	GLint view;

	GLint node;

	GLint depth_range;
	GLint pixels_per_radian;
	GLint inv_viewport_size;
	GLint transition_size;
	
	GLint texture_diffusemap;
	GLint texture_depth_stencil_attachment;

	GLint brightness;
	GLint contrast;
	GLint saturation;
	GLint gamma;

	GLint fog_parameters;
} r_particle_program;

/**
 * @brief Copies the specified particle into the view structure, provided it
 * passes a basic visibility test.
 */
void R_AddParticle(const r_particle_t *p) {

	if (r_view.num_particles == MAX_PARTICLES) {
		return;
	}

	if (R_CullSphere(p->origin, p->size)) {
		return;
	}

	r_particle_vertex_t *out = r_particles.particles + r_view.num_particles;

	out->position = Vec3_ToVec4(p->origin, p->size);
	out->color = Color_Color32(p->color);

	r_bsp_node_t *node = R_BlendNodeForPoint(p->origin);
	if (node) {
		out->node = (int32_t) (node - r_model_state.world->bsp->nodes);
		node->num_particles++;
	} else {
		out->node = -1;
	}

	r_particles.dirty = true;

	r_view.num_particles++;
}

/**
 * @brief
 */
void R_DrawParticles(const r_bsp_node_t *node) {

	glDepthMask(GL_FALSE);

	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE);

	glEnable(GL_DEPTH_TEST);
	glEnable(GL_PROGRAM_POINT_SIZE);

	glUseProgram(r_particle_program.name);

	glUniformMatrix4fv(r_particle_program.projection, 1, GL_FALSE, (GLfloat *) r_locals.projection3D.m);
	glUniformMatrix4fv(r_particle_program.view, 1, GL_FALSE, (GLfloat *) r_locals.view.m);
	
	glUniform1i(r_particle_program.node, node ? (int32_t) (node - r_model_state.world->bsp->nodes) : -1);

	glUniform1f(r_particle_program.pixels_per_radian, tanf(Radians(r_view.fov.y) / 2.0));
	glUniform2f(r_particle_program.depth_range, 1.0, MAX_WORLD_DIST);
	glUniform2f(r_particle_program.inv_viewport_size, 1.0 / r_context.drawable_width, 1.0 / r_context.drawable_height);
	glUniform1f(r_particle_program.transition_size, .0016f);

	glUniform1f(r_particle_program.brightness, r_brightness->value);
	glUniform1f(r_particle_program.contrast, r_contrast->value);
	glUniform1f(r_particle_program.saturation, r_saturation->value);
	glUniform1f(r_particle_program.gamma, r_gamma->value);

	glUniform3fv(r_particle_program.fog_parameters, 1, r_locals.fog_parameters.xyz);

	glBindVertexArray(r_particles.vertex_array);
	glBindBuffer(GL_ARRAY_BUFFER, r_particles.vertex_buffer);

	if (r_particles.dirty) {
		glBufferData(GL_ARRAY_BUFFER, r_view.num_particles * sizeof(r_particle_vertex_t), r_particles.particles, GL_DYNAMIC_DRAW);

		glEnableVertexAttribArray(r_particle_program.in_position);
		glEnableVertexAttribArray(r_particle_program.in_color);
		glEnableVertexAttribArray(r_particle_program.in_node);

		r_particles.dirty = false;
	}

	glActiveTexture(GL_TEXTURE0 + TEXTURE_DIFFUSEMAP);
	glBindTexture(GL_TEXTURE_2D, r_particles.diffusemap->texnum);
	
	glActiveTexture(GL_TEXTURE0 + TEXTURE_DEPTH_STENCIL_ATTACHMENT);
	glBindTexture(GL_TEXTURE_2D, r_context.depth_stencil_attachment);
	
	glActiveTexture(GL_TEXTURE0);

	glDrawArrays(GL_POINTS, 0, r_view.num_particles);
	
	glBindVertexArray(0);

	glDisable(GL_PROGRAM_POINT_SIZE);
	glDisable(GL_DEPTH_TEST);

	glBlendFunc(GL_ONE, GL_ZERO);
	glDisable(GL_BLEND);

	glDepthMask(GL_TRUE);

	R_GetError(NULL);
}

/**
 * @brief
 */
static void R_InitParticleProgram(void) {

	memset(&r_particle_program, 0, sizeof(r_particle_program));

	r_particle_program.name = R_LoadProgram(
			&MakeShaderDescriptor(GL_VERTEX_SHADER, "particle_vs.glsl"),
			&MakeShaderDescriptor(GL_GEOMETRY_SHADER, "particle_gs.glsl"),
			&MakeShaderDescriptor(GL_FRAGMENT_SHADER, "common_fs.glsl", "soften_fs.glsl", "particle_fs.glsl"),
			NULL);

	glUseProgram(r_particle_program.name);

	r_particle_program.in_position = glGetAttribLocation(r_particle_program.name, "in_position");
	r_particle_program.in_color = glGetAttribLocation(r_particle_program.name, "in_color");
	r_particle_program.in_node = glGetAttribLocation(r_particle_program.name, "in_node");

	r_particle_program.projection = glGetUniformLocation(r_particle_program.name, "projection");
	r_particle_program.view = glGetUniformLocation(r_particle_program.name, "view");

	r_particle_program.node = glGetUniformLocation(r_particle_program.name, "node");

	r_particle_program.pixels_per_radian = glGetUniformLocation(r_particle_program.name, "pixels_per_radian");
	r_particle_program.depth_range = glGetUniformLocation(r_particle_program.name, "depth_range");
	r_particle_program.inv_viewport_size = glGetUniformLocation(r_particle_program.name, "inv_viewport_size");
	r_particle_program.transition_size = glGetUniformLocation(r_particle_program.name, "transition_size");

	r_particle_program.texture_diffusemap = glGetUniformLocation(r_particle_program.name, "texture_diffusemap");
	r_particle_program.texture_depth_stencil_attachment = glGetUniformLocation(r_particle_program.name, "depth_stencil_attachment");

	r_particle_program.brightness = glGetUniformLocation(r_particle_program.name, "brightness");
	r_particle_program.contrast = glGetUniformLocation(r_particle_program.name, "contrast");
	r_particle_program.saturation = glGetUniformLocation(r_particle_program.name, "saturation");
	r_particle_program.gamma = glGetUniformLocation(r_particle_program.name, "gamma");

	r_particle_program.fog_parameters = glGetUniformLocation(r_particle_program.name, "fog_parameters");

	glUniform1i(r_particle_program.texture_diffusemap, TEXTURE_DIFFUSEMAP);
	glUniform1i(r_particle_program.texture_depth_stencil_attachment, TEXTURE_DEPTH_STENCIL_ATTACHMENT);

	glUseProgram(0);

	R_GetError(NULL);
}

/**
 * @brief
 */
void R_InitParticles(void) {

	memset(&r_particles, 0, sizeof(r_particles));

	glGenVertexArrays(1, &r_particles.vertex_array);
	glBindVertexArray(r_particles.vertex_array);

	glGenBuffers(1, &r_particles.vertex_buffer);
	glBindBuffer(GL_ARRAY_BUFFER, r_particles.vertex_buffer);

	glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, sizeof(r_particle_vertex_t), (void *) offsetof(r_particle_vertex_t, position));
	glVertexAttribPointer(1, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(r_particle_vertex_t), (void *) offsetof(r_particle_vertex_t, color));
	glVertexAttribIPointer(2, 1, GL_INT, sizeof(r_particle_vertex_t), (void *) offsetof(r_particle_vertex_t, node));
	
	glBindVertexArray(0);

	R_GetError(NULL);

	r_particles.diffusemap = R_LoadImage("particles/particle", IT_PROGRAM);
	r_particles.dirty = true;

	R_InitParticleProgram();
}

/**
 * @brief
 */
static void R_ShutdownParticleProgram(void) {

	glDeleteProgram(r_particle_program.name);

	r_particle_program.name = 0;
}

/**
 * @brief
 */
void R_ShutdownParticles(void) {

	glDeleteVertexArrays(1, &r_particles.vertex_array);
	glDeleteBuffers(1, &r_particles.vertex_buffer);

	R_ShutdownParticleProgram();
}
