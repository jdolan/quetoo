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

#include "InfoViewController.h"

#include "CreditsView.h"

#define _Class _InfoViewController

#pragma mark - Object

static void dealloc(Object *self) {

	InfoViewController *this = (InfoViewController *) self;

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

	((InfoViewController *) this)->tabView = $(alloc(TabView), initWithFrame, &frame);
	TabView *tabView = ((InfoViewController *) this)->tabView;

	tabView->tabPageView->view.autoresizingMask = ViewAutoresizingFill;

	// Tab buttons

	{
		{
			CreditsView *tabData = $(alloc(CreditsView), initWithFrame, NULL);

			tabData->view.autoresizingMask = ViewAutoresizingFill;
			tabData->view.identifier = strdup("credits");

			TabViewItem *tab = $(alloc(TabViewItem), initWithView, (View *) tabData);

			$(tab->label->text, setText, "Credits");

			$(tabView, addTab, tab);
		}
	}

	{
		View *row = $(alloc(View), initWithFrame, NULL);

		row->autoresizingMask = ViewAutoresizingNone;

		row->frame.w = Min(900, cgi.context->window_width - 30);
		row->frame.h = Min(500, cgi.context->window_height - 80);

		// Add the TabView

		$(row, addSubview, (View *) tabView);

		$((View *) this->panel->contentView, addSubview, (View *) row);
		release(row);
	}
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
 * @fn Class *InfoViewController::_InfoViewController(void)
 * @memberof InfoViewController
 */
Class *_InfoViewController(void) {
	static Class clazz;
	static Once once;

	do_once(&once, {
		clazz.name = "InfoViewController";
		clazz.superclass = _MenuViewController();
		clazz.instanceSize = sizeof(InfoViewController);
		clazz.interfaceOffset = offsetof(InfoViewController, interface);
		clazz.interfaceSize = sizeof(InfoViewControllerInterface);
		clazz.initialize = initialize;
	});

	return &clazz;
}

#undef _Class
