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

#include "CvarSelect.h"

#define _Class _CvarSelect

#pragma mark - View

/**
 * @see View::awakeWithDictionary(View *, const Dictionary *)
 */
static void awakeWithDictionary(View *self, const Dictionary *dictionary) {

	super(View, self, awakeWithDictionary, dictionary);

	CvarSelect *this = (CvarSelect *) self;

	const Inlet inlets[] = MakeInlets(
		MakeInlet("expectsStringValue", InletTypeBool, &this->expectsStringValue, NULL),
		MakeInlet("var", InletTypeApplicationDefined, &this->var, Cg_BindCvar)
	);

	$(self, bind, inlets, dictionary);
}

/**
 * @see View::init(View *)
 */
static View *init(View *self) {
	return (View *) $((CvarSelect *) self, initWithVariable, NULL);
}

/**
 * @see View::updateBindings(View *)
 */
static void updateBindings(View *self) {

	super(View, self, updateBindings);

	CvarSelect *this = (CvarSelect *) self;

	if (this->var) {
		const Array *options = (Array *) this->select.options;
		for (size_t i = 0; i < options->count; i++) {
			Option *option = $(options, objectAtIndex, i);
			if (this->expectsStringValue) {
				const char *string = option->value ?: option->title->text;
				option->isSelected = !strcmp(string, this->var->string);
			} else {
				option->isSelected = (intptr_t) option->value == this->var->integer;
			}
		}
	}
}

#pragma mark - Select

/**
 * @see Select::selectOptionWithValue(Select *, ident)
 */
static void selectOptionWithValue(Select *self, ident value) {

	const CvarSelect *this = (CvarSelect *) self;
	if (this->var) {

		Option *option = $(self, optionWithValue, value);
		if (option) {

			if (this->expectsStringValue) {
				const char *string = option->value ?: option->title->text;
				cgi.SetCvarString(this->var->name, string);
			} else {
				cgi.SetCvarInteger(this->var->name, (int32_t) (intptr_t) option->value);
			}
		}
	}

	super(Select, self, selectOptionWithValue, value);
}

#pragma mark - CvarSelect

/**
 * @fn CvarSelect *CvarSelect::initWithVariable(CvarSelect *self, cvar_t *var)
 *
 * @memberof CvarSelect
 */
static CvarSelect *initWithVariable(CvarSelect *self, cvar_t *var) {

	self = (CvarSelect *) super(Select, self, initWithFrame, NULL);
	if (self) {
		self->var = var;
	}

	return self;
}

/**
 * @fn CvarSelect *CvarSelect::initWithVariabeName(CvarSelect *self, const char (name)
 *
 * @memberof CvarSelect
 */
static CvarSelect *initWithVariableName(CvarSelect *self, const char *name) {
	return $(self, initWithVariable, cgi.GetCvar(name));
}

#pragma mark - Class lifecycle

/**
 * @see Class::initialize(Class *)
 */
static void initialize(Class *clazz) {

	((ViewInterface *) clazz->interface)->awakeWithDictionary = awakeWithDictionary;
	((ViewInterface *) clazz->interface)->init = init;
	((ViewInterface *) clazz->interface)->updateBindings = updateBindings;

	((SelectInterface *) clazz->interface)->selectOptionWithValue = selectOptionWithValue;

	((CvarSelectInterface *) clazz->interface)->initWithVariable = initWithVariable;
	((CvarSelectInterface *) clazz->interface)->initWithVariableName = initWithVariableName;
}

/**
 * @fn Class *CvarSelect::_CvarSelect(void)
 * @memberof CvarSelect
 */
Class *_CvarSelect(void) {
	static Class *clazz;
	static Once once;

	do_once(&once, {
		clazz = _initialize(&(const ClassDef) {
			.name = "CvarSelect",
			.superclass = _Select(),
			.instanceSize = sizeof(CvarSelect),
			.interfaceOffset = offsetof(CvarSelect, interface),
			.interfaceSize = sizeof(CvarSelectInterface),
			.initialize = initialize,
		});
	});

	return clazz;
}

#undef _Class
