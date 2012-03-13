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

#include "cg_local.h"

#define MAX_WEATHER_PARTICLES 2048
unsigned short cg_num_weather_particles;

/*
 * Cg_WeatherEffects
 */
void Cg_WeatherEffects(void) {
	int j, k, max;
	vec3_t start, end;
	c_trace_t tr;
	float ceiling;
	r_particle_t *p;
	s_sample_t *s;

	if (!cg_add_weather->value)
		return;

	if (!(cgi.view->weather & (WEATHER_RAIN | WEATHER_SNOW)))
		return;

	p = NULL; // so that we add the right amount

	// never attempt to add more than a set amount per frame
	max = 50 * cg_add_weather->value;

	k = 0;
	while (cg_num_weather_particles < MAX_WEATHER_PARTICLES && k++ < max) {

		VectorCopy(cgi.view->origin, start);
		start[0] = start[0] + (random() % 2048) - 1024;
		start[1] = start[1] + (random() % 2048) - 1024;

		VectorCopy(start, end);
		end[2] += 8192;

		// trace up looking for sky
		tr = cgi.Trace(start, end, 0.0, MASK_SHOT);

		if (!(tr.surface->flags & SURF_SKY))
			continue;

		if (!(p = Cg_AllocParticle()))
			break;

		p->type = PARTICLE_WEATHER;

		// drop down somewhere between sky and player
		ceiling = tr.end[2] > start[2] + 1024 ? start[2] + 1024 : tr.end[2];
		tr.end[2] = tr.end[2] - ((ceiling - start[2]) * randomf());

		VectorCopy(tr.end, p->org);
		p->org[2] -= 1;

		// trace down look for ground
		VectorCopy(start, end);
		end[2] -= 8192;

		tr = cgi.Trace(p->org, end, 0.0, MASK_ALL);

		if (!tr.surface) // this shouldn't happen
			VectorCopy(start, p->end);
		else
			VectorCopy(tr.end, p->end);

		// setup the particles
		if (cgi.view->weather & WEATHER_RAIN) {
			p->image = cg_particle_rain;
			p->vel[2] = -800;
			p->alpha = 0.4;
			p->color = 8;
			p->scale = 6;
		} else if (cgi.view->weather & WEATHER_SNOW) {
			p->image = cg_particle_snow;
			p->vel[2] = -120;
			p->alpha = 0.6;
			p->alpha_vel = randomf() * -1;
			p->color = 8;
			p->scale = 1.5;
		} else
			// undefined
			continue;

		for (j = 0; j < 2; j++) {
			p->vel[j] = randomc() * 2;
			p->accel[j] = randomc() * 2;
		}
		cg_num_weather_particles++;
	}

	// add an appropriate looping sound
	if (cgi.view->weather & WEATHER_RAIN)
		s = cg_sample_rain;
	else if (cgi.view->weather & WEATHER_SNOW)
		s = cg_sample_snow;
	else
		s = NULL;

	if (!s)
		return;

	cgi.LoopSample(cgi.view->origin, s);
}
