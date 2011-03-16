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

vec3_t r_mesh_verts[MD3_MAX_VERTS];  // verts are lerped here temporarily
vec3_t r_mesh_norms[MD3_MAX_VERTS];  // same for normal vectors


/*
 * R_ApplyMeshModelConfig
 *
 * Applies any client-side transformations specified by the model's world or
 * view configuration structure.
 */
void R_ApplyMeshModelConfig(r_entity_t *e){
	const r_mesh_config_t *c;
	vec3_t translate;
	int i;

	// translation is applied differently for view weapons
	if(e->effects & EF_WEAPON){

		// adjust forward / back offset according to field of view
		float f = (r_view.fov_x - 90.0) * 0.15;

		// add bob on all 3 axis as well
		float b = r_view.bob * 0.4;

		c = e->model->view_config;

		VectorMA(e->origin, c->translate[0] + f + b, r_view.forward, e->origin);
		VectorMA(e->origin, 6.0, r_view.right, e->origin);

		b = r_view.bob * 0.25;

		VectorMA(e->origin, c->translate[1] + b, r_view.right, e->origin);
		VectorMA(e->origin, c->translate[2] + b, r_view.up, e->origin);
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
	e->effects |= c->flags;
}


/*
 * R_CullMeshModel
 */
qboolean R_CullMeshModel(const r_entity_t *e){
	vec3_t mins, maxs;
	int i;

	if(e->effects & EF_WEAPON)  // never cull the weapon
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
static void R_SetMeshColor_default(const r_entity_t *e){
	vec4_t color;
	float f;
	int i;

	VectorCopy(e->lighting->color, color);

	if(e->effects & EF_BLEND)
		color[3] = 0.25;
	else
		color[3] = 1.0;

	if(e->effects & EF_PULSE){
		f = sin((r_view.time + e->model->num_verts) * 6.0) * 0.75;
		VectorScale(color, 1.0 + f, color);
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
static void R_SetMeshState_default(const r_entity_t *e){

	if(e->model->num_frames == 1){  // draw static arrays
		R_SetArrayState(e->model);
	}
	else {  // or use the default arrays
		R_ResetArrayState();

		R_BindArray(GL_TEXTURE_COORD_ARRAY, GL_FLOAT, e->model->texcoords);
	}

	if(!r_draw_bsp_wireframe->value){
		if(e->skin)  // resolve texture
			R_BindTexture(e->skin->texnum);
		else
			R_BindTexture(e->model->skin->texnum);

		R_SetMeshColor_default(e);
	}

	// enable hardware light sources, transform static lighting position
	if(r_state.lighting_enabled && !(e->effects & EF_NO_LIGHTING)){

		R_EnableLightsByRadius(e->origin);

		R_ApplyLighting(e->lighting);

#if 0
		if(e->effects & EF_WEAPON){
			r_entity_t ent;
			int i;

			for(i = 0; i < MAX_ACTIVE_LIGHTS; i++){
				const r_bsp_light_t *l = e->lighting->bsp_lights[i];

				if(!l)
					break;

				memset(&ent, 0, sizeof(ent));

				ent.lerp = 1.0;

				VectorSet(ent.scale, 1.0, 1.0, 1.0);
				VectorCopy(l->origin, ent.origin);

				R_AddEntity(&ent);
			}
		}
#endif
	}

	if(e->effects & EF_WEAPON)  // prevent weapon from poking into walls
		glDepthRange(0.0, 0.3);

	// now rotate and translate to the ent's origin
	R_RotateForEntity(e);
}


/*
 * R_ResetMeshState_default
 */
static void R_ResetMeshState_default(const r_entity_t *e){

	if(e->model->num_frames > 1)
		R_BindDefaultArray(GL_TEXTURE_COORD_ARRAY);

	if(e->effects & EF_WEAPON)
		glDepthRange(0.0, 1.0);

	R_RotateForEntity(NULL);
}


#define MESH_SHADOW_SCALE 1.5
#define MESH_SHADOW_ALPHA 0.3

/*
 * R_RotateForMeshShadow_default
 *
 * Applies translation, rotation and scale for the shadow of the specified
 * entity.  In order to reuse the vertex arrays from the primary rendering
 * pass, the shadow origin must transformed into model-view space.
 */
static void R_RotateForMeshShadow_default(const r_entity_t *e){
	vec3_t origin;
	float height, threshold, scale;

	if(!e){
		glPopMatrix();
		return;
	}

	R_TransformForEntity(e, e->lighting->shadow_origin, origin);

	height = -origin[2];

	threshold = LIGHTING_MAX_SHADOW_DISTANCE / e->scale[2];

	scale = MESH_SHADOW_SCALE * (threshold - height) / threshold;

	glPushMatrix();

	glTranslatef(origin[0], origin[1], -height + 1.0);

	glRotatef(-e->angles[PITCH], 0.0, 1.0, 0.0);

	glScalef(scale, scale, 0.0);
}


/*
 * R_DrawMeshModelShell_default
 *
 * Draws an animated, colored shell for the specified entity.  Rather than
 * re-lerping or re-scaling the entity, the currently bound vertex arrays
 * are simply re-drawn using a small depth offset.
 */
static void R_DrawMeshModelShell_default(const r_entity_t *e){

	if(VectorCompare(e->shell, vec3_origin))
		return;

	glColor3fv(e->shell);

	R_BindTexture(r_envmap_images[2]->texnum);

	R_EnableShell(true);

	glDrawArrays(GL_TRIANGLES, 0, e->model->num_verts);

	R_EnableShell(false);

	glColor4ubv(color_white);
}


/*
 * R_DrawMeshShadow_default
 *
 * Re-draws the mesh using the stencil test.  Meshes with stale lighting
 * information, or with a lighting point above our view, are not drawn.
 */
static void R_DrawMeshShadow_default(r_entity_t *e){
	const qboolean lighting = r_state.lighting_enabled;

	if(!r_shadows->value)
		return;

	if(r_draw_bsp_wireframe->value)
		return;

	if(e->effects & EF_NO_SHADOW)
		return;

	if(e->effects & EF_BLEND)
		return;

	if(VectorCompare(e->lighting->shadow_origin, vec3_origin))
		return;

	if(e->lighting->shadow_origin[2] > r_view.origin[2])
		return;

	R_EnableTexture(&texunit_diffuse, false);

	glColor4f(0.0, 0.0, 0.0, r_shadows->value * MESH_SHADOW_ALPHA);

	R_EnableBlend(true);

	R_RotateForMeshShadow_default(e);

	glDepthRange(0.0, 0.999);

	R_EnableStencilTest(true);

	if(lighting)
		R_EnableLighting(NULL, false);

	glDrawArrays(GL_TRIANGLES, 0, e->model->num_verts);

	if(lighting)
		R_EnableLighting(r_state.mesh_program, true);

	R_EnableStencilTest(false);

	glDepthRange(0.0, 1.0);

	R_RotateForMeshShadow_default(NULL);

	R_EnableBlend(false);

	R_EnableTexture(&texunit_diffuse, true);

	glColor4ubv(color_white);
}


/*
 * R_DrawMd2ModelLerped_default
 */
static void R_DrawMd2ModelLerped_default(const r_entity_t *e){
	const d_md2_t *md2;
	const d_md2_frame_t *frame, *old_frame;
	const d_md2_vertex_t *v, *ov, *verts;
	const d_md2_tri_t *tri;
	vec3_t trans, scale, oldscale;
	int i, vertind;

	md2 = (const d_md2_t *)e->model->extra_data;

	frame = (const d_md2_frame_t *)((byte *)md2 + md2->ofs_frames + e->frame * md2->frame_size);
	verts = v = frame->verts;

	old_frame = (const d_md2_frame_t *)((byte *)md2 + md2->ofs_frames + e->old_frame * md2->frame_size);
	ov = old_frame->verts;

	// trans should be the delta back to the previous frame * backlerp

	for(i = 0; i < 3; i++){
		trans[i] = e->back_lerp * old_frame->translate[i] + e->lerp * frame->translate[i];

		scale[i] = e->lerp * frame->scale[i];
		oldscale[i] = e->back_lerp * old_frame->scale[i];
	}

	// lerp the verts
	for(i = 0; i < md2->num_xyz; i++, v++, ov++){
		VectorSet(r_mesh_verts[i],
				trans[0] + ov->v[0] * oldscale[0] + v->v[0] * scale[0],
				trans[1] + ov->v[1] * oldscale[1] + v->v[1] * scale[1],
				trans[2] + ov->v[2] * oldscale[2] + v->v[2] * scale[2]);

		if(r_state.lighting_enabled){  // and the norms
			VectorSet(r_mesh_norms[i],
					e->back_lerp * approximate_normals[ov->n][0] + e->lerp * approximate_normals[v->n][0],
					e->back_lerp * approximate_normals[ov->n][1] + e->lerp * approximate_normals[v->n][1],
					e->back_lerp * approximate_normals[ov->n][2] + e->lerp * approximate_normals[v->n][2]);
		}
	}

	tri = (d_md2_tri_t *)((byte *)md2 + md2->ofs_tris);
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
}


/*
 * R_DrawMd3ModelLerped_default
 */
static void R_DrawMd3ModelLerped_default(const r_entity_t *e){
	const r_md3_t *md3;
	const d_md3_frame_t *frame, *old_frame;
	const r_md3_mesh_t *mesh;
	vec3_t trans;
	int i, j, k, vertind;
	unsigned *tri;

	md3 = (r_md3_t *)e->model->extra_data;

	frame = &md3->frames[e->frame];
	old_frame = &md3->frames[e->old_frame];

	for(i = 0; i < 3; i++)  // calculate the translation
		trans[i] = e->back_lerp * old_frame->translate[i] + e->lerp * frame->translate[i];

	for(k = 0, mesh = md3->meshes; k < md3->num_meshes; k++, mesh++){  // draw the meshes

		const r_md3_vertex_t *v = mesh->verts + e->frame * mesh->num_verts;
		const r_md3_vertex_t *ov = mesh->verts + e->old_frame * mesh->num_verts;

		for(i = 0; i < mesh->num_verts; i++, v++, ov++){  // lerp the verts
			VectorSet(r_mesh_verts[i],
					trans[0] + ov->point[0] * e->back_lerp + v->point[0] * e->lerp,
					trans[1] + ov->point[1] * e->back_lerp + v->point[1] * e->lerp,
					trans[2] + ov->point[2] * e->back_lerp + v->point[2] * e->lerp);

			if(r_state.lighting_enabled){  // and the norms
				VectorSet(r_mesh_norms[i],
						v->normal[0] + (ov->normal[0] - v->normal[0]) * e->back_lerp,
						v->normal[1] + (ov->normal[1] - v->normal[1]) * e->back_lerp,
						v->normal[2] + (ov->normal[2] - v->normal[2]) * e->back_lerp);
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
	}
}


/*
 * R_DrawMeshModelArrays_default
 */
static void R_DrawMeshModelArrays_default(const r_entity_t *e){

	glDrawArrays(GL_TRIANGLES, 0, e->model->num_verts);
}


/*
 * R_DrawMeshModel_default
 */
void R_DrawMeshModel_default(r_entity_t *e){

	if(e->frame >= e->model->num_frames || e->frame < 0){
		Com_Warn("R_DrawMeshModel %s: no such frame %d\n", e->model->name, e->frame);
		e->frame = 0;
	}

	if(e->old_frame >= e->model->num_frames || e->old_frame < 0){
		Com_Warn("R_DrawMeshModel %s: no such old_frame %d\n", e->model->name, e->old_frame);
		e->old_frame = 0;
	}

	if(e->lighting->state != LIGHTING_READY){  // update static lighting info

		if(e->effects & EF_WEAPON)
			VectorCopy(r_view.origin, e->lighting->origin);
		else
			VectorCopy(e->origin, e->lighting->origin);

		e->lighting->radius = e->scale[0] * e->model->radius;

		// determine scaled mins/maxs in world space
		VectorMA(e->lighting->origin, e->scale[0], e->model->mins, e->lighting->mins);
		VectorMA(e->lighting->origin, e->scale[0], e->model->maxs, e->lighting->maxs);

		R_UpdateLighting(e->lighting);
	}

	R_SetMeshState_default(e);

	if(e->model->num_frames == 1)  // draw static arrays
		R_DrawMeshModelArrays_default(e);

	else if(e->model->type == mod_md2)  // or an interpolated md2
		R_DrawMd2ModelLerped_default(e);

	else if(e->model->type == mod_md3)  // or an interpolated md3
		R_DrawMd3ModelLerped_default(e);

	R_DrawMeshModelShell_default(e);  // draw any shell effects

	R_DrawMeshShadow_default(e);  // lastly draw the shadow

	R_ResetMeshState_default(e);

	r_view.mesh_polys += e->model->num_verts / 3;
}
