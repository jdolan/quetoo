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

#pragma once

#include "cg_types.h"

#include <ObjectivelyMVC.h>

/**
 * @file
 * @brief The Theme provides a high level API for user interface construction.
 */

#define THEME_INPUT_LABEL_WIDTH 200

typedef struct ThemeColors ThemeColors;

typedef struct Theme Theme;
typedef struct ThemeInterface ThemeInterface;

/**
 * @brief The Theme type.
 * @extends Object
 */
struct Theme {

	/**
	 * @brief The superclass.
	 */
	Object object;

	/**
	 * @brief The interface.
	 * @protected
	 */
	ThemeInterface *interface;

	/**
	 * @brief The theme colors.
	 */
	struct ThemeColors {
		SDL_Color main;
		SDL_Color mainHighlight;

		SDL_Color dark;
		SDL_Color darkHighlight;
		SDL_Color darkBorder;

		SDL_Color light;
		SDL_Color lightHighlight;
		SDL_Color lightBorder;

		SDL_Color dialog;
		SDL_Color dialogHighlight;

		SDL_Color contrast;
		SDL_Color watermark;
	} colors;

	/**
	 * @brief The target View to which new user interface elements will be added.
	 */
	View *target;
};

/**
 * @brief The Theme interface.
 */
struct ThemeInterface {

	/**
	 * @brief The superclass interface.
	 */
	ObjectInterface objectInterface;

	/**
	 * @fn StackView *Theme::accessories(const Theme *self)
	 * @brief Creates a StackView suitable for holding accessory buttons (_Apply_, _Cancel_).
	 * @param self The Theme.
	 * @return The StackView, or `NULL` on error.
	 * @memberof Theme
	 */
	StackView *(*accessories)(const Theme *self);

	/**
	 * @fn void Theme::attach(const Theme *self, ident view)
	 * @brief Attaches the given View to the target View.
	 * @param self The Theme.
	 * @param view The View.
	 * @memberof Theme
	 */
	void (*attach)(const Theme *self, ident view);

	/**
	 * @fn void Theme::bindTextView(const Theme *self, const char *label, const char *bind, TextViewDelegate *delegate)
	 * @brief Attaches a BindTextView with the given parameters to the target view.
	 * @param self The Theme.
	 * @param label The Label text.
	 * @param bind The bind.
	 * @param delegate The TextViewDelegate.
	 * @memberof Theme
	 */
	void (*bindTextView)(const Theme *self, const char *label, const char *bind, TextViewDelegate *delegate);

	/**
	 * @fn Box *Theme::box(const Theme *self, const char *label)
	 * @brief Creates a Box with the specified label text.
	 * @param self The Theme.
	 * @param label The Box Label text.
	 * @return The Box, or `NULL` on error.
	 * @memberof Theme
	 */
	Box *(*box)(const Theme *self, const char *label);

	/**
	 * @fn void Theme::button(const Theme *self, const char *title, ActionFunction action, ident sender, ident data)
	 * @brief Attaches a Button with the given parameters to the target View.
	 * @param self The Theme.
	 * @param title The Button title.
	 * @param function The ActionFunction for click events.
	 * @param sender The Action sender.
	 * @param data The Action data.
	 * @memberof Theme
	 */
	void (*button)(const Theme *self, const char *title, ActionFunction function, ident sender, ident data);

	/**
	 * @fn void Theme::checkbox(const Theme *self, const char *label, const char *name)
	 * @brief Attaches a CvarCheckbox for the specified variable to the target View.
	 * @param self The Theme.
	 * @param label The Label text.
	 * @param name The variable name.
	 * @memberof Theme
	 */
	void (*checkbox)(const Theme *self, const char *label, const char *name);

	/**
	 * @fn StackView *Theme::columns(const Theme *self, size_t count)
	 * @brief Creates a column layout with the specified column count.
	 * @param self The theme.
	 * @param count The number of columns.
	 * @return The columns StackView, or `NULL` on error.
	 * @memberof Theme
	 */
	StackView *(*columns)(const Theme *self, size_t count);

	/**
	 * @fn StackView *Theme::container(const Theme *self)
	 * @brief Creates a container.
	 * @param self The Theme.
	 * @return The container, or `NULL` on error.
	 * @memberof Theme
	 */
	StackView *(*container)(const Theme *self);

	/**
	 * @fn void Theme::control(const Theme *self, const char *label, ident control)
	 * @brief Attaches the given Control to the target View.
	 * @param self The Theme.
	 * @param label The Label text.
	 * @param control The Control.
	 * @memberof Theme
	 */
	void (*control)(const Theme *self, const char *label, ident control);

	/**
	 * @fn Theme *Theme::init(Theme *self)
	 * @brief Initializes this Theme.
	 * @param self The Theme.
	 * @return The initialized Theme, or `NULL` on error.
	 * @memberof Theme
	 */
	Theme *(*init)(Theme *self);

	/**
	 * @fn Theme *Theme::init(Theme *self)
	 * @brief Initializes this Theme with the given target View.
	 * @param self The Theme.
	 * @param target The target View.
	 * @return The initialized Theme, or `NULL` on error.
	 * @memberof Theme
	 */
	Theme *(*initWithTarget)(Theme *self, ident target);

	/**
	 * @fn Panel *Theme::panel(const Theme *self)
	 * @brief Creates a Panel.
	 * @param self The Theme.
	 * @return The Panel, or `NULL` on error.
	 * @memberof Theme
	 */
	Panel *(*panel)(const Theme *self);
	
	/**
	 * @fn void Theme::slider(const Theme *self, const char *label, const char *name, double min, double max, double step)
	 * @brief Attaches a CvarSlider for the specified variable to the target View.
	 * @param self The Theme.
	 * @param label The Label text.
	 * @param name The variable name.
	 * @param min The min value.
	 * @param max The max value.
	 * @param step The step.
	 * @param delegate The SliderDelegate.
	 * @memberof Theme
	 */
	void (*slider)(const Theme *self, const char *label, const char *name,
				   double min, double max, double step, SliderDelegate *delegate);

	/**
	 * @fn void Theme::target(Theme *self, ident target)
	 * @brief Sets the container View that the Theme will add new elements to.
	 * @param self The Theme.
	 * @param target The target View.
	 * @memberof Theme
	 */
	void (*target)(Theme *self, ident target);

	/**
	 * @fn void Theme::targetSubview(Theme *self, ident view, int index)
	 * @brief Sets the target View that the Theme will add new elements to.
	 * @param self The Theme.
	 * @param target The View.
	 * @param index The index of the subview to target.
	 * @memberof Theme
	 */
	void (*targetSubview)(Theme *self, ident view, int index);

	/**
	 * @fn void Theme::textView(const Theme *self, const char *label, const char *name)
	 * @brief Attaches a CvarTextView for the specified variable to the target View.
	 * @param self The Theme.
	 * @param label The Label text.
	 * @param name The variable name.
	 * @memberof Theme
	 */
	void (*textView)(const Theme *self, const char *label, const char *name);
};

/**
 * @fn Class *Theme::_Theme(void)
 * @brief The Theme archetype.
 * @return The Theme Class.
 * @memberof Theme
 */
OBJECTIVELY_EXPORT Class *_Theme(void);
