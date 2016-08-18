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

#include <assert.h>

#include "MultiplayerViewController.h"

#include "ServersTableView.h"

#include "client.h"

extern cl_static_t cls;

#define _Class _MultiplayerViewController

#pragma mark - Actions

/**
 * @brief ActionFunction for the Refresh Button.
 */
static void refreshAction(Control *control, const SDL_Event *event, ident sender, ident data) {
	Cl_Servers_f();
}

/**
 * @brief ActionFunction for the Connect Button.
 */
static void connectAction(Control *control, const SDL_Event *event, ident sender, ident data) {

	if ($((Object *) control, isKindOfClass, &_TableView)) {
		if (event->button.clicks < 2) {
			return;
		}
	}

	const TableView *servers = (TableView *) data;

	if (servers->selectedRow != -1) {
		const cl_server_info_t *server = g_list_nth_data(cls.servers, servers->selectedRow);
		if (server) {
			Cbuf_AddText(va("connect %s\n", Net_NetaddrToString(&server->addr)));
		}
	}
}

#pragma mark - ViewController

/**
 * @see ViewController::loadView(ViewController *)
 */
static void loadView(ViewController *self) {

	super(ViewController, self, loadView);

	Cl_Servers_f();

	MenuViewController *this = (MenuViewController *) self;

	{
		Box *box = $(alloc(Box), initWithFrame, NULL);
		box->view.autoresizingMask = ViewAutoresizingContain;

		$(box->label, setText, "JOIN GAME");

		StackView *stackView = $(alloc(StackView), initWithFrame, NULL);
		stackView->view.padding.top = 4;

		ServersTableView *servers = $(alloc(ServersTableView), initWithFrame, NULL, ControlStyleDefault);
		$((View *) servers, sizeToFit);

		servers->tableView.control.view.frame.h = 320;

		$((Control *) servers, addActionForEventType, SDL_MOUSEBUTTONUP, connectAction, self, servers);

		$((View *) stackView, addSubview, (View *) servers);
		release(servers);

		{
			StackView *buttons = $(alloc(StackView), initWithFrame, NULL);
			buttons->axis = StackViewAxisHorizontal;
			buttons->spacing = DEFAULT_INPUT_SPACING;
			buttons->view.alignment = ViewAlignmentMiddleRight;
			buttons->view.padding.top = buttons->view.padding.bottom = DEFAULT_PANEL_PADDING;

			Button *refresh = $(alloc(Button), initWithFrame, NULL, ControlStyleDefault);
			$(refresh->title, setText, "Refresh");

			$((Control *) refresh, addActionForEventType, SDL_MOUSEBUTTONUP, refreshAction, self, NULL);

			$((View *) buttons, addSubview, (View *) refresh);
			release(refresh);

			Button *connect = $(alloc(Button), initWithFrame, NULL, ControlStyleDefault);
			$(connect->title, setText, "Connect");

			$((Control *) connect, addActionForEventType, SDL_MOUSEBUTTONUP, connectAction, self, servers);

			$((View *) buttons, addSubview, (View *) connect);
			release(connect);

			$((View *) buttons, sizeToFit);

			$((View *) stackView, addSubview, (View *) buttons);
			release(buttons);
		}

		$((View *) stackView, sizeToFit);

		$((View *) box, addSubview, (View *) stackView);
		release(stackView);

		$((View *) box, sizeToFit);

		$((View *) this->stackView, addSubview, (View *) box);
		release(box);
	}

	$((View *) this->stackView, sizeToFit);
	$((View *) this->panel, sizeToFit);
}

#pragma mark - Class lifecycle

/**
 * @see Class::initialize(Class *)
 */
static void initialize(Class *clazz) {

	((ViewControllerInterface *) clazz->interface)->loadView = loadView;
}

Class _MultiplayerViewController = {
	.name = "MultiplayerViewController",
	.superclass = &_MenuViewController,
	.instanceSize = sizeof(MultiplayerViewController),
	.interfaceOffset = offsetof(MultiplayerViewController, interface),
	.interfaceSize = sizeof(MultiplayerViewControllerInterface),
	.initialize = initialize,
};

#undef _Class

