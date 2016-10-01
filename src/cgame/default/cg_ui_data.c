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

#include "views/BindTextView.h"
#include "views/CvarCheckbox.h"
#include "views/CvarSlider.h"
#include "views/CvarTextView.h"
#include "views/PrimaryButton.h"

/**
 * @brief
 */
void Cg_BindInput(View *view, const char *name, const char *bind) {

	BindTextView *textView = $(alloc(BindTextView), initWithBind, bind);

	Cg_Input(view, name, (Control *) textView);

	release(textView);
}

/**
 * @brief
 */
void Cg_Button(View *view, const char *title, ActionFunction function, ident sender, ident data) {

	Button *button = $(alloc(Button), initWithFrame, NULL, ControlStyleDefault);

	$(button->title, setText, title);

	$((Control *) button, addActionForEventType, SDL_MOUSEBUTTONUP, function, sender, data);

	$(view, addSubview, (View *) button);

	release(button);
}

/**
 * @brief
 */
void Cg_CvarCheckboxInput(View *view, const char *label, const char *name) {

	cvar_t *var = cgi.CvarGet(name);
	assert(var);

	CvarCheckbox *checkbox = $(alloc(CvarCheckbox), initWithVariable, var);

	Cg_Input(view, label, (Control *) checkbox);

	release(checkbox);
}

/**
 * @brief
 */
void Cg_CvarSliderInput(View *view, const char *label, const char *name, double min, double max, double step) {

	cvar_t *var = cgi.CvarGet(name);
	assert(var);

	CvarSlider *slider = $(alloc(CvarSlider), initWithVariable, var, min, max, step);

	Cg_Input(view, label, (Control *) slider);

	release(slider);
}

/**
 * @brief
 */
void Cg_CvarTextView(View *view, const char *label, const char *name) {

	cvar_t *var = cgi.CvarGet(name);
	assert(var);
	
	CvarTextView *textView = $(alloc(CvarTextView), initWithVariable, var);

	Cg_Input(view, label, (Control *) textView);

	release(textView);
}

#define INPUT_LABEL_WIDTH 140

/**
 * @brief Adds a new Label and the specified Control to the given View.
 *
 * @remarks This function releases the Control for convenience.
 */
void Cg_Input(View *view, const char *label, Control *control) {

	assert(view);
	assert(control);

	Input *input = $(alloc(Input), initWithFrame, NULL);
	assert(input);

	$(input, setControl, control);

	$(input->label->text, setText, label);

	input->label->view.autoresizingMask &= ~ViewAutoresizingContain;
	input->label->view.frame.w = INPUT_LABEL_WIDTH;

	$(view, addSubview, (View *) input);

	release(input);
}

/**
 * @brief
 */
void Cg_PrimaryButton(View *view, const char *name, ActionFunction action, ident sender, ident data) {

	assert(view);

	PrimaryButton *button = $(alloc(PrimaryButton), initWithFrame, NULL, ControlStyleDefault);
	assert(button);

	$(button->button.title, setText, name);

	$((Control *) button, addActionForEventType, SDL_MOUSEBUTTONUP, action, sender, data);

	$(view, addSubview, (View *) button);
	release(button);
}


