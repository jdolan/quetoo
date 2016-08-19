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

#include "ui_data.h"

#include "views/BindTextView.h"
#include "views/CvarCheckbox.h"
#include "views/CvarSlider.h"
#include "views/CvarTextView.h"

/**
 * @brief
 */
void Ui_Bind(View *view, const char *name, const char *bind) {

	BindTextView *textView = $(alloc(BindTextView), initWithBind, bind);

	Ui_Input(view, name, (Control *) textView);

	release(textView);
}

/**
 * @brief
 */
void Ui_CvarCheckbox(View *view, const char *name, cvar_t *var) {

	CvarCheckbox *checkbox = $(alloc(CvarCheckbox), initWithVariable, var);

	Ui_Input(view, name, (Control *) checkbox);

	release(checkbox);
}

/**
 * @brief
 */
void Ui_CvarSlider(View *view, const char *name, cvar_t *var, double min, double max, double step) {

	CvarSlider *slider = $(alloc(CvarSlider), initWithVariable, var, min, max, step);

	Ui_Input(view, name, (Control *) slider);

	release(slider);
}

/**
 * @brief
 */
void Ui_CvarTextView(View *view, const char *name, cvar_t *var) {

	CvarTextView *textView = $(alloc(CvarTextView), initWithVariable, var);

	Ui_Input(view, name, (Control *) textView);

	release(textView);
}

#define MENU_INPUT_LABEL_WIDTH 140

/**
 * @brief Adds a new Label and the specified Control to the given View.
 *
 * @remarks This function releases the Control for convenience.
 */
void Ui_Input(View *view, const char *name, Control *control) {

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
}

