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

#include "TeamPlayerView.h"
#include "TeamView.h"

#define _Class _TeamView

#pragma mark - View

/**
 * @see View::updateBindings(View *)
 */
static void updateBindings(View *self) {

	super(View, self, updateBindings);

	TeamView *this = (TeamView *) self;

	if (this->team) {
		const color32_t c = Color_Color32(this->team->color);

		$(this->name->text, setText, this->team->name);
		$(this->name->text->view.style, addColorAttribute, "color", &MakeColor(c.r, c.g, c.b, c.a));
	} else {
		$(this->name->text, setText, "All players");
		$(this->name->text->view.style, removeAttribute, "color");
	}

	$((View *) this->players, removeAllSubviews);

	const cg_client_info_t *client = cg_state.clients;
	for (int32_t i = 0; i < MAX_CLIENTS; i++, client++) {

		if (*client->info && client->team == this->team) {
			TeamPlayerView *player = $(alloc(TeamPlayerView), initWithFrame, NULL);
			$(player, setPlayer, client);

			$((View *) this->players, addSubview, (View *) player);
			release(player);
		}
	}
}

#pragma mark - TeamView

/**
 * @fn TeamView *TeamView::initWithFrame(TeamView *, const SDL_Rect *)
 * @memberof TeamView
 */
static TeamView *initWithFrame(TeamView *self, const SDL_Rect *frame) {

	self = (TeamView *) super(View, self, initWithFrame, frame);
	if (self) {

		Outlet outlets[] = MakeOutlets(
			MakeOutlet("name", &self->name),
			MakeOutlet("players", &self->players)
		);

		View *this = (View *) self;

		$(this, awakeWithResourceName, "ui/teams/TeamView.json");
		$(this, resolve, outlets);
	}

	return self;
}

/**
 * @fn void TeamView::setTeam(TeamView *, const cg_team_info_t *)
 * @memberof TeamView
 */
static void setTeam(TeamView *self, const cg_team_info_t *team) {

	self->team = team;

	$((View *) self, updateBindings);
}

#pragma mark - Class lifecycle

/**
 * @see Class::initialize(Class *)
 */
static void initialize(Class *clazz) {

	((ViewInterface *) clazz->interface)->updateBindings = updateBindings;

	((TeamViewInterface *) clazz->interface)->initWithFrame = initWithFrame;
	((TeamViewInterface *) clazz->interface)->setTeam = setTeam;
}

/**
 * @fn Class *TeamView::_TeamView(void)
 * @memberof TeamView
 */
Class *_TeamView(void) {
	static Class *clazz;
	static Once once;

	do_once(&once, {
		clazz = _initialize(&(const ClassDef) {
			.name = "TeamView",
			.superclass = _View(),
			.instanceSize = sizeof(TeamView),
			.interfaceOffset = offsetof(TeamView, interface),
			.interfaceSize = sizeof(TeamViewInterface),
			.initialize = initialize,
		});
	});

	return clazz;
}

#undef _Class
