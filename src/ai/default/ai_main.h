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

#ifdef __AI_LOCAL_H__
extern ai_import_t aim;
extern ai_export_t aix;

extern ai_level_t ai_level;

extern ai_entity_data_t ai_entity_data;
extern ai_client_data_t ai_client_data;

extern cvar_t *sv_max_clients;
extern cvar_t *ai_ann;
extern cvar_t *ai_no_target;

/**
 * @brief Resolve the entity at the given index.
 */
#define ENTITY_FOR_NUM(n) \
	((g_entity_t *) ((byte *) aim.ge->entities + aim.ge->entity_size * (n)))

ai_locals_t *Ai_GetLocals(const g_entity_t *ent);
ai_export_t *Ai_LoadAi(ai_import_t *import);
#endif /* __AI_LOCAL_H__ */
