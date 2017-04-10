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

#include <ObjectivelyMVC/View.h>

#include "MainViewController.h"

#include "CreateServerViewController.h"
#include "HomeViewController.h"
#include "PlayViewController.h"
#include "PlayerViewController.h"
#include "SettingsViewController.h"

#include "DialogView.h"

#define _Class _MainViewController

#pragma mark - Object

static void dealloc(Object *self) {

	MainViewController *this = (MainViewController *) self;

	release(this->backgroundImage);
	release(this->logoImage);

	release(this->dialog);

	super(Object, self, dealloc);
}

#pragma mark - Actions

/**
 * @brief Quit the game.
 */
static void quitFunction(void) {

	cgi.Cbuf("quit\n");
}

/**
 * @brief ActionFunction for main menu PrimaryButtons.
 */
static void action(Control *control, const SDL_Event *event, ident sender, ident data) {

	NavigationViewController *this = (NavigationViewController *) sender;
	Class *clazz = (Class *) data;
	if (clazz) {
		$(this, popToRootViewController);

		ViewController *viewController = $((ViewController *) _alloc(clazz), init);

		$(this, pushViewController, viewController);

		release(viewController);

	} else {
		$(((MainViewController *) this)->dialog, showDialog, "Are you sure you want to quit?", "No", "Yes", quitFunction);
	}
}

#pragma mark - ViewController

/**
 * @see ViewController::loadView(ViewController *)
 */
static void loadView(ViewController *self) {

	super(ViewController, self, loadView);

	MainViewController *this = (MainViewController *) self;

	// Menu background

	this->backgroundImage = $(alloc(ImageView), initWithFrame, NULL);
	assert(this->backgroundImage);

	SDL_Surface *surface;

	if (cgi.LoadSurface("ui/background", &surface)) {
		$(this->backgroundImage, setImageWithSurface, surface);
		SDL_FreeSurface(surface);
	} else {
		$(this->backgroundImage, setImage, NULL);
	}

	this->backgroundImage->view.alignment = ViewAlignmentTopLeft;
	this->backgroundImage->view.autoresizingMask = ViewAutoresizingFill;

	$(self->view, addSubview, (View *) this->backgroundImage);

	// Quetoo logo

	const SDL_Rect frame = MakeRect(0, 0, 240, 110);

	this->logoImage = $(alloc(ImageView), initWithFrame, &frame);
	assert(this->logoImage);

	if (cgi.LoadSurface("ui/logo", &surface)) {
		$(this->logoImage, setImageWithSurface, surface);
		SDL_FreeSurface(surface);
	} else {
		$(this->logoImage, setImage, NULL);
	}

	this->logoImage->view.alignment = ViewAlignmentBottomRight;
	this->logoImage->view.autoresizingMask = ViewAutoresizingNone;

	$(self->view, addSubview, (View *) this->logoImage);

	// Quetoo version watermark

	Label *versionLabel = $(alloc(Label), initWithText, va("Quetoo %s", cgi.CvarGet("version")->string), NULL);
	assert(versionLabel);

	versionLabel->view.alignment = ViewAlignmentBottomLeft;
	versionLabel->view.autoresizingMask = ViewAutoresizingContain;

	versionLabel->text->color = QColors.Watermark;

	$(self->view, addSubview, (View *) versionLabel);
	release(versionLabel);

	// Menu bar

	View *view = $(alloc(View), initWithFrame, NULL);

	view->alignment = ViewAlignmentTopCenter;
	view->autoresizingMask = ViewAutoresizingWidth;

	view->backgroundColor = QColors.Main;
	view->borderColor = QColors.BorderLight;

	view->frame.h = 30;

	view->padding.right = DEFAULT_PANEL_SPACING;
	view->padding.left = DEFAULT_PANEL_SPACING;

	view->zIndex = 50; // Just below dialogs and panels

	{
		StackView *row = $(alloc(StackView), initWithFrame, NULL);

		row->spacing = DEFAULT_PANEL_SPACING;

		row->axis = StackViewAxisHorizontal;
		row->distribution = StackViewDistributionDefault;

		row->view.alignment = ViewAlignmentTopLeft;
		row->view.autoresizingMask = ViewAutoresizingContain;

		{
			Cg_PrimaryButton((View *) row, "HOME", ViewAlignmentTopLeft, QColors.Dark, action, self, _HomeViewController());
			Cg_PrimaryButton((View *) row, "PROFILE", ViewAlignmentTopLeft, QColors.Dark, action, self, _PlayerViewController());
			Cg_PrimaryButton((View *) row, "PLAY", ViewAlignmentTopLeft, QColors.Theme, action, self, _PlayViewController());
		}

		$(view, addSubview, (View *) row);
		release(row);
	}

	{
		StackView *row = $(alloc(StackView), initWithFrame, NULL);

		row->spacing = DEFAULT_PANEL_SPACING;

		row->axis = StackViewAxisHorizontal;
		row->distribution = StackViewDistributionDefault;

		row->view.alignment = ViewAlignmentTopRight;
		row->view.autoresizingMask = ViewAutoresizingContain;

		{
			Cg_PrimaryIcon((View *) row, "ui/pics/settings", ViewAlignmentTopRight, QColors.Dark, action, self, _SettingsViewController());
			Cg_PrimaryIcon((View *) row, "ui/pics/quit", ViewAlignmentTopRight,  QColors.Dark,action, self, NULL);
		}

		$(view, addSubview, (View *) row);
		release(row);
	}

	$(self->view, addSubview, (View *) view);
	release(view);

	action(NULL, NULL, self, _HomeViewController()); // Open home when the main menu is first opened

	// Dialog

	this->dialog = $(alloc(DialogView), init);
	$(self->view, addSubview, (View *) this->dialog);
}

#pragma mark - MainViewController

/**
 * @fn MainViewController *MainViewController::init(MainViewController *self)
 *
 * @memberof MainViewController
 */
static MainViewController *init(MainViewController *self) {

	return (MainViewController *) super(ViewController, self, init);
}

#pragma mark - Class lifecycle

/**
 * @see Class::initialize(Class *)
 */
static void initialize(Class *clazz) {

	((ObjectInterface *) clazz->def->interface)->dealloc = dealloc;

	((ViewControllerInterface *) clazz->def->interface)->loadView = loadView;

	((MainViewControllerInterface *) clazz->def->interface)->init = init;
}

/**
 * @fn Class *MainViewController::_MainViewController(void)
 * @memberof MainViewController
 */
Class *_MainViewController(void) {
	static Class clazz;
	static Once once;

	do_once(&once, {
		clazz.name = "MainViewController";
		clazz.superclass = _NavigationViewController();
		clazz.instanceSize = sizeof(MainViewController);
		clazz.interfaceOffset = offsetof(MainViewController, interface);
		clazz.interfaceSize = sizeof(MainViewControllerInterface);
		clazz.initialize = initialize;
	});

	return &clazz;
}

#undef _Class
