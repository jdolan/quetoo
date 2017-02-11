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

#ifdef __CG_LOCAL_H__
extern button_t cg_buttons[4];
#define in_speed cg_buttons[0]
#define in_attack cg_buttons[1]
#define in_hook cg_buttons[2]
#define in_score cg_buttons[3]

void Cg_ParseViewKick(void);
void Cg_Look(pm_cmd_t *cmd);
void Cg_Move(pm_cmd_t *cmd);
void Cg_ClearInput(void);
void Cg_InitInput(void);
#endif
