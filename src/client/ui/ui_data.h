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

#include "ui_types.h"

Data *Ui_Data(const char *path);
Font *Ui_Font(const char *path, const char *family, int32_t size, int32_t style);
Image *Ui_Image(const char *paOth);
Stylesheet *Ui_Stylesheet(const char *path);
Theme *Ui_Theme(void);
View *Ui_View(const char *path, Outlet *outlets);
void Ui_SetImage(ImageView *view, const char *path);
void Ui_WakeView(View *view, const char *path, Outlet *outlets);

#ifdef __UI_LOCAL_H__
#endif /* __UI_LOCAL_H__ */
