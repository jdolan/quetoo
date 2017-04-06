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

#include "TabViewController.h"

#define _Class _TabViewController

#pragma mark - Object

static void dealloc(Object *self) {

	TabViewController *this = (TabViewController *) self;

	release(this->panel);

	super(Object, self, dealloc);
}

#pragma mark - ViewController

/**
 * @see ViewController::loadView(ViewController *)
 */
static void loadView(ViewController *self) {

	super(ViewController, self, loadView);

	TabViewController *this = (TabViewController *) self;

	this->panel = $(alloc(Panel), initWithFrame, NULL);
	assert(this->panel);

	this->panel->isDraggable = false;
	this->panel->isResizable = false;

	this->panel->stackView.spacing = DEFAULT_PANEL_SPACING;

	this->panel->stackView.view.needsLayout = true;

	this->panel->stackView.view.backgroundColor = Colors.Clear;
	this->panel->stackView.view.borderColor = QColors.BorderLight;

	this->panel->stackView.view.alignment = ViewAlignmentTopLeft;
	this->panel->stackView.view.autoresizingMask = ViewAutoresizingFill;

	this->panel->stackView.distribution = StackViewDistributionFill;

	this->panel->contentView->view.autoresizingMask = ViewAutoresizingFill;

	this->panel->accessoryView->view.alignment = ViewAlignmentBottomRight;

	$(self->view, addSubview, (View *) this->panel);
}

#pragma mark - TabViewController

/**
 * @fn MainViewController *TabViewController::mainViewController(const TabViewController *self)
 *
 * @memberof TabViewController
 */
static MainViewController *mainViewController(const TabViewController *self) {
	return (MainViewController *) ((ViewController *) self)->parentViewController;
}

#pragma mark - Class lifecycle

/**
 * @see Class::initialize(Class *)
 */
static void initialize(Class *clazz) {

	((ObjectInterface *) clazz->def->interface)->dealloc = dealloc;

	((ViewControllerInterface *) clazz->def->interface)->loadView = loadView;

	((TabViewControllerInterface *) clazz->def->interface)->mainViewController = mainViewController;
}

/**
 * @fn Class *TabViewController::_TabViewController(void)
 * @memberof TabViewController
 */
Class *_TabViewController(void) {
	static Class clazz;
	static Once once;

	do_once(&once, {
		clazz.name = "TabViewController";
		clazz.superclass = _ViewController();
		clazz.instanceSize = sizeof(TabViewController);
		clazz.interfaceOffset = offsetof(TabViewController, interface);
		clazz.interfaceSize = sizeof(TabViewControllerInterface);
		clazz.initialize = initialize;
	});

	return &clazz;
}

#undef _Class
