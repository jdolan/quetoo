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
 * @brief The QuetooTheme provides a high level API for user interface construction.
 */

typedef struct QuetooThemeColors QuetooThemeColors;

typedef struct QuetooTheme QuetooTheme;
typedef struct QuetooThemeInterface QuetooThemeInterface;

/**
 * @brief The QuetooTheme type.
 * @extends Object
 */
struct QuetooTheme {

	/**
	 * @brief The superclass.
	 */
	Object object;

	/**
	 * @brief The interface.
	 * @protected
	 */
	QuetooThemeInterface *interface;

	/**
	 * @brief The theme colors.
	 */
	struct QuetooThemeColors {
		SDL_Color main;
		SDL_Color mainHighlight;

		SDL_Color dark;
		SDL_Color darkHighlight;
		SDL_Color darkBorder;

		SDL_Color light;
		SDL_Color lightHighlight;
		SDL_Color lightBorder;

		SDL_Color contrast;
		SDL_Color watermark;
	} colors;

	/**
	 * @brief The target View to which new user interface elements will be added.
	 */
	View *target;
};

/**
 * @brief The QuetooTheme interface.
 */
struct QuetooThemeInterface {

	/**
	 * @brief The superclass interface.
	 */
	ObjectInterface objectInterface;

	/**
	 * @fn StackView *QuetooTheme::accessories(const QuetooTheme *self)
	 * @brief Creates a StackView suitable for holding accessory buttons (_Apply_, _Cancel_).
	 * @param self The QuetooTheme.
	 * @return The StackView, or `NULL` on error.
	 * @memberof QuetooTheme
	 */
	StackView *(*accessories)(const QuetooTheme *self);

	/**
	 * @fn void QuetooTheme::attach(const QuetooTheme *self, ident view)
	 * @brief Attaches the given View to the target View.
	 * @param self The QuetooTheme.
	 * @param view The View.
	 * @memberof QuetooTheme
	 */
	void (*attach)(const QuetooTheme *self, ident view);

	/**
	 * @fn void QuetooTheme::bindTextView(const QuetooTheme *self, const char *label, const char *bind, TextViewDelegate *delegate)
	 * @brief Attaches a BindTextView with the given parameters to the target view.
	 * @param self The QuetooTheme.
	 * @param label The Label text.
	 * @param bind The bind.
	 * @param delegate The TextViewDelegate.
	 * @memberof QuetooTheme
	 */
	void (*bindTextView)(const QuetooTheme *self, const char *label, const char *bind, TextViewDelegate *delegate);

	/**
	 * @fn Box *QuetooTheme::box(const QuetooTheme *self, const char *label)
	 * @brief Creates a Box with the specified label text.
	 * @param self The QuetooTheme.
	 * @param label The Box Label text.
	 * @return The Box, or `NULL` on error.
	 * @memberof QuetooTheme
	 */
	Box *(*box)(const QuetooTheme *self, const char *label);

	/**
	 * @fn void QuetooTheme::button(const QuetooTheme *self, const char *title, ActionFunction action, ident sender, ident data)
	 * @brief Attaches a Button with the given parameters to the target View.
	 * @param self The QuetooTheme.
	 * @param title The Button title.
	 * @param function The ActionFunction for click events.
	 * @param sender The Action sender.
	 * @param data The Action data.
	 * @memberof QuetooTheme
	 */
	void (*button)(const QuetooTheme *self, const char *title, ActionFunction function, ident sender, ident data);

	/**
	 * @fn void QuetooTheme::checkbox(const QuetooTheme *self, const char *label, const char *name)
	 * @brief Attaches a CvarCheckbox for the specified variable to the target View.
	 * @param self The QuetooTheme.
	 * @param label The Label text.
	 * @param name The variable name.
	 * @memberof QuetooTheme
	 */
	void (*checkbox)(const QuetooTheme *self, const char *label, const char *name);

	/**
	 * @fn StackView *QuetooTheme::columns(const QuetooTheme *self, size_t count)
	 * @brief Creates a column layout with the specified column count.
	 * @param self The theme.
	 * @param count The number of columns.
	 * @return The columns StackView, or `NULL` on error.
	 * @memberof QuetooTheme
	 */
	StackView *(*columns)(const QuetooTheme *self, size_t count);

	/**
	 * @fn StackView *QuetooTheme::container(const QuetooTheme *self)
	 * @brief Creates a container.
	 * @param self The QuetooTheme.
	 * @return The container, or `NULL` on error.
	 * @memberof QuetooTheme
	 */
	StackView *(*container)(const QuetooTheme *self);

	/**
	 * @fn void QuetooTheme::control(const QuetooTheme *self, const char *label, ident control)
	 * @brief Attaches the given Control to the target View.
	 * @param self The QuetooTheme.
	 * @param label The Label text.
	 * @param control The Control.
	 * @memberof QuetooTheme
	 */
	void (*control)(const QuetooTheme *self, const char *label, ident control);

	/**
	 * @fn ImageView *QuetooTheme::imageView(const QuetooTheme *self, const char *name, const SDL_Rect *frame)
	 * @brief Creates an ImageView with the specified image and frame.
	 * @param self The QuetooTheme.
	 * @param name The image name (e.g. `"ui/pics/progress_bar"`).
	 * @param frame The frame.
	 * @return The ImageView, or `NULL` on error.
	 * @memberof QuetooTheme
	 */
	ImageView *(*image)(const QuetooTheme *self, const char *name, const SDL_Rect *frame);

	/**
	 * @fn QuetooTheme *QuetooTheme::init(QuetooTheme *self)
	 * @brief Initializes this QuetooTheme.
	 * @param self The QuetooTheme.
	 * @return The initialized QuetooTheme, or `NULL` on error.
	 * @memberof QuetooTheme
	 */
	QuetooTheme *(*init)(QuetooTheme *self);

	/**
	 * @fn QuetooTheme *QuetooTheme::init(QuetooTheme *self)
	 * @brief Initializes this QuetooTheme with the given target View.
	 * @param self The QuetooTheme.
	 * @param target The target View.
	 * @return The initialized QuetooTheme, or `NULL` on error.
	 * @memberof QuetooTheme
	 */
	QuetooTheme *(*initWithTarget)(QuetooTheme *self, ident target);

	/**
	 * @fn Panel *QuetooTheme::panel(const QuetooTheme *self)
	 * @brief Creates a Panel.
	 * @param self The QuetooTheme.
	 * @return The Panel, or `NULL` on error.
	 * @memberof QuetooTheme
	 */
	Panel *(*panel)(const QuetooTheme *self);
	
	/**
	 * @fn void QuetooTheme::slider(const QuetooTheme *self, const char *label, const char *name, double min, double max, double step)
	 * @brief Attaches a CvarSlider for the specified variable to the target View.
	 * @param self The QuetooTheme.
	 * @param label The Label text.
	 * @param name The variable name.
	 * @param min The min value.
	 * @param max The max value.
	 * @param step The step.
	 * @param delegate The SliderDelegate.
	 * @memberof QuetooTheme
	 */
	void (*slider)(const QuetooTheme *self, const char *label, const char *name,
				   double min, double max, double step, SliderDelegate *delegate);

	/**
	 * @fn void QuetooTheme::target(QuetooTheme *self, ident target)
	 * @brief Sets the container View that the QuetooTheme will add new elements to.
	 * @param self The QuetooTheme.
	 * @param target The target View.
	 * @memberof QuetooTheme
	 */
	void (*target)(QuetooTheme *self, ident target);

	/**
	 * @fn void QuetooTheme::targetSubview(QuetooTheme *self, ident view, int index)
	 * @brief Sets the target View that the QuetooTheme will add new elements to.
	 * @param self The QuetooTheme.
	 * @param target The View.
	 * @param index The index of the subview to target.
	 * @memberof QuetooTheme
	 */
	void (*targetSubview)(QuetooTheme *self, ident view, int index);

	/**
	 * @fn void QuetooTheme::textView(const QuetooTheme *self, const char *label, const char *name)
	 * @brief Attaches a CvarTextView for the specified variable to the target View.
	 * @param self The QuetooTheme.
	 * @param label The Label text.
	 * @param name The variable name.
	 * @memberof QuetooTheme
	 */
	void (*textView)(const QuetooTheme *self, const char *label, const char *name);
};

/**
 * @fn Class *QuetooTheme::_QuetooTheme(void)
 * @brief The QuetooTheme archetype.
 * @return The QuetooTheme Class.
 * @memberof QuetooTheme
 */
Class *_QuetooTheme(void);
