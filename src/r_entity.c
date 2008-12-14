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

static entity_t *r_bsp_entities;

static entity_t *r_opaque_mesh_entities;
static entity_t *r_alpha_test_mesh_entities;
static entity_t *r_blend_mesh_entities;

static entity_t *r_null_entities;


/*
R_AddEntity
*/
void R_AddEntity(const entity_t *ent){
	entity_t *e, *in, **chain;

	if(r_view.num_entities >= MAX_ENTITIES){
		Com_Warn("R_AddEntity: MAX_ENTITIES reached.\n");
		return;
	}

	e = &r_view.entities[r_view.num_entities++];
	*e = *ent;  // copy it in

	if(!e->model){  // null model chain
		e->next = r_null_entities;
		r_null_entities = e;
		return;
	}

	// frustum cull, appending those which pass to a sorted list
	if(e->model->type == mod_bsp_submodel){

		if(R_CullBspModel(e)){
			r_view.num_entities--;
			return;
		}

		chain = &r_bsp_entities;
	}
	else {  // mod_mesh
		R_ApplyMeshModelConfig(e);  // apply mesh config

		if(R_CullMeshModel(e)){
			r_view.num_entities--;
			return;
		}

		if(e->flags & EF_ALPHATEST)
			chain = &r_alpha_test_mesh_entities;
		else if(e->flags & EF_BLEND)
			chain = &r_blend_mesh_entities;
		else
			chain = &r_opaque_mesh_entities;
	}

	in = *chain;

	// insert into the sorted chain
	while(in){

		if(in->model == e->model){
			e->next = in->next;
			in->next = e;
			return;
		}

		in = in->next;
	}

	// or simply push to the head of the chain
	e->next = *chain;
	*chain = e;
}


/*
R_RotateForEntity
*/
void R_RotateForEntity(const entity_t *e, qboolean full){

	glTranslatef(e->origin[0], e->origin[1], e->origin[2]);

	glScalef(e->scale[0], e->scale[1], e->scale[2]);

	glRotatef(e->angles[YAW], 0.0, 0.0, 1.0);

	if(!full)
		return;

	glRotatef(e->angles[PITCH], 0.0, 1.0, 0.0);
	glRotatef(e->angles[ROLL], 1.0, 0.0, 0.0);
}


/*
R_DrawBspEntities
*/
static void R_DrawBspEntities(const entity_t *ents){
	const entity_t *e;

	if(!ents)
		return;

	e = ents;

	while(e){
		R_DrawBspModel(e);
		e = e->next;
	}
}


/*
R_DrawMeshEntities
*/
static void R_DrawMeshEntities(entity_t *ents){
	entity_t *e;

	e = ents;

	while(e){
		R_DrawMeshModel(e);
		e = e->next;
	}
}


/*
R_DrawOpaqueMeshEntities
*/
static void R_DrawOpaqueMeshEntities(entity_t *ents){

	if(!ents)
		return;

	R_EnableLighting(r_state.default_program, true);

	R_DrawMeshEntities(ents);

	R_EnableLighting(NULL, false);
}


/*
R_DrawAlphaTestMeshEntities
*/
static void R_DrawAlphaTestMeshEntities(entity_t *ents){

	if(!ents)
		return;

	R_EnableAlphaTest(true);

	R_EnableLighting(r_state.default_program, true);
	
	R_DrawMeshEntities(ents);

	R_EnableLighting(NULL, false);

	R_EnableAlphaTest(false);
}


/*
R_DrawBlendMeshEntities
*/
static void R_DrawBlendMeshEntities(entity_t *ents){

	if(!ents)
		return;

	R_EnableBlend(true);

	R_DrawMeshEntities(ents);

	R_EnableBlend(false);
}


/*
R_DrawNullModel
*/
static void R_DrawNullModel(const entity_t *e){
	int i;

	glPushMatrix();
	R_RotateForEntity(e, true);

	glDisable(GL_TEXTURE_2D);

	glBegin(GL_TRIANGLE_FAN);
	glVertex3f(0, 0, -16);
	for(i = 0; i <= 4; i++)
		glVertex3f(16 * cos(i * M_PI / 2), 16 * sin(i * M_PI / 2), 0);
	glEnd();

	glBegin(GL_TRIANGLE_FAN);
	glVertex3f(0, 0, 16);
	for(i = 4; i >= 0; i--)
		glVertex3f(16 * cos(i * M_PI / 2), 16 * sin(i * M_PI / 2), 0);
	glEnd();

	glPopMatrix();
	glEnable(GL_TEXTURE_2D);
}


/*
R_DrawNullEntities
*/
static void R_DrawNullEntities(const entity_t *ents){
	const entity_t *e;

	if(!ents)
		return;

	e = ents;

	while(e){
		R_DrawNullModel(e);
		e = e->next;
	}
}


/*
R_DrawEntities
*/
void R_DrawEntities(void){

	R_DrawBspEntities(r_bsp_entities);

	R_DrawOpaqueMeshEntities(r_opaque_mesh_entities);

	R_DrawAlphaTestMeshEntities(r_alpha_test_mesh_entities);

	R_DrawBlendMeshEntities(r_blend_mesh_entities);

	glColor4ubv(color_white);

	R_DrawNullEntities(r_null_entities);

	// clear chains for next frame
	r_bsp_entities = r_opaque_mesh_entities = r_alpha_test_mesh_entities = 
		r_blend_mesh_entities = r_null_entities = NULL;
}
