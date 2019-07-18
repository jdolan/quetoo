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

#pragma once

#include "cgame/cgame.h"
#include "game/default/g_types.h"

#ifdef __CG_LOCAL_H__

// an infinitely-timed particle
#define PARTICLE_INFINITE		0

// particle that immediately dies
#define PARTICLE_IMMEDIATE		1

typedef enum {
	PARTICLE_EFFECT_NONE,

	PARTICLE_EFFECT_COLOR = 1 << 0, // use color lerp
	PARTICLE_EFFECT_SCALE = 1 << 1, // use scale lerp
	PARTICLE_EFFECT_BOUNCE = 1 << 2, // collide with solids
} cg_particle_effects_t;

typedef enum {
	PARTICLE_SPECIAL_NONE
} cg_particle_special_t;

typedef struct cg_particle_s {
	r_particle_t part; // the r_particle_t to add to the view

	// common particle components
	uint32_t start; // client time when allocated
	uint32_t lifetime; // client time particle should remain active for. a lifetime of 0 = infinite (make sure they get freed sometime though!), 1 = immediate
	vec3_t vel;
	vec3_t accel;

	// special ID specific
	cg_particle_special_t special; // a special ID that can be used for think routines

	union {
		struct {
			uint32_t time; // next time to check for splat
		} blood;
	};

	// effect flag specifics
	cg_particle_effects_t effects; // flags for effects
	vec4_t color_start, color_end;
	vec_t scale_start, scale_end;
	vec_t bounce;

	// particle type specific
	union {
		struct {
			vec_t end_z; // weather particles are freed at this Z
		} weather;

		struct {
			vec_t radius;
			vec_t flicker;
		} corona;

		struct {
			vec_t length;
		} spark;
	};

	struct cg_particle_s *prev; // previous particle in the chain
	struct cg_particle_s *next; // next particle in chain
} cg_particle_t;

// particles are chained by image
typedef struct cg_particles_s {
	const r_media_t *media;
	r_particle_type_t type;
	cg_particle_t *particles;
	struct cg_particles_s *next;
} cg_particles_t;

/**
 * @brief Stores info related to teams on the server.
 */
typedef struct {
	char team_name[MAX_USER_INFO_KEY];
	color_t color;
} cg_team_info_t;

extern cg_team_info_t cg_team_info[MAX_TEAMS];

#endif /* __CG_LOCAL_H__ */
