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

#include "QuetooTheme.h"

#include "BindTextView.h"
#include "CvarCheckbox.h"
#include "CvarSelect.h"
#include "CvarSlider.h"
#include "CvarTextView.h"

#define _Class _QuetooTheme

#pragma mark - QuetooTheme

/**
 * @fn StackView *QuetooTheme::accessories(const QuetooTheme *self)
 * @memberof QuetooTheme
 */
static StackView *accessories(const QuetooTheme *self) {

	StackView *accessories = $(self, container);

	accessories->axis = StackViewAxisHorizontal;
	accessories->view.alignment = ViewAlignmentBottomRight;

	return accessories;
}

/**
 * @fn void QuetooTheme::attach(const QuetooTheme *self, ident view)
 * @memberof QuetooTheme
 */
static void attach(const QuetooTheme *self, ident view) {

	assert(self->target);

	$(self->target, addSubview, cast(View, view));
}

/**
 * @fn void QuetooTheme::bindTextView(const QuetooTheme *self, const char *label, const char *bind, TextViewDelegate *delegate)
 * @memberof QuetooTheme
 */
static void bindTextView(const QuetooTheme *self, const char *label, const char *bind, TextViewDelegate *delegate) {

	TextView *textView = (TextView *) $(alloc(BindTextView), initWithBind, bind);
	assert(textView);

	if (delegate) {
		textView->delegate = *delegate;
	}

	$(self, control, label, textView);
	release(textView);
}


/**
 * @fn Bpx *QuetooTheme::box(const QuetooTheme *self, const char *label)
 * @memberof QuetooTheme
 */
static Box *box(const QuetooTheme *self, const char *label) {

	Box *box = $(alloc(Box), initWithFrame, NULL);
	assert(box);

	$(box->label->text, setText, label);

	box->view.autoresizingMask |= ViewAutoresizingWidth;
	return box;
}

/**
 * @fn void QuetooTheme::button(const QuetooTheme *self, const char *title, ActionFunction function, ident sender, ident data)
 * @memberof QuetooTheme
 */
static void button(const QuetooTheme *self, const char *title, ActionFunction function, ident sender, ident data) {

	Control *button = (Control *) $(alloc(Button), initWithTitle, title);
	assert(button);

	$(button, addActionForEventType, SDL_MOUSEBUTTONUP, function, sender, data);

	$(self, attach, button);
	release(button);
}

/**
 * @fn void QuetooTheme::checkbox(const QuetooTheme *self, const char *label, const char *name)
 * @memberof QuetooTheme
 */
static void checkbox(const QuetooTheme *self, const char *label, const char *name) {

	CvarCheckbox *checkbox = $(alloc(CvarCheckbox), initWithVariable, cgi.CvarGet(name));
	assert(checkbox);

	$(self, control, label, checkbox);
	release(checkbox);
}

/**
 * @fn StackView *QuetooTheme::columns(const QuetooTheme *self, size_t count)
 * @memberof QuetooTheme
 */
static StackView *columns(const QuetooTheme *self, size_t count) {

	StackView *columns = $(self, container);

	$((View *) columns, addClassName, "columns");

	for (size_t i = 0; i < count; i++) {

		StackView *column = $(self, container);

		$((View *) column, addClassName, "column");
		$((View *) columns, addSubview, (View *) column);
	}

	return columns;
}

/**
 * @fn StackView *QuetooTheme::container(const QuetooTheme *self)
 * @memberof QuetooTheme
 */
static StackView *container(const QuetooTheme *self) {

	StackView *container = $(alloc(StackView), initWithFrame, NULL);
	assert(container);

	$((View *) container, addClassName, "container");

	return container;
}

/**
 * @fn void QuetooTheme::control(const QuetooTheme *self, const char *label, ident control)
 * @memberof QuetooTheme
 */
static void control(const QuetooTheme *self, const char *label, ident control) {

	Input *input = $(alloc(Input), initWithFrame, NULL);
	assert(input);

	$(input, setControl, cast(Control, control));
	$(input->label->text, setText, label);

	$(self, attach, input);
	release(input);
}

/**
 * @fn ImageView *QuetooTheme::imageView(const QuetooTheme *self, const char *name, const SDL_Rect *frame)
 * @memberof QuetooTheme
 */
static ImageView *image(const QuetooTheme *self, const char *name, const SDL_Rect *frame) {

	ImageView *image = $(alloc(ImageView), initWithFrame, frame);
	assert(image);

	SDL_Surface *surface;
	if (cgi.LoadSurface(name, &surface)) {
		$(image, setImageWithSurface, surface);
		SDL_FreeSurface(surface);
	} else {
		$(image, setImage, NULL);
	}

	return image;
}

/**
 * @fn QuetooTheme *QuetooTheme::init(QuetooTheme *self)
 * @memberof QuetooTheme
 */
static QuetooTheme *init(QuetooTheme *self) {
	return $(self, initWithTarget, NULL);
}

/**
 * @fn QuetooTheme *QuetooTheme::init(QuetooTheme *self)
 * @memberof QuetooTheme
 */
static QuetooTheme *initWithTarget(QuetooTheme *self, ident target) {

	self = (QuetooTheme *) super(Object, self, init);
	if (self) {
		self->colors = (QuetooThemeColors) {
			.main = { 128, 128, 128, 224 },
			.mainHighlight = { 80, 80, 80, 80 },

			.dark = { 8, 21, 26, 255 },
			.darkHighlight = { 23, 36, 41, 255 },
			.darkBorder = { 0, 0, 0, 0 },

			.light = { 12, 100, 100, 255 },
			.lightHighlight = { 27, 115, 115, 255 },
			.lightBorder = { 255, 255, 255, 0 },

			.contrast = { 255, 255, 255, 255 },
			.watermark = { 200, 200, 200, 255 }
		 };

		$(self, target, target);
	}
	
	return self;
}

/**
 * @fn Panel *QuetooTheme::panel(const QuetooTheme *self)
 * @memberof QuetooTheme
 */
static Panel *panel(const QuetooTheme *self) {

	Panel *panel = $(alloc(Panel), initWithFrame, NULL);
	assert(panel);

	panel->isDraggable = false;
	panel->isResizable = false;

	panel->control.view.alignment = ViewAlignmentMiddleCenter;
	panel->control.view.backgroundColor = self->colors.main;
	panel->control.view.borderColor = self->colors.lightBorder;

	return panel;
}

/**
 * @fn void QuetooTheme::slider(const QuetooTheme *self, const char *label, const char *name, double min, double max, double step, SliderDelegate *delegate)
 * @memberof QuetooTheme
 */
static void slider(const QuetooTheme *self, const char *label, const char *name,
				   double min, double max, double step, SliderDelegate *delegate) {

	Slider *slider = (Slider *) $(alloc(CvarSlider), initWithVariable, cgi.CvarGet(name), min, max, step);
	assert(slider);

	if (delegate) {
		slider->delegate = *delegate;
	}

	$(self, control, label, slider);
	release(slider);
}

/**
 * @fn void QuetooTheme::target(QuetooTheme *self, ident target)
 * @memberof QuetooTheme
 */
static void target(QuetooTheme *self, ident target) {

	if (target) {
		self->target = cast(View, target);
	} else {
		self->target = NULL;
	}
}

/**
 * @fn void QuetooTheme::target(QuetooTheme *self, ident target)
 * @memberof QuetooTheme
 */
static void targetSubview(QuetooTheme *self, ident view, int index) {

	assert(view);

	const Array *subviews = (Array *) cast(View, view)->subviews;

	self->target = $(subviews, objectAtIndex, index);
}

/**
 * @fn void QuetooTheme::textView(const QuetooTheme *self, const char *label, const char *name)
 * @memberof QuetooTheme
 */
static void textView(const QuetooTheme *self, const char *label, const char *name) {

	CvarTextView *textView = $(alloc(CvarTextView), initWithVariable, cgi.CvarGet(name));
	assert(textView);

	$(self, control, label, textView);
	release(textView);
}

#pragma mark - Class lifecycle

/**
 * @see Class::initialize(Class *)
 */
static void initialize(Class *clazz) {

	((QuetooThemeInterface *) clazz->def->interface)->accessories = accessories;
	((QuetooThemeInterface *) clazz->def->interface)->attach = attach;
	((QuetooThemeInterface *) clazz->def->interface)->bindTextView = bindTextView;
	((QuetooThemeInterface *) clazz->def->interface)->box = box;
	((QuetooThemeInterface *) clazz->def->interface)->button = button;
	((QuetooThemeInterface *) clazz->def->interface)->checkbox = checkbox;
	((QuetooThemeInterface *) clazz->def->interface)->columns = columns;
	((QuetooThemeInterface *) clazz->def->interface)->container = container;
	((QuetooThemeInterface *) clazz->def->interface)->control = control;
	((QuetooThemeInterface *) clazz->def->interface)->image = image;
	((QuetooThemeInterface *) clazz->def->interface)->init = init;
	((QuetooThemeInterface *) clazz->def->interface)->initWithTarget = initWithTarget;
	((QuetooThemeInterface *) clazz->def->interface)->panel = panel;
	((QuetooThemeInterface *) clazz->def->interface)->slider = slider;
	((QuetooThemeInterface *) clazz->def->interface)->target = target;
	((QuetooThemeInterface *) clazz->def->interface)->targetSubview = targetSubview;
	((QuetooThemeInterface *) clazz->def->interface)->textView = textView;
}

/**
 * @fn Class *QuetooTheme::_QuetooTheme(void)
 * @memberof QuetooTheme
 */
Class *_QuetooTheme(void) {
	static Class clazz;
	static Once once;

	do_once(&once, {
		clazz.name = "QuetooTheme";
		clazz.superclass = _Object();
		clazz.instanceSize = sizeof(QuetooTheme);
		clazz.interfaceOffset = offsetof(QuetooTheme, interface);
		clazz.interfaceSize = sizeof(QuetooThemeInterface);
		clazz.initialize = initialize;
	});

	return &clazz;
}

#undef _Class
