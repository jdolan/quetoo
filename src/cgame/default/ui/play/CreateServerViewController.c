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

#define _Class _CreateServerViewController

#pragma mark - Actions and delegate callbacks

/**
 * @brief Select teams mode
 */
static void selectTeams(Select *select, Option *option) {

	const intptr_t value = (intptr_t) option->value;
	switch (value) {
		case 1:
			cgi.SetCvarInteger(g_teams->name, 1);
			cgi.SetCvarInteger(g_ctf->name, 0);
			break;
		case 2:
			cgi.SetCvarInteger(g_teams->name, 0);
			cgi.SetCvarInteger(g_ctf->name, 1);
			break;
		default:
			cgi.SetCvarInteger(g_teams->name, 0);
			cgi.SetCvarInteger(g_ctf->name, 1);
			break;
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
				Cg_Debug("Wrote %s %"PRId64" bytes\n", MAP_LIST_UI, len);
			}

			release(string);
		}

		cgi.CloseFile(file);

		cgi.SetCvarString("g_map_list", MAP_LIST_UI);
		cgi.Cbuf(va("map %s", map));

	} else {
		cgi.Print("No maps selected\n");
	}

	release(selectedMaps);
}

#pragma mark - ViewController

/**
 * @see ViewController::loadView(ViewController *)
 */
static void loadView(ViewController *self) {

	super(ViewController, self, loadView);

	CreateServerViewController *this = (CreateServerViewController *) self;

	Outlet outlets[] = MakeOutlets(
		MakeOutlet("gameplay", &this->gameplay),
		MakeOutlet("teams", &this->teams),
		MakeOutlet("mapList", &this->mapList),
		MakeOutlet("create", &this->create)
	);

	$(self->view, awakeWithResourceName, "ui/play/CreateServerViewController.json");
	$(self->view, resolve, outlets);

	self->view->stylesheet = $$(Stylesheet, stylesheetWithResourceName, "ui/play/CreateServerViewController.css");
	assert(self->view->stylesheet);
	
	$(this->gameplay, addOption, "Default", "default");
	$(this->gameplay, addOption, "Deathmatch", "deathmatch");
	$(this->gameplay, addOption, "Instagib", "instagib");
	$(this->gameplay, addOption, "Arena", "arena");
	$(this->gameplay, addOption, "Duel", "duel");

	$(this->teams, addOption, "Free for all", (ident) 0);
	$(this->teams, addOption, "Team deathmatch", (ident) 1);
	$(this->teams, addOption, "Capture the flag", (ident) 2);

	this->teams->delegate.didSelectOption = selectTeams;

	$((Control *) this->create, addActionForEventType, SDL_MOUSEBUTTONUP, createAction, self, NULL);
}

#pragma mark - Class lifecycle

/**
 * @see Class::initialize(Class *)
 */
static void initialize(Class *clazz) {

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
