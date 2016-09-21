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

#include <assert.h>

#include "MultiplayerViewController.h"

#include "CreateServerViewController.h"
#include "ServersTableView.h"

#include "client.h"

extern cl_static_t cls;

#define _Class _MultiplayerViewController

#pragma mark - Actions

/**
 * @brief ActionFunction for the Create button.
 */
static void createAction(Control *control, const SDL_Event *event, ident sender, ident data) {

	MainViewController *mainViewController = $((MenuViewController *) sender, mainViewController);

	ViewController *viewController = $((ViewController *) _alloc(&_CreateServerViewController), init);

	$((NavigationViewController *) mainViewController, pushViewController, viewController);

	release(viewController);
}

/**
 * @brief ActionFunction for the Refresh button.
 */
static void refreshAction(Control *control, const SDL_Event *event, ident sender, ident data) {
	Cl_Servers_f();
}

/**
 * @brief ActionFunction for the Connect button.
 */
static void connectAction(Control *control, const SDL_Event *event, ident sender, ident data) {

	if ($((Object *) control, isKindOfClass, &_TableView)) {
		if (event->button.clicks < 2) {
			return;
		}
	}

	const TableView *servers = (TableView *) data;
	IndexSet *selectedRowIndexes = $(servers, selectedRowIndexes);
	if (selectedRowIndexes->count) {
		const cl_server_info_t *server = g_list_nth_data(cls.servers, selectedRowIndexes->indexes[0]);
		if (server) {
			Cbuf_AddText(va("connect %s\n", Net_NetaddrToString(&server->addr)));
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

	Cl_Servers_f();

	MenuViewController *this = (MenuViewController *) self;

	ServersTableView *servers;

	{
		Box *box = alloc(Box, initWithFrame, NULL);
		$(box->label, setText, "JOIN GAME");

		box->view.autoresizingMask |= ViewAutoresizingFill;

		const SDL_Rect frame = MakeRect(0, 0, 0, 320);
		servers = alloc(ServersTableView, initWithFrame, &frame, ControlStyleDefault);

		servers->tableView.control.view.autoresizingMask = ViewAutoresizingWidth;

		$((Control *) servers, addActionForEventType, SDL_MOUSEBUTTONUP, connectAction, self, servers);

		$((View *) box, addSubview, (View *) servers);
		release(servers);

		$((View *) this->panel->contentView, addSubview, (View *) box);
		release(box);
	}

	{
		this->panel->accessoryView->view.hidden = false;

		Ui_Button((View *) this->panel->accessoryView, "Create..", createAction, self, NULL);
		Ui_Button((View *) this->panel->accessoryView, "Refresh", refreshAction, self, NULL);
		Ui_Button((View *) this->panel->accessoryView, "Connect", connectAction, self, servers);
	}
}

#pragma mark - Class lifecycle

/**
 * @see Class::initialize(Class *)
 */
static void initialize(Class *clazz) {

	((ViewControllerInterface *) clazz->interface)->loadView = loadView;
}

Class _MultiplayerViewController = {
	.name = "MultiplayerViewController",
	.superclass = &_MenuViewController,
	.instanceSize = sizeof(MultiplayerViewController),
	.interfaceOffset = offsetof(MultiplayerViewController, interface),
	.interfaceSize = sizeof(MultiplayerViewControllerInterface),
	.initialize = initialize,
};

#undef _Class

