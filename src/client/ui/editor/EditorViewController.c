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

	EditorViewController *this = sender;

	if (this->model == NULL) {
		Com_Debug(DEBUG_UI, "Not editing a material\n");
		return;
	}

	Cmd_ExecuteString(va("r_save_materials %s", this->model->media.name));
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
		view->material->cm->roughness = slider->value;
	} else if (slider == view->parallaxAmplitude) {
		view->material->cm->parallax.amplitude = slider->value;
	} else if (slider == view->parallaxBias) {
		view->material->cm->parallax.bias = slider->value;
	} else if (slider == view->parallaxExponent) {
		view->material->cm->parallax.exponent = slider->value;
	} else if (slider == view->hardness) {
		view->material->cm->hardness = slider->value;
	} else if (slider == view->specularity) {
		view->material->cm->specularity = slider->value;
	} else if (slider == view->bloom) {
		view->material->cm->bloom = slider->value;
	} else if (slider == view->alphaTest) {
		view->material->cm->alpha_test = slider->value;
	} else if (slider == view->lightRadius) {
		view->material->cm->light.radius = slider->value;
	} else if (slider == view->lightIntensity) {
		view->material->cm->light.intensity = slider->value;
	} else if (slider == view->lightCone) {
		view->material->cm->light.cone = slider->value;
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

	view->parallaxAmplitude->delegate.self = self;
	view->parallaxAmplitude->delegate.didSetValue = didSetValue;

	view->parallaxBias->delegate.self = self;
	view->parallaxBias->delegate.didSetValue = didSetValue;

	view->parallaxExponent->delegate.self = self;
	view->parallaxExponent->delegate.didSetValue = didSetValue;

	view->hardness->delegate.self = self;
	view->hardness->delegate.didSetValue = didSetValue;

	view->specularity->delegate.self = self;
	view->specularity->delegate.didSetValue = didSetValue;

	view->bloom->delegate.self = self;
	view->bloom->delegate.didSetValue = didSetValue;

	view->alphaTest->delegate.self = self;
	view->alphaTest->delegate.didSetValue = didSetValue;

	view->lightRadius->delegate.self = self;
	view->lightRadius->delegate.didSetValue = didSetValue;

	view->lightIntensity->delegate.self = self;
	view->lightIntensity->delegate.didSetValue = didSetValue;

	view->lightCone->delegate.self = self;
	view->lightCone->delegate.didSetValue = didSetValue;

	$((Control *) view->save, addActionForEventType, SDL_MOUSEBUTTONUP, saveAction, self, NULL);

	$(self, setView, (View *) view);
	release(view);
}

/**
 * @see ViewController::viewWillAppear(ViewController *)
 */
static void viewWillAppear(ViewController *self) {

	EditorViewController *this = (EditorViewController *) self;

	this->material = NULL;
	this->model = NULL;

	EditorView *view = (EditorView *) self->view;

	float distance = MAX_WORLD_DIST;

	vec3_t start = Vec3_Fmaf(cl_view.origin, 16.f, cl_view.forward);
	vec3_t end = Vec3_Fmaf(start, MAX_WORLD_DIST, cl_view.forward);

	while (this->material == NULL) {

		const cm_trace_t tr = Cl_Trace(start, end, Box3_Zero(), 0, CONTENTS_MASK_VISIBLE);
		if (!tr.material) {
			break;
		}

		this->material = R_LoadMaterial(tr.material->name, ASSET_CONTEXT_TEXTURES);
		this->model = R_WorldModel();

		distance = Vec3_Distance(cl_view.origin, tr.end);
	}

	const r_entity_t *e = cl_view.entities;
	for (int32_t i = 0; i < cl_view.num_entities; i++, e++) {

		if (e->model == NULL) {
			continue;
		}

		if (e->model->type != MODEL_MESH) {
			continue;
		}

		if (e->model->mesh->faces->material == NULL) {
			continue;
		}

		if (e->effects & (EF_SELF | EF_WEAPON)) {
			continue;
		}

		const int32_t head_node = Cm_SetBoxHull(e->abs_model_bounds, CONTENTS_SOLID);

		const cm_trace_t tr = Cm_BoxTrace(cl_view.origin, end, Box3_Zero(), head_node, CONTENTS_SOLID);
		if (tr.fraction < 1.f) {

			const float dist = Vec3_Distance(cl_view.origin, tr.end);
			if (dist < distance) {
				this->material = e->model->mesh->faces->material;
				this->model = e->model;

				distance = dist;

			}
		}
	}

	$(view, setMaterial, this->material);

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
