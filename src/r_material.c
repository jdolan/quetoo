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

#define UPDATE_THRESHOLD 0.02


/*
 * R_UpdateMaterial
 */
static void R_UpdateMaterial(material_t *m){
	stage_t *s;

	if(r_view.time - m->time < UPDATE_THRESHOLD)
		return;

	m->time = r_view.time;

	for(s = m->stages; s; s = s->next){

		if(s->flags & STAGE_PULSE)
			s->pulse.dhz = (sin(r_view.time * s->pulse.hz * 6.28) + 1.0) / 2.0;

		if(s->flags & STAGE_STRETCH){
			s->stretch.dhz = (sin(r_view.time * s->stretch.hz * 6.28) + 1.0) / 2.0;
			s->stretch.damp = 1.5 - s->stretch.dhz * s->stretch.amp;
		}

		if(s->flags & STAGE_ROTATE)
			s->rotate.deg = r_view.time * s->rotate.hz * 360.0;

		if(s->flags & STAGE_SCROLL_S)
			s->scroll.ds = s->scroll.s * r_view.time;

		if(s->flags & STAGE_SCROLL_T)
			s->scroll.dt = s->scroll.t * r_view.time;

		if(s->flags & STAGE_ANIM){
			if(r_view.time >= s->anim.dtime){  // change frames
				s->anim.dtime = r_view.time + (1.0 / s->anim.fps);
				s->image = s->anim.images[++s->anim.dframe % s->anim.num_frames];
			}
		}
	}
}


/*
 * R_StageTextureMatrix
 */
static inline void R_StageTextureMatrix(msurface_t *surf, stage_t *stage){
	static qboolean identity = true;
	float s, t;

	if(!(stage->flags & STAGE_TEXTURE_MATRIX)){

		if(!identity)
			glLoadIdentity();

		identity = true;
		return;
	}

	glLoadIdentity();

	s = surf->stcenter[0] / surf->texinfo->image->width;
	t = surf->stcenter[1] / surf->texinfo->image->height;

	if(stage->flags & STAGE_STRETCH){
		glTranslatef(-s, -t, 0.0);
		glScalef(stage->stretch.damp, stage->stretch.damp, 1.0);
		glTranslatef(-s, -t, 0.0);
	}

	if(stage->flags & STAGE_ROTATE){
		glTranslatef(-s, -t, 0.0);
		glRotatef(stage->rotate.deg, 0.0, 0.0, 1.0);
		glTranslatef(-s, -t, 0.0);
	}

	if(stage->flags & STAGE_SCALE_S)
		glScalef(stage->scale.s, 1.0, 1.0);

	if(stage->flags & STAGE_SCALE_T)
		glScalef(1.0, stage->scale.t, 1.0);

	if(stage->flags & STAGE_SCROLL_S)
		glTranslatef(stage->scroll.ds, 0.0, 0.0);

	if(stage->flags & STAGE_SCROLL_T)
		glTranslatef(0.0, stage->scroll.dt, 0.0);

	identity = false;
}


/*
 * R_StageTexCoord
 */
static inline void R_StageTexCoord(stage_t *stage, vec3_t v, vec2_t in, vec2_t out){
	vec3_t tmp;

	if(stage->flags & STAGE_ENVMAP){  // generate texcoords

		VectorSubtract(v, r_view.origin, tmp);
		VectorNormalize(tmp);

		out[0] = tmp[0];
		out[1] = tmp[1];
	}
	else {  // or use the ones we were given
		out[0] = in[0];
		out[1] = in[1];
	}
}


/*
 * R_StageVertex
 */
static inline void R_StageVertex(msurface_t *surf, stage_t *stage, vec3_t in, vec3_t out){

	// TODO: vertex deforms
	VectorCopy(in, out);
}


/*
 * R_StageColor
 */
static inline void R_StageColor(stage_t *stage, vec3_t v, vec4_t color){
	float a;

	if(stage->flags & STAGE_TERRAIN){

		if(stage->flags & STAGE_COLOR)  // honor stage color
			VectorCopy(stage->color, color);
		else  // or use white
			VectorSet(color, 1.0, 1.0, 1.0);

		// resolve alpha for vert based on z axis height
		if(v[2] < stage->terrain.floor)
			a = 0.0;
		else if(v[2] > stage->terrain.ceil)
			a = 1.0;
		else
			a = (v[2] - stage->terrain.floor) / stage->terrain.height;

		color[3] = a;
	}
	else {  // simply use white
		color[0] = color[1] = color[2] = color[3] = 1.0;
	}
}


/*
 * R_SetSurfaceStageState
 */
static void R_SetSurfaceStageState(msurface_t *surf, stage_t *stage){
	vec4_t color;

	// bind the texture
	R_BindTexture(stage->image->texnum);

	// and optionally the lightmap
	if(r_state.rendermode == rendermode_default){
		if(stage->flags & STAGE_LIGHTMAP){
			R_EnableTexture(&texunit_lightmap, true);
			R_BindLightmapTexture(surf->lightmap_texnum);
		}
		else
			R_EnableTexture(&texunit_lightmap, false);
	}

	// load the texture matrix for rotations, stretches, etc..
	R_StageTextureMatrix(surf, stage);

	// set the blend function, ensuring a good default
	if(stage->flags & STAGE_BLEND)
		R_BlendFunc(stage->blend.src, stage->blend.dest);
	else
		R_BlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	// for terrain, enable the color array
	if(stage->flags & STAGE_TERRAIN)
		R_EnableColorArray(true);
	else
		R_EnableColorArray(false);

	// when not using the color array, resolve the shade color
	if(!r_state.color_array_enabled){

		if(stage->flags & STAGE_COLOR)  // explicit
			VectorCopy(stage->color, color);

		else if(stage->flags & STAGE_ENVMAP)  // implied
			VectorCopy(surf->color, color);

		else  // default
			VectorSet(color, 1.0, 1.0, 1.0);

		// modulate the alpha value for pulses
		if(stage->flags & STAGE_PULSE){
			R_EnableFog(false);  // disable fog, since it also sets alpha
			color[3] = stage->pulse.dhz;
		}
		else {
			R_EnableFog(true);  // ensure fog is available
			color[3] = 1.0;
		}

		glColor4fv(color);
	}
}


/*
 * R_DrawSurfaceStage
 */
static void R_DrawSurfaceStage(msurface_t *surf, stage_t *stage){
	float *v, *st;
	int i;

	for(i = 0; i < surf->numedges; i++){

		v = &r_worldmodel->verts[surf->index * 3 + i * 3];
		st = &r_worldmodel->texcoords[surf->index * 2 + i * 2];

		R_StageVertex(surf, stage, v, &r_state.vertex_array_3d[i * 3]);

		R_StageTexCoord(stage, v, st, &texunit_diffuse.texcoord_array[i * 2]);

		if(r_state.color_array_enabled)
			R_StageColor(stage, v, &r_state.color_array[i * 4]);

		if(texunit_lightmap.enabled){
			st = &r_worldmodel->lmtexcoords[surf->index * 2 + i * 2];
			texunit_lightmap.texcoord_array[i * 2 + 0] = st[0];
			texunit_lightmap.texcoord_array[i * 2 + 1] = st[1];
		}
	}

	glDrawArrays(GL_POLYGON, 0, i);
}


/*
 * R_DrawMaterialSurfaces
 */
void R_DrawMaterialSurfaces(msurfaces_t *surfs){
	msurface_t *surf;
	material_t *m;
	stage_t *s;
	int i, j;

	if(!r_materials->value || r_showpolys->value)
		return;

	if(!surfs->count)
		return;

	R_ResetArrayState();

	glEnable(GL_POLYGON_OFFSET_FILL);  // all stages use depth offset

	glMatrixMode(GL_TEXTURE);  // some stages will manipulate texcoords

	for(i = 0; i < surfs->count; i++){

		surf = surfs->surfaces[i];

		if(surf->frame != r_locals.frame)
			continue;

		m = &surf->texinfo->image->material;

		R_UpdateMaterial(m);

		j = -1;
		for(s = m->stages; s; s = s->next, j--){

			if(!(s->flags & STAGE_RENDER))
				continue;

			glPolygonOffset(j, 1.0);  // increase depth offset for each stage

			R_SetSurfaceStageState(surf, s);

			R_DrawSurfaceStage(surf, s);
		}
	}

	glPolygonOffset(0.0, 0.0);
	glDisable(GL_POLYGON_OFFSET_FILL);

	glLoadIdentity();
	glMatrixMode(GL_MODELVIEW);

	R_BlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	R_EnableTexture(&texunit_lightmap, false);

	R_EnableFog(true);

	R_EnableColorArray(false);

	glColor4ubv(color_white);
}


/*
 * R_ClearMaterials
 */
static void R_ClearMaterials(void){
	material_t *m;
	stage_t *s, *ss;
	int i;

	// clear previously loaded materials
	for(i = 0; i < MAX_GL_TEXTURES; i++){

		m = &r_images[i].material;
		s = m->stages;

		while(s){  // free the stages chain
			ss = s->next;
			Z_Free(s);
			s = ss;
		}

		memset(m, 0, sizeof(material_t));

		m->bump = DEFAULT_BUMP;
		m->parallax = DEFAULT_PARALLAX;
		m->hardness = DEFAULT_HARDNESS;
		m->specular = DEFAULT_SPECULAR;
	}
}


/*
 * R_ConstByName
 */
static inline GLenum R_ConstByName(const char *c){

	if(!strcmp(c, "GL_ONE"))
		return GL_ONE;
	if(!strcmp(c, "GL_ZERO"))
		return GL_ZERO;
	if(!strcmp(c, "GL_SRC_ALPHA"))
		return GL_SRC_ALPHA;
	if(!strcmp(c, "GL_ONE_MINUS_SRC_ALPHA"))
		return GL_ONE_MINUS_SRC_ALPHA;

	// ...

	Com_Warn("R_ConstByName: Failed to resolve: %s\n", c);
	return GL_ZERO;
}


/*
 * R_LoadAnimImages
 */
static int R_LoadAnimImages(stage_t *s){
	char *c, name[MAX_QPATH];
	int i, j, k;

	if(!s->image){
		Com_Warn("R_LoadAnimImages: Texture not defined in anim stage.\n");
		return -1;
	}

	strncpy(name, s->image->name, sizeof(name));
	j = strlen(name);

	if((i = atoi(&name[j - 1])) < 0){
		Com_Warn("R_LoadAnimImages: Texture name does not end in numeric: %s\n", name);
		return -1;
	}

	// the first image was already loaded by the stage parse, so just copy
	// the pointer into the images array

	s->anim.images[0] = s->image;
	name[j - 1] = 0;

	// now load the rest
	for(k = 1, i = i + 1; k < s->anim.num_frames; k++, i++){

		c = va("%s%d", name, i);
		s->anim.images[k] = R_LoadImage(c, it_material);

		if(s->anim.images[k] == r_notexture){
			Com_Warn("R_LoadAnimImages: Failed to resolve texture: %s\n", c);
			return -1;
		}
	}

	return 0;
}


/*
 * R_ParseStage
 */
static int R_ParseStage(stage_t *s, const char **buffer){
	const char *c;
	int i;

	while(true){

		c = Com_Parse(buffer);

		if(!strlen(c))
			break;

		if(!strcmp(c, "texture")){

			c = Com_Parse(buffer);
			s->image = R_LoadImage(va("textures/%s", c), it_material);

			if(s->image == r_notexture){
				Com_Warn("R_ParseStage: Failed to resolve texture: %s\n", c);
				return -1;
			}

			s->flags |= STAGE_TEXTURE;
			continue;
		}

		if(!strcmp(c, "envmap")){

			c = Com_Parse(buffer);
			i = atoi(c);

			if(i > -1 && i < NUM_ENVMAPTEXTURES)
				s->image = r_envmaptextures[i];
			else
				s->image = R_LoadImage(va("envmaps/%s", c), it_material);

			if(s->image == r_notexture){
				Com_Warn("R_ParseStage: Failed to resolve envmap: %s\n", c);
				return -1;
			}

			s->flags |= STAGE_ENVMAP;
			continue;
		}

		if(!strcmp(c, "blend")){

			c = Com_Parse(buffer);
			s->blend.src = R_ConstByName(c);

			if(s->blend.src == -1){
				Com_Warn("R_ParseStage: Failed to resolve blend src: %s\n", c);
				return -1;
			}

			c = Com_Parse(buffer);
			s->blend.dest = R_ConstByName(c);

			if(s->blend.dest == -1){
				Com_Warn("R_ParseStage: Failed to resolve blend dest: %s\n", c);
				return -1;
			}

			s->flags |= STAGE_BLEND;
			continue;
		}

		if(!strcmp(c, "color")){

			for(i = 0; i < 3; i++){

				c = Com_Parse(buffer);
				s->color[i] = atof(c);

				if(s->color[i] < 0.0 || s->color[i] > 1.0){
					Com_Warn("R_ParseStage: Failed to resolve color: %s\n", c);
					return -1;
				}
			}

			s->flags |= STAGE_COLOR;
			continue;
		}

		if(!strcmp(c, "pulse")){

			c = Com_Parse(buffer);
			s->pulse.hz = atof(c);

			if(s->pulse.hz < 0.0){
				Com_Warn("R_ParseStage: Failed to resolve frequency: %s\n", c);
				return -1;
			}

			s->flags |= STAGE_PULSE;
			continue;
		}

		if(!strcmp(c, "stretch")){

			c = Com_Parse(buffer);
			s->stretch.amp = atof(c);

			if(s->stretch.amp < 0.0){
				Com_Warn("R_ParseStage: Failed to resolve amplitude: %s\n", c);
				return -1;
			}

			c = Com_Parse(buffer);
			s->stretch.hz = atof(c);

			if(s->stretch.hz < 0.0){
				Com_Warn("R_ParseStage: Failed to resolve frequency: %s\n", c);
				return -1;
			}

			s->flags |= STAGE_STRETCH;
			continue;
		}

		if(!strcmp(c, "rotate")){

			c = Com_Parse(buffer);
			s->rotate.hz = atof(c);

			if(s->rotate.hz < 0.0){
				Com_Warn("R_ParseStage: Failed to resolve rotate: %s\n", c);
				return -1;
			}

			s->flags |= STAGE_ROTATE;
			continue;
		}

		if(!strcmp(c, "scroll.s")){

			c = Com_Parse(buffer);
			s->scroll.s = atof(c);

			s->flags |= STAGE_SCROLL_S;
			continue;
		}

		if(!strcmp(c, "scroll.t")){

			c = Com_Parse(buffer);
			s->scroll.t = atof(c);

			s->flags |= STAGE_SCROLL_T;
			continue;
		}

		if(!strcmp(c, "scale.s")){

			c = Com_Parse(buffer);
			s->scale.s = atof(c);

			s->flags |= STAGE_SCALE_S;
			continue;
		}

		if(!strcmp(c, "scale.t")){

			c = Com_Parse(buffer);
			s->scale.t = atof(c);

			s->flags |= STAGE_SCALE_T;
			continue;
		}

		if(!strcmp(c, "terrain")){

			c = Com_Parse(buffer);
			s->terrain.floor = atof(c);

			c = Com_Parse(buffer);
			s->terrain.ceil = atof(c);

			if(s->terrain.ceil < s->terrain.floor){
				Com_Warn("R_ParseStage: Inverted ceiling / floor values for %s\n",
						(s->image ? s->image->name : "NULL"));
				return -1;
			}

			s->terrain.height = s->terrain.ceil - s->terrain.floor;

			if(s->terrain.height == 0.0){
				Com_Warn("R_ParseStage: Zero height terrain specified for %s\n",
						(s->image ? s->image->name : "NULL"));
				return -1;
			}

			s->flags |= STAGE_TERRAIN;
			continue;
		}

		if(!strcmp(c, "anim")){

			c = Com_Parse(buffer);
			s->anim.num_frames = atoi(c);

			if(s->anim.num_frames < 1 || s->anim.num_frames > MAX_ANIM_FRAMES){
				Com_Warn("R_ParseStage: Invalid number of anim frames for %s\n",
						(s->image ? s->image->name : "NULL"));
				return -1;
			}

			c = Com_Parse(buffer);
			s->anim.fps = atof(c);

			if(s->anim.fps <= 0.0){
				Com_Warn("R_ParseStage: Invalid anim fps for %s\n",
						(s->image ? s->image->name : "NULL"));
				return -1;
			}

			// the frame images are loaded once the stage is parsed completely

			s->flags |= STAGE_ANIM;
			continue;
		}

		if(!strcmp(c, "lightmap")){
			s->flags |= STAGE_LIGHTMAP;
			continue;
		}

		if(!strcmp(c, "flare")){

			c = Com_Parse(buffer);
			i = atoi(c);

			if(i > -1 && i < NUM_FLARETEXTURES)
				s->image = r_flaretextures[i];
			else
				s->image = R_LoadImage(va("flares/%s", c), it_material);

			if(s->image == r_notexture){
				Com_Warn("R_ParseStage: Failed to resolve flare: %s\n", c);
				return -1;
			}

			s->flags |= STAGE_FLARE;
			continue;
		}

		if(*c == '}'){

			Com_Dprintf("Parsed stage\n"
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
					"  anim.fps: %3f\n",
					s->flags, (s->image ? s->image->name : "NULL"),
					s->blend.src, s->blend.dest,
					s->color[0], s->color[1], s->color[2],
					s->pulse.hz, s->stretch.amp, s->stretch.hz,
					s->rotate.hz, s->scroll.s, s->scroll.t,
					s->scale.s, s->scale.t, s->terrain.floor, s->terrain.ceil,
					s->anim.num_frames, s->anim.fps);

			// a texture, or envmap means render it
			if(s->flags & (STAGE_TEXTURE | STAGE_ENVMAP))
				s->flags |= STAGE_RENDER;

			return 0;
		}
	}

	Com_Warn("R_ParseStage: Malformed stage\n");
	return -1;
}


/*
 * R_LoadMaterials
 */
void R_LoadMaterials(const char *map){
	char path[MAX_QPATH];
	void *buf;
	const char *c;
	const char *buffer;
	qboolean inmaterial;
	image_t *image;
	material_t *m;
	stage_t *s, *ss;
	int i;

	// clear previously loaded materials
	R_ClearMaterials();

	// load the materials file for parsing
	snprintf(path, sizeof(path), "materials/%s", Com_Basename(map));
	strcpy(path + strlen(path) - 3, "mat");

	if((i = Fs_LoadFile(path, &buf)) < 1){
		Com_Dprintf("Couldn't load %s\n", path);
		return;
	}

	buffer = (char *)buf;

	inmaterial = false;
	image = NULL;
	m = NULL;

	while(true){

		c = Com_Parse(&buffer);

		if(!strlen(c))
			break;

		if(*c == '{' && !inmaterial){
			inmaterial = true;
			continue;
		}

		if(!strcmp(c, "material")){
			c = Com_Parse(&buffer);
			image = R_LoadImage(va("textures/%s", c), it_world);

			if(image == r_notexture){
				Com_Warn("R_LoadMaterials: Failed to resolve texture: %s\n", c);
				image = NULL;
			}

			continue;
		}

		if(!image)
			continue;

		m = &image->material;

		if(!strcmp(c, "bump")){

			m->bump = atof(Com_Parse(&buffer));

			if(m->bump < 0.0){
				Com_Warn("R_LoadMaterials: Invalid bump value for %s\n",
						image->name);
				m->bump = DEFAULT_BUMP;
			}
		}

		if(!strcmp(c, "parallax")){

			m->parallax = atof(Com_Parse(&buffer));

			if(m->parallax < 0.0){
				Com_Warn("R_LoadMaterials: Invalid parallax value for %s\n",
						image->name);
				m->parallax = DEFAULT_PARALLAX;
			}
		}

		if(!strcmp(c, "hardness")){

			m->hardness = atof(Com_Parse(&buffer));

			if(m->hardness < 0.0){
				Com_Warn("R_LoadMaterials: Invalid hardness value for %s\n",
						image->name);
				m->hardness = DEFAULT_HARDNESS;
			}
		}

		if(!strcmp(c, "specular")){
			m->specular = atof(Com_Parse(&buffer));

			if(m->specular < 0.0){
				Com_Warn("R_LoadMaterials: Invalid specular value for %s\n",
						image->name);
				m->specular = DEFAULT_SPECULAR;
			}
		}

		if(*c == '{' && inmaterial){

			s = (stage_t *)Z_Malloc(sizeof(*s));

			if(R_ParseStage(s, &buffer) == -1){
				Z_Free(s);
				continue;
			}

			// load animation frame images
			if(s->flags & STAGE_ANIM){
				if(R_LoadAnimImages(s) == -1){
					Z_Free(s);
					continue;
				}
			}

			// append the stage to the chain
			if(!m->stages)
				m->stages = s;
			else {
				ss = m->stages;
				while(ss->next)
					ss = ss->next;
				ss->next = s;
			}

			m->flags |= s->flags;
			m->num_stages++;
			continue;
		}

		if(*c == '}' && inmaterial){
			Com_Dprintf("Parsed material %s with %d stages\n",
					image->name, m->num_stages);
			inmaterial = false;
			image = NULL;
		}
	}

	Fs_FreeFile(buf);
}
