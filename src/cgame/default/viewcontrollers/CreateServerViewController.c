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

#define _Class _CreateServerViewController

#pragma mark - Actions and delegate callbacks

/**
 * @brief ActionFunction for the Create button.
 */
static void createAction(Control *control, const SDL_Event *event, ident sender, ident data) {

	CreateServerViewController *this = (CreateServerViewController *) sender;

	GList *selectedMaps = $(this->mapList, selectedMaps);
	for (const GList *list = selectedMaps; list; list = list->next) {

		g_map_list_map_t *map = cgi.Malloc(sizeof(g_map_list_map_t), MEM_TAG_CGAME);

		g_strlcpy(map->name, (const char *) list->data, sizeof(map->name));
		printf("selected %s\n", map->name);

		//mapList = g_list_append(mapList, map);
	}

	g_list_free(selectedMaps);
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

	CreateServerViewController *this = (CreateServerViewController *) self;

	StackView *columns = $(alloc(StackView), initWithFrame, NULL);

	columns->axis = StackViewAxisHorizontal;
	columns->spacing = DEFAULT_PANEL_SPACING;

	{
		StackView *column = $(alloc(StackView), initWithFrame, NULL);
		column->spacing = DEFAULT_PANEL_SPACING;

		{
			Box *box = $(alloc(Box), initWithFrame, NULL);
			$(box->label, setText, "CREATE SERVER");

			StackView *stackView = $(alloc(StackView), initWithFrame, NULL);

			Cg_CvarTextView((View *) stackView, "Hostname", "sv_hostname");
			Cg_CvarTextView((View *) stackView, "Clients", "sv_max_clients");
			Cg_CvarCheckboxInput((View *) stackView, "Public", "sv_public");
			Cg_CvarTextView((View *) stackView, "Password", "password");

			$((View *) box, addSubview, (View *) stackView);
			release(stackView);

			$((View *) column, addSubview, (View *) box);
			release(box);
		}

		{
			Box *box = $(alloc(Box), initWithFrame, NULL);
			$(box->label, setText, "GAME");

			StackView *stackView = $(alloc(StackView), initWithFrame, NULL);

			this->gameplay = $(alloc(Select), initWithFrame, NULL, ControlStyleDefault);

			$(this->gameplay, addOption, "Default", "default");
			$(this->gameplay, addOption, "Deathmatch", "deathmatch");
			$(this->gameplay, addOption, "Instagib", "instagib");
			$(this->gameplay, addOption, "Arena", "arena");
			$(this->gameplay, addOption, "Duel", "duel");

			this->gameplay->control.view.frame.w = DEFAULT_TEXTVIEW_WIDTH;

			Cg_Input((View *) stackView, "Gameplay", (Control *) this->gameplay);

			this->teamsplay = $(alloc(Select), initWithFrame, NULL, ControlStyleDefault);

			$(this->teamsplay, addOption, "Free for All", "free-for-all");
			$(this->teamsplay, addOption, "Team Deathmatch", "team-deathmatch");
			$(this->teamsplay, addOption, "Capture the Flag", "capture-the-flag");

			this->teamsplay->control.view.frame.w = DEFAULT_TEXTVIEW_WIDTH;

			Cg_Input((View *) stackView, "Teams play", (Control *) this->teamsplay);

			this->matchMode = $(alloc(Checkbox), initWithFrame, NULL, ControlStyleDefault);

			Cg_Input((View *) stackView, "Match mode", (Control *) this->matchMode);

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

		{
			Box *box = $(alloc(Box), initWithFrame, NULL);
			$(box->label, setText, "MAP LIST");

			StackView *stackView = $(alloc(StackView), initWithFrame, NULL);
			stackView->spacing = DEFAULT_PANEL_SPACING;

			const SDL_Rect frame = { .w = 760, .h = 600 };
			this->mapList = $(alloc(MapListCollectionView), initWithFrame, &frame, ControlStyleDefault);
			
			$((View *) stackView, addSubview, (View *) this->mapList);

			$((View *) box, addSubview, (View *) stackView);
			release(stackView);

			$((View *) column, addSubview, (View *) box);
			release(box);
		}

		$((View *) columns, addSubview, (View *) column);
		release(column);
	}

	$((View *) this->menuViewController.panel->contentView, addSubview, (View *) columns);
	release(columns);

	this->menuViewController.panel->accessoryView->view.hidden = false;
	Cg_Button((View *) this->menuViewController.panel->accessoryView, "Create", createAction, self, NULL);

}

#pragma mark - MapListCollectionView

#pragma mark - Class lifecycle

/**
 * @see Class::initialize(Class *)
 */
static void initialize(Class *clazz) {

	((ObjectInterface *) clazz->def->interface)->dealloc = dealloc;

	((ViewControllerInterface *) clazz->def->interface)->loadView = loadView;
}

Class _CreateServerViewController = {
	.name = "CreateServerViewController",
	.superclass = &_MenuViewController,
	.instanceSize = sizeof(CreateServerViewController),
	.interfaceOffset = offsetof(CreateServerViewController, interface),
	.interfaceSize = sizeof(CreateServerViewControllerInterface),
	.initialize = initialize,
};

#undef _Class

