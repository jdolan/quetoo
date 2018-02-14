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
 * @see View::awakeWithDictionary(View *, const Dictionary *)
 */
static void awakeWithDictionary(View *self, const Dictionary *dictionary) {

	super(View, self, awakeWithDictionary, dictionary);

	CvarCheckbox *this = (CvarCheckbox *) self;

	const Inlet inlets[] = MakeInlets(
		MakeInlet("var", InletTypeApplicationDefined, &this->var, Cg_BindCvar)
	);

	$(self, bind, inlets, dictionary);
}

/**
 * @see View::init(View *)
 */
static View *init(View *self) {
	return (View *) $((CvarCheckbox *) self, initWithVariable, NULL);
}

/**
 * @see View::updateBindings(View *)
 */
static void updateBindings(View *self) {

	super(View, self, updateBindings);

	Control *this = (Control *) self;

	const ControlState state = this->state;

	const cvar_t *var = ((CvarCheckbox *) self)->var;
	if (var) {

		this->state = var->value ? ControlStateSelected : ControlStateDefault;

		if ((ControlState) this->state != state) {
			$(this, stateDidChange);
		}
	} else {
		MVC_LogWarn("No variable set\n");
	}
}

#pragma mark - CvarCheckbox

/**
 * @brief ActionFunction for the Checkbox.
 */
static void action(Control *control, const SDL_Event *event, ident sender, ident data) {

	const CvarCheckbox *this = (CvarCheckbox *) control;

	if (this->var) {
		cgi.CvarSetValue(this->var->name, $(control, isSelected));
	} else {
		MVC_LogWarn("No variable set\n");
	}
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

		$((Control *) self, addActionForEventType, SDL_MOUSEBUTTONUP, action, self, NULL);
	}

	return self;
}

#pragma mark - Class lifecycle

/**
 * @see Class::initialize(Class *)
 */
static void initialize(Class *clazz) {

	((ViewInterface *) clazz->interface)->awakeWithDictionary = awakeWithDictionary;
	((ViewInterface *) clazz->interface)->init = init;
	((ViewInterface *) clazz->interface)->updateBindings = updateBindings;

	((CvarCheckboxInterface *) clazz->interface)->initWithVariable = initWithVariable;
}

/**
 * @fn Class *CvarCheckbox::_CvarCheckbox(void)
 * @memberof CvarCheckbox
 */
Class *_CvarCheckbox(void) {
	static Class *clazz;
	static Once once;

	do_once(&once, {
		clazz = _initialize(&(const ClassDef) {
			.name = "CvarCheckbox",
			.superclass = _Checkbox(),
			.instanceSize = sizeof(CvarCheckbox),
			.interfaceOffset = offsetof(CvarCheckbox, interface),
			.interfaceSize = sizeof(CvarCheckboxInterface),
			.initialize = initialize,
		});
	});

	return clazz;
}

#undef _Class

