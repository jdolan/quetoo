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

static GList *g_map_list_default;
GList *g_map_list;

static cvar_t *g_map_list_var;

/**
 * @brief Populates g_map_list from a text file. See default/maps.lst
 */
static GList *G_MapList_Parse(const char *filename) {
	GList *list = NULL;
	void *buf;

	if (gi.LoadFile(filename, &buf) == -1) {
		gi.Warn("Couldn't open %s\n", filename);
		return list;
	}

	const char *buffer = (char *) buf;

	g_map_list_map_t *map = NULL;

	while (true) {

		const char *c = ParseToken(&buffer);

		if (*c == '\0') {
			break;
		}

		if (*c == '{') {
			if (map) {
				gi.Error("Malformed maps.lst at \"%s\"\n", c);
			}
			map = gi.Malloc(sizeof(g_map_list_map_t), MEM_TAG_GAME);
		}

		if (map == NULL) { // skip any whitespace between maps
			continue;
		}

		if (!g_strcmp0(c, "name")) {
			g_strlcpy(map->name, ParseToken(&buffer), sizeof(map->name));
			const g_map_list_map_t *defaults = G_MapList_Find(g_map_list_default, map->name);
			if (defaults) {
				memcpy(map, defaults, sizeof(*map));
			}
			continue;
		}

		if (!g_strcmp0(c, "message")) {
			g_strlcpy(map->message, ParseToken(&buffer), sizeof(map->message));
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

		if (!g_strcmp0(c, "hook")) {
			const char *token = ParseToken(&buffer);

			if (!g_strcmp0(token, "default")) {
				map->hook = -1;
			} else {
				map->hook = (int32_t) strtol(token, NULL, 0);
			}
			continue;
		}

		if (!g_strcmp0(c, "teams")) {
			map->teams = (int32_t) strtol(ParseToken(&buffer), NULL, 0);
			continue;
		}

		if (!g_strcmp0(c, "ctf")) {
			map->ctf = (int32_t) strtol(ParseToken(&buffer), NULL, 0);
			continue;
		}

		if (!g_strcmp0(c, "match")) {
			map->match = (int32_t) strtol(ParseToken(&buffer), NULL, 0);
			continue;
		}

		if (!g_strcmp0(c, "rounds")) {
			map->rounds = (int32_t) strtol(ParseToken(&buffer), NULL, 0);
			continue;
		}

		if (!g_strcmp0(c, "frag_limit")) {
			map->frag_limit = (int32_t) strtol(ParseToken(&buffer), NULL, 0);
			continue;
		}

		if (!g_strcmp0(c, "round_limit")) {
			map->round_limit = (int32_t) strtol(ParseToken(&buffer), NULL, 0);
			continue;
		}

		if (!g_strcmp0(c, "capture_limit")) {
			map->capture_limit = (int32_t) strtol(ParseToken(&buffer), NULL, 0);
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
			         "message: %s\n"
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
			         map->name, map->message, map->sky, map->weather, map->gravity,
			         map->gameplay, map->teams, map->ctf, map->match, map->rounds,
			         map->frag_limit, map->round_limit, map->capture_limit,
			         map->time_limit, map->give, map->music);

			list = g_list_append(list, map);

			map = NULL;
		}
	}

	gi.FreeFile(buf);

	return list;
}

/**
 * @brief
 */
const g_map_list_map_t *G_MapList_Find(GList *list, const char *name) {

	for (list = list ?: g_map_list; list; list = list->next) {
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
			const g_map_list_map_t *map = G_MapList_Find(NULL, g_level.name);
			if (map) {
				list = g_list_find(g_map_list, map)->next;

				if (!list) {
					list = g_map_list;
				}
			} else {
				list = g_map_list;
			}
		}
	}

	return list ? list->data : NULL;
}

/**
 * @brief
 */
static void G_NextMap_f(void) {

	if (g_map_list_var->modified) {
		G_MapList_Shutdown();
		G_MapList_Init();
	}

	const g_map_list_map_t *map = G_MapList_Next();
	if (map) {
		gi.Cbuf(va("map %s\n", map->name));
	} else {
		gi.Cbuf("map edge\n");
	}
}

/**
 * @brief
 */
void G_MapList_Init(void) {

	g_map_list_default = NULL;
	g_map_list_default = G_MapList_Parse("maps.lst");
	g_map_list = g_map_list_default;

	g_map_list_var = gi.Cvar("g_map_list", "", 0, NULL);
	g_map_list_var->modified = false;

	if (strlen(g_map_list_var->string)) {

		g_map_list = G_MapList_Parse(g_map_list_var->string);

		if (g_map_list == NULL) {
			g_map_list = g_map_list_default;
		}
	}

	gi.Cmd("g_next_map", G_NextMap_f, CMD_GAME, "Advances the server to the next map");
}

/**
 * @brief
 */
void G_MapList_Shutdown(void) {

	g_list_free_full(g_map_list, gi.Free);

	g_map_list = NULL;
}
