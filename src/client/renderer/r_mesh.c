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

r_mesh_state_t r_mesh_state;

/*
 * @brief Applies any client-side transformations specified by the model's world or
 * view configuration structure.
 */
void R_ApplyMeshModelConfig(r_entity_t *e) {
	const r_mesh_config_t *c;
	vec3_t translate;
	int32_t i;

	// translation is applied differently for view weapons
	if (e->effects & EF_WEAPON) {

		// apply weapon bob on all 3 axis
		vec_t b = r_view.bob * 0.4;

		c = e->model->mesh->view_config;

		VectorMA(e->origin, c->translate[0] + b, r_view.forward, e->origin);
		VectorMA(e->origin, 6.0, r_view.right, e->origin);

		b = r_view.bob * 0.25;

		VectorMA(e->origin, c->translate[1] + b, r_view.right, e->origin);
		VectorMA(e->origin, c->translate[2] + b, r_view.up, e->origin);
	} else { // versus world and linked entities

		if (e->parent)
			c = e->model->mesh->link_config;
		else
			c = e->model->mesh->world_config;

		// normalize the config's translation to the entity scale
		for (i = 0; i < 3; i++)
			translate[i] = c->translate[i] * e->scale;

		// and add it to the origin
		VectorAdd(e->origin, translate, e->origin);
	}

	// apply scale
	e->scale *= c->scale;

	// lastly apply effects
	e->effects |= c->flags;
}

/*
 * @brief Returns the desired tag structure, or NULL.
 */
static const r_md3_tag_t *R_GetMeshModelTag(const r_model_t *mod, int32_t frame, const char *name) {

	if (frame > mod->mesh->num_frames) {
		Com_Warn("%s: Invalid frame: %d\n", mod->media.name, frame);
		return NULL;
	}

	const r_md3_t *md3 = (r_md3_t *) mod->mesh->data;
	const r_md3_tag_t *tag = &md3->tags[frame * md3->num_tags];
	int32_t i;

	for (i = 0; i < md3->num_tags; i++, tag++) {
		if (!g_strcmp0(name, tag->name)) {
			return tag;
		}
	}

	Com_Warn("%s: Tag not found: %s\n", mod->media.name, name);
	return NULL;
}

/*
 * @brief Applies transformation and rotation for the specified linked entity.
 */
void R_ApplyMeshModelTag(r_entity_t *e) {

	if (!e->parent || !e->parent->model || e->parent->model->type != MOD_MD3) {
		Com_Warn("Invalid parent entity\n");
		return;
	}

	if (!e->tag_name) {
		Com_Warn("NULL tag_name\n");
		return;
	}

	// interpolate the tag over the frames of the parent entity

	const r_md3_tag_t *start = R_GetMeshModelTag(e->parent->model, e->parent->old_frame,
			e->tag_name);
	const r_md3_tag_t *end = R_GetMeshModelTag(e->parent->model, e->parent->frame, e->tag_name);

	if (!start || !end) {
		return;
	}

	matrix4x4_t local, lerped, normalized;

	Matrix4x4_Concat(&local, &e->parent->matrix, &e->matrix);

	Matrix4x4_Interpolate(&lerped, &end->matrix, &start->matrix, e->parent->back_lerp);
	Matrix4x4_Normalize(&normalized, &lerped);

	Matrix4x4_Concat(&e->matrix, &local, &normalized);

	//Com_Debug("%s: %3.2f %3.2f %3.2f\n", e->tag_name,
	//		e->matrix.m[0][3], e->matrix.m[1][3], e->matrix.m[2][3]);
}

/*
 * @brief
 */
_Bool R_CullMeshModel(const r_entity_t *e) {
	vec3_t mins, maxs;

	if (e->effects & EF_WEAPON) // never cull the weapon
		return false;

	// calculate scaled bounding box in world space

	VectorMA(e->origin, e->scale, e->model->mins, mins);
	VectorMA(e->origin, e->scale, e->model->maxs, maxs);

	return R_CullBox(mins, maxs);
}

/*
 * @brief Updates static lighting information for the specified mesh entity.
 */
void R_UpdateMeshModelLighting(const r_entity_t *e) {

	if (e->lighting->state == LIGHTING_READY)
		return;

	if (e->effects & EF_WEAPON)
		VectorCopy(r_view.origin, e->lighting->origin);
	else
		VectorCopy(e->origin, e->lighting->origin);

	e->lighting->radius = e->scale * e->model->radius;

	// calculate scaled bounding box in world space
	VectorMA(e->lighting->origin, e->scale, e->model->mins, e->lighting->mins);
	VectorMA(e->lighting->origin, e->scale, e->model->maxs, e->lighting->maxs);

	//Com_Debug("%s\n", e->model->media.name);
	R_UpdateLighting(e->lighting);
}

/*
 * @brief
 */
static void R_SetMeshColor_default(const r_entity_t *e) {
	vec4_t color;
	vec_t v;
	int32_t i;

	VectorCopy(e->lighting->color, color);

	if (e->effects & EF_BLEND)
		color[3] = Clamp(e->alpha, 0.1, 1.0);
	else
		color[3] = 1.0;

	if (e->effects & EF_PULSE) {
		v = sin((r_view.time + e->model->num_verts) * 0.005) * 0.75;
		VectorScale(color, 1.0 + v, color);
	}

	v = 0.0;
	for (i = 0; i < 3; i++) { // find brightest component
		if (color[i] > v) // keep it
			v = color[i];
	}

	if (v > 1.0) // scale it back to 1.0
		VectorScale(color, 1.0 / v, color);

	R_Color(color);
}

/*
 * @brief Sets GL state to draw the specified entity.
 */
static void R_SetMeshState_default(const r_entity_t *e) {

	if (e->model->mesh->num_frames == 1) { // bind static arrays
		R_SetArrayState(e->model);
	} else { // or use the default arrays
		R_ResetArrayState();

		// but take advantage of static texture coordinate arrays
		if (texunit_diffuse.enabled) {
			R_BindArray(GL_TEXTURE_COORD_ARRAY, GL_FLOAT, e->model->texcoords);
		}
	}

	if (!r_draw_wireframe->value) {

		if (!(e->effects & EF_NO_DRAW)) { // setup state for diffuse render
			r_mesh_state.material = e->skins[0] ? e->skins[0] : e->model->mesh->material;

			R_BindTexture(r_mesh_state.material->diffuse->texnum);

			R_SetMeshColor_default(e);

			// hardware lighting
			if (r_state.lighting_enabled && !(e->effects & EF_NO_LIGHTING)) {

				R_UseMaterial(NULL, r_mesh_state.material);

				R_EnableLightsByRadius(e->origin);

				R_ApplyLighting(e->lighting);
#if 0
				if(e->effects & EF_WEAPON) {
					r_entity_t ent;
					int32_t i;

					for(i = 0; i < MAX_ACTIVE_LIGHTS; i++) {
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
		} else {
			R_UseMaterial(NULL, NULL);
		}
	} else {
		R_UseMaterial(NULL, NULL);
	}

	if (e->effects & EF_WEAPON) // prevent weapon from poking into walls
		glDepthRange(0.0, 0.3);

	// now rotate and translate to the ent's origin
	R_RotateForEntity(e);
}

/*
 * @brief
 */
static void R_ResetMeshState_default(const r_entity_t *e) {

	if (e->model->mesh->num_frames > 1) {
		if (texunit_diffuse.enabled) {
			R_BindDefaultArray(GL_TEXTURE_COORD_ARRAY);
		}
	}

	if (e->effects & EF_WEAPON)
		glDepthRange(0.0, 1.0);

	R_RotateForEntity(NULL);
}

#define MESH_SHADOW_SCALE 1.5
#define MESH_SHADOW_ALPHA 0.15

/*
 * @brief Applies translation, rotation and scale for the shadow of the specified
 * entity. In order to reuse the vertex arrays from the primary rendering
 * pass, the shadow origin must transformed into model-view space.
 */
static void R_RotateForMeshShadow_default(const r_entity_t *e) {
	vec3_t origin, offset;
	vec_t height, threshold, scale;

	if (!e) {
		glPopMatrix();
		return;
	}

	if (e->parent) {
		VectorSubtract(e->origin, e->parent->origin, offset);
		VectorAdd(e->lighting->shadow_origin, offset, offset);
		offset[2] = e->lighting->shadow_origin[2];
	} else {
		VectorCopy(e->lighting->shadow_origin, offset);
	}

	R_TransformForEntity(e, offset, origin);

	height = -origin[2];

	threshold = LIGHTING_MAX_SHADOW_DISTANCE / e->scale;

	scale = MESH_SHADOW_SCALE * (threshold - height) / threshold;

	glPushMatrix();

	glTranslatef(origin[0], origin[1], -height + 1.0);

	glRotatef(-e->angles[PITCH], 0.0, 1.0, 0.0);

	glScalef(scale, scale, 0.0);
}

/*
 * @brief Draws an animated, colored shell for the specified entity. Rather than
 * re-lerping or re-scaling the entity, the currently bound vertex arrays
 * are simply re-drawn using a small depth offset.
 */
static void R_DrawMeshShell_default(const r_entity_t *e) {
	vec4_t color;

	if (VectorCompare(e->shell, vec3_origin))
		return;

	VectorCopy(e->shell, color);
	color[3] = 1.0; // TODO: Might be cool to add a sin() here.

	R_Color(color);

	R_BindTexture(r_image_state.shell->texnum);

	R_EnableShell(true);

	glDrawArrays(GL_TRIANGLES, 0, e->model->num_verts);

	R_EnableShell(false);

	R_Color(NULL);
}

/*
 * @brief Re-draws the mesh using the stencil test. Meshes with stale lighting
 * information, or with a lighting point above our view, are not drawn.
 */
static void R_DrawMeshShadow_default(const r_entity_t *e) {
	const _Bool lighting = r_state.lighting_enabled;
	const _Bool blend = r_state.blend_enabled;

	const vec4_t color = { 0.0, 0.0, 0.0, r_shadows->value * MESH_SHADOW_ALPHA };

	if (!r_shadows->value)
		return;

	if (r_draw_wireframe->value)
		return;

	if (e->effects & EF_NO_SHADOW)
		return;

	if (e->effects & EF_BLEND)
		return;

	if (VectorCompare(e->lighting->shadow_origin, vec3_origin))
		return;

	if (e->lighting->shadow_origin[2] > r_view.origin[2])
		return;

	if (lighting)
		R_EnableLighting(NULL, false);

	R_EnableTexture(&texunit_diffuse, false);

	R_Color(color);

	if (!blend)
		R_EnableBlend(true);

	R_RotateForMeshShadow_default(e);

	glDepthRange(0.0, 0.999);

	R_EnableStencilTest(true);

	glDrawArrays(GL_TRIANGLES, 0, e->model->num_verts);

	R_EnableStencilTest(false);

	glDepthRange(0.0, 1.0);

	R_RotateForMeshShadow_default(NULL);

	if (!blend)
		R_EnableBlend(false);

	R_EnableTexture(&texunit_diffuse, true);

	R_Color(NULL);

	if (lighting)
		R_EnableLighting(r_state.default_program, true);
}

/*
 * @brief
 */
static void R_InterpolateMeshModel_default(const r_entity_t *e) {
	const r_md3_t *md3;
	const d_md3_frame_t *frame, *old_frame;
	const r_md3_mesh_t *mesh;
	vec3_t trans;
	int32_t vert_index;
	int32_t i, j;

	md3 = (r_md3_t *) e->model->mesh->data;

	frame = &md3->frames[e->frame];
	old_frame = &md3->frames[e->old_frame];

	vert_index = 0;

	for (i = 0; i < 3; i++) // calculate the translation
		trans[i] = e->back_lerp * old_frame->translate[i] + e->lerp * frame->translate[i];

	for (i = 0, mesh = md3->meshes; i < md3->num_meshes; i++, mesh++) { // iterate the meshes

		const r_md3_vertex_t *v = mesh->verts + e->frame * mesh->num_verts;
		const r_md3_vertex_t *ov = mesh->verts + e->old_frame * mesh->num_verts;

		const uint32_t *tri = mesh->tris;

		for (j = 0; j < mesh->num_verts; j++, v++, ov++) { // interpolate the vertexes
			VectorSet(r_mesh_state.vertexes[j],
					trans[0] + ov->point[0] * e->back_lerp + v->point[0] * e->lerp,
					trans[1] + ov->point[1] * e->back_lerp + v->point[1] * e->lerp,
					trans[2] + ov->point[2] * e->back_lerp + v->point[2] * e->lerp);

			if (r_state.lighting_enabled) { // and the normals
				VectorSet(r_mesh_state.normals[j],
						v->normal[0] + (ov->normal[0] - v->normal[0]) * e->back_lerp,
						v->normal[1] + (ov->normal[1] - v->normal[1]) * e->back_lerp,
						v->normal[2] + (ov->normal[2] - v->normal[2]) * e->back_lerp);
			}
		}

		for (j = 0; j < mesh->num_tris; j++, tri += 3) { // populate the triangles

			VectorCopy(r_mesh_state.vertexes[tri[0]], (&r_state.vertex_array_3d[vert_index + 0]));
			VectorCopy(r_mesh_state.vertexes[tri[1]], (&r_state.vertex_array_3d[vert_index + 3]));
			VectorCopy(r_mesh_state.vertexes[tri[2]], (&r_state.vertex_array_3d[vert_index + 6]));

			if (r_state.lighting_enabled) { // normal vectors for lighting
				VectorCopy(r_mesh_state.normals[tri[0]], (&r_state.normal_array[vert_index + 0]));
				VectorCopy(r_mesh_state.normals[tri[1]], (&r_state.normal_array[vert_index + 3]));
				VectorCopy(r_mesh_state.normals[tri[2]], (&r_state.normal_array[vert_index + 6]));
			}

			vert_index += 9;
		}
	}
}

/*
 * @brief Draw the diffuse pass of each mesh segment for the specified model.
 */
static void R_DrawMeshParts_default(const r_entity_t *e, const r_md3_t *md3) {
	r_md3_mesh_t *mesh = md3->meshes;
	int32_t i, offset = 0;

	for (i = 0; i < md3->num_meshes; i++, mesh++) {

		if (!r_draw_wireframe->value) {
			if (i > 0) { // update the diffuse state for the current mesh
				r_mesh_state.material = e->skins[i] ? e->skins[i] : e->model->mesh->material;

				R_BindTexture(r_mesh_state.material->diffuse->texnum);

				R_UseMaterial(NULL, r_mesh_state.material);
			}
		}

		glDrawArrays(GL_TRIANGLES, offset, mesh->num_tris * 3);

		R_DrawMeshMaterial(r_mesh_state.material, offset, mesh->num_tris * 3);

		offset += mesh->num_tris * 3;
	}
}

/*
 * @brief
 */
void R_DrawMeshModel_default(const r_entity_t *e) {

	if (e->frame >= e->model->mesh->num_frames) {
		Com_Warn("%s: no such frame %d\n", e->model->media.name, e->frame);
		return;
	}

	if (e->old_frame >= e->model->mesh->num_frames) {
		Com_Warn("%s: no such old_frame %d\n", e->model->media.name, e->old_frame);
		return;
	}

	R_SetMeshState_default(e);

	if (e->model->mesh->num_frames > 1) { // interpolate frames
		R_InterpolateMeshModel_default(e);
	}

	if (!(e->effects & EF_NO_DRAW)) { // draw the model

		if (e->model->type == MOD_MD3) {
			R_DrawMeshParts_default(e, (const r_md3_t *) e->model->mesh->data);
		} else {
			glDrawArrays(GL_TRIANGLES, 0, e->model->num_verts);

			R_DrawMeshMaterial(r_mesh_state.material, 0, e->model->num_verts);
		}

		R_DrawMeshShell_default(e); // draw any shell effects
	}

	R_DrawMeshShadow_default(e); // lastly draw the shadow

	R_ResetMeshState_default(e);

	r_view.num_mesh_models++;
	r_view.num_mesh_tris += e->model->num_verts / 3;
}
