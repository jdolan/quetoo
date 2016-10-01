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

#include "g_local.h"

GList *g_map_list;

/**
 * @brief Populates g_map_list from a text file. See default/maps.lst
 */
static void G_MapList_Parse(const char *filename) {
	void *buf;

	if (gi.LoadFile(filename, &buf) == -1) {
		gi.Warn("Couldn't open %s\n", filename);
		return;
	}

	const char *buffer = (char *) buf;

	g_map_list_map_t *map = NULL;

	while (true) {

		const char *c = ParseToken(&buffer);

		if (*c == '\0')
			break;

		if (*c == '{') {
			if (map) {
				gi.Error("Malformed maps.lst at \"%s\"\n", c);
				return;
			}
			map = gi.Malloc(sizeof(g_map_list_map_t), MEM_TAG_GAME);
		}

		if (map == NULL) // skip any whitespace between maps
			continue;

		if (!g_strcmp0(c, "name")) {
			g_strlcpy(map->name, ParseToken(&buffer), sizeof(map->name));
			continue;
		}

		if (!g_strcmp0(c, "title")) {
			g_strlcpy(map->title, ParseToken(&buffer), sizeof(map->title));
			continue;
		}

		if (!g_strcmp0(c, "sky")) {
			g_strlcpy(map->sky, ParseToken(&buffer), sizeof(map->sky));
			continue;
		}

		if (!g_strcmp0(c, "weather")) {
			g_strlcpy(map->weather, ParseToken(&buffer), sizeof(map->weather));
			continue;
		}

		if (!g_strcmp0(c, "gravity")) {
			map->gravity = atoi(ParseToken(&buffer));
			continue;
		}

		if (!g_strcmp0(c, "gameplay")) {
			map->gameplay = G_GameplayByName(ParseToken(&buffer));
			continue;
		}

		if (!g_strcmp0(c, "teams")) {
			map->teams = strtoul(ParseToken(&buffer), NULL, 0);
			continue;
		}

		if (!g_strcmp0(c, "ctf")) {
			map->ctf = strtoul(ParseToken(&buffer), NULL, 0);
			continue;
		}

		if (!g_strcmp0(c, "match")) {
			map->match = strtoul(ParseToken(&buffer), NULL, 0);
			continue;
		}

		if (!g_strcmp0(c, "rounds")) {
			map->rounds = strtoul(ParseToken(&buffer), NULL, 0);
			continue;
		}

		if (!g_strcmp0(c, "frag_limit")) {
			map->frag_limit = strtoul(ParseToken(&buffer), NULL, 0);
			continue;
		}

		if (!g_strcmp0(c, "round_limit")) {
			map->round_limit = strtoul(ParseToken(&buffer), NULL, 0);
			continue;
		}

		if (!g_strcmp0(c, "capture_limit")) {
			map->capture_limit = strtoul(ParseToken(&buffer), NULL, 0);
			continue;
		}

		if (!g_strcmp0(c, "time_limit")) {
			map->time_limit = atof(ParseToken(&buffer));
			continue;
		}

		if (!g_strcmp0(c, "give")) {
			g_strlcpy(map->give, ParseToken(&buffer), sizeof(map->give));
			continue;
		}

		if (!g_strcmp0(c, "music")) {
			g_strlcpy(map->music, ParseToken(&buffer), sizeof(map->music));
			continue;
		}

		if (*c == '}') { // wrap it up, B

			gi.Debug("Loaded map %s:\n"
			 "title: %s\n"
			 "sky: %s\n"
			 "weather: %s\n"
			 "gravity: %d\n"
			 "gameplay: %ud\n"
			 "teams: %ud\n"
			 "ctf: %ud\n"
			 "match: %ud\n"
			 "rounds: %ud\n"
			 "frag_limit: %ud\n"
			 "round_limit: %ud\n"
			 "capture_limit: %ud\n"
			 "time_limit: %f\n"
			 "give: %s\n"
			 "music: %s\n",
			 map->name, map->title, map->sky, map->weather, map->gravity,
			 map->gameplay, map->teams, map->ctf, map->match, map->rounds,
			 map->frag_limit, map->round_limit, map->capture_limit,
			 map->time_limit, map->give, map->music);

			g_map_list = g_list_append(g_map_list, map);

			map = NULL;
		}
	}

	gi.FreeFile(buf);
}

/**
 * @brief
 */
const g_map_list_map_t *G_MapList_Find(const char *name) {

	for (GList *list = g_map_list; list; list = list->next) {
		const g_map_list_map_t *map = list->data;
		if (!g_strcmp0(name, map->name)) {
			return map;
		}
	}

	return NULL;
}

/**
 * @brief
 */
const g_map_list_map_t *G_MapList_Next(void) {

	GList *list = NULL;

	const size_t len = g_list_length(g_map_list);
	if (len) {
		if (g_random_map->value) {
			list = g_list_nth(g_map_list, Random() % len);
		} else {
			const g_map_list_map_t *map = G_MapList_Find(g_level.name);
			if (map) {
				list = g_list_find(g_map_list, map)->next;
				
				if (!list)
					list = g_map_list;
			} else {
				list = g_map_list;
			}
		}
	}

	return list ? list->data : NULL;
}

static void G_NextMap_f(void) {

	const g_map_list_map_t *map = G_MapList_Next();
	if (map) {
		gi.AddCommandString(va("map %s\n", map->name));
	} else {
		gi.AddCommandString("map edge\n");
	}
}

/**
 * @brief
 */
void G_MapList_Init(void) {

	g_map_list = NULL;

	G_MapList_Parse("maps.lst");

	gi.Cmd("g_next_map", G_NextMap_f, CMD_GAME, "Advances the server to the next map");
}

/**
 * @brief
 */
void G_MapList_Shutdown(void) {

	g_list_free_full(g_map_list, gi.Free);

	g_map_list = NULL;
}
