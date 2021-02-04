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

	parser_t parser;
	char token[MAX_STRING_CHARS];
	Parse_Init(&parser, (const char *) buf, PARSER_ALL_COMMENTS);

	g_map_list_map_t *map = NULL;

	while (true) {

		if (!Parse_Token(&parser, PARSE_DEFAULT, token, sizeof(token))) {
			break;
		}

		if (*token == '{') {
			if (map) {
				gi.Error("Malformed maps.lst at %s: %u,%u\n", token, parser.position.row, parser.position.col);
			}

			map = gi.Malloc(sizeof(g_map_list_map_t), MEM_TAG_GAME);

			// setup defaults
			map->gameplay = -1;
			map->teams = -1;
			map->num_teams = -1;
			map->techs = -1;
			map->ctf = -1;
			map->match = -1;
			map->rounds = -1;
			map->frag_limit = -1;
			map->round_limit = -1;
			map->capture_limit = -1;
			map->time_limit = -1;
			map->hook = -1;
		}

		if (!map) {
			continue;
		}

		if (!g_strcmp0(token, "name")) {
			if (!Parse_Token(&parser, PARSE_DEFAULT, map->name, sizeof(map->name))) {
				gi.Error("Malformed maps.lst at %s: %u,%u\n", token, parser.position.row, parser.position.col);
			}

			const g_map_list_map_t *defaults = G_MapList_Find(g_map_list_default, map->name);
			
			if (defaults) {
				memcpy(map, defaults, sizeof(*map));
			}
			continue;
		}

		if (!g_strcmp0(token, "message")) {
			if (!Parse_Token(&parser, PARSE_DEFAULT, map->message, sizeof(map->message))) {
				gi.Error("Malformed maps.lst at %s: %u,%u\n", token, parser.position.row, parser.position.col);
			}
			continue;
		}

		if (!g_strcmp0(token, "sky")) {
			if (!Parse_Token(&parser, PARSE_DEFAULT, map->sky, sizeof(map->sky))) {
				gi.Error("Malformed maps.lst at %s: %u,%u\n", token, parser.position.row, parser.position.col);
			}
			continue;
		}

		if (!g_strcmp0(token, "weather")) {
			if (!Parse_Token(&parser, PARSE_DEFAULT, map->weather, sizeof(map->weather))) {
				gi.Error("Malformed maps.lst at %s: %u,%u\n", token, parser.position.row, parser.position.col);
			}
			continue;
		}

		if (!g_strcmp0(token, "gravity")) {
			if (!Parse_Primitive(&parser, PARSE_DEFAULT, PARSE_FLOAT, &map->gravity, 1)) {
				gi.Error("Malformed maps.lst at %s: %u,%u\n", token, parser.position.row, parser.position.col);
			}
			continue;
		}

		if (!g_strcmp0(token, "gameplay")) {
			if (!Parse_Token(&parser, PARSE_DEFAULT, token, sizeof(token))) {
				gi.Error("Malformed maps.lst at %s: %u,%u\n", "gameplay", parser.position.row, parser.position.col);
			}
			map->gameplay = G_GameplayByName(token);
			continue;
		}

		if (!g_strcmp0(token, "hook")) {
			if (!Parse_Token(&parser, PARSE_DEFAULT, token, sizeof(token))) {
				gi.Error("Malformed maps.lst at %s: %u,%u\n", "hook", parser.position.row, parser.position.col);
			}

			if (!g_strcmp0(token, "default")) {
				map->hook = -1;
			} else {
				map->hook = (int32_t) strtol(token, NULL, 0);
			}
			continue;
		}

		if (!g_strcmp0(token, "teams")) {
			if (!Parse_Primitive(&parser, PARSE_DEFAULT, PARSE_INT32, &map->teams, 1)) {
				gi.Error("Malformed maps.lst at %s: %u,%u\n", token, parser.position.row, parser.position.col);
			}
			continue;
		}

		if (!g_strcmp0(token, "num_teams")) {
			if (!Parse_Primitive(&parser, PARSE_DEFAULT, PARSE_INT32, &map->num_teams, 1)) {
				gi.Error("Malformed maps.lst at %s: %u,%u\n", token, parser.position.row, parser.position.col);
			}
			continue;
		}

		if (!g_strcmp0(token, "techs")) {
			if (!Parse_Primitive(&parser, PARSE_DEFAULT, PARSE_INT32, &map->techs, 1)) {
				gi.Error("Malformed maps.lst at %s: %u,%u\n", token, parser.position.row, parser.position.col);
			}
			continue;
		}

		if (!g_strcmp0(token, "ctf")) {
			if (!Parse_Primitive(&parser, PARSE_DEFAULT, PARSE_INT32, &map->ctf, 1)) {
				gi.Error("Malformed maps.lst at %s: %u,%u\n", token, parser.position.row, parser.position.col);
			}
			continue;
		}

		if (!g_strcmp0(token, "match")) {
			if (!Parse_Primitive(&parser, PARSE_DEFAULT, PARSE_INT32, &map->match, 1)) {
				gi.Error("Malformed maps.lst at %s: %u,%u\n", token, parser.position.row, parser.position.col);
			}
			continue;
		}

		if (!g_strcmp0(token, "rounds")) {
			if (!Parse_Primitive(&parser, PARSE_DEFAULT, PARSE_INT32, &map->rounds, 1)) {
				gi.Error("Malformed maps.lst at %s: %u,%u\n", token, parser.position.row, parser.position.col);
			}
			continue;
		}

		if (!g_strcmp0(token, "frag_limit")) {
			if (!Parse_Primitive(&parser, PARSE_DEFAULT, PARSE_INT32, &map->frag_limit, 1)) {
				gi.Error("Malformed maps.lst at %s: %u,%u\n", token, parser.position.row, parser.position.col);
			}
			continue;
		}

		if (!g_strcmp0(token, "round_limit")) {
			if (!Parse_Primitive(&parser, PARSE_DEFAULT, PARSE_INT32, &map->round_limit, 1)) {
				gi.Error("Malformed maps.lst at %s: %u,%u\n", token, parser.position.row, parser.position.col);
			}
			continue;
		}

		if (!g_strcmp0(token, "capture_limit")) {
			if (!Parse_Primitive(&parser, PARSE_DEFAULT, PARSE_INT32, &map->capture_limit, 1)) {
				gi.Error("Malformed maps.lst at %s: %u,%u\n", token, parser.position.row, parser.position.col);
			}
			continue;
		}

		if (!g_strcmp0(token, "time_limit")) {
			if (!Parse_Primitive(&parser, PARSE_DEFAULT, PARSE_FLOAT, &map->time_limit, 1)) {
				gi.Error("Malformed maps.lst at %s: %u,%u\n", token, parser.position.row, parser.position.col);
			}
			continue;
		}

		if (!g_strcmp0(token, "give")) {
			if (!Parse_Token(&parser, PARSE_DEFAULT, map->give, sizeof(map->give))) {
				gi.Error("Malformed maps.lst at %s: %u,%u\n", token, parser.position.row, parser.position.col);
			}
			continue;
		}

		if (!g_strcmp0(token, "music")) {
			if (!Parse_Token(&parser, PARSE_DEFAULT, map->music, sizeof(map->music))) {
				gi.Error("Malformed maps.lst at %s: %u,%u\n", token, parser.position.row, parser.position.col);
			}
			continue;
		}

		if (*token == '}') { // wrap it up, B

			G_Debug("Loaded map %s:\n"
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

	for (list = list ? : g_map_list; list; list = list->next) {
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

	const guint len = g_list_length(g_map_list);
	if (len) {
		if (g_random_map->value) {
			list = g_list_nth(g_map_list, RandomRangeu(0, len));
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

	g_map_list_var = gi.AddCvar("g_map_list", "", 0, NULL);
	g_map_list_var->modified = false;

	if (strlen(g_map_list_var->string)) {

		g_map_list = G_MapList_Parse(g_map_list_var->string);

		if (g_map_list == NULL) {
			g_map_list = g_map_list_default;
		}
	}

	gi.AddCmd("g_next_map", G_NextMap_f, CMD_GAME, "Advances the server to the next map");
}

/**
 * @brief
 */
void G_MapList_Shutdown(void) {

	g_list_free_full(g_map_list, gi.Free);

	g_map_list = NULL;
}
