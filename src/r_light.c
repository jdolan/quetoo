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


#define LIGHT_RADIUS_FACTOR 100.0  // light radius multiplier for shader

/*
 * R_AddLight
 */
void R_AddLight(const vec3_t org, float radius, const vec3_t color){
	light_t *l;

	if(!r_lights->value)
		return;

	if(r_view.num_lights == MAX_LIGHTS)
		return;

	l = &r_view.lights[r_view.num_lights++];

	VectorCopy(org, l->org);
	l->radius = radius;
	VectorCopy(color, l->color);
}


/*
 * R_AddSustainedLight
 */
void R_AddSustainedLight(const vec3_t org, float radius, const vec3_t color, float sustain){
	sustain_t *s;
	int i;

	if(!r_lights->value)
		return;

	s = r_view.sustains;

	for(i = 0; i < MAX_LIGHTS; i++, s++)
		if(!s->sustain)
			break;

	if(i == MAX_LIGHTS)
		return;

	VectorCopy(org, s->light.org);
	s->light.radius = radius;
	VectorCopy(color, s->light.color);

	s->time = r_view.time;
	s->sustain = r_view.time + sustain;
}


/*
 * R_AddSustainedLights
 */
static void R_AddSustainedLights(void){
	vec3_t color;
	float intensity;
	sustain_t *s;
	int i;

	// sustains must be recalculated every frame
	for(i = 0, s = r_view.sustains; i < MAX_LIGHTS; i++, s++){

		if(s->sustain <= r_view.time){  // clear it
			s->sustain = 0;
			continue;
		}

		intensity = (s->sustain - r_view.time) / (s->sustain - s->time);
		VectorScale(s->light.color, intensity, color);

		R_AddLight(s->light.org, s->light.radius, color);
	}
}


/*
 * R_MarkLights_
 */
static void R_MarkLights_(light_t *light, vec3_t trans, int bit, mnode_t *node){
	msurface_t *surf;
	vec3_t origin;
	float dist;
	int i;

	if(node->contents != CONTENTS_NODE)  // leaf
		return;

	if(node->visframe != r_locals.visframe)  // not visible
		if(!node->model)  // and not a bsp submodel
			return;

	VectorSubtract(light->org, trans, origin);

	dist = DotProduct(origin, node->plane->normal) - node->plane->dist;

	if(dist > light->radius * LIGHT_RADIUS_FACTOR){  // front only
		R_MarkLights_(light, trans, bit, node->children[0]);
		return;
	}

	if(dist < light->radius * -LIGHT_RADIUS_FACTOR){  // back only
		R_MarkLights_(light, trans, bit, node->children[1]);
		return;
	}

	if(node->model)  // mark bsp submodel
		node->model->lights |= bit;

	// mark all surfaces in this node
	surf = r_worldmodel->surfaces + node->firstsurface;

	for(i = 0; i < node->numsurfaces; i++, surf++){

		if(surf->lightframe != r_locals.lightframe){  // reset it
			surf->lightframe = r_locals.lightframe;
			surf->lights = 0;
		}

		surf->lights |= bit;  // add this light
	}

	// now go down both sides
	R_MarkLights_(light, trans, bit, node->children[0]);
	R_MarkLights_(light, trans, bit, node->children[1]);
}


/*
 * R_MarkLights
 */
void R_MarkLights(void){
	int i, j;

	r_locals.lightframe++;

	if(r_locals.lightframe > 0xffff)  // avoid overflows
		r_locals.lightframe = 0;

	R_AddSustainedLights();

	// flag all surfaces for each light source
	for(i = 0; i < r_view.num_lights; i++){

		light_t *light = &r_view.lights[i];

		// world surfaces
		R_MarkLights_(light, vec3_origin, 1 << i, r_worldmodel->nodes);

		// and bsp entity surfaces
		for(j = 0; j < r_view.num_entities; j++){

			entity_t *e = &r_view.entities[j];

			if(e->model && e->model->type == mod_bsp_submodel){
				e->model->lights = 0;
				R_MarkLights_(light, e->origin, 1 << i, e->model->nodes);
			}
		}
	}

	// disable first light to reset state
	glLightf(GL_LIGHT0, GL_CONSTANT_ATTENUATION, 0.0);
}


/*
 * R_EnableLights
 */
void R_EnableLights(int mask){
	static int last_mask;
	static int last_count;
	light_t *l;
	vec4_t position;
	vec4_t diffuse;
	int i, count;

	if(mask == last_mask)  // no change
		return;

	last_mask = mask;

	position[3] = diffuse[3] = 1.0;
	count = 0;

	if(mask){  // enable up to 8 light sources
		for(i = 0, l = r_view.lights; i < r_view.num_lights; i++, l++){

			if(count == MAX_ACTIVE_LIGHTS)
				break;

			if(mask & (1 << i)){

				VectorCopy(l->org, position);
				glLightfv(GL_LIGHT0 + count, GL_POSITION, position);

				VectorCopy(l->color, diffuse);
				glLightfv(GL_LIGHT0 + count, GL_DIFFUSE, diffuse);

				glLightf(GL_LIGHT0 + count, GL_CONSTANT_ATTENUATION,
						l->radius * LIGHT_RADIUS_FACTOR);
				count++;
			}
		}
	}

	if(count != last_count){
		if(count < MAX_ACTIVE_LIGHTS)  // disable the next light as a stop
			glLightf(GL_LIGHT0 + count, GL_CONSTANT_ATTENUATION, 0.0);
	}

	last_count = count;
}


/*
 * R_EnableLightsByRadius
 */
void R_EnableLightsByRadius(const vec3_t p){
	const light_t *l;
	vec3_t delta;
	int i, mask;

	mask = 0;

	for(i = 0, l = r_view.lights; i < r_view.num_lights; i++, l++){

		VectorSubtract(l->org, p, delta);

		if(VectorLength(delta) < l->radius * LIGHT_RADIUS_FACTOR)
			mask |= (1 << i);
	}

	R_EnableLights(mask);
}
