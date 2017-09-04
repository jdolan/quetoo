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

#include "HomeViewController.h"

#define _Class _HomeViewController

#pragma mark - ViewController

/**
 * @see ViewController::loadView(ViewController *)
 */
static void loadView(ViewController *self) {

	super(ViewController, self, loadView);

	MenuViewController *this = (MenuViewController *) self;

	StackView *columns = $(alloc(StackView), initWithFrame, NULL);
	columns->spacing = DEFAULT_PANEL_SPACING;
	
	columns->axis = StackViewAxisHorizontal;
	columns->distribution = StackViewDistributionFill;

	{
		StackView *column = $(alloc(StackView), initWithFrame, NULL);
		column->spacing = DEFAULT_PANEL_SPACING;

		{
			const SDL_Rect frame = MakeRect(0, 0, 700, 400);

			Box *box = $(alloc(Box), initWithFrame, &frame);
			$(box->label->text, setText, "News");

			box->view.autoresizingMask = ViewAutoresizingNone;

			Cgui_Label((View *) box->contentView, "Good news everybody! There's now a news feed that is completely hardcoded...");

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
 * @fn Class *HomeViewController::_HomeViewController(void)
 * @memberof HomeViewController
 */
Class *_HomeViewController(void) {
	static Class clazz;
	static Once once;

	do_once(&once, {
		clazz.name = "HomeViewController";
		clazz.superclass = _MenuViewController();
		clazz.instanceSize = sizeof(HomeViewController);
		clazz.interfaceOffset = offsetof(HomeViewController, interface);
		clazz.interfaceSize = sizeof(HomeViewControllerInterface);
		clazz.initialize = initialize;
	});

	return &clazz;
}

#undef _Class
