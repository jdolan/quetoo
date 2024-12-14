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
 * along with self program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */

#include <assert.h>

#include "ui_local.h"
#include "client.h"

#include "EditorView.h"

#define _Class _EditorView

#pragma mark - EditorView

/**
 * @fn EditorView *EditorView::initWithFrame(EditorView *self, const SDL_Rect *frame)
 * @memberof EditorView
 */
static EditorView *initWithFrame(EditorView *self, const SDL_Rect *frame) {

	self = (EditorView *) super(View, self, initWithFrame, frame);
	if (self) {

		$((View *) self, awakeWithResourceName, "ui/editor/EditorView.json");

		TabView *tabs = NULL;

		Outlet outlets[] = MakeOutlets(
			MakeOutlet("title", &self->title),
			MakeOutlet("tabs", &tabs),
			MakeOutlet("diffusemap", &self->diffusemap),
			MakeOutlet("normalmap", &self->normalmap),
			MakeOutlet("specularmap", &self->specularmap),
			MakeOutlet("roughness", &self->roughness),
			MakeOutlet("parallax_amplitude", &self->parallaxAmplitude),
			MakeOutlet("parallax_bias", &self->parallaxBias),
			MakeOutlet("parallax_exponent", &self->parallaxExponent),
			MakeOutlet("hardness", &self->hardness),
			MakeOutlet("specularity", &self->specularity),
			MakeOutlet("bloom", &self->bloom),
			MakeOutlet("alpha_test", &self->alphaTest),
		    MakeOutlet("light_radius", &self->lightRadius),
		    MakeOutlet("light_intensity", &self->lightIntensity),
			MakeOutlet("light_cone", &self->lightCone),
			MakeOutlet("save", &self->save)
		);

		$((View *) self, resolve, outlets);

		self->view.stylesheet = $$(Stylesheet, stylesheetWithResourceName, "ui/editor/EditorView.css");
		assert(self->view.stylesheet);

		TabViewItem *surface = $(tabs, tabWithIdentifier, "surface");
		$(tabs, selectTab, surface);
	}

	return self;
}

/**
 * @fn EditorView::setMaterial(EditorView *self, r_material_t *material)
 * @memberof EditorView
 */
static void setMaterial(EditorView *self, r_material_t *material) {

	self->material = material;

	if (self->material) {
		$(self->title->text, setTextWithFormat, "Editing %s", self->material->cm->basename);
		$(self->diffusemap, setDefaultText, self->material->cm->diffusemap.name);
		$(self->normalmap, setDefaultText, self->material->cm->normalmap.name);
		$(self->specularmap, setDefaultText, self->material->cm->specularmap.name);

		$(self->roughness, setValue, (double) self->material->cm->roughness);
		$(self->parallaxAmplitude, setValue, (double) self->material->cm->parallax.amplitude);
		$(self->parallaxBias, setValue, (double) self->material->cm->parallax.bias);
		$(self->parallaxExponent, setValue, (double) self->material->cm->parallax.exponent);
		$(self->hardness, setValue, (double) self->material->cm->hardness);
		$(self->specularity, setValue, (double) self->material->cm->specularity);
		$(self->bloom, setValue, (double) self->material->cm->bloom);

		if (material->cm->surface & SURF_ALPHA_TEST) {
			$(self->alphaTest, setValue, (double) self->material->cm->alpha_test);
			self->alphaTest->control.state &= ~ControlStateDisabled;
		} else {
			$(self->alphaTest, setValue, MATERIAL_ALPHA_TEST);
			self->alphaTest->control.state |= ControlStateDisabled;
		}

		$(self->lightRadius, setValue, (double) self->material->cm->light.radius);
		$(self->lightIntensity, setValue, (double) self->material->cm->light.intensity);
		$(self->lightCone, setValue, (double) self->material->cm->light.cone);

	} else {
		$(self->title->text, setText, "Select a material with your crosshair");
		$(self->diffusemap, setDefaultText, NULL);
		$(self->normalmap, setDefaultText, NULL);
		$(self->specularmap, setDefaultText, NULL);

		$(self->roughness, setValue, MATERIAL_ROUGHNESS);
		$(self->parallaxAmplitude, setValue, MATERIAL_PARALLAX_AMPLITUDE);
		$(self->parallaxBias, setValue, MATERIAL_PARALLAX_BIAS);
		$(self->parallaxExponent, setValue, MATERIAL_PARALLAX_EXPONENT);
		$(self->hardness, setValue, MATERIAL_HARDNESS);
		$(self->specularity, setValue, MATERIAL_SPECULARITY);
		$(self->bloom, setValue, MATERIAL_BLOOM);
		$(self->alphaTest, setValue, MATERIAL_ALPHA_TEST);
		$(self->lightRadius, setValue, MATERIAL_LIGHT_RADIUS);
		$(self->lightIntensity, setValue, MATERIAL_LIGHT_INTENSITY);
		$(self->lightIntensity, setValue, MATERIAL_LIGHT_CONE);
	}
}

#pragma mark - Class lifecycle

/**
 * @see Class::initialize(Class *)
 */
static void initialize(Class *clazz) {

	((EditorViewInterface *) clazz->interface)->initWithFrame = initWithFrame;
	((EditorViewInterface *) clazz->interface)->setMaterial = setMaterial;
}

/**
 * @fn Class *EditorView::_EditorView(void)
 * @memberof EditorView
 */
Class *_EditorView(void) {
	static Class *clazz;
	static Once once;

	do_once(&once, {
		clazz = _initialize(&(const ClassDef) {
			.name = "EditorView",
			.superclass = _View(),
			.instanceSize = sizeof(EditorView),
			.interfaceOffset = offsetof(EditorView, interface),
			.interfaceSize = sizeof(EditorViewInterface),
			.initialize = initialize,
		});
	});

	return clazz;
}

#undef _Class
