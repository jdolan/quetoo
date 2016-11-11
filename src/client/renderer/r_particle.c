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
 * @brief Copies the specified particle into the view structure, provided it
 * passes a basic visibility test.
 */
void R_AddParticle(const r_particle_t *p) {
	static r_element_t e;

	if (r_view.num_particles == lengthof(r_view.particles)) {
		return;
	}

	r_view.particles[r_view.num_particles++] = *p;

	e.type = ELEMENT_PARTICLE;

	e.element = (const void *) p;
	e.origin = (const vec_t *) p->org;

	R_AddElement(&e);
}

typedef struct {
	vec3_t vertex;
	vec2_t texcoord;
	u8vec4_t color;
} r_particle_interleave_vertex_t;

r_buffer_layout_t r_particle_buffer_layout[] = {
	{ .attribute = R_ARRAY_POSITION, .type = GL_FLOAT, .count = 3, .size = sizeof(vec3_t), .offset = 0 },
	{ .attribute = R_ARRAY_DIFFUSE, .type = GL_FLOAT, .count = 2, .size = sizeof(vec2_t), .offset = 12 },
	{ .attribute = R_ARRAY_COLOR, .type = GL_UNSIGNED_BYTE, .count = 4, .size = sizeof(u8vec4_t), .offset = 20 },
	{ .attribute = -1 }
};

/**
 * @brief Pools commonly used angular vectors for particle calculations and
 * accumulates particle primitives each frame.
 */
typedef struct {
	vec3_t weather_right;
	vec3_t weather_up;
	vec3_t splash_right[2];
	vec3_t splash_up[2];

	r_particle_interleave_vertex_t verts[MAX_PARTICLES * 4];
	r_buffer_t verts_buffer;

	GLuint elements[MAX_PARTICLES * 6];
	uint32_t num_particles;

	r_buffer_t element_buffer;
} r_particle_state_t;

static r_particle_state_t r_particle_state;

/**
 * @brief
 */
void R_InitParticles(void) {

	R_CreateInterleaveBuffer(&r_particle_state.verts_buffer, sizeof(r_particle_interleave_vertex_t),
	                         r_particle_buffer_layout, GL_DYNAMIC_DRAW, sizeof(r_particle_state.verts), NULL);

	R_CreateElementBuffer(&r_particle_state.element_buffer, GL_UNSIGNED_INT, GL_DYNAMIC_DRAW,
	                      sizeof(r_particle_state.elements), NULL);
}

/**
 * @brief
 */
void R_ShutdownParticles(void) {

	R_DestroyBuffer(&r_particle_state.verts_buffer);

	R_DestroyBuffer(&r_particle_state.element_buffer);
}

/**
 * @brief Generates the vertex coordinates for the specified particle.
 */
static void R_ParticleVerts(const r_particle_t *p, r_particle_interleave_vertex_t *verts) {
	vec3_t v, up, right, up_right, down_right;

	if (p->type == PARTICLE_BEAM || p->type == PARTICLE_SPARK) { // beams are lines with starts and ends
		VectorSubtract(p->org, p->end, v);
		VectorNormalize(v);
		VectorCopy(v, up);

		VectorSubtract(r_view.origin, p->end, v);
		CrossProduct(up, v, right);
		VectorNormalize(right);
		VectorScale(right, p->scale, right);

		VectorAdd(p->org, right, verts[0].vertex);
		VectorAdd(p->end, right, verts[1].vertex);
		VectorSubtract(p->end, right, verts[2].vertex);
		VectorSubtract(p->org, right, verts[3].vertex);
		return;
	}

	if (p->type == PARTICLE_DECAL) { // decals are aligned with surfaces
		AngleVectors(p->dir, NULL, right, up);

		VectorAdd(up, right, verts[0].vertex);
		VectorSubtract(right, up, verts[1].vertex);
		VectorNegate(verts[0].vertex, verts[2].vertex);
		VectorNegate(verts[1].vertex, verts[3].vertex);

		VectorMA(p->org, p->scale, verts[0].vertex, verts[0].vertex);
		VectorMA(p->org, p->scale, verts[1].vertex, verts[1].vertex);
		VectorMA(p->org, p->scale, verts[2].vertex, verts[2].vertex);
		VectorMA(p->org, p->scale, verts[3].vertex, verts[3].vertex);
		return;
	}

	// all other particles are aligned with the client's view

	if (p->type == PARTICLE_WEATHER) { // keep it vertical
		VectorScale(r_particle_state.weather_right, p->scale, right);
		VectorScale(r_particle_state.weather_up, p->scale, up);
	} else if (p->type == PARTICLE_SPLASH) { // keep it horizontal

		if (p->org[2] > r_view.origin[2]) { // it's above us
			VectorScale(r_particle_state.splash_right[0], p->scale, right);
			VectorScale(r_particle_state.splash_up[0], p->scale, up);
		} else { // it's below us
			VectorScale(r_particle_state.splash_right[1], p->scale, right);
			VectorScale(r_particle_state.splash_up[1], p->scale, up);
		}
	} else if (p->type == PARTICLE_ROLL) { // roll it
		vec3_t dir;

		VectorCopy(r_view.angles, dir);
		dir[2] = p->roll * r_view.time / 1000.0;

		AngleVectors(dir, NULL, right, up);

		VectorScale(right, p->scale, right);
		VectorScale(up, p->scale, up);
	} else { // default particle alignment with view
		VectorScale(r_view.right, p->scale, right);
		VectorScale(r_view.up, p->scale, up);
	}

	VectorAdd(up, right, up_right);
	VectorSubtract(right, up, down_right);

	VectorSubtract(p->org, down_right, verts[0].vertex);
	VectorAdd(p->org, up_right, verts[1].vertex);
	VectorAdd(p->org, down_right, verts[2].vertex);
	VectorSubtract(p->org, up_right, verts[3].vertex);
}

/**
 * @brief Generates texture coordinates for the specified particle.
 */
static void R_ParticleTexcoords(const r_particle_t *p, r_particle_interleave_vertex_t *verts) {
	vec_t s, t;

	_Bool is_atlas = p->image && p->image->type == IT_ATLAS_IMAGE;

	if (!p->image ||
	        (!p->scroll_s && !p->scroll_t &&!is_atlas) ||
	        p->type == PARTICLE_CORONA) {

		for (int32_t i = 0; i < 4; ++i) {
			Vector2Copy(default_texcoords[i], verts[i].texcoord);
		}

		return;
	}

	// atlas needs a different pipeline
	if (is_atlas) {
		const r_atlas_image_t *atlas_image = (const r_atlas_image_t *) p->image;

		Vector2Set(verts[0].texcoord, atlas_image->texcoords[0], atlas_image->texcoords[1]);
		Vector2Set(verts[1].texcoord, atlas_image->texcoords[2], atlas_image->texcoords[1]);
		Vector2Set(verts[2].texcoord, atlas_image->texcoords[2], atlas_image->texcoords[3]);
		Vector2Set(verts[3].texcoord, atlas_image->texcoords[0], atlas_image->texcoords[3]);
	} else {
		s = p->scroll_s * r_view.time / 1000.0;
		t = p->scroll_t *r_view.time / 1000.0;

		Vector2Set(verts[0].texcoord, s, t);
		Vector2Set(verts[1].texcoord, 1.0 + s, t);
		Vector2Set(verts[2].texcoord, 1.0 + s, 1.0 + t);
		Vector2Set(verts[3].texcoord, s, 1.0 + t);
	}
}

/**
 * @brief Generates vertex colors for the specified particle.
 */
static void R_ParticleColor(const r_particle_t *p, r_particle_interleave_vertex_t *verts) {

	for (int32_t i = 0; i < 4; i++) {
		ColorDecompose(p->color, verts[i].color);
	}
}

/**
 * @brief Updates the shared angular vectors required for particle generation.
 */
void R_UpdateParticleState(void) {
	vec3_t v;

	// reset the common angular vectors for particle alignment
	VectorCopy(r_view.angles, v);

	v[0] = 0.0; // keep weather particles vertical by removing pitch
	AngleVectors(v, NULL, r_particle_state.weather_right, r_particle_state.weather_up);

	v[0] = -90.0; // and splash particles horizontal by setting it
	AngleVectors(v, NULL, r_particle_state.splash_right[0], r_particle_state.splash_up[0]);

	v[0] = 90.0; // even if they are below us
	AngleVectors(v, NULL, r_particle_state.splash_right[1], r_particle_state.splash_up[1]);
}

/**
 * @brief Generates primitives for the specified particle elements. Each
 * particle's index into the shared array is written to the element's data
 * field.
 */
void R_UpdateParticles(r_element_t *e, const size_t count) {
	size_t i;

	for (i = 0; i < count; i++, e++) {

		if (e->type == ELEMENT_PARTICLE) {
			r_particle_t *p = (r_particle_t *) e->element;

			const uint32_t vertex_start = r_particle_state.num_particles * 4;

			R_ParticleVerts(p, &r_particle_state.verts[vertex_start]);
			R_ParticleTexcoords(p, &r_particle_state.verts[vertex_start]);
			R_ParticleColor(p, &r_particle_state.verts[vertex_start]);

			const uint32_t index_start = r_particle_state.num_particles * 6;

			r_particle_state.elements[index_start + 0] = vertex_start + 0;
			r_particle_state.elements[index_start + 1] = vertex_start + 1;
			r_particle_state.elements[index_start + 2] = vertex_start + 2;

			r_particle_state.elements[index_start + 3] = vertex_start + 0;
			r_particle_state.elements[index_start + 4] = vertex_start + 2;
			r_particle_state.elements[index_start + 5] = vertex_start + 3;

			e->data = (void *) (uintptr_t) r_particle_state.num_particles++;
		}
	}
}

/**
 * @brief
 */
void R_UploadParticles(void) {
	r_particle_state_t *p = &r_particle_state;

	if (!p->num_particles) {
		return;
	}

	R_UploadToBuffer(&p->verts_buffer, p->num_particles * sizeof(r_particle_interleave_vertex_t) * 4, p->verts);

	R_UploadToBuffer(&p->element_buffer, p->num_particles * sizeof(GLuint) * 6, p->elements);

	p->num_particles = 0;
}

/**
 * @brief Draws all particles for the current frame.
 */
void R_DrawParticles(const r_element_t *e, const size_t count) {
	GLsizei i, j;

	R_EnableColorArray(true);

	R_Color(NULL);

	R_ResetArrayState();

	// alter the array pointers
	R_BindAttributeBuffer(R_ARRAY_POSITION, &r_particle_state.verts_buffer);
	R_BindAttributeBuffer(R_ARRAY_DIFFUSE, &r_particle_state.verts_buffer);
	R_BindAttributeBuffer(R_ARRAY_COLOR, &r_particle_state.verts_buffer);
	R_BindAttributeBuffer(R_ARRAY_ELEMENTS, &r_particle_state.element_buffer);

	const GLint base = (GLint) (intptr_t) e->data;
	r_particle_type_t last_type = -1;
	GLuint last_texnum = texunit_diffuse.texnum;
	GLenum last_blend = -1;

	for (i = j = 0; i < (GLsizei) count; i++, e++) {
		const r_particle_t *p = (const r_particle_t *) e->element;

		// bind the particle's texture
		GLuint texnum = 0;

		if (p->image) {
			texnum = p->image->texnum;
		} else if (p->type == PARTICLE_CORONA) {
			texnum = last_texnum;    // corona texture switching = no
		}

		// draw pending particles
		if ((texnum != last_texnum ||
		        p->type != last_type ||
		        p->blend != last_blend) && i > j) {
			R_DrawArrays(GL_TRIANGLES, (base + j) * 6, (i - j) * 6);
			j = i;
		}

		// change states
		if (p->type != last_type) {
			if (p->type == PARTICLE_ROLL) {
				R_DepthRange(0.0, 0.999);
			} else {
				R_DepthRange(0.0, 1.0);
			}

			if (p->type == PARTICLE_CORONA) {
				R_UseProgram(r_state.corona_program);
			} else {
				R_UseProgram(r_state.null_program);
			}

			last_type = p->type;
		}

		if (p->blend != last_blend) {
			R_BlendFunc(GL_SRC_ALPHA, p->blend);
			last_blend = p->blend;
		}

		if (texnum != last_texnum) {
			R_BindTexture(texnum);
			last_texnum = texnum;
		}
	}

	if (i > j) { // draw any remaining particles
		R_DrawArrays(GL_TRIANGLES, (base + j) * 6, (i - j) * 6);
	}

	// restore depth range
	R_DepthRange(0.0, 1.0);

	// restore array pointers
	R_BindDefaultArray(R_ARRAY_POSITION);
	R_BindDefaultArray(R_ARRAY_DIFFUSE);
	R_BindDefaultArray(R_ARRAY_COLOR);

	R_BindDefaultArray(R_ARRAY_ELEMENTS);

	R_BlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	R_EnableColorArray(false);

	R_UseProgram(r_state.null_program);
}

