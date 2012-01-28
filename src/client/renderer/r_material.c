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

#define UPDATE_THRESHOLD 0.02

/*
 * R_UpdateMaterial
 *
 * Materials "think" every few milliseconds to advance animations.
 */
static void R_UpdateMaterial(r_material_t *m) {
	r_stage_t *s;

	if (r_view.time - m->time < UPDATE_THRESHOLD)
		return;

	m->time = r_view.time;

	for (s = m->stages; s; s = s->next) {

		if (s->flags & STAGE_PULSE)
			s->pulse.dhz = (sin(r_view.time * s->pulse.hz * 6.28) + 1.0) / 2.0;

		if (s->flags & STAGE_STRETCH) {
			s->stretch.dhz = (sin(r_view.time * s->stretch.hz * 6.28) + 1.0)
					/ 2.0;
			s->stretch.damp = 1.5 - s->stretch.dhz * s->stretch.amp;
		}

		if (s->flags & STAGE_ROTATE)
			s->rotate.deg = r_view.time * s->rotate.hz * 360.0;

		if (s->flags & STAGE_SCROLL_S)
			s->scroll.ds = s->scroll.s * r_view.time;

		if (s->flags & STAGE_SCROLL_T)
			s->scroll.dt = s->scroll.t * r_view.time;

		if (s->flags & STAGE_ANIM) {
			if (r_view.time >= s->anim.dtime) { // change frames
				s->anim.dtime = r_view.time + (1.0 / s->anim.fps);
				s->image
						= s->anim.images[++s->anim.dframe % s->anim.num_frames];
			}
		}
	}
}

/*
 * R_StageLighting
 *
 * Manages state for stages supporting static, dynamic, and per-pixel lighting.
 */
static void R_StageLighting(r_bsp_surface_t *surf, r_stage_t *stage) {

	// if the surface has a lightmap, and the stage specifies lighting..

	if ((surf->flags & R_SURF_LIGHTMAP) && (stage->flags & (STAGE_LIGHTMAP
			| STAGE_LIGHTING))) {

		R_EnableTexture(&texunit_lightmap, true);
		R_BindLightmapTexture(surf->lightmap_texnum);

		if (stage->flags & STAGE_LIGHTING) { // hardware lighting

			R_EnableLighting(r_state.world_program, true);

			if (r_state.lighting_enabled) {

				if (r_bumpmap->value && stage->image->normalmap) {

					R_BindDeluxemapTexture(surf->deluxemap_texnum);
					R_BindNormalmapTexture(stage->image->normalmap->texnum);

					R_EnableBumpmap(&stage->image->material, true);
				} else {
					if (r_state.bumpmap_enabled)
						R_EnableBumpmap(NULL, false);
				}

				if (surf->light_frame == r_locals.light_frame) // dynamic light sources
					R_EnableLights(surf->lights);
				else
					R_EnableLights(0);
			}
		} else {
			R_EnableLighting(NULL, false);
		}
	} else {
		R_EnableLighting(NULL, false);

		R_EnableTexture(&texunit_lightmap, false);
	}
}

/*
 * R_StageVertex
 *
 * Generates a single vertex for the specified stage.
 */
static inline void R_StageVertex(const r_bsp_surface_t *surf,
		const r_stage_t *stage, const vec3_t in, vec3_t out) {

	// TODO: vertex deforms
	VectorCopy(in, out);
}

/*
 * R_StageTextureMatrix
 *
 * Manages texture matrix manipulations for stages supporting rotations,
 * scrolls, and stretches (rotate, translate, scale).
 */
static inline void R_StageTextureMatrix(r_bsp_surface_t *surf, r_stage_t *stage) {
	static boolean_t identity = true;
	float s, t;

	if (!(stage->flags & STAGE_TEXTURE_MATRIX)) {

		if (!identity)
			glLoadIdentity();

		identity = true;
		return;
	}

	glLoadIdentity();

	s = surf->st_center[0] / surf->texinfo->image->width;
	t = surf->st_center[1] / surf->texinfo->image->height;

	if (stage->flags & STAGE_STRETCH) {
		glTranslatef(-s, -t, 0.0);
		glScalef(stage->stretch.damp, stage->stretch.damp, 1.0);
		glTranslatef(-s, -t, 0.0);
	}

	if (stage->flags & STAGE_ROTATE) {
		glTranslatef(-s, -t, 0.0);
		glRotatef(stage->rotate.deg, 0.0, 0.0, 1.0);
		glTranslatef(-s, -t, 0.0);
	}

	if (stage->flags & STAGE_SCALE_S)
		glScalef(stage->scale.s, 1.0, 1.0);

	if (stage->flags & STAGE_SCALE_T)
		glScalef(1.0, stage->scale.t, 1.0);

	if (stage->flags & STAGE_SCROLL_S)
		glTranslatef(stage->scroll.ds, 0.0, 0.0);

	if (stage->flags & STAGE_SCROLL_T)
		glTranslatef(0.0, stage->scroll.dt, 0.0);

	identity = false;
}

/*
 * R_StageTexCoord
 *
 * Generates a single texture coordinate for the specified stage and vertex.
 */
static inline void R_StageTexCoord(const r_stage_t *stage, const vec3_t v,
		const vec2_t in, vec2_t out) {

	vec3_t tmp;

	if (stage->flags & STAGE_ENVMAP) { // generate texcoords

		VectorSubtract(v, r_view.origin, tmp);
		VectorNormalize(tmp);

		out[0] = tmp[0];
		out[1] = tmp[1];
	} else { // or use the ones we were given
		out[0] = in[0];
		out[1] = in[1];
	}
}

#define NUM_DIRTMAP_ENTRIES 16
static const float dirtmap[NUM_DIRTMAP_ENTRIES] = { 0.6, 0.5, 0.3, 0.4, 0.7,
		0.3, 0.0, 0.4, 0.5, 0.2, 0.8, 0.5, 0.3, 0.2, 0.5, 0.3 };

/*
 * R_StageColor
 *
 * Generates a single color for the specified stage and vertex.
 */
static inline void R_StageColor(const r_stage_t *stage, const vec3_t v,
		vec4_t color) {

	float a;

	if (stage->flags & STAGE_TERRAIN) {

		if (stage->flags & STAGE_COLOR) // honor stage color
			VectorCopy(stage->color, color);
		else
			// or use white
			VectorSet(color, 1.0, 1.0, 1.0);

		// resolve alpha for vert based on z axis height
		if (v[2] < stage->terrain.floor)
			a = 0.0;
		else if (v[2] > stage->terrain.ceil)
			a = 1.0;
		else
			a = (v[2] - stage->terrain.floor) / stage->terrain.height;

		color[3] = a;
	} else if (stage->flags & STAGE_DIRTMAP) {

		// resolve dirtmap based on vertex position
		const int index = (int) (v[0] + v[1]) % NUM_DIRTMAP_ENTRIES;
		if (stage->flags & STAGE_COLOR) // honor stage color
			VectorCopy(stage->color, color);
		else
			// or use white
			VectorSet(color, 1.0, 1.0, 1.0);

		color[3] = dirtmap[index] * stage->dirt.intensity;
	} else { // simply use white
		color[0] = color[1] = color[2] = color[3] = 1.0;
	}
}

/*
 * R_SetSurfaceStageState
 *
 * Manages all state for the specified surface and stage.
 */
static void R_SetSurfaceStageState(r_bsp_surface_t *surf, r_stage_t *stage) {
	vec4_t color;

	// bind the texture
	R_BindTexture(stage->image->texnum);

	// resolve all static, dynamic, and per-pixel lighting
	R_StageLighting(surf, stage);

	// load the texture matrix for rotations, stretches, etc..
	R_StageTextureMatrix(surf, stage);

	// set the blend function, ensuring a sane default
	if (stage->flags & STAGE_BLEND)
		R_BlendFunc(stage->blend.src, stage->blend.dest);
	else
		R_BlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	// for terrain, enable the color array
	if (stage->flags & (STAGE_TERRAIN | STAGE_DIRTMAP))
		R_EnableColorArray(true);
	else {
		R_EnableColorArray(false);

		// resolve the shade color

		if (stage->flags & STAGE_COLOR) // explicit
			VectorCopy(stage->color, color);

		else if (stage->flags & STAGE_ENVMAP) // implicit
			VectorCopy(surf->color, color);

		else
			// default
			VectorSet(color, 1.0, 1.0, 1.0);

		// modulate the alpha value for pulses
		if (stage->flags & STAGE_PULSE) {
			R_EnableFog(false); // disable fog, since it also sets alpha
			color[3] = stage->pulse.dhz;
		} else {
			R_EnableFog(true); // ensure fog is available
			color[3] = 1.0;
		}

		glColor4fv(color);
	}
}

/*
 * R_DrawSurfaceStage
 *
 * Render the specified stage for the surface.  Resolve vertex attributes via
 * helper functions, outputting to the default vertex arrays.
 */
static void R_DrawSurfaceStage(r_bsp_surface_t *surf, r_stage_t *stage) {
	int i;

	for (i = 0; i < surf->num_edges; i++) {

		const float *v = &r_world_model->verts[surf->index * 3 + i * 3];
		const float *st = &r_world_model->texcoords[surf->index * 2 + i * 2];

		R_StageVertex(surf, stage, v, &r_state.vertex_array_3d[i * 3]);

		R_StageTexCoord(stage, v, st, &texunit_diffuse.texcoord_array[i * 2]);

		if (texunit_lightmap.enabled) { // lightmap texcoords
			st = &r_world_model->lmtexcoords[surf->index * 2 + i * 2];
			texunit_lightmap.texcoord_array[i * 2 + 0] = st[0];
			texunit_lightmap.texcoord_array[i * 2 + 1] = st[1];
		}

		if (r_state.color_array_enabled) // colors
			R_StageColor(stage, v, &r_state.color_array[i * 4]);

		if (r_state.lighting_enabled) { // normals and tangents

			const float *n = &r_world_model->normals[surf->index * 3 + i * 3];
			VectorCopy(n, (&r_state.normal_array[i * 3]));

			if (r_state.bumpmap_enabled) {
				const float *t = &r_world_model->tangents[surf->index * 4 + i
						* 4];
				VectorCopy(t, (&r_state.tangent_array[i * 4]));
			}
		}
	}

	glDrawArrays(GL_POLYGON, 0, i);
}

/*
 * R_DrawMaterialSurfaces
 *
 * Iterates the specified surfaces list, updating materials as they are
 * encountered, and rendering all visible stages.  State is lazily managed
 * throughout the iteration, so there is a concerted effort to restore the
 * state after all surface stages have been rendered.
 */
void R_DrawMaterialSurfaces(r_bsp_surfaces_t *surfs) {
	r_material_t *m;
	r_stage_t *s;
	unsigned int i;

	if (!r_materials->value || r_draw_wireframe->value)
		return;

	if (!surfs->count)
		return;

	R_EnableTexture(&texunit_lightmap, true);

	R_EnableLighting(r_state.world_program, true);

	R_EnableColorArray(true);

	R_ResetArrayState();

	R_EnableColorArray(false);

	R_EnableLighting(NULL, false);

	R_EnableTexture(&texunit_lightmap, false);

	glEnable(GL_POLYGON_OFFSET_FILL); // all stages use depth offset

	glMatrixMode(GL_TEXTURE); // some stages will manipulate texcoords

	for (i = 0; i < surfs->count; i++) {
		int j = -1;

		r_bsp_surface_t *surf = surfs->surfaces[i];

		if (surf->frame != r_locals.frame)
			continue;

		m = &surf->texinfo->image->material;

		R_UpdateMaterial(m);

		for (s = m->stages; s; s = s->next, j--) {

			if (!(s->flags & STAGE_RENDER))
				continue;

			if (r_view.render_mode == render_mode_pro) {
				// skip lighted stages in pro renderer
				if (s->flags & STAGE_LIGHTING)
					continue;
			}

			glPolygonOffset(j, 0.0); // increase depth offset for each stage

			R_SetSurfaceStageState(surf, s);

			R_DrawSurfaceStage(surf, s);
		}
	}

	glPolygonOffset(0.0, 0.0);
	glDisable(GL_POLYGON_OFFSET_FILL);

	glLoadIdentity();
	glMatrixMode(GL_MODELVIEW);

	R_BlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	R_EnableFog(true);

	R_EnableColorArray(false);

	R_EnableTexture(&texunit_lightmap, false);

	R_EnableLights(0);

	R_EnableBumpmap(NULL, false);

	R_EnableLighting(NULL, false);

	glColor4ubv(color_white);
}

/*
 * R_ClearMaterials
 */
static void R_ClearMaterials(void) {
	int i;

	// clear previously loaded materials
	for (i = 0; i < MAX_GL_TEXTURES; i++) {

		r_material_t *m = &r_images[i].material;
		r_stage_t *s = m->stages;

		while (s) { // free the stages chain
			r_stage_t *ss = s->next;
			Z_Free(s);
			s = ss;
		}

		memset(m, 0, sizeof(*m));

		m->bump = DEFAULT_BUMP;
		m->parallax = DEFAULT_PARALLAX;
		m->hardness = DEFAULT_HARDNESS;
		m->specular = DEFAULT_SPECULAR;
	}
}

/*
 * R_ConstByName
 */
static inline GLenum R_ConstByName(const char *c) {

	if (!strcmp(c, "GL_ONE"))
		return GL_ONE;
	if (!strcmp(c, "GL_ZERO"))
		return GL_ZERO;
	if (!strcmp(c, "GL_SRC_ALPHA"))
		return GL_SRC_ALPHA;
	if (!strcmp(c, "GL_ONE_MINUS_SRC_ALPHA"))
		return GL_ONE_MINUS_SRC_ALPHA;
	if (!strcmp(c, "GL_SRC_COLOR"))
		return GL_SRC_COLOR;
	if (!strcmp(c, "GL_DST_COLOR"))
		return GL_DST_COLOR;
	if (!strcmp(c, "GL_ONE_MINUS_SRC_COLOR"))
		return GL_ONE_MINUS_SRC_COLOR;

	// ...

	Com_Warn("R_ConstByName: Failed to resolve: %s\n", c);
	return GL_INVALID_ENUM;
}

/*
 * R_LoadAnimImages
 */
static int R_LoadAnimImages(r_stage_t *s) {
	char name[MAX_QPATH];
	int i, j, k;

	if (!s->image) {
		Com_Warn("R_LoadAnimImages: Texture not defined in anim stage.\n");
		return -1;
	}

	strncpy(name, s->image->name, sizeof(name));
	j = strlen(name);

	if ((i = atoi(&name[j - 1])) < 0) {
		Com_Warn(
				"R_LoadAnimImages: Texture name does not end in numeric: %s\n",
				name);
		return -1;
	}

	// the first image was already loaded by the stage parse, so just copy
	// the pointer into the images array

	s->anim.images[0] = s->image;
	name[j - 1] = 0;

	// now load the rest
	for (k = 1, i = i + 1; k < s->anim.num_frames; k++, i++) {

		const char *c = va("%s%d", name, i);
		s->anim.images[k] = R_LoadImage(c, it_material);

		if (s->anim.images[k] == r_null_image) {
			Com_Warn("R_LoadAnimImages: Failed to resolve texture: %s\n", c);
			return -1;
		}
	}

	return 0;
}

/*
 * R_ParseStage
 */
static int R_ParseStage(r_stage_t *s, const char **buffer) {
	int i;

	while (true) {

		const char *c = ParseToken(buffer);

		if (*c == '\0')
			break;

		if (!strcmp(c, "texture")) {

			c = ParseToken(buffer);
			s->image = R_LoadImage(va("textures/%s", c), it_material);

			if (s->image == r_null_image) {
				Com_Warn("R_ParseStage: Failed to resolve texture: %s\n", c);
				return -1;
			}

			s->flags |= STAGE_TEXTURE;
			continue;
		}

		if (!strcmp(c, "envmap")) {

			c = ParseToken(buffer);
			i = atoi(c);

			if (i > -1 && i < NUM_ENVMAP_IMAGES)
				s->image = r_envmap_images[i];
			else
				s->image = R_LoadImage(va("envmaps/%s", c), it_material);

			if (s->image == r_null_image) {
				Com_Warn("R_ParseStage: Failed to resolve envmap: %s\n", c);
				return -1;
			}

			s->flags |= STAGE_ENVMAP;
			continue;
		}

		if (!strcmp(c, "blend")) {

			c = ParseToken(buffer);
			s->blend.src = R_ConstByName(c);

			if (s->blend.src == GL_INVALID_ENUM) {
				Com_Warn("R_ParseStage: Failed to resolve blend src: %s\n", c);
				return -1;
			}

			c = ParseToken(buffer);
			s->blend.dest = R_ConstByName(c);

			if (s->blend.dest == GL_INVALID_ENUM) {
				Com_Warn("R_ParseStage: Failed to resolve blend dest: %s\n", c);
				return -1;
			}

			s->flags |= STAGE_BLEND;
			continue;
		}

		if (!strcmp(c, "color")) {

			for (i = 0; i < 3; i++) {

				c = ParseToken(buffer);
				s->color[i] = atof(c);

				if (s->color[i] < 0.0 || s->color[i] > 1.0) {
					Com_Warn("R_ParseStage: Failed to resolve color: %s\n", c);
					return -1;
				}
			}

			s->flags |= STAGE_COLOR;
			continue;
		}

		if (!strcmp(c, "pulse")) {

			c = ParseToken(buffer);
			s->pulse.hz = atof(c);

			if (s->pulse.hz < 0.0) {
				Com_Warn("R_ParseStage: Failed to resolve frequency: %s\n", c);
				return -1;
			}

			s->flags |= STAGE_PULSE;
			continue;
		}

		if (!strcmp(c, "stretch")) {

			c = ParseToken(buffer);
			s->stretch.amp = atof(c);

			if (s->stretch.amp < 0.0) {
				Com_Warn("R_ParseStage: Failed to resolve amplitude: %s\n", c);
				return -1;
			}

			c = ParseToken(buffer);
			s->stretch.hz = atof(c);

			if (s->stretch.hz < 0.0) {
				Com_Warn("R_ParseStage: Failed to resolve frequency: %s\n", c);
				return -1;
			}

			s->flags |= STAGE_STRETCH;
			continue;
		}

		if (!strcmp(c, "rotate")) {

			c = ParseToken(buffer);
			s->rotate.hz = atof(c);

			if (s->rotate.hz < 0.0) {
				Com_Warn("R_ParseStage: Failed to resolve rotate: %s\n", c);
				return -1;
			}

			s->flags |= STAGE_ROTATE;
			continue;
		}

		if (!strcmp(c, "scroll.s")) {

			c = ParseToken(buffer);
			s->scroll.s = atof(c);

			s->flags |= STAGE_SCROLL_S;
			continue;
		}

		if (!strcmp(c, "scroll.t")) {

			c = ParseToken(buffer);
			s->scroll.t = atof(c);

			s->flags |= STAGE_SCROLL_T;
			continue;
		}

		if (!strcmp(c, "scale.s")) {

			c = ParseToken(buffer);
			s->scale.s = atof(c);

			s->flags |= STAGE_SCALE_S;
			continue;
		}

		if (!strcmp(c, "scale.t")) {

			c = ParseToken(buffer);
			s->scale.t = atof(c);

			s->flags |= STAGE_SCALE_T;
			continue;
		}

		if (!strcmp(c, "terrain")) {

			c = ParseToken(buffer);
			s->terrain.floor = atof(c);

			c = ParseToken(buffer);
			s->terrain.ceil = atof(c);

			if (s->terrain.ceil < s->terrain.floor) {
				Com_Warn("R_ParseStage: Inverted terrain ceiling and floor "
					"values for %s\n", (s->image ? s->image->name : "NULL"));
				return -1;
			}

			s->terrain.height = s->terrain.ceil - s->terrain.floor;

			if (s->terrain.height == 0.0) {
				Com_Warn(
						"R_ParseStage: Zero height terrain specified for %s\n",
						(s->image ? s->image->name : "NULL"));
				return -1;
			}

			s->flags |= STAGE_TERRAIN;
			continue;
		}

		if (!strcmp(c, "dirtmap")) {

			c = ParseToken(buffer);
			s->dirt.intensity = atof(c);

			if (s->dirt.intensity <= 0.0 || s->dirt.intensity > 1.0) {
				Com_Warn("R_ParseStage: Invalid dirtmap intensity for %s\n",
						(s->image ? s->image->name : "NULL"));
				return -1;
			}

			s->flags |= STAGE_DIRTMAP;
			continue;
		}

		if (!strcmp(c, "anim")) {

			c = ParseToken(buffer);
			s->anim.num_frames = atoi(c);

			if (s->anim.num_frames < 1 || s->anim.num_frames > MAX_ANIM_FRAMES) {
				Com_Warn(
						"R_ParseStage: Invalid number of anim frames for %s\n",
						(s->image ? s->image->name : "NULL"));
				return -1;
			}

			c = ParseToken(buffer);
			s->anim.fps = atof(c);

			if (s->anim.fps <= 0.0) {
				Com_Warn("R_ParseStage: Invalid anim fps for %s\n",
						(s->image ? s->image->name : "NULL"));
				return -1;
			}

			// the frame images are loaded once the stage is parsed completely

			s->flags |= STAGE_ANIM;
			continue;
		}

		if (!strcmp(c, "lightmap")) {
			s->flags |= STAGE_LIGHTMAP;
			continue;
		}

		if (!strcmp(c, "flare")) {

			c = ParseToken(buffer);
			i = atoi(c);

			if (i > -1 && i < NUM_FLARE_IMAGES)
				s->image = r_flare_images[i];
			else
				s->image = R_LoadImage(va("flares/%s", c), it_material);

			if (s->image == r_null_image) {
				Com_Warn("R_ParseStage: Failed to resolve flare: %s\n", c);
				return -1;
			}

			s->flags |= STAGE_FLARE;
			continue;
		}

		if (*c == '}') {

			Com_Debug("Parsed stage\n"
				"  flags: %d\n"
				"  image: %s\n"
				"  blend: %d %d\n"
				"  color: %3f %3f %3f\n"
				"  pulse: %3f\n"
				"  stretch: %3f %3f\n"
				"  rotate: %3f\n"
				"  scroll.s: %3f\n"
				"  scroll.t: %3f\n"
				"  scale.s: %3f\n"
				"  scale.t: %3f\n"
				"  terrain.floor: %5f\n"
				"  terrain.ceil: %5f\n"
				"  anim.num_frames: %d\n"
				"  anim.fps: %3f\n", s->flags,
					(s->image ? s->image->name : "NULL"), s->blend.src,
					s->blend.dest, s->color[0], s->color[1], s->color[2],
					s->pulse.hz, s->stretch.amp, s->stretch.hz, s->rotate.hz,
					s->scroll.s, s->scroll.t, s->scale.s, s->scale.t,
					s->terrain.floor, s->terrain.ceil, s->anim.num_frames,
					s->anim.fps);

			// a texture, or envmap means render it
			if (s->flags & (STAGE_TEXTURE | STAGE_ENVMAP))
				s->flags |= STAGE_RENDER;

			if (s->flags & (STAGE_TERRAIN | STAGE_DIRTMAP))
				s->flags |= STAGE_LIGHTING;

			return 0;
		}
	}

	Com_Warn("R_ParseStage: Malformed stage\n");
	return -1;
}

/*
 * R_LoadMaterials
 */
void R_LoadMaterials(const char *map) {
	char path[MAX_QPATH];
	void *buf;
	const char *c;
	const char *buffer;
	boolean_t inmaterial;
	r_image_t *image;
	r_material_t *m;
	r_stage_t *s, *ss;
	int i;

	// clear previously loaded materials
	R_ClearMaterials();

	// load the materials file for parsing
	snprintf(path, sizeof(path), "materials/%s", Basename(map));
	strcpy(path + strlen(path) - 3, "mat");

	if ((i = Fs_LoadFile(path, &buf)) < 1) {
		Com_Debug("Couldn't load %s\n", path);
		return;
	}

	buffer = (char *) buf;

	inmaterial = false;
	image = NULL;
	m = NULL;

	while (true) {

		c = ParseToken(&buffer);

		if (*c == '\0')
			break;

		if (*c == '{' && !inmaterial) {
			inmaterial = true;
			continue;
		}

		if (!strcmp(c, "material")) {
			c = ParseToken(&buffer);
			image = R_LoadImage(va("textures/%s", c), it_world);

			if (image == r_null_image) {
				Com_Warn("R_LoadMaterials: Failed to resolve texture: %s\n", c);
				image = NULL;
			}

			continue;
		}

		if (!image)
			continue;

		m = &image->material;

		if (!strcmp(c, "normalmap")) {
			c = ParseToken(&buffer);
			image->normalmap = R_LoadImage(va("textures/%s", c), it_normalmap);

			if (image->normalmap == r_null_image) {
				Com_Warn("R_LoadMaterials: Failed to resolve normalmap: %s\n",
						c);
				image->normalmap = NULL;
			}
		}

		if (!strcmp(c, "bump")) {

			m->bump = atof(ParseToken(&buffer));

			if (m->bump < 0.0) {
				Com_Warn("R_LoadMaterials: Invalid bump value for %s\n",
						image->name);
				m->bump = DEFAULT_BUMP;
			}
		}

		if (!strcmp(c, "parallax")) {

			m->parallax = atof(ParseToken(&buffer));

			if (m->parallax < 0.0) {
				Com_Warn("R_LoadMaterials: Invalid parallax value for %s\n",
						image->name);
				m->parallax = DEFAULT_PARALLAX;
			}
		}

		if (!strcmp(c, "hardness")) {

			m->hardness = atof(ParseToken(&buffer));

			if (m->hardness < 0.0) {
				Com_Warn("R_LoadMaterials: Invalid hardness value for %s\n",
						image->name);
				m->hardness = DEFAULT_HARDNESS;
			}
		}

		if (!strcmp(c, "specular")) {
			m->specular = atof(ParseToken(&buffer));

			if (m->specular < 0.0) {
				Com_Warn("R_LoadMaterials: Invalid specular value for %s\n",
						image->name);
				m->specular = DEFAULT_SPECULAR;
			}
		}

		if (*c == '{' && inmaterial) {

			s = (r_stage_t *) Z_Malloc(sizeof(*s));

			if (R_ParseStage(s, &buffer) == -1) {
				Z_Free(s);
				continue;
			}

			// load animation frame images
			if (s->flags & STAGE_ANIM) {
				if (R_LoadAnimImages(s) == -1) {
					Z_Free(s);
					continue;
				}
			}

			// append the stage to the chain
			if (!m->stages)
				m->stages = s;
			else {
				ss = m->stages;
				while (ss->next)
					ss = ss->next;
				ss->next = s;
			}

			m->flags |= s->flags;
			m->num_stages++;
			continue;
		}

		if (*c == '}' && inmaterial) {
			Com_Debug("Parsed material %s with %d stages\n", image->name,
					m->num_stages);
			inmaterial = false;
			image = NULL;
		}
	}

	Fs_FreeFile(buf);
}
