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
 * @brief ActionFunction for the Quickjoin button.
 * @description Selects a server based on maximum ping and minimum players. Any
 * server that matches the above criteria will be weighted by how much "better"
 * they are by how much lower their ping is and how many more players there are.
 */
static void quickjoinAction(Control *control, const SDL_Event *event, ident sender, ident data) {

	const uint16_t max_ping = Clamp(qj_max_ping->integer, 0, 999);
	const uint16_t min_clients = Clamp(qj_min_clients->integer, 0, MAX_CLIENTS);

	GList *server = cgi.Servers();

	cl_server_info_t *svdata;
	uint32_t total_weight = 0;

	while (server != NULL) {
		svdata = (cl_server_info_t *) server->data;

		server = server->next;

		uint32_t weight = 1;

		if (svdata->ping > max_ping ||
			svdata->clients < min_clients ||
			svdata->clients >= svdata->max_clients) {

			weight = 0;
		} else {
			// more weight for more populated servers
			weight += svdata->clients - min_clients;

			// more weight for lower ping servers
			weight += ((int16_t) (max_ping - svdata->ping)) / 20;
		}

		total_weight += weight;
	}

	if(total_weight == 0) {
		return;
	}

	server = cgi.Servers();

	uint32_t random_weight = Random() % total_weight;
	uint32_t current_weight = 0;

	while (server != NULL) {
		svdata = (cl_server_info_t *) server->data;

		server = server->next;

		uint32_t weight = 1;

		if (svdata->ping > max_ping ||
			svdata->clients < min_clients ||
			svdata->clients >= svdata->max_clients) {

			weight = 0;
		} else {
			// more weight for more populated servers
			weight += svdata->clients - min_clients;

			// more weight for lower ping servers
			weight += ((uint32_t) (max_ping - svdata->ping)) / 20;
		}

		current_weight += weight;

		if (current_weight > random_weight) {
			cgi.Connect(&svdata->addr);
			break;
		}
	}
}

/**
 * @brief ActionFunction for the Create button.
 */
static void createAction(Control *control, const SDL_Event *event, ident sender, ident data) {

	MainViewController *mainViewController = $((MenuViewController *) sender, mainViewController);

	ViewController *viewController = $((ViewController *) alloc(CreateServerViewController), init);

	$((NavigationViewController *) mainViewController, pushViewController, viewController);

	release(viewController);
}

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

	MenuViewController *this = (MenuViewController *) self;

	ServersTableView *servers;

	{
		Box *box = $(alloc(Box), initWithFrame, NULL);
		$(box->label, setText, "JOIN GAME");

		box->view.autoresizingMask |= ViewAutoresizingFill;

		const SDL_Rect frame = MakeRect(0, 0, 0, 320);
		servers = $(alloc(ServersTableView), initWithFrame, &frame, ControlStyleDefault);

		servers->tableView.control.view.autoresizingMask = ViewAutoresizingWidth;

		$((Control *) servers, addActionForEventType, SDL_MOUSEBUTTONUP, connectAction, self, servers);

		$((View *) box, addSubview, (View *) servers);
		release(servers);

		$((View *) this->panel->contentView, addSubview, (View *) box);
		release(box);
	}

	{
		this->panel->accessoryView->view.hidden = false;

		Cg_Button((View *) this->panel->accessoryView, "Quickjoin", quickjoinAction, self, NULL);
		Cg_Button((View *) this->panel->accessoryView, "Create..", createAction, self, NULL);
		Cg_Button((View *) this->panel->accessoryView, "Refresh", refreshAction, self, NULL);
		Cg_Button((View *) this->panel->accessoryView, "Connect", connectAction, self, servers);
	}
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
		clazz.superclass = _MenuViewController();
		clazz.instanceSize = sizeof(MultiplayerViewController);
		clazz.interfaceOffset = offsetof(MultiplayerViewController, interface);
		clazz.interfaceSize = sizeof(MultiplayerViewControllerInterface);
		clazz.initialize = initialize;
	});

	return &clazz;
}

#undef _Class
