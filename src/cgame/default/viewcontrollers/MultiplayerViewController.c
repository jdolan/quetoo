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

#include "MultiplayerViewController.h"

#include "CreateServerViewController.h"
#include "ServersTableView.h"

#define _Class _MultiplayerViewController

#pragma mark - Actions

/**
 * @brief ActionFunction for the Refresh button.
 */
static void refreshAction(Control *control, const SDL_Event *event, ident sender, ident data) {
	cgi.GetServers();
}

/**
 * @brief ActionFunction for the Connect button.
 */
static void connectAction(Control *control, const SDL_Event *event, ident sender, ident data) {

	if ($((Object *) control, isKindOfClass, _TableView())) {
		if (event->button.clicks < 2) {
			return;
		}
	}

	const TableView *servers = (TableView *) data;
	IndexSet *selectedRowIndexes = $(servers, selectedRowIndexes);
	if (selectedRowIndexes->count) {
		const guint index = (guint) selectedRowIndexes->indexes[0];
		const cl_server_info_t *server = g_list_nth_data(cgi.Servers(), index);
		if (server) {
			cgi.Connect(&server->addr);
		}
	}

	release(selectedRowIndexes);
}

#pragma mark - ViewController

/**
 * @see ViewController::loadView(ViewController *)
 */
static void loadView(ViewController *self) {

	super(ViewController, self, loadView);

	cgi.GetServers();

	TabViewController *this = (TabViewController *) self;

	ServersTableView *servers;

	StackView *columns = $(alloc(StackView), initWithFrame, NULL);

	columns->axis = StackViewAxisHorizontal;
	columns->spacing = DEFAULT_PANEL_SPACING;

	{
		StackView *column = $(alloc(StackView), initWithFrame, NULL);

		columns->spacing = DEFAULT_PANEL_SPACING;

		{
			Box *box = $(alloc(Box), initWithFrame, NULL);
			$(box->label, setText, "Actions");

			StackView *stackView = $(alloc(StackView), initWithFrame, NULL);

			stackView->axis = StackViewAxisHorizontal;
			stackView->spacing = DEFAULT_PANEL_SPACING;

			Cg_Button((View *) stackView, "Refresh", refreshAction, self, NULL);
			Cg_Button((View *) stackView, "Connect", connectAction, self, NULL);

			$((View *) box, addSubview, (View *) stackView);
			release(stackView);

			$((View *) column, addSubview, (View *) box);
			release(box);
		}

		{
			Box *box = $(alloc(Box), initWithFrame, NULL);
			$(box->label, setText, "Servers");

			box->view.autoresizingMask |= ViewAutoresizingFill;

			servers = $(alloc(ServersTableView), initWithFrame, NULL, ControlStyleDefault);

			servers->tableView.control.view.frame.h = 200;

			servers->tableView.control.view.autoresizingMask = ViewAutoresizingWidth;

			$((Control *) servers, addActionForEventType, SDL_MOUSEBUTTONUP, connectAction, self, servers);

			$((View *) box, addSubview, (View *) servers);
			release(servers);

			$((View *) column, addSubview, (View *) box);
			release(box);
		}

		{
			Box *box = $(alloc(Box), initWithFrame, NULL);
			$(box->label, setText, "Server info");

			box->view.autoresizingMask |= ViewAutoresizingFill;

			servers = $(alloc(ServersTableView), initWithFrame, NULL, ControlStyleDefault);

			servers->tableView.control.view.frame.h = 200;

			servers->tableView.control.view.autoresizingMask = ViewAutoresizingWidth;

			$((Control *) servers, addActionForEventType, SDL_MOUSEBUTTONUP, connectAction, self, servers);

			$((View *) box, addSubview, (View *) servers);
			release(servers);

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
 * @fn Class *MultiplayerViewController::_MultiplayerViewController(void)
 * @memberof MultiplayerViewController
 */
Class *_MultiplayerViewController(void) {
	static Class clazz;
	static Once once;

	do_once(&once, {
		clazz.name = "MultiplayerViewController";
		clazz.superclass = _TabViewController();
		clazz.instanceSize = sizeof(MultiplayerViewController);
		clazz.interfaceOffset = offsetof(MultiplayerViewController, interface);
		clazz.interfaceSize = sizeof(MultiplayerViewControllerInterface);
		clazz.initialize = initialize;
	});

	return &clazz;
}

#undef _Class
