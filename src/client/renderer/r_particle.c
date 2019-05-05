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
	vec3_t start;
	vec2_t texcoord0;
	vec2_t texcoord1;
	u8vec4_t color;
	vec_t scale;
	vec_t roll;
	vec3_t end;
	int32_t type;
} r_particle_vertex_t;

static r_buffer_layout_t r_particle_buffer_layout[] = {
	{ .attribute = R_ATTRIB_POSITION, .type = R_TYPE_FLOAT, .count = 3 },
	{ .attribute = R_ATTRIB_DIFFUSE, .type = R_TYPE_FLOAT, .count = 2 },
	{ .attribute = R_ATTRIB_LIGHTMAP, .type = R_TYPE_FLOAT, .count = 2 },
	{ .attribute = R_ATTRIB_COLOR, .type = R_TYPE_UNSIGNED_BYTE, .count = 4, .normalized = true },
	{ .attribute = R_ATTRIB_SCALE, .type = R_TYPE_FLOAT },
	{ .attribute = R_ATTRIB_ROLL, .type = R_TYPE_FLOAT },
	{ .attribute = R_ATTRIB_END, .type = R_TYPE_FLOAT, .count = 3 },
	{ .attribute = R_ATTRIB_TYPE, .type = R_TYPE_INT, .integer = true },
	{ .attribute = -1 }
};

/**
 * @brief Pools commonly used angular vectors for particle calculations and
 * accumulates particle primitives each frame.
 */
typedef struct {

	r_particle_vertex_t verts[MAX_PARTICLES];
	r_buffer_t vertex_buffer;

	GLuint elements[MAX_PARTICLES * 6];
	size_t num_particles;

	r_buffer_t element_buffer;
} r_particle_state_t;

static r_particle_state_t r_particle_state;

/**
 * @brief
 */
void R_InitParticles(void) {

	R_CreateInterleaveBuffer(&r_particle_state.vertex_buffer, &(const r_create_interleave_t) {
		.struct_size = sizeof(r_particle_vertex_t),
		.layout = r_particle_buffer_layout,
		.hint = GL_DYNAMIC_DRAW,
		.size = sizeof(r_particle_state.verts)
	});

	R_CreateElementBuffer(&r_particle_state.element_buffer, &(const r_create_element_t) {
		.type = R_TYPE_UNSIGNED_INT,
		.hint = GL_STATIC_DRAW,
		.size = sizeof(r_particle_state.elements)
	});
}

/**
 * @brief
 */
void R_ShutdownParticles(void) {

	R_DestroyBuffer(&r_particle_state.vertex_buffer);
	R_DestroyBuffer(&r_particle_state.element_buffer);
}

/**
 * @brief Generates the geometry shader input for the specified particle.
 */
static void R_UpdateParticle(const r_particle_t *p, r_particle_vertex_t *out) {

	VectorCopy(p->org, out->start);
	VectorCopy(p->end, out->end);

	out->scale = p->scale;
	out->type = p->type;
	out->roll = p->roll;

	if (!p->scroll_s && !p->scroll_t && !(p->flags & PARTICLE_FLAG_REPEAT)) {
		 if (p->image && p->image->media.type == MEDIA_ATLAS_IMAGE) {
			 const r_atlas_image_t *atlas_image = (r_atlas_image_t *) p->image;
			 for (int32_t i = 0; i < 2; i++) {
				 out->texcoord0[i] = atlas_image->texcoords[i];
				 out->texcoord1[i] = atlas_image->texcoords[2 + i];
			 }
		 } else {
			 Vector2Copy(default_texcoords[0], out->texcoord0);
			 Vector2Copy(default_texcoords[2], out->texcoord1);
		 }
	} else {
		vec_t s = p->scroll_s * r_view.ticks / 1000.0;
		vec_t t = p->scroll_t * r_view.ticks / 1000.0;

		vec_t x_offset = 1.0;

		// scale the texcoords to repeat if we asked for it
		if (p->flags & PARTICLE_FLAG_REPEAT) {

			vec3_t distance;
			VectorSubtract(p->org, p->end, distance);
			const vec_t length = VectorLength(distance);

			x_offset = (length / p->scale) * p->repeat_scale;
		}

		Vector2Set(out->texcoord0, s, t);
		Vector2Set(out->texcoord1, x_offset + s, 1.0 + t);
	}

	ColorDecompose(p->color, out->color);

	for (int32_t i = 0; i < 3; i++) {
		out->color[i] *= p->color[3];
	}
}

/**
 * @brief Generates primitives for the specified particle elements. Each
 * particle's index into the shared array is written to the element's data
 * field.
 */
void R_UpdateParticles(r_element_t *e, const size_t count) {

	for (size_t i = 0; i < count; i++, e++) {

		if (e->type != ELEMENT_PARTICLE) {
			continue;
		}

		r_particle_t *p = (r_particle_t *) e->element;

		R_UpdateParticle(p, &r_particle_state.verts[r_particle_state.num_particles]);

		e->data = (void *) (uintptr_t) r_particle_state.num_particles++;
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

	R_UploadToBuffer(&p->vertex_buffer, p->num_particles * sizeof(r_particle_vertex_t), p->verts);

	p->num_particles = 0;
}

/**
 * @brief Draws `count` particles for the current frame, starting at e.
 */
void R_DrawParticles(const r_element_t *e, const size_t count) {
	GLsizei i, j;

	R_EnableTexture(texunit_lightmap, true);

	R_EnableColorArray(true);

	R_Color(NULL);

	R_ResetArrayState();

	R_UseProgram(program_particle);

	R_BindAttributeInterleaveBuffer(&r_particle_state.vertex_buffer, R_ATTRIB_MASK_ALL);

	const GLint base = (GLint) (intptr_t) e->data;

	// these are set to -1 to immediately trigger a change.
	r_particle_type_t last_type = -1;
	GLenum last_blend = -1;
	GLuint last_texnum = 0;

	for (i = j = 0; i < (GLsizei) count; i++, e++) {
		const r_particle_t *p = (const r_particle_t *) e->element;

		const GLuint texnum = p->image ? p->image->texnum : last_texnum;

		if (i > j) { // draw pending particles
			if (texnum != last_texnum || p->type != last_type || p->blend != last_blend) {
				R_DrawArrays(GL_POINTS, base + j, i - j);
				j = i;
			}
		}

		// change states
		if (p->type != last_type) {
			if (p->type == PARTICLE_EXPLOSION || p->type == PARTICLE_CORONA) {
				R_DepthRange(0.0, 0.999);
			} else {
				R_DepthRange(0.0, 1.0);
			}

			if (p->type == PARTICLE_FLARE) {
				R_EnableDepthTest(false);
			} else {
				R_EnableDepthTest(true);
			}

			last_type = p->type;
		}

		if (p->blend != last_blend) {
			R_BlendFunc(GL_ONE, p->blend);
			last_blend = p->blend;
		}

		if (texnum != last_texnum) {
			R_BindDiffuseTexture(texnum);
			last_texnum = texnum;
		}
	}

	if (i > j) { // draw any remaining particles
		R_DrawArrays(GL_POINTS, base + j, i - j);
	}

	// restore array pointers
	R_UnbindAttributeBuffers();

	R_BlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	R_DepthRange(0.0, 1.0);

	R_EnableColorArray(false);

	R_EnableDepthTest(true);

	R_EnableTexture(texunit_lightmap, false);

	R_UseProgram(program_null);
}
