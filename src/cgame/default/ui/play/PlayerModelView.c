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
}

/**
 * @brief ActionFunction for zooming the model's 3D view
 */
static void zoomAction(Control *control, const SDL_Event *event, ident sender, ident data) {

	PlayerModelView *this = (PlayerModelView *) sender;

	this->zoom = Clampf(this->zoom + event->wheel.y * 0.0125f, 0.0f, 1.0f);
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
 * @brief
 */
static View *init(View *self) {
	return (View *) $((PlayerModelView *) self, initWithFrame, NULL);
}

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

		memset(&this->view, 0, sizeof(this->view));

		this->view.type = VIEW_PLAYER_MODEL;
		this->view.ticks = cgi.client->unclamped_time;
		this->view.fov = Vec2(45.f, 30.f);
		this->view.origin = Vec3(0.f, -100.f, 0.f);
		this->view.angles = Vec3(0.f, 0.f, 0.f);
		this->view.forward = Vec3(0.f, 1.f, 0.f);
		this->view.right = Vec3(1.f, 0.f, 0.f);
		this->view.up = Vec3(0.f, 0.f, 1.f);

		const SDL_Rect viewport = $(self, viewport);
		this->view.viewport = Vec4(viewport.x, viewport.y, viewport.w, viewport.h);

		if (this->framebuffer.name == 0) {
			this->framebuffer = cgi.CreateFramebuffer(viewport.w, viewport.h);
		}

		this->view.framebuffer = &this->framebuffer;

		cgi.AddEntity(&this->view, &this->platformBase);
		cgi.AddEntity(&this->view, &this->platformCenter);

		this->torso.parent = cgi.AddEntity(&this->view, &this->legs);
		r_entity_t *torso = cgi.AddEntity(&this->view, &this->torso);

		this->head.parent = torso;
		cgi.AddEntity(&this->view, &this->head);

		this->weapon.parent = torso;
		cgi.AddEntity(&this->view, &this->weapon);

		const SDL_Rect viewport = $(self, viewport);
		this->view.viewport = Vec4(viewport.x, viewport.y, viewport.w, viewport.h);

		cgi.DrawPlayerModelView(&this->view);

		const SDL_Rect renderFrame = $(self, renderFrame);
		cgi.Draw2DFramebuffer(renderFrame.x, renderFrame.y, renderFrame.w, renderFrame.h, &this->framebuffer, color_white);
	}
}

/**
 * @see View::renderDeviceDidReset(View *)
 */
static void renderDeviceDidReset(View *self) {

	super(View, self, renderDeviceDidReset);

	PlayerModelView *this = (PlayerModelView *) self;

	this->info[0] = '\0';
}

/**
 * @see View::renderDeviceWillReset(View *)
 */
static void renderDeviceWillReset(View *self) {

	super(View, self, renderDeviceWillReset);

	PlayerModelView *this = (PlayerModelView *) self;

	cgi.DestroyFramebuffer(&this->framebuffer);
}

/**
 * @see View::updateBindings(View *)
 */
static void updateBindings(View *self) {
	
	super(View, self, updateBindings);

	PlayerModelView *this = (PlayerModelView *) self;

	this->animation1.frame = this->animation2.frame = -1;

	char info[MAX_STRING_CHARS];
	g_snprintf(info, sizeof(info), "newbie\\%s\\%s\\%s\\%s\\0",
			   cg_skin->string, cg_shirt->string, cg_pants->string, cg_helmet->string);

	if (strcmp(this->info, info) == 0) {
		return;
	}

	g_strlcpy(this->info, info, sizeof(this->info));
	Cg_LoadClient(&this->client, this->info);

	this->legs.model = this->client.legs;
	this->legs.scale = 1.0;
	this->legs.color = Vec4(1.0, 1.0, 1.0, 1.0);
	memcpy(this->legs.skins, this->client.legs_skins, sizeof(this->legs.skins));

	this->torso.model = this->client.torso;
	this->torso.scale = 1.0;
	this->torso.color = Vec4(1.0, 1.0, 1.0, 1.0);
	this->torso.tag = "tag_torso";
	memcpy(this->torso.skins, this->client.torso_skins, sizeof(this->torso.skins));

	this->head.model = this->client.head;
	this->head.scale = 1.0;
	this->head.color = Vec4(1.0, 1.0, 1.0, 1.0);
	this->head.tag = "tag_head";
	memcpy(this->head.skins, this->client.head_skins, sizeof(this->head.skins));

	this->weapon.model = cgi.LoadModel("models/weapons/rocketlauncher/tris");
	this->weapon.scale = 1.0;
	this->weapon.color = Vec4(1.0, 1.0, 1.0, 1.0);
	this->weapon.tag = "tag_weapon";

	this->platformBase.model = cgi.LoadModel("models/objects/platform/base/tris");
	this->platformBase.scale = 1.0;
	this->platformBase.color = Vec4(1.0, 1.0, 1.0, 1.0);

	this->platformCenter.model = cgi.LoadModel("models/objects/platform/center/tris");
	this->platformCenter.scale = 1.0;
	this->platformCenter.color = Vec4(1.0, 1.0, 1.0, 1.0);

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

	const r_mesh_animation_t *anim = &model->animations[a->animation];

	const int32_t frameTime = 1500 / anim->hz;
	const int32_t animationTime = anim->num_frames * frameTime;
	const int32_t elapsedTime = cgi.client->unclamped_time - a->time;

	if (elapsedTime >= animationTime) {

		a->time = cgi.client->unclamped_time;

		animate_(model, a, e);
		return;
	}

	int32_t frame = elapsedTime / frameTime;
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

	a->lerp = (elapsedTime % frameTime) / (float) frameTime;
	a->fraction = elapsedTime / (float) animationTime;

	e->frame = a->frame;
	e->old_frame = a->old_frame;
	e->lerp = a->lerp;
	e->back_lerp = 1.0f - a->lerp;
}

/**
 * @fn void PlayerModelView::animate(PlayerModelView *self)
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

	if (self->client.shirt.a) {
		self->torso.tints[0] = Color_Vec4(self->client.shirt);
	} else {
		self->torso.tints[0] = Vec4_Zero();
	}

	if (self->client.pants.a) {
		self->legs.tints[1] = Color_Vec4(self->client.pants);
	} else {
		self->legs.tints[1] = Vec4_Zero();
	}

	if (self->client.helmet.a) {
		self->head.tints[2] = Color_Vec4(self->client.helmet);
	} else {
		self->head.tints[2] = Vec4_Zero();
	}
}

/**
 * @fn PlayerModelView *PlayerModelView::initWithFrame(PlayerModelView *self, const SDL_Rect *frame)
 *
 * @memberof PlayerModelView
 */
static PlayerModelView *initWithFrame(PlayerModelView *self, const SDL_Rect *frame) {

	self = (PlayerModelView *) super(Control, self, initWithFrame, frame);
	if (self) {
		self->yaw = 150.0;
		self->zoom = 0.1;

		self->animation1.animation = ANIM_TORSO_STAND1;
		self->animation2.animation = ANIM_LEGS_IDLE;

		self->iconView = $(alloc(ImageView), initWithFrame, NULL);
		assert(self->iconView);

		$((View *) self->iconView, addClassName, "iconView");
		$((View *) self, addSubview, (View *) self->iconView);

		$((Control *) self, addActionForEventType, SDL_MOUSEMOTION, rotateAction, self, NULL);
		$((Control *) self, addActionForEventType, SDL_MOUSEWHEEL, zoomAction, self, NULL);
	}

	return self;
}

#pragma mark - Class lifecycle

/*-*
 * @see Class::initialize(Class *)
 */
static void initialize(Class *clazz) {

	((ObjectInterface *) clazz->interface)->dealloc = dealloc;

	((ViewInterface *) clazz->interface)->init = init;
	((ViewInterface *) clazz->interface)->render = render;
	((ViewInterface *) clazz->interface)->renderDeviceDidReset = renderDeviceDidReset;
	((ViewInterface *) clazz->interface)->renderDeviceWillReset = renderDeviceWillReset;
	((ViewInterface *) clazz->interface)->updateBindings = updateBindings;

	((ControlInterface *) clazz->interface)->captureEvent = captureEvent;

	((PlayerModelViewInterface *) clazz->interface)->animate = animate;
	((PlayerModelViewInterface *) clazz->interface)->initWithFrame = initWithFrame;
}

/**
 * @fn Class *PlayerModelView::_PlayerModelView(void)
 * @memberof PlayerModelView
 */
Class *_PlayerModelView(void) {
	static Class *clazz;
	static Once once;

	do_once(&once, {
		clazz = _initialize(&(const ClassDef) {
			.name = "PlayerModelView",
			.superclass = _Control(),
			.instanceSize = sizeof(PlayerModelView),
			.interfaceOffset = offsetof(PlayerModelView, interface),
			.interfaceSize = sizeof(PlayerModelViewInterface),
			.initialize = initialize,
		});
	});

	return clazz;
}

#undef _Class
