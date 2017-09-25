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

#include "cg_local.h"

#include "PlayerModelView.h"

#define _Class _PlayerModelView

#pragma mark - Actions

/**
 * @brief ActionFunction for clicking the model's 3D view
 */
static void rotateAction(Control *control, const SDL_Event *event, ident sender, ident data) {

	PlayerModelView *this = (PlayerModelView *) sender;

	this->yaw += event->motion.xrel;

	if (this->yaw < 0) {
		this->yaw = 360;
	} else if (this->yaw > 360) {
		this->yaw = 0;
	}
}

/**
 * @brief ActionFunction for zooming the model's 3D view
 */
static void zoomAction(Control *control, const SDL_Event *event, ident sender, ident data) {

	PlayerModelView *this = (PlayerModelView *) sender;

	this->zoom += event->wheel.y * 0.2;
	this->zoom = Clamp(this->zoom, 0.0, 1.0);
}

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
 * @brief Renders the given entity stub.
 */
static void renderMeshEntity(r_entity_t *e) {

	cgi.view->current_entity = e;

	cgi.SetMatrixForEntity(e);

	cgi.DrawMeshModel(e);
}

/**
 * @brief Renders the given entity stub.
 */
static void renderMeshMaterialsEntity(r_entity_t *e) {

	cgi.view->current_entity = e;

	cgi.DrawMeshModelMaterials(e);
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

	cgi.view->ticks = cgi.client->unclamped_time;

	if (this->client.torso) {
		$(this, animate);

		cgi.PushMatrix(R_MATRIX_PROJECTION);
		cgi.PushMatrix(R_MATRIX_MODELVIEW);

		const SDL_Rect viewport = $(self, viewport);
		cgi.SetViewport(viewport.x, viewport.y, viewport.w, viewport.h, false);

		// create projection matrix
		const vec_t aspect = (vec_t) viewport.w / (vec_t) viewport.h;

		const vec_t ymax = NEAR_Z * tan(Radians(40)); // Field of view
		const vec_t ymin = -ymax;

		const vec_t xmin = ymin * aspect;
		const vec_t xmax = ymax * aspect;
		matrix4x4_t mat;

		Matrix4x4_FromFrustum(&mat, xmin, xmax, ymin, ymax, NEAR_Z, FAR_Z);

		cgi.SetMatrix(R_MATRIX_PROJECTION, &mat);

		// create base modelview matrix
		Matrix4x4_CreateIdentity(&mat);

		// Quake is retarded: rotate so that Z is up
		Matrix4x4_ConcatRotate(&mat, -90.0, 1.0, 0.0, 0.0);
		Matrix4x4_ConcatRotate(&mat,  90.0, 0.0, 0.0, 1.0);
		Matrix4x4_ConcatTranslate(&mat, 90.0 - (this->zoom * 45.0), 0.0, 20.0 - (this->zoom * 35.0));

		Matrix4x4_ConcatRotate(&mat, -25.0 - (this->zoom * -10.0), 0.0, 1.0, 0.0);

		Matrix4x4_ConcatRotate(&mat, sin(Radians(cgi.client->unclamped_time * 0.05)) * 10.0, 0.0, 0.0, 1.0);

		cgi.EnableDepthTest(true);
		cgi.DepthRange(0.0, 0.1);

		// Platform base; doesn't rotate

		cgi.SetMatrix(R_MATRIX_MODELVIEW, &mat);

		renderMeshEntity(&this->platformBase);
		renderMeshMaterialsEntity(&this->platformBase);

		// Rotating stuff

		Matrix4x4_ConcatRotate(&mat, this->yaw, 0.0, 0.0, 1.0);

		cgi.SetMatrix(R_MATRIX_MODELVIEW, &mat);

		renderMeshEntity(&this->legs);
		renderMeshEntity(&this->torso);
		renderMeshEntity(&this->head);
		renderMeshEntity(&this->weapon);
		renderMeshEntity(&this->platformCenter);

		renderMeshMaterialsEntity(&this->legs);
		renderMeshMaterialsEntity(&this->torso);
		renderMeshMaterialsEntity(&this->head);
		renderMeshMaterialsEntity(&this->weapon);
		renderMeshMaterialsEntity(&this->platformCenter);

		cgi.DepthRange(0.0, 1.0);
		cgi.EnableDepthTest(false);

		cgi.SetViewport(0, 0, cgi.context->width, cgi.context->height, false);

		cgi.PopMatrix(R_MATRIX_MODELVIEW);
		cgi.PopMatrix(R_MATRIX_PROJECTION);
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
	
	super(View, self, updateBindings);

	PlayerModelView *this = (PlayerModelView *) self;

	this->animation1.frame = this->animation2.frame = -1;

	char string[MAX_STRING_CHARS];
	g_snprintf(string, sizeof(string), "newbie\\%s\\%s\\%s\\%s\\0",
			   cg_skin->string, cg_shirt->string, cg_pants->string, cg_helmet->string);

	Cg_LoadClient(&this->client, string);

	this->legs.model = this->client.legs;
	this->legs.scale = 1.0;
	this->legs.effects = EF_NO_LIGHTING;
	Vector4Set(this->legs.color, 1.0, 1.0, 1.0, 1.0);

	this->torso.model = this->client.torso;
	this->torso.scale = 1.0;
	this->torso.effects = EF_NO_LIGHTING;
	Vector4Set(this->torso.color, 1.0, 1.0, 1.0, 1.0);

	this->head.model = this->client.head;
	this->head.scale = 1.0;
	this->head.effects = EF_NO_LIGHTING;
	Vector4Set(this->head.color, 1.0, 1.0, 1.0, 1.0);

	this->weapon.model = cgi.LoadModel("models/weapons/rocketlauncher/tris");
	this->weapon.scale = 1.0;
	this->weapon.effects = EF_NO_LIGHTING;
	Vector4Set(this->weapon.color, 1.0, 1.0, 1.0, 1.0);

	this->platformBase.model = cgi.LoadModel("models/objects/platform/base/tris");
	this->platformBase.scale = 1.0;
	this->platformBase.effects = EF_NO_LIGHTING;
	Vector4Set(this->platformBase.color, 1.0, 1.0, 1.0, 1.0);

	this->platformCenter.model = cgi.LoadModel("models/objects/platform/center/tris");
	this->platformCenter.scale = 1.0;
	this->platformCenter.effects = EF_NO_LIGHTING;
	Vector4Set(this->platformCenter.color, 1.0, 1.0, 1.0, 1.0);

	memcpy(this->legs.skins, this->client.legs_skins, sizeof(this->legs.skins));
	memcpy(this->torso.skins, this->client.torso_skins, sizeof(this->torso.skins));
	memcpy(this->head.skins, this->client.head_skins, sizeof(this->head.skins));

	this->iconView->texture = this->client.icon->texnum;
}

#pragma mark - Control

/**
 * @see Control::captureEvent(Control *, const SDL_Event *)
 */
static _Bool captureEvent(Control *self, const SDL_Event *event) {

	if (event->type == SDL_MOUSEMOTION && event->motion.state & SDL_BUTTON_LMASK) {

		if ($((View *) self, didReceiveEvent, event)) {
			return true;
		}
	} else if (event->type == SDL_MOUSEWHEEL) {

		if ($((View *) self, didReceiveEvent, event)) {
			return true;
		}
	}

	return super(Control, self, captureEvent, event);
}

#pragma mark - PlayerModelView

/**
 * @brief Runs the animation, proceeding to the next in the sequence upon completion.
 */
static void animate_(const r_mesh_model_t *model, cl_entity_animation_t *a, r_entity_t *e) {

	e->frame = e->old_frame = 0;
	e->lerp = 1.0;
	e->back_lerp = 0.0;

	const r_model_animation_t *anim = &model->animations[a->animation];

	const uint32_t frameTime = 1500 / anim->hz;
	const uint32_t animationTime = anim->num_frames * frameTime;
	const uint32_t elapsedTime = cgi.client->unclamped_time - a->time;
	uint16_t frame = elapsedTime / frameTime;

	if (elapsedTime >= animationTime) {

		a->time = cgi.client->unclamped_time;

		animate_(model, a, e);
		return;
	}

	frame = anim->first_frame + frame;

	if (frame != a->frame) {
		if (a->frame == -1) {
			a->old_frame = frame;
			a->frame = frame;
		} else {
			a->old_frame = a->frame;
			a->frame = frame;
		}
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

	const r_mesh_model_t *model = self->torso.model->mesh;

	animate_(model, &self->animation1, &self->torso);
	animate_(model, &self->animation2, &self->legs);

	self->head.frame = 0;
	self->head.lerp = 1.0;

	self->weapon.frame = 0;
	self->weapon.lerp = 1.0;

	VectorClear(self->legs.origin);
	VectorClear(self->torso.origin);
	VectorClear(self->head.origin);
	VectorClear(self->weapon.origin);
	VectorClear(self->platformBase.origin);
	VectorClear(self->platformCenter.origin);

	VectorClear(self->legs.angles);
	VectorClear(self->torso.angles);
	VectorClear(self->head.angles);
	VectorClear(self->weapon.angles);
	VectorClear(self->platformBase.angles);
	VectorClear(self->platformCenter.angles);

	self->legs.scale = self->torso.scale = self->head.scale = 1.0;
	self->weapon.scale = 1.0;
	self->platformBase.scale = self->platformCenter.scale = 1.0;

	if (self->client.shirt.a) {
		ColorToVec4(self->client.shirt, self->torso.tints[0]);
	}

	if (self->client.pants.a) {
		ColorToVec4(self->client.pants, self->legs.tints[1]);
	}

	if (self->client.helmet.a) {
		ColorToVec4(self->client.helmet, self->head.tints[2]);
	}

	Matrix4x4_CreateFromEntity(&self->legs.matrix, self->legs.origin, self->legs.angles, self->legs.scale);
	Matrix4x4_CreateFromEntity(&self->torso.matrix, self->torso.origin, self->torso.angles, self->torso.scale);
	Matrix4x4_CreateFromEntity(&self->head.matrix, self->head.origin, self->head.angles, self->head.scale);
	Matrix4x4_CreateFromEntity(&self->weapon.matrix, self->weapon.origin, self->weapon.angles, self->weapon.scale);
	Matrix4x4_CreateFromEntity(&self->platformBase.matrix, self->platformBase.origin, self->platformBase.angles, self->platformBase.scale);
	Matrix4x4_CreateFromEntity(&self->platformCenter.matrix, self->platformCenter.origin, self->platformCenter.angles, self->platformCenter.scale);

	Cg_ApplyMeshModelTag(&self->torso, &self->legs, "tag_torso");
	Cg_ApplyMeshModelTag(&self->head, &self->torso, "tag_head");
	Cg_ApplyMeshModelTag(&self->weapon, &self->torso, "tag_weapon");
}

/**
 * @fn PlayerModelView *PlayerModelView::init(PlayerModelView *self)
 *
 * @memberof PlayerModelView
 */
static PlayerModelView *initWithFrame(PlayerModelView *self, const SDL_Rect *frame, ControlStyle style) {

	self = (PlayerModelView *) super(Control, self, initWithFrame, frame, style);
	if (self) {
		self->yaw = 150.0;
		self->zoom = 0.4;

		self->animation1.animation = ANIM_TORSO_STAND1;
		self->animation2.animation = ANIM_LEGS_IDLE;

		const SDL_Rect iconFrame = MakeRect(0, 0, 64, 64);

		self->iconView = $(alloc(ImageView), initWithFrame, &iconFrame);
		assert(self->iconView);

		self->iconView->view.alignment = ViewAlignmentTopRight;

		$((View *) self, addSubview, (View *) self->iconView);

		self->control.view.padding.top = 4;
		self->control.view.padding.right = 4;
		self->control.view.padding.bottom = 4;
		self->control.view.padding.left = 4;

		$((Control *) self, addActionForEventType, SDL_MOUSEMOTION, rotateAction, self, NULL);
		$((Control *) self, addActionForEventType, SDL_MOUSEWHEEL, zoomAction, self, NULL);
	}

	return self;
}

#pragma mark - Class lifecycle

/**
 * @see Class::initialize(Class *)
 */
static void initialize(Class *clazz) {

	((ObjectInterface *) clazz->def->interface)->dealloc = dealloc;

	((ViewInterface *) clazz->def->interface)->render = render;
	((ViewInterface *) clazz->def->interface)->renderDeviceDidReset = renderDeviceDidReset;
	((ViewInterface *) clazz->def->interface)->updateBindings = updateBindings;

	((ControlInterface *) clazz->def->interface)->captureEvent = captureEvent;

	((PlayerModelViewInterface *) clazz->def->interface)->animate = animate;
	((PlayerModelViewInterface *) clazz->def->interface)->initWithFrame = initWithFrame;
}

/**
 * @fn Class *PlayerModelView::_PlayerModelView(void)
 * @memberof PlayerModelView
 */
Class *_PlayerModelView(void) {
	static Class clazz;
	static Once once;

	do_once(&once, {
		clazz.name = "PlayerModelView";
		clazz.superclass = _Control();
		clazz.instanceSize = sizeof(PlayerModelView);
		clazz.interfaceOffset = offsetof(PlayerModelView, interface);
		clazz.interfaceSize = sizeof(PlayerModelViewInterface);
		clazz.initialize = initialize;
	});

	return &clazz;
}

#undef _Class
