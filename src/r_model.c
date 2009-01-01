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

model_t r_models[MAX_MOD_KNOWN];
int r_nummodels;

model_t r_inlinemodels[MAX_MOD_KNOWN];

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
void R_AllocVertexArrays(model_t *mod){
	msurface_t *surf;
	dmd2_t *md2;
	mmd3_t *md3;
	mmd3mesh_t *mesh;
	int i, j;
	int v, st, t, c;

	mod->vertexcount = 0;

	// first resolve the vertex count
	if(mod->type == mod_bsp){

		for(i = 0, surf = mod->surfaces; i < mod->numsurfaces; i++, surf++){
			for(j = 0; j < surf->numedges; j++)
				mod->vertexcount++;
		}
	}
	else if(mod->type == mod_md2){

		if(mod->num_frames == 1){
			md2 = (dmd2_t *)mod->extradata;
			mod->vertexcount = md2->num_tris * 3;
		}
	}
	else if(mod->type == mod_md3){

		if(mod->num_frames == 1){
			md3 = (mmd3_t *)mod->extradata;

			for(i = 0, mesh = md3->meshes; i < md3->num_meshes; i++, mesh++)
				mod->vertexcount += mesh->num_tris * 3;
		}
	}

	if(!mod->vertexcount)  // animated models don't use VA
		return;

	v = mod->vertexcount * 3 * sizeof(GLfloat);
	st = mod->vertexcount * 2 * sizeof(GLfloat);
	t = mod->vertexcount * 4 * sizeof(GLfloat);
	c = mod->vertexcount * 4 * sizeof(GLfloat);

	// allocate the arrays
	mod->verts = (GLfloat *)R_HunkAlloc(v);
	mod->texcoords = (GLfloat *)R_HunkAlloc(st);
	mod->normals = (GLfloat *)R_HunkAlloc(v);

	if(mod->type != mod_bsp)
		return;

	mod->lmtexcoords = (GLfloat *)R_HunkAlloc(st);
	mod->tangents = (GLfloat *)R_HunkAlloc(t);
	mod->colors = (GLfloat *)R_HunkAlloc(c);
}


/*
 * R_LoadVertexBuffers
 */
static void R_LoadVertexBuffers(model_t *mod){
	int v, st, t, c;

	if(!mod->vertexcount)  // animated models don't use VBO
		return;

	v = mod->vertexcount * 3 * sizeof(GLfloat);
	st = mod->vertexcount * 2 * sizeof(GLfloat);
	t = mod->vertexcount * 4 * sizeof(GLfloat);
	c = mod->vertexcount * 4 * sizeof(GLfloat);

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


/*
 * R_LoadModel
 */
model_t *R_LoadModel(const char *name){
	model_t *mod;
	void *buf;
	int i;

	if(!name || !name[0]){
		Com_Error(ERR_DROP, "R_LoadModel: NULL name.");
	}

	// inline models are fetched from a separate array
	if(name[0] == '*'){
		i = atoi(name + 1);
		if(i < 1 || !r_worldmodel || i >= r_worldmodel->numsubmodels){
			Com_Error(ERR_DROP, "R_LoadModel: Bad inline model number.");
		}
		return &r_inlinemodels[i];
	}

	// search the currently loaded models
	for(i = 0 , mod = r_models; i < r_nummodels; i++, mod++){
		if(!mod->name[0])
			continue;
		if(!strcmp(mod->name, name))
			return mod;
	}

	// find a free model slot spot
	for(i = 0 , mod = r_models; i < r_nummodels; i++, mod++){
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

	if((Fs_LoadFile(name, &buf)) == -1){
		if(!r_worldmodel)  // failed to load the world (.bsp)
			Com_Error(ERR_DROP, "R_LoadModel: %s not found.", name);
		return NULL;
	}

	strcpy(mod->name, name);

	r_loadmodel = mod;

	// fill it in
	r_loadmodel->extradata = R_HunkBegin();

	// call the apropriate loader
	switch(LittleLong(*(unsigned *)buf)){
		case MD2_HEADER:
			R_LoadMd2Model(mod, buf);
			break;

		case MD3_HEADER:
			R_LoadMd3Model(mod, buf);
			break;

		case BSP_HEADER:
			R_LoadBspModel(mod, buf);
			break;

		default:
			Fs_FreeFile(buf);
			Com_Error(ERR_DROP, "R_LoadModel: Unknown type for %s.", mod->name);
	}

	r_loadmodel->extradatasize = R_HunkEnd(r_loadmodel->extradata);

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
	model_t *mod;
	int total;

	total = 0;
	Com_Printf("Loaded models:\n");
	for(i = 0, mod = r_models; i < r_nummodels; i++, mod++){
		if(!mod->name[0])
			continue;
		Com_Printf("%6i: %s\n", mod->vertexcount, mod->name);
		total += mod->extradatasize;
	}
	Com_Printf("Total resident: %i\n", total);
}


/*
 * R_HunkStats_f
 */
void R_HunkStats_f(void){
	Com_Printf("Hunk usage: %.2f / %.2f MB\n",
			hunkofs / 1024.0 / 1024.0, r_hunkmegs->value);
}


/*
 * R_FreeModels
 */
static void R_FreeModels(void){
	int i;

	for(i = 0; i < r_nummodels; i++){
		model_t *mod = &r_models[i];

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
	memset(r_inlinemodels, 0, sizeof(r_inlinemodels));

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

	// finally load it for rendering
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
