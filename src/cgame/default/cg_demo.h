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

#if defined(__CG_LOCAL_H__)

extern cvar_t *cg_demo_freecam;
extern cvar_t *cg_draw_demo_bar;

void Cg_ClearDemo(void);
bool Cg_DemoInFreeCamera(void);
bool Cg_DemoHidesHud(void);
bool Cg_DemoOverridingView(void);
void Cg_DrawDemoBar(void);
void Cg_DrawDemoLetterbox(void);
void Cg_InitDemo(void);
void Cg_PredictDemoCamera(const GPtrArray *cmds);
void Cg_UpdateDemoFov(void);
void Cg_UpdateDemoView(void);

#endif /* __CG_LOCAL_H__ */
