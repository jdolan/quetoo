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

#include "ClientMenuViewController.h"

#define _Class _ClientMenuViewController

#pragma mark - Object

static void dealloc(Object *self) {

	ClientMenuViewController *this = (ClientMenuViewController *) self;

	release(this->panel);

	super(Object, self, dealloc);
}

#pragma mark - ViewController

/**
 * @see ViewController::loadView(ViewController *)
 */
static void loadView(ViewController *self) {

	super(ViewController, self, loadView);

	ClientMenuViewController *this = (ClientMenuViewController *) self;

	this->panel = $(alloc(Panel), initWithFrame, NULL);
	assert(this->panel);

	this->panel->stackView.view.alignment = ViewAlignmentMiddleCenter;
	this->panel->stackView.view.needsLayout = true;

	$(self->view, addSubview, (View *) this->panel);
}

#pragma mark - ClientMenuViewController

/**
 * @fn EditorViewController *ClientMenuViewController::editorViewController(const ClientMenuViewController *self)
 *
 * @memberof ClientMenuViewController
 */
static EditorViewController *editorViewController(const ClientMenuViewController *self) {
	return (EditorViewController *) ((ViewController *) self)->parentViewController;
}

#pragma mark - Class lifecycle

/**
 * @see Class::initialize(Class *)
 */
static void initialize(Class *clazz) {

	((ObjectInterface *) clazz->def->interface)->dealloc = dealloc;

	((ViewControllerInterface *) clazz->def->interface)->loadView = loadView;

	((ClientMenuViewControllerInterface *) clazz->def->interface)->editorViewController = editorViewController;
}

/**
 * @fn Class *ClientMenuViewController::_ClientMenuViewController(void)
 * @memberof ClientMenuViewController
 */
Class *_ClientMenuViewController(void) {
	static Class clazz;
	static Once once;

	do_once(&once, {
		clazz.name = "ClientMenuViewController";
		clazz.superclass = _ViewController();
		clazz.instanceSize = sizeof(ClientMenuViewController);
		clazz.interfaceOffset = offsetof(ClientMenuViewController, interface);
		clazz.interfaceSize = sizeof(ClientMenuViewControllerInterface);
		clazz.initialize = initialize;
	});

	return &clazz;
}

#undef _Class
