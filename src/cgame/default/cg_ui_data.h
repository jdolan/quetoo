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

extern void Cgui_BindInput(View *view, const char *label, const char *bind);
extern void Cgui_Button(View *view, const char *title, ActionFunction function, ident sender, ident data);
extern void Cgui_CvarCheckboxInput(View *view, const char *label, const char *name);
extern void Cgui_CvarSliderInput(View *view, const char *label, const char *name, double min, double max, double step);
extern void Cgui_CvarTextView(View *view, const char *label, const char *name);
extern void Cgui_Input(View *view, const char *label, Control *control);
extern void Cgui_Label(View *view, const char *text);
extern void Cgui_Picture(View *view, const char *pic, ViewAlignment align, ViewAutoresizing resize);
extern void Cgui_PrimaryButton(View *view, const char *label, SDL_Color color, ActionFunction action, ident sender, ident data);
extern void Cgui_PrimaryIcon(View *view, const char *icon, SDL_Color color, ActionFunction action, ident sender, ident data);
extern void Cgui_TabButton(View *view, const char *label, SDL_Color color, ActionFunction action, ident sender, ident data, _Bool isSelected);
