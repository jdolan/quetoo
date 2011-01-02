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

#include "renderer.h"

r_model_t r_models[MAX_MOD_KNOWN];
int r_nummodels;

r_model_t r_inline_models[MAX_MOD_KNOWN];

static byte *hunk = NULL;
static size_t hunkbytes;
static ptrdiff_t hunkofs;


/*
 * R_HunkBegin
 */
static void *R_HunkBegin(){
	return hunk + hunkofs;
}


/*
 * R_HunkEnd
 */
static int R_HunkEnd(void *buf){
	return hunk + hunkofs - (byte *)buf;
}


/*
 * R_HunkAlloc
 */
void *R_HunkAlloc(size_t size){
	byte *b;

	if(hunkofs + size > hunkbytes){
		Com_Error(ERR_FATAL, "R_HunkAlloc: Overflow.");
	}

	b = hunk + hunkofs;
	hunkofs += size;
	return b;
}


/*
 * R_AllocVertexArrays
 */
void R_AllocVertexArrays(r_model_t *mod){
	int i, j;
	int v, st, t, c;

	mod->num_verts = 0;

	// first resolve the vertex count
	if(mod->type == mod_bsp){
		r_bsp_surface_t *surf;

		for(i = 0, surf = mod->surfaces; i < mod->num_surfaces; i++, surf++){
			for(j = 0; j < surf->num_edges; j++)
				mod->num_verts++;
		}
	}
	else if(mod->type == mod_md2){

		d_md2_t *md2 = (d_md2_t *)mod->extra_data;
		mod->num_verts = md2->num_tris * 3;
	}
	else if(mod->type == mod_md3){

		r_md3_t *md3 = (r_md3_t *)mod->extra_data;
		r_md3_mesh_t *mesh = md3->meshes;

		for(i = 0; i < md3->num_meshes; i++, mesh++)
			mod->num_verts += mesh->num_tris * 3;
	}
	else if(mod->type == mod_obj){

		r_obj_t *obj = (r_obj_t *)mod->extra_data;
		mod->num_verts = obj->num_tris * 3;
	}

	v = mod->num_verts * 3 * sizeof(GLfloat);
	st = mod->num_verts * 2 * sizeof(GLfloat);
	t = mod->num_verts * 4 * sizeof(GLfloat);
	c = mod->num_verts * 4 * sizeof(GLfloat);

	// allocate the arrays, static models get verts and normals
	if(mod->num_frames < 2){
		mod->verts = (GLfloat *)R_HunkAlloc(v);
		mod->normals = (GLfloat *)R_HunkAlloc(v);
	}

	// all models get texcoords
	mod->texcoords = (GLfloat *)R_HunkAlloc(st);

	if(mod->type != mod_bsp)
		return;

	// and bsp models get lightmap texcoords, tangents, and colors
	mod->lmtexcoords = (GLfloat *)R_HunkAlloc(st);
	mod->tangents = (GLfloat *)R_HunkAlloc(t);
	mod->colors = (GLfloat *)R_HunkAlloc(c);
}


/*
 * R_LoadVertexBuffers
 */
static void R_LoadVertexBuffers(r_model_t *mod){
	int v, st, t, c;

	if(!qglGenBuffers)
		return;

	if(mod->num_frames > 1)  // animated models don't use VBO
		return;

	v = mod->num_verts * 3 * sizeof(GLfloat);
	st = mod->num_verts * 2 * sizeof(GLfloat);
	t = mod->num_verts * 4 * sizeof(GLfloat);
	c = mod->num_verts * 4 * sizeof(GLfloat);

	// load the vertex buffer objects
	qglGenBuffers(1, &mod->vertex_buffer);
	qglBindBuffer(GL_ARRAY_BUFFER, mod->vertex_buffer);
	qglBufferData(GL_ARRAY_BUFFER, v, mod->verts, GL_STATIC_DRAW);

	qglGenBuffers(1, &mod->texcoord_buffer);
	qglBindBuffer(GL_ARRAY_BUFFER, mod->texcoord_buffer);
	qglBufferData(GL_ARRAY_BUFFER, st, mod->texcoords, GL_STATIC_DRAW);

	qglGenBuffers(1, &mod->normal_buffer);
	qglBindBuffer(GL_ARRAY_BUFFER, mod->normal_buffer);
	qglBufferData(GL_ARRAY_BUFFER, v, mod->normals, GL_STATIC_DRAW);

	qglBindBuffer(GL_ARRAY_BUFFER, 0);

	if(mod->type != mod_bsp)
		return;

	// including lightmap coords, tangents, and colors for bsp
	qglGenBuffers(1, &mod->lmtexcoord_buffer);
	qglBindBuffer(GL_ARRAY_BUFFER, mod->lmtexcoord_buffer);
	qglBufferData(GL_ARRAY_BUFFER, st, mod->lmtexcoords, GL_STATIC_DRAW);

	qglGenBuffers(1, &mod->tangent_buffer);
	qglBindBuffer(GL_ARRAY_BUFFER, mod->tangent_buffer);
	qglBufferData(GL_ARRAY_BUFFER, t, mod->tangents, GL_STATIC_DRAW);

	qglGenBuffers(1, &mod->color_buffer);
	qglBindBuffer(GL_ARRAY_BUFFER, mod->color_buffer);
	qglBufferData(GL_ARRAY_BUFFER, c, mod->colors, GL_STATIC_DRAW);

	qglBindBuffer(GL_ARRAY_BUFFER, 0);
}


typedef struct r_model_format_s {
	const char *name;
	void (*load)(r_model_t *mod, void *buffer);
} r_model_format_t;

static r_model_format_t r_model_formats[] = {
		{".obj", R_LoadObjModel},
		{".md3", R_LoadMd3Model},
		{".md2", R_LoadMd2Model},
		{".bsp", R_LoadBspModel}
};

#define NUM_MODEL_FORMATS (sizeof(r_model_formats) / sizeof(r_model_format_t))

/*
 * R_LoadModel
 */
r_model_t *R_LoadModel(const char *name){
	r_model_format_t *format;
	r_model_t *mod;
	char n[MAX_QPATH];
	void *buf;
	int i;

	if(!name || !name[0]){
		Com_Error(ERR_DROP, "R_LoadModel: NULL name.");
	}

	// inline models are fetched from a separate array
	if(name[0] == '*'){
		i = atoi(name + 1);
		if(i < 1 || !r_worldmodel || i >= r_worldmodel->num_submodels){
			Com_Error(ERR_DROP, "R_LoadModel: Bad inline model number.");
		}
		return &r_inline_models[i];
	}

	Com_StripExtension(name, n);

	// search the currently loaded models
	for(i = 0 , mod = r_models; i < r_nummodels; i++, mod++){
		if(!strcmp(n, mod->name))
			return mod;
	}

	// find a free model slot spot
	for(i = 0, mod = r_models; i < r_nummodels; i++, mod++){
		if(!mod->name[0])
			break;  // free spot
	}

	if(i == r_nummodels){
		if(r_nummodels == MAX_MOD_KNOWN){
			Com_Error(ERR_DROP, "R_LoadModel: MAX_MOD_KNOWN reached.");
		}
		r_nummodels++;
	}

	// load the file
	format = r_model_formats;
	for(i = 0; i < NUM_MODEL_FORMATS; i++, format++){

		Com_StripExtension(name, n);
		strcat(n, format->name);

		if(Fs_LoadFile(n, &buf) != -1)
			break;
	}

	if(i == NUM_MODEL_FORMATS) {  // not found
		Com_Warn("R_LoadModel: Failed to load %s.\n", name);
		return NULL;
	}

	Com_StripExtension(n, mod->name);

	r_loadmodel = mod;
	r_loadmodel->extra_data = R_HunkBegin();

	format->load(mod, buf);

	r_loadmodel->extra_data_size = R_HunkEnd(r_loadmodel->extra_data);

	// assemble vertex buffer objects from static arrays
	R_LoadVertexBuffers(mod);

	Fs_FreeFile(buf);

	return mod;
}


/*
 * R_ListModels_f
 */
void R_ListModels_f(void){
	int i;
	r_model_t *mod;
	int total;

	total = 0;
	Com_Print("Loaded models:\n");
	for(i = 0, mod = r_models; i < r_nummodels; i++, mod++){
		if(!mod->name[0])
			continue;
		Com_Print("%6i: %s\n", mod->num_verts, mod->name);
		total += mod->extra_data_size;
	}
	Com_Print("Total resident: %i\n", total);
}


/*
 * R_HunkStats_f
 */
void R_HunkStats_f(void){
	Com_Print("Hunk usage: %.2f / %.2f MB\n",
			hunkofs / 1024.0 / 1024.0, r_hunkmegs->value);
}


/*
 * R_FreeModels
 */
static void R_FreeModels(void){
	int i;

	for(i = 0; i < r_nummodels; i++){
		r_model_t *mod = &r_models[i];

		if(mod->vertex_buffer)
			qglDeleteBuffers(1, &mod->vertex_buffer);
		if(mod->texcoord_buffer)
			qglDeleteBuffers(1, &mod->texcoord_buffer);
		if(mod->lmtexcoord_buffer)
			qglDeleteBuffers(1, &mod->lmtexcoord_buffer);
		if(mod->normal_buffer)
			qglDeleteBuffers(1, &mod->normal_buffer);
	}

	r_worldmodel = NULL;

	// reset the models array
	memset(r_models, 0, sizeof(r_models));
	r_nummodels = 0;

	// reset inline models
	memset(r_inline_models, 0, sizeof(r_inline_models));

	// reset the hunk
	memset(hunk, 0, hunkbytes);
	hunkofs = 0;
}


/*
 * R_BeginLoading
 *
 * Loads the specified map after resetting all model data.
 */
void R_BeginLoading(const char *map, int mapsize){
	int ms;

	R_FreeModels();  // free all models

	// load map for collision detection (prediction)
	Cm_LoadMap(map, &ms);

	if(ms != mapsize){
		Com_Error(ERR_DROP, "Local map version differs from server: "
				"%i != %i.", ms, mapsize);
	}

	// then load materials
	R_LoadMaterials(map);

	r_worldmodel = R_LoadModel(map);
}


/*
 * R_InitModels
 */
void R_InitModels(void){

	// allocate the hunk
	hunkbytes = (int)r_hunkmegs->value * 1024 * 1024;

	if(!(hunk = Z_Malloc(hunkbytes))){  // malloc the new one
		Com_Error(ERR_FATAL, "R_HunkInit: Unable to allocate hunk.");
	}

	hunkofs = 0;
}


/*
 * R_Shutdown
 */
void R_ShutdownModels(void){

	R_FreeModels();

	if(hunk)  // free the hunk
		Z_Free(hunk);
}
