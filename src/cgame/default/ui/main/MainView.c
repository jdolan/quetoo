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

#pragma mark - Object

/**
 * @see Object::dealloc(Object *)
 */
static void dealloc(Object *self) {

	MainView *this = (MainView *) self;

	release(this->background);
	release(this->topBar);
	release(this->primaryButtons);
	release(this->primaryIcons);
	release(this->bottomBar);
	release(this->logo);
	release(this->version);

	super(Object, self, dealloc);
}

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
	this->bottomBar->view.hidden = !isActive;
}

#pragma mark - MainView

/**
 * @fn MainView *MainView::initWithFrame(MainView *self, const SDL_Rect *frame)
 * @memberof MainView
 */
static MainView *initWithFrame(MainView *self, const SDL_Rect *frame) {

	self = (MainView *) super(View, self, initWithFrame, frame);
	if (self) {

		View *this = (View *) self;
		this->identifier = strdup("main");

		this->stylesheet = cgi.Stylesheet("ui/main/MainView.css");
		assert(this->stylesheet);

		QuetooTheme *theme = $(alloc(QuetooTheme), initWithTarget, self);
		assert(theme);

		{
			self->background = $(theme, image, va("ui/backgrounds/%d", Random() % 6), NULL);
			assert(self->background);

			self->background->view.autoresizingMask = ViewAutoresizingFill;

			$(this, addSubview, (View *) self->background);
		}

		{
			self->logo = $(theme, image, "ui/logo", NULL);
			assert(self->logo);

			self->logo->view.alignment = ViewAlignmentBottomRight;

			$(this, addSubview, (View *) self->logo);
		}

		{
			self->version = $(alloc(Label), initWithText, va("Quetoo %s", cgi.CvarGet("version")->string), NULL);
			assert(self->version);

			self->version->text->color = theme->colors.watermark;
			self->version->view.alignment = ViewAlignmentBottomLeft;

			$(this, addSubview, (View *) self->version);
		}

		{
			self->contentView = $(alloc(View), initWithFrame, NULL);
			assert(self->contentView);

			self->contentView->autoresizingMask = ViewAutoresizingFill;
			self->contentView->clipsSubviews = true;
			self->contentView->padding.top = 36;
			self->contentView->padding.bottom = 36;

			$(this, addSubview, (View *) self->contentView);
		}

		{
			self->topBar = $(alloc(StackView), initWithFrame, NULL);
			assert(self->topBar);

			self->topBar->view.identifier = strdup("topBar");

			self->topBar->axis = StackViewAxisHorizontal;
			self->topBar->distribution = StackViewDistributionFill;
			self->topBar->view.alignment = ViewAlignmentTopCenter;
			self->topBar->view.autoresizingMask |= ViewAutoresizingWidth;
			self->topBar->view.backgroundColor = theme->colors.mainHighlight;
			self->topBar->view.borderColor = theme->colors.lightBorder;
			self->topBar->view.padding.right = 12;
			self->topBar->view.padding.left = 12;

			self->primaryButtons = $(alloc(StackView), initWithFrame, NULL);
			assert(self->primaryButtons);

			$((View *) self->primaryButtons, addClassName, "primaryButtons");

			self->primaryButtons->axis = StackViewAxisHorizontal;
			self->primaryButtons->spacing = 12;
			self->primaryButtons->view.alignment = ViewAlignmentMiddleLeft;

			$((View *) self->topBar, addSubview, (View *) self->primaryButtons);

			self->primaryIcons = $(alloc(StackView), initWithFrame, NULL);
			assert(self->primaryIcons);

			$((View *) self->primaryIcons, addClassName, "primaryIcons");

			self->primaryIcons->axis = StackViewAxisHorizontal;
			self->primaryIcons->spacing = 12;
			self->primaryIcons->view.alignment = ViewAlignmentMiddleRight;

			$((View *) self->topBar, addSubview, (View *) self->primaryIcons);

			$(this, addSubview, (View *) self->topBar);
		}

		{
			self->bottomBar = $(alloc(StackView), initWithFrame, NULL);
			assert(self->bottomBar);

			self->bottomBar->view.identifier = strdup("bottomBar");

			self->bottomBar->axis = StackViewAxisHorizontal;
			self->bottomBar->spacing = 12;
			self->bottomBar->view.alignment = ViewAlignmentBottomCenter;
			self->bottomBar->view.autoresizingMask |= ViewAutoresizingWidth;
			self->bottomBar->view.backgroundColor = theme->colors.mainHighlight;
			self->bottomBar->view.borderColor = theme->colors.lightBorder;
			self->bottomBar->view.padding.right = 12;
			self->bottomBar->view.padding.left = 12;

			$(this, addSubview, (View *) self->bottomBar);
		}

		release(theme);
	}

	return self;
}

#pragma mark - Class lifecycle

/**
 * @see Class::initialize(Class *)
 */
static void initialize(Class *clazz) {

	((ObjectInterface *) clazz->def->interface)->dealloc = dealloc;

	((ViewInterface *) clazz->def->interface)->updateBindings = updateBindings;

	((MainViewInterface *) clazz->def->interface)->initWithFrame = initWithFrame;
}

/**
 * @fn Class *MainView::_MainView(void)
 * @memberof MainView
 */
Class *_MainView(void) {
	static Class clazz;
	static Once once;

	do_once(&once, {
		clazz.name = "MainView";
		clazz.superclass = _View();
		clazz.instanceSize = sizeof(MainView);
		clazz.interfaceOffset = offsetof(MainView, interface);
		clazz.interfaceSize = sizeof(MainViewInterface);
		clazz.initialize = initialize;
	});

	return &clazz;
}

#undef _Class
