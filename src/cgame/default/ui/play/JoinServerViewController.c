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

#include "JoinServerViewController.h"
#include "Theme.h"

static const char *_hostname = "Hostname";
static const char *_source = "Source";
static const char *_name = "Map";
static const char *_gameplay = "Gameplay";
static const char *_players = "Players";
static const char *_ping = "Ping";

#define _Class _JoinServerViewController

#pragma mark - Actions

/**
 * @brief ActionFunction for the Quickjoin button.
 * @description Selects a server based on minumum ping and maximum players with
 * a bit of lovely random thrown in. Any server that matches the criteria will
 * be weighted by how much "better" they are by how much lower their ping is and
 * how many more players there are.
 */
static void quickjoinAction(Control *control, const SDL_Event *event, ident sender, ident data) {

	JoinServerViewController *this = (JoinServerViewController *) sender;

	const int16_t max_ping = Clamp(cg_quick_join_max_ping->integer, 0, 999);
	const int16_t min_clients = Clamp(cg_quick_join_min_clients->integer, 0, MAX_CLIENTS);

	uint32_t total_weight = 0;

	const GList *list = this->servers;

	while (list != NULL) {
		const cl_server_info_t *server = list->data;

		int16_t weight = 1;

		if (!(server->clients < min_clients || server->clients >= server->max_clients)) {
			// more weight for more populated servers
			weight += ((int16_t) (server->clients - min_clients)) * 5;

			// more weight for lower ping servers
			weight += ((int16_t) (max_ping - server->ping)) / 10;

			if (server->ping > max_ping) { // one third weight for high ping servers
				weight /= 3;
			}
		}

		total_weight += max(weight, 1);

		list = list->next;
	}

	if (total_weight == 0) {
		return;
	}

	list = this->servers;

	const uint32_t random_weight = Randomr(0, total_weight);
	uint32_t current_weight = 0;

	while (list != NULL) {
		const cl_server_info_t *server = list->data;

		uint32_t weight = 1;

		if (server->ping > max_ping ||
			server->clients < min_clients ||
			server->clients >= server->max_clients) {

			weight = 0;
		} else {
			// more weight for more populated servers
			weight += server->clients - min_clients;

			// more weight for lower ping servers
			weight += ((uint32_t) (max_ping - server->ping)) / 20;
		}

		current_weight += weight;

		if (current_weight > random_weight) {
			cgi.Connect(&server->addr);
			break;
		}

		list = list->next;
	}
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

	JoinServerViewController *this = (JoinServerViewController *) sender;

	if (control == (Control *) this->serversTableView) {
		if (event->button.clicks < 2) {
			return;
		}
	}

	IndexSet *selectedRowIndexes = $((TableView *) this->serversTableView, selectedRowIndexes);
	if (selectedRowIndexes->count) {

		const guint index = (guint) selectedRowIndexes->indexes[0];
		const cl_server_info_t *server = g_list_nth_data(this->servers, index);

		if (server) {
			cgi.Connect(&server->addr);
		}
	}

	release(selectedRowIndexes);
}

#pragma mark - TableViewDataSource

/**
 * @see TableViewDataSource::numberOfRows
 */
static size_t numberOfRows(const TableView *tableView) {

	JoinServerViewController *this = tableView->dataSource.self;

	return g_list_length(this->servers);
}

/**
 * @see TableViewDataSource::valueForColumnAndRow
 */
static ident valueForColumnAndRow(const TableView *tableView, const TableColumn *column, size_t row) {

	JoinServerViewController *this = tableView->dataSource.self;

	cl_server_info_t *server = g_list_nth_data(this->servers, (guint) row);
	assert(server);

	if (g_strcmp0(column->identifier, _hostname) == 0) {
		return server->hostname;
	} else if (g_strcmp0(column->identifier, _source) == 0) {
		return &server->source;
	} else if (g_strcmp0(column->identifier, _name) == 0) {
		return server->name;
	} else if (g_strcmp0(column->identifier, _gameplay) == 0) {
		return server->gameplay;
	} else if (g_strcmp0(column->identifier, _players) == 0) {
		return &server->clients;
	} else if (g_strcmp0(column->identifier, _ping) == 0) {
		return &server->ping;
	}

	return NULL;
}

#pragma mark - TableViewDelegate

/**
 * @see TableViewDelegate::cellForColumnAndRow
 */
static TableCellView *cellForColumnAndRow(const TableView *tableView, const TableColumn *column, size_t row) {

	JoinServerViewController *this = tableView->dataSource.self;

	cl_server_info_t *server = g_list_nth_data(this->servers, (guint) row);
	assert(server);

	TableCellView *cell = $(alloc(TableCellView), initWithFrame, NULL);

	if (g_strcmp0(column->identifier, _hostname) == 0) {
		$(cell->text, setText, server->hostname);
	} else if (g_strcmp0(column->identifier, _source) == 0) {
		switch (server->source) {
			case SERVER_SOURCE_INTERNET:
				$(cell->text, setText, "Internet");
				break;
			case SERVER_SOURCE_USER:
				$(cell->text, setText, "User");
				break;
			case SERVER_SOURCE_BCAST:
				$(cell->text, setText, "LAN");
				break;
		}
	} else if (g_strcmp0(column->identifier, _name) == 0) {
		$(cell->text, setText, server->name);
	} else if (g_strcmp0(column->identifier, _gameplay) == 0) {
		$(cell->text, setText, server->gameplay);
	} else if (g_strcmp0(column->identifier, _players) == 0) {
		$(cell->text, setText, va("%d/%d", server->clients, server->max_clients));
	} else if (g_strcmp0(column->identifier, _ping) == 0) {
		$(cell->text, setText, va("%3d", server->ping));
	}

	return cell;
}

/**
 * @see TableViewDelegate::didSetSortColumn(TableView *)
 */
static void didSetSortColumn(TableView *tableView) {
	$((JoinServerViewController *) tableView->delegate.self, reloadServers);
}

#pragma mark - Object

/**
 * @see Object::dealloc(Object *)
 */
static void dealloc(Object *self) {

	JoinServerViewController *this = (JoinServerViewController *) self;

	g_list_free(this->servers);

	release(this->serversTableView);

	super(Object, self, dealloc);
}

#pragma mark - ViewController

/**
 * @see ViewController::handleNotification(ViewController *, const Notification *)
 */
static void handleNotification(ViewController *self, const Notification *notification) {

	if (notification->name == NOTIFICATION_SERVER_PARSED) {
		$((JoinServerViewController *) self, reloadServers);
	}

	super(ViewController, self, handleNotification, notification);
}

/**
 * @see ViewController::loadView(ViewController *)
 */
static void loadView(ViewController *self) {

	super(ViewController, self, loadView);

	Theme *theme = $(alloc(Theme), initWithTarget, self->view);
	assert(theme);

	self->view->autoresizingMask = ViewAutoresizingContain;
	self->view->identifier = strdup("Join");

	JoinServerViewController *this = (JoinServerViewController *) self;

	StackView *container = $(theme, container);
	assert(container);

	$(theme, attach, container);
	$(theme, target, container);

	{
		const SDL_Rect frame = { .w = 1200, .h = 600 };
		this->serversTableView = $(alloc(TableView), initWithFrame, &frame, ControlStyleDefault);

		TableColumn *hostname = $(alloc(TableColumn), initWithIdentifier, _hostname);
		hostname->width = 360;
		$(this->serversTableView, addColumn, hostname);
		release(hostname);

		TableColumn *source = $(alloc(TableColumn), initWithIdentifier, _source);
		source->width = 100;
		$(this->serversTableView, addColumn, source);
		release(source);

		TableColumn *name = $(alloc(TableColumn), initWithIdentifier, _name);
		name->width = 120;
		$(this->serversTableView, addColumn, name);
		release(name);

		TableColumn *gameplay = $(alloc(TableColumn), initWithIdentifier, _gameplay);
		gameplay->width = 100;
		$(this->serversTableView, addColumn, gameplay);
		release(gameplay);

		TableColumn *players = $(alloc(TableColumn), initWithIdentifier, _players);
		players->width = 80;
		$(this->serversTableView, addColumn, players);
		release(players);

		TableColumn *ping = $(alloc(TableColumn), initWithIdentifier, _ping);
		ping->width = 80;
		$(this->serversTableView, addColumn, ping);
		release(ping);

		this->serversTableView->control.selection = ControlSelectionSingle;

		this->serversTableView->dataSource.numberOfRows = numberOfRows;
		this->serversTableView->dataSource.valueForColumnAndRow = valueForColumnAndRow;
		this->serversTableView->dataSource.self = this;

		this->serversTableView->delegate.cellForColumnAndRow = cellForColumnAndRow;
		this->serversTableView->delegate.didSetSortColumn = didSetSortColumn;
		this->serversTableView->delegate.self = this;

		$((Control *) this->serversTableView, addActionForEventType, SDL_MOUSEBUTTONUP, connectAction, this, NULL);

		$(theme, attach, this->serversTableView);
	}

	$(theme, target, container);

	StackView *accessories = $(theme, accessories);

	$(theme, attach, accessories);
	$(theme, target, accessories);

	$(theme, button, "Quick join", quickjoinAction, self, NULL);
	$(theme, button, "Refresh", refreshAction, self, NULL);
	$(theme, button, "Connect", connectAction, self, NULL);

	release(accessories);
	release(container);
	release(theme);
}

/**
 * @see ViewController::viewWillAppear(ViewController *)
 */
static void viewWillAppear(ViewController *self) {

	super(ViewController, self, viewWillAppear);

	JoinServerViewController *this = (JoinServerViewController *) self;

	if (this->servers == NULL) {
		cgi.GetServers();
	} else {
		$(this, reloadServers);
	}
}

#pragma mark - JoinServerViewController

/**
 * @brief GCompareDataFunc for server sorting.
 */
static gint comparator(gconstpointer a, gconstpointer b, gpointer data) {

	JoinServerViewController *this = (JoinServerViewController *) data;

	if (this->serversTableView->sortColumn) {
		const cl_server_info_t *s0, *s1;

		switch (this->serversTableView->sortColumn->order) {
			case OrderAscending:
				s0 = a, s1 = b;
				break;
			case OrderDescending:
				s0 = b, s1 = a;
				break;
			default:
				return 0;
		}

		if (g_strcmp0(this->serversTableView->sortColumn->identifier, _hostname) == 0) {
			return g_strcmp0(s0->hostname, s1->hostname);
		} else if (g_strcmp0(this->serversTableView->sortColumn->identifier, _source) == 0) {
			return s0->source - s1->source;
		} else if (g_strcmp0(this->serversTableView->sortColumn->identifier, _name) == 0) {
			return g_strcmp0(s0->name, s1->name);
		} else if (g_strcmp0(this->serversTableView->sortColumn->identifier, _gameplay) == 0) {
			return g_strcmp0(s0->gameplay, s1->gameplay);
		} else if (g_strcmp0(this->serversTableView->sortColumn->identifier, _players) == 0) {
			return s0->clients - s1->clients;
		} else if (g_strcmp0(this->serversTableView->sortColumn->identifier, _ping) == 0) {
			return s0->ping - s1->ping;
		}

		assert(false);
	}

	return 0;
}

/**
 * @fn void JoinServerViewController::reloadServers(JoinServerViewController *self)
 * @memberof JoinServerViewController
 */
static void reloadServers(JoinServerViewController *self) {

	g_list_free(self->servers);

	self->servers = g_list_copy(cgi.Servers());
	self->servers = g_list_sort_with_data(self->servers, comparator, self);

	$(self->serversTableView, reloadData);
}

#pragma mark - Class lifecycle

/**
 * @see Class::initialize(Class *)
 */
static void initialize(Class *clazz) {

	((ObjectInterface *) clazz->def->interface)->dealloc = dealloc;

	((ViewControllerInterface *) clazz->def->interface)->handleNotification = handleNotification;
	((ViewControllerInterface *) clazz->def->interface)->loadView = loadView;
	((ViewControllerInterface *) clazz->def->interface)->viewWillAppear = viewWillAppear;

	((JoinServerViewControllerInterface *) clazz->def->interface)->reloadServers = reloadServers;
}

/**
 * @fn Class *JoinServerViewController::_JoinServerViewController(void)
 * @memberof JoinServerViewController
 */
Class *_JoinServerViewController(void) {
	static Class clazz;
	static Once once;

	do_once(&once, {
		clazz.name = "JoinServerViewController";
		clazz.superclass = _ViewController();
		clazz.instanceSize = sizeof(JoinServerViewController);
		clazz.interfaceOffset = offsetof(JoinServerViewController, interface);
		clazz.interfaceSize = sizeof(JoinServerViewControllerInterface);
		clazz.initialize = initialize;
	});

	return &clazz;
}

#undef _Class
