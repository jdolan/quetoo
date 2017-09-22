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

#include "AudioViewController.h"
#include "Theme.h"

#define _Class _AudioViewController

#pragma mark - Actions and delegate callbacks

/**
 * @brief ActionFunction for Apply Button.
 */
static void applyAction(Control *control, const SDL_Event *event, ident sender, ident data) {
	cgi.Cbuf("s_restart");
}

#pragma mark - ViewController

/**
 * @see ViewController::loadView(ViewController *)
 */
static void loadView(ViewController *self) {

	super(ViewController, self, loadView);

	self->view->autoresizingMask = ViewAutoresizingContain;
	self->view->identifier = strdup("Audio");

	Theme *theme = $(alloc(Theme), initWithTarget, self->view);
	assert(theme);

	StackView *container = $(theme, container);

	$(theme, attach, container);
	$(theme, target, container);

	StackView *columns = $(theme, columns, 2);

	$(theme, attach, columns);
	$(theme, targetSubview, columns, 0);

	{
		Box *box = $(theme, box, "Volume");

		$(theme, attach, box);
		$(theme, target, box->contentView);

		$(theme, slider, "Effects", "s_volume", 0.0, 1.0, 0.1);
		$(theme, slider, "Music", "s_music_volume", 0.0, 1.0, 0.1);

		release(box);
	}

	$(theme, targetSubview, columns, 1);

	{
		Box *box = $(theme, box, "Sounds");

		$(theme, attach, box);
		$(theme, target, box->contentView);

		$(theme, checkbox, "Ambient sounds", "s_ambient");
		$(theme, checkbox, "Hit sounds", cg_hit_sound->name);

		release(box);
	}

	$(theme, target, container);

	StackView *accessories = $(theme, container);

	accessories->axis = StackViewAxisHorizontal;
	accessories->view.alignment = ViewAlignmentBottomRight;

	$(theme, attach, accessories);
	$(theme, target, accessories);

	$(theme, button, "Apply", applyAction, self, NULL);

	release(accessories);
	release(columns);
	release(container);
	release(theme);
}

#pragma mark - Class lifecycle

/**
 * @see Class::initialize(Class *)
 */
static void initialize(Class *clazz) {
	((ViewControllerInterface *) clazz->def->interface)->loadView = loadView;
}

/**
 * @fn Class *AudioViewController::_AudioViewController(void)
 * @memberof AudioViewController
 */
Class *_AudioViewController(void) {
	static Class clazz;
	static Once once;

	do_once(&once, {
		clazz.name = "AudioViewController";
		clazz.superclass = _ViewController();
		clazz.instanceSize = sizeof(AudioViewController);
		clazz.interfaceOffset = offsetof(AudioViewController, interface);
		clazz.interfaceSize = sizeof(AudioViewControllerInterface);
		clazz.initialize = initialize;
	});

	return &clazz;
}

#undef _Class
