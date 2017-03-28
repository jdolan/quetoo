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

#include "CvarSlider.h"

#define _Class _InputViewController

#pragma mark - ViewController

/**
 * @see ViewController::loadView(ViewController *)
 */
static void loadView(ViewController *self) {

	super(ViewController, self, loadView);

	TabViewController *this = (TabViewController *) self;

	{
		Box *box = $(alloc(Box), initWithFrame, NULL);
		$(box->label, setText, "Mouse");

		StackView *stackView = $(alloc(StackView), initWithFrame, NULL);

		Cg_CvarSliderInput((View *) stackView, "Sensitivity", "m_sensitivity", 0.1, 10.0, 0.1);
		Cg_CvarSliderInput((View *) stackView, "Zoom sensitivity", "m_sensitivity_zoom", 0.1, 10.0, 0.1);
		Cg_CvarCheckboxInput((View *) stackView, "Invert mouse", "m_invert");
		Cg_CvarCheckboxInput((View *) stackView, "Smooth mouse", "m_interpolate");

		$((View *) box, addSubview, (View *) stackView);
		release(stackView);

		$((View *) this->stackView, addSubview, (View *) box);
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
 * @fn Class *InputViewController::_InputViewController(void)
 * @memberof InputViewController
 */
Class *_InputViewController(void) {
	static Class clazz;
	static Once once;

	do_once(&once, {
		clazz.name = "InputViewController";
		clazz.superclass = _TabViewController();
		clazz.instanceSize = sizeof(InputViewController);
		clazz.interfaceOffset = offsetof(InputViewController, interface);
		clazz.interfaceSize = sizeof(InputViewControllerInterface);
		clazz.initialize = initialize;
	});

	return &clazz;
}

#undef _Class
