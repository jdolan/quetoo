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

	if (r_view.num_particles == lengthof(r_view.particles))
		return;

	r_view.particles[r_view.num_particles++] = *p;

	e.type = ELEMENT_PARTICLE;

	e.element = (const void *) p;
	e.origin = (const vec_t *) p->org;

	R_AddElement(&e);
}

/**
 * @brief Pools commonly used angular vectors for particle calculations and
 * accumulates particle primitives each frame.
 */
typedef struct {
	vec3_t weather_right;
	vec3_t weather_up;
	vec3_t splash_right[2];
	vec3_t splash_up[2];

	GLfloat verts[MAX_PARTICLES * 3 * 4];
	GLfloat texcoords[MAX_PARTICLES * 2 * 4];
	GLfloat colors[MAX_PARTICLES * 4 * 4];
	GLuint elements[MAX_PARTICLES * 6];
	uint32_t num_particles;
	
	r_buffer_t verts_buffer;
	r_buffer_t texcoords_buffer;
	r_buffer_t colors_buffer;
	r_buffer_t element_buffer;
} r_particle_state_t;

static r_particle_state_t r_particle_state;

/**
 * @brief
 */
void R_InitParticles(void) {
	
	R_CreateBuffer(&r_particle_state.verts_buffer, GL_DYNAMIC_DRAW, R_BUFFER_DATA, sizeof(r_particle_state.verts), NULL);
	R_CreateBuffer(&r_particle_state.texcoords_buffer, GL_DYNAMIC_DRAW, R_BUFFER_DATA, sizeof(r_particle_state.texcoords), NULL);
	R_CreateBuffer(&r_particle_state.colors_buffer, GL_DYNAMIC_DRAW, R_BUFFER_DATA, sizeof(r_particle_state.colors), NULL);

	R_CreateBuffer(&r_particle_state.element_buffer, GL_DYNAMIC_DRAW, R_BUFFER_ELEMENT, sizeof(r_particle_state.element_buffer), NULL);
}

/**
 * @brief
 */
void R_ShutdownParticles(void) {
	
	R_DestroyBuffer(&r_particle_state.verts_buffer);
	R_DestroyBuffer(&r_particle_state.texcoords_buffer);
	R_DestroyBuffer(&r_particle_state.colors_buffer);

	R_DestroyBuffer(&r_particle_state.element_buffer);
}

/**
 * @brief Generates the vertex coordinates for the specified particle.
 */
static void R_ParticleVerts(const r_particle_t *p, GLfloat *out) {
	vec3_t v, up, right, up_right, down_right;
	vec3_t *verts;

	verts = (vec3_t *) out;

	if (p->type == PARTICLE_BEAM || p->type == PARTICLE_SPARK) { // beams are lines with starts and ends
		VectorSubtract(p->org, p->end, v);
		VectorNormalize(v);
		VectorCopy(v, up);

		VectorSubtract(r_view.origin, p->end, v);
		CrossProduct(up, v, right);
		VectorNormalize(right);
		VectorScale(right, p->scale, right);

		VectorAdd(p->org, right, verts[0]);
		VectorAdd(p->end, right, verts[1]);
		VectorSubtract(p->end, right, verts[2]);
		VectorSubtract(p->org, right, verts[3]);
		return;
	}

	if (p->type == PARTICLE_DECAL) { // decals are aligned with surfaces
		AngleVectors(p->dir, NULL, right, up);

		VectorAdd(up, right, verts[0]);
		VectorSubtract(right, up, verts[1]);
		VectorNegate(verts[0], verts[2]);
		VectorNegate(verts[1], verts[3]);

		VectorMA(p->org, p->scale, verts[0], verts[0]);
		VectorMA(p->org, p->scale, verts[1], verts[1]);
		VectorMA(p->org, p->scale, verts[2], verts[2]);
		VectorMA(p->org, p->scale, verts[3], verts[3]);
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

	VectorSubtract(p->org, down_right, verts[0]);
	VectorAdd(p->org, up_right, verts[1]);
	VectorAdd(p->org, down_right, verts[2]);
	VectorSubtract(p->org, up_right, verts[3]);
}

/**
 * @brief Generates texture coordinates for the specified particle.
 */
static void R_ParticleTexcoords(const r_particle_t *p, GLfloat *out) {
	vec_t s, t;

	_Bool is_atlas = p->image && p->image->type == IT_ATLAS_IMAGE;

	if (!p->image ||
		(!p->scroll_s && !p->scroll_t && !is_atlas) ||
		p->type == PARTICLE_CORONA) {
		memcpy(out, default_texcoords, sizeof(vec2_t) * 4);
		return;
	}

	// atlas needs a different pipeline
	if (is_atlas) {
		const r_atlas_image_t *atlas_image = (const r_atlas_image_t *) p->image;
		
		out[0] = atlas_image->texcoords[0];
		out[1] = atlas_image->texcoords[1];

		out[2] = atlas_image->texcoords[2];
		out[3] = atlas_image->texcoords[1];

		out[4] = atlas_image->texcoords[2];
		out[5] = atlas_image->texcoords[3];

		out[6] = atlas_image->texcoords[0];
		out[7] = atlas_image->texcoords[3];
	} else {
		s = p->scroll_s * r_view.time / 1000.0;
		t = p->scroll_t * r_view.time / 1000.0;

		out[0] = 0.0 + s;
		out[1] = 0.0 + t;

		out[2] = 1.0 + s;
		out[3] = 0.0 + t;

		out[4] = 1.0 + s;
		out[5] = 1.0 + t;

		out[6] = 0.0 + s;
		out[7] = 1.0 + t;
	}
}

/**
 * @brief Generates vertex colors for the specified particle.
 */
static void R_ParticleColor(const r_particle_t *p, GLfloat *out) {

	for (int32_t i = 0; i < 4; i++) {
		Vector4Copy(p->color, out);
		out += 4;
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

			R_ParticleVerts(p, &r_particle_state.verts[vertex_start * 3]);
			R_ParticleTexcoords(p, &r_particle_state.texcoords[vertex_start * 2]);
			R_ParticleColor(p, &r_particle_state.colors[vertex_start * 4]);
			
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

void R_UploadParticles(void) {

	if (!r_particle_state.num_particles)
		return;

	R_UploadToBuffer(&r_particle_state.verts_buffer, 0, r_particle_state.num_particles * sizeof(vec3_t) * 4, r_particle_state.verts);
	R_UploadToBuffer(&r_particle_state.texcoords_buffer, 0, r_particle_state.num_particles * sizeof(vec2_t) * 4, r_particle_state.texcoords);
	R_UploadToBuffer(&r_particle_state.colors_buffer, 0, r_particle_state.num_particles * sizeof(vec4_t) * 4, r_particle_state.colors);

	R_UploadToBuffer(&r_particle_state.element_buffer, 0, r_particle_state.num_particles * sizeof(GLuint) * 6, r_particle_state.elements);

	r_particle_state.num_particles = 0;
}

/**
 * @brief Draws all particles for the current frame.
 */
void R_DrawParticles(const r_element_t *e, const size_t count) {
	size_t i, j;

	R_EnableColorArray(true);

	R_Color(NULL);

	R_ResetArrayState();

	// alter the array pointers
	R_BindArray(R_ARRAY_VERTEX, &r_particle_state.verts_buffer);
	R_BindArray(R_ARRAY_TEX_DIFFUSE, &r_particle_state.texcoords_buffer);
	R_BindArray(R_ARRAY_COLOR, &r_particle_state.colors_buffer);

	R_BindArray(R_ARRAY_ELEMENTS, &r_particle_state.element_buffer);

	const GLuint base = (uintptr_t) e->data;
	r_particle_type_t last_type = -1;
	GLuint last_texnum = texunit_diffuse.texnum;
	GLenum last_blend = -1;

	for (i = j = 0; i < count; i++, e++) {
		const r_particle_t *p = (const r_particle_t *) e->element;

		// bind the particle's texture
		GLuint texnum = 0;
		
		if (p->image)
			texnum = p->image->texnum;
		else if (p->type == PARTICLE_CORONA)
			texnum = last_texnum; // corona texture switching = no

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
	R_BindDefaultArray(R_ARRAY_VERTEX);
	R_BindDefaultArray(R_ARRAY_TEX_DIFFUSE);
	R_BindDefaultArray(R_ARRAY_COLOR);

	R_BindDefaultArray(R_ARRAY_ELEMENTS);

	R_BlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	R_EnableColorArray(false);

	R_UseProgram(r_state.null_program);
}

