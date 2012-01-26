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

/*
 * R_LoadMeshSkin
 *
 * Resolves the skin for the specified model.  By default, we simply load
 * "skin.tga" in the model's directory.
 */
static void R_LoadMeshSkin(r_model_t *mod) {
	char skin[MAX_QPATH];

	Dirname(mod->name, skin);
	strcat(skin, "skin");

	mod->skin = R_LoadImage(skin, it_skin);
}

/*
 * R_LoadMd3Animations
 *
 * Parses animation.cfg, loading the frame specifications for the given model.
 */
static void R_LoadMd3Animations(r_model_t *mod) {
	r_md3_t *md3;
	char path[MAX_QPATH];
	const char *buffer, *c;
	void *buf;
	unsigned short skip;

	md3 = (r_md3_t *) mod->extra_data;

	Dirname(mod->name, path);
	strcat(path, "animation.cfg");

	if (Fs_LoadFile(path, &buf) == -1) {
		Com_Warn("R_LoadMd3Animation: No animation.cfg for %s\n", mod->name);
		return;
	}

	md3->animations = (r_md3_animation_t *) R_HunkAlloc(
			sizeof(r_md3_animation_t) * MD3_MAX_ANIMATIONS);

	buffer = (char *) buf;
	skip = 0;

	while (true) {

		c = ParseToken(&buffer);

		if (*c == '\0')
			break;

		if (*c >= '0' && *c <= '9') {
			r_md3_animation_t *a = &md3->animations[md3->num_animations];

			a->first_frame = (unsigned short)strtoul(c, NULL, 0);
			c = ParseToken(&buffer);
			a->num_frames = (unsigned short)strtoul(c, NULL, 0);
			c = ParseToken(&buffer);
			a->looped_frames = (unsigned short)strtoul(c, NULL, 0);
			c = ParseToken(&buffer);
			a->hz = (unsigned short)strtoul(c, NULL, 0);

			if (md3->num_animations == ANIM_LEGS_WALKCR)
				skip = a->first_frame
						- md3->animations[ANIM_TORSO_GESTURE].first_frame;

			if (md3->num_animations >= ANIM_LEGS_WALKCR)
				a->first_frame -= skip;

			if (!a->num_frames)
				Com_Warn("R_LoadMd3Animations: %s: No frames for %d\n",
						mod->name, md3->num_animations);

			if (!a->hz)
				Com_Warn("R_LoadMd3Animations: %s: No hz for %d\n", mod->name,
						md3->num_animations);

			Com_Debug("R_LoadMd3Animations: Parsed %d: %d %d %d %d\n",
					md3->num_animations, a->first_frame, a->num_frames,
					a->looped_frames, a->hz);

			md3->num_animations++;
		}

		if (md3->num_animations == MD3_MAX_ANIMATIONS) {
			Com_Warn("R_LoadMd3Animations: MD3_MAX_ANIMATIONS reached: %s\n",
					mod->name);
			break;
		}
	}

	Com_Debug("R_LoadMd3Animations: Loaded %d animations: %s\n",
			md3->num_animations, mod->name);
}

/*
 * R_LoadMeshConfig
 */
static void R_LoadMeshConfig(r_mesh_config_t *config, const char *path) {
	const char *buffer, *c;
	void *buf;

	if (Fs_LoadFile(path, &buf) == -1)
		return;

	buffer = (char *) buf;

	while (true) {

		c = ParseToken(&buffer);

		if (*c == '\0')
			break;

		if (!strcmp(c, "translate")) {
			sscanf(ParseToken(&buffer), "%f %f %f", &config->translate[0],
					&config->translate[1], &config->translate[2]);
			continue;
		}

		if (!strcmp(c, "scale")) {
			sscanf(ParseToken(&buffer), "%f", &config->scale);
			continue;
		}

		if (!strcmp(c, "alpha_test")) {
			config->flags |= EF_ALPHATEST;
			continue;
		}

		if (!strcmp(c, "blend")) {
			config->flags |= EF_BLEND;
			continue;
		}
	}

	Fs_FreeFile(buf);
}

/*
 * R_LoadMeshConfigs
 */
static void R_LoadMeshConfigs(r_model_t *mod) {
	char path[MAX_QPATH];

	mod->world_config
			= (r_mesh_config_t *) R_HunkAlloc(sizeof(r_mesh_config_t));
	mod->view_config = (r_mesh_config_t *) R_HunkAlloc(sizeof(r_mesh_config_t));
	mod->link_config = (r_mesh_config_t *) R_HunkAlloc(sizeof(r_mesh_config_t));

	mod->world_config->scale = 1.0;

	Dirname(mod->name, path);

	R_LoadMeshConfig(mod->world_config, va("%sworld.cfg", path));

	// by default, additional configs inherit from world
	memcpy(mod->view_config, mod->world_config, sizeof(r_mesh_config_t));
	memcpy(mod->link_config, mod->world_config, sizeof(r_mesh_config_t));

	R_LoadMeshConfig(mod->view_config, va("%sview.cfg", path));
	R_LoadMeshConfig(mod->link_config, va("%slink.cfg", path));
}

/*
 * R_LoadMd3VertexArrays
 */
static void R_LoadMd3VertexArrays(r_model_t *mod) {
	r_md3_t *md3;
	d_md3_frame_t *frame;
	r_md3_mesh_t *mesh;
	r_md3_vertex_t *v;
	d_md3_texcoord_t *texcoords;
	int i, j, vert_index, texcoord_index;
	unsigned *tri;

	R_AllocVertexArrays(mod); // allocate the arrays

	md3 = (r_md3_t *) mod->extra_data;

	frame = md3->frames;

	vert_index = texcoord_index = 0;

	for (i = 0, mesh = md3->meshes; i < md3->num_meshes; i++, mesh++) { // iterate the meshes

		v = mesh->verts;

		if (mod->num_frames == 1) { // for static models, build the verts and normals
			for (j = 0; j < mesh->num_verts; j++, v++) {
				VectorAdd(frame->translate, v->point, r_mesh_verts[j]);
				VectorCopy(v->normal, r_mesh_norms[j]);
			}
		}

		tri = mesh->tris;
		texcoords = mesh->coords;

		for (j = 0; j < mesh->num_tris; j++, tri += 3) { // populate the arrays

			if (mod->num_frames == 1) {
				VectorCopy(r_mesh_verts[tri[0]], (&mod->verts[vert_index + 0]));
				VectorCopy(r_mesh_verts[tri[1]], (&mod->verts[vert_index + 3]));
				VectorCopy(r_mesh_verts[tri[2]], (&mod->verts[vert_index + 6]));

				VectorCopy(r_mesh_norms[tri[0]], (&mod->normals[vert_index + 0]));
				VectorCopy(r_mesh_norms[tri[1]], (&mod->normals[vert_index + 3]));
				VectorCopy(r_mesh_norms[tri[2]], (&mod->normals[vert_index + 6]));
			}

			memcpy(&mod->texcoords[texcoord_index + 0], &texcoords[tri[0]], sizeof(vec2_t));
			memcpy(&mod->texcoords[texcoord_index + 2], &texcoords[tri[1]], sizeof(vec2_t));
			memcpy(&mod->texcoords[texcoord_index + 4], &texcoords[tri[2]], sizeof(vec2_t));

			texcoord_index += 6;
			vert_index += 9;
		}
	}
}

/*
 * R_LoadMd3Model
 */
void R_LoadMd3Model(r_model_t *mod, void *buffer) {
	int version, i, j, l;
	d_md3_t *inmodel;
	r_md3_t *outmodel;
	d_md3_frame_t *inframe, *outframe;
	d_md3_tag_t *intag;
	r_md3_tag_t *outtag;
	d_md3_orientation_t orient;
	d_md3_mesh_t *inmesh;
	r_md3_mesh_t *outmesh;
	d_md3_texcoord_t *incoord, *outcoord;
	d_md3_vertex_t *invert;
	r_md3_vertex_t *outvert;
	unsigned *inindex, *outindex;
	float lat, lng;

	inmodel = (d_md3_t *) buffer;

	version = LittleLong(inmodel->version);
	if (version != MD3_VERSION) {
		Com_Error(ERR_DROP, "R_LoadMd3Model: %s has wrong version number "
			"(%i should be %i)", mod->name, version, MD3_VERSION);
	}

	mod->type = mod_md3;
	mod->version = version;

	outmodel = (r_md3_t *) R_HunkAlloc(sizeof(r_md3_t));

	// byte swap the header fields and sanity check
	inmodel->ofs_frames = LittleLong(inmodel->ofs_frames);
	inmodel->ofs_tags = LittleLong(inmodel->ofs_tags);
	inmodel->ofs_meshes = LittleLong(inmodel->ofs_meshes);

	mod->num_frames = outmodel->num_frames = LittleLong(inmodel->num_frames);
	outmodel->num_tags = LittleLong(inmodel->num_tags);
	outmodel->num_meshes = LittleLong(inmodel->num_meshes);

	if (outmodel->num_frames < 1) {
		Com_Error(ERR_DROP, "R_LoadMd3Model: %s has no frames.", mod->name);
	}

	if (outmodel->num_frames > MD3_MAX_FRAMES) {
		Com_Error(ERR_DROP, "R_LoadMd3Model: %s has too many frames.",
				mod->name);
	}

	if (outmodel->num_tags > MD3_MAX_TAGS) {
		Com_Error(ERR_DROP, "R_LoadMd3Model: %s has too many tags.", mod->name);
	}

	if (outmodel->num_meshes > MD3_MAX_MESHES) {
		Com_Error(ERR_DROP, "R_LoadMd3Model: %s has too many meshes.",
				mod->name);
	}

	// load the frames
	inframe = (d_md3_frame_t *) ((byte *) inmodel + inmodel->ofs_frames);
	outmodel->frames = outframe = (d_md3_frame_t *) R_HunkAlloc(
			outmodel->num_frames * sizeof(d_md3_frame_t));

	ClearBounds(mod->mins, mod->maxs);

	for (i = 0; i < outmodel->num_frames; i++, inframe++, outframe++) {
		for (j = 0; j < 3; j++) {
			outframe->mins[j] = LittleFloat(inframe->mins[j]);
			outframe->maxs[j] = LittleFloat(inframe->maxs[j]);
			outframe->translate[j] = LittleFloat(inframe->translate[j]);
		}

		AddPointToBounds(outframe->mins, mod->mins, mod->maxs);
		AddPointToBounds(outframe->maxs, mod->mins, mod->maxs);
	}

	// load the tags
	if (outmodel->num_tags) {

		intag = (d_md3_tag_t *) ((byte *) inmodel + inmodel->ofs_tags);
		outmodel->tags = outtag
				= (r_md3_tag_t *) R_HunkAlloc(
						outmodel->num_tags * outmodel->num_frames
								* sizeof(r_md3_tag_t));

		for (i = 0; i < outmodel->num_frames; i++) {
			for (l = 0; l < outmodel->num_tags; l++, intag++, outtag++) {
				memcpy(outtag->name, intag->name, MD3_MAX_PATH);

				for (j = 0; j < 3; j++) {
					orient.origin[j] = LittleFloat(intag->orient.origin[j]);
					orient.axis[0][j] = LittleFloat(intag->orient.axis[0][j]);
					orient.axis[1][j] = LittleFloat(intag->orient.axis[1][j]);
					orient.axis[2][j] = LittleFloat(intag->orient.axis[2][j]);
				}

				Matrix4x4_FromVectors(&outtag->matrix, orient.axis[0],
						orient.axis[1], orient.axis[2], orient.origin);
			}
		}
	}

	// load the meshes
	inmesh = (d_md3_mesh_t *) ((byte *) inmodel + inmodel->ofs_meshes);
	outmodel->meshes = outmesh = (r_md3_mesh_t *) R_HunkAlloc(
			outmodel->num_meshes * sizeof(r_md3_mesh_t));

	for (i = 0; i < outmodel->num_meshes; i++, outmesh++) {
		memcpy(outmesh->name, inmesh->name, MD3_MAX_PATH);

		inmesh->ofs_tris = LittleLong(inmesh->ofs_tris);
		inmesh->ofs_skins = LittleLong(inmesh->ofs_skins);
		inmesh->ofs_tcs = LittleLong(inmesh->ofs_tcs);
		inmesh->ofs_verts = LittleLong(inmesh->ofs_verts);
		inmesh->size = LittleLong(inmesh->size);

		outmesh->flags = LittleLong(inmesh->flags);
		outmesh->num_skins = LittleLong(inmesh->num_skins);
		outmesh->num_tris = LittleLong(inmesh->num_tris);
		outmesh->num_verts = LittleLong(inmesh->num_verts);

		if (outmesh->num_skins > MD3_MAX_SHADERS) {
			Com_Error(ERR_DROP, "R_LoadMd3Model: %s: %s has too many skins.",
					mod->name, outmesh->name);
		}

		if (outmesh->num_tris > MD3_MAX_TRIANGLES) {
			Com_Error(ERR_DROP,
					"R_LoadMd3Model: %s: %s has too many triangles.",
					mod->name, outmesh->name);
		}

		if (outmesh->num_verts > MD3_MAX_VERTS) {
			Com_Error(ERR_DROP,
					"R_LoadMd3Model: %s: %s has too many vertexes.", mod->name,
					outmesh->name);
		}

		// load the triangle indexes
		inindex = (unsigned *) ((byte *) inmesh + inmesh->ofs_tris);
		outmesh->tris = outindex = (unsigned *) R_HunkAlloc(
				outmesh->num_tris * sizeof(unsigned) * 3);

		for (j = 0; j < outmesh->num_tris; j++, inindex += 3, outindex += 3) {
			outindex[0] = (unsigned) LittleLong(inindex[0]);
			outindex[1] = (unsigned) LittleLong(inindex[1]);
			outindex[2] = (unsigned) LittleLong(inindex[2]);
		}

		// load the texcoords
		incoord = (d_md3_texcoord_t *) ((byte *) inmesh + inmesh->ofs_tcs);
		outmesh->coords = outcoord = (d_md3_texcoord_t *) R_HunkAlloc(
				outmesh->num_verts * sizeof(d_md3_texcoord_t));

		for (j = 0; j < outmesh->num_verts; j++, incoord++, outcoord++) {
			outcoord->st[0] = LittleFloat(incoord->st[0]);
			outcoord->st[1] = LittleFloat(incoord->st[1]);
		}

		// load the verts and norms
		invert = (d_md3_vertex_t *) ((byte *) inmesh + inmesh->ofs_verts);
		outmesh->verts = outvert = (r_md3_vertex_t *) R_HunkAlloc(
				outmodel->num_frames * outmesh->num_verts
						* sizeof(r_md3_vertex_t));

		for (l = 0; l < outmodel->num_frames; l++) {
			for (j = 0; j < outmesh->num_verts; j++, invert++, outvert++) {
				outvert->point[0] = LittleShort(invert->point[0])
						* MD3_XYZ_SCALE;
				outvert->point[1] = LittleShort(invert->point[1])
						* MD3_XYZ_SCALE;
				outvert->point[2] = LittleShort(invert->point[2])
						* MD3_XYZ_SCALE;

				lat = (invert->norm >> 8) & 0xff;
				lng = (invert->norm & 0xff);

				lat *= M_PI / 128.0;
				lng *= M_PI / 128.0;

				outvert->normal[0] = cos(lat) * sin(lng);
				outvert->normal[1] = sin(lat) * sin(lng);
				outvert->normal[2] = cos(lng);
			}
		}

		Com_Debug("R_LoadMd3Model: %s: %s: %d triangles\n", mod->name,
				outmesh->name, outmesh->num_tris);

		inmesh = (d_md3_mesh_t *) ((byte *) inmesh + inmesh->size);
	}

	// load the skin for objects, and the animations for players
	if (!strstr(mod->name, "players/"))
		R_LoadMeshSkin(mod);

	else if (strstr(mod->name, "/upper"))
		R_LoadMd3Animations(mod);

	// and the configs
	R_LoadMeshConfigs(mod);

	// and finally the arrays
	R_LoadMd3VertexArrays(mod);

	Com_Debug("R_LoadMd3Model: %s\n"
		"  %d meshes\n  %d frames\n  %d tags\n  %d vertexes\n", mod->name,
			outmodel->num_meshes, outmodel->num_frames, outmodel->num_tags,
			mod->num_verts);
}

/*
 * R_LoadObjModelVertexArrays
 */
static void R_LoadObjModelVertexArrays(r_model_t *mod) {
	const r_obj_t *obj;
	const r_obj_tri_t *t;
	int i, j, vert_index, texcoord_index;

	R_AllocVertexArrays(mod);

	obj = (r_obj_t *) mod->extra_data;

	vert_index = texcoord_index = 0;

	t = obj->tris;

	for (i = 0; i < obj->num_tris; i++, t++) { // build the arrays

		const r_obj_vert_t *v = t->verts;

		for (j = 0; j < 3; j++, v++) { // each vert

			VectorCopy((&obj->verts[(v->vert - 1) * 3]),
					(&mod->verts[vert_index + j * 3]));

			if (v->normal) {
				VectorCopy((&obj->normals[(v->normal - 1) * 3]),
						(&mod->normals[vert_index + j * 3]));
			}

			if (v->texcoord) {
				memcpy(&mod->texcoords[texcoord_index + j * 2],
						&obj->texcoords[(v->texcoord - 1) * 2], sizeof(vec2_t));
			}
		}

		texcoord_index += 6;
		vert_index += 9;
	}
}

/*
 * R_LoadObjModelTris
 *
 * Triangulation of arbitrary polygons.  Assembles count tris on the model
 * from the specified array of verts.  All tris will share the first vert.
 */
static void R_LoadObjModelTris(r_obj_t *obj, const r_obj_vert_t *verts,
		int count) {
	int i;

	if (!obj->tris)
		return;

	for (i = 0; i < count; i++) { // walk around the polygon

		const int v0 = 0;
		const int v1 = 1 + i;
		const int v2 = 2 + i;

		r_obj_tri_t *t = &obj->tris[obj->num_tris_parsed + i];

		t->verts[0] = verts[v0];
		t->verts[1] = verts[v1];
		t->verts[2] = verts[v2];
	}
}

#define MAX_OBJ_FACE_VERTS 128

/*
 * R_LoadObjModelFace
 *
 * Each line consists of 3 or more vertex definitions, e.g.
 *
 *   57/13/31 58/14/32 59/15/33 21/15/19
 *
 * Tokenize the line with Com_Parse, and parse each vertex definition.
 * Faces with more than 3 vertices must be broken down into triangles.
 *
 * Returns the number of triangles produced for the specified line.
 */
static int R_LoadObjModelFace(const r_model_t *mod, r_obj_t *obj,
		const char *line) {
	r_obj_vert_t *v, verts[MAX_OBJ_FACE_VERTS];
	const char *d;
	char *e, tok[32];
	int i, tris;

	memset(verts, 0, sizeof(verts));
	i = 0;

	while (true) {

		const char *c = ParseToken(&line);

		if (*c == '\0') // done
			break;

		if (i == MAX_OBJ_FACE_VERTS) {
			Com_Error(ERR_DROP, "R_LoadObjModelFace: too many vertexes: %s.",
					mod->name);
		}

		if (!obj->tris) { // simply count verts
			i++;
			continue;
		}

		d = c;
		v = &verts[i++];

		memset(tok, 0, sizeof(tok));
		e = tok;

		while (*d) { // parse the vertex definition

			if (*d == '/') { // index delimiter, parse the token

				if (!v->vert)
					v->vert = atoi(tok);

				else if (!v->texcoord)
					v->texcoord = atoi(tok);

				else if (!v->normal)
					v->normal = atoi(tok);

				memset(tok, 0, sizeof(tok));
				e = tok;

				d++;
				continue;
			}

			*e++ = *d++;
		}

		// parse whatever is left in the token

		if (!v->vert)
			v->vert = atoi(tok);

		else if (!v->texcoord)
			v->texcoord = atoi(tok);

		else if (!v->normal)
			v->normal = atoi(tok);
	}

	tris = i - 2; // number of triangles from parsed verts

	if (tris < 1)
		Com_Error(ERR_DROP, "R_LoadObjModelFace: too few vertexes: %s.",
				mod->name);

	R_LoadObjModelTris(obj, verts, tris); // break verts up into tris

	return tris;
}

/*
 * R_LoadObjModelLine
 *
 * Parse the object file line.  If the structures have been allocated,
 * populate them.  Otherwise simply accumulate counts.
 */
static void R_LoadObjModelLine(const r_model_t *mod, r_obj_t *obj,
		const char *line) {

	if (!line || !line[0]) // don't bother
		return;

	if (!strncmp(line, "v ", 2)) { // vertex

		if (obj->verts) { // parse it
			float *f = obj->verts + obj->num_verts_parsed * 3;

			if (sscanf(line + 2, "%f %f %f", &f[0], &f[2], &f[1]) != 3)
				Com_Error(ERR_DROP,
						"R_LoadObjModelLine: Malformed vertex for %s: %s.",
						mod->name, line);

			obj->num_verts_parsed++;
		} else
			// or just count it
			obj->num_verts++;
	} else if (!strncmp(line, "vn ", 3)) { // normal

		if (obj->normals) { // parse it
			float *f = obj->normals + obj->num_normals_parsed * 3;

			if (sscanf(line + 3, "%f %f %f", &f[0], &f[1], &f[2]) != 3)
				Com_Error(ERR_DROP,
						"R_LoadObjModelLine: Malformed normal for %s: %s.",
						mod->name, line);

			obj->num_normals_parsed++;
		} else
			// or just count it
			obj->num_normals++;

	} else if (!strncmp(line, "vt ", 3)) { // texcoord

		if (obj->texcoords) { // parse it
			float *f = obj->texcoords + obj->num_texcoords_parsed * 2;

			if (sscanf(line + 3, "%f %f", &f[0], &f[1]) != 2)
				Com_Error(ERR_DROP,
						"R_LoadObjModelLine: Malformed texcoord for %s: %s.",
						mod->name, line);

			f[1] = -f[1];
			obj->num_texcoords_parsed++;
		} else
			// or just count it
			obj->num_texcoords++;
	} else if (!strncmp(line, "f ", 2)) { // face

		if (obj->tris) // parse it
			obj->num_tris_parsed += R_LoadObjModelFace(mod, obj, line + 2);
		else
			// or just count it
			obj->num_tris += R_LoadObjModelFace(mod, obj, line + 2);
	}

	// else we just ignore it
}

/*
 * R_LoadObjModel_
 *
 * Drives the actual parsing of the object file.  The file is read twice:
 * once to acquire primitive counts, and a second time to load them.
 */
static void R_LoadObjModel_(r_model_t *mod, r_obj_t *obj, const void *buffer) {
	char line[MAX_STRING_CHARS];
	const char *c;
	boolean_t comment;
	int i;

	c = buffer;
	comment = false;
	i = 0;

	memset(&line, 0, sizeof(line));

	while (*c) {

		if (*c == '#') {
			comment = true;
			c++;
			continue;
		}

		if (*c == '\r' || *c == '\n') { // end of line

			if (i && !comment)
				R_LoadObjModelLine(mod, obj, Trim(line));

			c++;
			comment = false;
			i = 0;

			memset(&line, 0, sizeof(line));

			continue;
		}

		line[i++] = *c++;
	}
}

/*
 * R_LoadObjModel
 */
void R_LoadObjModel(r_model_t *mod, void *buffer) {
	r_obj_t *obj;
	const float *v;
	int i;

	mod->type = mod_obj;

	mod->num_frames = 1;

	obj = (r_obj_t *) R_HunkAlloc(sizeof(r_obj_t));

	R_LoadObjModel_(mod, obj, buffer); // resolve counts

	if (!obj->num_verts) {
		Com_Error(ERR_DROP,
				"R_LoadObjModel: Failed to resolve vertex data: %s\n",
				mod->name);
	}

	// allocate the arrays
	obj->verts = (float *) R_HunkAlloc(obj->num_verts * sizeof(float) * 3);
	obj->normals = (float *) R_HunkAlloc(obj->num_normals * sizeof(float) * 3);
	obj->texcoords = (float *) R_HunkAlloc(
			obj->num_texcoords * sizeof(float) * 2);
	obj->tris
			= (r_obj_tri_t *) R_HunkAlloc(obj->num_tris * sizeof(r_obj_tri_t));

	R_LoadObjModel_(mod, obj, buffer); // load it

	ClearBounds(mod->mins, mod->maxs);

	v = obj->verts;
	for (i = 0; i < obj->num_verts; i++, v += 3) { // resolve mins/maxs
		AddPointToBounds(v, mod->mins, mod->maxs);
	}

	// load the skin
	R_LoadMeshSkin(mod);

	// and configs
	R_LoadMeshConfigs(mod);

	// and finally the arrays
	R_LoadObjModelVertexArrays(mod);
}

