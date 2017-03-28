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

#include "PlayViewController.h"

#include "CreateServerViewController.h"
#include "MultiplayerViewController.h"
#include "QuickJoinViewController.h"

#define _Class _PlayViewController

#pragma mark - Object

static void dealloc(Object *self) {

	PlayViewController *this = (PlayViewController *) self;

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

	((PlayViewController *) this)->navigationViewController = $(alloc(NavigationViewController), init);
	NavigationViewController *nvc = ((PlayViewController *) this)->navigationViewController;

	StackView *columns = $(alloc(StackView), initWithFrame, NULL);

	columns->axis = StackViewAxisHorizontal;
	columns->spacing = DEFAULT_PANEL_SPACING;

	{
		StackView *column = $(alloc(StackView), initWithFrame, NULL);
		column->spacing = DEFAULT_PANEL_SPACING;

		column->view.backgroundColor = QColors.MainHighlight;

		{
			Cg_PrimaryButton((View *) column, "Quick join", ViewAlignmentTopLeft, QColors.Theme, tabAction, nvc, _QuickJoinViewController());
			Cg_PrimaryButton((View *) column, "Create server", ViewAlignmentTopLeft, QColors.Border, tabAction, nvc, _CreateServerViewController());
			Cg_PrimaryButton((View *) column, "Server browser", ViewAlignmentTopLeft, QColors.Border, tabAction, nvc, _MultiplayerViewController());
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
 * @fn Class *PlayViewController::_PlayViewController(void)
 * @memberof PlayViewController
 */
Class *_PlayViewController(void) {
	static Class clazz;
	static Once once;

	do_once(&once, {
		clazz.name = "PlayViewController";
		clazz.superclass = _MenuViewController();
		clazz.instanceSize = sizeof(PlayViewController);
		clazz.interfaceOffset = offsetof(PlayViewController, interface);
		clazz.interfaceSize = sizeof(PlayViewControllerInterface);
		clazz.initialize = initialize;
	});

	return &clazz;
}

#undef _Class
