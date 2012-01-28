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

r_state_t r_state;

const float default_texcoords[] = {  // useful for particles, pics, etc..
	0.0, 0.0, 1.0, 0.0, 1.0, 1.0, 0.0, 1.0
};


/*
 * R_SelectTexture
 */
void R_SelectTexture(r_texunit_t *texunit) {

	if (texunit == r_state.active_texunit)
		return;

	r_state.active_texunit = texunit;

	qglActiveTexture(texunit->texture);
	qglClientActiveTexture(texunit->texture);
}

/*
 * R_BindTexture
 */
void R_BindTexture(GLuint texnum) {

	if (texnum == r_state.active_texunit->texnum)
		return;

	r_state.active_texunit->texnum = texnum;

	glBindTexture(GL_TEXTURE_2D, texnum);
}

/*
 * R_BindLightmapTexture
 */
void R_BindLightmapTexture(GLuint texnum) {

	if (texnum == texunit_lightmap.texnum)
		return; // small optimization to save state changes

	R_SelectTexture(&texunit_lightmap);

	R_BindTexture(texnum);

	R_SelectTexture(&texunit_diffuse);
}

/*
 * R_BindDeluxemapTexture
 */
void R_BindDeluxemapTexture(GLuint texnum) {

	if (texnum == texunit_deluxemap.texnum)
		return; // small optimization to save state changes

	R_SelectTexture(&texunit_deluxemap);

	R_BindTexture(texnum);

	R_SelectTexture(&texunit_diffuse);
}

/*
 * R_BindNormalmapTexture
 */
void R_BindNormalmapTexture(GLuint texnum) {

	if (texnum == texunit_normalmap.texnum)
		return; // small optimization to save state changes

	R_SelectTexture(&texunit_normalmap);

	R_BindTexture(texnum);

	R_SelectTexture(&texunit_diffuse);
}

/*
 * R_BindArray
 */
void R_BindArray(GLenum target, GLenum type, GLvoid *array) {

	switch (target) {
	case GL_VERTEX_ARRAY:
		if (r_state.ortho)
			glVertexPointer(2, type, 0, array);
		else
			glVertexPointer(3, type, 0, array);
		break;
	case GL_TEXTURE_COORD_ARRAY:
		glTexCoordPointer(2, type, 0, array);
		break;
	case GL_COLOR_ARRAY:
		glColorPointer(4, type, 0, array);
		break;
	case GL_NORMAL_ARRAY:
		glNormalPointer(type, 0, array);
		break;
	case GL_TANGENT_ARRAY:
		R_AttributePointer("TANGENT", 4, array);
		break;
	default:
		break;
	}
}

/*
 * R_BindDefaultArray
 *
 * Binds the appropriate shared vertex array to the specified target.
 */
void R_BindDefaultArray(GLenum target) {

	switch (target) {
	case GL_VERTEX_ARRAY:
		if (r_state.ortho)
			R_BindArray(target, GL_SHORT, r_state.vertex_array_2d);
		else
			R_BindArray(target, GL_FLOAT, r_state.vertex_array_3d);
		break;
	case GL_TEXTURE_COORD_ARRAY:
		R_BindArray(target, GL_FLOAT, r_state.active_texunit->texcoord_array);
		break;
	case GL_COLOR_ARRAY:
		R_BindArray(target, GL_FLOAT, r_state.color_array);
		break;
	case GL_NORMAL_ARRAY:
		R_BindArray(target, GL_FLOAT, r_state.normal_array);
		break;
	case GL_TANGENT_ARRAY:
		R_BindArray(target, GL_FLOAT, r_state.tangent_array);
		break;
	default:
		break;
	}
}

/*
 * R_BindBuffer
 */
void R_BindBuffer(GLenum target, GLenum type, GLuint id) {

	if (!qglBindBuffer)
		return;

	if (!r_vertex_buffers->value)
		return;

	qglBindBuffer(GL_ARRAY_BUFFER, id);

	if (type && id) // assign the array pointer as well
		R_BindArray(target, type, NULL);
}

/*
 * R_BlendFunc
 */
void R_BlendFunc(GLenum src, GLenum dest) {

	if (r_state.blend_src == src && r_state.blend_dest == dest)
		return;

	r_state.blend_src = src;
	r_state.blend_dest = dest;

	glBlendFunc(src, dest);
}

/*
 * R_EnableBlend
 */
void R_EnableBlend(boolean_t enable) {

	if (r_state.blend_enabled == enable)
		return;

	r_state.blend_enabled = enable;

	if (enable) {
		glEnable(GL_BLEND);
		glDepthMask(GL_FALSE);
	} else {
		glDisable(GL_BLEND);
		glDepthMask(GL_TRUE);
	}
}

/*
 * R_EnableAlphaTest
 */
void R_EnableAlphaTest(boolean_t enable) {

	if (r_state.alpha_test_enabled == enable)
		return;

	r_state.alpha_test_enabled = enable;

	if (enable)
		glEnable(GL_ALPHA_TEST);
	else
		glDisable(GL_ALPHA_TEST);
}

/*
 * R_EnableStencilTest
 */
void R_EnableStencilTest(boolean_t enable) {

	if (r_state.stencil_test_enabled == enable)
		return;

	r_state.stencil_test_enabled = enable;

	if (enable)
		glEnable(GL_STENCIL_TEST);
	else
		glDisable(GL_STENCIL_TEST);
}

/*
 * R_EnableTexture
 */
void R_EnableTexture(r_texunit_t *texunit, boolean_t enable) {

	if (enable == texunit->enabled)
		return;

	texunit->enabled = enable;

	R_SelectTexture(texunit);

	if (enable) { // activate texture unit

		glEnable(GL_TEXTURE_2D);

		glEnableClientState(GL_TEXTURE_COORD_ARRAY);

		if (texunit == &texunit_lightmap) {

			if (r_draw_lightmaps->value)
				glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
			else
				glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
		}
	} else { // or deactivate it
		glDisable(GL_TEXTURE_2D);

		glDisableClientState(GL_TEXTURE_COORD_ARRAY);
	}

	R_SelectTexture(&texunit_diffuse);
}

/*
 * R_EnableColorArray
 */
void R_EnableColorArray(boolean_t enable) {

	if (r_state.color_array_enabled == enable)
		return;

	r_state.color_array_enabled = enable;

	if (enable)
		glEnableClientState(GL_COLOR_ARRAY);
	else
		glDisableClientState(GL_COLOR_ARRAY);
}

/*
 * R_EnableLighting
 *
 * Enables hardware-accelerated lighting with the specified program.  This
 * should be called after any texture units which will be active for lighting
 * have been enabled.
 */
void R_EnableLighting(r_program_t *program, boolean_t enable) {

	if (!r_programs->value)
		return;

	if (enable && (!program || !program->id))
		return;

	if (!r_lighting->value || r_state.lighting_enabled == enable)
		return;

	r_state.lighting_enabled = enable;

	if (enable) { // toggle state
		R_UseProgram(program);

		R_ResetLights();

		glEnableClientState(GL_NORMAL_ARRAY);
	} else {
		glDisableClientState(GL_NORMAL_ARRAY);

		R_UseProgram(NULL);
	}
}

/*
 * R_UseMaterial
 */
static inline void R_UseMaterial(r_material_t *material) {
	static float last_b, last_p, last_s, last_h;
	float b, p, s, h;

	if (r_state.active_material == material)
		return;

	r_state.active_material = material;

	if (!r_state.active_material)
		return;

	b = r_state.active_material->bump * r_bumpmap->value;
	if (b != last_b)
		R_ProgramParameter1f("BUMP", b);
	last_b = b;

	p = r_state.active_material->parallax * r_parallax->value;
	if (p != last_p)
		R_ProgramParameter1f("PARALLAX", p);
	last_p = p;

	h = r_state.active_material->hardness * r_hardness->value;
	if (h != last_h)
		R_ProgramParameter1f("HARDNESS", h);
	last_h = h;

	s = r_state.active_material->specular * r_specular->value;
	if (s != last_s)
		R_ProgramParameter1f("SPECULAR", s);
	last_s = s;
}

/*
 * R_EnableBumpmap
 *
 * Enables bumpmapping while updating program parameters to reflect the
 * specified material.
 */
void R_EnableBumpmap(r_material_t *material, boolean_t enable) {

	if (!r_state.lighting_enabled)
		return;

	if (!r_bumpmap->value)
		return;

	R_UseMaterial(material);

	if (r_state.bumpmap_enabled == enable)
		return;

	r_state.bumpmap_enabled = enable;

	if (enable) { // toggle state
		R_EnableAttribute("TANGENT");

		R_ProgramParameter1i("BUMPMAP", 1);
	} else {
		R_DisableAttribute("TANGENT");

		R_ProgramParameter1i("BUMPMAP", 0);
	}
}

/*
 * R_EnableWarp
 */
void R_EnableWarp(r_program_t *program, boolean_t enable) {

	if (!r_programs->value)
		return;

	if (enable && (!program || !program->id))
		return;

	if (!r_warp->value || r_state.warp_enabled == enable)
		return;

	r_state.warp_enabled = enable;

	R_SelectTexture(&texunit_lightmap);

	if (enable) {
		glEnable(GL_TEXTURE_2D);

		R_BindTexture(r_warp_image->texnum);

		R_UseProgram(program);
	} else {
		glDisable(GL_TEXTURE_2D);

		R_UseProgram(NULL);
	}

	R_SelectTexture(&texunit_diffuse);
}

/*
 * R_EnableColorShell
 */
void R_EnableShell(boolean_t enable) {

	if (enable == r_state.shell_enabled)
		return;

	r_state.shell_enabled = enable;

	if (enable) {
		glEnable(GL_POLYGON_OFFSET_FILL);
		glPolygonOffset(-1.0, 0.0);

		R_EnableBlend(true);
		R_BlendFunc(GL_SRC_ALPHA, GL_ONE);

		if (r_state.lighting_enabled)
			R_ProgramParameter1f("OFFSET", r_view.time / 3.0);
	} else {
		R_BlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		R_EnableBlend(false);

		glPolygonOffset(0.0, 0.0);
		glDisable(GL_POLYGON_OFFSET_FILL);

		if (r_state.lighting_enabled)
			R_ProgramParameter1f("OFFSET", 0.0);
	}
}

/*
 * R_EnableFog
 */
void R_EnableFog(boolean_t enable) {

	if (!r_fog->value || r_state.fog_enabled == enable)
		return;

	r_state.fog_enabled = false;

	if (enable) {
		if ((r_view.weather & WEATHER_FOG) || r_fog->integer == 2) {

			r_state.fog_enabled = true;

			glFogfv(GL_FOG_COLOR, r_view.fog_color);

			glFogf(GL_FOG_DENSITY, 1.0);
			glEnable(GL_FOG);
		}
	} else {
		glFogf(GL_FOG_DENSITY, 0.0);
		glDisable(GL_FOG);
	}
}

#define NEAR_Z 4
#define FAR_Z 16384

/*
 * R_Setup3D
 */
void R_Setup3D(void) {
	float xmin, xmax, ymin, ymax;
	float aspect;

	if (!r_view.width || !r_view.height)
		return;

	glViewport(r_view.x, r_view.y, r_view.width, r_view.height);

	// set up projection matrix
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();

	aspect = (float) r_view.width / (float) r_view.height;

	ymax = NEAR_Z * tan(r_view.fov[1] * M_PI / 360.0);
	ymin = -ymax;

	xmin = ymin * aspect;
	xmax = ymax * aspect;

	glFrustum(xmin, xmax, ymin, ymax, NEAR_Z, FAR_Z);

	// setup the model-view matrix
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();

	glRotatef(-90.0, 1.0, 0.0, 0.0); // put Z going up
	glRotatef(90.0, 0.0, 0.0, 1.0); // put Z going up

	glRotatef(-r_view.angles[2], 1.0, 0.0, 0.0);
	glRotatef(-r_view.angles[0], 0.0, 1.0, 0.0);
	glRotatef(-r_view.angles[1], 0.0, 0.0, 1.0);

	glTranslatef(-r_view.origin[0], -r_view.origin[1], -r_view.origin[2]);

	r_state.ortho = false;

	// bind default vertex array
	R_BindDefaultArray(GL_VERTEX_ARRAY);

	glDisable(GL_BLEND);

	glEnable(GL_DEPTH_TEST);
}

/*
 * R_Setup2D
 */
void R_Setup2D(void) {

	glViewport(0, 0, r_context.width, r_context.height);

	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();

	glOrtho(0, r_context.width, r_context.height, 0, -1, 1);

	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();

	r_state.ortho = true;

	// bind default vertex array
	R_BindDefaultArray(GL_VERTEX_ARRAY);

	// and set default texcoords for all 2d pics
	memcpy(texunit_diffuse.texcoord_array, default_texcoords, sizeof(vec2_t) * 4);

	glEnable(GL_BLEND);

	glDisable(GL_DEPTH_TEST);
}

/*
 * R_SetDEfaultState
 *
 * Sets OpenGL state parameters to appropiate defaults.
 */
void R_SetDefaultState(void) {
	int i;
	r_texunit_t *tex;

	// setup vertex array pointers
	glEnableClientState(GL_VERTEX_ARRAY);
	R_BindDefaultArray(GL_VERTEX_ARRAY);

	R_EnableColorArray(true);
	R_BindDefaultArray(GL_COLOR_ARRAY);
	R_EnableColorArray(false);

	glEnableClientState(GL_NORMAL_ARRAY);
	R_BindDefaultArray(GL_NORMAL_ARRAY);
	glDisableClientState(GL_NORMAL_ARRAY);

	// setup texture units
	for (i = 0; i < r_config.max_texunits && i < MAX_GL_TEXUNITS; i++) {

		tex = &r_state.texunits[i];
		tex->texture = GL_TEXTURE0_ARB + i;

		R_EnableTexture(tex, true);

		R_BindDefaultArray(GL_TEXTURE_COORD_ARRAY);

		if (i > 0) // turn them off for now
			R_EnableTexture(tex, false);
	}

	R_SelectTexture(&texunit_diffuse);

	// alpha test parameters
	glAlphaFunc(GL_GREATER, 0.25);

	// stencil test parameters
	glStencilFunc(GL_GEQUAL, 1, 0xff);
	glStencilOp(GL_KEEP, GL_KEEP, GL_INCR);

	// fog parameters
	glFogi(GL_FOG_MODE, GL_LINEAR);
	glFogf(GL_FOG_DENSITY, 0.0);
	glFogf(GL_FOG_START, FOG_START);
	glFogf(GL_FOG_END, FOG_END);

	// alpha blend parameters
	R_BlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
}

/*
 * R_InitState
 */
void R_InitState(void) {
	memset(&r_state, 0, sizeof(r_state));
}
