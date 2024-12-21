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

void Ui_HandleEvent(const SDL_Event *event);
void Ui_ViewWillAppear(void);
void Ui_ViewWillDisappear(void);
ViewController *Ui_TopViewController(void);
void Ui_PushViewController(ViewController *viewController);
void Ui_PopToViewController(ViewController *viewController);
void Ui_PopViewController(void);
void Ui_PopAllViewControllers(void);
void Ui_Draw(void);
void Ui_Init(void);
void Ui_Shutdown(void);

#ifdef __UI_LOCAL_H__
#endif /* __UI_LOCAL_H__ */
