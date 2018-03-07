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

static r_buffer_layout_t r_particle_buffer_layout[] = {
	{ .attribute = R_ATTRIB_POSITION, .type = R_TYPE_FLOAT, .count = 3 },
	{ .attribute = R_ATTRIB_DIFFUSE, .type = R_TYPE_FLOAT, .count = 2 },
	{ .attribute = R_ATTRIB_COLOR, .type = R_TYPE_UNSIGNED_BYTE, .count = 4, .normalized = true },
	{ .attribute = -1 }
};

typedef struct {
	vec3_t start;
	vec2_t texcoord0;
	vec2_t texcoord1;
	u8vec4_t color;
	vec_t scale;
	vec_t roll;
	vec3_t end;
	int32_t type;
} r_geometry_particle_interleave_vertex_t;

static r_buffer_layout_t r_geometry_particle_buffer_layout[] = {
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
	vec3_t weather_right;
	vec3_t weather_up;
	vec3_t splash_right[2];
	vec3_t splash_up[2];

	r_particle_interleave_vertex_t verts[MAX_PARTICLES * 4];
	r_buffer_t verts_buffer;

	r_geometry_particle_interleave_vertex_t geometry_verts[MAX_PARTICLES];
	r_buffer_t geometry_verts_buffer;

	uint32_t elements[MAX_PARTICLES * 6];
	uint32_t num_particles;

	r_buffer_t element_buffer;
} r_particle_state_t;

static r_particle_state_t r_particle_state;

/**
 * @brief
 */
void R_InitParticles(void) {

	R_CreateInterleaveBuffer(&r_particle_state.verts_buffer, &(const r_create_interleave_t) {
		.struct_size = sizeof(r_particle_interleave_vertex_t),
		.layout = r_particle_buffer_layout,
		.hint = GL_DYNAMIC_DRAW,
		.size = sizeof(r_particle_state.verts)
	});

	R_CreateElementBuffer(&r_particle_state.element_buffer, &(const r_create_element_t) {
		.type = R_TYPE_UNSIGNED_INT,
		.hint = GL_STATIC_DRAW,
		.size = sizeof(r_particle_state.elements)
	});

	R_CreateInterleaveBuffer(&r_particle_state.geometry_verts_buffer, &(const r_create_interleave_t) {
		.struct_size = sizeof(r_geometry_particle_interleave_vertex_t),
		.layout = r_geometry_particle_buffer_layout,
		.hint = GL_DYNAMIC_DRAW,
		.size = sizeof(r_particle_state.geometry_verts)
	});
}

/**
 * @brief
 */
void R_ShutdownParticles(void) {

	R_DestroyBuffer(&r_particle_state.verts_buffer);
	R_DestroyBuffer(&r_particle_state.element_buffer);

	R_DestroyBuffer(&r_particle_state.geometry_verts_buffer);
}

/**
 * @brief Generates the vertex coordinates for the specified particle.
 */
static void R_ParticleVerts(const r_particle_t *p, r_particle_interleave_vertex_t *verts) {
	vec3_t v, up, right, up_right, down_right;

	if (p->type == PARTICLE_BEAM || p->type == PARTICLE_SPARK || p->type == PARTICLE_WIRE) { // beams are lines with starts and ends
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
	} else if (p->type == PARTICLE_ROLL || p->type == PARTICLE_EXPLOSION) { // roll it
		vec3_t dir;

		VectorCopy(r_view.angles, dir);
		dir[2] = p->roll * r_view.ticks / 1000.0;

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
	        (!p->scroll_s && !p->scroll_t &&! is_atlas && !(p->flags & PARTICLE_FLAG_REPEAT)) ||
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
		s = p->scroll_s * r_view.ticks / 1000.0;
		t = p->scroll_t *r_view.ticks / 1000.0;

		vec_t x_offset = 1.0;

		// scale the texcoords to repeat if we asked for it
		if (p->flags & PARTICLE_FLAG_REPEAT) {

			vec3_t distance;
			VectorSubtract(p->org, p->end, distance);
			const vec_t length = VectorLength(distance);

			x_offset = (length / p->scale) * p->repeat_scale;
		}

		Vector2Set(verts[0].texcoord, s, t);
		Vector2Set(verts[1].texcoord, x_offset + s, t);
		Vector2Set(verts[2].texcoord, x_offset + s, 1.0 + t);
		Vector2Set(verts[3].texcoord, s, 1.0 + t);
	}
}

/**
 * @brief Generates vertex colors for the specified particle.
 */
static void R_ParticleColor(const r_particle_t *p, r_particle_interleave_vertex_t *verts) {

	for (int32_t i = 0; i < 4; i++) {
		ColorDecompose(p->color, verts[i].color);
		
		for (int32_t x = 0; x < 3; x++) {
			verts[i].color[x] *= p->color[3];
		}
	}
}

/**
 * @brief Generates the vertex coordinates for the specified particle.
 */
static void R_ParticleGeometryVerts(const r_particle_t *p, r_geometry_particle_interleave_vertex_t *verts) {

	verts->scale = p->scale;
	VectorCopy(p->org, verts->start);
	verts->type = p->type;
	verts->roll = p->roll;

	if (p->type == PARTICLE_BEAM || p->type == PARTICLE_SPARK || p->type == PARTICLE_WIRE) { // beams are lines with starts and ends
		VectorCopy(p->end, verts->end);
	}

	// all other particles are aligned with the client's view
}

/**
 * @brief Generates texture coordinates for the specified particle.
 */
static void R_ParticleGeometryTexcoords(const r_particle_t *p, r_geometry_particle_interleave_vertex_t *vert) {
	vec_t s, t;
	_Bool is_atlas = p->image && p->image->type == IT_ATLAS_IMAGE;

	if (!p->image ||
	        (!p->scroll_s && !p->scroll_t &&!is_atlas && !(p->flags & PARTICLE_FLAG_REPEAT)) ||
	        p->type == PARTICLE_CORONA) {

		Vector2Copy(default_texcoords[0], vert->texcoord0);
		Vector2Copy(default_texcoords[2], vert->texcoord1);

		return;
	}

	// atlas needs a different pipeline
	if (is_atlas) {
		const r_atlas_image_t *atlas_image = (const r_atlas_image_t *) p->image;

		for (int32_t i = 0; i < 2; i++) {
			vert->texcoord0[i] = atlas_image->texcoords[i];
			vert->texcoord1[i] = atlas_image->texcoords[2 + i];
		}
	} else {
		s = p->scroll_s * r_view.ticks / 1000.0;
		t = p->scroll_t * r_view.ticks / 1000.0;

		vec_t x_offset = 1.0;

		// scale the texcoords to repeat if we asked for it
		if (p->flags & PARTICLE_FLAG_REPEAT) {

			vec3_t distance;
			VectorSubtract(p->org, p->end, distance);
			const vec_t length = VectorLength(distance);

			x_offset = (length / p->scale) * p->repeat_scale;
		}

		Vector2Set(vert->texcoord0, s, t);
		Vector2Set(vert->texcoord1, x_offset + s, 1.0 + t);
	}
}

/**
 * @brief Generates vertex colors for the specified particle.
 */
static void R_ParticleGeometryColor(const r_particle_t *p, r_geometry_particle_interleave_vertex_t *vert) {

	ColorDecompose(p->color, vert->color);
	
	for (int32_t x = 0; x < 3; x++) {
		vert->color[x] *= p->color[3];
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

		if (e->type != ELEMENT_PARTICLE) {
			continue;
		}

		r_particle_t *p = (r_particle_t *) e->element;

		if (r_state.particle_program == program_particle) {

			R_ParticleGeometryVerts(p, &r_particle_state.geometry_verts[r_particle_state.num_particles]);
			R_ParticleGeometryTexcoords(p, &r_particle_state.geometry_verts[r_particle_state.num_particles]);
			R_ParticleGeometryColor(p, &r_particle_state.geometry_verts[r_particle_state.num_particles]);
		} else {
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
		}

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

	if (r_state.particle_program == program_particle) {
		R_UploadToBuffer(&p->geometry_verts_buffer, p->num_particles * sizeof(r_geometry_particle_interleave_vertex_t), p->geometry_verts);
	} else {
		R_UploadToBuffer(&p->verts_buffer, p->num_particles * sizeof(r_particle_interleave_vertex_t) * 4, p->verts);
		R_UploadToBuffer(&p->element_buffer, p->num_particles * sizeof(uint32_t) * 6, p->elements);
	}

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
	if (r_state.particle_program == program_particle) {
		R_UseProgram(program_particle);
		R_UseParticleData_particle(r_particle_state.weather_right, r_particle_state.weather_up, r_particle_state.splash_right, r_particle_state.splash_up);

		R_UseProgram(program_particle_corona);
		R_UseParticleData_particle_corona(r_particle_state.weather_right, r_particle_state.weather_up, r_particle_state.splash_right, r_particle_state.splash_up);

		R_BindAttributeInterleaveBuffer(&r_particle_state.geometry_verts_buffer, R_ATTRIB_MASK_ALL);
		R_EnableTexture(texunit_lightmap, true);
	} else {
		R_BindAttributeInterleaveBuffer(&r_particle_state.verts_buffer, R_ATTRIB_MASK_ALL);
		R_BindAttributeBuffer(R_ATTRIB_ELEMENTS, &r_particle_state.element_buffer);
	}

	const GLint base = (GLint) (intptr_t) e->data;

	// these are set to -1 to immediately trigger a change.
	r_particle_type_t last_type = -1;
	GLenum last_blend = -1;
	GLuint last_texnum = texunit_diffuse->texnum;

	for (i = j = 0; i < (GLsizei) count; i++, e++) {
		const r_particle_t *p = (const r_particle_t *) e->element;

		// bind the particle's texture
		GLuint texnum = 0;

		if (p->image) {
			texnum = p->image->texnum;
		} else if (p->type == PARTICLE_CORONA) {
			texnum = last_texnum; // corona texture switching = no
		}

		// draw pending particles
		if ((texnum != last_texnum || p->type != last_type || p->blend != last_blend) && i > j) {
			if (r_state.particle_program == program_particle) {
				R_DrawArrays(GL_POINTS, base + j, i - j);
			} else {
				R_DrawArrays(GL_TRIANGLES, (base + j) * 6, (i - j) * 6);
			}
			j = i;
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

			if (p->type == PARTICLE_CORONA) {
				R_UseProgram(r_state.corona_program);
			} else {
				R_UseProgram(r_state.particle_program);
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
		if (r_state.particle_program == program_particle) {
			R_DrawArrays(GL_POINTS, base + j, i - j);
		} else {
			R_DrawArrays(GL_TRIANGLES, (base + j) * 6, (i - j) * 6);
		}
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
