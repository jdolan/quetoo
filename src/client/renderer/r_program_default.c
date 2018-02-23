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

// these are the variables defined in the GLSL shader
typedef struct {
	r_program_t *program; // link back to program for attribs

	r_uniform1i_t diffuse;
	r_uniform1i_t lightmap;
	r_uniform1i_t deluxemap;
	r_uniform1i_t normalmap;
	r_uniform1i_t glossmap;
	r_uniform1i_t tintmap;
	r_uniform1i_t stainmap;

	r_uniform1f_t bump;
	r_uniform1f_t parallax;
	r_uniform1f_t hardness;
	r_uniform1f_t specular;

	r_uniform1f_t time_fraction;
	r_uniform1f_t time;

	r_sampler2d_t sampler0;
	r_sampler2d_t sampler1;
	r_sampler2d_t sampler2;
	r_sampler2d_t sampler3;
	r_sampler2d_t sampler4;
	r_sampler2d_t sampler6;
	r_sampler2d_t sampler8;

	r_uniform4fv_t tints[TINT_TOTAL];

	r_uniform_fog_t fog;
	r_uniform_light_t *lights;

	r_uniform_caustic_t caustic;

	r_uniform_matrix4fv_t normal_mat;

	r_uniform1f_t alpha_threshold;
} r_default_program_t;

static r_default_program_t r_default_program;

/**
 * @brief
 */
void R_PreLink_default(const r_program_t *program) {

	R_BindAttributeLocation(program, "POSITION", R_ATTRIB_POSITION);
	R_BindAttributeLocation(program, "COLOR", R_ATTRIB_COLOR);
	R_BindAttributeLocation(program, "TEXCOORD0", R_ATTRIB_DIFFUSE);
	R_BindAttributeLocation(program, "TEXCOORD1", R_ATTRIB_LIGHTMAP);
	R_BindAttributeLocation(program, "NORMAL", R_ATTRIB_NORMAL);
	R_BindAttributeLocation(program, "TANGENT", R_ATTRIB_TANGENT);
	R_BindAttributeLocation(program, "BITANGENT", R_ATTRIB_BITANGENT);

	R_BindAttributeLocation(program, "NEXT_POSITION", R_ATTRIB_NEXT_POSITION);
	R_BindAttributeLocation(program, "NEXT_NORMAL", R_ATTRIB_NEXT_NORMAL);
	R_BindAttributeLocation(program, "NEXT_TANGENT", R_ATTRIB_NEXT_TANGENT);
	R_BindAttributeLocation(program, "NEXT_BITANGENT", R_ATTRIB_NEXT_BITANGENT);
}

/**
 * @brief
 */
void R_InitProgram_default(r_program_t *program) {

	r_default_program_t *p = &r_default_program;

	p->program = program;

	R_ProgramVariable(&program->attributes[R_ATTRIB_POSITION], R_ATTRIBUTE, "POSITION", true);
	R_ProgramVariable(&program->attributes[R_ATTRIB_COLOR], R_ATTRIBUTE, "COLOR", true);
	R_ProgramVariable(&program->attributes[R_ATTRIB_DIFFUSE], R_ATTRIBUTE, "TEXCOORD0", true);
	R_ProgramVariable(&program->attributes[R_ATTRIB_LIGHTMAP], R_ATTRIBUTE, "TEXCOORD1", true);
	R_ProgramVariable(&program->attributes[R_ATTRIB_NORMAL], R_ATTRIBUTE, "NORMAL", true);
	R_ProgramVariable(&program->attributes[R_ATTRIB_TANGENT], R_ATTRIBUTE, "TANGENT", true);
	R_ProgramVariable(&program->attributes[R_ATTRIB_BITANGENT], R_ATTRIBUTE, "BITANGENT", true);

	R_ProgramVariable(&program->attributes[R_ATTRIB_NEXT_POSITION], R_ATTRIBUTE, "NEXT_POSITION", true);
	R_ProgramVariable(&program->attributes[R_ATTRIB_NEXT_NORMAL], R_ATTRIBUTE, "NEXT_NORMAL", true);
	R_ProgramVariable(&program->attributes[R_ATTRIB_NEXT_TANGENT], R_ATTRIBUTE, "NEXT_TANGENT", true);
	R_ProgramVariable(&program->attributes[R_ATTRIB_NEXT_BITANGENT], R_ATTRIBUTE, "NEXT_BITANGENT", true);

	R_ProgramVariable(&p->diffuse, R_UNIFORM_INT, "DIFFUSE", true);
	R_ProgramVariable(&p->lightmap, R_UNIFORM_INT, "LIGHTMAP", true);
	R_ProgramVariable(&p->deluxemap, R_UNIFORM_INT, "DELUXEMAP", true);
	R_ProgramVariable(&p->normalmap, R_UNIFORM_INT, "NORMALMAP", true);
	R_ProgramVariable(&p->glossmap, R_UNIFORM_INT, "GLOSSMAP", true);
	R_ProgramVariable(&p->tintmap, R_UNIFORM_INT, "TINTMAP", true);
	R_ProgramVariable(&p->stainmap, R_UNIFORM_INT, "STAINMAP", true);

	R_ProgramVariable(&p->bump, R_UNIFORM_FLOAT, "BUMP", true);
	R_ProgramVariable(&p->parallax, R_UNIFORM_FLOAT, "PARALLAX", true);
	R_ProgramVariable(&p->hardness, R_UNIFORM_FLOAT, "HARDNESS", true);
	R_ProgramVariable(&p->specular, R_UNIFORM_FLOAT, "SPECULAR", true);

	R_ProgramVariable(&p->sampler0, R_SAMPLER_2D, "SAMPLER0", true);
	R_ProgramVariable(&p->sampler1, R_SAMPLER_2D, "SAMPLER1", true);
	R_ProgramVariable(&p->sampler2, R_SAMPLER_2D, "SAMPLER2", true);
	R_ProgramVariable(&p->sampler3, R_SAMPLER_2D, "SAMPLER3", true);
	R_ProgramVariable(&p->sampler4, R_SAMPLER_2D, "SAMPLER4", true);
	R_ProgramVariable(&p->sampler6, R_SAMPLER_2D, "SAMPLER6", true);
	R_ProgramVariable(&p->sampler8, R_SAMPLER_2D, "SAMPLER8", true);

	R_ProgramVariable(&p->fog.start, R_UNIFORM_FLOAT, "FOG.START", true);
	R_ProgramVariable(&p->fog.end, R_UNIFORM_FLOAT, "FOG.END", true);
	R_ProgramVariable(&p->fog.color, R_UNIFORM_VEC3, "FOG.COLOR", true);
	R_ProgramVariable(&p->fog.density, R_UNIFORM_FLOAT, "FOG.DENSITY", true);

	for (int32_t i = 0; i < TINT_TOTAL; i++) {
		R_ProgramVariable(&p->tints[i], R_UNIFORM_VEC4, va("TINTS[%i]", i), true);
	}

	if (r_state.max_active_lights) {
		p->lights = Mem_TagMalloc(sizeof(r_uniform_light_t) * r_state.max_active_lights, MEM_TAG_RENDERER);

		for (int32_t i = 0; i < r_state.max_active_lights; ++i) {
			R_ProgramVariable(&p->lights[i].origin, R_UNIFORM_VEC3, va("LIGHTS.ORIGIN[%i]", i), true);
			R_ProgramVariable(&p->lights[i].color, R_UNIFORM_VEC3, va("LIGHTS.COLOR[%i]", i), true);
			R_ProgramVariable(&p->lights[i].radius, R_UNIFORM_FLOAT, va("LIGHTS.RADIUS[%i]", i), true);
		}

		R_ProgramParameter1f(&p->lights[0].radius, 0.0);
	} else {
		p->lights = NULL;
	}

	R_ProgramVariable(&p->caustic.enable, R_UNIFORM_INT, "CAUSTIC.ENABLE", true);
	R_ProgramVariable(&p->caustic.color, R_UNIFORM_VEC3, "CAUSTIC.COLOR", true);

	R_ProgramVariable(&p->normal_mat, R_UNIFORM_MAT4, "NORMAL_MAT", true);

	R_ProgramVariable(&p->alpha_threshold, R_UNIFORM_FLOAT, "ALPHA_THRESHOLD", true);

	R_ProgramVariable(&p->time_fraction, R_UNIFORM_FLOAT, "TIME_FRACTION", true);
	R_ProgramVariable(&p->time, R_UNIFORM_FLOAT, "TIME", true);

	R_ProgramParameter1i(&p->diffuse, 0);
	R_ProgramParameter1i(&p->lightmap, 0);
	R_ProgramParameter1i(&p->deluxemap, 0);
	R_ProgramParameter1i(&p->normalmap, 0);
	R_ProgramParameter1i(&p->glossmap, 0);
	R_ProgramParameter1i(&p->tintmap, 0);

	R_ProgramParameter1f(&p->bump, 1.0);
	R_ProgramParameter1f(&p->parallax, 1.0);
	R_ProgramParameter1f(&p->hardness, 1.0);
	R_ProgramParameter1f(&p->specular, 1.0);

	R_ProgramParameter1i(&p->sampler0, R_TEXUNIT_DIFFUSE);
	R_ProgramParameter1i(&p->sampler1, R_TEXUNIT_LIGHTMAP);
	R_ProgramParameter1i(&p->sampler2, R_TEXUNIT_DELUXEMAP);
	R_ProgramParameter1i(&p->sampler3, R_TEXUNIT_NORMALMAP);
	R_ProgramParameter1i(&p->sampler4, R_TEXUNIT_SPECULARMAP);
	R_ProgramParameter1i(&p->sampler6, R_TEXUNIT_TINTMAP);
	R_ProgramParameter1i(&p->sampler8, R_TEXUNIT_STAINMAP);

	R_ProgramParameter1f(&p->fog.density, 0.0);
	R_ProgramParameter1f(&p->alpha_threshold, ALPHA_TEST_DISABLED_THRESHOLD);

	R_ProgramParameter1i(&p->caustic.enable, 0);

	R_ProgramParameter1f(&p->time_fraction, 0.0f);
	R_ProgramParameter1f(&p->time, 0.0f);
}

/**
 * @brief
 */
void R_Shutdown_default(void) {

	r_default_program_t *p = &r_default_program;

	if (p->lights) {
		Mem_Free(p->lights);
	}
}

/**
 * @brief
 */
void R_UseProgram_default(void) {
	r_default_program_t *p = &r_default_program;

	R_ProgramParameter1i(&p->diffuse, texunit_diffuse->enabled);
	R_ProgramParameter1i(&p->deluxemap, texunit_deluxemap->enabled);
	R_ProgramParameter1i(&p->lightmap, texunit_lightmap->enabled);
	R_ProgramParameter1i(&p->stainmap, texunit_stainmap->enabled);
}

/**
 * @brief
 */
void R_UseMaterial_default(const r_material_t *material) {
	r_default_program_t *p = &r_default_program;

	if (!material || !material->normalmap ||
	        !r_bumpmap->value || r_draw_bsp_lightmaps->value ||
			!r_state.lighting_enabled) {

		R_DisableAttribute(R_ATTRIB_TANGENT);
		R_DisableAttribute(R_ATTRIB_BITANGENT);
		R_ProgramParameter1i(&p->normalmap, 0);
	} else {
		R_EnableAttribute(R_ATTRIB_TANGENT);
		R_EnableAttribute(R_ATTRIB_BITANGENT);

		R_BindNormalmapTexture(material->normalmap->texnum);
		R_ProgramParameter1i(&p->normalmap, 1);

		if (material->specularmap) {
			R_BindSpecularmapTexture(material->specularmap->texnum);
			R_ProgramParameter1i(&p->glossmap, 1);
		} else {
			R_ProgramParameter1i(&p->glossmap, 0);
		}

		R_ProgramParameter1f(&p->bump, material->cm->bump * r_bumpmap->value);
		R_ProgramParameter1f(&p->parallax, material->cm->parallax * r_parallax->value);
		R_ProgramParameter1f(&p->hardness, material->cm->hardness * r_hardness->value);
		R_ProgramParameter1f(&p->specular, material->cm->specular * r_specular->value);
	}
	
	if (material && material->tintmap) {
		R_BindTintTexture(material->tintmap->texnum);
		R_ProgramParameter1i(&p->tintmap, 1);
	} else {
		R_ProgramParameter1i(&p->tintmap, 0);
	}
}

/**
 * @brief
 */
void R_UseFog_default(const r_fog_parameters_t *fog) {
	r_default_program_t *p = &r_default_program;

	if (fog && fog->density) {
		R_ProgramParameter1f(&p->fog.density, fog->density);
		R_ProgramParameter1f(&p->fog.start, fog->start);
		R_ProgramParameter1f(&p->fog.end, fog->end);
		R_ProgramParameter3fv(&p->fog.color, fog->color);
	} else {
		R_ProgramParameter1f(&p->fog.density, 0.0);
	}
}

/**
 * @brief
 */
void R_UseLight_default(const uint16_t light_index, const matrix4x4_t *world_view, const r_light_t *light) {
	r_default_program_t *p = &r_default_program;

	if (light && light->radius) {
		vec3_t origin;
		Matrix4x4_Transform(world_view, light->origin, origin);

		R_ProgramParameter3fv(&p->lights[light_index].origin, origin);
		R_ProgramParameter3fv(&p->lights[light_index].color, light->color);
		R_ProgramParameter1f(&p->lights[light_index].radius, light->radius);
	} else {
		R_ProgramParameter1f(&p->lights[light_index].radius, 0.0);
	}
}

/**
 * @brief
 */
void R_UseCaustic_default(const r_caustic_parameters_t *caustic) {
	r_default_program_t *p = &r_default_program;

	if (caustic && caustic->enable) {
		R_ProgramParameter1i(&p->caustic.enable, caustic->enable);
		R_ProgramParameter3fv(&p->caustic.color, caustic->color);
		R_ProgramParameter1f(&p->time, r_view.ticks / 1000.0);
	} else {
		R_ProgramParameter1i(&p->caustic.enable, 0);
	}
}

/**
 * @brief
 */
void R_MatricesChanged_default(void) {
	r_default_program_t *p = &r_default_program;

	if (r_state.active_program->matrix_dirty[R_MATRIX_MODELVIEW]) {
		// recalculate normal matrix if the modelview has changed.
		static matrix4x4_t normalMatrix;

		R_GetMatrix(R_MATRIX_MODELVIEW, &normalMatrix);
		Matrix4x4_Invert_Full(&normalMatrix, &normalMatrix);
		Matrix4x4_Transpose(&normalMatrix, &normalMatrix);
		R_ProgramParameterMatrix4fv(&p->normal_mat, (const GLfloat *) normalMatrix.m);
	}
}

/**
 * @brief
 */
void R_UseAlphaTest_default(const vec_t threshold) {
	r_default_program_t *p = &r_default_program;

	R_ProgramParameter1f(&p->alpha_threshold, threshold);
}

/**
 * @brief
 */
void R_UseInterpolation_default(const vec_t time_fraction) {
	r_default_program_t *p = &r_default_program;

	R_ProgramParameter1f(&p->time_fraction, time_fraction);
}

/**
 * @brief
 */
void R_UseTints_default(void) {
	r_default_program_t *p = &r_default_program;

	if (!r_state.active_material->tintmap) {
		return;
	}

	for (int32_t i = 0; i < TINT_TOTAL; i++) {
		if (r_view.current_entity->tints[i][3]) {
			R_ProgramParameter4fv(&p->tints[i], r_view.current_entity->tints[i]);
		} else {
			R_ProgramParameter4fv(&p->tints[i], r_state.active_material->cm->tintmap_defaults[i]);
		}
	}
}
