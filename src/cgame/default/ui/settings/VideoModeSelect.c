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

#include "VideoModeSelect.h"

#define _Class _VideoModeSelect

#pragma mark - Object

static void dealloc(Object *self) {

	VideoModeSelect *this = (VideoModeSelect *) self;

	g_free(this->modes);

	super(Object, self, dealloc);
}

#pragma mark - View

/**
 * @see View::updateBindings(View *)
 */
static void updateBindings(View *self) {

	super(View, self, updateBindings);

	VideoModeSelect *this = (VideoModeSelect *) self;

	g_free(this->modes);

	Select *select = (Select *) this;

	$(select, removeAllOptions);
	$(select, addOption, "Custom", NULL);
	$(select, selectOptionWithValue, NULL);

	const int32_t display = SDL_GetWindowDisplayIndex($(self, window));
	const int32_t numDisplayModes = SDL_GetNumDisplayModes(display);

	if (numDisplayModes) {
		this->modes = g_malloc(sizeof(SDL_DisplayMode) * numDisplayModes);
		assert(this->modes);

		SDL_DisplayMode *mode = this->modes;
		for (int32_t i = 0; i < numDisplayModes; i++, mode++) {
			SDL_GetDisplayMode(display, i, mode);

			if (SDL_BITSPERPIXEL(mode->format) == 32) {

				char *title = va("%dx%d @ %dHz", mode->w, mode->h, mode->refresh_rate);
				$(select, addOption, title, mode);

				if (mode->w == cgi.context->window_width && mode->h == cgi.context->window_height) {
					select->selectedOption = $(select, optionWithValue, mode);
				}
			}
		}
	}

	$(self, sizeToFit);
}

#pragma mark - VideoModeSelect

/**
 * @fn VideoModeSelect *VideoModeSelect::init(VideoModeSelect *self)
 * @memberof VideoModeSelect
 */
static VideoModeSelect *initWithFrame(VideoModeSelect *self, const SDL_Rect *frame, ControlStyle style) {

	self = (VideoModeSelect *) super(Select, self, initWithFrame, frame, style);
	if (self) {
		$((View *) self, updateBindings);
	}

	return self;
}

#pragma mark - Class lifecycle

/**
 * @see Class::initialize(Class *)
 */
static void initialize(Class *clazz) {

	((ObjectInterface *) clazz->def->interface)->dealloc = dealloc;

	((ViewInterface *) clazz->def->interface)->updateBindings = updateBindings;

	((VideoModeSelectInterface *) clazz->def->interface)->initWithFrame = initWithFrame;
}

/**
 * @fn Class *VideoModeSelect::_VideoModeSelect(void)
 * @memberof VideoModeSelect
 */
Class *_VideoModeSelect(void) {
	static Class clazz;
	static Once once;

	do_once(&once, {
		clazz.name = "VideoModeSelect";
		clazz.superclass = _Select();
		clazz.instanceSize = sizeof(VideoModeSelect);
		clazz.interfaceOffset = offsetof(VideoModeSelect, interface);
		clazz.interfaceSize = sizeof(VideoModeSelectInterface);
		clazz.initialize = initialize;
	});

	return &clazz;
}

#undef _Class

