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

#include "r_types.h"

r_model_t *R_LoadModel(const char *name);
r_model_t *R_WorldModel(void);

#ifdef __R_LOCAL_H__

typedef struct {
	r_model_t *world;

	GLuint null_vertices;
	GLuint null_elements;
	size_t null_elements_count;

} r_model_state_t;

extern r_model_state_t r_model_state;

void R_InitModels(void);
void R_ShutdownModels(void);

#endif /* __R_LOCAL_H__ */
