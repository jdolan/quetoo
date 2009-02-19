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

vec3_t r_mesh_verts[MD3_MAX_VERTS];  // verts are lerped here temporarily
vec3_t r_mesh_norms[MD3_MAX_VERTS];  // same for normal vectors

typedef struct shadow_s {
	vec3_t org;
	vec3_t dir;
	float scale;
} shadow_t;

static shadow_t shadows[MAX_EDICTS];
static int num_shadows;


/*
 * R_AddMeshShadow
 */
static void R_AddMeshShadow(const entity_t *e){
	shadow_t *sh;
	float h;

	if(!r_shadows->value)
		return;

	if(num_shadows == MAX_EDICTS)
		return;

	if(e->flags & EF_NO_SHADOW)  // no shadow for the weapon
		return;

	if(VectorCompare(e->lighting->point, vec3_origin))
		return;

	if(e->lighting->point[2] > r_view.origin[2])
		return;

	if((h = e->origin[2] - e->lighting->point[2]) > 128)
		return;

	sh = &shadows[num_shadows++];

	VectorCopy(e->lighting->point, sh->org);
	sh->org[2] += 0.1;

	// transform the normal vector to angles
	VectorAngles(e->lighting->normal, sh->dir);

	sh->scale = 5.0 / (0.5 * h + 1);
	sh->scale = sh->scale > 1.0 ? 1.0 : sh->scale;
}


/*
 * R_DrawMeshShadows
 */
void R_DrawMeshShadows(void){
	vec3_t right, up, verts[4];
	int i, j, k, l;
	float x;

	if(!r_shadows->value)
		return;

	if(!num_shadows)
		return;

	R_ResetArrayState();

	glColor4f(1.0, 1.0, 1.0, r_shadows->value);

	R_BindTexture(r_shadowtexture->texnum);

	j = k = l = 0;
	for(i = 0; i < num_shadows; i++){

		const shadow_t *sh = &shadows[i];
		AngleVectors(sh->dir, NULL, right, up);

		VectorAdd(up, right, verts[0]);
		VectorSubtract(right, up, verts[1]);
		VectorNegate(verts[0], verts[2]);
		VectorNegate(verts[1], verts[3]);

		x = r_shadowtexture->width * sh->scale;

		for(j = 0; j < 4; j++)
			VectorMA(sh->org, x, verts[j], &r_state.vertex_array_3d[k + j * 3]);
		k += 12;

		memcpy(&texunit_diffuse.texcoord_array[l], default_texcoords, sizeof(vec2_t) * 4);
		l += 8;
	}

	num_shadows = 0;

	glDrawArrays(GL_QUADS, 0, i * 4);

	glColor4ubv(color_white);
}


/*
 * R_ApplyMeshModelConfig
 */
void R_ApplyMeshModelConfig(entity_t *e){
	mesh_config_t *c;
	vec3_t forward, right, up;
	vec3_t translate, velocity;
	float f;
	int i;

	// translation is applied differently for view weapons
	if(e->flags & EF_WEAPON){
		c = e->model->view_config;

		// adjust forward/back offset according to fov
		f = (r_view.fov_x - 100.0) / -4.0;

		AngleVectors(e->angles, forward, right, up);

		VectorMA(e->origin, c->translate[0] + f, forward, e->origin);

		// adjust up and right according to time and velocity
		VectorCopy(r_view.velocity, velocity);
		velocity[2] = 0.0;

		f = sin(r_view.time * 5.0) * (0.25 + VectorLength(velocity) / 400.0);

		VectorMA(e->origin, c->translate[1] + f, right, e->origin);
		VectorMA(e->origin, c->translate[2] + f, up, e->origin);
	}
	else {  // versus world entities
		c = e->model->world_config;

		// normalize the config's translation to the entity scale
		for(i = 0; i < 3; i++)
			translate[i] = c->translate[i] * e->scale[i];

		// and add it to the origin
		VectorAdd(e->origin, translate, e->origin);
	}

	for(i = 0; i < 3; i++)  // apply config scale
		e->scale[i] *= c->scale[i];

	// lastly apply effects
	e->flags |= c->flags;
}


/*
 * R_CullMeshModel
 */
qboolean R_CullMeshModel(const entity_t *e){
	vec3_t mins, maxs;
	int i;

	if(e->flags & EF_WEAPON)  // never cull the weapon
		return false;

	// determine scaled mins/maxs
	for(i = 0; i < 3; i++){
		mins[i] = e->model->mins[i] * e->scale[i];
		maxs[i] = e->model->maxs[i] * e->scale[i];
	}

	VectorAdd(e->origin, mins, mins);
	VectorAdd(e->origin, maxs, maxs);

	return R_CullBox(mins, maxs);
}


/*
 * R_SetMeshColor_default
 */
static void R_SetMeshColor_default(const entity_t *e){
	vec4_t color;
	float f;
	int i;

	VectorCopy(e->lighting->color, color);

	if(e->flags & EF_BLEND)
		color[3] = 0.25;
	else
		color[3] = 1.0;

	if(e->flags & EF_PULSE){
		f = (1.0 + sin((r_view.time + e->model->vertexcount) * 6.0)) * 0.5;
		VectorScale(color, 0.75 + f, color);
	}

	f = 0.0;
	for(i = 0; i < 3; i++){  // find brightest component

		if(color[i] > f)  // keep it
			f = color[i];
	}

	if(f > 1.0)  // scale it back to 1.0
		VectorScale(color, 1.0 / f, color);

	glColor4fv(color);
}


/*
 * R_SetMeshState_default
 */
static void R_SetMeshState_default(const entity_t *e){

	if(e->skin)  // resolve texture
		R_BindTexture(e->skin->texnum);
	else
		R_BindTexture(e->model->skin->texnum);

	R_SetMeshColor_default(e);  // and color

	if(e->flags & EF_WEAPON)  // prevent weapon from poking into walls
		glDepthRange(0.0, 0.3);

	if(r_state.lighting_enabled && !(e->flags & EF_NO_LIGHTING))  // hardware lighting
		R_EnableLightsByRadius(e->origin);

	glPushMatrix();  // now rotate and translate to the ent's origin

	R_RotateForEntity(e);
}


/*
 * R_ResetMeshState_default
 */
static void R_ResetMeshState_default(const entity_t *e){

	if(e->flags & EF_WEAPON)
		glDepthRange(0.0, 1.0);

	glPopMatrix();
}


/*
 * R_DrawMeshModelShell_default
 *
 * Draws an animated, colored shell for the specified entity.  Rather than
 * re-lerping or re-scaling the entity, the currently bound vertex arrays
 * are simply re-drawn using a small depth offset.
 */
static void R_DrawMeshModelShell_default(const entity_t *e){
	float f;
	int i;

	if(VectorCompare(e->shell, vec3_origin))
		return;

	glColor3fv(e->shell);

	R_BindTexture(r_envmaptextures[2]->texnum);

	glEnable(GL_POLYGON_OFFSET_FILL);
	glPolygonOffset(-1.0, 1.0);

	R_EnableBlend(true);
	R_BlendFunc(GL_SRC_ALPHA, GL_ONE);

	// copy the models texcoords in and augment them
	memcpy(&texunit_diffuse.texcoord_array, e->model->texcoords,
			sizeof(vec2_t) * e->model->vertexcount);

	f = r_view.time / 3.0;

	for(i = 0; i < e->model->vertexcount; i++){
		texunit_diffuse.texcoord_array[2 * i + 0] += f;
		texunit_diffuse.texcoord_array[2 * i + 1] += f;
	}

	glDrawArrays(GL_TRIANGLES, 0, e->model->vertexcount);

	R_BlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	R_EnableBlend(false);

	glPolygonOffset(0.0, 0.0);
	glDisable(GL_POLYGON_OFFSET_FILL);

	glColor4ubv(color_white);
}


/*
 * R_DrawMd2ModelLerped_default
 */
static void R_DrawMd2ModelLerped_default(const entity_t *e){
	const dmd2_t *md2;
	const dmd2frame_t *frame, *oldframe;
	const dmd2vertex_t *v, *ov, *verts;
	const dmd2triangle_t *tri;
	vec3_t trans, scale, oldscale;
	int i, vertind;

	R_ResetArrayState();

	R_BindArray(GL_TEXTURE_COORD_ARRAY, GL_FLOAT, e->model->texcoords);

	md2 = (const dmd2_t *)e->model->extradata;

	frame = (const dmd2frame_t *)((byte *)md2 + md2->ofs_frames + e->frame * md2->framesize);
	verts = v = frame->verts;

	oldframe = (const dmd2frame_t *)((byte *)md2 + md2->ofs_frames + e->oldframe * md2->framesize);
	ov = oldframe->verts;

	// trans should be the delta back to the previous frame * backlerp

	for(i = 0; i < 3; i++){
		trans[i] = e->backlerp * oldframe->translate[i] + e->lerp * frame->translate[i];

		scale[i] = e->lerp * frame->scale[i];
		oldscale[i] = e->backlerp * oldframe->scale[i];
	}

	// lerp the verts
	for(i = 0; i < md2->num_xyz; i++, v++, ov++){
		VectorSet(r_mesh_verts[i],
				trans[0] + ov->v[0] * oldscale[0] + v->v[0] * scale[0],
				trans[1] + ov->v[1] * oldscale[1] + v->v[1] * scale[1],
				trans[2] + ov->v[2] * oldscale[2] + v->v[2] * scale[2]);

		if(r_state.lighting_enabled){  // and the norms
			VectorSet(r_mesh_norms[i],
					e->backlerp * bytedirs[ov->n][0] + e->lerp * bytedirs[v->n][0],
					e->backlerp * bytedirs[ov->n][1] + e->lerp * bytedirs[v->n][1],
					e->backlerp * bytedirs[ov->n][2] + e->lerp * bytedirs[v->n][2]);
		}
	}

	tri = (dmd2triangle_t *)((byte *)md2 + md2->ofs_tris);
	vertind = 0;

	for(i = 0; i < md2->num_tris; i++, tri++){  // draw the tris
		
		VectorCopy(r_mesh_verts[tri->index_xyz[0]], (&r_state.vertex_array_3d[vertind + 0]));
		VectorCopy(r_mesh_verts[tri->index_xyz[1]], (&r_state.vertex_array_3d[vertind + 3]));
		VectorCopy(r_mesh_verts[tri->index_xyz[2]], (&r_state.vertex_array_3d[vertind + 6]));

		if(r_state.lighting_enabled){  // normal vectors for lighting
			VectorCopy(r_mesh_norms[tri->index_xyz[0]], (&r_state.normal_array[vertind + 0]));
			VectorCopy(r_mesh_norms[tri->index_xyz[1]], (&r_state.normal_array[vertind + 3]));
			VectorCopy(r_mesh_norms[tri->index_xyz[2]], (&r_state.normal_array[vertind + 6]));
		}

		vertind += 9;
	}

	glDrawArrays(GL_TRIANGLES, 0, md2->num_tris * 3);

	r_view.mesh_polys += md2->num_tris;

	R_BindDefaultArray(GL_TEXTURE_COORD_ARRAY);
}


/*
 * R_DrawMd3ModelLerped_default
 */
static void R_DrawMd3ModelLerped_default(const entity_t *e){
	const mmd3_t *md3;
	const dmd3frame_t *frame, *oldframe;
	const mmd3mesh_t *mesh;
	vec3_t trans;
	int i, j, k, vertind;
	unsigned *tri;

	R_ResetArrayState();

	R_BindArray(GL_TEXTURE_COORD_ARRAY, GL_FLOAT, e->model->texcoords);

	md3 = (mmd3_t *)e->model->extradata;

	frame = &md3->frames[e->frame];
	oldframe = &md3->frames[e->oldframe];

	for(i = 0; i < 3; i++)  // calculate the trans
		trans[i] = e->backlerp * oldframe->translate[i] + e->lerp * frame->translate[i];

	for(k = 0, mesh = md3->meshes; k < md3->num_meshes; k++, mesh++){  // draw the meshes

		const mmd3vertex_t *v = mesh->verts + e->frame * mesh->num_verts;
		const mmd3vertex_t *ov = mesh->verts + e->oldframe * mesh->num_verts;

		for(i = 0; i < mesh->num_verts; i++, v++, ov++){  // lerp the verts
			VectorSet(r_mesh_verts[i],
					trans[0] + ov->point[0] * e->backlerp + v->point[0] * e->lerp,
					trans[1] + ov->point[1] * e->backlerp + v->point[1] * e->lerp,
					trans[2] + ov->point[2] * e->backlerp + v->point[2] * e->lerp);

			if(r_state.lighting_enabled){  // and the norms
				VectorSet(r_mesh_norms[i],
						v->normal[0] + (ov->normal[0] - v->normal[0]) * e->backlerp,
						v->normal[1] + (ov->normal[1] - v->normal[1]) * e->backlerp,
						v->normal[2] + (ov->normal[2] - v->normal[2]) * e->backlerp);
			}
		}

		tri = mesh->tris;
		vertind = 0;

		for(j = 0; j < mesh->num_tris; j++, tri += 3){  // draw the tris

			VectorCopy(r_mesh_verts[tri[0]], (&r_state.vertex_array_3d[vertind + 0]));
			VectorCopy(r_mesh_verts[tri[1]], (&r_state.vertex_array_3d[vertind + 3]));
			VectorCopy(r_mesh_verts[tri[2]], (&r_state.vertex_array_3d[vertind + 6]));

			if(r_state.lighting_enabled){  // normal vectors for lighting
				VectorCopy(r_mesh_norms[tri[0]], (&r_state.normal_array[vertind + 0]));
				VectorCopy(r_mesh_norms[tri[1]], (&r_state.normal_array[vertind + 3]));
				VectorCopy(r_mesh_norms[tri[2]], (&r_state.normal_array[vertind + 6]));
			}

			vertind += 9;
		}

		glDrawArrays(GL_TRIANGLES, 0, mesh->num_tris * 3);

		r_view.mesh_polys += mesh->num_tris;
	}

	R_BindDefaultArray(GL_TEXTURE_COORD_ARRAY);
}


/*
 * R_DrawMeshModelArrays_default
 */
static void R_DrawMeshModelArrays_default(const entity_t *e){

	R_SetArrayState(e->model);

	glDrawArrays(GL_TRIANGLES, 0, e->model->vertexcount);

	r_view.mesh_polys += e->model->vertexcount / 3;
}


/*
 * R_DrawMeshModel_default
 */
void R_DrawMeshModel_default(entity_t *e){

	if(e->frame >= e->model->num_frames || e->frame < 0){
		Com_Warn("R_DrawMeshModel %s: no such frame %d\n", e->model->name, e->frame);
		e->frame = 0;
	}

	if(e->oldframe >= e->model->num_frames || e->oldframe < 0){
		Com_Warn("R_DrawMeshModel %s: no such oldframe %d\n", e->model->name, e->oldframe);
		e->oldframe = 0;
	}

	if(e->lighting->dirty){  // update static lighting info
		if(e->flags & EF_WEAPON)
			R_LightPoint(r_view.origin, e->lighting);
		else
			R_LightPoint(e->origin, e->lighting);
	}

	R_SetMeshState_default(e);

	if(e->model->num_frames == 1)  // draw static arrays
		R_DrawMeshModelArrays_default(e);

	else if(e->model->type == mod_md2)  // or an interpolated md2
		R_DrawMd2ModelLerped_default(e);

	else if(e->model->type == mod_md3)  // or an interpolated md3
		R_DrawMd3ModelLerped_default(e);

	R_DrawMeshModelShell_default(e);  // lastly draw any shells

	R_ResetMeshState_default(e);

	R_AddMeshShadow(e);
}
