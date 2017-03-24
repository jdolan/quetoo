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

#pragma mark - Actions

/**
 * @brief ActionFunction for settings tabs.
 */
static void tabAction(Control *control, const SDL_Event *event, ident sender, ident data) {

	NavigationViewController *this = (NavigationViewController *) sender;
	Class *clazz = (Class *) data;

	$(this, popToRootViewController);

	ViewController *viewController = $((ViewController *) _alloc(clazz), init);

	$(this, pushViewController, viewController); // seg faults

	release(viewController);
}

#pragma mark - ViewController

/**
 * @see ViewController::loadView(ViewController *)
 */
static void loadView(ViewController *self) {

	super(ViewController, self, loadView);

	MenuViewController *this = (MenuViewController *) self;

	// pretty sure this->navigationViewController is initialized incorrectly, ewverything using it seems to seg fault
	((SettingsViewController *) this)->navigationViewController = $(alloc(NavigationViewController), init);
	NavigationViewController *nvc = ((SettingsViewController *) this)->navigationViewController;

	StackView *columns = $(alloc(StackView), initWithFrame, NULL);

	columns->axis = StackViewAxisHorizontal;
	columns->spacing = DEFAULT_PANEL_SPACING;

	{
		StackView *column = $(alloc(StackView), initWithFrame, NULL);
		column->spacing = DEFAULT_PANEL_SPACING;

		{
			Cg_PrimaryButton((View *) column, "Bindings", ViewAlignmentTopLeft, Colors.DefaultColor, tabAction, nvc, _BindingViewController());
			Cg_PrimaryButton((View *) column, "Input", ViewAlignmentTopLeft, Colors.DefaultColor, tabAction, nvc, _InputViewController());
			Cg_PrimaryButton((View *) column, "Video", ViewAlignmentTopLeft, Colors.DefaultColor, tabAction, nvc, _VideoViewController());
			Cg_PrimaryButton((View *) column, "Audio", ViewAlignmentTopLeft, Colors.DefaultColor, tabAction, nvc, _AudioViewController());
			Cg_PrimaryButton((View *) column, "Misc", ViewAlignmentTopLeft, Colors.DefaultColor, tabAction, nvc, _MiscViewController());
		}

		$((View *) columns, addSubview, (View *) column);
		release(column);
	}

	$((View *) columns, addSubview, (View *) nvc); // seg faults right now

	$((View *) this->panel->contentView, addSubview, (View *) columns);
	release(columns);

	this->panel->accessoryView->view.hidden = false;
}

#pragma mark - Class lifecycle

/**
 * @see Class::initialize(Class *)
 */
static void initialize(Class *clazz) {

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
