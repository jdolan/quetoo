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
#include "client.h"

#include "EditorViewController.h"
#include "EditorView.h"


#define _Class _EditorViewController

#pragma mark - Actions & Delegates

/**
 * @brief ActionFunction for the Save Button.
 */
static void saveAction(Control *control, const SDL_Event *event, ident sender, ident data) {

	printf("Save!\n");
}

/**
 * @brief SliderDelegate callback for changing bump.
 */
static void didSetValue(Slider *slider) {

	ViewController *this = (ViewController *) slider->delegate.self;
	EditorView *view = (EditorView *) this->view;

	if (!view->material) {
		return;
	}

	const vec_t value = (vec_t) view->bumpSlider->value;

	if (slider == view->bumpSlider) {
		view->material->cm->bump = value;
	} else if (slider == view->hardnessSlider) {
		view->material->cm->bump = value;
	} else if (slider == view->specularSlider) {
		view->material->cm->specular = value;
	} else if (slider == view->parallaxSlider) {
		view->material->cm->parallax = value;
	} else {
		Com_Debug(DEBUG_UI, "Unknown Slider %p\n", slider);
	}
}

#pragma mark - ViewController

/**
 * @see ViewController::loadView(ViewController *)
 */
static void loadView(ViewController *self) {

	super(ViewController, self, loadView);

	EditorViewController *this = (EditorViewController *) self;

	((MenuViewController *) this)->panel->stackView.view.alignment = ViewAlignmentMiddleCenter;

	EditorView *editorView = $(alloc(EditorView), initWithFrame, NULL);
	assert(editorView);

	editorView->bumpSlider->delegate.self = (ident *) this;
	editorView->bumpSlider->delegate.didSetValue = didSetValue;

	editorView->hardnessSlider->delegate.self = (ident *) this;
	editorView->hardnessSlider->delegate.didSetValue = didSetValue;

	editorView->specularSlider->delegate.self = (ident *) this;
	editorView->specularSlider->delegate.didSetValue = didSetValue;

	editorView->parallaxSlider->delegate.self = (ident *) this;
	editorView->parallaxSlider->delegate.didSetValue = didSetValue;

	$((View *) ((MenuViewController *) this)->panel->contentView, addSubview, (View *) editorView);
	release(editorView);

	((MenuViewController *) this)->panel->accessoryView->view.hidden = false;
	//addButton((View *) ((MenuViewController *) this)->panel->accessoryView, "Save", saveAction, self, NULL);

	$(self->view, addSubview, (View *) ((MenuViewController *) this)->panel);

	$((View *) this, updateBindings);
}

#pragma mark - EditorViewController

/**
 * @fn EditorViewController *EditorViewController::init(EditorViewController *self)
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
		clazz.superclass = _MenuViewController();
		clazz.instanceSize = sizeof(EditorViewController);
		clazz.interfaceOffset = offsetof(EditorViewController, interface);
		clazz.interfaceSize = sizeof(EditorViewControllerInterface);
		clazz.initialize = initialize;
	});

	return &clazz;
}

#undef _Class
