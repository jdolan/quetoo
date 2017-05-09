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

#include <Objectively/Value.h>

#include "cg_local.h"

#include "CreateServerView.h"
#include "MapListCollectionItemView.h"

#define _Class _CreateServerView

#pragma mark - Actions and delegate callbacks

/**
 * @brief Select gameplay mode
 */
static void selectGameplay(Select *select, Option *option) {
	if (option->value == (ident) 0) {
		cgi.CvarSet(g_gameplay->name, "default");
	} else if (option->value == (ident) 1) {
		cgi.CvarSet(g_gameplay->name, "deathmatch");
	} else if (option->value == (ident) 2) {
		cgi.CvarSet(g_gameplay->name, "instagib");
	} else if (option->value == (ident) 3) {
		cgi.CvarSet(g_gameplay->name, "arena");
	} else if (option->value == (ident) 4) {
		cgi.CvarSet(g_gameplay->name, "duel");
	}
}

/**
 * @brief Select teamplay mode
 */
static void selectTeamsplay(Select *select, Option *option) {
	if (option->value == (ident) 0) {
		cgi.CvarSet(g_teams->name, "0");
		cgi.CvarSet(g_ctf->name, "0");
	} else if (option->value == (ident) 1) {
		cgi.CvarSet(g_teams->name, "1");
		cgi.CvarSet(g_ctf->name, "0");
	} else if (option->value == (ident) 2) {
		cgi.CvarSet(g_teams->name, "0");
		cgi.CvarSet(g_ctf->name, "1");
	}
}

/**
 * @brief ActionFunction for the Create button.
 */
static void createAction(Control *control, const SDL_Event *event, ident sender, ident data) {

	CreateServerView *this = (CreateServerView *) sender;

	Array *selectedMaps = $(this->mapList, selectedMaps);
	if (selectedMaps->count) {
		char map[MAX_QPATH];

		file_t *file = cgi.OpenFileWrite(MAP_LIST_UI);
		if (file) {

			MutableString *string = mstr("");

			for (size_t i = 0; i < selectedMaps->count; i++) {

				const Value *value = $(selectedMaps, objectAtIndex, i);
				const MapListItemInfo *info = (MapListItemInfo *) value->value;

				char name[MAX_QPATH];
				StripExtension(Basename(info->mapname), name);

				if (i == 0) {
					g_strlcpy(map, name, sizeof(map));
				}

				$(string, appendFormat, "{\n\tname %s\n}\n", name);
			}

			const int64_t len = cgi.WriteFile(file, string->string.chars, string->string.length, 1);

			if (len == -1) {
				cgi.Warn("Failed to write %s\n", MAP_LIST_UI);
			} else {
				cgi.Debug("Wrote %s %"PRId64" bytes\n", MAP_LIST_UI, len);
			}

			release(string);
		}

		cgi.CloseFile(file);

		cgi.CvarSet("g_map_list", MAP_LIST_UI);
		cgi.Cbuf(va("map %s", map));

	} else {
		cgi.Print("No maps selected\n");
	}

	release(selectedMaps);
}

#pragma mark - Object

/**
 * @see Object::dealloc(Object *)
 */
static void dealloc(Object *self) {

	CreateServerView *this = (CreateServerView *) self;

	release(this->gameplay);

	release(this->matchMode);
	release(this->teamsplay);

	release(this->mapList);

	super(Object, self, dealloc);
}

#pragma mark - CreateServerView

/**
 * @fn CreateServerView *CreateServerView::initWithFrame(CreateServerView *self, const SDL_Rect *frame)
 *
 * @memberof CreateServerView
 */
 static CreateServerView *initWithFrame(CreateServerView *self, const SDL_Rect *frame) {

	self = (CreateServerView *) super(View, self, initWithFrame, frame);
	if (self) {

		StackView *columns = $(alloc(StackView), initWithFrame, NULL);

		columns->spacing = DEFAULT_PANEL_SPACING;

		columns->axis = StackViewAxisHorizontal;
		columns->distribution = StackViewDistributionFill;

		columns->view.autoresizingMask = ViewAutoresizingFill;

		{
			StackView *column = $(alloc(StackView), initWithFrame, NULL);

			column->spacing = DEFAULT_PANEL_SPACING;

			{
				Box *box = $(alloc(Box), initWithFrame, NULL);
				$(box->label, setText, "Host options");

				box->view.autoresizingMask |= ViewAutoresizingWidth;

				StackView *stackView = $(alloc(StackView), initWithFrame, NULL);

				Cgui_CvarTextView((View *) stackView, "Hostname", "sv_hostname");
				Cgui_CvarSliderInput((View *) stackView, "Max players", "sv_max_clients", 1.0, 32.0, 1.0);
				Cgui_CvarCheckboxInput((View *) stackView, "Public", "sv_public");
				Cgui_CvarTextView((View *) stackView, "Password", "password");

				$((View *) box, addSubview, (View *) stackView);
				release(stackView);

				$((View *) column, addSubview, (View *) box);
				release(box);
			}

			{
				Box *box = $(alloc(Box), initWithFrame, NULL);
				$(box->label, setText, "Game options");

				box->view.autoresizingMask |= ViewAutoresizingWidth;

				StackView *stackView = $(alloc(StackView), initWithFrame, NULL);

				self->gameplay = $(alloc(Select), initWithFrame, NULL, ControlStyleDefault);

				$(self->gameplay, addOption, "Default", (ident) 0);
				$(self->gameplay, addOption, "Deathmatch", (ident) 1);
				$(self->gameplay, addOption, "Instagib", (ident) 2);
				$(self->gameplay, addOption, "Arena", (ident) 3);
				$(self->gameplay, addOption, "Duel", (ident) 4);

				self->gameplay->control.view.frame.w = DEFAULT_TEXTVIEW_WIDTH;
				self->gameplay->delegate.didSelectOption = selectGameplay;

				if (!g_strcmp0(g_gameplay->string, "default")) {
					$(self->gameplay, selectOptionWithValue, (ident) 0);
				} else if (!g_strcmp0(g_gameplay->string, "deathmatch")) {
					$(self->gameplay, selectOptionWithValue, (ident) 1);
				} else if (!g_strcmp0(g_gameplay->string, "instagib")) {
					$(self->gameplay, selectOptionWithValue, (ident) 2);
				} else if (!g_strcmp0(g_gameplay->string, "arena")) {
					$(self->gameplay, selectOptionWithValue, (ident) 3);
				} else if (!g_strcmp0(g_gameplay->string, "duel")) {
					$(self->gameplay, selectOptionWithValue, (ident) 4);
				}

				Cgui_Input((View *) stackView, "Gameplay", (Control *) self->gameplay);

				self->teamsplay = $(alloc(Select), initWithFrame, NULL, ControlStyleDefault);

				$(self->teamsplay, addOption, "Free for All", (ident) 0);
				$(self->teamsplay, addOption, "Team deathmatch", (ident) 1);
				$(self->teamsplay, addOption, "Capture the flag", (ident) 2);

				self->teamsplay->control.view.frame.w = DEFAULT_TEXTVIEW_WIDTH;
				self->teamsplay->delegate.didSelectOption = selectTeamsplay;

				if (g_ctf->integer != 0) {
					$(self->teamsplay, selectOptionWithValue, (ident) 2);
				} else {
					$(self->teamsplay, selectOptionWithValue, (ident) (ptrdiff_t) g_teams->integer);
				}

				Cgui_Input((View *) stackView, "Teams play", (Control *) self->teamsplay);

				Cgui_CvarCheckboxInput((View *) stackView, "Match mode", "g_match");

				Cgui_CvarSliderInput((View *) stackView, "Minimum players", "g_ai_max_clients", -1.0, 16.0, 1.0);

				$((View *) box, addSubview, (View *) stackView);
				release(stackView);

				$((View *) column, addSubview, (View *) box);
				release(box);
			}

			$((View *) columns, addSubview, (View *) column);
			release(column);
		}

		{
			StackView *column = $(alloc(StackView), initWithFrame, NULL);

			column->spacing = DEFAULT_PANEL_SPACING;

			column->view.autoresizingMask |= ViewAutoresizingHeight;

			{
				Box *box = $(alloc(Box), initWithFrame, NULL);
				$(box->label, setText, "Choose map");

				box->view.autoresizingMask |= ViewAutoresizingHeight;

				StackView *stackView = $(alloc(StackView), initWithFrame, NULL);

				stackView->spacing = DEFAULT_PANEL_SPACING;

				stackView->view.autoresizingMask |= ViewAutoresizingHeight;

				const SDL_Rect frame = MakeRect(0, 0, 510, 0);
				self->mapList = $(alloc(MapListCollectionView), initWithFrame, &frame, ControlStyleDefault);

				self->mapList->collectionView.control.view.autoresizingMask |= ViewAutoresizingHeight;

				$((View *) stackView, addSubview, (View *) self->mapList);

				$((View *) box, addSubview, (View *) stackView);
				release(stackView);

				$((View *) column, addSubview, (View *) box);
				release(box);
			}

			$((View *) columns, addSubview, (View *) column);
			release(column);
		}

		$((View *) self, addSubview, (View *) columns);
		release(columns);
	}

	return self;
}

#pragma mark - Class lifecycle

/**
 * @see Class::initialize(Class *)
 */
static void initialize(Class *clazz) {

	((ObjectInterface *) clazz->def->interface)->dealloc = dealloc;

	((CreateServerViewInterface *) clazz->def->interface)->initWithFrame = initWithFrame;
}

/**
 * @fn Class *CreateServerView::_CreateServerView(void)
 * @memberof CreateServerView
 */
Class *_CreateServerView(void) {
	static Class clazz;
	static Once once;

	do_once(&once, {
		clazz.name = "CreateServerView";
		clazz.superclass = _View();
		clazz.instanceSize = sizeof(CreateServerView);
		clazz.interfaceOffset = offsetof(CreateServerView, interface);
		clazz.interfaceSize = sizeof(CreateServerViewInterface);
		clazz.initialize = initialize;
	});

	return &clazz;
}

#undef _Class
