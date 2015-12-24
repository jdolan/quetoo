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

#ifndef __GAME_MAP_LIST_H__
#define __GAME_MAP_LIST_H__

#include "g_types.h"

#ifdef __GAME_LOCAL_H__

typedef struct {
	char name[32];
	char title[128];
	char sky[32];
	char weather[64];
	int32_t gravity;
	int32_t gameplay;
	int32_t teams;
	int32_t ctf;
	int32_t match;
	int32_t rounds;
	int32_t frag_limit;
	int32_t round_limit;
	int32_t capture_limit;
	vec_t time_limit;
	char give[MAX_STRING_CHARS];
	char music[MAX_STRING_CHARS];
} g_map_list_map_t;

extern GList *g_map_list;

const g_map_list_map_t *G_MapList_Find(const char *name);
const g_map_list_map_t *G_MapList_Next(void);

void G_MapList_Init(void);
void G_MapList_Shutdown(void);

#endif /* __GAME_LOCAL_H__ */

#endif /* __GAME_MAP_LIST_H__ */
