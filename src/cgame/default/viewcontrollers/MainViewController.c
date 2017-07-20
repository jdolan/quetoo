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

#include "HomeViewController.h"
#include "InfoViewController.h"
#include "PlayViewController.h"
#include "PlayerViewController.h"
#include "SettingsViewController.h"

#include "DialogView.h"

#define _Class _MainViewController

#pragma mark - Object

static void dealloc(Object *self) {

	MainViewController *this = (MainViewController *) self;

	release(this->decorationView);

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
 * @brief Disconnect from the current game.
 */
static void disconnectFunction(void) {

	cgi.Cbuf("disconnect\n");
}

/**
 * @brief ActionFunction for main menu PrimaryButtons.
 */
static void action(Control *control, const SDL_Event *event, ident sender, ident data) {

	MainViewController *self = (MainViewController *) sender;

	NavigationViewController *this = (NavigationViewController *) sender;

	Class *clazz = (Class *) data;

	if (clazz) {
		//$(this, popToRootViewController); FIXME See #482

		ViewController *viewController = $((ViewController *) _alloc(clazz), init);

		$(this, pushViewController, viewController);

		release(viewController);

	} else {
		if (self->state == CL_ACTIVE) {
			$(((MainViewController *) this)->dialog, showDialog,
				"Are you sure you want to disconnect?", "No", "Yes", disconnectFunction);
		} else {
			$(((MainViewController *) this)->dialog, showDialog,
				"Are you sure you want to quit?", "No", "Yes", quitFunction);
		}
	}
}

#pragma mark - ViewController

/**
 * @see ViewController::loadView(ViewController *)
 */
static void loadView(ViewController *self) {

	super(ViewController, self, loadView);

	MainViewController *this = (MainViewController *) self;

	// Menu decorations

	this->decorationView = $(alloc(View), initWithFrame, NULL);

	this->decorationView->autoresizingMask = ViewAutoresizingFill;

	// Menu background

	{
		ImageView *backgroundImage = $(alloc(ImageView), initWithFrame, NULL);
		assert(backgroundImage);

		SDL_Surface *surface;

		if (cgi.LoadSurface(va("ui/backgrounds/%d", Random() % 6), &surface)) {
			$(backgroundImage, setImageWithSurface, surface);
			SDL_FreeSurface(surface);
		} else {
			$(backgroundImage, setImage, NULL);
		}

		backgroundImage->view.autoresizingMask = ViewAutoresizingFill;

		$(this->decorationView, addSubview, (View *) backgroundImage);
		release(backgroundImage);
	}

	// Quetoo logo

	{
		const SDL_Rect frame = MakeRect(0, 0, 240, 110);

		ImageView *logoImage = $(alloc(ImageView), initWithFrame, &frame);
		assert(logoImage);

		SDL_Surface *surface;

		if (cgi.LoadSurface("ui/logo", &surface)) {
			$(logoImage, setImageWithSurface, surface);
			SDL_FreeSurface(surface);
		} else {
			$(logoImage, setImage, NULL);
		}

		logoImage->view.alignment = ViewAlignmentBottomRight;
		logoImage->view.autoresizingMask = ViewAutoresizingNone;

		$(this->decorationView, addSubview, (View *) logoImage);
		release(logoImage);
	}

	// Quetoo version watermark

	{
		Label *versionLabel = $(alloc(Label), initWithText, va("Quetoo %s", cgi.CvarGet("version")->string), NULL);
		assert(versionLabel);

		versionLabel->view.alignment = ViewAlignmentBottomLeft;
		versionLabel->view.autoresizingMask = ViewAutoresizingContain;

		versionLabel->text->color = QColors.Watermark;

		$(this->decorationView, addSubview, (View *) versionLabel);
		release(versionLabel);
	}

	$(self->view, addSubview, this->decorationView);

	// Top menu bar

	View *topBar = $(alloc(View), initWithFrame, NULL);

	topBar->alignment = ViewAlignmentTopCenter;
	topBar->autoresizingMask = ViewAutoresizingWidth;

	topBar->backgroundColor = QColors.MainHighlight;
	topBar->borderColor = QColors.BorderLight;

	topBar->frame.h = 30;

	topBar->padding.right = DEFAULT_PANEL_SPACING;
	topBar->padding.left = DEFAULT_PANEL_SPACING;

	topBar->zIndex = 50; // Just below dialogs and panels

	{
		StackView *row = $(alloc(StackView), initWithFrame, NULL);

		row->spacing = DEFAULT_PANEL_SPACING;

		row->axis = StackViewAxisHorizontal;
		row->distribution = StackViewDistributionDefault;

		row->view.alignment = ViewAlignmentTopLeft;
		row->view.autoresizingMask = ViewAutoresizingContain;

		{
			Cgui_PrimaryButton((View *) row, "HOME", QColors.Dark, action, self, _HomeViewController());
			Cgui_PrimaryButton((View *) row, "PROFILE", QColors.Dark, action, self, _PlayerViewController());
			Cgui_PrimaryButton((View *) row, "PLAY", QColors.Theme, action, self, _PlayViewController());
		}

		$(topBar, addSubview, (View *) row);
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
			Cgui_PrimaryIcon((View *) row, "ui/pics/settings", QColors.Dark, action, self, _SettingsViewController());
			Cgui_PrimaryIcon((View *) row, "ui/pics/info", QColors.Dark, action, self, _InfoViewController());
			Cgui_PrimaryIcon((View *) row, "ui/pics/quit",  QColors.Dark, action, self, NULL);
		}

		$(topBar, addSubview, (View *) row);
		release(row);
	}

	$(self->view, addSubview, (View *) topBar);
	release(topBar);

	// Bottom menu bar

	this->bottomBar = $(alloc(View), initWithFrame, NULL);

	this->bottomBar->alignment = ViewAlignmentBottomCenter;
	this->bottomBar->autoresizingMask = ViewAutoresizingContain;

	this->bottomBar->backgroundColor = QColors.MainHighlight;
	this->bottomBar->borderColor = QColors.BorderLight;

	this->bottomBar->padding.top = -6; // HAAAAAAAAAX!!!!
	this->bottomBar->padding.right = DEFAULT_PANEL_SPACING;
	this->bottomBar->padding.left = DEFAULT_PANEL_SPACING;

	this->bottomBar->zIndex = 50; // Just below dialogs and panels

	{
		StackView *row = $(alloc(StackView), initWithFrame, NULL);

		row->spacing = DEFAULT_PANEL_SPACING;

		row->axis = StackViewAxisHorizontal;
		row->distribution = StackViewDistributionDefault;

		row->view.alignment = ViewAlignmentBottomCenter;
		row->view.autoresizingMask = ViewAutoresizingContain;

		{
			Cgui_PrimaryButton((View *) row, "JOIN", QColors.Dark, action, self, _PlayerViewController());
			Cgui_PrimaryButton((View *) row, "VOTES", QColors.Dark, action, self, _PlayerViewController());
		}

		$(this->bottomBar, addSubview, (View *) row);
		release(row);
	}
	$(self->view, addSubview, (View *) this->bottomBar);
	release(this->bottomBar);

	action(NULL, NULL, self, _HomeViewController()); // Open home when the main menu is first opened

	// Dialog

	this->dialog = $(alloc(DialogView), init);
	$(self->view, addSubview, (View *) this->dialog);
}

/**
 * @see ViewController::viewWillAppear(ViewController *)
 */
static void viewWillAppear(ViewController *self) {

	MainViewController *this = (MainViewController *) self;

	const _Bool in_game = (this->state == CL_ACTIVE);

	this->decorationView->hidden = in_game;

	this->bottomBar->hidden = !in_game;
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
	((ViewControllerInterface *) clazz->def->interface)->viewWillAppear = viewWillAppear;

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
