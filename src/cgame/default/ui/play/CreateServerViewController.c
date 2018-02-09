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

#include "CreateServerViewController.h"
#include "MapListCollectionItemView.h"
#include "QuetooTheme.h"

#define _Class _CreateServerViewController

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

	CreateServerViewController *this = (CreateServerViewController *) sender;

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

	CreateServerViewController *this = (CreateServerViewController *) self;

	release(this->gameplay);
	release(this->mapList);
	release(this->matchMode);
	release(this->teamsplay);

	super(Object, self, dealloc);
}

#pragma mark - ViewController

/**
 * @see ViewController::loadView(ViewController *)
 */
static void loadView(ViewController *self) {

	super(ViewController, self, loadView);

	self->view->identifier = strdup("Create");
	self->view->stylesheet = cgi.Stylesheet("ui/play/CreateServerViewController.css");

	QuetooTheme *theme = $(alloc(QuetooTheme), initWithTarget, self->view);
	assert(theme);

	CreateServerViewController *this = (CreateServerViewController *) self;

	StackView *container = $(theme, container);

	$(theme, attach, container);
	$(theme, target, container);

	StackView *columns = $(theme, columns, 2);

	$(theme, attach, columns);
	$(theme, targetSubview, columns, 0);

	{
		Box *box = $(theme, box, "Server");

		$(theme, attach, box);
		$(theme, target, box->contentView);

		$(theme, textView, "Hostname", "sv_hostname");
		$(theme, textView, "Clients", "sv_max_clients");
		$(theme, checkbox, "Public", "sv_public");
		$(theme, textView, "Password", "password");

		release(box);
	}

	$(theme, targetSubview, columns, 0);

	{
		Box *box = $(theme, box, "Game");

		$(theme, attach, box);
		$(theme, target, box->contentView);

		this->gameplay = $(alloc(Select), initWithFrame, NULL);
		assert(this->gameplay);

		$(this->gameplay, addOption, "Default", (ident) 0);
		$(this->gameplay, addOption, "Deathmatch", (ident) 1);
		$(this->gameplay, addOption, "Instagib", (ident) 2);
		$(this->gameplay, addOption, "Arena", (ident) 3);
		$(this->gameplay, addOption, "Duel", (ident) 4);

		this->gameplay->control.view.frame.w = 160;
		this->gameplay->delegate.didSelectOption = selectGameplay;

		if (!g_strcmp0(g_gameplay->string, "default")) {
			$(this->gameplay, selectOptionWithValue, (ident) 0);
		} else if (!g_strcmp0(g_gameplay->string, "deathmatch")) {
			$(this->gameplay, selectOptionWithValue, (ident) 1);
		} else if (!g_strcmp0(g_gameplay->string, "instagib")) {
			$(this->gameplay, selectOptionWithValue, (ident) 2);
		} else if (!g_strcmp0(g_gameplay->string, "arena")) {
			$(this->gameplay, selectOptionWithValue, (ident) 3);
		} else if (!g_strcmp0(g_gameplay->string, "duel")) {
			$(this->gameplay, selectOptionWithValue, (ident) 4);
		}

		$(theme, control, "Gameplay", (Control *) this->gameplay);

		this->teamsplay = $(alloc(Select), initWithFrame, NULL);
		assert(this->teamsplay);

		$(this->teamsplay, addOption, "Free for all", (ident) 0);
		$(this->teamsplay, addOption, "Team deathmatch", (ident) 1);
		$(this->teamsplay, addOption, "Capture the flag", (ident) 2);

		this->teamsplay->delegate.didSelectOption = selectTeamsplay;

		if (g_ctf->integer != 0) {
			$(this->teamsplay, selectOptionWithValue, (ident) 2);
		} else {
			$(this->teamsplay, selectOptionWithValue, (ident) (ptrdiff_t) g_teams->integer);
		}

		$(theme, control, "Teams play", (Control *) this->teamsplay);
		$(theme, checkbox, "Match mode", "g_match");

		release(box);
	}

	$(theme, targetSubview, columns, 1);

	{
		Box *box = $(theme, box, "Map list");

		$(theme, attach, box);
		$(theme, target, box->contentView);

		this->mapList = $(alloc(MapListCollectionView), initWithFrame, NULL);
		assert(this->mapList);

		$(theme, attach, this->mapList);

		release(box);
	}

	$(theme, target, container);

	StackView *accessories = $(theme, accessories);

	$(theme, attach, accessories);
	$(theme, target, accessories);

	$(theme, button, "Create", createAction, self, NULL);

	release(accessories);
	release(columns);
	release(container);
	release(theme);
}

#pragma mark - Class lifecycle

/**
 * @see Class::initialize(Class *)
 */
static void initialize(Class *clazz) {

	((ObjectInterface *) clazz->interface)->dealloc = dealloc;

	((ViewControllerInterface *) clazz->interface)->loadView = loadView;
}

/**
 * @fn Class *CreateServerViewController::_CreateServerViewController(void)
 * @memberof CreateServerViewController
 */
Class *_CreateServerViewController(void) {
	static Class *clazz;
	static Once once;

	do_once(&once, {
		clazz = _initialize(&(const ClassDef) {
			.name = "CreateServerViewController",
			.superclass = _ViewController(),
			.instanceSize = sizeof(CreateServerViewController),
			.interfaceOffset = offsetof(CreateServerViewController, interface),
			.interfaceSize = sizeof(CreateServerViewControllerInterface),
			.initialize = initialize,
		});
	});

	return clazz;
}

#undef _Class
