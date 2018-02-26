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

		Outlet outlets[] = MakeOutlets(
			MakeOutlet("name", &self->name),
			MakeOutlet("diffuse", &self->diffuse),
			MakeOutlet("normalmap", &self->normalmap),
			MakeOutlet("specularmap", &self->specularmap),
			MakeOutlet("bumpmap", &self->bump),
			MakeOutlet("hardness", &self->hardness),
			MakeOutlet("specular", &self->specular),
			MakeOutlet("parallax", &self->parallax),
			MakeOutlet("save", &self->save)
		);

		$((View *) self, resolve, outlets);

		self->view.stylesheet = $$(Stylesheet, stylesheetWithResourceName, "ui/editor/EditorView.css");
		assert(self->view.stylesheet);
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
		$(self->name, setDefaultText, self->material->cm->basename);
		$(self->diffuse, setDefaultText, self->material->cm->diffuse.name);
		$(self->normalmap, setDefaultText, self->material->cm->normalmap.name);
		$(self->specularmap, setDefaultText, self->material->cm->specularmap.name);

		$(self->bump, setValue, (double) self->material->cm->bump);
		$(self->hardness, setValue, (double) self->material->cm->hardness);
		$(self->specular, setValue, (double) self->material->cm->specular);
		$(self->parallax, setValue, (double) self->material->cm->parallax);
	} else {
		$(self->name, setDefaultText, NULL);
		$(self->diffuse, setDefaultText, NULL);
		$(self->normalmap, setDefaultText, NULL);
		$(self->specularmap, setDefaultText, NULL);

		$(self->bump, setValue, DEFAULT_BUMP);
		$(self->hardness, setValue, DEFAULT_HARDNESS);
		$(self->specular, setValue, DEFAULT_SPECULAR);
		$(self->parallax, setValue, DEFAULT_PARALLAX);
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
