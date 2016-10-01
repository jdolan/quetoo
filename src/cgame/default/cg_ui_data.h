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

extern void Cg_BindInput(View *view, const char *label, const char *bind);
extern void Cg_Button(View *view, const char *title, ActionFunction function, ident sender, ident data);
extern void Cg_CvarCheckboxInput(View *view, const char *label, const char *name);
extern void Cg_CvarSliderInput(View *view, const char *label, const char *name, double min, double max, double step);
extern void Cg_CvarTextView(View *view, const char *label, const char *name);
extern void Cg_Input(View *view, const char *label, Control *control);
extern void Cg_PrimaryButton(View *view, const char *label, ActionFunction action, ident sender, ident data);
