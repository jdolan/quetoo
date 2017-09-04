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

#include "InputView.h"

#define _Class _InputView

#pragma mark - InputView

/**
 * @fn InputView *InputView::initWithFrame(InputView *self, const SDL_Rect *frame)
 *
 * @memberof InputView
 */
 static InputView *initWithFrame(InputView *self, const SDL_Rect *frame) {

	self = (InputView *) super(View, self, initWithFrame, frame);
	if (self) {

		StackView *columns = $(alloc(StackView), initWithFrame, NULL);

		columns->spacing = DEFAULT_PANEL_SPACING;

		columns->axis = StackViewAxisHorizontal;
		columns->distribution = StackViewDistributionFillEqually;

		columns->view.autoresizingMask = ViewAutoresizingFill;

		{
			Box *box = $(alloc(Box), initWithFrame, NULL);
			$(box->label->text, setText, "Controls");

			box->view.autoresizingMask = ViewAutoresizingFill;

			ScrollView *scrollView = $(alloc(ScrollView), initWithFrame, NULL, ControlStyleDefault);

			scrollView->control.view.autoresizingMask = ViewAutoresizingFill; // Works except it snaps back to the top randomly
			scrollView->control.view.backgroundColor = Colors.Red;

			StackView *stackView = $(alloc(StackView), initWithFrame, NULL);

			stackView->view.autoresizingMask |= ViewAutoresizingWidth;
			stackView->view.backgroundColor = Colors.Green;

                        Cgui_Label((View *) stackView, "Movement");

			Cgui_BindInput((View *) stackView, "Forward", "+forward");
			Cgui_BindInput((View *) stackView, "Back", "+back");
			Cgui_BindInput((View *) stackView, "Strafe left", "+move_left");
			Cgui_BindInput((View *) stackView, "Strafe right", "+move_right");
			Cgui_BindInput((View *) stackView, "Jump", "+move_up");
			Cgui_BindInput((View *) stackView, "Crouch", "+move_down");

			Cgui_BindInput((View *) stackView, "Run/walk", "+speed");

			Cgui_Label((View *) stackView, "Communication");

			Cgui_BindInput((View *) stackView, "Chat", "cl_message_mode");
			Cgui_BindInput((View *) stackView, "Team chat", "cl_message_mode_2");

			Cgui_Label((View *) stackView, "Interface");

			Cgui_BindInput((View *) stackView, "Scoreboard", "+score");

			Cgui_BindInput((View *) stackView, "Screenshot", "r_screenshot");

			Cgui_BindInput((View *) stackView, "Toggle console", "cl_toggle_console");

			Cgui_Label((View *) stackView, "Combat");

			Cgui_BindInput((View *) stackView, "Attack", "+attack");

			Cgui_BindInput((View *) stackView, "Zoom", "+ZOOM");

			Cgui_BindInput((View *) stackView, "Grapple", "+hook");

			Cgui_Label((View *) stackView, "Weapons");

			Cgui_BindInput((View *) stackView, "Next weapon", "cg_weapon_next");
			Cgui_BindInput((View *) stackView, "Previous weapon", "cg_weapon_previous");

			Cgui_BindInput((View *) stackView, "Blaster", "use blaster");
			Cgui_BindInput((View *) stackView, "Shotgun", "use shotgun");
			Cgui_BindInput((View *) stackView, "Super shotgun", "use super shotgun");
			Cgui_BindInput((View *) stackView, "Machinegun", "use machinegun");
			Cgui_BindInput((View *) stackView, "Hand grenades", "use hand grenades");
			Cgui_BindInput((View *) stackView, "Grenade launcher", "use grenade launcher");
			Cgui_BindInput((View *) stackView, "Rocket launcher", "use rocket launcher");
			Cgui_BindInput((View *) stackView, "Hyperblaster", "use hyperblaster");
			Cgui_BindInput((View *) stackView, "Lightning", "use lightning gun");
			Cgui_BindInput((View *) stackView, "Railgun", "use railgun");
			Cgui_BindInput((View *) stackView, "BFG-10K", "use bfg10k");

			$(scrollView, setContentView, (View *) stackView);

			$((View *) scrollView, addSubview, (View *) stackView);
			release(stackView);

			$((View *) box, addSubview, (View *) scrollView);
			release(scrollView);

			$((View *) columns, addSubview, (View *) box);
			release(box);
		}

		{
			StackView *column = $(alloc(StackView), initWithFrame, NULL);

			column->spacing = DEFAULT_PANEL_SPACING;

			{
				Box *box = $(alloc(Box), initWithFrame, NULL);
				$(box->label->text, setText, "Mouse");

				box->view.autoresizingMask |= ViewAutoresizingWidth;

				StackView *stackView = $(alloc(StackView), initWithFrame, NULL);

				Cgui_CvarSliderInput((View *) stackView, "Mouse sensitivity", "m_sensitivity", 0.1, 8.0, 0.1);

				Cgui_CvarSliderInput((View *) stackView, "Zoom sensitivity", "m_sensitivity_zoom", 0.1, 8.0, 0.1);

				Cgui_CvarCheckboxInput((View *) stackView, "Invert mouse", "m_invert");
				Cgui_CvarCheckboxInput((View *) stackView, "Smooth mouse", "m_interpolate");

				$((View *) box, addSubview, (View *) stackView);
				release(stackView);

				$((View *) column, addSubview, (View *) box);
				release(box);
			}

			{
				Box *box = $(alloc(Box), initWithFrame, NULL);
				$(box->label->text, setText, "Misc");

				box->view.autoresizingMask |= ViewAutoresizingWidth;

				StackView *stackView = $(alloc(StackView), initWithFrame, NULL);

				Cgui_CvarCheckboxInput((View *) stackView, "Always run", "cg_run");

				$((View *) box, addSubview, (View *) stackView);
				release(stackView);

				$((View *) column, addSubview, (View *) box);
				release(box);
			}

			$((View *) columns, addSubview, (View *) column);
			release(column);
		}

		$((View *) self, addSubview, (View *) columns);
		release(columns);
	}

	return self;
}

#pragma mark - Class lifecycle

/**
 * @see Class::initialize(Class *)
 */
static void initialize(Class *clazz) {

	((InputViewInterface *) clazz->def->interface)->initWithFrame = initWithFrame;
}

/**
 * @fn Class *InputView::_InputView(void)
 * @memberof InputView
 */
Class *_InputView(void) {
	static Class clazz;
	static Once once;

	do_once(&once, {
		clazz.name = "InputView";
		clazz.superclass = _View();
		clazz.instanceSize = sizeof(InputView);
		clazz.interfaceOffset = offsetof(InputView, interface);
		clazz.interfaceSize = sizeof(InputViewInterface);
		clazz.initialize = initialize;
	});

	return &clazz;
}

#undef _Class
