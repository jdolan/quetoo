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

#include "CreditsViewController.h"

static const char *_name = "Name";
static const char *_role = "Role";

typedef struct {
	const char *name;
	const char *role;
} credit_t;

#define MAX_CREDITS 3

static credit_t credits[MAX_CREDITS] = {
	{
		.name = "First \"Bob\" Last",
		.role = "Project lead"
	},
	{
		.name = "First \"Jimmy\" Last",
		.role = "Maintainer"
	},
	{
		.name = "First \"Joe\" Last",
		.role = "Developer"
	}
};

#define _Class _CreditsViewController

#pragma mark - TableViewDataSource

/**
 * @see TableViewDataSource::numberOfRows
 */
static size_t numberOfRows(const TableView *tableView) {
	return MAX_CREDITS;
}

/**
 * @see TableViewDataSource::valueForColumnAndRow
 */
static ident valueForColumnAndRow(const TableView *tableView, const TableColumn *column, size_t row) {

	assert(row < MAX_CREDITS);

	if (g_strcmp0(column->identifier, _name) == 0) {
		return &credits[row].name;
	} else if (g_strcmp0(column->identifier, _role) == 0) {
		return &credits[row].role;
	}

	return NULL;
}

#pragma mark - TableViewDelegate

/**
 * @see TableViewDelegate::cellForColumnAndRow
 */
static TableCellView *cellForColumnAndRow(const TableView *tableView, const TableColumn *column, size_t row) {

	assert(row < MAX_CREDITS);

	TableCellView *cell = $(alloc(TableCellView), initWithFrame, NULL);

	if (g_strcmp0(column->identifier, _name) == 0) {
		$(cell->text, setText, credits[row].name);
	} else if (g_strcmp0(column->identifier, _role) == 0) {
		$(cell->text, setText, credits[row].role);
	}

	return cell;
}

#pragma mark - ViewController

/**
 * @see ViewController::loadView(ViewController *)
 */
static void loadView(ViewController *self) {

	super(ViewController, self, loadView);

	self->view->padding.top = DEFAULT_PANEL_SPACING;
	self->view->padding.right = DEFAULT_PANEL_SPACING;
	self->view->padding.bottom = DEFAULT_PANEL_SPACING;
	self->view->padding.left = DEFAULT_PANEL_SPACING;

	{
		Box *box = $(alloc(Box), initWithFrame, NULL);
		$(box->label, setText, "Credits");

		box->view.autoresizingMask = ViewAutoresizingFill;

		TableView *tableView = $(alloc(TableView), initWithFrame, NULL, ControlStyleDefault);

		tableView->control.selection = ControlSelectionNone;

		tableView->control.view.autoresizingMask = ViewAutoresizingFill;

		tableView->dataSource.numberOfRows = numberOfRows;
		tableView->dataSource.valueForColumnAndRow = valueForColumnAndRow;

		tableView->delegate.cellForColumnAndRow = cellForColumnAndRow;

		{
			TableColumn *column = $(alloc(TableColumn), initWithIdentifier, _name);
			assert(column);

			column->comparator = (Comparator) g_ascii_strcasecmp;
			column->width = 30;

			$(tableView, addColumn, column);
			release(column);
		}

		{
			TableColumn *column = $(alloc(TableColumn), initWithIdentifier, _role);
			assert(column);

			column->comparator = (Comparator) g_ascii_strcasecmp;
			column->width = 70;

			$(tableView, addColumn, column);
			release(column);
		}

		$(tableView, reloadData);

		$((View *) box, addSubview, (View *) tableView);
		release(tableView);

		$(self->view, addSubview, (View *) box);
		release(box);
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
 * @fn Class *CreditsViewController::_CreditsViewController(void)
 * @memberof CreditsViewController
 */
Class *_CreditsViewController(void) {
	static Class clazz;
	static Once once;

	do_once(&once, {
		clazz.name = "CreditsViewController";
		clazz.superclass = _TabViewController();
		clazz.instanceSize = sizeof(CreditsViewController);
		clazz.interfaceOffset = offsetof(CreditsViewController, interface);
		clazz.interfaceSize = sizeof(CreditsViewControllerInterface);
		clazz.initialize = initialize;
	});

	return &clazz;
}

#undef _Class
