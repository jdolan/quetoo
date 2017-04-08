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

#include "MainViewController.h"

#include "CreateServerViewController.h"
#include "HomeViewController.h"
#include "MultiplayerViewController.h"
#include "PlayViewController.h"
#include "PlayerViewController.h"
#include "SettingsViewController.h"

#include "DialogView.h"
#include "PrimaryButton.h"

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

	Panel *panel = $(alloc(Panel), initWithFrame, NULL);

	panel->isDraggable = false;
	panel->isResizable = false;

	panel->stackView.spacing = 0;
	panel->stackView.axis = StackViewAxisHorizontal;
	panel->stackView.distribution = StackViewDistributionFillEqually;

	panel->stackView.view.needsLayout = true;

	panel->stackView.view.frame.h = 36;

	panel->stackView.view.padding.top = 0;
	panel->stackView.view.padding.bottom = 0;

	panel->stackView.view.backgroundColor = QColors.Main;
	panel->stackView.view.zIndex = 50; // Just below dialogs and panels

	panel->stackView.view.alignment = ViewAlignmentTopCenter;
	panel->stackView.view.autoresizingMask = ViewAutoresizingWidth;

	panel->stackView.view.backgroundColor = QColors.Main;
	panel->stackView.view.borderColor = Colors.Clear;

	{
		panel->contentView->axis = StackViewAxisHorizontal;
		panel->contentView->distribution = StackViewDistributionDefault;

		panel->contentView->view.alignment = ViewAlignmentTopLeft;
		panel->contentView->view.autoresizingMask = ViewAutoresizingContain;

		Cg_PrimaryButton((View *) panel->contentView, "HOME", ViewAlignmentTopLeft, QColors.Dark, action, self, _HomeViewController());

		Cg_PrimaryButton((View *) panel->contentView, "PROFILE", ViewAlignmentTopLeft, QColors.Dark, action, self, _PlayerViewController());

		Cg_PrimaryButton((View *) panel->contentView, "PLAY", ViewAlignmentTopLeft, QColors.Theme, action, self, _PlayViewController());
	{

	}
		panel->accessoryView->axis = StackViewAxisHorizontal;
		panel->accessoryView->distribution = StackViewDistributionDefault;

		panel->accessoryView->view.hidden = false;

		panel->accessoryView->view.alignment = ViewAlignmentTopRight;
		panel->accessoryView->view.autoresizingMask = ViewAutoresizingContain;

		Cg_PrimaryIcon((View *) panel->accessoryView, "ui/pics/settings", ViewAlignmentTopRight, QColors.Dark, action, self, _SettingsViewController());
		Cg_PrimaryIcon((View *) panel->accessoryView, "ui/pics/quit", ViewAlignmentTopRight,  QColors.Dark,action, self, NULL);
	}

	$(self->view, addSubview, (View *) panel);
	release(panel);

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
