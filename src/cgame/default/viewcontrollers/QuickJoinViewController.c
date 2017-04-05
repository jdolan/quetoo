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

#include "QuickJoinViewController.h"

#define _Class _QuickJoinViewController

#pragma mark - Actions

/**
 * @brief ActionFunction for the Quickjoin button.
 * @description Selects a server based on minumum ping and maximum players with
 * a bit of lovely random thrown in. Any server that matches the criteria will
 * be weighted by how much "better" they are by how much lower their ping is and
 * how many more players there are.
 */
static void quickJoinAction(Control *control, const SDL_Event *event, ident sender, ident data) {

	const int16_t max_ping = Clamp(cg_quick_join_max_ping->integer, 0, 999);
	const int16_t min_clients = Clamp(cg_quick_join_min_clients->integer, 0, MAX_CLIENTS);

	uint32_t total_weight = 0;

	cgi.GetServers();

	const GList *list = cgi.Servers();

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

	list = cgi.Servers();

	uint32_t random_weight = Random() % total_weight;
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

#pragma mark - ViewController

/**
 * @see ViewController::loadView(ViewController *)
 */
static void loadView(ViewController *self) {

	super(ViewController, self, loadView);

	TabViewController *this = (TabViewController *) self;

	StackView *columns = $(alloc(StackView), initWithFrame, NULL);

	columns->axis = StackViewAxisHorizontal;
	columns->spacing = DEFAULT_PANEL_SPACING;

	{
		StackView *column = $(alloc(StackView), initWithFrame, NULL);

		columns->spacing = DEFAULT_PANEL_SPACING;

		{
			Box *box = $(alloc(Box), initWithFrame, NULL);
			$(box->label, setText, "Filters");

			StackView *stackView = $(alloc(StackView), initWithFrame, NULL);

			Cg_CvarSliderInput((View *) stackView, "Maximum ping", "cg_quick_join_max_ping", 5.0, 500.0, 5.0);
			Cg_CvarSliderInput((View *) stackView, "Minimum players", "cg_quick_join_min_clients", 1.0, MAX_CLIENTS, 1.0);

			$((View *) box, addSubview, (View *) stackView);
			release(stackView);

			$((View *) column, addSubview, (View *) box);
			release(box);
		}

		$((View *) columns, addSubview, (View *) column);
		release(column);
	}

	$((View *) this->panel->contentView, addSubview, (View *) columns);
	release(columns);

	this->panel->accessoryView->view.hidden = false;

	Cg_Button((View *) this->panel->accessoryView, "Join", quickJoinAction, self, NULL);
}

#pragma mark - Class lifecycle

/**
 * @see Class::initialize(Class *)
 */
static void initialize(Class *clazz) {

	((ViewControllerInterface *) clazz->def->interface)->loadView = loadView;
}

/**
 * @fn Class *QuickJoinViewController::_QuickJoinViewController(void)
 * @memberof QuickJoinViewController
 */
Class *_QuickJoinViewController(void) {
	static Class clazz;
	static Once once;

	do_once(&once, {
		clazz.name = "QuickJoinViewController";
		clazz.superclass = _TabViewController();
		clazz.instanceSize = sizeof(QuickJoinViewController);
		clazz.interfaceOffset = offsetof(QuickJoinViewController, interface);
		clazz.interfaceSize = sizeof(QuickJoinViewControllerInterface);
		clazz.initialize = initialize;
	});

	return &clazz;
}

#undef _Class
