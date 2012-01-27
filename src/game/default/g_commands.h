/*
 * Copyright(c) 1997-2001 Id Software, Inc.
 * Copyright(c) 2002 The Quakeforge Project.
 * Copyright(c) 2006 Quake2World.
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

#ifndef __G_COMMANDS_H__
#define __G_COMMANDS_H__

#ifdef __G_LOCAL_H__

boolean_t G_AddClientToTeam(g_edict_t *ent, char *team_name);
void G_ClientCommand(g_edict_t *ent);
void G_Score_f(g_edict_t *ent);

#endif /* __G_LOCAL_H__ */

#endif /* __G_COMMANDS_H__ */
