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

#include "KeysViewController.h"

#define _Class _KeysViewController

#pragma mark - ViewController

/**
 * @see ViewController::loadView(ViewController *)
 */
static void loadView(ViewController *self) {

	super(ViewController, self, loadView);

	MenuViewController *this = (MenuViewController *) self;

	StackView *columns = $(alloc(StackView), initWithFrame, NULL);

	columns->axis = StackViewAxisHorizontal;
	columns->spacing = DEFAULT_PANEL_SPACING;

	{
		StackView *column = $(alloc(StackView), initWithFrame, NULL);
		column->spacing = DEFAULT_PANEL_SPACING;

		{
			Box *box = $(alloc(Box), initWithFrame, NULL);
			$(box->label->text, setText, "MOVEMENT");

			StackView *stackView = $(alloc(StackView), initWithFrame, NULL);

			Cg_BindInput((View *) stackView, "Forward", "+forward");
			Cg_BindInput((View *) stackView, "Back", "+back");
			Cg_BindInput((View *) stackView, "Move left", "+move_left");
			Cg_BindInput((View *) stackView, "Move right", "+move_right");
			Cg_BindInput((View *) stackView, "Jump", "+move_up");
			Cg_BindInput((View *) stackView, "Crouch", "+move_down");
			Cg_BindInput((View *) stackView, "Turn left", "+left");
			Cg_BindInput((View *) stackView, "Turn right", "+right");
			Cg_BindInput((View *) stackView, "Center view", "center_view");
			Cg_BindInput((View *) stackView, "Run / walk", "+speed");
			Cg_CvarCheckboxInput((View *) stackView, "Always Run", "cg_run");

			$((View *) box, addSubview, (View *) stackView);
			release(stackView);

			$((View *) column, addSubview, (View *) box);
			release(box);
		}

		{
			Box *box = $(alloc(Box), initWithFrame, NULL);
			$(box->label->text, setText, "COMMUNICATIONS");

			StackView *stackView = $(alloc(StackView), initWithFrame, NULL);

			Cg_BindInput((View *) stackView, "Say", "cl_message_mode");
			Cg_BindInput((View *) stackView, "Say Team", "cl_message_mode_2");
			Cg_BindInput((View *) stackView, "Show score", "+score");
			Cg_BindInput((View *) stackView, "Take screenshot", "r_screenshot");

			$((View *) box, addSubview, (View *) stackView);
			release(stackView);

			$((View *) column, addSubview, (View *) box);
			release(box);
		}

		$((View *) columns, addSubview, (View *) column);
		release(column);
	}

	{
		StackView *column = $(alloc(StackView), initWithFrame, NULL);
		column->spacing = DEFAULT_PANEL_SPACING;

		{
			Box *box = $(alloc(Box), initWithFrame, NULL);
			$(box->label->text, setText, "COMBAT");

			StackView *stackView = $(alloc(StackView), initWithFrame, NULL);

			Cg_BindInput((View *) stackView, "Attack", "+attack");
			Cg_BindInput((View *) stackView, "Grapple Hook", "+hook");
			Cg_BindInput((View *) stackView, "Next weapon", "cg_weapon_next");
			Cg_BindInput((View *) stackView, "Previous weapon", "cg_weapon_previous");
			Cg_BindInput((View *) stackView, "Zoom", "+ZOOM");

			Cg_BindInput((View *) stackView, "Blaster", "use blaster");
			Cg_BindInput((View *) stackView, "Shotgun", "use shotgun");
			Cg_BindInput((View *) stackView, "Super shotgun", "use super shotgun");
			Cg_BindInput((View *) stackView, "Machinegun", "use machinegun");
			Cg_BindInput((View *) stackView, "Hand grenades", "use hand grenades");
			Cg_BindInput((View *) stackView, "Grenade launcher", "use grenade launcher");
			Cg_BindInput((View *) stackView, "Rocket launcher", "use rocket launcher");
			Cg_BindInput((View *) stackView, "Hyperblaster", "use hyperblaster");
			Cg_BindInput((View *) stackView, "Lightning", "use lightning gun");
			Cg_BindInput((View *) stackView, "Railgun", "use railgun");
			Cg_BindInput((View *) stackView, "BFG-10K", "use bfg10k");

			$((View *) box, addSubview, (View *) stackView);
			release(stackView);

			$((View *) column, addSubview, (View *) box);
			release(box);
		}

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

	((ViewControllerInterface *) clazz->def->interface)->loadView = loadView;
}

/**
 * @fn Class *KeysViewController::_KeysViewController(void)
 * @memberof KeysViewController
 */
Class *_KeysViewController(void) {
	static Class clazz;
	static Once once;

	do_once(&once, {
		clazz.name = "KeysViewController";
		clazz.superclass = _MenuViewController();
		clazz.instanceSize = sizeof(KeysViewController);
		clazz.interfaceOffset = offsetof(KeysViewController, interface);
		clazz.interfaceSize = sizeof(KeysViewControllerInterface);
		clazz.initialize = initialize;
	});

	return &clazz;
}

#undef _Class

