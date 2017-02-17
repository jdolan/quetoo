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

#include "ui_local.h"

#include "EditorViewController.h"

/**
 * @brief
 */
void addButton(View *view, const char *title, ActionFunction function, ident sender, ident data) {

	Button *button = $(alloc(Button), initWithFrame, NULL, ControlStyleDefault);

	$(button->title, setText, title);

	$((Control *) button, addActionForEventType, SDL_MOUSEBUTTONUP, function, sender, data);

	$(view, addSubview, (View *) button);

	release(button);
}

#define INPUT_LABEL_WIDTH 140

/**
 * @brief Adds a new Label and the specified Control to the given View.
 *
 * @remarks This function releases the Control for convenience.
 */
void addInput(View *view, const char *label, Control *control) {

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

#define _Class _EditorViewController

#pragma mark - Actions & Delegates

/**
 * @brief ActionFunction for the Save Button.
 */
static void saveAction(Control *control, const SDL_Event *event, ident sender, ident data) {

	printf("Save!\n");
}

/**
 * @brief SliderDelegate callback for changing this->bump.
 */
static void didSetBump(Slider *this) {

//	((View *) this)->editor->material.cm.this->bump = (vec_t) this->value;

	printf("Bump: %0.3f\n", this->value);
}

/**
 * @brief SliderDelegate callback for changing this->hardness.
 */
static void didSetHardness(Slider *this) {

	printf("Hardness: %0.3f\n", this->value);
}

/**
 * @brief SliderDelegate callback for changing this->specular.
 */
static void didSetSpecular(Slider *this) {

	printf("Specular: %0.3f\n", this->value);
}

/**
 * @brief SliderDelegate callback for changing this->parallax.
 */
static void didSetParallax(Slider *this) {

	printf("Parallax: %0.3f\n", this->value);
}

#pragma mark - ViewController

/**
 * @see ViewController::loadView(ViewController *)
 */
static void loadView(ViewController *self) {

	super(ViewController, self, loadView);

	EditorViewController *this = (EditorViewController *) self;

	Panel *panel = $(alloc(Panel), initWithFrame, NULL);

	panel->stackView.view.alignment = ViewAlignmentMiddleCenter;

	StackView *columns = $(alloc(StackView), initWithFrame, NULL);

	columns->axis = StackViewAxisHorizontal;
	columns->spacing = DEFAULT_PANEL_SPACING;

	{
		StackView *column = $(alloc(StackView), initWithFrame, NULL);
		column->spacing = DEFAULT_PANEL_SPACING;

		{
			Box *box = $(alloc(Box), initWithFrame, NULL);
			$(box->label, setText, "MATERIAL");

			StackView *stackView = $(alloc(StackView), initWithFrame, NULL);

			// bump

			this->bumpSlider = $(alloc(Slider), initWithFrame, NULL, ControlStyleDefault);

			this->bumpSlider->delegate.didSetValue = didSetBump;

			this->bumpSlider->min = 0.0;
			this->bumpSlider->max = 20.0;
			this->bumpSlider->step = 0.125;

			$(this->bumpSlider, setValue, 1.0);

			addInput((View *) stackView, "Bump", (Control *) this->bumpSlider);

			// hardness

			this->hardnessSlider = $(alloc(Slider), initWithFrame, NULL, ControlStyleDefault);

			this->hardnessSlider->delegate.didSetValue = didSetHardness;

			this->hardnessSlider->min = 0.0;
			this->hardnessSlider->max = 20.0;
			this->hardnessSlider->step = 0.1;

			$(this->hardnessSlider, setValue, 1.0);

			addInput((View *) stackView, "Hardness", (Control *) this->hardnessSlider);

			// specular

			this->specularSlider = $(alloc(Slider), initWithFrame, NULL, ControlStyleDefault);

			this->specularSlider->delegate.didSetValue = didSetSpecular;

			this->specularSlider->min = 0.0;
			this->specularSlider->max = 20.0;
			this->specularSlider->step = 0.1;

			$(this->specularSlider, setValue, 1.0);

			addInput((View *) stackView, "Specular", (Control *) this->specularSlider);

			// parallax

			this->parallaxSlider = $(alloc(Slider), initWithFrame, NULL, ControlStyleDefault);

			this->parallaxSlider->delegate.didSetValue = didSetParallax;

			this->parallaxSlider->min = 0.0;
			this->parallaxSlider->max = 20.0;
			this->parallaxSlider->step = 0.1;

			$(this->parallaxSlider, setValue, 1.0);

			addInput((View *) stackView, "Parallax", (Control *) this->parallaxSlider);

			$((View *) box, addSubview, (View *) stackView);
			release(stackView);

			$((View *) column, addSubview, (View *) box);
			release(box);
		}

		$((View *) columns, addSubview, (View *) column);
		release(column);
	}

	$((View *) panel->contentView, addSubview, (View *) columns);
	release(columns);

	panel->accessoryView->view.hidden = false;
	addButton((View *) panel->accessoryView, "Save", saveAction, self, NULL);

	$(self->view, addSubview, (View *) panel);
	release(panel);
}

#pragma mark - EditorViewController

/**
 * @fn EditorViewController *EditorViewController::init(EditorViewController *self)
 *
 * @memberof EditorViewController
 */
static EditorViewController *init(EditorViewController *self) {

	return (EditorViewController *) super(ViewController, self, init);
}

#pragma mark - Class lifecycle

/**
 * @see Class::initialize(Class *)
 */
static void initialize(Class *clazz) {

	((ViewControllerInterface *) clazz->def->interface)->loadView = loadView;

	((EditorViewControllerInterface *) clazz->def->interface)->init = init;
}

/**
 * @fn Class *EditorViewController::_EditorViewController(void)
 * @memberof EditorViewController
 */
Class *_EditorViewController(void) {
	static Class clazz;
	static Once once;

	do_once(&once, {
		clazz.name = "EditorViewController";
		clazz.superclass = _NavigationViewController();
		clazz.instanceSize = sizeof(EditorViewController);
		clazz.interfaceOffset = offsetof(EditorViewController, interface);
		clazz.interfaceSize = sizeof(EditorViewControllerInterface);
		clazz.initialize = initialize;
	});

	return &clazz;
}

#undef _Class
