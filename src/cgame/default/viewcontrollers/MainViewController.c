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
		$(((MainViewController *) this)->dialog, showDialog, "Are you sure you're done pwning nubz?'", quitFunction);
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

	// Menu bar

	Panel *panel = $(alloc(Panel), initWithFrame, NULL);

	((View *) panel)->backgroundColor = QColors.Main;

	panel->isDraggable = false;
	panel->isResizable = false;

	panel->stackView.distribution = StackViewAxisHorizontal;
	panel->stackView.spacing = 0;

	panel->stackView.view.alignment = ViewAlignmentTopLeft;
	panel->stackView.view.autoresizingMask = ViewAutoresizingNone;

	{
		panel->contentView->axis = StackViewAxisHorizontal;
		panel->contentView->distribution = StackViewDistributionDefault;

		Cg_PrimaryButton((View *) panel->contentView, "PROFILE", ViewAlignmentTopLeft, QColors.Theme, action, self, _PlayerViewController());

		Cg_PrimaryButton((View *) panel->contentView, "PLAY", ViewAlignmentTopLeft, QColors.Theme, action, self, _PlayViewController());

		Cg_PrimaryIcon((View *) panel->contentView, "ui/pics/settings", ViewAlignmentTopRight, QColors.Border, action, self, _SettingsViewController());
		Cg_PrimaryIcon((View *) panel->contentView, "ui/pics/quit", ViewAlignmentTopRight,  QColors.Border,action, self, NULL);
	}

	SDL_Size size = MakeSize(cgi.context->window_width, 36);
	$((View *) (StackView *) panel, resize, &size);

	$(self->view, addSubview, (View *) panel);
	release(panel);

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
