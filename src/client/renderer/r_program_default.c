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

// these are the variables defined in the GLSL shader
typedef struct r_default_program_s {
	r_attribute_t tangent;

	r_uniform1i_t lightmap;
	r_uniform1i_t deluxemap;
	r_uniform1i_t normalmap;
	r_uniform1i_t glossmap;

	r_uniform1f_t bump;
	r_uniform1f_t parallax;
	r_uniform1f_t hardness;
	r_uniform1f_t specular;

	r_sampler2d_t sampler0;
	r_sampler2d_t sampler1;
	r_sampler2d_t sampler2;
	r_sampler2d_t sampler3;
	r_sampler2d_t sampler4;
} r_default_program_t;

static r_default_program_t r_default_program;

/*
 * R_InitProgram_default
 */
void R_InitProgram_default(void) {

	r_default_program_t *p = &r_default_program;

	R_ProgramVariable(&p->tangent, R_ATTRIBUTE, "TANGENT");

	R_ProgramVariable(&p->lightmap, R_UNIFORM_INT, "LIGHTMAP");
	R_ProgramVariable(&p->deluxemap, R_UNIFORM_INT, "DELUXEMAP");
	R_ProgramVariable(&p->normalmap, R_UNIFORM_INT, "NORMALMAP");
	R_ProgramVariable(&p->glossmap, R_UNIFORM_INT, "GLOSSMAP");

	R_ProgramVariable(&p->bump, R_UNIFORM_FLOAT, "BUMP");
	R_ProgramVariable(&p->parallax, R_UNIFORM_FLOAT, "PARALLAX");
	R_ProgramVariable(&p->hardness, R_UNIFORM_FLOAT, "HARDNESS");
	R_ProgramVariable(&p->specular, R_UNIFORM_FLOAT, "SPECULAR");

	R_ProgramVariable(&p->sampler0, R_SAMPLER_2D, "SAMPLER0");
	R_ProgramVariable(&p->sampler1, R_SAMPLER_2D, "SAMPLER1");
	R_ProgramVariable(&p->sampler2, R_SAMPLER_2D, "SAMPLER2");
	R_ProgramVariable(&p->sampler3, R_SAMPLER_2D, "SAMPLER3");
	R_ProgramVariable(&p->sampler4, R_SAMPLER_2D, "SAMPLER4");

	R_DisableAttribute(&p->tangent);

	R_ProgramParameter1i(&p->lightmap, 0);
	R_ProgramParameter1i(&p->deluxemap, 0);
	R_ProgramParameter1i(&p->normalmap, 0);
	R_ProgramParameter1i(&p->glossmap, 0);

	R_ProgramParameter1f(&p->bump, 1.0);
	R_ProgramParameter1f(&p->parallax, 1.0);
	R_ProgramParameter1f(&p->hardness, 1.0);
	R_ProgramParameter1f(&p->specular, 1.0);

	R_ProgramParameter1i(&p->sampler0, 0);
	R_ProgramParameter1i(&p->sampler1, 1);
	R_ProgramParameter1i(&p->sampler2, 2);
	R_ProgramParameter1i(&p->sampler3, 3);
	R_ProgramParameter1i(&p->sampler4, 4);
}

/*
 * R_UseProgram_default
 */
void R_UseProgram_default(void) {

	r_default_program_t *p = &r_default_program;

	R_ProgramParameter1i(&p->lightmap, texunit_lightmap.enabled);
	R_ProgramParameter1i(&p->deluxemap, texunit_lightmap.enabled);
}

/*
 * R_UseMaterial_default
 */
void R_UseMaterial_default(const r_bsp_surface_t *surf, const r_image_t *image) {

	r_default_program_t *p = &r_default_program;

	if (!image || !image->normalmap || !r_bumpmap->value) {
		R_DisableAttribute(&p->tangent);
		R_ProgramParameter1i(&p->normalmap, 0);
		return;
	}

	R_EnableAttribute(&p->tangent);

	// first deal with the surface
	if (surf) {
		R_BindDeluxemapTexture(surf->deluxemap_texnum);
		R_ProgramParameter1i(&p->deluxemap, 1);
	} else
		R_ProgramParameter1i(&p->deluxemap, 0);

	// then the material
	const r_material_t *material = image ? &image->material : NULL;

	if (r_state.active_material == material)
		return;

	R_BindNormalmapTexture(image->normalmap->texnum);
	R_ProgramParameter1i(&p->normalmap, 1);

	if (image->glossmap) {
		R_BindGlossmapTexture(image->glossmap->texnum);
		R_ProgramParameter1i(&p->glossmap, 1);
	} else
		R_ProgramParameter1i(&p->glossmap, 0);

	R_ProgramParameter1f(&p->bump, material->bump * r_bumpmap->value);
	R_ProgramParameter1f(&p->parallax, material->parallax * r_parallax->value);
	R_ProgramParameter1f(&p->hardness, material->hardness * r_hardness->value);
	R_ProgramParameter1f(&p->specular, material->specular * r_specular->value);
}
