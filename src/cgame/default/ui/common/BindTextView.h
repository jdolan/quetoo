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

#include <ObjectivelyMVC/TextView.h>

/**
 * @file
 * @brief A TextView bound to a key binding.
 */

typedef struct BindTextView BindTextView;
typedef struct BindTextViewInterface BindTextViewInterface;

/**
 * @brief A TextView bound to a key binding.
 * @extends TextView
 */
struct BindTextView {

	/**
	 * @brief The superclass.
	 * @private
	 */
	TextView textView;

	/**
	 * @brief The interface.
	 * @private
	 */
	BindTextViewInterface *interface;

	/**
	 * @brief The bind (e.g. `+forward`).
	 */
	char *bind;
};

/**
 * @brief The BindTextView interface.
 */
struct BindTextViewInterface {

	/**
	 * @brief The superclass interface.
	 */
	TextViewInterface textViewInterface;

	/**
	 * @fn BindTextView *BindTextView::initWithBind(BindTextView *self, const char *bind)
	 * @brief Initializes this TextView with the given bind.
	 * @param bind The bind (e.g. `+forward).
	 * @return The initialized BindTextView, or `NULL` on error.
	 * @memberof BindTextView
	 */
	BindTextView *(*initWithBind)(BindTextView *self, const char *bind);
};

/**
 * @fn Class *BindTextView::_BindTextView(void)
 * @brief The BindTextView archetype.
 * @return The BindTextView Class.
 * @memberof BindTextView
 */
Class *_BindTextView(void);
