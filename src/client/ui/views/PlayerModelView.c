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

#include "PlayerModelView.h"

extern cl_static_t cls;
extern cl_client_t cl;

#define _Class _PlayerModelView

#pragma mark - Object

/**
 * @see Object::dealloc(Object *)
 */
static void dealloc(Object *self) {

	PlayerModelView *this = (PlayerModelView *) self;

	release(this->iconView);

	super(Object, self, dealloc);
}

#pragma mark - View

/**
 * @brief Renders the given entity's OBJ model.
 */
static void renderMeshEntity_obj(const r_entity_t *e) {

	glBindTexture(GL_TEXTURE_2D, e->model->mesh->material->diffuse->texnum);

	glTexCoordPointer(2, GL_FLOAT, 0, e->model->texcoords);
	glVertexPointer(3, GL_FLOAT, 0, e->model->verts);

	glDrawArrays(GL_TRIANGLES, 0, e->model->num_verts);
}

/**
 * @brief Renders the given entity's MD3 model.
 */
static void renderMeshEntity_md3(const r_entity_t *e) {

	vec3_t verts[e->model->num_verts];
	glVertexPointer(3, GL_FLOAT, 0, verts);

	const r_md3_t *md3 = (r_md3_t *) e->model->mesh->data;

	const d_md3_frame_t *frame = &md3->frames[e->frame];
	const d_md3_frame_t *oldFrame = &md3->frames[e->old_frame];

	vec3_t translate;
	VectorLerp(oldFrame->translate, frame->translate, e->back_lerp, translate);

	const r_md3_mesh_t *mesh = md3->meshes;
	for (uint16_t i = 0; i < md3->num_meshes; i++, mesh++) {

		const r_material_t *material = e->skins[i] ?: e->model->mesh->material;
		glBindTexture(GL_TEXTURE_2D, material->diffuse->texnum);

		glTexCoordPointer(2, GL_FLOAT, 0, mesh->coords);

		const r_md3_vertex_t *v = mesh->verts + e->frame * mesh->num_verts;
		const r_md3_vertex_t *ov = mesh->verts + e->old_frame * mesh->num_verts;

		for (uint16_t j = 0; j < mesh->num_verts; j++, v++, ov++) {
			VectorSet(verts[j],
					  translate[0] + ov->point[0] * e->back_lerp + v->point[0] * e->lerp,
					  translate[1] + ov->point[1] * e->back_lerp + v->point[1] * e->lerp,
					  translate[2] + ov->point[2] * e->back_lerp + v->point[2] * e->lerp);
		}

		glDrawElements(GL_TRIANGLES, mesh->num_tris * 3, GL_UNSIGNED_INT, mesh->tris);
	}
}

/**
 * @brief Renders the given entity stub.
 */
static void renderMeshEntity(r_entity_t *e) {
	extern void R_SetMatrixForEntity(r_entity_t *e);

	R_SetMatrixForEntity(e);

	glPushMatrix();
	
	glMultMatrixf((GLfloat *) e->matrix.m);

	switch (e->model->type) {
		case MOD_OBJ:
			renderMeshEntity_obj(e);
			break;
		case MOD_MD3:
			renderMeshEntity_md3(e);
			break;
		default:
			break;
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

	PlayerModelView *this = (PlayerModelView *) self;

	if (this->client.torso == NULL) {
		$(self, updateBindings);
	}

	if (this->client.torso) {
		$(this, animate);

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

		glRotatef(cl.systime * 0.08, 0.0, 0.0, 1.0);

		glDepthRange(0.0, 0.1);
		glEnable(GL_DEPTH_TEST);
		glEnable(GL_TEXTURE_2D);

		renderMeshEntity(&this->legs);
		renderMeshEntity(&this->torso);
		renderMeshEntity(&this->head);
		renderMeshEntity(&this->weapon);

		glDisable(GL_TEXTURE_2D);
		glDisable(GL_DEPTH_TEST);
		glDepthRange(0.0, 1.0);

		glPopMatrix();

		glViewport(0, 0, r_context.width, r_context.height);

		glMatrixMode(GL_PROJECTION);
		glLoadIdentity();

		glOrtho(0, r_context.window_width, r_context.window_height, 0, -1, 1);

		glMatrixMode(GL_MODELVIEW);
		glLoadIdentity();
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
	extern cvar_t *name;
	extern cvar_t *skin;

	super(View, self, updateBindings);

	PlayerModelView *this = (PlayerModelView *) self;

	char string[MAX_STRING_CHARS];
	g_snprintf(string, sizeof(string), "%s\\%s", name->string, skin->string);

	cls.cgame->LoadClient(&this->client, string);

	this->legs.model = this->client.legs;
	this->legs.scale = 1.0;

	this->torso.model = this->client.torso;
	this->torso.parent = &this->legs;
	this->torso.scale = 1.0;
	this->torso.tag_name = "tag_torso";

	this->head.model = this->client.head;
	this->head.parent = &this->torso;
	this->head.scale = 1.0;
	this->head.tag_name = "tag_head";

	this->weapon.model = R_LoadModel("models/weapons/rocketlauncher/tris");
	this->weapon.parent = &this->torso;
	this->weapon.scale = 1.0;
	this->weapon.tag_name = "tag_weapon";

	memcpy(this->legs.skins, this->client.legs_skins, sizeof(this->legs.skins));
	memcpy(this->torso.skins, this->client.torso_skins, sizeof(this->torso.skins));
	memcpy(this->head.skins, this->client.head_skins, sizeof(this->head.skins));

	this->iconView->texture = this->client.icon->texnum;
}

#pragma mark - PlayerModelView

/**
 * @brief Runs the animation, proceeding to the next in the sequence upon completion.
 */
static void animate_(const r_md3_t *md3, cl_entity_animation_t *a, r_entity_t *e) {

	e->frame = e->old_frame = 0;
	e->lerp = 1.0;
	e->back_lerp = 0.0;

	const r_md3_animation_t *anim = &md3->animations[a->animation];

	const uint32_t frameTime = (1500 / time_scale->value) / anim->hz;
	const uint32_t animationTime = anim->num_frames * frameTime;
	const uint32_t elapsedTime = cl.systime - a->time;
	uint16_t frame = elapsedTime / frameTime;

	if (elapsedTime >= animationTime) {

		if (a->animation == ANIM_TORSO_STAND2) {
			a->animation = ANIM_TORSO_GESTURE;
		} else if (a->animation == ANIM_LEGS_IDLECR) {
			a->animation = ANIM_LEGS_WALKCR;
		} else {
			a->animation++;
		}

		a->time = cl.systime;

		animate_(md3, a, e);
		return;
	}

	frame = anim->first_frame + frame;

	if (frame != a->frame) { // shuffle the frames
		a->old_frame = a->frame;
		a->frame = frame;
	}

	a->lerp = (elapsedTime % frameTime) / (vec_t) frameTime;
	a->fraction = elapsedTime / (vec_t) animationTime;

	e->frame = a->frame;
	e->old_frame = a->old_frame;
	e->lerp = a->lerp;
	e->back_lerp = 1.0 - a->lerp;
}

/**
 * @fn void PlayerModelView::animate(PlayerModelView *self)
 *
 * @memberof PlayerModelView
 */
static void animate(PlayerModelView *self) {

	const r_md3_t *md3 = (r_md3_t *) self->torso.model->mesh->data;

	animate_(md3, &self->animation1, &self->torso);
	animate_(md3, &self->animation2, &self->legs);

	self->head.frame = 0;
	self->head.lerp = 1.0;

	self->weapon.frame = 0;
	self->weapon.lerp = 1.0;
}

/**
 * @fn PlayerModelView *PlayerModelView::init(PlayerModelView *self)
 *
 * @memberof PlayerModelView
 */
static PlayerModelView *initWithFrame(PlayerModelView *self, const SDL_Rect *frame) {

	self = (PlayerModelView *) super(View, self, initWithFrame, frame);
	if (self) {
		self->view.backgroundColor = Colors.Charcoal;
		self->view.backgroundColor.a = 64;

		self->animation1.animation = ANIM_TORSO_STAND1;
		self->animation2.animation = ANIM_LEGS_RUN;

		const SDL_Rect iconFrame = MakeRect(0, 0, 64, 64);

		self->iconView = alloc(ImageView, initWithFrame, &iconFrame);
		assert(self->iconView);

		self->iconView->view.alignment = ViewAlignmentTopRight;

		$((View *) self, addSubview, (View *) self->iconView);

		self->view.padding.top = 4;
		self->view.padding.right = 4;
		self->view.padding.bottom = 4;
		self->view.padding.left = 4;
	}

	return self;
}

#pragma mark - Class lifecycle

/**
 * @see Class::initialize(Class *)
 */
static void initialize(Class *clazz) {

	((ObjectInterface *) clazz->interface)->dealloc = dealloc;

	((ViewInterface *) clazz->interface)->render = render;
	((ViewInterface *) clazz->interface)->renderDeviceDidReset = renderDeviceDidReset;
	((ViewInterface *) clazz->interface)->updateBindings = updateBindings;

	((PlayerModelViewInterface *) clazz->interface)->animate = animate;
	((PlayerModelViewInterface *) clazz->interface)->initWithFrame = initWithFrame;
}

Class _PlayerModelView = {
	.name = "PlayerModelView",
	.superclass = &_View,
	.instanceSize = sizeof(PlayerModelView),
	.interfaceOffset = offsetof(PlayerModelView, interface),
	.interfaceSize = sizeof(PlayerModelViewInterface),
	.initialize = initialize,
};

#undef _Class

