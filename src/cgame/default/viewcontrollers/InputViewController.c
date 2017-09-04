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

#include "InputViewController.h"

#define _Class _InputViewController

#pragma mark - ViewController


/**
 * @see ViewController::loadView(ViewController *)
 */
static void loadView(ViewController *self) {

	super(ViewController, self, loadView);

	self->view->autoresizingMask = ViewAutoresizingContain;
	self->view->identifier = strdup("Input");

	StackView *stackView = $(alloc(StackView), initWithFrame, NULL);
	stackView->spacing = DEFAULT_PANEL_SPACING;

	{
		StackView *columns = $(alloc(StackView), initWithFrame, NULL);
		columns->spacing = DEFAULT_PANEL_SPACING;

		columns->axis = StackViewAxisHorizontal;
		columns->distribution = StackViewDistributionFillEqually;

		{
			StackView *column = $(alloc(StackView), initWithFrame, NULL);
			column->spacing = DEFAULT_PANEL_SPACING;

			{
				Box *box = $(alloc(Box), initWithFrame, NULL);
				$(box->label->text, setText, "Movement");

				Cgui_BindInput((View *) box->contentView, "Forward", "+forward");
				Cgui_BindInput((View *) box->contentView, "Back", "+back");
				Cgui_BindInput((View *) box->contentView, "Move left", "+move_left");
				Cgui_BindInput((View *) box->contentView, "Move right", "+move_right");
				Cgui_BindInput((View *) box->contentView, "Jump", "+move_up");
				Cgui_BindInput((View *) box->contentView, "Crouch", "+move_down");
				Cgui_BindInput((View *) box->contentView, "Turn left", "+left");
				Cgui_BindInput((View *) box->contentView, "Turn right", "+right");
				Cgui_BindInput((View *) box->contentView, "Center view", "center_view");
				Cgui_BindInput((View *) box->contentView, "Run / walk", "+speed");
				Cgui_CvarCheckboxInput((View *) box->contentView, "Always Run", "cg_run");

				$((View *) column, addSubview, (View *) box);
				release(box);
			}

			{
				Box *box = $(alloc(Box), initWithFrame, NULL);
				$(box->label->text, setText, "Communication");

				Cgui_BindInput((View *) box->contentView, "Say", "cl_message_mode");
				Cgui_BindInput((View *) box->contentView, "Say Team", "cl_message_mode_2");
				Cgui_BindInput((View *) box->contentView, "Show score", "+score");
				Cgui_BindInput((View *) box->contentView, "Take screenshot", "r_screenshot");

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
				$(box->label->text, setText, "Combat");

				Cgui_BindInput((View *) box->contentView, "Attack", "+attack");
				Cgui_BindInput((View *) box->contentView, "Grapple Hook", "+hook");
				Cgui_BindInput((View *) box->contentView, "Next weapon", "cg_weapon_next");
				Cgui_BindInput((View *) box->contentView, "Previous weapon", "cg_weapon_previous");
				Cgui_BindInput((View *) box->contentView, "Zoom", "+ZOOM");

				Cgui_BindInput((View *) box->contentView, "Blaster", "use blaster");
				Cgui_BindInput((View *) box->contentView, "Shotgun", "use shotgun");
				Cgui_BindInput((View *) box->contentView, "Super shotgun", "use super shotgun");
				Cgui_BindInput((View *) box->contentView, "Machinegun", "use machinegun");
				Cgui_BindInput((View *) box->contentView, "Hand grenades", "use hand grenades");
				Cgui_BindInput((View *) box->contentView, "Grenade launcher", "use grenade launcher");
				Cgui_BindInput((View *) box->contentView, "Rocket launcher", "use rocket launcher");
				Cgui_BindInput((View *) box->contentView, "Hyperblaster", "use hyperblaster");
				Cgui_BindInput((View *) box->contentView, "Lightning", "use lightning gun");
				Cgui_BindInput((View *) box->contentView, "Railgun", "use railgun");
				Cgui_BindInput((View *) box->contentView, "BFG-10K", "use bfg10k");

				$((View *) column, addSubview, (View *) box);
				release(box);
			}
			
			$((View *) columns, addSubview, (View *) column);
			release(column);
		}

		$((View *) stackView, addSubview, (View *) columns);
		release(columns);
	}

	$(self->view, addSubview, (View *) stackView);
	release(stackView);
}

#pragma mark - Class lifecycle

/**
 * @see Class::initialize(Class *)
 */
static void initialize(Class *clazz) {
	((ViewControllerInterface *) clazz->def->interface)->loadView = loadView;
}

/**
 * @fn Class *InputViewController::_InputViewController(void)
 * @memberof InputViewController
 */
Class *_InputViewController(void) {
	static Class clazz;
	static Once once;

	do_once(&once, {
		clazz.name = "InputViewController";
		clazz.superclass = _ViewController();
		clazz.instanceSize = sizeof(InputViewController);
		clazz.interfaceOffset = offsetof(InputViewController, interface);
		clazz.interfaceSize = sizeof(InputViewControllerInterface);
		clazz.initialize = initialize;
	});

	return &clazz;
}

#undef _Class
