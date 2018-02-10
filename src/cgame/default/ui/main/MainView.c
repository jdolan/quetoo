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

#include "MainView.h"
#include "QuetooTheme.h"

#define _Class _MainView

#pragma mark - View

/**
 * @see View::updateBindings(View *)
 */
static void updateBindings(View *self) {

	super(View, self, updateBindings);

	MainView *this = (MainView *) self;

	const _Bool isActive = *cgi.state == CL_ACTIVE;

	this->background->view.hidden = isActive;
	this->logo->view.hidden = isActive;
	this->version->view.hidden = isActive;
	this->secondaryMenu->view.hidden = !isActive;
}

#pragma mark - MainView

/**
 * @fn MainView *MainView::initWithFrame(MainView *self, const SDL_Rect *frame)
 * @memberof MainView
 */
static MainView *initWithFrame(MainView *self, const SDL_Rect *frame) {

	self = (MainView *) super(View, self, initWithFrame, frame);
	if (self) {

		Outlet outlets[] = MakeOutlets(
			MakeOutlet("background", &self->background),
			MakeOutlet("logo", &self->logo),
			MakeOutlet("version", &self->version),
			MakeOutlet("contentView", &self->contentView),
			MakeOutlet("primaryMenu", &self->primaryMenu),
			MakeOutlet("secondaryMenu", &self->secondaryMenu)
		);

		View *this = (View *) self;

		cgi.WakeView(this, "ui/main/MainView.json", outlets);
		this->stylesheet = cgi.Stylesheet("ui/main/MainView.css");

		Image *image = cgi.Image(va("ui/backgrounds/%d", Random() % 6));
		if (image) {
			$(self->background, setImage, image);
		}
		release(image);

		image = cgi.Image("ui/logo");
		if (image) {
			$(self->logo, setImage, image);
		}
		release(image);

		$(self->version->text, setText, va("Quetoo %s", cgi.CvarGet("version")->string));
	}

	return self;
}

#pragma mark - Class lifecycle

/**
 * @see Class::initialize(Class *)
 */
static void initialize(Class *clazz) {

	((ViewInterface *) clazz->interface)->updateBindings = updateBindings;

	((MainViewInterface *) clazz->interface)->initWithFrame = initWithFrame;
}

/**
 * @fn Class *MainView::_MainView(void)
 * @memberof MainView
 */
Class *_MainView(void) {
	static Class *clazz;
	static Once once;

	do_once(&once, {
		clazz = _initialize(&(const ClassDef) {
			.name = "MainView",
			.superclass = _View(),
			.instanceSize = sizeof(MainView),
			.interfaceOffset = offsetof(MainView, interface),
			.interfaceSize = sizeof(MainViewInterface),
			.initialize = initialize,
		});
	});

	return clazz;
}

#undef _Class
