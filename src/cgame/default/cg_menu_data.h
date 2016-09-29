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

#include <ObjectivelyMVC.h>

#include "cg_types.h"

extern void Ui_Bind(View *view, const char *name, const char *bind);
extern void Ui_Button(View *view, const char *title, ActionFunction function, ident sender, ident data);
extern void Ui_CvarCheckbox(View *view, const char *name, cvar_t *var);
extern void Ui_CvarSlider(View *view, const char *name, cvar_t *var, double min, double max, double step);
extern void Ui_CvarTextView(View *view, const char *name, cvar_t *var);
extern void Ui_Input(View *view, const char *name, Control *control);
extern void Ui_PrimaryButton(View *view, const char *name, ActionFunction action, ident sender, ident data);
