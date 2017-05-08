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

#include "CreateServerView.h"
#include "QuickJoinView.h"
#include "ServerBrowserView.h"

#define _Class _PlayViewController

#pragma mark - Object

static void dealloc(Object *self) {

	PlayViewController *this = (PlayViewController *) self;

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

	this->panel->stackView.view.zIndex = 100;

	this->panel->stackView.view.padding.top = 0;
	this->panel->stackView.view.padding.right = 0;
	this->panel->stackView.view.padding.bottom = 0;
	this->panel->stackView.view.padding.left = 0;

	this->panel->contentView->view.clipsSubviews = true;

	// Setup the TabView

	((PlayViewController *) this)->tabView = $(alloc(TabView), initWithFrame, NULL);
	TabView *tabView = ((PlayViewController *) this)->tabView;

	// Tab buttons

	{
		{
			QuickJoinView *tabData = $(alloc(QuickJoinView), initWithFrame, NULL);

			tabData->view.identifier = "quick_join";

			TabViewItem *tab = $(alloc(TabViewItem), initWithView, (View *) tabData);

			$(tab->label->text, setText, "Quick Join");

			$(tabView, addTab, tab);
		}

		{
			ServerBrowserView *tabData = $(alloc(ServerBrowserView), initWithFrame, NULL);

			tabData->view.identifier = "server_browser";

			TabViewItem *tab = $(alloc(TabViewItem), initWithView, (View *) tabData);

			$(tab->label->text, setText, "Server Browser");

			$(tabView, addTab, tab);
		}

		{
			CreateServerView *tabData = $(alloc(CreateServerView), initWithFrame, NULL);

			tabData->view.identifier = "create_server";

			TabViewItem *tab = $(alloc(TabViewItem), initWithView, (View *) tabData);

			$(tab->label->text, setText, "Create Server");

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
