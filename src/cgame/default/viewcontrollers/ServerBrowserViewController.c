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

#include "ServerBrowserViewController.h"

#include "CreateServerViewController.h"
#include "ServersTableView.h"

#define _Class _ServerBrowserViewController

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

	columns->spacing = DEFAULT_PANEL_SPACING;

	columns->axis = StackViewAxisHorizontal;
	columns->distribution = StackViewDistributionFill;

	columns->view.autoresizingMask = ViewAutoresizingFill;

	{
		StackView *column = $(alloc(StackView), initWithFrame, NULL);

		column->spacing = DEFAULT_PANEL_SPACING;

		column->view.autoresizingMask |= ViewAutoresizingHeight;

		{
			Box *box = $(alloc(Box), initWithFrame, NULL);
			$(box->label, setText, "Servers");

			box->view.autoresizingMask |= ViewAutoresizingHeight;

			const SDL_Rect frame = MakeRect(0, 0, 600, 0);
			servers = $(alloc(ServersTableView), initWithFrame, &frame, ControlStyleDefault);

			servers->tableView.control.view.autoresizingMask = ViewAutoresizingHeight;

			$((Control *) servers, addActionForEventType, SDL_MOUSEBUTTONUP, connectAction, self, servers);

			$((View *) box, addSubview, (View *) servers);
			release(servers);

			$((View *) column, addSubview, (View *) box);
			release(box);
		}

		$((View *) columns, addSubview, (View *) column);
		release(column);
	}

/*
	{
		StackView *column = $(alloc(StackView), initWithFrame, NULL);

		column->spacing = DEFAULT_PANEL_SPACING;

		column->view.autoresizingMask |= ViewAutoresizingHeight;

		{
			Box *box = $(alloc(Box), initWithFrame, NULL);
			$(box->label, setText, "Filters");

			box->view.autoresizingMask |= ViewAutoresizingHeight;

			StackView *stackView = $(alloc(StackView), initWithFrame, NULL);

			Cgui_CvarCheckboxInput((View *) stackView, "Toggle things", "s_ambient");

			$((View *) box, addSubview, (View *) stackView);
			release(stackView);

			$((View *) column, addSubview, (View *) box);
			release(box);
		}

		$((View *) columns, addSubview, (View *) column);
		release(column);
	}
*/

	$((View *) this->panel->contentView, addSubview, (View *) columns);
	release(columns);

	this->panel->accessoryView->view.hidden = false;

	Cgui_Button((View *) this->panel->accessoryView, "Refresh", refreshAction, self, NULL);
	Cgui_Button((View *) this->panel->accessoryView, "Connect", connectAction, self, NULL);
}

#pragma mark - Class lifecycle

/**
 * @see Class::initialize(Class *)
 */
static void initialize(Class *clazz) {

	((ViewControllerInterface *) clazz->def->interface)->loadView = loadView;
}

/**
 * @fn Class *ServerBrowserViewController::_ServerBrowserViewController(void)
 * @memberof ServerBrowserViewController
 */
Class *_ServerBrowserViewController(void) {
	static Class clazz;
	static Once once;

	do_once(&once, {
		clazz.name = "ServerBrowserViewController";
		clazz.superclass = _TabViewController();
		clazz.instanceSize = sizeof(ServerBrowserViewController);
		clazz.interfaceOffset = offsetof(ServerBrowserViewController, interface);
		clazz.interfaceSize = sizeof(ServerBrowserViewControllerInterface);
		clazz.initialize = initialize;
	});

	return &clazz;
}

#undef _Class
