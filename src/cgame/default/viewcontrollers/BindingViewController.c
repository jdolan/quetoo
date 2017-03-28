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

#include "BindingViewController.h"

#define _Class _BindingViewController

#pragma mark - ViewController

/**
 * @see ViewController::loadView(ViewController *)
 */
static void loadView(ViewController *self) {

	super(ViewController, self, loadView);

	TabViewController *this = (TabViewController *) self;

	this->view->backgroundColor = Colors.Red;

	{
		Box *box = $(alloc(Box), initWithFrame, NULL);
		$(box->label, setText, "Movement");

		StackView *stackView = $(alloc(StackView), initWithFrame, NULL);

		Cg_BindInput((View *) stackView, "Forward", "+forward");
		Cg_BindInput((View *) stackView, "Back", "+back");
		Cg_BindInput((View *) stackView, "Strafe left", "+move_left");
		Cg_BindInput((View *) stackView, "Strafe right", "+move_right");
		Cg_BindInput((View *) stackView, "Jump", "+move_up");
		Cg_BindInput((View *) stackView, "Crouch", "+move_down");

		Cg_BindInput((View *) stackView, "Run/walk", "+speed");
		Cg_CvarCheckboxInput((View *) stackView, "Always run", "cg_run");

		$((View *) box, addSubview, (View *) stackView);
		release(stackView);

		$(this->view, addSubview, (View *) box);
		release(box);
	}

	{
		Box *box = $(alloc(Box), initWithFrame, NULL);
		$(box->label, setText, "Multiplayer");

		StackView *stackView = $(alloc(StackView), initWithFrame, NULL);

		Cg_BindInput((View *) stackView, "Chat", "cl_message_mode");
		Cg_BindInput((View *) stackView, "Team chat", "cl_message_mode_2");

		Cg_BindInput((View *) stackView, "Scoreboard", "+score");

		Cg_BindInput((View *) stackView, "Screenshot", "r_screenshot");

		Cg_BindInput((View *) stackView, "Toggle console", "cl_toggle_console");

		$((View *) box, addSubview, (View *) stackView);
		release(stackView);

		$(this->view, addSubview, (View *) box);
		release(box);
	}

	{
		Box *box = $(alloc(Box), initWithFrame, NULL);
		$(box->label, setText, "Combat");

		StackView *stackView = $(alloc(StackView), initWithFrame, NULL);

		Cg_BindInput((View *) stackView, "Attack", "+attack");

		Cg_BindInput((View *) stackView, "Zoom", "+ZOOM");

		Cg_BindInput((View *) stackView, "Grapple", "+hook");

		Cg_BindInput((View *) stackView, "Next weapon", "cg_weapon_next");
		Cg_BindInput((View *) stackView, "Previous weapon", "cg_weapon_previous");

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

		$(this->view, addSubview, (View *) box);
		release(box);
	}
}

#pragma mark - Class lifecycle

/**
 * @see Class::initialize(Class *)
 */
static void initialize(Class *clazz) {

	((ViewControllerInterface *) clazz->def->interface)->loadView = loadView;
}

/**
 * @fn Class *BindingViewController::_BindingViewController(void)
 * @memberof BindingViewController
 */
Class *_BindingViewController(void) {
	static Class clazz;
	static Once once;

	do_once(&once, {
		clazz.name = "BindingViewController";
		clazz.superclass = _TabViewController();
		clazz.instanceSize = sizeof(BindingViewController);
		clazz.interfaceOffset = offsetof(BindingViewController, interface);
		clazz.interfaceSize = sizeof(BindingViewControllerInterface);
		clazz.initialize = initialize;
	});

	return &clazz;
}

#undef _Class
