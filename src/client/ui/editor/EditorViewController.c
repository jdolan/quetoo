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
static void didSetValue(Slider *slider, double value) {

	EditorViewController *this = (EditorViewController *) slider->delegate.self;
	EditorView *view = (EditorView *) this->viewController.view;

	if (!view->material) {
		return;
	}

	if (slider == view->roughness) {
		view->material->cm->roughness = view->roughness->value;
	} else if (slider == view->hardness) {
		view->material->cm->hardness = view->hardness->value;
	} else if (slider == view->specularity) {
		view->material->cm->specularity = view->specularity->value;
	} else if (slider == view->parallax) {
		view->material->cm->parallax = view->parallax->value;
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

	view->roughness->delegate.self = self;
	view->roughness->delegate.didSetValue = didSetValue;

	view->hardness->delegate.self = self;
	view->hardness->delegate.didSetValue = didSetValue;

	view->specularity->delegate.self = self;
	view->specularity->delegate.didSetValue = didSetValue;

	view->parallax->delegate.self = self;
	view->parallax->delegate.didSetValue = didSetValue;

	$((Control *) view->save, addActionForEventType, SDL_MOUSEBUTTONUP, saveAction, self, NULL);

	$(self, setView, (View *) view);
	release(view);
}

/**
 * @see ViewController::viewWillAppear(ViewController *)
 */
static void viewWillAppear(ViewController *self) {

	EditorView *view = (EditorView *) self->view;

	r_material_t *material = NULL;

	vec3_t end;
	end = Vec3_Add(r_view.origin, Vec3_Scale(r_view.forward, MAX_WORLD_DIST));

	const cm_trace_t tr = Cl_Trace(r_view.origin, end, Vec3_Zero(), Vec3_Zero(), 0, CONTENTS_MASK_VISIBLE);
	if (tr.texinfo && tr.texinfo->material) {
		material = R_LoadMaterial(tr.texinfo->name, ASSET_CONTEXT_TEXTURES);
	}

	$(view, setMaterial, material);

	super(ViewController, self, viewWillAppear);
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
	((ViewControllerInterface *) clazz->interface)->viewWillAppear = viewWillAppear;

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
