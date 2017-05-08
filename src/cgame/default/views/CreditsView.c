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

#include "parse.h"

#include "CreditsView.h"

static const char *_name = "Name";
static const char *_role = "Role";
static const char *_link = "Link";

typedef struct {
	char name[MAX_STRING_CHARS];
	char role[MAX_STRING_CHARS];
	char link[MAX_STRING_CHARS];
} credit_t;

#define _Class _CreditsView

#pragma mark - Object

/**
 * @see Object::dealloc(Object *)
 */
static void dealloc(Object *self) {

	CreditsView *this = (CreditsView *) self;

	g_slist_free_full(this->credits, cgi.Free);

	release(this->tableView);

	super(Object, self, dealloc);
}

#pragma mark - TableViewDataSource

/**
 * @see TableViewDataSource::numberOfRows
 */
static size_t numberOfRows(const TableView *tableView) {
	return g_slist_length(((CreditsView *) tableView->dataSource.self)->credits);
}

/**
 * @see TableViewDataSource::valueForColumnAndRow
 */
static ident valueForColumnAndRow(const TableView *tableView, const TableColumn *column, size_t row) {

	credit_t *credit = g_slist_nth_data(((CreditsView *) tableView->dataSource.self)->credits, (guint) row);
	assert(credit);

	if (g_strcmp0(column->identifier, _name) == 0) {
		return &credit->name;
	} else if (g_strcmp0(column->identifier, _role) == 0) {
		return &credit->role;
	} else if (g_strcmp0(column->identifier, _link) == 0) {
		return &credit->link;
	}

	return NULL;
}

#pragma mark - TableViewDelegate

/**
 * @see TableViewDelegate::cellForColumnAndRow
 */
static TableCellView *cellForColumnAndRow(const TableView *tableView, const TableColumn *column, size_t row) {

	credit_t *credit = g_slist_nth_data(((CreditsView *) tableView->dataSource.self)->credits, (guint) row);
	assert(credit);

	TableCellView *cell = $(alloc(TableCellView), initWithFrame, NULL);

	if (g_strcmp0(column->identifier, _name) == 0) {
		$(cell->text, setText, credit->name);
	} else if (g_strcmp0(column->identifier, _role) == 0) {
		$(cell->text, setText, credit->role);
	} else if (g_strcmp0(column->identifier, _link) == 0) {
		$(cell->text, setText, credit->link);
	}

	return cell;
}

#pragma mark - CreditsView

/**
 * @fn CreditsView *CreditsView::initWithFrame(CreditsView *self, const SDL_Rect *frame)
 *
 * @memberof CreditsView
 */
 static CreditsView *initWithFrame(CreditsView *self, const SDL_Rect *frame) {

	self = (CreditsView *) super(View, self, initWithFrame, frame);
	if (self) {

		/*
		self->view.padding.top = DEFAULT_PANEL_SPACING;
		self->view.padding.right = DEFAULT_PANEL_SPACING;
		self->view.padding.bottom = DEFAULT_PANEL_SPACING;
		self->view.padding.left = DEFAULT_PANEL_SPACING;
		*/

		{
			Box *box = $(alloc(Box), initWithFrame, NULL);
			$(box->label, setText, "Credits");

			box->view.autoresizingMask = ViewAutoresizingFill;

			self->tableView = $(alloc(TableView), initWithFrame, NULL, ControlStyleDefault);

			self->tableView->control.selection = ControlSelectionNone;

			self->tableView->control.view.autoresizingMask = ViewAutoresizingFill;

			self->tableView->dataSource.self = (ident) self;
			self->tableView->dataSource.numberOfRows = numberOfRows;
			self->tableView->dataSource.valueForColumnAndRow = valueForColumnAndRow;

			self->tableView->delegate.cellForColumnAndRow = cellForColumnAndRow;

			{
				TableColumn *column = $(alloc(TableColumn), initWithIdentifier, _name);
				assert(column);

				column->comparator = (Comparator) g_ascii_strcasecmp;
				column->width = 30;

				$(self->tableView, addColumn, column);
				release(column);
			}

			{
				TableColumn *column = $(alloc(TableColumn), initWithIdentifier, _role);
				assert(column);

				column->comparator = (Comparator) g_ascii_strcasecmp;
				column->width = 40;

				$(self->tableView, addColumn, column);
				release(column);
			}

			{
				TableColumn *column = $(alloc(TableColumn), initWithIdentifier, _link);
				assert(column);

				column->comparator = (Comparator) g_ascii_strcasecmp;
				column->width = 30;

				$(self->tableView, addColumn, column);
				release(column);
			}

			$((View *) box, addSubview, (View *) self->tableView);

			$((View *) self, addSubview, (View *) box);
			release(box);
		}

		$(self, loadCredits, "docs/credits.txt");
	}

	return self;
}

/**
 * @brief Loads the credits file from the data dir
 */
static void loadCredits(CreditsView *self, const char *path) {
	void *buf;
	parser_t parser;
	char token_name[MAX_STRING_CHARS];
	char token_role[MAX_STRING_CHARS];
	char token_link[MAX_STRING_CHARS];

	if (self->credits != NULL) {
		g_slist_free_full(self->credits, cgi.Free);
		self->credits = NULL;
	}

	if (cgi.LoadFile(path, &buf) == -1) {
		return;
	}

	Parse_Init(&parser, (const char *) buf, PARSER_DEFAULT);

	while (true) {

		if (!Parse_Token(&parser, 0, token_name, sizeof(token_name))) {
			break;
		}

		if (!Parse_Token(&parser, 0, token_role, sizeof(token_role))) {
			break;
		}

		if (!Parse_Token(&parser, 0, token_link, sizeof(token_link))) {
			break;
		}

		credit_t *c;

 		c = (credit_t *) cgi.Malloc(sizeof(*c), MEM_TAG_UI);

		g_strlcpy(c->name, token_name, sizeof(token_name));
		g_strlcpy(c->role, token_role, sizeof(token_role));
		g_strlcpy(c->link, token_link, sizeof(token_link));

		self->credits = g_slist_append(self->credits, c);
	}

	cgi.FreeFile(buf);

	$(self->tableView, reloadData);
}

#pragma mark - Class lifecycle

/**
 * @see Class::initialize(Class *)
 */
static void initialize(Class *clazz) {

	((ObjectInterface *) clazz->def->interface)->dealloc = dealloc;

	((CreditsViewInterface *) clazz->def->interface)->loadCredits = loadCredits;
	((CreditsViewInterface *) clazz->def->interface)->initWithFrame = initWithFrame;
}

/**
 * @fn Class *CreditsView::_CreditsView(void)
 * @memberof CreditsView
 */
Class *_CreditsView(void) {
	static Class clazz;
	static Once once;

	do_once(&once, {
		clazz.name = "CreditsView";
		clazz.superclass = _View();
		clazz.instanceSize = sizeof(CreditsView);
		clazz.interfaceOffset = offsetof(CreditsView, interface);
		clazz.interfaceSize = sizeof(CreditsViewInterface);
		clazz.initialize = initialize;
	});

	return &clazz;
}

#undef _Class
