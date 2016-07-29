/*
 * Copyright(c) 1997-2001 id Software, Inc.
 * Copyright(c) 2002 The Quakeforge Project.
 * Copyright(c) 2006 Quetoo.
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

#include <assert.h>

#include "MeshModelView.h"

#define _Class _MeshModelView

#pragma mark - View

/**
 * @brief Renders the given entity stub.
 */
static void renderModel(r_entity_t *e) {
	extern void R_SetMatrixForEntity(r_entity_t *e);

	R_SetMatrixForEntity(e);

	glPushMatrix();
	
	glMultMatrixf((GLfloat *) e->matrix.m);

	vec3_t verts[e->model->num_verts];
	glVertexPointer(3, GL_FLOAT, 0, verts);

	const r_md3_t *md3 = (r_md3_t *) e->model->mesh->data;
	const r_md3_mesh_t *mesh = md3->meshes;
	for (uint16_t i = 0; i < md3->num_meshes; i++, mesh++) {

		const r_material_t *material = e->skins[i] ?: e->model->mesh->material;
		glBindTexture(GL_TEXTURE_2D, material->diffuse->texnum);

		const r_md3_vertex_t *v = mesh->verts + e->frame * mesh->num_verts;

		glTexCoordPointer(2, GL_FLOAT, 0, mesh->coords);

		for (uint16_t j = 0; j < mesh->num_verts; j++, v++) {
			VectorAdd(v->point, md3->frames[e->frame].translate, verts[j]);
		}

		glDrawElements(GL_TRIANGLES, mesh->num_tris * 3, GL_UNSIGNED_INT, mesh->tris);
	}

	glPopMatrix();
}

#define NEAR_Z 1.0
#define FAR_Z  MAX_WORLD_COORD

/**
 * @see View::render(View *, Renderer *)
 */
static void render(View *self, Renderer *renderer) {

	super(View, self, render, renderer);

	if (cvar_user_info_modified) {
		$(self, updateBindings);
	}

	MeshModelView *this = (MeshModelView *) self;
	if (this->client.upper) {

		const SDL_Rect viewport = $(self, viewport);
		glViewport(viewport.x, viewport.y, viewport.w, viewport.h);

		glMatrixMode(GL_PROJECTION);
		glLoadIdentity();

		const vec_t aspect = (vec_t) viewport.w / (vec_t) viewport.h;

		const vec_t ymax = NEAR_Z * tan(Radians(35));
		const vec_t ymin = -ymax;

		const vec_t xmin = ymin * aspect;
		const vec_t xmax = ymax * aspect;

		glFrustum(xmin, xmax, ymin, ymax, NEAR_Z, FAR_Z);

		glMatrixMode(GL_MODELVIEW);
		glLoadIdentity();

		// Quake is retarded: rotate so that Z is up
		glRotatef(-90.0, 1.0, 0.0, 0.0);
		glRotatef( 90.0, 0.0, 0.0, 1.0);
		glTranslatef(64.0, 0.0, -8.0);
		
		glPushMatrix();

		glRotatef(quetoo.time * 0.08, 0.0, 0.0, 1.0);

		glEnable(GL_DEPTH_TEST);
		glEnable(GL_TEXTURE_2D);

		$(this, animate);

		renderModel(&this->lower);
		renderModel(&this->upper);
		renderModel(&this->head);

		glDisable(GL_TEXTURE_2D);
		glDisable(GL_DEPTH_TEST);

		glPopMatrix();

		glViewport(0, 0, r_context.window_width, r_context.window_height);
	}
}

/**
 * @see View::renderDeviceDidReset(View *)
 */
static void renderDeviceDidReset(View *self) {

	super(View, self, renderDeviceDidReset);

	$(self, updateBindings);
}

/**
 * @see View::updateBindings(View *)
 */
static void updateBindings(View *self) {
	extern cl_static_t cls;
	extern cvar_t *name;
	extern cvar_t *skin;

	super(View, self, updateBindings);

	MeshModelView *this = (MeshModelView *) self;

	char string[MAX_STRING_CHARS];
	g_snprintf(string, sizeof(string), "%s\\%s", name->string, skin->string);

	cls.cgame->LoadClient(&this->client, string);

	this->lower.model = this->client.lower;
	this->lower.scale = 1.0;

	this->upper.model = this->client.upper;
	this->upper.parent = &this->lower;
	this->upper.scale = 1.0;
	this->upper.tag_name = "tag_torso";

	this->head.model = this->client.head;
	this->head.parent = &this->upper;
	this->head.scale = 1.0;
	this->head.tag_name = "tag_head";

	memcpy(this->lower.skins, this->client.lower_skins, sizeof(this->lower.skins));
	memcpy(this->upper.skins, this->client.upper_skins, sizeof(this->upper.skins));
	memcpy(this->head.skins, this->client.head_skins, sizeof(this->head.skins));
}

#pragma mark - MeshModelView

/**
 * @fn void MeshModelView::animate(MeshModelView *self)
 *
 * @memberof MeshModelView
 */
static void animate(MeshModelView *self) {

	const r_md3_t *md3 = (r_md3_t *) self->upper.model->mesh->data;

	const r_md3_animation_t *lower = &md3->animations[ANIM_LEGS_RUN];
	self->lower.frame = lower->first_frame;

	const r_md3_animation_t *upper = &md3->animations[ANIM_TORSO_ATTACK1];
	self->upper.frame = upper->first_frame;

	self->head.frame = 0;
}

/**
 * @fn MeshModelView *MeshModelView::init(MeshModelView *self)
 *
 * @memberof MeshModelView
 */
static MeshModelView *initWithFrame(MeshModelView *self, const SDL_Rect *frame) {

	self = (MeshModelView *) super(View, self, initWithFrame, frame);
	if (self) {
		self->view.backgroundColor = Colors.Charcoal;
		self->view.backgroundColor.a = 128;
	}

	return self;
}

#pragma mark - Class lifecycle

/**
 * @see Class::initialize(Class *)
 */
static void initialize(Class *clazz) {

	((ViewInterface *) clazz->interface)->render = render;
	((ViewInterface *) clazz->interface)->renderDeviceDidReset = renderDeviceDidReset;
	((ViewInterface *) clazz->interface)->updateBindings = updateBindings;

	((MeshModelViewInterface *) clazz->interface)->animate = animate;
	((MeshModelViewInterface *) clazz->interface)->initWithFrame = initWithFrame;
}

Class _MeshModelView = {
	.name = "MeshModelView",
	.superclass = &_View,
	.instanceSize = sizeof(MeshModelView),
	.interfaceOffset = offsetof(MeshModelView, interface),
	.interfaceSize = sizeof(MeshModelViewInterface),
	.initialize = initialize,
};

#undef _Class

