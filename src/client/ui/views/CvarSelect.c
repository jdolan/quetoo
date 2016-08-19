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

#include "CvarSelect.h"

#define _Class _CvarSelect

#pragma mark - View

/**
 * @see View::updateBindings(View *)
 */
static void updateBindings(View *self) {

	super(View, self, updateBindings);

	CvarSelect *this = (CvarSelect *) self;

	$((Select *) this, selectOptionWithValue, (ident) (intptr_t) this->var->integer);
}

#pragma mark - CvarSelect

/**
 * @brief SelectDelegate callback.
 */
static void didSelectOption(Select *Select, Option *option) {

	const CvarSelect *this = (CvarSelect *) Select;

	Cvar_SetValue(this->var->name, (int32_t) option->value);
}

/**
 * @fn CvarSelect *CvarSelect::initWithVariable(CvarSelect *self, cvar_t *var)
 *
 * @memberof CvarSelect
 */
static CvarSelect *initWithVariable(CvarSelect *self, cvar_t *var) {

	self = (CvarSelect *) super(Select, self, initWithFrame, NULL, ControlStyleDefault);
	if (self) {

		self->var = var;
		assert(self->var);

		Select *this = (Select *) self;

		this->delegate.didSelectOption = didSelectOption;
	}

	return self;
}

#pragma mark - Class lifecycle

/**
 * @see Class::initialize(Class *)
 */
static void initialize(Class *clazz) {

	((ViewInterface *) clazz->interface)->updateBindings = updateBindings;

	((CvarSelectInterface *) clazz->interface)->initWithVariable = initWithVariable;
}

Class _CvarSelect = {
	.name = "CvarSelect",
	.superclass = &_Select,
	.instanceSize = sizeof(CvarSelect),
	.interfaceOffset = offsetof(CvarSelect, interface),
	.interfaceSize = sizeof(CvarSelectInterface),
	.initialize = initialize,
};

#undef _Class

