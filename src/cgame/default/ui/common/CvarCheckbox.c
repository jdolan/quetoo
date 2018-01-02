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

#include "CvarCheckbox.h"

#define _Class _CvarCheckbox

#pragma mark - View

/**
 * @see View::updateBindings(View *)
 */
static void updateBindings(View *self) {

	super(View, self, updateBindings);

	Control *this = (Control *) self;

	const ControlState state = this->state;

	const cvar_t *var = ((CvarCheckbox *) self)->var;

	this->state = var->integer ? ControlStateSelected : ControlStateDefault;

	if ((ControlState) this->state != state) {
		$(this, stateDidChange);
	}
}

#pragma mark - CvarCheckbox

/**
 * @brief ActionFunction for the Checkbox.
 */
static void action(Control *control, const SDL_Event *event, ident sender, ident data) {

	const CvarCheckbox *this = (CvarCheckbox *) control;

	cgi.CvarSetValue(this->var->name, $(control, selected));
}

/**
 * @fn CvarCheckbox *CvarCheckbox::initWithVariable(CvarCheckbox *self, cvar_t *var)
 *
 * @memberof CvarCheckbox
 */
static CvarCheckbox *initWithVariable(CvarCheckbox *self, cvar_t *var) {

	self = (CvarCheckbox *) super(Checkbox, self, initWithFrame, NULL);
	if (self) {

		self->var = var;
		assert(self->var);

		$((Control *) self, addActionForEventType, SDL_MOUSEBUTTONUP, action, self, NULL);

		$((View *) self, updateBindings);
	}

	return self;
}

#pragma mark - Class lifecycle

/**
 * @see Class::initialize(Class *)
 */
static void initialize(Class *clazz) {

	((ViewInterface *) clazz->def->interface)->updateBindings = updateBindings;

	((CvarCheckboxInterface *) clazz->def->interface)->initWithVariable = initWithVariable;
}

/**
 * @fn Class *CvarCheckbox::_CvarCheckbox(void)
 * @memberof CvarCheckbox
 */
Class *_CvarCheckbox(void) {
	static Class clazz;
	static Once once;

	do_once(&once, {
		clazz.name = "CvarCheckbox";
		clazz.superclass = _Checkbox();
		clazz.instanceSize = sizeof(CvarCheckbox);
		clazz.interfaceOffset = offsetof(CvarCheckbox, interface);
		clazz.interfaceSize = sizeof(CvarCheckboxInterface);
		clazz.initialize = initialize;
	});

	return &clazz;
}

#undef _Class

