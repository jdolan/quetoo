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

		const vec_t ymax = NEAR_Z * tan(Radians(35));
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
		Matrix4x4_ConcatTranslate(&mat, 64.0, 0.0, -8.0);

		Matrix4x4_ConcatRotate(&mat, cgi.client->unclamped_time * 0.08, 0.0, 0.0, 1.0);

		cgi.SetMatrix(R_MATRIX_MODELVIEW, &mat);

		cgi.EnableDepthTest(true);
		cgi.DepthRange(0.0, 0.1);

		renderMeshEntity(&this->legs);
		renderMeshEntity(&this->torso);
		renderMeshEntity(&this->head);
		renderMeshEntity(&this->weapon);

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

	char string[MAX_STRING_CHARS];
	g_snprintf(string, sizeof(string), "newbie\\%s", cg_skin->string);

	Cg_LoadClient(&this->client, string);

	this->legs.model = this->client.legs;
	this->legs.scale = 1.0;
	this->legs.effects = EF_NO_LIGHTING;
	Vector4Set(this->legs.color, 1.0, 1.0, 1.0, 1.0);

	this->torso.model = this->client.torso;
	this->torso.parent = &this->legs;
	this->torso.scale = 1.0;
	this->torso.tag_name = "tag_torso";
	this->torso.effects = EF_NO_LIGHTING;
	Vector4Set(this->torso.color, 1.0, 1.0, 1.0, 1.0);

	this->head.model = this->client.head;
	this->head.parent = &this->torso;
	this->head.scale = 1.0;
	this->head.tag_name = "tag_head";
	this->head.effects = EF_NO_LIGHTING;
	Vector4Set(this->head.color, 1.0, 1.0, 1.0, 1.0);

	this->weapon.model = cgi.LoadModel("models/weapons/rocketlauncher/tris");
	this->weapon.parent = &this->torso;
	this->weapon.scale = 1.0;
	this->weapon.tag_name = "tag_weapon";
	this->weapon.effects = EF_NO_LIGHTING;
	Vector4Set(this->weapon.color, 1.0, 1.0, 1.0, 1.0);

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

	const uint32_t frameTime = 1500 / anim->hz;
	const uint32_t animationTime = anim->num_frames * frameTime;
	const uint32_t elapsedTime = cgi.client->unclamped_time - a->time;
	uint16_t frame = elapsedTime / frameTime;

	if (elapsedTime >= animationTime) {

		if (a->animation == ANIM_TORSO_STAND2) {
			a->animation = ANIM_TORSO_GESTURE;
		} else if (a->animation == ANIM_LEGS_IDLECR) {
			a->animation = ANIM_LEGS_WALKCR;
		} else {
			a->animation++;
		}

		a->time = cgi.client->unclamped_time;

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

		self->iconView = $(alloc(ImageView), initWithFrame, &iconFrame);
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

	((ObjectInterface *) clazz->def->interface)->dealloc = dealloc;

	((ViewInterface *) clazz->def->interface)->render = render;
	((ViewInterface *) clazz->def->interface)->renderDeviceDidReset = renderDeviceDidReset;
	((ViewInterface *) clazz->def->interface)->updateBindings = updateBindings;

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
		clazz.superclass = _View();
		clazz.instanceSize = sizeof(PlayerModelView);
		clazz.interfaceOffset = offsetof(PlayerModelView, interface);
		clazz.interfaceSize = sizeof(PlayerModelViewInterface);
		clazz.initialize = initialize;
	});

	return &clazz;
}

#undef _Class

