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

#include "Theme.h"

#include "BindTextView.h"
#include "CvarCheckbox.h"
#include "CvarSelect.h"
#include "CvarSlider.h"
#include "CvarTextView.h"

#define _Class _Theme

#pragma mark - Theme

/**
 * @fn StackView *Theme::accessories(const Theme *self)
 * @memberof Theme
 */
static StackView *accessories(const Theme *self) {

	StackView *accessories = $(self, container);

	accessories->axis = StackViewAxisHorizontal;
	accessories->view.alignment = ViewAlignmentBottomRight;

	return accessories;
}

/**
 * @fn void Theme::attach(const Theme *self, ident view)
 * @memberof Theme
 */
static void attach(const Theme *self, ident view) {

	assert(self->target);

	$(self->target, addSubview, cast(View, view));
}

/**
 * @fn void Theme::bindTextView(const Theme *self, const char *label, const char *bind, TextViewDelegate *delegate)
 * @memberof Theme
 */
void bindTextView(const Theme *self, const char *label, const char *bind, TextViewDelegate *delegate) {

	TextView *textView = (TextView *) $(alloc(BindTextView), initWithBind, bind);
	assert(textView);

	if (delegate) {
		textView->delegate = *delegate;
	}

	$(self, control, label, textView);
	release(textView);
}


/**
 * @fn Bpx *Theme::box(const Theme *self, const char *label)
 * @memberof Theme
 */
static Box *box(const Theme *self, const char *label) {

	Box *box = $(alloc(Box), initWithFrame, NULL);
	assert(box);

	$(box->label->text, setText, label);

	box->view.autoresizingMask |= ViewAutoresizingWidth;
	return box;
}

/**
 * @fn void Theme::button(const Theme *self, const char *title, ActionFunction function, ident sender, ident data)
 * @memberof Theme
 */
static void button(const Theme *self, const char *title, ActionFunction function, ident sender, ident data) {

	Control *button = (Control *) $(alloc(Button), initWithTitle, title, ControlStyleDefault);
	assert(button);

	$(button, addActionForEventType, SDL_MOUSEBUTTONUP, function, sender, data);

	$(self, attach, button);
	release(button);
}

/**
 * @fn void Theme::checkbox(const Theme *self, const char *label, const char *name)
 * @memberof Theme
 */
static void checkbox(const Theme *self, const char *label, const char *name) {

	CvarCheckbox *checkbox = $(alloc(CvarCheckbox), initWithVariable, cgi.CvarGet(name));
	assert(checkbox);

	$(self, control, label, checkbox);
	release(checkbox);
}

/**
 * @fn StackView *Theme::columns(const Theme *self, size_t count)
 * @memberof Theme
 */
static StackView *columns(const Theme *self, size_t count) {

	StackView *columns = $(self, container);

	columns->axis = StackViewAxisHorizontal;
	columns->distribution = StackViewDistributionFill;

	for (size_t i = 0; i < count; i++) {

		StackView *column = $(self, container);
		$((View *) columns, addSubview, (View *) column);
	}

	return columns;
}

/**
 * @fn StackView *Theme::container(const Theme *self)
 * @memberof Theme
 */
static StackView *container(const Theme *self) {

	StackView *container = $(alloc(StackView), initWithFrame, NULL);
	assert(container);

	container->spacing = DEFAULT_PANEL_SPACING << 1;
	return container;
}

/**
 * @fn void Theme::control(const Theme *self, const char *label, ident control)
 * @memberof Theme
 */
static void control(const Theme *self, const char *label, ident control) {

	Input *input = $(alloc(Input), initWithFrame, NULL);
	assert(input);

	$(input, setControl, cast(Control, control));
	$(input->label->text, setText, label);

	input->label->view.frame.w = THEME_INPUT_LABEL_WIDTH;
	input->label->view.autoresizingMask &= ~ViewAutoresizingContain;

	$(self, attach, input);
	release(input);
}

/**
 * @fn Theme *Theme::init(Theme *self)
 * @memberof Theme
 */
static Theme *init(Theme *self) {
	return $(self, initWithTarget, NULL);
}

/**
 * @fn Theme *Theme::init(Theme *self)
 * @memberof Theme
 */
static Theme *initWithTarget(Theme *self, ident target) {

	self = (Theme *) super(Object, self, init);
	if (self) {
		self->colors = (ThemeColors) {
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
 * @fn Panel *Theme::panel(const Theme *self)
 * @memberof Theme
 */
static Panel *panel(const Theme *self) {

	Panel *panel = $(alloc(Panel), initWithFrame, NULL);
	assert(panel);

	panel->isDraggable = false;
	panel->isResizable = false;
	panel->stackView.view.alignment = ViewAlignmentMiddleCenter;
	panel->stackView.view.backgroundColor = self->colors.main;
	panel->stackView.view.borderColor = self->colors.lightBorder;

	return panel;
}

/**
 * @fn void Theme::slider(const Theme *self, const char *label, const char *name, double min, double max, double step, SliderDelegate *delegate)
 * @memberof Theme
 */
static void slider(const Theme *self, const char *label, const char *name,
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
 * @fn void Theme::target(Theme *self, ident target)
 * @memberof Theme
 */
static void target(Theme *self, ident target) {

	if (target) {
		self->target = cast(View, target);
	} else {
		self->target = NULL;
	}
}

/**
 * @fn void Theme::target(Theme *self, ident target)
 * @memberof Theme
 */
static void targetSubview(Theme *self, ident view, int index) {

	assert(view);

	const Array *subviews = (Array *) cast(View, view)->subviews;

	self->target = $(subviews, objectAtIndex, index);
}

/**
 * @fn void Theme::textView(const Theme *self, const char *label, const char *name)
 * @memberof Theme
 */
static void textView(const Theme *self, const char *label, const char *name) {

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

	((ThemeInterface *) clazz->def->interface)->accessories = accessories;
	((ThemeInterface *) clazz->def->interface)->attach = attach;
	((ThemeInterface *) clazz->def->interface)->bindTextView = bindTextView;
	((ThemeInterface *) clazz->def->interface)->box = box;
	((ThemeInterface *) clazz->def->interface)->button = button;
	((ThemeInterface *) clazz->def->interface)->checkbox = checkbox;
	((ThemeInterface *) clazz->def->interface)->columns = columns;
	((ThemeInterface *) clazz->def->interface)->container = container;
	((ThemeInterface *) clazz->def->interface)->control = control;
	((ThemeInterface *) clazz->def->interface)->init = init;
	((ThemeInterface *) clazz->def->interface)->initWithTarget = initWithTarget;
	((ThemeInterface *) clazz->def->interface)->panel = panel;
	((ThemeInterface *) clazz->def->interface)->slider = slider;
	((ThemeInterface *) clazz->def->interface)->target = target;
	((ThemeInterface *) clazz->def->interface)->targetSubview = targetSubview;
	((ThemeInterface *) clazz->def->interface)->textView = textView;
}

/**
 * @fn Class *Theme::_Theme(void)
 * @memberof Theme
 */
Class *_Theme(void) {
	static Class clazz;
	static Once once;

	do_once(&once, {
		clazz.name = "Theme";
		clazz.superclass = _Object();
		clazz.instanceSize = sizeof(Theme);
		clazz.interfaceOffset = offsetof(Theme, interface);
		clazz.interfaceSize = sizeof(ThemeInterface);
		clazz.initialize = initialize;
	});

	return &clazz;
}

#undef _Class
