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
#include "views/PrimaryIcon.h"

/**
 * @brief
 */
void Cgui_BindInput(View *view, const char *name, const char *bind) {

	BindTextView *textView = $(alloc(BindTextView), initWithBind, view, bind);

	Cgui_Input(view, name, (Control *) textView);

	release(textView);
}

/**
 * @brief
 */
void Cgui_Button(View *view, const char *title, ActionFunction function, ident sender, ident data) {

	Button *button = $(alloc(Button), initWithFrame, NULL, ControlStyleDefault);

	$(button->title, setText, title);

	$((Control *) button, addActionForEventType, SDL_MOUSEBUTTONUP, function, sender, data);

	$(view, addSubview, (View *) button);

	release(button);
}

/**
 * @brief
 */
void Cgui_CvarCheckboxInput(View *view, const char *label, const char *name) {

	cvar_t *var = cgi.CvarGet(name);
	assert(var);

	CvarCheckbox *checkbox = $(alloc(CvarCheckbox), initWithVariable, var);

	Cgui_Input(view, label, (Control *) checkbox);

	release(checkbox);
}

/**
 * @brief
 */
void Cgui_CvarSliderInput(View *view, const char *label, const char *name, double min, double max, double step) {

	cvar_t *var = cgi.CvarGet(name);
	assert(var);

	CvarSlider *slider = $(alloc(CvarSlider), initWithVariable, var, min, max, step);

	if ((step - floor(step)) == 0.0) { // Integer step; also visualize label as integer
		$((Slider *) slider, setLabelFormat, "%0.0f");
	}

	Cgui_Input(view, label, (Control *) slider);

	release(slider);
}

/**
 * @brief
 */
void Cgui_CvarTextView(View *view, const char *label, const char *name) {

	cvar_t *var = cgi.CvarGet(name);
	assert(var);

	CvarTextView *textView = $(alloc(CvarTextView), initWithVariable, var);

	Cgui_Input(view, label, (Control *) textView);

	release(textView);
}

#define INPUT_LABEL_WIDTH 160

/**
 * @brief Adds a new Label and the specified Control to the given View.
 *
 * @remarks This function releases the Control for convenience.
 */
void Cgui_Input(View *view, const char *label, Control *control) {

	assert(view);
	assert(control);

	Input *input = $(alloc(Input), initWithFrame, NULL);
	assert(input);

	$(input, setControl, control);

	$(input->label->text, setText, label);

	input->label->view.frame.w = INPUT_LABEL_WIDTH;

	input->label->view.autoresizingMask &= ~ViewAutoresizingContain;

	$(view, addSubview, (View *) input);

	release(input);
}

#define LABEL_HEIGHT 30

/**
 * @brief
 */
void Cgui_Label(View *view, const char *text) {

	Label *label = $(alloc(Label), initWithText, text, NULL);

	label->view.frame.h = LABEL_HEIGHT;

	label->view.autoresizingMask |= ViewAutoresizingWidth;

	label->view.alignment = ViewAlignmentTopLeft;

	label->text->view.alignment = ViewAlignmentMiddleCenter;

	$(view, addSubview, (View *) label);

	release(label);
}

/**
 * @brief
 */
void Cgui_Picture(View *view, const char *pic, ViewAlignment align, ViewAutoresizing resize) {

	assert(view);

	ImageView *imageView = $(alloc(ImageView), initWithFrame, NULL);
	assert(imageView);

	SDL_Surface *surface;

	if (cgi.LoadSurface(va("ui/pics/%s", pic), &surface)) {
		$(imageView, setImageWithSurface, surface);

		imageView->view.frame.w = surface->w;
		imageView->view.frame.h = surface->h;

		SDL_FreeSurface(surface);
	} else {
		$(imageView, setImage, NULL);
	}

	imageView->view.alignment = align;
	imageView->view.autoresizingMask = resize;

	$(view, addSubview, (View *) imageView);
	release(imageView);
}

/**
 * @brief
 */
void Cgui_PrimaryButton(View *view, const char *name, SDL_Color color, ActionFunction action, ident sender, ident data) {

	assert(view);

	const SDL_Rect frame = MakeRect(0, 0, DEFAULT_PRIMARY_BUTTON_WIDTH, 36);

	PrimaryButton *button = $(alloc(PrimaryButton), initWithFrame, &frame, ControlStyleDefault);
	assert(button);

	$(button->button.title, setText, name);

	Control *control = (Control *) button;

	control->view.autoresizingMask = ViewAutoresizingNone;

	control->view.backgroundColor = color;
	control->view.borderColor = QColors.BorderLight;

	$(control, addActionForEventType, SDL_MOUSEBUTTONUP, action, sender, data);

	$(view, addSubview, (View *) button);

	release(button);
}

/**
 * @brief
 */
void Cgui_PrimaryIcon(View *view, const char *icon, SDL_Color color, ActionFunction action, ident sender, ident data) {

	assert(view);

	const SDL_Rect frame = MakeRect(0, 0, DEFAULT_PRIMARY_ICON_WIDTH, DEFAULT_PRIMARY_ICON_WIDTH);

	PrimaryIcon *button = $(alloc(PrimaryIcon), initWithFrame, &frame, icon);
	assert(button);

	Control *control = (Control *) button;

	control->view.autoresizingMask = ViewAutoresizingNone;

	control->view.backgroundColor = color;
	control->view.borderColor = QColors.BorderLight;

	$(control, addActionForEventType, SDL_MOUSEBUTTONUP, action, sender, data);

	$(view, addSubview, (View *) button);

	release(button);
}
