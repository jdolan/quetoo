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

static const char *weaps[] = {
	"w_bfg", "w_chaingun", "w_glauncher", "w_hyperblaster", "w_machinegun",
	"w_railgun", "w_rlauncher", "w_shotgun", "w_sshotgun", NULL
};


/*
R_LoadMeshSkin

Resolves the skin for the specified model.  For most models, skin.tga is tried
within the model's directory.  Player-weapon models are a special case.
*/
static void R_LoadMeshSkin(model_t *mod){
	char skin[MAX_QPATH];
	qboolean weap;
	int i;

	weap = false;
	i = 0;
	while(weaps[i]){  // determine if this is a weapon model for skin checks
		if(strstr(mod->name, weaps[i])){
			weap = true;
			break;
		}
		i++;
	}

	// load the skin
	if(weap){  // using the model name for weaps
		Com_StripExtension(mod->name, skin);
		mod->skin = R_LoadImage(skin, it_skin);

		if(mod->skin == r_notexture){  // trying the common one if necessary
			snprintf(skin, sizeof(skin), "players/common/%s", weaps[i]);
			mod->skin = R_LoadImage(skin, it_skin);
		}
	}
	else {  // or simply skin for others
		Com_Dirname(mod->name, skin);
		strcat(skin, "skin");

		mod->skin = R_LoadImage(skin, it_skin);
	}
}


/*
R_LoadMeshConfig
*/
static void R_LoadMeshConfig(mesh_config_t *config, const char *path){
	const char *buffer, *c;
	void *buf;

	if(Fs_LoadFile(path, &buf) == -1)
		return;

	buffer = (char *)buf;

	while(true){

		c = Com_Parse(&buffer);

		if(!strlen(c))
			break;

		if(!strcmp(c, "translate")){
			sscanf(Com_Parse(&buffer), "%f %f %f", &config->translate[0],
					&config->translate[1], &config->translate[2]);
			continue;
		}

		if(!strcmp(c, "scale")){
			sscanf(Com_Parse(&buffer), "%f %f %f", &config->scale[0],
					&config->scale[1], &config->scale[2]);
			continue;
		}

		if(!strcmp(c, "alpha_test")){
			config->flags |= EF_ALPHATEST;
			continue;
		}

		if(!strcmp(c, "blend")){
			config->flags |= EF_BLEND;
			continue;
		}
	}

	Fs_FreeFile(buf);
}


/*
R_LoadMeshConfigs
*/
static void R_LoadMeshConfigs(model_t *mod){
	char path[MAX_QPATH];
	int i;

	mod->world_config = (mesh_config_t *)R_HunkAlloc(sizeof(mesh_config_t));
	mod->view_config = (mesh_config_t *)R_HunkAlloc(sizeof(mesh_config_t));

	VectorSet(mod->world_config->scale, 1.0, 1.0, 1.0);
	VectorSet(mod->view_config->scale, 1.0, 1.0, 1.0);

	i = 0;
	while(weaps[i]){  // determine if this is a weapon model
		if(strstr(mod->name, weaps[i]))  // for which configs are not supported
			return;
		i++;
	}

	Com_Dirname(mod->name, path);

	R_LoadMeshConfig(mod->world_config, va("%sworld.cfg", path));

	// by default, the view config inherits the world config
	memcpy(mod->view_config, mod->world_config, sizeof(mesh_config_t));

	R_LoadMeshConfig(mod->view_config, va("%sview.cfg", path));
}


/*
R_LoadMd2VertexArrays
*/
static void R_LoadMd2VertexArrays(model_t *mod){
	dmd2_t *md2;
	dmd2frame_t *frame;
	dmd2triangle_t *tri;
	dmd2vertex_t *v;
	dmd2texcoord_t *st;
	vec2_t coord;
	int i, j, vertind, coordind;

	R_AllocVertexArrays(mod);  // allocate the arrays

	if(!mod->vertexcount)
		return;

	md2 = (dmd2_t *)mod->extradata;

	frame = (dmd2frame_t *)((byte *)md2 + md2->ofs_frames);

	vertind = coordind = 0;

	for(i = 0, v = frame->verts; i < md2->num_xyz; i++, v++){  // reconstruct the verts
		VectorSet(r_mesh_verts[i],
				v->v[0] * frame->scale[0] + frame->translate[0],
				v->v[1] * frame->scale[1] + frame->translate[1],
				v->v[2] * frame->scale[2] + frame->translate[2]);

		VectorCopy(bytedirs[v->n], r_mesh_norms[i]);
	}

	tri = (dmd2triangle_t *)((byte *)md2 + md2->ofs_tris);
	st = (dmd2texcoord_t *)((byte *)md2 + md2->ofs_st);

	for(i = 0; i < md2->num_tris; i++, tri++){

		for(j = 0; j < 3; j++){
			coord[0] = ((float)st[tri->index_st[j]].s / md2->skinwidth);
			coord[1] = ((float)st[tri->index_st[j]].t / md2->skinheight);

			memcpy(&mod->texcoords[coordind + j * 2], coord, sizeof(vec2_t));
		}

		memcpy(&mod->verts[vertind + 0], r_mesh_verts[tri->index_xyz[0]], sizeof(vec3_t));
		memcpy(&mod->verts[vertind + 3], r_mesh_verts[tri->index_xyz[1]], sizeof(vec3_t));
		memcpy(&mod->verts[vertind + 6], r_mesh_verts[tri->index_xyz[2]], sizeof(vec3_t));

		memcpy(&mod->normals[vertind + 0], r_mesh_norms[tri->index_xyz[0]], sizeof(vec3_t));
		memcpy(&mod->normals[vertind + 3], r_mesh_norms[tri->index_xyz[1]], sizeof(vec3_t));
		memcpy(&mod->normals[vertind + 6], r_mesh_norms[tri->index_xyz[2]], sizeof(vec3_t));

		coordind += 6;
		vertind += 9;
	}
}


/*
R_LoadMd2Model
*/
void R_LoadMd2Model(model_t *mod, void *buffer){
	int i, j, version;
	dmd2_t *pinmodel, *poutmodel;
	dmd2texcoord_t *pincoord, *poutcoord;
	dmd2triangle_t *pintri, *pouttri;
	dmd2frame_t *pinframe, *poutframe;
	int *pincmd, *poutcmd;
	vec3_t mins, maxs;

	pinmodel = (dmd2_t *)buffer;

	version = LittleLong(pinmodel->version);
	if(version != MD2_VERSION){
		Com_Error(ERR_DROP, "R_LoadMd2Model: %s has wrong version number "
				"(%i should be %i).", mod->name, version, MD2_VERSION);
	}

	mod->type = mod_md2;
	mod->version = version;

	poutmodel = R_HunkAlloc(LittleLong(pinmodel->ofs_end));

	// byte swap the header fields and sanity check
	for(i = 0; i < sizeof(dmd2_t) / 4; i++)
		((int *)poutmodel)[i] = LittleLong(((int *)buffer)[i]);

	mod->num_frames = poutmodel->num_frames;

	if(poutmodel->num_xyz <= 0){
		Com_Error(ERR_DROP, "R_LoadMd2Model: %s has no vertices.", mod->name);
	}

	if(poutmodel->num_xyz > MD2_MAX_VERTS){
		Com_Error(ERR_DROP, "R_LoadMd2Model: %s has too many vertices.", mod->name);
	}

	if(poutmodel->num_st <= 0){
		Com_Error(ERR_DROP, "R_LoadMd2Model: %s has no st vertices.", mod->name);
	}

	if(poutmodel->num_tris <= 0){
		Com_Error(ERR_DROP, "R_LoadMd2Model: %s has no triangles.", mod->name);
	}

	if(poutmodel->num_frames <= 0){
		Com_Error(ERR_DROP, "R_LoadMd2Model: %s has no frames.", mod->name);
	}

	// load the texcoords
	pincoord = (dmd2texcoord_t *)((byte *)pinmodel + poutmodel->ofs_st);
	poutcoord = (dmd2texcoord_t *)((byte *)poutmodel + poutmodel->ofs_st);

	for(i = 0; i < poutmodel->num_st; i++){
		poutcoord[i].s = LittleShort(pincoord[i].s);
		poutcoord[i].t = LittleShort(pincoord[i].t);
	}

	// load triangle lists
	pintri = (dmd2triangle_t *)((byte *)pinmodel + poutmodel->ofs_tris);
	pouttri = (dmd2triangle_t *)((byte *)poutmodel + poutmodel->ofs_tris);

	for(i = 0; i < poutmodel->num_tris; i++){
		for(j = 0; j < 3; j++){
			pouttri[i].index_xyz[j] = LittleShort(pintri[i].index_xyz[j]);
			pouttri[i].index_st[j] = LittleShort(pintri[i].index_st[j]);
		}
	}

	// load the frames
	for(i = 0; i < poutmodel->num_frames; i++){
		pinframe = (dmd2frame_t *)((byte *)pinmodel
				+ poutmodel->ofs_frames + i * poutmodel->framesize);
		poutframe = (dmd2frame_t *)((byte *)poutmodel
				+ poutmodel->ofs_frames + i * poutmodel->framesize);

		memcpy(poutframe->name, pinframe->name, sizeof(poutframe->name));

		for(j = 0; j < 3; j++){
			poutframe->scale[j] = LittleFloat(pinframe->scale[j]);
			poutframe->translate[j] = LittleFloat(pinframe->translate[j]);
		}

		VectorCopy(poutframe->translate, mins);
		VectorMA(mins, 255.0, poutframe->scale, maxs);

		AddPointToBounds(mins, mod->mins, mod->maxs);
		AddPointToBounds(maxs, mod->mins, mod->maxs);

		// verts are all 8 bit, so no swapping needed
		memcpy(poutframe->verts, pinframe->verts, poutmodel->num_xyz * sizeof(dmd2vertex_t));
	}

	// load the glcmds
	pincmd = (int *)((byte *)pinmodel + poutmodel->ofs_glcmds);
	poutcmd = (int *)((byte *)poutmodel + poutmodel->ofs_glcmds);

	for(i = 0; i < poutmodel->num_glcmds; i++)
		poutcmd[i] = LittleLong(pincmd[i]);

	// load the skin
	R_LoadMeshSkin(mod);

	// and configs
	R_LoadMeshConfigs(mod);

	R_LoadMd2VertexArrays(mod);
}


/*
R_LoadMd3VertexArrays
*/
static void R_LoadMd3VertexArrays(model_t *mod){
	mmd3_t *md3;
	dmd3frame_t *frame;
	mmd3mesh_t *mesh;
	mmd3vertex_t *v;
	dmd3coord_t *texcoords;
	int i, j, k, vertind, coordind;
	unsigned *tri;

	R_AllocVertexArrays(mod);  // allocate the arrays

	if(!mod->vertexcount)
		return;

	md3 = (mmd3_t *)mod->extradata;

	frame = md3->frames;

	vertind = coordind = 0;

	for(k = 0, mesh = md3->meshes; k < md3->num_meshes; k++, mesh++){  // iterate the meshes

		v = mesh->verts;

		for(i = 0; i < mesh->num_verts; i++, v++){  // reconstruct the verts
			VectorAdd(frame->translate, v->point, r_mesh_verts[i]);
			VectorCopy(v->normal, r_mesh_norms[i]);
		}

		tri = mesh->tris;
		texcoords = mesh->coords;

		for(j = 0; j < mesh->num_tris; j++, tri += 3){  // populate the arrays
			memcpy(&mod->texcoords[coordind + 0], &texcoords[tri[0]], sizeof(vec2_t));
			memcpy(&mod->verts[vertind + 0], &r_mesh_verts[tri[0]], sizeof(vec3_t));

			memcpy(&mod->texcoords[coordind + 2], &texcoords[tri[1]], sizeof(vec2_t));
			memcpy(&mod->verts[vertind + 3], &r_mesh_verts[tri[1]], sizeof(vec3_t));

			memcpy(&mod->texcoords[coordind + 4], &texcoords[tri[2]], sizeof(vec2_t));
			memcpy(&mod->verts[vertind + 6], &r_mesh_verts[tri[2]], sizeof(vec3_t));

			memcpy(&mod->normals[vertind + 0], &r_mesh_norms[tri[0]], sizeof(vec3_t));
			memcpy(&mod->normals[vertind + 3], &r_mesh_norms[tri[1]], sizeof(vec3_t));
			memcpy(&mod->normals[vertind + 6], &r_mesh_norms[tri[2]], sizeof(vec3_t));

			coordind += 6;
			vertind += 9;
		}
	}
}


/*
R_LoadMd3Model
*/
void R_LoadMd3Model(model_t *mod, void *buffer){
	int version, i, j, l;
	dmd3_t *pinmodel;
	mmd3_t *poutmodel;
	dmd3frame_t *pinframe, *poutframe;
	dmd3tag_t *pintag, *pouttag;
	dmd3mesh_t *pinmesh;
	mmd3mesh_t *poutmesh;
	dmd3coord_t *pincoord, *poutcoord;
	dmd3vertex_t *pinvert;
	mmd3vertex_t *poutvert;
	unsigned int *pinindex, *poutindex;
	float lat, lng;

	pinmodel = (dmd3_t *)buffer;

	version = LittleLong(pinmodel->version);
	if(version != MD3_VERSION){
		Com_Error(ERR_DROP, "R_LoadMd3Model: %s has wrong version number "
				"(%i should be %i)", mod->name, version, MD3_VERSION);
	}

	mod->type = mod_md3;
	mod->version = version;

	poutmodel = R_HunkAlloc(sizeof(mmd3_t));

	// byte swap the header fields and sanity check
	pinmodel->ofs_frames = LittleLong(pinmodel->ofs_frames);
	pinmodel->ofs_tags = LittleLong(pinmodel->ofs_tags);
	pinmodel->ofs_meshes = LittleLong(pinmodel->ofs_meshes);

	mod->num_frames = poutmodel->num_frames = LittleLong(pinmodel->num_frames);
	poutmodel->num_tags = LittleLong(pinmodel->num_tags);
	poutmodel->num_meshes = LittleLong(pinmodel->num_meshes);

	if(poutmodel->num_frames < 1){
		Com_Error(ERR_DROP, "R_LoadMd3Model: %s has no frames.", mod->name);
	}

	if(poutmodel->num_tags > MD3_MAX_TAGS){
		Com_Error(ERR_DROP, "R_LoadMd3Model: %s has too many tags.", mod->name);
	}

	if(poutmodel->num_meshes > MD3_MAX_MESHES){
		Com_Error(ERR_DROP, "R_LoadMd3Model: %s has too many meshes.", mod->name);
	}

	// load the frames
	pinframe = (dmd3frame_t *)((byte *)pinmodel + pinmodel->ofs_frames);
	poutmodel->frames = poutframe = (dmd3frame_t *)R_HunkAlloc(
			poutmodel->num_frames * sizeof(dmd3frame_t));

	for(i = 0; i < poutmodel->num_frames; i++, pinframe++, poutframe++){
		for(j = 0; j < 3; j++){
			poutframe->mins[j] = LittleFloat(pinframe->mins[j]);
			poutframe->maxs[j] = LittleFloat(pinframe->maxs[j]);
			poutframe->translate[j] = LittleFloat(pinframe->translate[j]);
		}

		AddPointToBounds(poutframe->mins, mod->mins, mod->maxs);
		AddPointToBounds(poutframe->maxs, mod->mins, mod->maxs);
	}

	// load the tags
	if(poutmodel->num_tags){

		pintag = (dmd3tag_t *)((byte *)pinmodel + pinmodel->ofs_tags);
		poutmodel->tags = pouttag = (dmd3tag_t *)R_HunkAlloc(
				poutmodel->num_tags * sizeof(dmd3tag_t));

		for(i = 0; i < poutmodel->num_frames; i++){
			for(l = 0; l < poutmodel->num_tags; l++, pintag++, pouttag++){
				memcpy(pouttag->name, pintag->name, MD3_MAX_PATH);
				for(j = 0; j < 3; j++){
					pouttag->orient.origin[j] = LittleFloat(pintag->orient.origin[j]);
					pouttag->orient.axis[0][j] = LittleFloat(pintag->orient.axis[0][j]);
					pouttag->orient.axis[1][j] = LittleFloat(pintag->orient.axis[1][j]);
					pouttag->orient.axis[2][j] = LittleFloat(pintag->orient.axis[2][j]);
				}
			}
		}
	}

	// load the meshes
	pinmesh = (dmd3mesh_t *)((byte *)pinmodel + pinmodel->ofs_meshes);
	poutmodel->meshes = poutmesh = (mmd3mesh_t *)R_HunkAlloc(
			poutmodel->num_meshes * sizeof(mmd3mesh_t));

	for(i = 0; i < poutmodel->num_meshes; i++, poutmesh++){
		memcpy(poutmesh->name, pinmesh->name, MD3_MAX_PATH);

		pinmesh->ofs_tris = LittleLong(pinmesh->ofs_tris);
		pinmesh->ofs_tcs = LittleLong(pinmesh->ofs_tcs);
		pinmesh->ofs_verts = LittleLong(pinmesh->ofs_verts);
		pinmesh->meshsize = LittleLong(pinmesh->meshsize);

		poutmesh->num_tris = LittleLong(pinmesh->num_tris);
		poutmesh->num_verts = LittleLong(pinmesh->num_verts);

		// load the triangle indexes
		pinindex = (unsigned *)((byte *)pinmesh + pinmesh->ofs_tris);
		poutmesh->tris = poutindex = (unsigned *)R_HunkAlloc(
				poutmesh->num_tris * sizeof(unsigned) * 3);

		for(j = 0; j < poutmesh->num_tris; j++, pinindex += 3, poutindex += 3){
			poutindex[0] = (unsigned)LittleLong(pinindex[0]);
			poutindex[1] = (unsigned)LittleLong(pinindex[1]);
			poutindex[2] = (unsigned)LittleLong(pinindex[2]);
		}

		// load the texcoords
		pincoord = (dmd3coord_t *)((byte *)pinmesh + pinmesh->ofs_tcs);
		poutmesh->coords = poutcoord = (dmd3coord_t *)R_HunkAlloc(
				poutmesh->num_verts * sizeof(dmd3coord_t));

		for(j = 0; j < poutmesh->num_verts; j++, pincoord++, poutcoord++){
			poutcoord->st[0] = LittleFloat(pincoord->st[0]);
			poutcoord->st[1] = LittleFloat(pincoord->st[1]);
		}

		// load the verts and norms
		pinvert = (dmd3vertex_t *)((byte *)pinmesh + pinmesh->ofs_verts);
		poutmesh->verts = poutvert = (mmd3vertex_t *)R_HunkAlloc(
				poutmodel->num_frames * poutmesh->num_verts * sizeof(mmd3vertex_t));

		for(l = 0; l < poutmodel->num_frames; l++){
			for(j = 0; j < poutmesh->num_verts; j++, pinvert++, poutvert++){
				poutvert->point[0] = LittleShort(pinvert->point[0]) * MD3_XYZ_SCALE;
				poutvert->point[1] = LittleShort(pinvert->point[1]) * MD3_XYZ_SCALE;
				poutvert->point[2] = LittleShort(pinvert->point[2]) * MD3_XYZ_SCALE;

				lat = (pinvert->norm >> 8) & 0xff;
				lng = (pinvert->norm & 0xff);

				lat *= M_PI / 128.0;
				lng *= M_PI / 128.0;

				poutvert->normal[0] = cos(lat) * sin(lng);
				poutvert->normal[1] = sin(lat) * sin(lng);
				poutvert->normal[2] = cos(lng);
			}
		}

		pinmesh = (dmd3mesh_t *)((byte *)pinmesh + pinmesh->meshsize);
	}

	// load the skin
	R_LoadMeshSkin(mod);

	// and configs
	R_LoadMeshConfigs(mod);

	R_LoadMd3VertexArrays(mod);
}
