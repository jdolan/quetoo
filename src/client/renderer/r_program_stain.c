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
 * @brief
 */
void R_PreLink_stain(const r_program_t *program) {

	R_BindAttributeLocation(program, "POSITION", R_ATTRIB_POSITION);
	R_BindAttributeLocation(program, "COLOR", R_ATTRIB_COLOR);
	R_BindAttributeLocation(program, "TEXCOORD", R_ATTRIB_DIFFUSE);
}

/**
 * @brief
 */
void R_InitProgram_stain(r_program_t *program) {

	R_ProgramVariable(&program->attributes[R_ATTRIB_POSITION], R_ATTRIBUTE, "POSITION", true);
	R_ProgramVariable(&program->attributes[R_ATTRIB_COLOR], R_ATTRIBUTE, "COLOR", true);
	R_ProgramVariable(&program->attributes[R_ATTRIB_DIFFUSE], R_ATTRIBUTE, "TEXCOORD", true);
}
