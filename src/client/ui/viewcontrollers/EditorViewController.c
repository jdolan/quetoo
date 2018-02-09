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

#pragma mark - Actions and delegate callbacks

/**
 * @brief ActionFunction for the Save Button.
 */
static void saveAction(Control *control, const SDL_Event *event, ident sender, ident data) {
	Cmd_ExecuteString("r_save_materials");
}

/**
 * @brief SliderDelegate callback for changing bump.
 */
static void didSetValue(Slider *slider) {

	EditorViewController *this = (EditorViewController *) slider->delegate.self;
	EditorView *view = (EditorView *) this->viewController.view;

	if (!view->material) {
		return;
	}

	if (slider == view->bumpSlider) {
		view->material->cm->bump = view->bumpSlider->value;
	} else if (slider == view->hardnessSlider) {
		view->material->cm->hardness = view->hardnessSlider->value;
	} else if (slider == view->specularSlider) {
		view->material->cm->specular = view->specularSlider->value;
	} else if (slider == view->parallaxSlider) {
		view->material->cm->parallax = view->parallaxSlider->value;
	} else {
		Com_Debug(DEBUG_UI, "Unknown Slider %p\n", (void *) slider);
	}
}

#pragma mark - ViewController

/**
 * @see ViewController::loadView(ViewController *)
 */
static void loadView(ViewController *self) {

	super(ViewController, self, loadView);

	EditorView *view = $(alloc(EditorView), initWithFrame, NULL);
	assert(view);

	view->bumpSlider->delegate.self = self;
	view->bumpSlider->delegate.didSetValue = didSetValue;

	view->hardnessSlider->delegate.self = self;
	view->hardnessSlider->delegate.didSetValue = didSetValue;

	view->specularSlider->delegate.self = self;
	view->specularSlider->delegate.didSetValue = didSetValue;

	view->parallaxSlider->delegate.self = self;
	view->parallaxSlider->delegate.didSetValue = didSetValue;

	$((Control *) view->saveButton, addActionForEventType, SDL_MOUSEBUTTONUP, saveAction, self, NULL);

	$(self, setView, (View *) view);
	release(view);
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

	((ViewControllerInterface *) clazz->interface)->loadView = loadView;

	((EditorViewControllerInterface *) clazz->interface)->init = init;
}

/**
 * @fn Class *EditorViewController::_EditorViewController(void)
 * @memberof EditorViewController
 */
Class *_EditorViewController(void) {
	static Class *clazz;
	static Once once;

	do_once(&once, {
		clazz = _initialize(&(const ClassDef) {
			.name = "EditorViewController",
			.superclass = _ViewController(),
			.instanceSize = sizeof(EditorViewController),
			.interfaceOffset = offsetof(EditorViewController, interface),
			.interfaceSize = sizeof(EditorViewControllerInterface),
			.initialize = initialize,
		});
	});

	return clazz;
}

#undef _Class
