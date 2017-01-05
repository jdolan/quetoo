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

#include "ServersTableView.h"

static const char *_hostname = "Hostname";
static const char *_source = "Source";
static const char *_name = "Map";
static const char *_gameplay = "Gameplay";
static const char *_players = "Players";
static const char *_ping = "Ping";

#define _Class _ServersTableView

/**
 * @brief Comparator for numeric values (clients and ping).
 */
static Order intcmp(const ident a, const ident b) {

	int32_t i = *(int32_t *) a;
	int32_t j = *(int32_t *) b;

	return i - j;
}

#pragma mark - TableViewDataSource

/**
 * @see TableViewDataSource::numberOfRows
 */
static size_t numberOfRows(const TableView *tableView) {
	return g_list_length(cgi.Servers());
}

/**
 * @see TableViewDataSource::valueForColumnAndRow
 */
static ident valueForColumnAndRow(const TableView *tableView, const TableColumn *column, size_t row) {

	cl_server_info_t *server = g_list_nth_data(cgi.Servers(), (guint) row);
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

	cl_server_info_t *server = g_list_nth_data(cgi.Servers(), (guint) row);
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

#pragma mark - View

/**
 * @see View::updateBindings
 */
static void updateBindings(View *self) {

	super(View, self, updateBindings);

	$((TableView *) self, reloadData);
}

#pragma mark - ServersTableView

/**
 * @fn ServersTableView *ServersTableView::initWithFrame(ServersTableView *self, const SDL_Rect *frame, ControlStyle style)
 *
 * @memberof ServersTableView
 */
static ServersTableView *initWithFrame(ServersTableView *self, const SDL_Rect *frame, ControlStyle style) {

	self = (ServersTableView *) super(TableView, self, initWithFrame, frame, style);
	if (self) {
		self->tableView.control.selection = ControlSelectionSingle;

		self->tableView.dataSource.numberOfRows = numberOfRows;
		self->tableView.dataSource.valueForColumnAndRow = valueForColumnAndRow;

		self->tableView.delegate.cellForColumnAndRow = cellForColumnAndRow;

		{
			TableColumn *column = $(alloc(TableColumn), initWithIdentifier, _hostname);
			assert(column);

			column->comparator = (Comparator) g_ascii_strcasecmp;
			column->width = 360;

			$((TableView *) self, addColumn, column);
			release(column);
		}

		{
			TableColumn *column = $(alloc(TableColumn), initWithIdentifier, _source);
			assert(column);

			column->comparator = intcmp;
			column->width = 100;

			$((TableView *) self, addColumn, column);
			release(column);
		}

		{
			TableColumn *column = $(alloc(TableColumn), initWithIdentifier, _name);
			assert(column);

			column->comparator = (Comparator) g_ascii_strcasecmp;
			column->width = 120;

			$((TableView *) self, addColumn, column);
			release(column);
		}

		{
			TableColumn *column = $(alloc(TableColumn), initWithIdentifier, _gameplay);
			assert(column);

			column->comparator = (Comparator) g_ascii_strcasecmp;
			column->width = 100;

			$((TableView *) self, addColumn, column);
			release(column);
		}

		{
			TableColumn *column = $(alloc(TableColumn), initWithIdentifier, _players);
			assert(column);

			column->comparator = intcmp;
			column->width = 80;

			$((TableView *) self, addColumn, column);
			release(column);
		}

		{
			TableColumn *column = $(alloc(TableColumn), initWithIdentifier, _ping);
			assert(column);

			column->comparator = intcmp;
			column->width = 80;

			$((TableView *) self, addColumn, column);
			release(column);
		}

		$((TableView *) self, reloadData);
	}

	return self;
}

#pragma mark - Class lifecycle

/**
 * @see Class::initialize(Class *)
 */
static void initialize(Class *clazz) {

	((ViewInterface *) clazz->def->interface)->updateBindings = updateBindings;

	((ServersTableViewInterface *) clazz->def->interface)->initWithFrame = initWithFrame;
}

Class *_ServersTableView(void) {
	static Class _class;
	
	if (!_class.name) {
		_class.name = "ServersTableView";
		_class.superclass = _TableView();
		_class.instanceSize = sizeof(ServersTableView);
		_class.interfaceOffset = offsetof(ServersTableView, interface);
		_class.interfaceSize = sizeof(ServersTableViewInterface);
		_class.initialize = initialize;
	}

	return &_class;
}

#undef _Class

