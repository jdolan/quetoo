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

#include "MenuInput.h"

#include "BindTextView.h"
#include "CvarCheckbox.h"
#include "CvarSlider.h"

#define _Class _MenuInput

#pragma mark - MenuInput

static void input(View *view, Control *control, const char *name) {

	assert(view);
	assert(control);

	Label *label = $(alloc(Label), initWithText, name, NULL);
	assert(label);

	label->view.frame.w = MENU_INPUT_LABEL_WIDTH;

	Input *input = $(alloc(Input), initWithOrientation, InputOrientationLeft, control, label);
	assert(input);

	$(view, addSubview, (View *) input);

	release(input);
	release(label);
	release(control);
}

static void bindTextView(View *view, const char *bind, const char *name) {
	input(view, (Control *) $(alloc(BindTextView), initWithBind, bind), name);
}

static void cvarCheckbox(View *view, cvar_t *var, const char *name) {
	input(view, (Control *) $(alloc(CvarCheckbox), initWithVariable, var), name);
}

static void cvarSlider(View *view, cvar_t *var, const char *name, double min, double max, double step) {
	input(view, (Control *) $(alloc(CvarSlider), initWithVariable, var, min, max, step), name);
}

#pragma mark - Class lifecycle

/**
 * @see Class::initialize(Class *)
 */
static void initialize(Class *clazz) {

	((MenuInputInterface *) clazz->interface)->input = input;
	((MenuInputInterface *) clazz->interface)->bindTextView = bindTextView;
	((MenuInputInterface *) clazz->interface)->cvarCheckbox = cvarCheckbox;
	((MenuInputInterface *) clazz->interface)->cvarSlider = cvarSlider;
}

Class _MenuInput = {
	.name = "MenuInput",
	.superclass = &_Input,
	.instanceSize = sizeof(MenuInput),
	.interfaceOffset = offsetof(MenuInput, interface),
	.interfaceSize = sizeof(MenuInputInterface),
	.initialize = initialize,
};

#undef _Class

