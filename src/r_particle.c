/*
 * Copyright(c) 1997-2001 Id Software, Inc.
 * Copyright(c) 2002 The Quakeforge Project.
 * Copyright(c) 2006 Quake2World.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or(at your option) any later version.
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

#include "renderer.h"


/*
 * R_AddParticle
 */
void R_AddParticle(particle_t *p){

	if(r_view.num_particles >= MAX_PARTICLES)
		return;

	r_view.particles[r_view.num_particles++] = *p;
}


/*
 * R_DrawParticles
 */
void R_DrawParticles(void){
	particle_t *p;
	image_t *image;
	int i, j, k, l, m;
	vec3_t right, up, upright, downright, verts[4];
	vec3_t v, weather_right, weather_up;  // keep weather particles vertical
	vec3_t splash_right, splash_up;  // and splash particles horizontal
	vec3_t splash_right_, splash_up_;  // even if they are below us
	float scale;
	byte color[4];

	if(!r_view.num_particles)
		return;

	R_EnableColorArray(true);

	R_ResetArrayState();

	image = r_particletexture;
	R_BindTexture(image->texnum);

	VectorCopy(r_view.angles, v);

	v[0] = 0;  // keep weather particles vertical by removing pitch
	AngleVectors(v, NULL, weather_right, weather_up);

	v[0] = -90;  // and splash particles horizontal by setting it
	AngleVectors(v, NULL, splash_right, splash_up);

	v[0] = 90;  // even if they are below us
	AngleVectors(v, NULL, splash_right_, splash_up_);

	j = k = l = 0;
	for(p = r_view.particles, i = 0; i < r_view.num_particles; i++, p++){

		// bind the particle's texture
		if(p->image != image){

			glDrawArrays(GL_QUADS, 0, l / 3);
			j = k = l = 0;

			image = p->image;
			R_BindTexture(image->texnum);
		}

		// hack a scale up to keep particles from disappearing
		scale = (p->curorg[0] - r_view.origin[0]) * r_locals.forward[0] +
			(p->curorg[1] - r_view.origin[1]) * r_locals.forward[1] +
			(p->curorg[2] - r_view.origin[2]) * r_locals.forward[2];

		if(scale < 20)
			scale = p->curscale;
		else
			scale = p->curscale + scale * 0.002;

		if(p->type == PARTICLE_BEAM){  // beams are lines with starts and ends
			VectorSubtract(p->curorg, p->end, v);
			VectorNormalize(v);
			VectorCopy(v, up);

			VectorSubtract(r_view.origin, p->end, v);
			CrossProduct(up, v, right);
			VectorNormalize(right);
			VectorScale(right, scale, right);

			VectorAdd(p->curorg, right, verts[0]);
			VectorAdd(p->end, right, verts[1]);
			VectorSubtract(p->end, right, verts[2]);
			VectorSubtract(p->curorg, right, verts[3]);
		}
		else if(p->type == PARTICLE_DECAL){  // decals are aligned with surfaces
			AngleVectors(p->dir, NULL, right, up);

			VectorAdd(up, right, verts[0]);
			VectorSubtract(right, up, verts[1]);
			VectorNegate(verts[0], verts[2]);
			VectorNegate(verts[1], verts[3]);

			VectorMA(p->curorg, scale, verts[0], verts[0]);
			VectorMA(p->curorg, scale, verts[1], verts[1]);
			VectorMA(p->curorg, scale, verts[2], verts[2]);
			VectorMA(p->curorg, scale, verts[3], verts[3]);
		}
		else {  // all other particles are aligned with the client's view
			if(p->type == PARTICLE_WEATHER){  // keep it vertical
				VectorScale(weather_right, scale, right);
				VectorScale(weather_up, scale, up);
			}
			else if(p->type == PARTICLE_SPLASH){  // keep it horizontal
				if(p->curorg[2] > r_view.origin[2]){  // it's above us
					VectorScale(splash_right, scale, right);
					VectorScale(splash_up, scale, up);
				}
				else {  // it's below us
					VectorScale(splash_right_, scale, right);
					VectorScale(splash_up_, scale, up);
				}
			}
			else if(p->type == PARTICLE_ROLL){  // roll it
				VectorCopy(r_view.angles, p->dir);
				p->dir[2] = p->roll * r_view.time;

				AngleVectors(p->dir, NULL, right, up);

				VectorScale(right, scale, right);
				VectorScale(up, scale, up);
			}
			else {  // default particle alignment with view
				VectorScale(r_locals.right, scale, right);
				VectorScale(r_locals.up, scale, up);
			}

			VectorAdd(up, right, upright);
			VectorSubtract(right, up, downright);

			VectorSubtract(p->curorg, downright, verts[0]);
			VectorAdd(p->curorg, upright, verts[1]);
			VectorAdd(p->curorg, downright, verts[2]);
			VectorSubtract(p->curorg, upright, verts[3]);
		}

		*(int *)color = palette[p->color];
		color[3] = p->curalpha * 255;

		for(m = 0; m < 4; m++){  // duplicate color data to all 4 verts
			r_state.color_array[j + 0] = color[0] / 255.0;
			r_state.color_array[j + 1] = color[1] / 255.0;
			r_state.color_array[j + 2] = color[2] / 255.0;
			r_state.color_array[j + 3] = color[3] / 255.0;
			j += 4;
		}

		// copy texcoord info
		memcpy(&texunit_diffuse.texcoord_array[k], default_texcoords, sizeof(vec2_t) * 4);
		k += sizeof(vec2_t) / sizeof(vec_t) * 4;

		// and lastly copy the 4 verts
		memcpy(&r_state.vertex_array_3d[l], verts, sizeof(vec3_t) * 4);
		l += sizeof(vec3_t) / sizeof(vec_t) * 4;
	}

	glDrawArrays(GL_QUADS, 0, l / 3);

	R_EnableColorArray(false);

	glColor4ubv(color_white);
}

