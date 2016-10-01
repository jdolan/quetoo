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

#include "SkinSelect.h"

extern cvar_t *cg_skin;

#define _Class _SkinSelect

/**
 * @brief Comparator for SkinSelect.
 */
static Order sortSkins(const ident a, const ident b) {

	const char *c = ((const Option *) a)->title->text;
	const char *d = ((const Option *) b)->title->text;

	return g_strcmp0(c, d) < 0 ? OrderAscending : OrderDescending;
}

/**
 * @brief Fs_EnumerateFunc for resolving available skins for a give model.
 */
static void enumerateSkins(const char *path, void *data) {
	char name[MAX_QPATH];

	Select *this = (Select *) data;

	char *s = strstr(path, "players/");
	if (s) {
		StripExtension(s + strlen("players/"), name);

		if (g_str_has_suffix(name, "_i")) {
			name[strlen(name) - strlen("_i")] = '\0';

			Array *options = (Array *) this->options;
			for (size_t i = 0; i < options->count; i++) {

				const Option *option = $(options, objectAtIndex, i);
				if (g_strcmp0(option->title->text, name) == 0) {
					return;
				}
			}

			ident value = (ident) options->count;
			$(this, addOption, name, value);

			if (g_strcmp0(cg_skin->string, name) == 0) {
				$(this, selectOptionWithValue, value);
			}
		}
	}
}

/**
 * @brief Fs_EnumerateFunc for resolving available models.
 */
static void enumerateModels(const char *path, void *data) {
	cgi.EnumerateFiles(va("%s/*.tga", path), enumerateSkins, data);
}

/**
 * @brief SelectDelegate callback.
 */
static void didSelectOption(Select *select, Option *option) {
	cgi.CvarSet(cg_skin->name, option->title->text);
}

#pragma mark - SkinSelect

/**
 * @fn SkinSelect *SkinSelect::initWithFrame(SkinSelect *self, const SDL_Rect *frame, ControlStyle style)
 *
 * @memberof SkinSelect
 */
SkinSelect *initWithFrame(SkinSelect *self, const SDL_Rect *frame, ControlStyle style) {

	self = (SkinSelect *) super(Select, self, initWithFrame, frame, style);
	if (self) {

		self->select.comparator = sortSkins;
		self->select.delegate.didSelectOption = didSelectOption;

		cgi.EnumerateFiles("players/*", enumerateModels, self);

		$((View *) self, sizeToFit);
	}
	
	return self;
}

#pragma mark - Class lifecycle

/**
 * @see Class::initialize(Class *)
 */
static void initialize(Class *clazz) {

	((SkinSelectInterface *) clazz->interface)->initWithFrame = initWithFrame;
}

Class _SkinSelect = {
	.name = "SkinSelect",
	.superclass = &_Select,
	.instanceSize = sizeof(SkinSelect),
	.interfaceOffset = offsetof(SkinSelect, interface),
	.interfaceSize = sizeof(SkinSelectInterface),
	.initialize = initialize,
};

#undef _Class

