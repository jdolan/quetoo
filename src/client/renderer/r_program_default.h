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

#ifndef __R_PROGRAM_DEFAULT_H__
#define __R_PROGRAM_DEFAULT_H__

#include "r_types.h"

#ifdef __R_LOCAL_H__
void R_InitProgram_default(void);
void R_UseProgram_default(void);
void R_UseMaterial_default(const r_material_t *material);
void R_UseFog_default(const r_fog_parameters_t *value);
void R_UseLight_default(const uint32_t light_index, const r_light_t *lights);
#endif /* __R_LOCAL_H__ */

#endif /* __R_PROGRAM_DEFAULT_H__ */
