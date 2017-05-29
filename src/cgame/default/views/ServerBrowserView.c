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

#include "ServerBrowserView.h"

static const char *_hostname = "Hostname";
static const char *_mapname = "Map";
static const char *_gameplay = "Gameplay";
static const char *_players = "Players";
static const char *_ping = "Ping";

#define _Class _ServerBrowserView

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

	ServerBrowserView *this = (ServerBrowserView *) sender;

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

	ServerBrowserView *this = tableView->dataSource.self;

	return g_list_length(this->servers);
}

/**
 * @see TableViewDataSource::valueForColumnAndRow
 */
static ident valueForColumnAndRow(const TableView *tableView, const TableColumn *column, size_t row) {

	ServerBrowserView *this = tableView->dataSource.self;

	cl_server_info_t *server = g_list_nth_data(this->servers, (guint) row);
	assert(server);

	if (g_strcmp0(column->identifier, _hostname) == 0) {
		return server->hostname;
	} else if (g_strcmp0(column->identifier, _mapname) == 0) {
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

	ServerBrowserView *this = tableView->dataSource.self;

	cl_server_info_t *server = g_list_nth_data(this->servers, (guint) row);
	assert(server);

	TableCellView *cell = $(alloc(TableCellView), initWithFrame, NULL);

	if (g_strcmp0(column->identifier, _hostname) == 0) {
		$(cell->text, setText, server->hostname);
	} else if (g_strcmp0(column->identifier, _mapname) == 0) {
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
	$((ServerBrowserView *) tableView->delegate.self, reloadServers);
}

#pragma mark - Object

/**
 * @see Object::dealloc(Object *)
 */
static void dealloc(Object *self) {

	ServerBrowserView *this = (ServerBrowserView *) self;

	g_list_free(this->servers);

	release(this->serversTableView);

	super(Object, self, dealloc);
}

#pragma mark - View

/**
 * @see View::respondToEvent(View *, const SDL_Event *)
 */
static void respondToEvent(View *self, const SDL_Event *event) {

	if (event->type == SDL_USEREVENT) {
		if (event->user.code == EVENT_SERVER_PARSED) {
			$((ServerBrowserView *) self, reloadServers);
		}
	}

	super(View, self, respondToEvent, event);
}

/**
 * @see View::viewWillAppear(View *)
 */
/*
static void viewWillAppear(View *self) {

	$((ServerBrowserView *) self, reloadServers);

	super(View, self, viewWillAppear);
}
*/

#pragma mark - ServerBrowserView

/**
 * @brief GCompareDataFunc for server sorting.
 */
static gint comparator(gconstpointer a, gconstpointer b, gpointer data) {

	ServerBrowserView *this = (ServerBrowserView *) data;

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
		} else if (g_strcmp0(this->serversTableView->sortColumn->identifier, _mapname) == 0) {
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
 * @fn ServerBrowserView *ServerBrowserView::initWithFrame(ServerBrowserView *self, const SDL_Rect *frame)
 *
 * @memberof ServerBrowserView
 */
 static ServerBrowserView *initWithFrame(ServerBrowserView *self, const SDL_Rect *frame) {

	self = (ServerBrowserView *) super(View, self, initWithFrame, frame);
	if (self) {

		cgi.GetServers();

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

				const SDL_Rect frame = MakeRect(0, 0, 670, 0);
				self->serversTableView = $(alloc(TableView), initWithFrame, &frame, ControlStyleDefault);

				TableColumn *hostname = $(alloc(TableColumn), initWithIdentifier, _hostname);
				hostname->width = 300;
				$(self->serversTableView, addColumn, hostname);
				release(hostname);

				TableColumn *name = $(alloc(TableColumn), initWithIdentifier, _mapname);
				name->width = 80;
				$(self->serversTableView, addColumn, name);
				release(name);

				TableColumn *gameplay = $(alloc(TableColumn), initWithIdentifier, _gameplay);
				gameplay->width = 90;
				$(self->serversTableView, addColumn, gameplay);
				release(gameplay);

				TableColumn *players = $(alloc(TableColumn), initWithIdentifier, _players);
				players->width = 60;
				$(self->serversTableView, addColumn, players);
				release(players);

				TableColumn *ping = $(alloc(TableColumn), initWithIdentifier, _ping);
				ping->width = 40;
				$(self->serversTableView, addColumn, ping);
				release(ping);

				self->serversTableView->control.view.autoresizingMask = ViewAutoresizingWidth;
				self->serversTableView->control.selection = ControlSelectionSingle;

				self->serversTableView->dataSource.numberOfRows = numberOfRows;
				self->serversTableView->dataSource.valueForColumnAndRow = valueForColumnAndRow;
				self->serversTableView->dataSource.self = self;

				self->serversTableView->delegate.cellForColumnAndRow = cellForColumnAndRow;
				self->serversTableView->delegate.didSetSortColumn = didSetSortColumn;
				self->serversTableView->delegate.self = self;

				$((Control *) self->serversTableView, addActionForEventType, SDL_MOUSEBUTTONUP, connectAction, self, NULL);

				$((View *) box, addSubview, (View *) self->serversTableView);

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

		$((View *) self, addSubview, (View *) columns);
		release(columns);
	}

	return self;
}

/**
 * @fn void ServerBrowserView::reloadServers(ServerBrowserView *self)
 * @memberof ServerBrowserView
 */
static void reloadServers(ServerBrowserView *self) {

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

	((ViewInterface *) clazz->def->interface)->respondToEvent = respondToEvent;
//	((ViewInterface *) clazz->def->interface)->viewWillAppear = viewWillAppear;

	((ServerBrowserViewInterface *) clazz->def->interface)->initWithFrame = initWithFrame;
	((ServerBrowserViewInterface *) clazz->def->interface)->reloadServers = reloadServers;
}

/**
 * @fn Class *ServerBrowserView::_ServerBrowserView(void)
 * @memberof ServerBrowserView
 */
Class *_ServerBrowserView(void) {
	static Class clazz;
	static Once once;

	do_once(&once, {
		clazz.name = "ServerBrowserView";
		clazz.superclass = _View();
		clazz.instanceSize = sizeof(ServerBrowserView);
		clazz.interfaceOffset = offsetof(ServerBrowserView, interface);
		clazz.interfaceSize = sizeof(ServerBrowserViewInterface);
		clazz.initialize = initialize;
	});

	return &clazz;
}

#undef _Class
