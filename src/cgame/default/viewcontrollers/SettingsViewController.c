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

#include "AudioView.h"
#include "InputView.h"
#include "InterfaceView.h"
#include "VideoView.h"

#define _Class _SettingsViewController

#pragma mark - Object

static void dealloc(Object *self) {

	SettingsViewController *this = (SettingsViewController *) self;

	release(this->tabView);

	super(Object, self, dealloc);
}

#pragma mark - ViewController

/**
 * @see ViewController::loadView(ViewController *)
 */
static void loadView(ViewController *self) {

	super(ViewController, self, loadView);

	MenuViewController *this = (MenuViewController *) self;

	this->panel->stackView.view.padding.top = 0;
	this->panel->stackView.view.padding.right = 0;
	this->panel->stackView.view.padding.bottom = 0;
	this->panel->stackView.view.padding.left = 0;

	this->panel->stackView.view.zIndex = 100;

	this->panel->contentView->view.clipsSubviews = true;

	// Setup the TabView

	const SDL_Rect frame = MakeRect(0, 0, 900, 500);

	((SettingsViewController *) this)->tabView = $(alloc(TabView), initWithFrame, &frame);
	TabView *tabView = ((SettingsViewController *) this)->tabView;

	tabView->tabPageView->view.autoresizingMask = ViewAutoresizingFill;

	// Tab buttons

	{

		{
			VideoView *tabData = $(alloc(VideoView), initWithFrame, NULL);

			tabData->view.autoresizingMask = ViewAutoresizingFill;
			tabData->view.identifier = "video";

			TabViewItem *tab = $(alloc(TabViewItem), initWithView, (View *) tabData);

			$(tab->label->text, setText, "Video");

			$(tabView, addTab, tab);
		}

		{
			InputView *tabData = $(alloc(InputView), initWithFrame, NULL);

			tabData->view.autoresizingMask = ViewAutoresizingFill;
			tabData->view.identifier = "input";

			TabViewItem *tab = $(alloc(TabViewItem), initWithView, (View *) tabData);

			$(tab->label->text, setText, "Input");

			$(tabView, addTab, tab);
		}

		{
			InterfaceView *tabData = $(alloc(InterfaceView), initWithFrame, NULL);

			tabData->view.autoresizingMask = ViewAutoresizingFill;
			tabData->view.identifier = "interface";

			TabViewItem *tab = $(alloc(TabViewItem), initWithView, (View *) tabData);

			$(tab->label->text, setText, "Interface");

			$(tabView, addTab, tab);
		}

		{
			AudioView *tabData = $(alloc(AudioView), initWithFrame, NULL);

			tabData->view.autoresizingMask = ViewAutoresizingFill;
			tabData->view.identifier = "audio";

			TabViewItem *tab = $(alloc(TabViewItem), initWithView, (View *) tabData);

			$(tab->label->text, setText, "Audio");

			$(tabView, addTab, tab);
		}
	}

	$((View *) this->panel->contentView, addSubview, (View *) tabView);
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
