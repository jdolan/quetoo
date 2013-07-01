/*
 * Copyright(c) 1997-2001 Id Software, Inc.
 * Copyright(c) 2002 The Quakeforge Project.
 * Copyright(c) 2006 Quake2World.
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

/*
 * @brief Copies the specified particle into the view structure. Simple PVS
 * culling is done for large particles.
 */
void R_AddParticle(const r_particle_t *p) {

	if (r_view.num_particles >= MAX_PARTICLES)
		return;

	if (p->type != PARTICLE_BEAM && R_LeafForPoint(p->org, NULL)->vis_frame != r_locals.vis_frame)
		return;

	r_view.particles[r_view.num_particles++] = *p;

	R_AddElement(ELEMENT_PARTICLE, (const void *) p);
}

/*
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
	GLubyte colors[MAX_PARTICLES * 4 * 4];
} r_particle_state_t;

static r_particle_state_t r_particle_state;

/*
 * @brief Generates the vertex coordinates for the specified particle.
 */
static void R_ParticleVerts(const r_particle_t *p, GLfloat *out) {
	vec3_t v, up, right, up_right, down_right;
	vec3_t *verts;
	vec_t scale;

	verts = (vec3_t *) out;

	scale = // hack a scale up to keep particles from disappearing
			(p->org[0] - r_view.origin[0]) * r_view.forward[0] + (p->org[1] - r_view.origin[1])
					* r_view.forward[1] + (p->org[2] - r_view.origin[2]) * r_view.forward[2];

	scale = scale > 20.0 ? p->scale + scale * 0.002 : p->scale;

	if (p->type == PARTICLE_BEAM) { // beams are lines with starts and ends
		VectorSubtract(p->org, p->end, v);
		VectorNormalize(v);
		VectorCopy(v, up);

		VectorSubtract(r_view.origin, p->end, v);
		CrossProduct(up, v, right);
		VectorNormalize(right);
		VectorScale(right, scale, right);

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

		VectorMA(p->org, scale, verts[0], verts[0]);
		VectorMA(p->org, scale, verts[1], verts[1]);
		VectorMA(p->org, scale, verts[2], verts[2]);
		VectorMA(p->org, scale, verts[3], verts[3]);
		return;
	}

	// all other particles are aligned with the client's view

	if (p->type == PARTICLE_WEATHER) { // keep it vertical
		VectorScale(r_particle_state.weather_right, scale, right);
		VectorScale(r_particle_state.weather_up, scale, up);
	} else if (p->type == PARTICLE_SPLASH) { // keep it horizontal

		if (p->org[2] > r_view.origin[2]) { // it's above us
			VectorScale(r_particle_state.splash_right[0], scale, right);
			VectorScale(r_particle_state.splash_up[0], scale, up);
		} else { // it's below us
			VectorScale(r_particle_state.splash_right[1], scale, right);
			VectorScale(r_particle_state.splash_up[1], scale, up);
		}
	} else if (p->type == PARTICLE_ROLL) { // roll it
		vec3_t dir;

		VectorCopy(r_view.angles, dir);
		dir[2] = p->roll * r_view.time / 1000.0;

		AngleVectors(dir, NULL, right, up);

		VectorScale(right, scale, right);
		VectorScale(up, scale, up);
	} else { // default particle alignment with view
		VectorScale(r_view.right, scale, right);
		VectorScale(r_view.up, scale, up);
	}

	VectorAdd(up, right, up_right);
	VectorSubtract(right, up, down_right);

	VectorSubtract(p->org, down_right, verts[0]);
	VectorAdd(p->org, up_right, verts[1]);
	VectorAdd(p->org, down_right, verts[2]);
	VectorSubtract(p->org, up_right, verts[3]);
}

/*
 * @brief Generates texture coordinates for the specified particle.
 */
static void R_ParticleTexcoords(const r_particle_t *p, GLfloat *out) {
	vec_t s, t;

	if (!p->scroll_s && !p->scroll_t) {
		memcpy(out, default_texcoords, sizeof(vec2_t) * 4);
		return;
	}

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

/*
 * @brief Generates vertex colors for the specified particle.
 */
static void R_ParticleColor(const r_particle_t *p, GLubyte *out) {
	int32_t i;

	for (i = 0; i < 4; i++) {
		memcpy(out, &palette[p->color], 4);
		out[3] = Clamp(p->alpha * 255, 0, 255);
		out += 4;
	}
}

/*
 * @brief Updates the shared angular vectors required for particle generation.
 */
static void R_UpdateParticleState(void) {
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

/*
 * @brief Generates primitives for the specified particle elements. Each
 * particle's index into the shared array is written to the element's data
 * field.
 */
void R_UpdateParticles(r_element_t *e, const size_t count) {
	size_t i, j;

	R_UpdateParticleState();

	for (i = j = 0; i < count; i++, e++) {

		if (e->type == ELEMENT_PARTICLE) {
			r_particle_t *p = (r_particle_t *) e->element;

			R_ParticleVerts(p, &r_particle_state.verts[j * 3 * 4]);
			R_ParticleTexcoords(p, &r_particle_state.texcoords[j * 2 * 4]);
			R_ParticleColor(p, &r_particle_state.colors[j * 4 * 4]);

			e->data = (void *) (intptr_t) (j++ * 4);
		}
	}
}

/*
 * @brief Draws all particles for the current frame.
 */
void R_DrawParticles(const r_element_t *e, const size_t count) {
	size_t i, j;

	R_EnableColorArray(true);

	R_ResetArrayState();

	// alter the array pointers
	R_BindArray(GL_VERTEX_ARRAY, GL_FLOAT, r_particle_state.verts);
	R_BindArray(GL_TEXTURE_COORD_ARRAY, GL_FLOAT, r_particle_state.texcoords);
	R_BindArray(GL_COLOR_ARRAY, GL_UNSIGNED_BYTE, r_particle_state.colors);

	const GLuint base = (intptr_t) e->data;

	for (i = j = 0; i < count; i++, e++) {
		const r_particle_t *p = (const r_particle_t *) e->element;

		// bind the particle's texture
		if (p->image->texnum != texunit_diffuse.texnum) {

			if (i > j) { // draw pending particles
				glDrawArrays(GL_QUADS, base + j * 4, (i - j) * 4);
				j = i;
			}

			R_BindTexture(p->image->texnum);
			R_BlendFunc(GL_SRC_ALPHA, p->blend);
		}
	}

	if (i > j) { // draw any remaining particles
		glDrawArrays(GL_QUADS, base + j * 4, (i - j) * 4);
	}

	// restore array pointers
	R_BindDefaultArray(GL_VERTEX_ARRAY);
	R_BindDefaultArray(GL_TEXTURE_COORD_ARRAY);
	R_BindDefaultArray(GL_COLOR_ARRAY);

	R_BlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	R_EnableColorArray(false);

	R_Color(NULL);
}

