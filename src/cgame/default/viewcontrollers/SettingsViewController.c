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

#include "SettingsViewController.h"

#include "AudioViewController.h"
#include "BindingViewController.h"
#include "InputViewController.h"
#include "MiscViewController.h"
#include "VideoViewController.h"

#define _Class _SettingsViewController

#pragma mark - Object

static void dealloc(Object *self) {

	SettingsViewController *this = (SettingsViewController *) self;

	release(this->navigationViewController);

	super(Object, self, dealloc);
}

#pragma mark - Actions

/**
 * @brief ActionFunction for settings tabs.
 */
static void tabAction(Control *control, const SDL_Event *event, ident sender, ident data) {

	NavigationViewController *this = (NavigationViewController *) sender;
	Class *clazz = (Class *) data;

	$(this, popToRootViewController);

	ViewController *viewController = $((ViewController *) _alloc(clazz), init);

	$(this, pushViewController, viewController);

	release(viewController);
}

#pragma mark - ViewController

/**
 * @see ViewController::loadView(ViewController *)
 */
static void loadView(ViewController *self) {

	super(ViewController, self, loadView);

	MenuViewController *this = (MenuViewController *) self;

	this->panel->stackView.view.frame.w = 900;
	this->panel->stackView.view.frame.h = 550;

	this->panel->stackView.view.needsLayout = false;
	this->panel->stackView.view.clipsSubviews = true;

	this->panel->stackView.view.autoresizingMask = ViewAutoresizingNone;

	((SettingsViewController *) this)->navigationViewController = $(alloc(NavigationViewController), init);
	NavigationViewController *nvc = ((SettingsViewController *) this)->navigationViewController;

	StackView *columns = $(alloc(StackView), initWithFrame, NULL);

	columns->axis = StackViewAxisHorizontal;
	columns->spacing = DEFAULT_PANEL_SPACING;

	{
		StackView *column = $(alloc(StackView), initWithFrame, NULL);
		column->spacing = DEFAULT_PANEL_SPACING;

		column->view.backgroundColor = QColors.MainHighlight;

		{
			Cg_PrimaryButton((View *) column, "Bindings", ViewAlignmentTopLeft, QColors.Border, tabAction, nvc, _BindingViewController());
			Cg_PrimaryButton((View *) column, "Input", ViewAlignmentTopLeft, QColors.Border, tabAction, nvc, _InputViewController());
			Cg_PrimaryButton((View *) column, "Video", ViewAlignmentTopLeft, QColors.Border, tabAction, nvc, _VideoViewController());
			Cg_PrimaryButton((View *) column, "Audio", ViewAlignmentTopLeft, QColors.Border, tabAction, nvc, _AudioViewController());
			Cg_PrimaryButton((View *) column, "Misc", ViewAlignmentTopLeft, QColors.Border, tabAction, nvc, _MiscViewController());
		}

		$((View *) columns, addSubview, (View *) column);
		release(column);
	}

	$((ViewController *) nvc, moveToParentViewController, self);

	{
		StackView *column = $(alloc(StackView), initWithFrame, NULL);
		column->spacing = DEFAULT_PANEL_SPACING;

		column->view.backgroundColor = QColors.Main;

		$((View *) column, addSubview, nvc->viewController.view);

		$((View *) columns, addSubview, (View *) column);
		release(column);
	}

	$((View *) this->panel->contentView, addSubview, (View *) columns);
	release(columns);
}

#pragma mark - Class lifecycle

/**
 * @see Class::initialize(Class *)
 */
static void initialize(Class *clazz) {

	((ObjectInterface *) clazz->def->interface)->dealloc = dealloc;

	((ViewControllerInterface *) clazz->def->interface)->loadView = loadView;
}

/**
 * @fn Class *SettingsViewController::_SettingsViewController(void)
 * @memberof SettingsViewController
 */
Class *_SettingsViewController(void) {
	static Class clazz;
	static Once once;

	do_once(&once, {
		clazz.name = "SettingsViewController";
		clazz.superclass = _MenuViewController();
		clazz.instanceSize = sizeof(SettingsViewController);
		clazz.interfaceOffset = offsetof(SettingsViewController, interface);
		clazz.interfaceSize = sizeof(SettingsViewControllerInterface);
		clazz.initialize = initialize;
	});

	return &clazz;
}

#undef _Class
