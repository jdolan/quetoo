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
#include "InputViewController.h"
#include "InterfaceViewController.h"
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

	this->panel->isDraggable = false;
	this->panel->isResizable = false;

	this->panel->stackView.view.zIndex = 100;

	this->panel->stackView.view.autoresizingMask = ViewAutoresizingContain;

	this->panel->stackView.view.padding.top = 0;
	this->panel->stackView.view.padding.right = 0;
	this->panel->stackView.view.padding.bottom = 0;
	this->panel->stackView.view.padding.left = 0;

	this->panel->contentView->view.clipsSubviews = true;

	// Setup the NavigationViewController

	((SettingsViewController *) this)->navigationViewController = $(alloc(NavigationViewController), init);
	NavigationViewController *nvc = ((SettingsViewController *) this)->navigationViewController;

	$((ViewController *) nvc, moveToParentViewController, self);

	// Rows

	StackView *rows = $(alloc(StackView), initWithFrame, NULL);

	{
		StackView *row = $(alloc(StackView), initWithFrame, NULL);

		row->spacing = DEFAULT_PANEL_SPACING;

		row->axis = StackViewAxisHorizontal;
		row->distribution = StackViewDistributionFillEqually;

		row->view.backgroundColor = QColors.MainHighlight;

		row->view.autoresizingMask |= ViewAutoresizingWidth;

		row->view.padding.top = DEFAULT_PANEL_SPACING;
		row->view.padding.right = DEFAULT_PANEL_SPACING;
		row->view.padding.bottom = DEFAULT_PANEL_SPACING;
		row->view.padding.left = DEFAULT_PANEL_SPACING;

		{
			// Tab buttons

			{
				Cg_PrimaryButton((View *) row, "Input", ViewAlignmentTopLeft, QColors.Dark, tabAction, nvc, _InputViewController());
				Cg_PrimaryButton((View *) row, "Video", ViewAlignmentTopLeft, QColors.Dark, tabAction, nvc, _VideoViewController());
				Cg_PrimaryButton((View *) row, "Audio", ViewAlignmentTopLeft, QColors.Dark, tabAction, nvc, _AudioViewController());
				Cg_PrimaryButton((View *) row, "Interface", ViewAlignmentTopLeft, QColors.Dark, tabAction, nvc, _InterfaceViewController());
			}
		}

		$((View *) rows, addSubview, (View *) row);
		release(row);
	}

	{
		View *row = $(alloc(View), initWithFrame, NULL);

		row->autoresizingMask = ViewAutoresizingNone;

		row->frame.w = Min(900, cgi.context->window_width - 30);
		row->frame.h = Min(500, cgi.context->window_height - 80);

		// NavigationViewController's tab view

		$(row, addSubview, nvc->viewController.view);

		// Shadow

		Cg_Picture(row, "shadow_s", ViewAlignmentTopLeft, ViewAutoresizingWidth);

		$((View *) rows, addSubview, row);
		release(row);
	}

	$((View *) this->panel->contentView, addSubview, (View *) rows);
	release(rows);
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
