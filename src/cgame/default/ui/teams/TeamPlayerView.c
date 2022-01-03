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

#define _Class _TeamPlayerView

#pragma mark - View

/**
 * @see View::updateBindings(View *)
 */
static void updateBindings(View *self) {

	super(View, self, updateBindings);

	TeamPlayerView *this = (TeamPlayerView *) self;

	if (this->client && strlen(this->client->info)) {

		char name[MAX_USER_INFO_VALUE];
		StrStrip(this->client->name, name);

		$(this->name->text, setText, name);

		SDL_Surface *surface = cgi.LoadSurface(this->client->icon->media.name);
		$(this->icon, setImageWithSurface, surface);
		SDL_FreeSurface(surface);
		
	} else {
		$(this->name->text, setText, NULL);
		$(this->icon, setImage, NULL);
	}
}

#pragma mark - TeamPlayerView

/**
 * @fn TeamPlayerView *TeamPlayerView::initWithFrame(TeamPlayerView *, const SDL_Rect *)
 * @memberof TeamPlayerView
 */
static TeamPlayerView *initWithFrame(TeamPlayerView *self, const SDL_Rect *frame) {

	self = (TeamPlayerView *) super(View, self, initWithFrame, frame);
	if (self) {

		Outlet outlets[] = MakeOutlets(
			MakeOutlet("icon", &self->icon),
			MakeOutlet("name", &self->name)
		);

		View *this = (View *) self;

		$(this, awakeWithResourceName, "ui/teams/TeamPlayerView.json");
		$(this, resolve, outlets);
	}

	return self;
}

/**
 * @fn void TeamPlayerView::setPlayer(TeamPlayerView *, const cg_client_info_t *)
 * @memberof TeamPlayerView
 */
static void setPlayer(TeamPlayerView *self, const cg_client_info_t *client) {

	self->client = client;

	$((View *) self, updateBindings);
}

#pragma mark - Class lifecycle

/**
 * @see Class::initialize(Class *)
 */
static void initialize(Class *clazz) {

	((ViewInterface *) clazz->interface)->updateBindings = updateBindings;

	((TeamPlayerViewInterface *) clazz->interface)->initWithFrame = initWithFrame;
	((TeamPlayerViewInterface *) clazz->interface)->setPlayer = setPlayer;
}

/**
 * @fn Class *TeamPlayerView::_TeamPlayerView(void)
 * @memberof TeamPlayerView
 */
Class *_TeamPlayerView(void) {
	static Class *clazz;
	static Once once;

	do_once(&once, {
		clazz = _initialize(&(const ClassDef) {
			.name = "TeamPlayerView",
			.superclass = _View(),
			.instanceSize = sizeof(TeamPlayerView),
			.interfaceOffset = offsetof(TeamPlayerView, interface),
			.interfaceSize = sizeof(TeamPlayerViewInterface),
			.initialize = initialize,
		});
	});

	return clazz;
}

#undef _Class
