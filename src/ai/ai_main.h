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

void Ai_GetUserInfo(const g_entity_t *self, char *userinfo);
void Ai_Think(g_entity_t *self, pm_cmd_t *cmd);
void Ai_Begin(g_entity_t *self);
void Ai_Frame(void);
void Ai_Init(ai_import_t *import);
void Ai_Shutdown(void);

#ifdef __AI_LOCAL_H__
extern cvar_t *ai_passive;
extern ai_level_t ai_level;
extern ai_import_t aii;

ai_locals_t *Ai_GetLocals(const g_entity_t *ent);
#endif /* __AI_LOCAL_H__ */
