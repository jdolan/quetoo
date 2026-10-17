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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#pragma once

#if defined(__G_LOCAL_H__)
void G_Ai_InitSkins(void);
void G_Ai_ShutdownSkins(void);
void G_Ai_GetRosterInfo(const GameClient *cl, const GameAiRoster *entry, char *info);
const GameAiRoster *G_Ai_GetRoster(const GameClient *cl, char *info);
const GameAiRoster *G_Ai_FindRoster(const char *name);
uint32_t G_Ai_RosterCount(void);
bool G_Ai_IsLevelRoster(const GameAiRoster *entry);
void G_Ai_CheckLevelRoster(void);
void G_Ai_PrintRoster(void);
#endif
