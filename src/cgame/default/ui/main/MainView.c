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

#include "cg_local.h"

#include "MainView.h"
#include "Theme.h"

#define _Class _MainView

#pragma mark - Object

/**
 * @see Object::dealloc(Object *)
 */
static void dealloc(Object *self) {

	MainView *this = (MainView *) self;

	release(this->background);
	release(this->topBar);
	release(this->bottomBar);
	release(this->logo);
	release(this->version);
	release(this->dialog);

	super(Object, self, dealloc);
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
		this->autoresizingMask = ViewAutoresizingFill;

		Theme *theme = $(alloc(Theme), initWithTarget, self);
		assert(theme);

		{
			self->background = $(alloc(ImageView), initWithFrame, NULL);
			assert(self->background);

			SDL_Surface *surface;
			if (cgi.LoadSurface(va("ui/backgrounds/%d", Random() % 6), &surface)) {
				$(self->background, setImageWithSurface, surface);
				SDL_FreeSurface(surface);
			}

			self->background->view.autoresizingMask = ViewAutoresizingFill;
			self->background->view.zIndex = -1;

			$(this, addSubview, (View *) self->background);
		}

		{
			self->logo = $(alloc(ImageView), initWithFrame, NULL);
			assert(self->logo);

			SDL_Surface *surface;
			if (cgi.LoadSurface("ui/logo", &surface)) {
				$(self->logo, setImageWithSurface, surface);
				SDL_FreeSurface(surface);
			}

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
			self->topBar = $(alloc(StackView), initWithFrame, NULL);
			assert(self->topBar);

			self->topBar->axis = StackViewAxisHorizontal;
			self->topBar->spacing = DEFAULT_PANEL_SPACING;
			self->topBar->view.alignment = ViewAlignmentTopCenter;
			self->topBar->view.autoresizingMask |= ViewAutoresizingWidth;
			self->topBar->view.backgroundColor = theme->colors.mainHighlight;
			self->topBar->view.borderColor = theme->colors.lightBorder;
			self->topBar->view.padding.right = DEFAULT_PANEL_SPACING;
			self->topBar->view.padding.left = DEFAULT_PANEL_SPACING;
			self->topBar->view.zIndex = 1;

			$(this, addSubview, (View *) self->topBar);
		}

		{
			self->contentView = $(alloc(View), initWithFrame, NULL);
			assert(self->contentView);

			self->contentView->alignment = ViewAlignmentMiddleCenter;
			self->contentView->autoresizingMask = ViewAutoresizingFill;
			self->contentView->padding.top = 80;
			self->contentView->padding.bottom = 80;
		}

		{
			self->bottomBar = $(alloc(StackView), initWithFrame, NULL);
			assert(self->bottomBar);

			self->bottomBar->axis = StackViewAxisHorizontal;
			self->bottomBar->spacing = DEFAULT_PANEL_SPACING;
			self->bottomBar->view.alignment = ViewAlignmentBottomCenter;
			self->bottomBar->view.autoresizingMask |= ViewAutoresizingWidth;
			self->bottomBar->view.backgroundColor = theme->colors.mainHighlight;
			self->bottomBar->view.borderColor = theme->colors.lightBorder;
			self->bottomBar->view.padding.right = DEFAULT_PANEL_SPACING;
			self->bottomBar->view.padding.left = DEFAULT_PANEL_SPACING;
			self->bottomBar->view.zIndex = 1;

			$(this, addSubview, (View *) self->bottomBar);
		}

		{
			self->dialog = $(alloc(DialogView), init);
			assert(self->dialog);

			$(this, addSubview, (View *) self->dialog);
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
