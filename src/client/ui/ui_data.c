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

#include "ui_data.h"
#include "client.h"

/**
 * @brief
 */
Data *Ui_Data(const char *path) {

	Data *data = NULL;

	void *buffer;
	const int64_t length = Fs_Load(path, &buffer);
	if (length != -1) {
		data = $$(Data, dataWithBytes, buffer, length);
		assert(data);

		Fs_Free(buffer);
	} else {
		Com_Warn("Failed to load Data %s\n", path);
	}

	return data;
}

/**
 * @brief
 */
Font *Ui_Font(const char *path, const char *family, int32_t size, int32_t style) {

	Font *font = NULL;

	Data *data = Ui_Data(path);
	if (data) {

		font = $(alloc(Font), initWithData, data, family, size, style);
		assert(font);

		release(data);
	} else {
		Com_Warn("Failed to load Font %s\n", path);
	}

	return font;
}

/**
 * @brief
 */
Image *Ui_Image(const char *path) {

	Image *image = NULL;

	SDL_Surface *surface;
	if (Img_LoadImage(path, &surface)) {

		image = $(alloc(Image), initWithSurface, surface);
		assert(image);

		SDL_FreeSurface(surface);
	} else {
		Com_Warn("Failed to load Image %s\n", path);
	}

	return image;
}

/**
 * @brief
 */
Stylesheet *Ui_Stylesheet(const char *path) {

	Stylesheet *stylesheet = NULL;

	void *buffer;
	if (Fs_Load(path, &buffer) != -1) {

		stylesheet = $$(Stylesheet, stylesheetWithCharacters, (char *) buffer);
		assert(stylesheet);

		Fs_Free(buffer);
	} else {
		Com_Warn("Failed to load Stylesheet %s\n", path);
	}

	return stylesheet;
}

/**
 * @brief
 */
Theme *Ui_Theme(void) {
	return $$(Theme, theme, r_context.window);
}

/**
 * @brief
 */
View *Ui_View(const char *path, Outlet *outlets) {

	View *view = NULL;

	Data *data = Ui_Data(path);
	if (data) {

		view = $$(View, viewWithData, data, outlets);
		assert(view);

		release(data);
	} else {
		Com_Warn("Failed to load View %s\n", path);
	}


	return view;
}

/**
 * @brief
 */
void Ui_WakeView(View *view, const char *path, Outlet *outlets) {

	assert(view);

	Data *data = Ui_Data(path);
	if (data) {
		$(view, awakeWithData, data);
		$(view, resolve, outlets);

		release(data);
	} else {
		Com_Warn("Failed to wake View %s\n", path);
	}
}
